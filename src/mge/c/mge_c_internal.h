#pragma once

#include <mge/c/mge_c.h>

#include <mge/audio/AudioModule.h>
#include <mge/core/Application.h>
#include <mge/core/Clock.h>
#include <mge/core/Log.h>
#include <mge/core/Time.h>
#include <mge/core/ecs/Camera.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/gpu/GpuModule.h>
#include <mge/graphics/DayNight.h>
#include <mge/graphics/MeshRenderer.h>
#include <mge/graphics/Renderer2D.h>
#include <mge/graphics/Sky.h>
#include <mge/graphics/Texture2D.h>
#include <mge/physics/CharacterController.h>
#include <mge/physics/PhysicsWorld.h>
#include <mge/physics/RigidBody.h>
#include <mge/platform/PlatformModule.h>
#include <mge/procgen/MeshData.h>
#include <mge/procgen/Primitives.h>
#include <mge/synth/Mixer.h>
#include <mge/synth/Music.h>
#include <mge/synth/Sfx.h>
#include <mge/voxel/Block.h>
#include <mge/voxel/Chunk.h>
#include <mge/voxel/VoxelRaycast.h>
#include <mge/voxel/VoxelWorld.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mge::c_impl {

    void setLastError(std::string message);

    class CApplication;

} // namespace mge::c_impl

struct mge_mesh {
    mge_app* app = nullptr;
    mge::GpuResourceId trackerId = 0;
    bool alive = false;
    mge::MeshData cpuMesh;
    mge::GpuMesh gpuMesh;
};

struct mge_texture {
    mge_app* app = nullptr;
    mge::GpuResourceId trackerId = 0;
    bool alive = false;
    mge::Texture2D texture;
};

struct mge_audio_sample {
    mge_app* app = nullptr;
    mge::Sample sample;
    mge::SampleId sampleId = mge::kInvalidSample;
};

struct mge_block_registry {
    mge::BlockRegistry registry;
};

struct mge_voxel_world {
    const mge::BlockRegistry* registry = nullptr;
    std::unique_ptr<mge::VoxelWorld> world;
    std::unique_ptr<mge::BlockRegistry> ownedRegistry;
};

struct mge_app {
    mge::c_impl::CApplication* app = nullptr;
    mge::World* world = nullptr;

    mge::PlatformModule* platform = nullptr;
    mge::GpuModule* gpu = nullptr;
    mge::AudioModule* audio = nullptr;

    // Renderizadores creados en demanda cuando hay GPU
    std::unique_ptr<mge::MeshRenderer> meshRenderer;
    std::unique_ptr<mge::SkyRenderer> skyRenderer;
    mge::DayNight dayNight;
    std::unique_ptr<mge::Renderer2D> uiRenderer;

    // Vista y proyección de la cámara
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projMatrix{1.0f};
    bool customCamera = false;
    mge::Lighting currentLighting{};
    bool inRenderPass = false;

    // Mapeo de acciones y ejes de input
    std::unordered_map<std::string, std::string> actionBindings;
    struct AxisBinding {
        std::string negKey;
        std::string posKey;
    };
    std::unordered_map<std::string, AxisBinding> axisBindings;
    std::unordered_map<std::string, bool> injectedKeys;
    glm::vec2 mouseDelta{0.0f};
    glm::vec2 lastMousePos{0.0f};
    bool firstMouse = true;
};

namespace mge::c_impl {

    class CApplication : public Application {
    public:
        CApplication(const ApplicationConfig& config, mge_app_callbacks callbacks)
            : Application(config), m_callbacks(callbacks) {
            m_cApp.app = this;
            m_cApp.world = &world();
        }

        mge_app& cApp() { return m_cApp; }

    protected:
        bool onInit() override {
            m_cApp.platform = getModule<PlatformModule>();
            m_cApp.gpu = getModule<GpuModule>();
            m_cApp.audio = getModule<AudioModule>();

            if (m_cApp.gpu) {
                m_cApp.meshRenderer =
                    std::make_unique<MeshRenderer>(m_cApp.gpu->device(), m_cApp.gpu->swapchainFormat());
                m_cApp.skyRenderer = std::make_unique<SkyRenderer>(
                    m_cApp.gpu->device(), m_cApp.gpu->swapchainFormat(), m_cApp.gpu->device().depthFormat());
                m_cApp.uiRenderer =
                    std::make_unique<Renderer2D>(m_cApp.gpu->device(), m_cApp.gpu->swapchainFormat());
            }

            if (m_callbacks.on_init) {
                if (!m_callbacks.on_init(&m_cApp, m_callbacks.user_data)) {
                    return false;
                }
            }
            return true;
        }

        void onFixedUpdate(float fixedDt) override {
            if (m_callbacks.on_fixed_update) {
                m_callbacks.on_fixed_update(&m_cApp, fixedDt, m_callbacks.user_data);
            }
        }

        void onUpdate(float dt) override {
            if (m_cApp.platform) {
                const auto mousePos = m_cApp.platform->input().mousePosition();
                const glm::vec2 curPos{mousePos.x, mousePos.y};
                if (m_cApp.firstMouse) {
                    m_cApp.lastMousePos = curPos;
                    m_cApp.firstMouse = false;
                }
                m_cApp.mouseDelta = curPos - m_cApp.lastMousePos;
                m_cApp.lastMousePos = curPos;
            }

            if (m_callbacks.on_update) {
                m_callbacks.on_update(&m_cApp, dt, m_callbacks.user_data);
            }
        }

        void onRender() override {
            m_cApp.inRenderPass = false;

            if (m_cApp.gpu) {
                const auto* frame = m_cApp.gpu->frame();
                if (frame && m_cApp.uiRenderer) {
                    m_cApp.uiRenderer->begin(frame->width, frame->height, frame->displayScale);
                }
            }

            if (m_callbacks.on_render) {
                m_callbacks.on_render(&m_cApp, m_callbacks.user_data);
            }

            if (m_cApp.inRenderPass && m_cApp.meshRenderer) {
                m_cApp.meshRenderer->end();
                m_cApp.inRenderPass = false;
            }

            if (m_cApp.gpu) {
                const auto* frame = m_cApp.gpu->frame();
                if (frame && m_cApp.uiRenderer && m_cApp.uiRenderer->vertexCount() > 0) {
                    m_cApp.uiRenderer->flush(*frame->cmd, frame->swapchain);
                }
            }
        }

        void onShutdown() override {
            if (m_callbacks.on_shutdown) {
                m_callbacks.on_shutdown(&m_cApp, m_callbacks.user_data);
            }
            // Destruir renderizadores antes de que GpuModule destruya el device
            m_cApp.meshRenderer.reset();
            m_cApp.skyRenderer.reset();
            m_cApp.uiRenderer.reset();
        }

    private:
        mge_app_callbacks m_callbacks{};
        mge_app m_cApp;
    };

} // namespace mge::c_impl
