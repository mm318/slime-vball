const std = @import("std");
const builtin = @import("builtin");
const c = @import("c");
const text = @import("support.zig").text;
comptime {
    _ = @import("modules.zig");
}

var state: c.game_t = std.mem.zeroes(c.game_t);
var view: c.graphics_t = std.mem.zeroes(c.graphics_t);
var clock_frequency: f64 = 0;
var next_frame: f64 = 0;

fn frameTime() f64 {
    return @as(f64, @floatFromInt(c.SDL_GetPerformanceCounter())) / clock_frequency;
}

fn gameFrame() bool {
    const now = frameTime();
    var event: c.SDL_Event = undefined;
    while (c.SDL_PollEvent(&event) != 0) {
        if (!c.handle_event(&state, view.window, event, now)) return false;
    }
    if (now < next_frame) return true;
    next_frame = now + c.FRAME_INTERVAL;
    c.step_game(&state, now);
    c.render_game(&view, &state);
    const output = c.SDL_GetWindowSurface(view.window);
    if (output == null or c.SDL_BlitScaled(view.screen, null, output, null) != 0 or c.SDL_UpdateWindowSurface(view.window) != 0) {
        c.SDL_Log("Could not present the game: %s", c.SDL_GetError());
        c.free_resources(&view);
        c.exit(c.EXIT_FAILURE);
    }
    return true;
}

fn browserFrame() callconv(.c) void {
    if (!gameFrame()) {
        c.emscripten_cancel_main_loop();
        c.free_resources(&view);
    }
}

pub fn main() u8 {
    c.SDL_SetMainReady();
    _ = c.SDL_SetHint(c.SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
    if (builtin.os.tag == .emscripten) {
        _ = c.SDL_SetHint(c.SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
    }
    if (c.SDL_Init(c.SDL_INIT_VIDEO) != 0) {
        c.SDL_Log("Could not initialize SDL: %s", c.SDL_GetError());
        return c.EXIT_FAILURE;
    }
    view.window = c.SDL_CreateWindow("Slime Volleyball: One Slime", c.SDL_WINDOWPOS_CENTERED, c.SDL_WINDOWPOS_CENTERED, c.WINDOW_WIDTH, c.WINDOW_HEIGHT, 0);
    view.screen = c.SDL_CreateRGBSurfaceWithFormat(0, c.SCREEN_WIDTH, c.SCREEN_HEIGHT, 32, c.SDL_PIXELFORMAT_RGBA32);
    if (view.window == null or view.screen == null or c.SDL_GetWindowSurface(view.window) == null) {
        c.SDL_Log("Could not create the window: %s", c.SDL_GetError());
        c.free_resources(&view);
        return c.EXIT_FAILURE;
    }
    view.slime1 = c.create_circle_sprite(c.PLAYER_DRAW_RADIUS, c.PLAYER_DRAW_RADIUS, c.NET_COLOR);
    view.slime2 = c.create_circle_sprite(c.PLAYER_DRAW_RADIUS, c.PLAYER_DRAW_RADIUS, c.NET_COLOR);
    view.ball = c.create_circle_sprite(c.BALL_DRAW_RADIUS, 2 * c.BALL_DRAW_RADIUS + 1, c.NET_COLOR);
    if (view.slime1 == null or view.slime2 == null or view.ball == null) {
        c.SDL_Log("Could not create the sprites: %s", c.SDL_GetError());
        c.free_resources(&view);
        return c.EXIT_FAILURE;
    }
    view.ground = .{ .x = 0, .y = c.PLAY_HEIGHT, .w = c.SCREEN_WIDTH, .h = c.SCREEN_HEIGHT - c.PLAY_HEIGHT };
    const net_top: c_int = @intFromFloat(@round(@as(f32, 0.7) * c.SCREEN_HEIGHT));
    view.net = .{ .x = c.SCREEN_WIDTH / 2 - c.NET_WIDTH / 2, .y = net_top, .w = c.NET_WIDTH, .h = c.PLAY_HEIGHT - net_top + c.NET_PADDING };
    state.solo = true;
    c.reset_match(&state);
    state.phase = c.TITLE;
    _ = text(&state.message, "Click the mouse to play!", .{});
    clock_frequency = @floatFromInt(c.SDL_GetPerformanceFrequency());
    next_frame = frameTime();
    if (builtin.os.tag == .emscripten) {
        c.emscripten_set_main_loop(&browserFrame, 0, true);
    } else {
        while (gameFrame()) {
            const remaining = (next_frame - frameTime()) * 1000.0;
            if (remaining > 0) c.SDL_Delay(@intFromFloat(@ceil(remaining)));
        }
        c.free_resources(&view);
    }
    return c.EXIT_SUCCESS;
}
