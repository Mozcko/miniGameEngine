// NOLINTBEGIN(readability-identifier-naming)
#ifndef MGE_C_H
#define MGE_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(MGE_C_BUILD_SHARED)
#define MGE_C_API __declspec(dllexport)
#elif defined(MGE_C_USE_SHARED)
#define MGE_C_API __declspec(dllimport)
#else
#define MGE_C_API
#endif
#else
#if defined(MGE_C_BUILD_SHARED)
#define MGE_C_API __attribute__((visibility("default")))
#else
#define MGE_C_API
#endif
#endif

/* --- Versioning --- */
#define MGE_C_API_VERSION_MAJOR 1
#define MGE_C_API_VERSION_MINOR 0
#define MGE_C_API_VERSION ((MGE_C_API_VERSION_MAJOR << 16) | MGE_C_API_VERSION_MINOR)

/**
 * Retorna la versión compilada de la API de C: (MAJOR << 16) | MINOR.
 */
MGE_C_API uint32_t mge_c_api_version(void);

/**
 * Retorna la cadena de versión del motor (ej: "1.0.0" o "1.1.0").
 */
MGE_C_API const char* mge_version_string(void);

/* --- Error Codes & Logging --- */
typedef enum mge_result {
    MGE_OK = 0,
    MGE_ERROR = -1,
    MGE_ERROR_INVALID_ARGUMENT = -2,
    MGE_ERROR_INVALID_HANDLE = -3,
    MGE_ERROR_NOT_INITIALIZED = -4,
    MGE_ERROR_GPU = -5,
    MGE_ERROR_NOT_FOUND = -6,
    MGE_ERROR_UNSUPPORTED = -7
} mge_result;

/**
 * Retorna el último mensaje de error registrado en el hilo actual.
 * El puntero es válido hasta la siguiente llamada a la API en el mismo hilo.
 */
MGE_C_API const char* mge_last_error(void);

MGE_C_API void mge_log_info(const char* category, const char* message);
MGE_C_API void mge_log_warn(const char* category, const char* message);
MGE_C_API void mge_log_error(const char* category, const char* message);

/* --- Math Types (POD) --- */
typedef struct mge_vec2 {
    float x;
    float y;
} mge_vec2;

typedef struct mge_vec3 {
    float x;
    float y;
    float z;
} mge_vec3;

typedef struct mge_vec4 {
    float x;
    float y;
    float z;
    float w;
} mge_vec4;

/* --- Opaque Handles --- */
typedef struct mge_app mge_app;
typedef struct mge_mesh mge_mesh;
typedef struct mge_texture mge_texture;
typedef struct mge_audio_sample mge_audio_sample;
typedef struct mge_block_registry mge_block_registry;
typedef struct mge_voxel_world mge_voxel_world;

typedef uint64_t mge_entity;
#define MGE_NULL_ENTITY 0xFFFFFFFFFFFFFFFFULL

typedef uint32_t mge_component_id;
#define MGE_INVALID_COMPONENT_ID 0xFFFFFFFFu

typedef uint16_t mge_block_id;
#define MGE_BLOCK_AIR 0

typedef uint64_t mge_sound_instance;

/* --- Application Lifecycle --- */
typedef enum mge_module_flags {
    MGE_MODULE_NONE = 0,
    MGE_MODULE_PLATFORM = (1 << 0),
    MGE_MODULE_GPU = (1 << 1),
    MGE_MODULE_GRAPHICS = (1 << 2),
    MGE_MODULE_AUDIO = (1 << 3),
    MGE_MODULE_DEFAULT = (MGE_MODULE_PLATFORM | MGE_MODULE_GPU | MGE_MODULE_GRAPHICS | MGE_MODULE_AUDIO)
} mge_module_flags;

typedef struct mge_app_config {
    size_t struct_size; /* sizeof(mge_app_config) para compatibilidad ABI */
    const char* title;
    int32_t width;
    int32_t height;
    bool headless;
    bool vsync;
    int32_t tick_rate;  /* Pasos fijos por segundo (ej: 60) */
    int32_t max_frames; /* 0 = sin límite; >0 sale automáticamente tras n frames */
    uint32_t modules;   /* Máscara de mge_module_flags */
} mge_app_config;

