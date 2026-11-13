// NOLINTBEGIN(readability-identifier-naming)
#include <mge/c/mge_c_internal.h>

#include <algorithm>
#include <cstring>

extern "C" {

int mge_app_run(const mge_app_config* config, const mge_app_callbacks* callbacks) {
    if (!callbacks) {
        mge::c_impl::setLastError("mge_app_run: callbacks no puede ser nulo");
        return MGE_ERROR_INVALID_ARGUMENT;
    }

    if (callbacks->struct_size < sizeof(mge_app_callbacks)) {
        mge::c_impl::setLastError(
            "Invalid mge_app_callbacks.struct_size: set struct_size = sizeof(mge_app_callbacks)");
        return MGE_ERROR_INVALID_ARGUMENT;
    }

    mge_app_callbacks localCallbacks{};
    std::memcpy(&localCallbacks, callbacks, std::min(callbacks->struct_size, sizeof(mge_app_callbacks)));

    mge::ApplicationConfig appConfig;
    uint32_t modules = MGE_MODULE_DEFAULT;

    if (config) {
        if (config->struct_size < sizeof(mge_app_config)) {
            mge::c_impl::setLastError(
                "Invalid mge_app_config.struct_size: set struct_size = sizeof(mge_app_config)");
            return MGE_ERROR_INVALID_ARGUMENT;
        }
        mge_app_config localConfig{};
        std::memcpy(&localConfig, config, std::min(config->struct_size, sizeof(mge_app_config)));

        if (localConfig.title) appConfig.title = localConfig.title;
        if (localConfig.width > 0) appConfig.width = localConfig.width;
        if (localConfig.height > 0) appConfig.height = localConfig.height;
        appConfig.headless = localConfig.headless;
        appConfig.maxFrames = localConfig.max_frames;
        if (localConfig.tick_rate > 0) appConfig.tickRate = static_cast<double>(localConfig.tick_rate);
        appConfig.vsync = localConfig.vsync;
        if (localConfig.modules != 0) modules = localConfig.modules;
    }

    mge::c_impl::CApplication app(appConfig, localCallbacks);

    if (!appConfig.headless) {
#if defined(MGE_HAS_PLATFORM)
        if (modules & MGE_MODULE_PLATFORM) {
            app.addModule<mge::PlatformModule>();
#if defined(MGE_HAS_GPU)
            if (modules & MGE_MODULE_GPU) {
                app.addModule<mge::GpuModule>();
            }
#endif
#if defined(MGE_HAS_AUDIO)
            if (modules & MGE_MODULE_AUDIO) {
                app.addModule<mge::AudioModule>();
            }
#endif
        }
#endif
    }

    return app.run();
}

void mge_app_request_stop(mge_app* app) {
    if (!app || !app->app) return;
    app->app->requestStop();
}

float mge_time_delta(mge_app* app) {
    if (!app || !app->app) return 0.0f;
    return app->app->time().delta();
}

float mge_time_fixed_delta(mge_app* app) {
    if (!app || !app->app) return 0.0f;
    return app->app->time().fixedDelta();
}

float mge_time_alpha(mge_app* app) {
    if (!app || !app->app) return 0.0f;
    return app->app->time().alpha();
}

double mge_time_elapsed(mge_app* app) {
    if (!app || !app->app) return 0.0;
    return app->app->time().elapsed();
}

uint64_t mge_time_frame_count(mge_app* app) {
    if (!app || !app->app) return 0;
    return app->app->time().frameCount();
}

void mge_time_set_scale(mge_app* app, float scale) {
    if (!app || !app->app) return;
    app->app->time().setTimeScale(scale);
}

} // extern "C"
// NOLINTEND(readability-identifier-naming)
