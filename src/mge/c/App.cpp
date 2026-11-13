// NOLINTBEGIN(readability-identifier-naming)
#include <mge/c/mge_c_internal.h>

extern "C" {

int mge_app_run(const mge_app_config* config, const mge_app_callbacks* callbacks) {
    if (!callbacks) {
        mge::c_impl::setLastError("mge_app_run: callbacks no puede ser nulo");
        return 1;
    }

    mge::ApplicationConfig appConfig;
    uint32_t modules = MGE_MODULE_DEFAULT;

    if (config) {
        if (config->title) appConfig.title = config->title;
        if (config->width > 0) appConfig.width = config->width;
        if (config->height > 0) appConfig.height = config->height;
        appConfig.headless = config->headless;
        appConfig.maxFrames = config->max_frames;
        if (config->tick_rate > 0) appConfig.tickRate = static_cast<double>(config->tick_rate);
        appConfig.vsync = config->vsync;
        if (config->modules != 0) modules = config->modules;
    }

    mge::c_impl::CApplication app(appConfig, *callbacks);

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