typedef struct mge_app_callbacks {
    size_t struct_size; /* sizeof(mge_app_callbacks) */
    bool (*on_init)(mge_app* app, void* user_data);
    void (*on_fixed_update)(mge_app* app, float dt, void* user_data);
    void (*on_update)(mge_app* app, float dt, void* user_data);
    void (*on_render)(mge_app* app, void* user_data);
    void (*on_shutdown)(mge_app* app, void* user_data);
    void* user_data;
} mge_app_callbacks;

/**
 * Inicializa y ejecuta la aplicación hasta que se solicite salir.
 * Devuelve 0 en salida normal o código de error distinto de cero.
 */
MGE_C_API int mge_app_run(const mge_app_config* config, const mge_app_callbacks* callbacks);

/**
 * Solicita la finalización controlada del bucle principal de la aplicación.
 */
MGE_C_API void mge_app_request_stop(mge_app* app);

/* Consultas de tiempo */
MGE_C_API float mge_time_delta(mge_app* app);
MGE_C_API float mge_time_fixed_delta(mge_app* app);
MGE_C_API float mge_time_alpha(mge_app* app);
MGE_C_API double mge_time_elapsed(mge_app* app);
MGE_C_API uint64_t mge_time_frame_count(mge_app* app);
MGE_C_API void mge_time_set_scale(mge_app* app, float scale);

/* --- Input --- */
MGE_C_API mge_result mge_input_bind_action(mge_app* app, const char* action, const char* key_name);
MGE_C_API mge_result mge_input_bind_axis(mge_app* app, const char* axis, const char* negative_key,
                                         const char* positive_key);
MGE_C_API bool mge_input_is_action_pressed(mge_app* app, const char* action);
MGE_C_API bool mge_input_is_action_down(mge_app* app, const char* action);
MGE_C_API bool mge_input_is_action_released(mge_app* app, const char* action);
MGE_C_API float mge_input_get_axis(mge_app* app, const char* axis);
MGE_C_API void mge_input_mouse_delta(mge_app* app, float* dx, float* dy);
MGE_C_API void mge_input_set_cursor_lock(mge_app* app, bool locked);

/**
 * Inyecta una pulsación o liberación de tecla con fines de prueba (headless test injection).
 */
MGE_C_API void mge_test_inject_key(mge_app* app, const char* key_name, bool down);

/* --- ECS --- */
typedef struct mge_transform {
    float position[3];
    float rotation[4]; /* xyzw cuaternión */
    float scale[3];
} mge_transform;

typedef struct mge_camera {
    float fov_y;
    float near_plane;
    float far_plane;
    bool is_orthographic;
    float ortho_size;
} mge_camera;

typedef struct mge_character_controller {
    int32_t mode; /* 0 = walk, 1 = fly */
    float speed;
    float jump_speed;
    float gravity;
    bool is_grounded;
} mge_character_controller;

typedef struct mge_rigid_body {
    float velocity[3];
    float mass;
    float restitution;
    float gravity_scale;
    bool use_gravity;
    bool is_kinematic;
} mge_rigid_body;

MGE_C_API mge_entity mge_entity_create(mge_app* app);
MGE_C_API void mge_entity_destroy(mge_app* app, mge_entity entity);
MGE_C_API bool mge_entity_is_valid(mge_app* app, mge_entity entity);

/* Componentes integrados */
MGE_C_API mge_result mge_transform_set(mge_app* app, mge_entity entity, const mge_transform* transform);
MGE_C_API mge_result mge_transform_get(mge_app* app, mge_entity entity, mge_transform* out_transform);
MGE_C_API bool mge_transform_has(mge_app* app, mge_entity entity);
MGE_C_API void mge_transform_remove(mge_app* app, mge_entity entity);

MGE_C_API mge_result mge_camera_set(mge_app* app, mge_entity entity, const mge_camera* camera);
MGE_C_API mge_result mge_camera_get(mge_app* app, mge_entity entity, mge_camera* out_camera);
MGE_C_API bool mge_camera_has(mge_app* app, mge_entity entity);
MGE_C_API void mge_camera_remove(mge_app* app, mge_entity entity);

MGE_C_API mge_result mge_character_controller_set(mge_app* app, mge_entity entity,
                                                  const mge_character_controller* controller);
