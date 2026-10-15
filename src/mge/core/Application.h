#pragma once

#include <mge/core/Module.h>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mge {

    struct ApplicationConfig {
        std::string title = "miniGameEngine";
        int width = 1280;
        int height = 720;
        // Pista para el juego: sin ventana/GPU (tests, servidores, CI). El core nunca crea una ventana;
        // un juego headless simplemente no agrega el PlatformModule.
        bool headless = false;
        // 0 = sin límite (hasta requestStop). Útil para tests y corridas headless.
        std::uint64_t maxFrames = 0;
    };

    // Runner del motor: dueño del loop y de los módulos. Vive en el core (sin SDL ni GPU).
    // Un juego hereda de Application, agrega los módulos que necesita y define createApplication().
    class Application {
    public:
        explicit Application(ApplicationConfig config = {});
        virtual ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        // Inicializa módulos y juego, corre el loop y cierra todo. Devuelve el código de salida:
        // 0 = OK, 1 = falló la inicialización (el motivo está en el log).
        int run();

        // Termina el loop al final del frame actual
        void requestStop() { m_running = false; }
        bool isRunning() const { return m_running; }

        const ApplicationConfig& getConfig() const { return m_config; }
        std::uint64_t getFrameCount() const { return m_frameCount; }

        // Agrega un módulo (antes de run()). Devuelve una referencia para configurarlo.
        template <typename T, typename... Args> T& addModule(Args&&... args) {
            static_assert(std::is_base_of_v<Module, T>, "T debe heredar de mge::Module");
            auto module = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *module;
            m_modules.push_back(std::move(module));
            return ref;
        }

        // Busca un módulo por tipo (nullptr si no está)
        template <typename T> T* getModule() const {
            for (const auto& module : m_modules) {
                if (auto* typed = dynamic_cast<T*>(module.get())) return typed;
            }
            return nullptr;
        }

    protected:
        // Hooks del juego. onInit corre después de inicializar los módulos (ej: con contexto OpenGL).
        // Devolver false aborta el arranque.
        virtual bool onInit() { return true; }
        virtual void onUpdate(float /*dt*/) {}
        virtual void onRender() {}
        // Antes de cerrar los módulos (los recursos de GPU del juego todavía son válidos)
        virtual void onShutdown() {}

    private:
        bool initModules();
        void shutdownModules();

        ApplicationConfig m_config;
        std::vector<std::unique_ptr<Module>> m_modules;
        std::size_t m_initializedModules = 0; // Los primeros N inicializaron bien
        bool m_running = false;
        std::uint64_t m_frameCount = 0;
    };

    // Definida por el juego: devuelve su Application. La llama el main() del motor (mge_main).
    Application* createApplication();
}
