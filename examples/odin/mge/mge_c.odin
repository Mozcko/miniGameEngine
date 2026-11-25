package mge

import "core:c"

when ODIN_OS == .Windows {
	foreign import lib "mge_c.lib"
} else {
	foreign import lib "system:mge_c"
}

/* --- Versioning --- */
C_API_VERSION_MAJOR :: 1
C_API_VERSION_MINOR :: 0
C_API_VERSION :: (C_API_VERSION_MAJOR << 16) | C_API_VERSION_MINOR

/* --- Error Codes --- */
Result :: enum c.int {
	OK                     = 0,
	ERROR                  = -1,
	ERROR_INVALID_ARGUMENT = -2,
	ERROR_INVALID_HANDLE   = -3,
	ERROR_NOT_INITIALIZED  = -4,
	ERROR_GPU              = -5,
	ERROR_NOT_FOUND        = -6,
	ERROR_UNSUPPORTED      = -7,
}

/* --- Math Types (POD) --- */
Vec2 :: struct {
	x, y: f32,
}

Vec3 :: struct {
	x, y, z: f32,
}

Vec4 :: struct {
	x, y, z, w: f32,
}

/* --- Opaque Handles --- */
App :: struct {}
Mesh :: struct {}
Texture :: struct {}
Audio_Sample :: struct {}
Block_Registry :: struct {}
Voxel_World :: struct {}

Entity :: u64
NULL_ENTITY :: Entity(0xFFFFFFFFFFFFFFFF)

Component_Id :: u32
INVALID_COMPONENT_ID :: Component_Id(0xFFFFFFFF)

Block_Id :: u16
BLOCK_AIR :: Block_Id(0)

Sound_Instance :: u64

/* --- Application Lifecycle --- */
Module_Flag :: enum u32 {
	NONE     = 0,
	PLATFORM = 1 << 0,
	GPU      = 1 << 1,
	GRAPHICS = 1 << 2,
	AUDIO    = 1 << 3,
	DEFAULT  = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3),
}

App_Config :: struct {
	struct_size: c.size_t,
	title:       cstring,
	width:       i32,
	height:      i32,
	headless:    bool,
	vsync:       bool,
	tick_rate:   i32,
	max_frames:  i32,
	modules:     u32,
}

App_Callbacks :: struct {
	struct_size:     c.size_t,
	on_init:         proc "c" (app: ^App, user_data: rawptr) -> bool,
	on_fixed_update: proc "c" (app: ^App, dt: f32, user_data: rawptr),
	on_update:       proc "c" (app: ^App, dt: f32, user_data: rawptr),
	on_render:       proc "c" (app: ^App, user_data: rawptr),
	on_shutdown:     proc "c" (app: ^App, user_data: rawptr),
	user_data:       rawptr,
}

/* --- ECS --- */
Transform :: struct {
	position: [3]f32,
	rotation: [4]f32, // xyzw quaternion
	scale:    [3]f32,
}

Camera :: struct {
	fov_y:           f32,
	near_plane:      f32,
	far_plane:       f32,
	is_orthographic: bool,
	ortho_size:      f32,
}

Character_Controller :: struct {
	mode:        i32, // 0 = walk, 1 = fly
	speed:       f32,
	jump_speed:  f32,
	gravity:     f32,
	is_grounded: bool,
}

Rigid_Body :: struct {
	velocity:      [3]f32,
	mass:          f32,
	restitution:   f32,
	gravity_scale: f32,
	use_gravity:   bool,
	is_kinematic:  bool,
}

System_Phase :: enum c.int {
	FIXED  = 0,
	UPDATE = 1,
	RENDER = 2,
}

/* --- Rendering --- */
Lighting :: struct {
	sun_dir:        [3]f32,
	sun_color:      [3]f32,
	sun_intensity:  f32,
	sky_ambient:    [3]f32,
	ground_ambient: [3]f32,
	fog_color:      [3]f32,
	fog_density:    f32,
}

/* --- Audio --- */
Sfx_Preset :: enum c.int {
	JUMP      = 0,
	COIN      = 1,
	HURT      = 2,
	EXPLOSION = 3,
	POWER_UP  = 4,
	CLICK     = 5,
}

Sfx_Params :: struct {
	wave_type:           i32, // 0: Square, 1: Saw, 2: Sine, 3: Noise, 4: Triangle
	attack_time:         f32,
	sustain_time:        f32,
	sustain_punch:       f32,
	decay_time:          f32,
	start_frequency:     f32,
	min_frequency:       f32,
	slide:               f32,
	delta_slide:         f32,
	square_duty:         f32,
	duty_sweep:          f32,
	vibrato_depth:       f32,
	vibrato_speed:       f32,
	change_amount:       f32,
	change_speed:        f32,
	lp_filter_cutoff:    f32,
	lp_filter_sweep:     f32,
	lp_filter_resonance: f32,
	hp_filter_cutoff:    f32,
	hp_filter_sweep:     f32,
	phaser_offset:       f32,
	phaser_sweep:        f32,
	repeat_speed:        f32,
}

