const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mge_include = b.option(
        []const u8,
        "mge-include",
        "Path to the directory containing mge/c/mge_c.h (default: ../../src)",
    ) orelse "../../src";

    const mge_lib = b.option(
        []const u8,
        "mge-lib",
        "Path to the directory containing libmge_c (default: ../../build/src/mge/c)",
    ) orelse "../../build/src/mge/c";

    const include_path: std.Build.LazyPath = if (std.fs.path.isAbsolute(mge_include))
        .{ .cwd_relative = mge_include }
    else
        b.path(mge_include);

    const lib_path: std.Build.LazyPath = if (std.fs.path.isAbsolute(mge_lib))
        .{ .cwd_relative = mge_lib }
    else
        b.path(mge_lib);

    const root_module = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    root_module.addIncludePath(include_path);
    root_module.addLibraryPath(lib_path);
    root_module.linkSystemLibrary("mge_c", .{});

    if (target.result.os.tag != .windows) {
        root_module.addRPath(lib_path);
    }

    const exe = b.addExecutable(.{
        .name = "zig_hello",
        .root_module = root_module,
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the Zig miniGameEngine example");
    run_step.dependOn(&run_cmd.step);
}
