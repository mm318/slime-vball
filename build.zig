const std = @import("std");
const Build = std.Build;
const builtin = @import("builtin");

const emsdk_version = "7.0.0";
const game_sources = [_][]const u8{
    "src/main.c",
    "src/game.c",
    "src/ai.c",
    "src/replay.c",
    "src/ui.c",
};

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const web = target.result.os.tag == .emscripten;
    const emsdk = b.dependency("emsdk", .{});

    const install_sdk = sdkCommand(b, emsdk);
    install_sdk.addArgs(&.{ "install", emsdk_version });

    const activate_sdk = sdkCommand(b, emsdk);
    activate_sdk.addArgs(&.{ "activate", emsdk_version });
    activate_sdk.step.dependOn(&install_sdk.step);
    b.step("install-emsdk", "Install Emscripten " ++ emsdk_version).dependOn(&activate_sdk.step);

    const sysroot: ?Build.LazyPath = if (web)
        emsdk.path("upstream/emscripten/cache/sysroot/include")
    else
        null;
    const sdl = b.dependency("sdl", .{
        .target = target,
        .optimize = .fast,
        .emscripten_include = sysroot,
    });
    const sdl_lib = sdl.artifact("SDL2");

    const game = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    game.addIncludePath(sdl_lib.getEmittedIncludeTree().path(b, "SDL2"));
    game.addCMacro("SDL_MAIN_HANDLED", "1");
    game.linkLibrary(sdl_lib);

    const c_flags = [_][]const u8{ "-std=c11", "-Wall", "-Wextra", "-Wpedantic" };
    game.addCSourceFiles(.{
        .files = &game_sources,
        .flags = if (web) &(c_flags ++ .{"-fno-sanitize=undefined"}) else &c_flags,
    });

    if (sysroot) |include| {
        game.addSystemIncludePath(include);
        buildWeb(b, emsdk, game, sdl_lib, optimize);
    } else {
        if (target.result.os.tag == .linux) game.linkSystemLibrary("m", .{});

        const exe = b.addExecutable(.{ .name = "slime-vball", .root_module = game });
        b.installArtifact(exe);
        if (target.result.os.tag == .linux) {
            b.installFile("slime_vball.desktop", "share/applications/slime-vball.desktop");
        }

        const run = b.addRunArtifact(exe);
        b.step("run", "Run Slime Volleyball on the desktop").dependOn(&run.step);

        const checks = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        });
        checks.addIncludePath(sdl_lib.getEmittedIncludeTree().path(b, "SDL2"));
        checks.addCMacro("SDL_MAIN_HANDLED", "1");
        checks.addCMacro("SLIME_NO_MAIN", "1");
        checks.linkLibrary(sdl_lib);
        if (target.result.os.tag == .linux) checks.linkSystemLibrary("m", .{});
        checks.addCSourceFiles(.{ .files = &game_sources, .flags = &c_flags });
        checks.addCSourceFile(.{ .file = b.path("tests/ui.c"), .flags = &c_flags });
        const ui_test = b.addExecutable(.{ .name = "slime-ui-test", .root_module = checks });
        const test_run = b.addRunArtifact(ui_test);
        b.step("test", "Check prompts, progression, replay, and SDL colors").dependOn(&test_run.step);
    }
}

fn buildWeb(
    b: *Build,
    emsdk: *Build.Dependency,
    game: *Build.Module,
    sdl: *Build.Step.Compile,
    optimize: std.builtin.Optimize,
) void {
    const lib = b.addLibrary(.{ .name = "slime-vball", .root_module = game });
    const link = b.addRunFile(emTool(b, emsdk, "emcc"));
    link.setName("Emscripten link Slime Volleyball");

    switch (optimize) {
        .debug => link.addArgs(&.{ "-Og", "-gsource-map", "-sSTACK_OVERFLOW_CHECK=1", "--emrun" }),
        .safe, .fast => link.addArgs(&.{ "-O3", "--closure", "1" }),
        .small => link.addArgs(&.{ "-Oz", "--closure", "1" }),
    }

    link.addPrefixedFileArg("--shell-file=", b.path("web_assets/shell.html"));
    link.addArtifactArg(lib);
    link.addArtifactArg(sdl);
    link.addArg("-o");
    const html = link.addOutputFileArg("index.html");

    const install = b.addInstallDirectory(.{
        .source_dir = html.dirname(),
        .install_dir = .prefix,
        .install_subdir = "web",
    });
    b.getInstallStep().dependOn(&install.step);

    const run = b.addRunFile(emTool(b, emsdk, "emrun"));
    run.addFileArg(.{ .relative = .{ .base = .install_prefix, .sub_path = "web/index.html" } });
    run.addPassthruArgs();
    run.step.dependOn(b.getInstallStep());
    b.step("run", "Run Slime Volleyball in a browser").dependOn(&run.step);
}

fn emTool(b: *Build, emsdk: *Build.Dependency, name: []const u8) Build.LazyPath {
    const suffix = if (builtin.os.tag == .windows) ".bat" else "";
    return emsdk.path(b.fmt("upstream/emscripten/{s}{s}", .{ name, suffix }));
}

fn sdkCommand(b: *Build, emsdk: *Build.Dependency) *Build.Step.Run {
    if (builtin.os.tag == .windows) return b.addRunFile(emsdk.path("emsdk.bat"));

    const run = b.addSystemCommand(&.{"bash"});
    run.addFileArg(emsdk.path("emsdk"));
    return run;
}
