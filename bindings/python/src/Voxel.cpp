#include "Common.h"

#include <cstdio>
#include <cstring>

namespace mge::python {

    struct PyBlockDesc {
        std::string name = "block";
        bool isSolid = true;
        bool isOpaque = true;
        glm::vec4 color{1.0f};
    };

    class PyBlockRegistry {
    public:
        PyBlockRegistry() = default;

        uint16_t registerBlock(const PyBlockDesc& desc) {
            mge::BlockDesc d;
            d.name = desc.name;
            d.solid = desc.isSolid;
            d.opaque = desc.isOpaque;
            d.color = desc.color;
            return m_registry.add(d);
        }

        uint16_t registerBlockWithName(const std::string& name, const PyBlockDesc& desc) {
            mge::BlockDesc d;
            d.name = name;
            d.solid = desc.isSolid;
            d.opaque = desc.isOpaque;
            d.color = desc.color;
            return m_registry.add(d);
        }

        std::string name(uint16_t id) const { return m_registry.get(id).name; }

        bool isSolid(uint16_t id) const { return m_registry.isSolid(id); }

        bool isOpaque(uint16_t id) const { return m_registry.isOpaque(id); }

        nanobind::object find(const std::string& blockName) const {
            auto opt = m_registry.find(blockName);
            if (!opt.has_value()) return nanobind::none();
            return nanobind::cast(opt.value());
        }

        size_t size() const { return m_registry.size(); }

        const mge::BlockRegistry& engineRegistry() const { return m_registry; }
        mge::BlockRegistry& engineRegistry() { return m_registry; }

    private:
        mge::BlockRegistry m_registry;
    };

    struct PyRaycastResult {
        bool hit = false;
        glm::vec3 point{0.0f};
        glm::ivec3 blockPos{0};
        glm::ivec3 normal{0};
        float distance = 0.0f;
        uint16_t blockId = 0;
    };

    namespace {

        struct PyBufferGuard {
            Py_buffer view{};
            bool acquired = false;
            ~PyBufferGuard() {
                if (acquired) {
                    PyBuffer_Release(&view);
                }
            }
        };

        void fillChunkFromPyObject(mge::ChunkData& chunk, nanobind::handle obj) {
            namespace nb = nanobind;
            auto& dst = chunk.blocks();

            if (PyObject_CheckBuffer(obj.ptr())) {
                PyBufferGuard guard;
                if (PyObject_GetBuffer(obj.ptr(), &guard.view, PyBUF_CONTIG_RO) != 0) {
                    throw nb::python_error();
                }
                guard.acquired = true;

                const std::size_t itemSize =
                    guard.view.itemsize > 0 ? static_cast<std::size_t>(guard.view.itemsize) : 1u;
                const std::size_t totalBytes =
                    guard.view.len >= 0 ? static_cast<std::size_t>(guard.view.len) : 0u;
                if (totalBytes != static_cast<std::size_t>(mge::kChunkVolume) * itemSize) {
                    throw EngineError("generate_chunk must return 4096 block IDs");
                }

                if (itemSize == 1) {
                    const auto* src = static_cast<const std::uint8_t*>(guard.view.buf);
                    for (std::size_t i = 0; i < dst.size(); ++i) {
                        dst[i] = static_cast<mge::BlockId>(src[i]);
                    }
                    return;
                }
                if (itemSize == sizeof(mge::BlockId)) {
                    std::memcpy(dst.data(), guard.view.buf, dst.size() * sizeof(mge::BlockId));
                    return;
                }
                if (itemSize == 4) {
                    const auto* src = static_cast<const std::uint32_t*>(guard.view.buf);
                    for (std::size_t i = 0; i < dst.size(); ++i) {
                        dst[i] = static_cast<mge::BlockId>(src[i]);
                    }
                    return;
                }
                if (itemSize == 8) {
                    const auto* src = static_cast<const std::uint64_t*>(guard.view.buf);
                    for (std::size_t i = 0; i < dst.size(); ++i) {
                        dst[i] = static_cast<mge::BlockId>(src[i]);
                    }
                    return;
                }
                throw EngineError("Unsupported buffer item size returned by generate_chunk");
            }

            if (PySequence_Check(obj.ptr())) {
                const Py_ssize_t len = PySequence_Size(obj.ptr());
                if (len != static_cast<Py_ssize_t>(mge::kChunkVolume)) {
                    throw EngineError("generate_chunk must return 4096 block IDs");
                }
                nb::sequence seq = nb::borrow<nb::sequence>(obj);
                for (std::size_t i = 0; i < dst.size(); ++i) {
                    dst[i] = nb::cast<mge::BlockId>(seq[i]);
                }
                return;
            }

            throw EngineError("generate_chunk must return a buffer or sequence of 4096 block IDs");
        }

    } // namespace

