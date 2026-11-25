package main

import "core:math"
import "core:os"
import "mge"

Game_State :: struct {
	cube:       ^mge.Mesh,
	coin_sound: ^mge.Audio_Sample,
	rotation_y: f32,
}

on_init :: proc "c" (app: ^mge.App, user_data: rawptr) -> bool {
	if user_data == nil {
		return false
	}
	state := (^Game_State)(user_data)

	mge.log_info("OdinExample", mge.version_string())

	// Procedural unit cube
	state.cube = mge.mesh_create_cube(app, 1.0)
	if state.cube == nil {
		return false
	}

	// Procedural coin sound effect
	state.coin_sound = mge.audio_sfx_create_preset(app, .COIN, 0)

	// Input actions
	mge.input_bind_action(app, "Quit", "Escape")
	mge.input_bind_action(app, "Coin", "Space")

	// View matrix looking at the origin from (0, 2, 4)
	view := [16]f32{
		1.0,  0.0,         0.0,         0.0,
		0.0,  0.89442719, -0.44721360,  0.0,
		0.0,  0.44721360,  0.89442719,  0.0,
		0.0, -0.89442719, -4.47213595,  1.0,
	}

	// Perspective projection with Z in [0, 1]
	fov_rad :: f32(1.04719755) // 60 degrees
	aspect :: f32(800.0 / 600.0)
	f := 1.0 / math.tan_f32(fov_rad * 0.5)
	near_plane :: f32(0.1)
	far_plane :: f32(100.0)

	proj: [16]f32
	proj[0] = f / aspect
	proj[5] = f
	proj[10] = far_plane / (near_plane - far_plane)
	proj[11] = -1.0
	proj[14] = (near_plane * far_plane) / (near_plane - far_plane)

	mge.renderer_set_camera(app, &view, &proj)
	return true
}

on_update :: proc "c" (app: ^mge.App, dt: f32, user_data: rawptr) {
	if user_data == nil {
		return
	}
	state := (^Game_State)(user_data)

	if mge.input_is_action_pressed(app, "Quit") {
		mge.app_request_stop(app)
	}

	if mge.input_is_action_pressed(app, "Coin") {
		if state.coin_sound != nil {
			mge.audio_play(app, state.coin_sound, 1.0, 0.0)
		}
	}

	state.rotation_y += dt * 1.5
	mge.sky_advance_time(app, dt * 0.1)
}

on_render :: proc "c" (app: ^mge.App, user_data: rawptr) {
	if user_data == nil {
		return
	}
	state := (^Game_State)(user_data)

	cos_y := math.cos_f32(state.rotation_y)
	sin_y := math.sin_f32(state.rotation_y)
	model := [16]f32{
		cos_y, 0.0, -sin_y, 0.0,
		0.0,   1.0,  0.0,   0.0,
		sin_y, 0.0,  cos_y, 0.0,
		0.0,   0.0,  0.0,   1.0,
	}

	color := [4]f32{0.2, 0.8, 0.5, 1.0}
	mge.renderer_draw_mesh(app, state.cube, &model, &color)

	text_color := [4]f32{1.0, 1.0, 1.0, 1.0}
	mge.ui_text(app, 20.0, 20.0, "Hello from Odin", 2.0, &text_color)

	hint_color := [4]f32{0.8, 0.8, 0.8, 1.0}
	mge.ui_text(app, 20.0, 60.0, "Space: Coin sound | Esc: Quit", 1.0, &hint_color)
}

on_shutdown :: proc "c" (app: ^mge.App, user_data: rawptr) {
	if user_data == nil {
		return
	}
	state := (^Game_State)(user_data)

	if state.coin_sound != nil {
		mge.audio_sample_destroy(state.coin_sound)
		state.coin_sound = nil
	}
	if state.cube != nil {
		mge.mesh_destroy(state.cube)
		state.cube = nil
	}
}

main :: proc() {
	headless := false
	for arg in os.args[1:] {
		if arg == "--headless" {
			headless = true
		}
	}

	config := mge.App_Config{
		struct_size = size_of(mge.App_Config),
		title       = "miniGameEngine - Hello Odin",
		width       = 800,
		height      = 600,
		headless    = headless,
	}
	if headless {
		config.max_frames = 60
		config.modules = u32(mge.Module_Flag.NONE)
	}

	state: Game_State

	callbacks := mge.App_Callbacks{
		struct_size = size_of(mge.App_Callbacks),
		on_init     = on_init,
		on_update   = on_update,
		on_render   = on_render,
		on_shutdown = on_shutdown,
		user_data   = &state,
	}

	rc := mge.app_run(&config, &callbacks)
	if rc != 0 {
		os.exit(int(rc))
	}
}
