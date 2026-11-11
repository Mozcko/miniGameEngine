#include "test_c.h"

typedef struct Position {
    float x;
    float y;
    float z;
} Position;

typedef struct Velocity {
    float vx;
    float vy;
    float vz;
} Velocity;

static int s_eachCount = 0;
static void each_pos_vel_callback(mge_entity entity, void** components, void* user_data) {
    (void)entity;
    (void)user_data;
    Position* pos = (Position*)components[0];
    Velocity* vel = (Velocity*)components[1];
    pos->x += vel->vx;
    s_eachCount++;
}

static int s_systemTickCount = 0;
static void movement_system(mge_app* app, float dt, void* user_data) {
    (void)app;
    (void)dt;
    (void)user_data;
    s_systemTickCount++;
}

static bool on_init_ecs(mge_app* app, void* user_data) {
    int* result = (int*)user_data;

    // 1. Creación y validación de entidades
    mge_entity e1 = mge_entity_create(app);
    mge_entity e2 = mge_entity_create(app);
    if (!mge_entity_is_valid(app, e1) || !mge_entity_is_valid(app, e2)) {
        *result = 1;
        return false;
    }

    // 2. Registro dinámico de componentes
    mge_component_id posId = mge_component_register(app, "Position", sizeof(Position), sizeof(float));
    mge_component_id velId = mge_component_register(app, "Velocity", sizeof(Velocity), sizeof(float));
    if (posId == MGE_INVALID_COMPONENT_ID || velId == MGE_INVALID_COMPONENT_ID) {
        *result = 2;
        return false;
    }

    // 3. Agregar componentes
    Position* p1 = (Position*)mge_component_add(app, e1, posId);
    if (!p1) { *result = 3; return false; }
    p1->x = 10.0f; p1->y = 20.0f; p1->z = 30.0f;

    Velocity* v1 = (Velocity*)mge_component_add(app, e1, velId);
    if (!v1) { *result = 4; return false; }
    v1->vx = 1.0f; v1->vy = 0.0f; v1->vz = 0.0f;

    Position* p2 = (Position*)mge_component_add(app, e2, posId);
    if (!p2) { *result = 5; return false; }
    p2->x = 100.0f; p2->y = 200.0f; p2->z = 300.0f;

    // 4. Consultas has y get
    if (!mge_component_has(app, e1, posId) || !mge_component_has(app, e1, velId)) {
        *result = 6;
        return false;
    }
    if (!mge_component_has(app, e2, posId) || mge_component_has(app, e2, velId)) {
        *result = 7;
        return false;
    }

    Position* gotP1 = (Position*)mge_component_get(app, e1, posId);
    if (!gotP1 || gotP1->x != 10.0f || gotP1->y != 20.0f || gotP1->z != 30.0f) {
        *result = 8;
        return false;
    }

    // 5. Iteración multi-componente con each
    s_eachCount = 0;
    mge_component_id bothTypes[2] = {posId, velId};
    mge_component_each(app, bothTypes, 2, each_pos_vel_callback, NULL);
    if (s_eachCount != 1) { // Solo e1 tiene ambos
        *result = 9;
        return false;
    }
    if (gotP1->x != 11.0f) { // 10 + 1
        *result = 10;
        return false;
    }

    // 6. Eliminación de componentes
    mge_component_remove(app, e1, velId);
    if (mge_component_has(app, e1, velId)) {
        *result = 11;
        return false;
    }

    // 7. Destrucción de entidad e invalidación
    mge_entity_destroy(app, e2);
    if (mge_entity_is_valid(app, e2)) {
        *result = 12;
        return false;
    }

    // 8. Manejo de entidad inválida (retorna MGE_ERROR_INVALID_HANDLE)
    mge_transform invalidTrans = {{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
    if (mge_transform_set(app, e2, &invalidTrans) != MGE_ERROR_INVALID_HANDLE) {
        *result = 13;
        return false;
    }

    // 9. Built-in Transform
    mge_transform t = {{5.0f, 15.0f, 25.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};
    if (mge_transform_set(app, e1, &t) != MGE_OK) {
        *result = 14;
        return false;
    }
    if (!mge_transform_has(app, e1)) {
        *result = 15;
        return false;
    }
    mge_transform gotT;
    if (mge_transform_get(app, e1, &gotT) != MGE_OK || gotT.position[0] != 5.0f) {
        *result = 16;
        return false;
    }

    // 10. Built-in Camera, CharacterController, RigidBody
    mge_camera cam = {0.785f, 0.1f, 100.0f, false, 10.0f};
    if (mge_camera_set(app, e1, &cam) != MGE_OK || !mge_camera_has(app, e1)) {
        *result = 17;
        return false;
    }

    mge_character_controller cc = {0, 5.0f, 8.0f, 28.0f, true};
    if (mge_character_controller_set(app, e1, &cc) != MGE_OK || !mge_character_controller_has(app, e1)) {
        *result = 18;
        return false;
    }

    mge_rigid_body rb = {{0.0f, 1.0f, 0.0f}, 2.0f, 0.5f, 1.0f, true, false};
    if (mge_rigid_body_set(app, e1, &rb) != MGE_OK || !mge_rigid_body_has(app, e1)) {
        *result = 19;
        return false;
    }

    // 11. Registrar un sistema de simulación
    if (mge_system_add(app, MGE_PHASE_UPDATE, "MovementSystem", movement_system, NULL) != MGE_OK) {
        *result = 20;
        return false;
    }

    mge_app_request_stop(app);
    *result = 0;
    return true;
}

int test_c_ecs(void) {
    mge_app_config config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(mge_app_config);
    config.headless = true;
    config.max_frames = 2;

    int testResult = -1;
    mge_app_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.struct_size = sizeof(mge_app_callbacks);
    callbacks.on_init = on_init_ecs;
    callbacks.user_data = &testResult;

    mge_app_run(&config, &callbacks);
    TEST_ASSERT_EQ(testResult, 0);
    return 0;
}
