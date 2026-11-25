const std = @import("std");
const c = @cImport({
    @cInclude("mge/c/mge_c.h");
});

const GameState = struct {
    cube: ?*c.mge_mesh = null,
    coin_sound: ?*c.mge_audio_sample = null,
    rotation_y: f32 = 0.0,
};

fn onInit(app: ?*c.mge_app, user_data: ?*anyopaque) callconv(.c) bool {
    const state: *GameState = @ptrCast(@alignCast(user_data orelse return false));

    c.mge_log_info("ZigExample", c.mge_version_string());

    // Procedural unit cube
    state.cube = c.mge_mesh_create_cube(app, 1.0);
    if (state.cube == null) {
        return false;
    }

    // Procedural coin sound effect
    state.coin_sound = c.mge_audio_sfx_create_preset(app, c.MGE_SFX_COIN, 0);

    // Input actions
    _ = c.mge_input_bind_action(app, "Quit", "Escape");
    _ = c.mge_input_bind_action(app, "Coin", "Space");

    // View matrix looking at the origin from (0, 2, 4)
    const view = [16]f32{
        1.0, 0.0, 0.0, 0.0,
        0.0, 0.89442719, -0.44721360, 0.0,
        0.0, 0.44721360, 0.89442719, 0.0,
        0.0, -0.89442719, -4.47213595, 1.0,
    };

    // Perspective projection with Z in [0, 1]
    const fov_rad: f32 = 1.04719755; // 60 degrees
    const aspect: f32 = 800.0 / 600.0;
    const f: f32 = 1.0 / @tan(fov_rad * 0.5);
    const near_plane: f32 = 0.1;
    const far_plane: f32 = 100.0;

    var proj = [_]f32{0.0} ** 16;
    proj[0] = f / aspect;
    proj[5] = f;
    proj[10] = far_plane / (near_plane - far_plane);
    proj[11] = -1.0;
    proj[14] = (near_plane * far_plane) / (near_plane - far_plane);

    c.mge_renderer_set_camera(app, &view, &proj);
    return true;
}

fn onUpdate(app: ?*c.mge_app, dt: f32, user_data: ?*anyopaque) callconv(.c) void {
    const state: *GameState = @ptrCast(@alignCast(user_data orelse return));

    if (c.mge_input_is_action_pressed(app, "Quit")) {
        c.mge_app_request_stop(app);
    }

    if (c.mge_input_is_action_pressed(app, "Coin")) {
        if (state.coin_sound) |sound| {
            _ = c.mge_audio_play(app, sound, 1.0, 0.0);
        }
    }

    state.rotation_y += dt * 1.5;
    c.mge_sky_advance_time(app, dt * 0.1);
}

fn onRender(app: ?*c.mge_app, user_data: ?*anyopaque) callconv(.c) void {
    const state: *GameState = @ptrCast(@alignCast(user_data orelse return));

    const cos_y: f32 = @cos(state.rotation_y);
    const sin_y: f32 = @sin(state.rotation_y);
    const model = [16]f32{
        cos_y, 0.0, -sin_y, 0.0,
        0.0, 1.0, 0.0, 0.0,
        sin_y, 0.0, cos_y, 0.0,
        0.0, 0.0, 0.0, 1.0,
    };

    const color = [4]f32{ 0.95, 0.65, 0.15, 1.0 };
    c.mge_renderer_draw_mesh(app, state.cube, &model, &color);

    const text_color = [4]f32{ 1.0, 1.0, 1.0, 1.0 };
    c.mge_ui_text(app, 20.0, 20.0, "Hello from Zig", 2.0, &text_color);

    const hint_color = [4]f32{ 0.8, 0.8, 0.8, 1.0 };
    c.mge_ui_text(app, 20.0, 60.0, "Space: Coin sound | Esc: Quit", 1.0, &hint_color);
}

fn onShutdown(app: ?*c.mge_app, user_data: ?*anyopaque) callconv(.c) void {
    _ = app;
    const state: *GameState = @ptrCast(@alignCast(user_data orelse return));

    if (state.coin_sound) |sound| {
        c.mge_audio_sample_destroy(sound);
        state.coin_sound = null;
    }
    if (state.cube) |cube| {
        c.mge_mesh_destroy(cube);
        state.cube = null;
    }
}

pub fn main() u8 {
    var headless = false;
    if (std.process.argsAlloc(std.heap.c_allocator)) |args| {
        defer std.process.argsFree(std.heap.c_allocator, args);
        for (args[1..]) |arg| {
            if (std.mem.eql(u8, arg, "--headless")) {
                headless = true;
            }
        }
    } else |_| {}

    var config = std.mem.zeroes(c.mge_app_config);
    config.struct_size = @sizeOf(c.mge_app_config);
    config.title = "miniGameEngine - Hello Zig";
    config.width = 800;
    config.height = 600;
    config.headless = headless;
    if (headless) {
        config.max_frames = 60;
        config.modules = c.MGE_MODULE_NONE;
    }

    var state = GameState{};

    var callbacks = std.mem.zeroes(c.mge_app_callbacks);
    callbacks.struct_size = @sizeOf(c.mge_app_callbacks);
    callbacks.on_init = onInit;
    callbacks.on_update = onUpdate;
    callbacks.on_render = onRender;
    callbacks.on_shutdown = onShutdown;
    callbacks.user_data = &state;

    const rc = c.mge_app_run(&config, &callbacks);
    return if (rc == 0) 0 else 1;
}
