#include "test_c.h"

static bool on_init_input(mge_app* app, void* user_data) {
    int* result = (int*)user_data;

    // 1. Bind actions and axes by key names
    if (mge_input_bind_action(app, "Jump", "Space") != MGE_OK) {
        *result = 1;
        return false;
    }
    if (mge_input_bind_axis(app, "Horizontal", "A", "D") != MGE_OK) {
        *result = 2;
        return false;
    }

    // Initially not pressed
    if (mge_input_is_action_down(app, "Jump")) {
        *result = 3;
        return false;
    }
    if (mge_input_get_axis(app, "Horizontal") != 0.0f) {
        *result = 4;
        return false;
    }

    // 2. Key injection via mge_test_inject_key
    mge_test_inject_key(app, "Space", true);
    if (!mge_input_is_action_down(app, "Jump")) {
        *result = 5;
        return false;
    }
    if (!mge_input_is_action_pressed(app, "Jump")) {
        *result = 6;
        return false;
    }

    // 3. Horizontal axis with injected D key
    mge_test_inject_key(app, "D", true);
    if (mge_input_get_axis(app, "Horizontal") != 1.0f) {
        *result = 7;
        return false;
    }

    // 4. Release injected keys
    mge_test_inject_key(app, "Space", false);
    mge_test_inject_key(app, "D", false);
    if (mge_input_is_action_down(app, "Jump")) {
        *result = 8;
        return false;
    }
    if (mge_input_get_axis(app, "Horizontal") != 0.0f) {
        *result = 9;
        return false;
    }

    // 5. Inject A (negative)
    mge_test_inject_key(app, "A", true);
    if (mge_input_get_axis(app, "Horizontal") != -1.0f) {
        *result = 10;
        return false;
    }
    mge_test_inject_key(app, "A", false);

    mge_app_request_stop(app);
    *result = 0;
    return true;
}

int test_c_input(void) {
    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.headless = true;
    config.max_frames = 1;

    int testResult = -1;
    mge_app_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init_input;
    callbacks.user_data = &testResult;

    mge_app_run(&config, &callbacks);
    TEST_ASSERT_EQ(testResult, 0);
    return 0;
}
