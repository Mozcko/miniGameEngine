#pragma once

namespace mge {

    class Application;

    // Un módulo opcional del motor (plataforma/ventana, audio, render...). La Application los
    // inicializa en el orden en que se agregaron y los cierra en orden inverso.
    // El core no conoce ningún módulo concreto: así puede correr sin ventana ni GPU (ADR 0003).
    class Module {
    public:
        virtual ~Module() = default;

        // Nombre para el log
        virtual const char* name() const = 0;

        // false = error fatal: la aplicación no arranca (los módulos ya inicializados se cierran)
        virtual bool onInit(Application& /*app*/) { return true; }
        // Al inicio de cada frame (ej: leer eventos del sistema)
        virtual void onFrameBegin(Application& /*app*/) {}
        // Al final de cada frame (ej: presentar la imagen)
        virtual void onFrameEnd(Application& /*app*/) {}
        // Solo se llama si onInit devolvió true
        virtual void onShutdown(Application& /*app*/) {}
    };
}