MGE_C_API mge_result mge_character_controller_get(mge_app* app, mge_entity entity,
                                                  mge_character_controller* out_controller);
MGE_C_API bool mge_character_controller_has(mge_app* app, mge_entity entity);
MGE_C_API void mge_character_controller_remove(mge_app* app, mge_entity entity);

MGE_C_API mge_result mge_rigid_body_set(mge_app* app, mge_entity entity, const mge_rigid_body* body);
MGE_C_API mge_result mge_rigid_body_get(mge_app* app, mge_entity entity, mge_rigid_body* out_body);
MGE_C_API bool mge_rigid_body_has(mge_app* app, mge_entity entity);
MGE_C_API void mge_rigid_body_remove(mge_app* app, mge_entity entity);

/* Componentes en tiempo de ejecución (dinámicos) */
MGE_C_API mge_component_id mge_component_register(mge_app* app, const char* name, size_t size, size_t align);
MGE_C_API void* mge_component_add(mge_app* app, mge_entity entity, mge_component_id comp_id);
MGE_C_API void* mge_component_get(mge_app* app, mge_entity entity, mge_component_id comp_id);
MGE_C_API bool mge_component_has(mge_app* app, mge_entity entity, mge_component_id comp_id);
MGE_C_API void mge_component_remove(mge_app* app, mge_entity entity, mge_component_id comp_id);
MGE_C_API void mge_component_each(mge_app* app, const mge_component_id* comp_ids, size_t comp_count,
                                  void (*callback)(mge_entity entity, void** components, void* user_data),
                                  void* user_data);

/* Sistemas */
typedef enum mge_system_phase {
    MGE_PHASE_FIXED = 0,
    MGE_PHASE_UPDATE = 1,
    MGE_PHASE_RENDER = 2
} mge_system_phase;

MGE_C_API mge_result mge_system_add(mge_app* app, mge_system_phase phase, const char* name,
                                    void (*fn)(mge_app* app, float dt, void* user_data), void* user_data);

/* --- Procgen & Meshes --- */
MGE_C_API mge_mesh* mge_mesh_create_cube(mge_app* app, float size);
MGE_C_API mge_mesh* mge_mesh_create_sphere(mge_app* app, float radius, int rings, int sectors);
MGE_C_API mge_mesh* mge_mesh_create_plane(mge_app* app, float width, float depth);
MGE_C_API mge_mesh* mge_mesh_create_cylinder(mge_app* app, float radius, float height, int sectors);
MGE_C_API mge_mesh* mge_mesh_upload(mge_app* app, const float* vertices, size_t vertex_count,
                                    const uint32_t* indices, size_t index_count);
MGE_C_API void mge_mesh_destroy(mge_mesh* mesh);

/* --- Rendering --- */
typedef struct mge_lighting {
    float sun_dir[3];
    float sun_color[3];
    float sun_intensity;
    float sky_ambient[3];
    float ground_ambient[3];
    float fog_color[3];
    float fog_density;
} mge_lighting;

MGE_C_API void mge_renderer_draw_mesh(mge_app* app, mge_mesh* mesh, const float model[16],
                                      const float color[4]);
MGE_C_API void mge_renderer_set_lighting(mge_app* app, const mge_lighting* lighting);
MGE_C_API void mge_renderer_set_camera(mge_app* app, const float view[16], const float proj[16]);
MGE_C_API void mge_sky_set_time_of_day(mge_app* app, float hour);
MGE_C_API void mge_sky_advance_time(mge_app* app, float hours);

/* --- 2D UI & Text --- */
MGE_C_API void mge_ui_rect(mge_app* app, float x, float y, float w, float h, const float color[4]);
MGE_C_API void mge_ui_rect_outline(mge_app* app, float x, float y, float w, float h, const float color[4],
                                   float thickness);
MGE_C_API void mge_ui_line(mge_app* app, float x0, float y0, float x1, float y1, const float color[4],
                           float thickness);
MGE_C_API void mge_ui_text(mge_app* app, float x, float y, const char* text, float scale,
                           const float color[4]);

MGE_C_API mge_texture* mge_texture_create_rgba(mge_app* app, int width, int height, const uint8_t* pixels);
MGE_C_API void mge_texture_destroy(mge_texture* texture);
MGE_C_API void mge_ui_image(mge_app* app, mge_texture* texture, float x, float y, float w, float h,
                            const float color[4]);

