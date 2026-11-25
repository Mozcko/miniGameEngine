#pragma once

namespace mge {

    class Application;

    // An optional engine module (platform/window, audio, render...). Application initializes them
    // in the order they were added and shuts them down in reverse order.
    // The core knows no concrete module: this allows running without a window or GPU (ADR 0003).
    class Module {
    public:
        virtual ~Module() = default;

        // Name for logging
        virtual const char* name() const = 0;

        // false = fatal error: the application does not start (already-initialized modules are shut down)
        virtual bool onInit(Application& /*app*/) { return true; }
        // At the start of each frame (e.g., poll OS events)
        virtual void onFrameBegin(Application& /*app*/) {}
        // At the end of each frame (e.g., present the image)
        virtual void onFrameEnd(Application& /*app*/) {}
        // Only called if onInit returned true
        virtual void onShutdown(Application& /*app*/) {}
    };
}
