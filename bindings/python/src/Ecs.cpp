#include "Ecs.h"

namespace mge::python {

    namespace {
        std::string makePyComponentKey(nanobind::handle cls) {
            namespace nb = nanobind;
            const std::string modName = nb::cast<std::string>(nb::str(cls.attr("__module__")));
            const std::string qualName = nb::hasattr(cls, "__qualname__")
                                             ? nb::cast<std::string>(nb::str(cls.attr("__qualname__")))
                                             : nb::cast<std::string>(nb::str(cls.attr("__name__")));
            return "py:" + modName + "." + qualName;
        }
    } // namespace

    PyWorld::PyWorld() : m_ownedWorld(std::make_unique<mge::World>()), m_worldRef(m_ownedWorld.get()) {}
    PyWorld::PyWorld(mge::World* world) : m_worldRef(world) {}

    PyWorld::~PyWorld() {
        nanobind::gil_scoped_acquire gil;
        m_pyComponents.clear();
        m_pySystems.clear();
    }

    PyEntity PyWorld::create() {
        const mge::Entity e = registry().create();
        return PyEntity(e);
    }

    void PyWorld::destroy(PyEntity entity) {
        if (entity.isNull()) return;
        m_pyComponents.erase(entity.id);
        registry().destroy(entity.toEngine());
    }

    bool PyWorld::isAlive(PyEntity entity) const {
        if (entity.isNull()) return false;
        return m_worldRef->registry().valid(entity.toEngine());
    }

    void PyWorld::add(PyEntity entity, nanobind::object comp) {
        namespace nb = nanobind;
        if (!isAlive(entity)) {
            throw EngineError("Cannot add component to invalid entity");
        }
        const mge::Entity e = entity.toEngine();

        if (nb::isinstance<mge::Transform>(comp)) {
            registry().add<mge::Transform>(e, nb::cast<mge::Transform>(comp));
            return;
        }
        if (nb::isinstance<mge::Camera>(comp)) {
            registry().add<mge::Camera>(e, nb::cast<mge::Camera>(comp));
            return;
        }
        if (nb::isinstance<mge::CharacterController>(comp)) {
            registry().add<mge::CharacterController>(e, nb::cast<mge::CharacterController>(comp));
            return;
        }
        if (nb::isinstance<mge::RigidBody>(comp)) {
            registry().add<mge::RigidBody>(e, nb::cast<mge::RigidBody>(comp));
            return;
        }

        // Componente de Python personalizado
        const std::string typeName = makePyComponentKey(comp.type());
        const mge::ComponentTypeId compId =
            registry().registerRuntimeComponent(typeName, sizeof(void*), alignof(void*));
        if (compId == mge::kInvalidComponentType) {
            throw EngineError("Failed to register runtime component: " + typeName);
        }
        if (registry().addRaw(e, compId) == nullptr) {
            throw EngineError("Failed to add runtime component: " + typeName);
        }

        auto* typeObj = reinterpret_cast<PyTypeObject*>(comp.type().ptr());
        m_pyComponents[entity.id][typeObj] = comp;
    }

    bool PyWorld::has(PyEntity entity, nanobind::object compType) const {
        namespace nb = nanobind;
        if (!isAlive(entity)) return false;
        const mge::Entity e = entity.toEngine();

        if (compType.is(nb::type<mge::Transform>())) {
            return registry().has<mge::Transform>(e);
        }
        if (compType.is(nb::type<mge::Camera>())) {
            return registry().has<mge::Camera>(e);
        }
        if (compType.is(nb::type<mge::CharacterController>())) {
            return registry().has<mge::CharacterController>(e);
        }
        if (compType.is(nb::type<mge::RigidBody>())) {
            return registry().has<mge::RigidBody>(e);
        }

        auto it = m_pyComponents.find(entity.id);
        if (it == m_pyComponents.end()) return false;
        auto* typeObj = reinterpret_cast<PyTypeObject*>(compType.ptr());
        return it->second.find(typeObj) != it->second.end();
    }

