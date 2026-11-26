#include "test_c.h"

static mge_mesh* s_leakedMesh = NULL;
static mge_texture* s_leakedTexture = NULL;

static bool on_init_safety(mge_app* app, void* user_data) {
    int* result = (int*)user_data;

    // Create procedural mesh (registered with the tracker)
    s_leakedMesh = mge_mesh_create_cube(app, 1.0f);
    TEST_ASSERT(s_leakedMesh != NULL);

    // Create 2x2 RGBA texture
    uint8_t pixels[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    s_leakedTexture = mge_texture_create_rgba(app, 2, 2, pixels);

    mge_app_request_stop(app);
    *result = 0;
    return true;
}

int test_c_safety(void) {
    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.headless = true;
    config.max_frames = 1;

    int testResult = -1;
    mge_app_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init_safety;
    callbacks.user_data = &testResult;

    // Run and close the app
    mge_app_run(&config, &callbacks);
    TEST_ASSERT_EQ(testResult, 0);

    // STEP 0: Release GPU resources AFTER the application has already closed
    // Must be safe, with no crashes or invalid memory accesses
    mge_mesh_destroy(s_leakedMesh);
    mge_texture_destroy(s_leakedTexture);

    // Destroy NULL safely
    mge_mesh_destroy(NULL);
    mge_texture_destroy(NULL);

    return 0;
}
