#include "Render.h"
#include "App.h"

#include <cstring>

namespace mge::python {

    PyMesh::~PyMesh() {
        destroy();
    }

    void PyMesh::destroy() {
        if (m_alive && m_app && m_app->gpu()) {
            m_app->gpu()->device().tracker().unregisterResource(m_trackerId);
        }
        m_gpuMesh = mge::GpuMesh{};
        m_alive = false;
        m_app = nullptr;
    }

    std::shared_ptr<PyMesh> PyMesh::createAndRegister(mge::MeshData data, mge::Application* app) {
        auto* pyApp = app ? static_cast<PyApplication*>(app) : PyApplication::activeApp();
        auto mesh = std::make_shared<PyMesh>();
        mesh->m_app = pyApp;
        mesh->m_cpuMesh = std::move(data);
        mesh->m_alive = false;

        if (pyApp && pyApp->gpu()) {
            mesh->m_gpuMesh = pyApp->gpu()->device().uploadMesh(mesh->m_cpuMesh);
            mesh->m_alive = !mesh->m_gpuMesh.empty();
            if (mesh->m_alive) {
                PyMesh* rawPtr = mesh.get();
                mesh->m_trackerId = pyApp->gpu()->device().tracker().registerResource("GpuMesh", [rawPtr]() {
                    rawPtr->m_gpuMesh = mge::GpuMesh{};
                    rawPtr->m_alive = false;
                    rawPtr->m_app = nullptr;
                });
            }
        }
        return mesh;
    }

    std::shared_ptr<PyMesh> PyMesh::createCube(float size, mge::Application* app) {
        return createAndRegister(mge::primitives::box(glm::vec3(size)), app);
    }

    std::shared_ptr<PyMesh> PyMesh::createSphere(float radius, int rings, int sectors,
                                                 mge::Application* app) {
        (void)rings;
        (void)sectors;
        return createAndRegister(mge::primitives::icosphere(radius, 2), app);
    }

    std::shared_ptr<PyMesh> PyMesh::createPlane(float width, float depth, mge::Application* app) {
        return createAndRegister(mge::primitives::plane(width, depth), app);
    }

    std::shared_ptr<PyMesh> PyMesh::createCylinder(float radius, float height, int sectors,
                                                   mge::Application* app) {
        return createAndRegister(mge::primitives::cylinder(radius, height, sectors > 2 ? sectors : 16), app);
    }