    nanobind::object PyWorld::get(PyEntity entity, nanobind::object compType) {
        namespace nb = nanobind;
        if (!isAlive(entity)) {
            throw nb::key_error("Entity is not alive");
        }
        const mge::Entity e = entity.toEngine();

        if (compType.is(nb::type<mge::Transform>())) {
            if (!registry().has<mge::Transform>(e)) {
                throw nb::key_error("Entity does not have Transform component");
            }
            return nb::cast(&registry().get<mge::Transform>(e), nb::rv_policy::reference);
        }
        if (compType.is(nb::type<mge::Camera>())) {
            if (!registry().has<mge::Camera>(e)) {
                throw nb::key_error("Entity does not have Camera component");
            }
            return nb::cast(&registry().get<mge::Camera>(e), nb::rv_policy::reference);
        }
        if (compType.is(nb::type<mge::CharacterController>())) {
            if (!registry().has<mge::CharacterController>(e)) {
                throw nb::key_error("Entity does not have CharacterController component");
            }
            return nb::cast(&registry().get<mge::CharacterController>(e), nb::rv_policy::reference);
        }
        if (compType.is(nb::type<mge::RigidBody>())) {
            if (!registry().has<mge::RigidBody>(e)) {
                throw nb::key_error("Entity does not have RigidBody component");
            }
            return nb::cast(&registry().get<mge::RigidBody>(e), nb::rv_policy::reference);
        }

        auto it = m_pyComponents.find(entity.id);
        if (it != m_pyComponents.end()) {
            auto* typeObj = reinterpret_cast<PyTypeObject*>(compType.ptr());
            auto compIt = it->second.find(typeObj);
            if (compIt != it->second.end()) {
                return compIt->second;
            }
        }
        throw nb::key_error("Entity does not have requested component");
    }

    void PyWorld::remove(PyEntity entity, nanobind::object compType) {
        namespace nb = nanobind;
        if (!isAlive(entity)) return;
        const mge::Entity e = entity.toEngine();

        if (compType.is(nb::type<mge::Transform>())) {
            registry().remove<mge::Transform>(e);
            return;
        }
        if (compType.is(nb::type<mge::Camera>())) {
            registry().remove<mge::Camera>(e);
            return;
        }
        if (compType.is(nb::type<mge::CharacterController>())) {
            registry().remove<mge::CharacterController>(e);
            return;
        }
        if (compType.is(nb::type<mge::RigidBody>())) {
            registry().remove<mge::RigidBody>(e);
            return;
        }

        auto it = m_pyComponents.find(entity.id);
        if (it != m_pyComponents.end()) {
            auto* typeObj = reinterpret_cast<PyTypeObject*>(compType.ptr());
            it->second.erase(typeObj);
            if (it->second.empty()) {
                m_pyComponents.erase(it);
            }
        }
        const std::string typeName = makePyComponentKey(compType);
        const mge::ComponentTypeId compId =
            registry().registerRuntimeComponent(typeName, sizeof(void*), alignof(void*));
        if (compId != mge::kInvalidComponentType) {
            registry().removeRaw(e, compId);
        }
    }

