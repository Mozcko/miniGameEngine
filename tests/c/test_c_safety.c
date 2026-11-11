#include "test_c.h"

static mge_mesh* s_leakedMesh = NULL;
static mge_texture* s_leakedTexture = NULL;

static bool on_init_safety(mge_app* app, void* user_data) {
    int* result = (int*)user_data;

    // Crear mesh procedural (se registra con el tracker)
    s_leakedMesh = mge_mesh_create_cube(app, 1.0f);
    TEST_ASSERT(s_leakedMesh != NULL);

    // Crear textura 2x2 RGBA
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

    // Ejecutar y cerrar la app
    mge_app_run(&config, &callbacks);
    TEST_ASSERT_EQ(testResult, 0);

    // PASO 0: Liberar recursos de GPU DESPUÉS de que la aplicación ya cerró
    // Debe ser seguro, sin cuelgues ni accesos a memoria inválida
    mge_mesh_destroy(s_leakedMesh);
    mge_texture_destroy(s_leakedTexture);

    // Destruir NULL de forma segura
    mge_mesh_destroy(NULL);
    mge_texture_destroy(NULL);

    return 0;
}
