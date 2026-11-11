#include <mge/c/mge_c.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct GameState {
    mge_mesh* cube;
    mge_audio_sample* coinSound;
    float rotationY;
} GameState;

static bool on_init(mge_app* app, void* user_data) {
    GameState* state = (GameState*)user_data;

    /* Crear cubo procedural */
    state->cube = mge_mesh_create_cube(app, 1.0f);
    if (!state->cube) {
        return false;
    }

    /* Sonido procedural de moneda */
    state->coinSound = mge_audio_sfx_create_preset(app, MGE_SFX_COIN, 0);

    /* Mapear acciones de entrada */
    mge_input_bind_action(app, "Quit", "Escape");
    mge_input_bind_action(app, "Coin", "Space");

    /* Matriz de vista mirando al origen desde (0, 2, 4) */
    float view[16] = {
        1.0f,  0.0f,          0.0f,          0.0f,
        0.0f,  0.89442719f,  -0.44721360f,   0.0f,
        0.0f,  0.44721360f,   0.89442719f,   0.0f,
        0.0f, -0.89442719f,  -4.47213595f,   1.0f
    };
    /* Proyeccion perspectiva con Z en [0, 1] */
    const float fovRad = 1.04719755f;
    const float aspect = 800.0f / 600.0f;
    const float f = 1.0f / tanf(fovRad * 0.5f);
    const float nearPlane = 0.1f;
    const float farPlane = 100.0f;
    float proj[16] = {0};
    proj[0] = f / aspect;
    proj[5] = f;
    proj[10] = farPlane / (nearPlane - farPlane);
    proj[11] = -1.0f;
    proj[14] = (nearPlane * farPlane) / (nearPlane - farPlane);
    mge_renderer_set_camera(app, view, proj);

    return true;
}

static void on_update(mge_app* app, float dt, void* user_data) {
    GameState* state = (GameState*)user_data;

    if (mge_input_is_action_pressed(app, "Quit")) {
        mge_app_request_stop(app);
    }

    if (mge_input_is_action_pressed(app, "Coin")) {
        if (state->coinSound) {
            mge_audio_play(app, state->coinSound, 1.0f, 0.0f);
        }
    }

    /* Rotar el cubo en el eje Y */
    state->rotationY += dt * 1.5f;

    /* Avanzar suavemente el ciclo de cielo */
    mge_sky_advance_time(app, dt * 0.1f);
}

static void on_render(mge_app* app, void* user_data) {
    GameState* state = (GameState*)user_data;

    /* Matriz de rotacion en Y */
    const float c = cosf(state->rotationY);
    const float s = sinf(state->rotationY);
    float model[16] = {
         c,    0.0f,  -s,   0.0f,
        0.0f,  1.0f, 0.0f,  0.0f,
         s,    0.0f,   c,   0.0f,
        0.0f,  0.0f, 0.0f,  1.0f
    };

    float color[4] = {0.2f, 0.7f, 1.0f, 1.0f};
    mge_renderer_draw_mesh(app, state->cube, model, color);

    /* Texto 2D */
    float textColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    mge_ui_text(app, 20.0f, 20.0f, "Hello from C", 2.0f, textColor);

    float hintColor[4] = {0.8f, 0.8f, 0.8f, 1.0f};
    mge_ui_text(app, 20.0f, 60.0f, "Space: Coin sound | Esc: Quit", 1.0f, hintColor);
}

static void on_shutdown(mge_app* app, void* user_data) {
    (void)app;
    GameState* state = (GameState*)user_data;
    if (state->coinSound) {
        mge_audio_sample_destroy(state->coinSound);
        state->coinSound = NULL;
    }
    if (state->cube) {
        mge_mesh_destroy(state->cube);
        state->cube = NULL;
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.title = "miniGameEngine - Hello C";
    config.width = 800;
    config.height = 600;

    GameState state;
    memset(&state, 0, sizeof(state));

    mge_app_callbacks callbacks;
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init;
    callbacks.on_fixed_update = NULL;
    callbacks.on_update = on_update;
    callbacks.on_render = on_render;
    callbacks.on_shutdown = on_shutdown;
    callbacks.user_data = &state;

    return (int)mge_app_run(&config, &callbacks);
}