    nanobind::object PyWorld::each(nanobind::args args) {
        namespace nb = nanobind;
        if (args.size() == 0) return nb::list();

        bool hasCallback = false;
        nb::object callback;
        std::vector<nb::object> types;

        if (!PyType_Check(args[0].ptr()) && nb::isinstance<nb::callable>(args[0])) {
            hasCallback = true;
            callback = nb::cast<nb::object>(args[0]);
            for (size_t i = 1; i < args.size(); ++i) {
                types.push_back(nb::cast<nb::object>(args[i]));
            }
        } else {
            for (size_t i = 0; i < args.size(); ++i) {
                types.push_back(nb::cast<nb::object>(args[i]));
            }
        }

        nb::list results;
        std::unordered_map<uint64_t, bool> seen;
        for (const auto& pair : m_pyComponents) {
            seen[pair.first] = true;
        }
        for (const auto& typeObj : types) {
            if (typeObj.is(nb::type<mge::Transform>())) {
                registry().each<mge::Transform>(
                    [&seen](mge::Entity e, mge::Transform&) { seen[entityToId(e)] = true; });
            } else if (typeObj.is(nb::type<mge::Camera>())) {
                registry().each<mge::Camera>(
                    [&seen](mge::Entity e, mge::Camera&) { seen[entityToId(e)] = true; });
            } else if (typeObj.is(nb::type<mge::CharacterController>())) {
                registry().each<mge::CharacterController>(
                    [&seen](mge::Entity e, mge::CharacterController&) { seen[entityToId(e)] = true; });
            } else if (typeObj.is(nb::type<mge::RigidBody>())) {
                registry().each<mge::RigidBody>(
                    [&seen](mge::Entity e, mge::RigidBody&) { seen[entityToId(e)] = true; });
            }
        }

        for (const auto& item : seen) {
            const PyEntity ent(item.first);
            if (!isAlive(ent)) continue;

            bool matchesAll = true;
            for (const auto& t : types) {
                if (!has(ent, t)) {
                    matchesAll = false;
                    break;
                }
            }
            if (!matchesAll) continue;

            std::vector<nb::object> compList;
            for (const auto& t : types) {
                compList.push_back(get(ent, t));
            }

            if (hasCallback) {
                if (compList.size() == 1) {
                    callback(ent, compList[0]);
                } else if (compList.size() == 2) {
                    callback(ent, compList[0], compList[1]);
                } else if (compList.size() == 3) {
                    callback(ent, compList[0], compList[1], compList[2]);
                } else {
                    nb::list callArgs;
                    callArgs.append(ent);
                    for (auto& c : compList) callArgs.append(c);
                    callback(*nb::tuple(callArgs));
                }
            } else {
                nb::list tupleItems;
                tupleItems.append(ent);
                for (auto& c : compList) tupleItems.append(c);
                results.append(nb::tuple(tupleItems));
            }
        }

        return hasCallback ? nb::object(nb::none()) : nb::object(results);
    }

    void PyWorld::addSystem(nanobind::object phase, const std::string& name, nanobind::object fn) {
        namespace nb = nanobind;
        mge::SystemPhase sp = mge::SystemPhase::Update;
        int p = 1;
        try {
            p = nb::cast<int>(phase);
        } catch (...) {
            p = 1;
        }
        if (p == 0) sp = mge::SystemPhase::Fixed;
        if (p == 2) sp = mge::SystemPhase::Render;

        auto holder = std::make_shared<nanobind::object>(fn);
        m_pySystems.push_back(holder);
        std::weak_ptr<nanobind::object> weakHolder = holder;

        m_worldRef->addSystem(sp, name, [weakHolder](mge::World&, float dt) {
            auto spHolder = weakHolder.lock();
            if (!spHolder || spHolder->is_none()) return;
            nb::gil_scoped_acquire gil;
            try {
                (*spHolder)(dt);
            } catch (const nb::python_error& e) {
                MGE_LOG_ERROR("ECS", "Error en sistema Python: ", e.what());
            }
        });
    }

    void PyWorld::clear() {
        nanobind::gil_scoped_acquire gil;
        m_pyComponents.clear();
        m_pySystems.clear();
    }

    void initEcs(nanobind::module_& m) {
        namespace nb = nanobind;

        // --- SystemPhase ---
        nb::enum_<mge::SystemPhase>(m, "SystemPhase")
            .value("FIXED", mge::SystemPhase::Fixed)
            .value("UPDATE", mge::SystemPhase::Update)
            .value("RENDER", mge::SystemPhase::Render)
            .export_values();

        // --- Entity ---
        nb::class_<PyEntity>(m, "Entity")
            .def(nb::init<>())
            .def_prop_ro("id", [](const PyEntity& e) { return e.id; })
            .def_prop_ro("index", &PyEntity::index)
            .def_prop_ro("generation", &PyEntity::generation)
            .def_prop_ro("is_null", &PyEntity::isNull)
            .def(nb::self == nb::self)
            .def(nb::self != nb::self)
            .def("__hash__", [](const PyEntity& e) { return e.id; })
            .def("__repr__", [](const PyEntity& e) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Entity(index=%u, gen=%u)", e.index(), e.generation());
                return std::string(buf);
            });