/* --- Audio --- */
typedef enum mge_sfx_preset {
    MGE_SFX_JUMP = 0,
    MGE_SFX_COIN = 1,
    MGE_SFX_HURT = 2,
    MGE_SFX_EXPLOSION = 3,
    MGE_SFX_POWER_UP = 4,
    MGE_SFX_CLICK = 5
} mge_sfx_preset;

typedef struct mge_sfx_params {
    int32_t wave_type; /* 0: Square, 1: Saw, 2: Sine, 3: Noise, 4: Triangle */
    float attack_time;
    float sustain_time;
    float sustain_punch;
    float decay_time;
    float start_frequency;
    float min_frequency;
    float slide;
    float delta_slide;
    float square_duty;
    float duty_sweep;
    float vibrato_depth;
    float vibrato_speed;
    float change_amount;
    float change_speed;
    float lp_filter_cutoff;
    float lp_filter_sweep;
    float lp_filter_resonance;
    float hp_filter_cutoff;
    float hp_filter_sweep;
    float phaser_offset;
    float phaser_sweep;
    float repeat_speed;
} mge_sfx_params;

MGE_C_API mge_audio_sample* mge_audio_sfx_create_preset(mge_app* app, mge_sfx_preset preset, uint32_t seed);
MGE_C_API mge_audio_sample* mge_audio_sfx_create_custom(mge_app* app, const mge_sfx_params* params);
MGE_C_API mge_audio_sample* mge_audio_sample_load_wav(mge_app* app, const char* file_path);
MGE_C_API void mge_audio_sample_destroy(mge_audio_sample* sample);
MGE_C_API size_t mge_audio_sample_frame_count(const mge_audio_sample* sample);
MGE_C_API float mge_audio_sample_rms(const mge_audio_sample* sample);

MGE_C_API mge_sound_instance mge_audio_play(mge_app* app, mge_audio_sample* sample, float gain, float pan);
MGE_C_API void mge_audio_song_play(mge_app* app, const char* song_name, float fade_seconds);
MGE_C_API void mge_audio_song_stop(mge_app* app, float fade_seconds);
MGE_C_API void mge_audio_set_master_volume(mge_app* app, float volume);
MGE_C_API void mge_audio_set_sfx_volume(mge_app* app, float volume);
MGE_C_API void mge_audio_set_music_volume(mge_app* app, float volume);
MGE_C_API void mge_audio_set_mute(mge_app* app, bool mute);

/* --- Voxel & Physics --- */
typedef struct mge_block_desc {
    const char* name;
    bool is_solid;
    bool is_opaque;
    float color[4];
} mge_block_desc;

typedef mge_block_id (*mge_voxel_generator_fn)(int32_t x, int32_t y, int32_t z, void* user_data);

typedef struct mge_voxel_raycast_hit {
    int32_t block[3];
    int32_t normal[3];
    float distance;
    mge_block_id block_id;
    float point[3];
} mge_voxel_raycast_hit;

MGE_C_API mge_block_registry* mge_block_registry_create(mge_app* app);
MGE_C_API mge_block_id mge_block_register(mge_block_registry* reg, const mge_block_desc* desc);
MGE_C_API void mge_block_registry_destroy(mge_block_registry* reg);

MGE_C_API mge_voxel_world* mge_voxel_world_create(mge_app* app, mge_block_registry* reg,
                                                  mge_voxel_generator_fn generator, void* user_data);
MGE_C_API void mge_voxel_world_destroy(mge_voxel_world* world);

MGE_C_API mge_block_id mge_voxel_world_get_block(mge_voxel_world* world, int32_t x, int32_t y, int32_t z);
MGE_C_API bool mge_voxel_world_set_block(mge_voxel_world* world, int32_t x, int32_t y, int32_t z,
                                         mge_block_id block);
MGE_C_API bool mge_voxel_world_raycast(mge_voxel_world* world, const float origin[3], const float dir[3],
                                       float max_dist, mge_voxel_raycast_hit* out_hit);

#ifdef __cplusplus
}
#endif

#endif /* MGE_C_H */
// NOLINTEND(readability-identifier-naming)