/* --- Voxel & Physics --- */
Block_Desc :: struct {
	name:      cstring,
	is_solid:  bool,
	is_opaque: bool,
	color:     [4]f32,
}

Voxel_Generator_Fn :: proc "c" (x, y, z: i32, user_data: rawptr) -> Block_Id

Voxel_Raycast_Hit :: struct {
	block:    [3]i32,
	normal:   [3]i32,
	distance: f32,
	block_id: Block_Id,
	point:    [3]f32,
}

@(default_calling_convention = "c", link_prefix = "mge_")
foreign lib {
	/* Versioning & Logging */
	c_api_version :: proc() -> u32 ---
	version_string :: proc() -> cstring ---
	last_error :: proc() -> cstring ---
	log_info :: proc(category, message: cstring) ---
	log_warn :: proc(category, message: cstring) ---
	log_error :: proc(category, message: cstring) ---

	/* Application Lifecycle */
	app_run :: proc(config: ^App_Config, callbacks: ^App_Callbacks) -> c.int ---
	app_request_stop :: proc(app: ^App) ---

	/* Time */
	time_delta :: proc(app: ^App) -> f32 ---
	time_fixed_delta :: proc(app: ^App) -> f32 ---
	time_alpha :: proc(app: ^App) -> f32 ---
	time_elapsed :: proc(app: ^App) -> f64 ---
	time_frame_count :: proc(app: ^App) -> u64 ---
	time_set_scale :: proc(app: ^App, scale: f32) ---

	/* Input */
	input_bind_action :: proc(app: ^App, action, key_name: cstring) -> Result ---
	input_bind_axis :: proc(app: ^App, axis, negative_key, positive_key: cstring) -> Result ---
	input_is_action_pressed :: proc(app: ^App, action: cstring) -> bool ---
	input_is_action_down :: proc(app: ^App, action: cstring) -> bool ---
	input_is_action_released :: proc(app: ^App, action: cstring) -> bool ---
	input_get_axis :: proc(app: ^App, axis: cstring) -> f32 ---
	input_mouse_delta :: proc(app: ^App, dx, dy: ^f32) ---
	input_set_cursor_lock :: proc(app: ^App, locked: bool) ---
	test_inject_key :: proc(app: ^App, key_name: cstring, down: bool) ---

	/* ECS */
	entity_create :: proc(app: ^App) -> Entity ---
	entity_destroy :: proc(app: ^App, entity: Entity) ---
	entity_is_valid :: proc(app: ^App, entity: Entity) -> bool ---

	transform_set :: proc(app: ^App, entity: Entity, transform: ^Transform) -> Result ---
	transform_get :: proc(app: ^App, entity: Entity, out_transform: ^Transform) -> Result ---
	transform_has :: proc(app: ^App, entity: Entity) -> bool ---
	transform_remove :: proc(app: ^App, entity: Entity) ---

	camera_set :: proc(app: ^App, entity: Entity, camera: ^Camera) -> Result ---
	camera_get :: proc(app: ^App, entity: Entity, out_camera: ^Camera) -> Result ---
	camera_has :: proc(app: ^App, entity: Entity) -> bool ---
	camera_remove :: proc(app: ^App, entity: Entity) ---

	character_controller_set :: proc(app: ^App, entity: Entity, controller: ^Character_Controller) -> Result ---
	character_controller_get :: proc(app: ^App, entity: Entity, out_controller: ^Character_Controller) -> Result ---
	character_controller_has :: proc(app: ^App, entity: Entity) -> bool ---
	character_controller_remove :: proc(app: ^App, entity: Entity) ---

	rigid_body_set :: proc(app: ^App, entity: Entity, body: ^Rigid_Body) -> Result ---
	rigid_body_get :: proc(app: ^App, entity: Entity, out_body: ^Rigid_Body) -> Result ---
	rigid_body_has :: proc(app: ^App, entity: Entity) -> bool ---
	rigid_body_remove :: proc(app: ^App, entity: Entity) ---

	component_register :: proc(app: ^App, name: cstring, size, align: c.size_t) -> Component_Id ---
	component_add :: proc(app: ^App, entity: Entity, comp_id: Component_Id) -> rawptr ---
	component_get :: proc(app: ^App, entity: Entity, comp_id: Component_Id) -> rawptr ---
	component_has :: proc(app: ^App, entity: Entity, comp_id: Component_Id) -> bool ---
	component_remove :: proc(app: ^App, entity: Entity, comp_id: Component_Id) ---
	component_each :: proc(app: ^App, comp_ids: [^]Component_Id, comp_count: c.size_t, callback: proc "c" (entity: Entity, components: [^]rawptr, user_data: rawptr), user_data: rawptr) ---

	system_add :: proc(app: ^App, phase: System_Phase, name: cstring, fn: proc "c" (app: ^App, dt: f32, user_data: rawptr), user_data: rawptr) -> Result ---

	/* Procgen & Meshes */
	mesh_create_cube :: proc(app: ^App, size: f32) -> ^Mesh ---
	mesh_create_sphere :: proc(app: ^App, radius: f32, rings, sectors: c.int) -> ^Mesh ---
	mesh_create_plane :: proc(app: ^App, width, depth: f32) -> ^Mesh ---
	mesh_create_cylinder :: proc(app: ^App, radius, height: f32, sectors: c.int) -> ^Mesh ---
	mesh_upload :: proc(app: ^App, vertices: [^]f32, vertex_count: c.size_t, indices: [^]u32, index_count: c.size_t) -> ^Mesh ---
	mesh_destroy :: proc(mesh: ^Mesh) ---

	/* Rendering */
	renderer_draw_mesh :: proc(app: ^App, mesh: ^Mesh, model: ^[16]f32, color: ^[4]f32) ---
	renderer_set_lighting :: proc(app: ^App, lighting: ^Lighting) ---
	renderer_set_camera :: proc(app: ^App, view: ^[16]f32, proj: ^[16]f32) ---
	sky_set_time_of_day :: proc(app: ^App, hour: f32) ---
	sky_advance_time :: proc(app: ^App, hours: f32) ---

	/* 2D UI & Text */
	ui_rect :: proc(app: ^App, x, y, w, h: f32, color: ^[4]f32) ---
	ui_rect_outline :: proc(app: ^App, x, y, w, h: f32, color: ^[4]f32, thickness: f32) ---
	ui_line :: proc(app: ^App, x0, y0, x1, y1: f32, color: ^[4]f32, thickness: f32) ---
	ui_text :: proc(app: ^App, x, y: f32, text: cstring, scale: f32, color: ^[4]f32) ---
	texture_create_rgba :: proc(app: ^App, width, height: c.int, pixels: [^]u8) -> ^Texture ---
	texture_destroy :: proc(texture: ^Texture) ---
	ui_image :: proc(app: ^App, texture: ^Texture, x, y, w, h: f32, color: ^[4]f32) ---

	/* Audio */
	audio_sfx_create_preset :: proc(app: ^App, preset: Sfx_Preset, seed: u32) -> ^Audio_Sample ---
	audio_sfx_create_custom :: proc(app: ^App, params: ^Sfx_Params) -> ^Audio_Sample ---
	audio_sample_load_wav :: proc(app: ^App, file_path: cstring) -> ^Audio_Sample ---
	audio_sample_destroy :: proc(sample: ^Audio_Sample) ---
	audio_sample_frame_count :: proc(sample: ^Audio_Sample) -> c.size_t ---
	audio_sample_rms :: proc(sample: ^Audio_Sample) -> f32 ---
	audio_play :: proc(app: ^App, sample: ^Audio_Sample, gain, pan: f32) -> Sound_Instance ---
	audio_song_play :: proc(app: ^App, song_name: cstring, fade_seconds: f32) ---
	audio_song_stop :: proc(app: ^App, fade_seconds: f32) ---
	audio_set_master_volume :: proc(app: ^App, volume: f32) ---
	audio_set_sfx_volume :: proc(app: ^App, volume: f32) ---
	audio_set_music_volume :: proc(app: ^App, volume: f32) ---
	audio_set_mute :: proc(app: ^App, mute: bool) ---

	/* Voxel & Physics */
	block_registry_create :: proc(app: ^App) -> ^Block_Registry ---
	block_register :: proc(reg: ^Block_Registry, desc: ^Block_Desc) -> Block_Id ---
	block_registry_destroy :: proc(reg: ^Block_Registry) ---
	voxel_world_create :: proc(app: ^App, reg: ^Block_Registry, generator: Voxel_Generator_Fn, user_data: rawptr) -> ^Voxel_World ---
	voxel_world_destroy :: proc(world: ^Voxel_World) ---
	voxel_world_get_block :: proc(world: ^Voxel_World, x, y, z: i32) -> Block_Id ---
	voxel_world_set_block :: proc(world: ^Voxel_World, x, y, z: i32, block: Block_Id) -> bool ---
	voxel_world_raycast :: proc(world: ^Voxel_World, origin, dir: ^[3]f32, max_dist: f32, out_hit: ^Voxel_Raycast_Hit) -> bool ---
}