        // --- Transform ---
        nb::class_<mge::Transform>(m, "Transform")
            .def(nb::init<>())
            .def(
                "__init__",
                [](mge::Transform* self, const glm::vec3& pos, const glm::quat& rot, const glm::vec3& sc) {
                    new (self) mge::Transform();
                    self->position = pos;
                    self->rotation = rot;
                    self->scale = sc;
                },
                nb::arg("position") = glm::vec3(0.0f),
                nb::arg("rotation") = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), nb::arg("scale") = glm::vec3(1.0f))
            .def_rw("position", &mge::Transform::position)
            .def_rw("rotation", &mge::Transform::rotation)
            .def_rw("scale", &mge::Transform::scale)
            .def("forward", [](const mge::Transform& t) { return t.rotation * glm::vec3(0.0f, 0.0f, -1.0f); })
            .def("right", [](const mge::Transform& t) { return t.rotation * glm::vec3(1.0f, 0.0f, 0.0f); })
            .def("up", [](const mge::Transform& t) { return t.rotation * glm::vec3(0.0f, 1.0f, 0.0f); })
            .def("matrix",
                 [](const mge::Transform& t) {
                     return glm::translate(glm::mat4(1.0f), t.position) * glm::mat4_cast(t.rotation) *
                            glm::scale(glm::mat4(1.0f), t.scale);
                 })
            .def("__repr__", [](const mge::Transform& t) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "Transform(pos=[%.2f, %.2f, %.2f], scale=[%.2f, %.2f, %.2f])",
                              static_cast<double>(t.position.x), static_cast<double>(t.position.y),
                              static_cast<double>(t.position.z), static_cast<double>(t.scale.x),
                              static_cast<double>(t.scale.y), static_cast<double>(t.scale.z));
                return std::string(buf);
            });

        // --- Camera ---
        nb::class_<mge::Camera>(m, "Camera")
            .def(nb::init<>())
            .def(
                "__init__",
                [](mge::Camera* self, float fovY, float nearPlane, float farPlane, bool isOrtho,
                   float orthoSize) {
                    new (self) mge::Camera();
                    self->projection =
                        isOrtho ? mge::ProjectionType::Orthographic : mge::ProjectionType::Perspective;
                    self->fovY = fovY;
                    self->nearPlane = nearPlane;
                    self->farPlane = farPlane;
                    self->orthoHeight = orthoSize;
                },
                nb::arg("fov_y") = glm::radians(60.0f), nb::arg("near_plane") = 0.1f,
                nb::arg("far_plane") = 1000.0f, nb::arg("is_orthographic") = false,
                nb::arg("ortho_size") = 10.0f)
            .def_rw("fov_y", &mge::Camera::fovY)
            .def_prop_rw(
                "fov", [](const mge::Camera& c) { return glm::degrees(c.fovY); },
                [](mge::Camera& c, float deg) { c.fovY = glm::radians(deg); })
            .def_rw("near_plane", &mge::Camera::nearPlane)
            .def_rw("far_plane", &mge::Camera::farPlane)
            .def_prop_rw(
                "is_orthographic",
                [](const mge::Camera& c) { return c.projection == mge::ProjectionType::Orthographic; },
                [](mge::Camera& c, bool ortho) {
                    c.projection =
                        ortho ? mge::ProjectionType::Orthographic : mge::ProjectionType::Perspective;
                })
            .def_rw("ortho_size", &mge::Camera::orthoHeight)
            .def("__repr__", [](const mge::Camera& c) {
                char buf[96];
                std::snprintf(buf, sizeof(buf), "Camera(fov=%.1f deg, near=%.2f, far=%.1f)",
                              static_cast<double>(glm::degrees(c.fovY)), static_cast<double>(c.nearPlane),
                              static_cast<double>(c.farPlane));
                return std::string(buf);
            });

        // --- CharacterController ---
        nb::class_<mge::CharacterController>(m, "CharacterController")
            .def(nb::init<>())
            .def(
                "__init__",
                [](mge::CharacterController* self, int mode, float speed, float jumpSpeed, float gravity,
                   bool grounded) {
                    new (self) mge::CharacterController();
                    self->mode = mode == 1 ? mge::CharacterMode::Fly : mge::CharacterMode::Walk;
                    self->walkSpeed = speed;
                    self->jumpSpeed = jumpSpeed;
                    self->gravity = gravity;
                    self->grounded = grounded;
                },
                nb::arg("mode") = 0, nb::arg("speed") = 5.0f, nb::arg("jump_speed") = 8.0f,
                nb::arg("gravity") = 20.0f, nb::arg("is_grounded") = false)
            .def_prop_rw(
                "mode", [](const mge::CharacterController& cc) { return static_cast<int>(cc.mode); },
                [](mge::CharacterController& cc, int mode) {
                    cc.mode = mode == 1 ? mge::CharacterMode::Fly : mge::CharacterMode::Walk;
                })
            .def_rw("speed", &mge::CharacterController::walkSpeed)
            .def_rw("jump_speed", &mge::CharacterController::jumpSpeed)
            .def_rw("gravity", &mge::CharacterController::gravity)
            .def_rw("is_grounded", &mge::CharacterController::grounded)
            .def("__repr__", [](const mge::CharacterController& cc) {
                char buf[96];
                std::snprintf(buf, sizeof(buf), "CharacterController(speed=%.1f, grounded=%s)",
                              static_cast<double>(cc.walkSpeed), cc.grounded ? "true" : "false");
                return std::string(buf);
            });

        // --- RigidBody ---
        nb::class_<mge::RigidBody>(m, "RigidBody")
            .def(nb::init<>())
            .def(
                "__init__",
                [](mge::RigidBody* self, const glm::vec3& vel, float mass, float restitution, float gravScale,
                   bool useGrav, bool kinematic) {
                    new (self) mge::RigidBody();
                    self->velocity = vel;
                    self->mass = kinematic ? 0.0f : mass;
                    self->restitution = restitution;
                    self->gravityScale = useGrav ? gravScale : 0.0f;
                },
                nb::arg("velocity") = glm::vec3(0.0f), nb::arg("mass") = 1.0f, nb::arg("restitution") = 0.0f,
                nb::arg("gravity_scale") = 1.0f, nb::arg("use_gravity") = true,
                nb::arg("is_kinematic") = false)
            .def_rw("velocity", &mge::RigidBody::velocity)
            .def_rw("mass", &mge::RigidBody::mass)
            .def_rw("restitution", &mge::RigidBody::restitution)
            .def_rw("gravity_scale", &mge::RigidBody::gravityScale)
            .def_prop_rw(
                "use_gravity", [](const mge::RigidBody& rb) { return rb.gravityScale > 0.0f; },
                [](mge::RigidBody& rb, bool use) {
                    if (!use) {
                        rb.gravityScale = 0.0f;
                    } else if (rb.gravityScale == 0.0f) {
                        rb.gravityScale = 1.0f;
                    }
                })
            .def_prop_rw(
                "is_kinematic", [](const mge::RigidBody& rb) { return rb.mass <= 0.0f; },
                [](mge::RigidBody& rb, bool kin) {
                    if (kin) {
                        rb.mass = 0.0f;
                    } else if (rb.mass <= 0.0f) {
                        rb.mass = 1.0f;
                    }
                })
            .def("__repr__", [](const mge::RigidBody& rb) {
                char buf[96];
                std::snprintf(buf, sizeof(buf), "RigidBody(mass=%.1f, vel=[%.1f, %.1f, %.1f])",
                              static_cast<double>(rb.mass), static_cast<double>(rb.velocity.x),
                              static_cast<double>(rb.velocity.y), static_cast<double>(rb.velocity.z));
                return std::string(buf);
            });

        // --- World ---
        nb::class_<PyWorld>(m, "World")
            .def(nb::init<>())
            .def("create", &PyWorld::create)
            .def("destroy", &PyWorld::destroy, nb::arg("entity"))
            .def("is_alive", &PyWorld::isAlive, nb::arg("entity"))
            .def("add", &PyWorld::add, nb::arg("entity"), nb::arg("component"))
            .def("has", &PyWorld::has, nb::arg("entity"), nb::arg("component_type"))
            .def("get", &PyWorld::get, nb::arg("entity"), nb::arg("component_type"))
            .def("remove", &PyWorld::remove, nb::arg("entity"), nb::arg("component_type"))
            .def("each", &PyWorld::each)
            .def("add_system", &PyWorld::addSystem, nb::arg("phase"), nb::arg("name"), nb::arg("fn"))
            .def("clear", &PyWorld::clear);
    }

} // namespace mge::python
