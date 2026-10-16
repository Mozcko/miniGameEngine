#pragma once

#include <mge/core/Clock.h>
#include <mge/core/Module.h>
#include <mge/core/Time.h>
#include <mge/core/ecs/World.h>

#include <glm/glm.hpp>

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
        // Paso fijo: onFixedUpdate corre tickRate veces por segundo (de tiempo escalado)
        double tickRate = 60.0;
        // Máximo de pasos fijos por frame: si un frame tarda mucho, el tiempo sobrante se descarta
        // (evita la "espiral de la muerte", donde cada frame acumula más trabajo que el anterior)
        int maxFixedStepsPerFrame = 5;
        // Un frame no cuenta más que esto (pausas del sistema, breakpoints, mover la ventana)
        double maxFrameTime = 0.25;
        // Sincronizar la presentación con el monitor (sin tearing; limita los FPS a los Hz de la pantalla).
        // false = lo más rápido posible. La plataforma/GPU lo aplican si existen; el core no lo usa.
        bool vsync = true;
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
        // 1 si falló la inicialización (el motivo está en el log); si no, el de setExitCode (por defecto 0).
        // La variable de entorno MGE_MAX_FRAMES (si config.maxFrames es 0) limita los frames: útil
        // para correr cualquier juego unos frames en CI.
        int run();

        // Código que devolverá run() al terminar normalmente (ej: un test headless que falla)
        void setExitCode(int code) { m_exitCode = code; }

        // Termina el loop al final del frame actual
        void requestStop() { m_running = false; }
        bool isRunning() const { return m_running; }

        const ApplicationConfig& getConfig() const { return m_config; }
        std::uint64_t getFrameCount() const { return m_time.frameCount(); }

        // Estado de tiempo (delta, paso fijo, alpha, escala). time().setTimeScale(0) = pausa.
        Time& time() { return m_time; }
        const Time& time() const { return m_time; }

        // Entidades + sistemas. El loop corre las fases: Fixed en cada paso fijo (antes de
        // onFixedUpdate), Update antes de onUpdate y Render antes de onRender.
        World& world() { return m_world; }
        const World& world() const { return m_world; }

        // Tamaño en píxeles de la imagen que se dibuja. Lo actualiza el módulo de GPU cada frame (el
        // swapchain sigue a la ventana); sin GPU es config.width × config.height. Antes de la fase Render
        // el loop ajusta a este tamaño las cámaras con autoAspect (CameraSystem).
        const glm::uvec2& viewport() const { return m_viewport; }
        void setViewport(const glm::uvec2& viewport) { m_viewport = viewport; }

        // Reemplaza el reloj (antes de run()). Por defecto SteadyClock (tiempo real).
        // Tests/headless deterministas: setClock(std::make_unique<ManualClock>(1.0 / 60.0)).
        void setClock(std::unique_ptr<Clock> clock) { m_clock = std::move(clock); }

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
        // Hooks del juego. onInit corre después de inicializar los módulos (ej: con la GPU lista).
        // Devolver false aborta el arranque.
        virtual bool onInit() { return true; }
        // Simulación determinista: 0..N veces por frame, siempre con el mismo fixedDt (1 / tickRate)
        virtual void onFixedUpdate(float /*fixedDt*/) {}
        // Una vez por frame con el delta escalado (0 en pausa). Cámara, input, animaciones...
        virtual void onUpdate(float /*dt*/) {}
        // Una vez por frame. time().alpha() = fracción del siguiente paso fijo (para interpolar)
        virtual void onRender() {}
        // Antes de cerrar los módulos (los recursos de GPU del juego todavía son válidos)
        virtual void onShutdown() {}

    private:
        bool initModules();
        void shutdownModules();
        // Avanza el tiempo con el delta real del frame y corre los pasos fijos que tocan
        void advanceTime(double rawFrameTime);

        ApplicationConfig m_config;
        std::vector<std::unique_ptr<Module>> m_modules;
        std::size_t m_initializedModules = 0; // Los primeros N inicializaron bien
        bool m_running = false;
        int m_exitCode = 0;
        Time m_time;
        World m_world;
        glm::uvec2 m_viewport{0, 0};
        double m_fixedDeltaSeconds = 1.0 / 60.0; // Precisión completa para el acumulador
        std::unique_ptr<Clock> m_clock;
    };

    // Definida por el juego: devuelve su Application. La llama el main() del motor (mge_main).
    Application* createApplication();
}
