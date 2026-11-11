#include "test_c.h"

typedef struct LifecycleState {
    bool initCalled;
    int fixedUpdateCount;
    int updateCount;
    int renderCount;
    bool shutdownCalled;
} LifecycleState;

static bool on_init(mge_app* app, void* user_data) {
    LifecycleState* s = (LifecycleState*)user_data;
    s->initCalled = true;
    mge_log_info("TestC", "on_init ejecutado correctamente");
    (void)app;
    return true;
}

static void on_fixed_update(mge_app* app, float fixed_dt, void* user_data) {
    LifecycleState* s = (LifecycleState*)user_data;
    s->fixedUpdateCount++;
    (void)app;
    (void)fixed_dt;
}

static void on_update(mge_app* app, float dt, void* user_data) {
    LifecycleState* s = (LifecycleState*)user_data;
    s->updateCount++;
    (void)dt;

    if (mge_time_frame_count(app) >= 5) {
        mge_app_request_stop(app);
    }
}

static void on_render(mge_app* app, void* user_data) {
    LifecycleState* s = (LifecycleState*)user_data;
    s->renderCount++;
    (void)app;
}

static void on_shutdown(mge_app* app, void* user_data) {
    LifecycleState* s = (LifecycleState*)user_data;
    s->shutdownCalled = true;
    (void)app;
}

int test_c_lifecycle(void) {
    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.title = "Test Lifecycle";
    config.headless = true;
    config.max_frames = 10;
    config.tick_rate = 60;

    LifecycleState state = {false, 0, 0, 0, false};

    mge_app_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init;
    callbacks.on_fixed_update = on_fixed_update;
    callbacks.on_update = on_update;
    callbacks.on_render = on_render;
    callbacks.on_shutdown = on_shutdown;
    callbacks.user_data = &state;

    const int exitCode = mge_app_run(&config, &callbacks);
    TEST_ASSERT_EQ(exitCode, 0);
    TEST_ASSERT(state.initCalled);
    TEST_ASSERT(state.updateCount >= 5);
    TEST_ASSERT(state.renderCount >= 5);
    TEST_ASSERT(state.shutdownCalled);

    // Verificación de versiones
    TEST_ASSERT_EQ(mge_c_api_version(), MGE_C_API_VERSION);
    TEST_ASSERT(mge_version_string() != NULL);

    return 0;
}
