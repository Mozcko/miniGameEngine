#include <mge/c/mge_c.h>

#include <stdbool.h>
#include <string.h>

static bool on_init(mge_app* app, void* user_data) {
    (void)app;
    (void)user_data;
    return true;
}

static void on_update(mge_app* app, float dt, void* user_data) {
    (void)dt;
    (void)user_data;
    if (mge_time_frame_count(app) >= 30) {
        mge_app_request_stop(app);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.title = "C Consumer Sample";
    config.width = 640;
    config.height = 480;

    mge_app_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init;
    callbacks.on_update = on_update;

    return (int)mge_app_run(&config, &callbacks);
}
