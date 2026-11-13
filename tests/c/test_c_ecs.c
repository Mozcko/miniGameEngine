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

static int s_eachSpawnCount = 0;
static mge_component_id s_spawnCompId = MGE_INVALID_COMPONENT_ID;
static mge_app* s_spawnApp = NULL;
static void each_add_during_callback(mge_entity entity, void** components, void* user_data) {
    (void)entity;
    int* sum = (int*)user_data;
    int* val = (int*)components[0];
    *sum += *val;
    s_eachSpawnCount++;

    // Crear 10 entidades y adjuntar el mismo componente para forzar realojamiento del pool
    for (int i = 0; i < 10; ++i) {
        mge_entity ne = mge_entity_create(s_spawnApp);
        int* nv = (int*)mge_component_add(s_spawnApp, ne, s_spawnCompId);
        if (nv) {
            *nv = 1000;
        }
    }
}

typedef struct DestroyCtx {
    mge_app* app;
    mge_entity victim;
    int visited;
} DestroyCtx;

static void each_destroy_callback(mge_entity entity, void** components, void* user_data) {
    (void)components;
    DestroyCtx* ctx = (DestroyCtx*)user_data;
    ctx->visited++;
    if (entity != ctx->victim && mge_entity_is_valid(ctx->app, ctx->victim)) {
        mge_entity_destroy(ctx->app, ctx->victim);
    }
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

    // 12. Validación de registro de componentes (Item 1)
    if (mge_component_register(app, "Velocity", sizeof(Velocity), sizeof(float)) != velId) {
        *result = 21;
        return false;
    }
    if (mge_component_register(app, "Velocity", 64, 4) != MGE_INVALID_COMPONENT_ID ||
        strlen(mge_last_error()) == 0) {
        *result = 22;
        return false;
    }
    if (mge_component_register(app, "BadAlignC", 16, 3) != MGE_INVALID_COMPONENT_ID ||
        strlen(mge_last_error()) == 0) {
        *result = 23;
        return false;
    }
    if (mge_component_register(app, "BadHugeAlignC", 16, 8192) != MGE_INVALID_COMPONENT_ID) {
        *result = 24;
        return false;
    }
    if (mge_component_register(app, "BadSizeC", 1024u * 1024u, 16) != MGE_INVALID_COMPONENT_ID) {
        *result = 25;
        return false;
    }
    if (mge_component_register(app, "DefaultAlignC", 16, 0) == MGE_INVALID_COMPONENT_ID) {
        *result = 26;
        return false;
    }

    // 13. comp_id fuera de rango (Item 2)
    if (mge_component_add(app, e1, 999) != NULL || strlen(mge_last_error()) == 0) {
        *result = 27;
        return false;
    }
    if (mge_component_get(app, e1, 999) != NULL || strlen(mge_last_error()) == 0) {
        *result = 28;
        return false;
    }
    if (mge_component_has(app, e1, 999)) {
        *result = 29;
        return false;
    }

    // 14. add-during-each y destroy-during-each en mge_component_each (Item 3, validado con ASan)
    s_spawnCompId = mge_component_register(app, "Counter", sizeof(int), sizeof(int));
    if (s_spawnCompId == MGE_INVALID_COMPONENT_ID) {
        *result = 30;
        return false;
    }
    mge_entity spawnSeed[5];
    for (int i = 0; i < 5; ++i) {
        spawnSeed[i] = mge_entity_create(app);
        int* cv = (int*)mge_component_add(app, spawnSeed[i], s_spawnCompId);
        if (!cv) {
            *result = 31;
            return false;
        }
        *cv = i + 1;
    }
    s_spawnApp = app;
    s_eachSpawnCount = 0;
    int sumCounter = 0;
    mge_component_each(app, &s_spawnCompId, 1, each_add_during_callback, &sumCounter);
    if (s_eachSpawnCount != 5 || sumCounter != 15) {
        *result = 32;
        return false;
    }

    mge_component_id tagId = mge_component_register(app, "TagC", sizeof(int), sizeof(int));
    mge_entity d1 = mge_entity_create(app);
    mge_entity d2 = mge_entity_create(app);
    mge_component_add(app, d1, tagId);
    mge_component_add(app, d2, tagId);
    DestroyCtx dctx = {app, d2, 0};
    mge_component_each(app, &tagId, 1, each_destroy_callback, &dctx);
    if (dctx.visited != 1 || mge_entity_is_valid(app, d2)) {
        *result = 33;
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