    class PyTerrainGenerator : public mge::TerrainGenerator {
    public:
        explicit PyTerrainGenerator(nanobind::object fn) : m_fn(std::move(fn)) {
            if (m_fn && !m_fn.is_none() && nanobind::hasattr(m_fn, "generate_chunk")) {
                m_mode = Mode::PerChunkMethod;
            }
        }

        void generate(const mge::ChunkCoord& coord, mge::ChunkData& chunk) const override {
            namespace nb = nanobind;
            nb::gil_scoped_acquire gil;
            if (!m_fn || m_fn.is_none()) return;

            if (m_mode == Mode::PerChunkMethod) {
                try {
                    nb::object res = m_fn.attr("generate_chunk")(coord.x, coord.y, coord.z);
                    fillChunkFromPyObject(chunk, res);
                } catch (const std::exception& e) {
                    MGE_LOG_ERROR("Voxel", "Error en generador de chunk: ", e.what());
                    throw;
                }
                return;
            }

            if (m_mode == Mode::Auto || m_mode == Mode::PerChunk) {
                try {
                    nb::object res = m_fn(coord.x, coord.y, coord.z);
                    if (m_mode == Mode::Auto) {
                        if (nb::isinstance<nb::int_>(res)) {
                            m_mode = Mode::PerBlock;
                        } else {
                            m_mode = Mode::PerChunk;
                            fillChunkFromPyObject(chunk, res);
                            return;
                        }
                    } else {
                        fillChunkFromPyObject(chunk, res);
                        return;
                    }
                } catch (const std::exception& e) {
                    MGE_LOG_ERROR("Voxel", "Error en generador de chunk: ", e.what());
                    throw;
                }
            }

            const glm::ivec3 origin = mge::chunkOrigin(coord);
            for (int z = 0; z < static_cast<int>(mge::kChunkSize); ++z) {
                for (int y = 0; y < static_cast<int>(mge::kChunkSize); ++y) {
                    for (int x = 0; x < static_cast<int>(mge::kChunkSize); ++x) {
                        try {
                            auto res = m_fn(origin.x + x, origin.y + y, origin.z + z);
                            const auto id = nb::cast<mge::BlockId>(res);
                            chunk.set(x, y, z, id);
                        } catch (const nb::python_error& e) {
                            MGE_LOG_ERROR("Voxel", "Error en generador de terreno: ", e.what());
                            throw;
                        }
                    }
                }
            }
        }

    private:
        enum class Mode {
            Auto,
            PerChunk,
            PerChunkMethod,
            PerBlock
        };

        nanobind::object m_fn;
        mutable Mode m_mode = Mode::Auto;
    };

    class PyVoxelWorld {
    public:
        PyVoxelWorld(std::shared_ptr<PyBlockRegistry> registry, nanobind::object generator = nanobind::none())
            : m_registry(std::move(registry)) {
            if (!m_registry) {
                m_registry = std::make_shared<PyBlockRegistry>();
            }
            std::unique_ptr<mge::TerrainGenerator> gen;
            if (!generator.is_none()) {
                gen = std::make_unique<PyTerrainGenerator>(generator);
            }
            m_world = std::make_unique<mge::VoxelWorld>(m_registry->engineRegistry(), std::move(gen));
        }

        uint16_t getBlock(int32_t x, int32_t y, int32_t z) {
            if (!m_world) return 0;
            const glm::ivec3 pos{x, y, z};
            const mge::ChunkCoord coord = mge::chunkOf(pos);
            if (!m_world->isLoaded(coord)) {
                if (m_world->generator()) {
                    m_world->loadChunk(coord);
                } else {
                    return 0;
                }
            }
            return m_world->getBlock(pos);
        }

        bool setBlock(int32_t x, int32_t y, int32_t z, uint16_t blockId) {
            if (!m_world) return false;
            const glm::ivec3 pos{x, y, z};
            const mge::ChunkCoord coord = mge::chunkOf(pos);
            if (!m_world->isLoaded(coord)) {
                m_world->loadChunk(coord);
            }
            return m_world->setBlock(pos, blockId);
        }

        void loadChunk(int32_t cx, int32_t cy, int32_t cz) {
            if (m_world) {
                m_world->loadChunk(mge::ChunkCoord{cx, cy, cz});
            }
        }

        bool isChunkLoaded(int32_t cx, int32_t cy, int32_t cz) const {
            return m_world ? m_world->isLoaded(mge::ChunkCoord{cx, cy, cz}) : false;
        }

