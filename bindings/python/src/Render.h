#pragma once

#include "Common.h"

namespace mge::python {

    class PyApplication;

    class PyMesh {
    public:
        PyMesh() = default;
        ~PyMesh();

        void destroy();
        bool isAlive() const { return m_alive; }
        const mge::MeshData& cpuMesh() const { return m_cpuMesh; }
        const mge::GpuMesh& gpuMesh() const { return m_gpuMesh; }

        static std::shared_ptr<PyMesh> createCube(float size = 1.0f, mge::Application* app = nullptr);
        static std::shared_ptr<PyMesh> createSphere(float radius = 0.5f, int rings = 16, int sectors = 16,
                                                    mge::Application* app = nullptr);
        static std::shared_ptr<PyMesh> createPlane(float width = 1.0f, float depth = 1.0f,
                                                   mge::Application* app = nullptr);
        static std::shared_ptr<PyMesh> createCylinder(float radius = 0.5f, float height = 1.0f,
                                                      int sectors = 16, mge::Application* app = nullptr);
        static std::shared_ptr<PyMesh> upload(const std::vector<float>& vertices,
                                              const std::vector<uint32_t>& indices,
                                              mge::Application* app = nullptr);
        static std::shared_ptr<PyMesh> createAndRegister(mge::MeshData data, mge::Application* app);

    private:
        friend class PyRenderer;
        PyApplication* m_app = nullptr;
        mge::GpuResourceId m_trackerId = 0;
        bool m_alive = false;
        mge::MeshData m_cpuMesh;
        mge::GpuMesh m_gpuMesh;
    };

    class PyTexture2D {
    public:
        PyTexture2D() = default;
        ~PyTexture2D();

        void destroy();
        bool isAlive() const { return m_alive; }
        int width() const { return m_width; }
        int height() const { return m_height; }
        const mge::Texture2D& texture() const { return m_texture; }

        static std::shared_ptr<PyTexture2D> createRgba(int width, int height, const uint8_t* pixels,
                                                       size_t byteCount, mge::Application* app = nullptr);
        static std::shared_ptr<PyTexture2D> createSolid(int width, int height, const glm::vec4& color,
                                                        mge::Application* app = nullptr);

    private:
        friend class PyRenderer;
        PyApplication* m_app = nullptr;
        mge::GpuResourceId m_trackerId = 0;
        bool m_alive = false;
        int m_width = 0;
        int m_height = 0;
        mge::Texture2D m_texture;
    };

    struct PyLighting {
        glm::vec3 sunDir{0.0f, -1.0f, 0.0f};
        glm::vec3 sunColor{1.0f, 1.0f, 1.0f};
        float sunIntensity = 1.0f;
        glm::vec3 skyAmbient{0.2f, 0.2f, 0.25f};
        glm::vec3 groundAmbient{0.1f, 0.1f, 0.1f};
        glm::vec3 fogColor{0.7f, 0.8f, 0.9f};
        float fogDensity = 0.0f;
    };

    class PyRenderer {
    public:
        explicit PyRenderer(PyApplication* app) : m_app(app) {}

        void drawMesh(const std::shared_ptr<PyMesh>& mesh, const glm::mat4& transform,
                      const glm::vec4& tint = glm::vec4(1.0f));
        void drawMeshWithTransform(const std::shared_ptr<PyMesh>& mesh, const mge::Transform& transform,
                                   const glm::vec4& tint = glm::vec4(1.0f));

        void setLighting(const PyLighting& lighting);
        void setCamera(const glm::mat4& view, const glm::mat4& proj);
        void setTimeOfDay(float hour);
        void advanceTime(float hours);

        // UI 2D
        void drawRect(float x, float y, float w, float h, const glm::vec4& color);
        void drawRectOutline(float x, float y, float w, float h, const glm::vec4& color,
                             float thickness = 1.0f);
        void drawLine(float x0, float y0, float x1, float y1, const glm::vec4& color, float thickness = 1.0f);
        void drawText(const std::string& text, float x, float y, float scale = 1.0f,
                      const glm::vec4& color = glm::vec4(1.0f));
        void drawImage(const std::shared_ptr<PyTexture2D>& texture, float x, float y, float w, float h,
                       const glm::vec4& color = glm::vec4(1.0f));

        std::shared_ptr<PyMesh> createCube(float size = 1.0f);
        std::shared_ptr<PyMesh> createSphere(float radius = 0.5f, int rings = 16, int sectors = 16);
        std::shared_ptr<PyMesh> createPlane(float width = 1.0f, float depth = 1.0f);
        std::shared_ptr<PyMesh> createCylinder(float radius = 0.5f, float height = 1.0f, int sectors = 16);

    private:
        PyApplication* m_app = nullptr;
    };

} // namespace mge::python