    std::shared_ptr<PyMesh> PyMesh::upload(const std::vector<float>& vertices,
                                           const std::vector<uint32_t>& indices, mge::Application* app) {
        mge::MeshData data;
        const size_t vertexCount = vertices.size() / 3;
        data.positions.reserve(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) {
            data.positions.emplace_back(vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]);
        }
        data.indices = indices;
        return createAndRegister(std::move(data), app);
    }

    // --- Texture2D ---
    PyTexture2D::~PyTexture2D() {
        destroy();
    }

    void PyTexture2D::destroy() {
        if (m_alive && m_app && m_app->gpu()) {
            m_app->gpu()->device().tracker().unregisterResource(m_trackerId);
        }
        m_texture = mge::Texture2D{};
        m_alive = false;
        m_app = nullptr;
    }

    std::shared_ptr<PyTexture2D> PyTexture2D::createRgba(int width, int height, const uint8_t* pixels,
                                                         size_t byteCount, mge::Application* app) {
        auto* pyApp = app ? static_cast<PyApplication*>(app) : PyApplication::activeApp();
        auto tex = std::make_shared<PyTexture2D>();
        tex->m_app = pyApp;
        tex->m_width = width;
        tex->m_height = height;
        tex->m_alive = false;

        if (pyApp && pyApp->gpu() && pixels && byteCount >= static_cast<size_t>(width * height * 4)) {
            tex->m_texture = mge::Texture2D::fromPixels(pyApp->gpu()->device(), static_cast<uint32_t>(width),
                                                        static_cast<uint32_t>(height), pixels);
            tex->m_alive = static_cast<bool>(tex->m_texture);
            if (tex->m_alive) {
                PyTexture2D* rawPtr = tex.get();
                tex->m_trackerId = pyApp->gpu()->device().tracker().registerResource("Texture2D", [rawPtr]() {
                    rawPtr->m_texture = mge::Texture2D{};
                    rawPtr->m_alive = false;
                    rawPtr->m_app = nullptr;
                });
            }
        }
        return tex;
    }

    std::shared_ptr<PyTexture2D> PyTexture2D::createSolid(int width, int height, const glm::vec4& color,
                                                          mge::Application* app) {
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height * 4));
        const auto r = static_cast<uint8_t>(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
        const auto g = static_cast<uint8_t>(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f);
        const auto b = static_cast<uint8_t>(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f);
        const auto a = static_cast<uint8_t>(glm::clamp(color.a, 0.0f, 1.0f) * 255.0f);
        for (size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i] = r;
            pixels[i + 1] = g;
            pixels[i + 2] = b;
            pixels[i + 3] = a;
        }
        return createRgba(width, height, pixels.data(), pixels.size(), app);
    }

    // --- Renderer ---
    void PyRenderer::drawMesh(const std::shared_ptr<PyMesh>& mesh, const glm::mat4& transform,
                              const glm::vec4& tint) {
        if (!mesh) return;
        if (!mesh->isAlive()) {
            throw EngineError("Cannot draw inert mesh: GPU resource was released at application shutdown");
        }
        if (!m_app || !m_app->gpu() || !m_app->meshRenderer()) return;

        const auto* frame = m_app->gpu()->frame();
        if (!frame) return;

        if (!m_app->inRenderPass()) {
            glm::vec4 clearColor(0.08f, 0.12f, 0.18f, 1.0f);
            clearColor = glm::vec4(m_app->dayNight().sky().zenithColor, 1.0f);
            if (!m_app->meshRenderer()->begin(*frame->cmd, frame->swapchain, frame->width, frame->height,
                                              clearColor)) {
                return;
            }

            const glm::mat4 viewProj = m_app->projMatrix() * m_app->viewMatrix();
            glm::vec3 camPos(0.0f);
            if (m_app->customCamera()) {
                const glm::mat4 invView = glm::inverse(m_app->viewMatrix());
                camPos = glm::vec3(invView[3]);
            }
            m_app->meshRenderer()->setCamera(viewProj, camPos);
            m_app->meshRenderer()->setLighting(m_app->currentLighting());

            if (m_app->skyRenderer()) {
                m_app->skyRenderer()->draw(*m_app->meshRenderer(), glm::inverse(viewProj), camPos,
                                           m_app->dayNight().sky());
            }
            m_app->setInRenderPass(true);
        }

        (void)tint;
        mge::MeshMaterial mat;
        m_app->meshRenderer()->draw(mesh->gpuMesh(), transform, nullptr, mge::MeshPass::Opaque, mat);
    }

    void PyRenderer::drawMeshWithTransform(const std::shared_ptr<PyMesh>& mesh,
                                           const mge::Transform& transform, const glm::vec4& tint) {
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position) *
                                glm::mat4_cast(transform.rotation) *
                                glm::scale(glm::mat4(1.0f), transform.scale);
        drawMesh(mesh, model, tint);
    }

    void PyRenderer::setLighting(const PyLighting& lighting) {
        if (!m_app) return;
        mge::Lighting l{};
        l.sunDirection = glm::normalize(lighting.sunDir);
        l.sunColor = lighting.sunColor * lighting.sunIntensity;
        l.skyAmbient = lighting.skyAmbient;
        l.groundAmbient = lighting.groundAmbient;
        l.fogColor = lighting.fogColor;
        l.fog = (lighting.fogDensity > 0.0f);
        if (lighting.fogDensity > 0.0f) {
            l.fogEnd = 1.0f / lighting.fogDensity;
            l.fogStart = l.fogEnd * 0.4f;
        }
        m_app->setCurrentLighting(l);
    }

    void PyRenderer::setCamera(const glm::mat4& view, const glm::mat4& proj) {
        if (!m_app) return;
        m_app->setViewMatrix(view);
        m_app->setProjMatrix(proj);
    }

    void PyRenderer::setTimeOfDay(float hour) {
        if (!m_app) return;
        m_app->dayNight().hour = hour;
    }

    void PyRenderer::advanceTime(float hours) {
        if (!m_app) return;
        m_app->dayNight().advance(hours);
    }

    void PyRenderer::drawRect(float x, float y, float w, float h, const glm::vec4& color) {
        if (!m_app || !m_app->uiRenderer()) return;
        m_app->uiRenderer()->rect(glm::vec2(x, y), glm::vec2(w, h), color);
    }

    void PyRenderer::drawRectOutline(float x, float y, float w, float h, const glm::vec4& color,
                                     float thickness) {
        if (!m_app || !m_app->uiRenderer()) return;
        m_app->uiRenderer()->rectOutline(glm::vec2(x, y), glm::vec2(w, h), color, thickness);
    }

    void PyRenderer::drawLine(float x0, float y0, float x1, float y1, const glm::vec4& color,
                              float thickness) {
        if (!m_app || !m_app->uiRenderer()) return;
        m_app->uiRenderer()->line(glm::vec2(x0, y0), glm::vec2(x1, y1), color, thickness);
    }

    void PyRenderer::drawText(const std::string& text, float x, float y, float scale,
                              const glm::vec4& color) {
        if (!m_app || !m_app->uiRenderer()) return;
        m_app->uiRenderer()->text(glm::vec2(x, y), text, color, scale);
    }

    void PyRenderer::drawImage(const std::shared_ptr<PyTexture2D>& texture, float x, float y, float w,
                               float h, const glm::vec4& color) {
        if (!texture) return;
        if (!texture->isAlive()) {
            throw EngineError("Cannot draw inert texture: GPU resource was released at shutdown");
        }
        if (!m_app || !m_app->uiRenderer()) return;
        m_app->uiRenderer()->image(&texture->texture(), glm::vec2(x, y), glm::vec2(w, h), color);
    }

    std::shared_ptr<PyMesh> PyRenderer::createCube(float size) {
        return PyMesh::createCube(size, m_app);
    }

    std::shared_ptr<PyMesh> PyRenderer::createSphere(float radius, int rings, int sectors) {
        return PyMesh::createSphere(radius, rings, sectors, m_app);
    }

    std::shared_ptr<PyMesh> PyRenderer::createPlane(float width, float depth) {
        return PyMesh::createPlane(width, depth, m_app);
    }

    std::shared_ptr<PyMesh> PyRenderer::createCylinder(float radius, float height, int sectors) {
        return PyMesh::createCylinder(radius, height, sectors, m_app);
    }

    void initRender(nanobind::module_& m) {
        namespace nb = nanobind;

        // --- Mesh ---
        nb::class_<PyMesh>(m, "Mesh")
            .def_static("create_cube", &PyMesh::createCube, nb::arg("size") = 1.0f, nb::arg("app") = nullptr)
            .def_static("create_sphere", &PyMesh::createSphere, nb::arg("radius") = 0.5f,
                        nb::arg("rings") = 16, nb::arg("sectors") = 16, nb::arg("app") = nullptr)
            .def_static("create_plane", &PyMesh::createPlane, nb::arg("width") = 1.0f,
                        nb::arg("depth") = 1.0f, nb::arg("app") = nullptr)
            .def_static("create_cylinder", &PyMesh::createCylinder, nb::arg("radius") = 0.5f,
                        nb::arg("height") = 1.0f, nb::arg("sectors") = 16, nb::arg("app") = nullptr)
            .def_static("upload", &PyMesh::upload, nb::arg("vertices"), nb::arg("indices"),
                        nb::arg("app") = nullptr)
            .def_prop_ro("is_alive", &PyMesh::isAlive)
            .def("destroy", &PyMesh::destroy);

        // --- Texture2D ---
        nb::class_<PyTexture2D>(m, "Texture2D")
            .def_static(
                "create_rgba",
                [](int w, int h, nb::bytes bytes, mge::Application* app) {
                    return PyTexture2D::createRgba(w, h, reinterpret_cast<const uint8_t*>(bytes.c_str()),
                                                   bytes.size(), app);
                },
                nb::arg("width"), nb::arg("height"), nb::arg("pixels"), nb::arg("app") = nullptr)
            .def_static("create_solid", &PyTexture2D::createSolid, nb::arg("width"), nb::arg("height"),
                        nb::arg("color"), nb::arg("app") = nullptr)
            .def_prop_ro("is_alive", &PyTexture2D::isAlive)
            .def_prop_ro("width", &PyTexture2D::width)
            .def_prop_ro("height", &PyTexture2D::height)
            .def("destroy", &PyTexture2D::destroy);

        // --- Lighting ---
        nb::class_<PyLighting>(m, "Lighting")
            .def(nb::init<>())
            .def(
                "__init__",
                [](PyLighting* self, const glm::vec3& sunDir, const glm::vec3& sunColor, float sunIntensity,
                   const glm::vec3& skyAmbient, const glm::vec3& groundAmbient, const glm::vec3& fogColor,
                   float fogDensity) {
                    new (self) PyLighting();
                    self->sunDir = sunDir;
                    self->sunColor = sunColor;
                    self->sunIntensity = sunIntensity;
                    self->skyAmbient = skyAmbient;
                    self->groundAmbient = groundAmbient;
                    self->fogColor = fogColor;
                    self->fogDensity = fogDensity;
                },
                nb::arg("sun_dir") = glm::vec3(0.0f, -1.0f, 0.0f), nb::arg("sun_color") = glm::vec3(1.0f),
                nb::arg("sun_intensity") = 1.0f, nb::arg("sky_ambient") = glm::vec3(0.2f, 0.2f, 0.25f),
                nb::arg("ground_ambient") = glm::vec3(0.1f),
                nb::arg("fog_color") = glm::vec3(0.7f, 0.8f, 0.9f), nb::arg("fog_density") = 0.0f)
            .def_rw("sun_dir", &PyLighting::sunDir)
            .def_rw("sun_color", &PyLighting::sunColor)
            .def_rw("sun_intensity", &PyLighting::sunIntensity)
            .def_rw("sky_ambient", &PyLighting::skyAmbient)
            .def_rw("ground_ambient", &PyLighting::groundAmbient)
            .def_rw("fog_color", &PyLighting::fogColor)
            .def_rw("fog_density", &PyLighting::fogDensity);

        // --- Renderer ---
        nb::class_<PyRenderer>(m, "Renderer")
            .def(
                "draw_mesh",
                [](PyRenderer& r, const std::shared_ptr<PyMesh>& mesh, nb::object transform,
                   const glm::vec4& tint) {
                    if (nb::isinstance<mge::Transform>(transform)) {
                        r.drawMeshWithTransform(mesh, nb::cast<mge::Transform>(transform), tint);
                    } else if (nb::isinstance<glm::mat4>(transform)) {
                        r.drawMesh(mesh, nb::cast<glm::mat4>(transform), tint);
                    } else {
                        throw nb::type_error("transform must be Mat4 or Transform");
                    }
                },
                nb::arg("mesh"), nb::arg("transform"), nb::arg("tint") = glm::vec4(1.0f))
            .def("set_lighting", &PyRenderer::setLighting, nb::arg("lighting"))
            .def("set_camera", &PyRenderer::setCamera, nb::arg("view"), nb::arg("proj"))
            .def("set_time_of_day", &PyRenderer::setTimeOfDay, nb::arg("hour"))
            .def("advance_time", &PyRenderer::advanceTime, nb::arg("hours"))
            .def("draw_rect", &PyRenderer::drawRect, nb::arg("x"), nb::arg("y"), nb::arg("w"), nb::arg("h"),
                 nb::arg("color"))
            .def("draw_rect_outline", &PyRenderer::drawRectOutline, nb::arg("x"), nb::arg("y"), nb::arg("w"),
                 nb::arg("h"), nb::arg("color"), nb::arg("thickness") = 1.0f)
            .def("draw_line", &PyRenderer::drawLine, nb::arg("x0"), nb::arg("y0"), nb::arg("x1"),
                 nb::arg("y1"), nb::arg("color"), nb::arg("thickness") = 1.0f)
            .def("draw_text", &PyRenderer::drawText, nb::arg("text"), nb::arg("x"), nb::arg("y"),
                 nb::arg("scale") = 1.0f, nb::arg("color") = glm::vec4(1.0f))
            .def("draw_image", &PyRenderer::drawImage, nb::arg("texture"), nb::arg("x"), nb::arg("y"),
                 nb::arg("w"), nb::arg("h"), nb::arg("color") = glm::vec4(1.0f))
            .def("create_cube", &PyRenderer::createCube, nb::arg("size") = 1.0f)
            .def("create_sphere", &PyRenderer::createSphere, nb::arg("radius") = 0.5f, nb::arg("rings") = 16,
                 nb::arg("sectors") = 16)
            .def("create_plane", &PyRenderer::createPlane, nb::arg("width") = 1.0f, nb::arg("depth") = 1.0f)
            .def("create_cylinder", &PyRenderer::createCylinder, nb::arg("radius") = 0.5f,
                 nb::arg("height") = 1.0f, nb::arg("sectors") = 16);
    }

} // namespace mge::python