        PyRaycastResult raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDist = 50.0f) {
            PyRaycastResult res;
            if (!m_world) return res;

            if (m_world->generator()) {
                const float stepSize = static_cast<float>(mge::kChunkSize);
                const glm::vec3 normDir = glm::normalize(direction);
                for (float t = 0.0f; t <= maxDist; t += stepSize) {
                    const glm::vec3 p = origin + normDir * t;
                    const mge::ChunkCoord coord = mge::chunkOf(mge::blockAt(p));
                    if (!m_world->isLoaded(coord)) {
                        m_world->loadChunk(coord);
                    }
                }
                const glm::vec3 endP = origin + normDir * maxDist;
                const mge::ChunkCoord endCoord = mge::chunkOf(mge::blockAt(endP));
                if (!m_world->isLoaded(endCoord)) {
                    m_world->loadChunk(endCoord);
                }
            }

            const mge::Ray ray(origin, direction);
            auto hit = mge::raycastVoxels(*m_world, ray, maxDist);
            if (!hit.has_value()) return res;

            res.hit = true;
            res.point = hit->point;
            res.blockPos = hit->block;
            res.normal = hit->normal;
            res.distance = hit->distance;
            res.blockId = hit->blockId;
            return res;
        }

    private:
        std::shared_ptr<PyBlockRegistry> m_registry;
        std::unique_ptr<mge::VoxelWorld> m_world;
    };

    void initVoxel(nanobind::module_& m) {
        namespace nb = nanobind;

        // --- BlockDesc ---
        nb::class_<PyBlockDesc>(m, "BlockDesc")
            .def(nb::init<>())
            .def(
                "__init__",
                [](PyBlockDesc* self, const std::string& name, bool isSolid, bool isOpaque,
                   const glm::vec4& color) {
                    new (self) PyBlockDesc();
                    self->name = name;
                    self->isSolid = isSolid;
                    self->isOpaque = isOpaque;
                    self->color = color;
                },
                nb::arg("name") = "block", nb::arg("is_solid") = true, nb::arg("is_opaque") = true,
                nb::arg("color") = glm::vec4(1.0f))
            .def_rw("name", &PyBlockDesc::name)
            .def_rw("is_solid", &PyBlockDesc::isSolid)
            .def_rw("is_opaque", &PyBlockDesc::isOpaque)
            .def_rw("color", &PyBlockDesc::color);

        // --- BlockRegistry ---
        nb::class_<PyBlockRegistry>(m, "BlockRegistry")
            .def(nb::init<>())
            .def("register_block", &PyBlockRegistry::registerBlock, nb::arg("desc"))
            .def("register_block", &PyBlockRegistry::registerBlockWithName, nb::arg("name"), nb::arg("desc"))
            .def("name", &PyBlockRegistry::name, nb::arg("id"))
            .def("is_solid", &PyBlockRegistry::isSolid, nb::arg("id"))
            .def("is_opaque", &PyBlockRegistry::isOpaque, nb::arg("id"))
            .def("find", &PyBlockRegistry::find, nb::arg("name"))
            .def("__len__", &PyBlockRegistry::size)
            .def_ro_static("AIR", &mge::kAir);

        // --- RaycastResult ---
        nb::class_<PyRaycastResult>(m, "RaycastResult")
            .def_ro("hit", &PyRaycastResult::hit)
            .def_ro("point", &PyRaycastResult::point)
            .def_prop_ro("block_pos",
                         [](const PyRaycastResult& r) {
                             return nb::make_tuple(r.blockPos.x, r.blockPos.y, r.blockPos.z);
                         })
            .def_prop_ro(
                "normal",
                [](const PyRaycastResult& r) { return nb::make_tuple(r.normal.x, r.normal.y, r.normal.z); })
            .def_ro("distance", &PyRaycastResult::distance)
            .def_ro("block_id", &PyRaycastResult::blockId)
            .def("__repr__", [](const PyRaycastResult& r) {
                if (!r.hit) return std::string("RaycastResult(hit=False)");
                char buf[128];
                std::snprintf(buf, sizeof(buf), "RaycastResult(hit=True, block=[%d, %d, %d], dist=%.2f)",
                              r.blockPos.x, r.blockPos.y, r.blockPos.z, static_cast<double>(r.distance));
                return std::string(buf);
            });

        // --- VoxelWorld ---
        nb::class_<PyVoxelWorld>(m, "VoxelWorld")
            .def(nb::init<std::shared_ptr<PyBlockRegistry>, nanobind::object>(),
                 nb::arg("registry") = nullptr, nb::arg("generator") = nb::none())
            .def("get_block", &PyVoxelWorld::getBlock, nb::arg("x"), nb::arg("y"), nb::arg("z"))
            .def("set_block", &PyVoxelWorld::setBlock, nb::arg("x"), nb::arg("y"), nb::arg("z"),
                 nb::arg("block_id"))
            .def("load_chunk", &PyVoxelWorld::loadChunk, nb::arg("cx"), nb::arg("cy"), nb::arg("cz"))
            .def("is_chunk_loaded", &PyVoxelWorld::isChunkLoaded, nb::arg("cx"), nb::arg("cy"), nb::arg("cz"))
            .def("raycast", &PyVoxelWorld::raycast, nb::arg("origin"), nb::arg("direction"),
                 nb::arg("max_dist") = 50.0f);
    }

} // namespace mge::python
