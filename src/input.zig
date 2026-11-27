const std = @import("std");
const c = @import("c");
const game = @import("game.zig");
const replay = @import("replay.zig");
const ui = @import("ui.zig");

const Point = struct { x: f32, y: f32 };
const jump_threshold: f32 = 18;
const tap_slop: f32 = 12;

pub export fn clear_input(state: *c.game_t) void {
    state.input = std.mem.zeroes(c.input_t);
    state.players[0].vel.x = 0;
    if (!state.solo) state.players[1].vel.x = 0;
}

fn playing(state: *const c.game_t) bool {
    return !state.paused and (state.phase == c.SERVE or state.phase == c.RALLY);
}

fn inside(point: Point, rect: c.SDL_Rect) bool {
    return point.x >= @as(f32, @floatFromInt(rect.x)) and point.y >= @as(f32, @floatFromInt(rect.y)) and
        point.x < @as(f32, @floatFromInt(rect.x + rect.w)) and point.y < @as(f32, @floatFromInt(rect.y + rect.h));
}

fn hitButton(state: *const c.game_t, point: Point) c_int {
    if (inside(point, ui.ui_button_rect(c.UI_REPLAY))) return c.UI_REPLAY;
    if (inside(point, ui.ui_button_rect(c.UI_PAUSE))) return c.UI_PAUSE;
    if (ui.menu_visible(state)) {
        for ([_]c_int{ c.UI_CONTINUE, c.UI_SOLO, c.UI_LOCAL }) |button| {
            if (inside(point, ui.ui_button_rect(button))) return button;
        }
    }
    return c.UI_NONE;
}

fn activate(state: *c.game_t, button: c_int, now: f64) void {
    if (!ui.ui_button_enabled(state, button)) return;
    switch (button) {
        c.UI_CONTINUE => if (state.paused) game.toggle_pause(state, now) else game.continue_result(state),
        c.UI_SOLO => game.start_match(state, true),
        c.UI_LOCAL => game.start_match(state, false),
        c.UI_REPLAY => replay.press_replay(state, now),
        c.UI_PAUSE => game.toggle_pause(state, now),
        else => {},
    }
}

fn pointerDown(state: *c.game_t, point: Point, touch: bool, device: i64, finger: i64) bool {
    const button = hitButton(state, point);
    if (button == c.UI_NONE) return false;
    if (state.input.pressed_button == c.UI_NONE and ui.ui_button_enabled(state, button)) {
        state.input.pressed_button = button;
        state.input.press_touch = touch;
        state.input.press_cancelled = false;
        state.input.press_device = device;
        state.input.press_finger = finger;
        state.input.press_x = point.x;
        state.input.press_y = point.y;
    }
    return true; // Disabled controls also consume the press.
}

fn matchesPress(state: *const c.game_t, touch: bool, device: i64, finger: i64) bool {
    return state.input.pressed_button != c.UI_NONE and state.input.press_touch == touch and
        state.input.press_device == device and state.input.press_finger == finger;
}

fn pointerMove(state: *c.game_t, point: Point, device: i64, finger: i64) void {
    if (matchesPress(state, true, device, finger) and
        (@abs(point.x - state.input.press_x) > tap_slop or @abs(point.y - state.input.press_y) > tap_slop))
    {
        state.input.press_cancelled = true;
    }
}

fn pointerUp(state: *c.game_t, point: Point, touch: bool, device: i64, finger: i64, now: f64) void {
    if (!matchesPress(state, touch, device, finger)) return;
    const button = state.input.pressed_button;
    const cancelled = state.input.press_cancelled;
    state.input.pressed_button = c.UI_NONE;
    if (!cancelled and inside(point, ui.ui_button_rect(button))) activate(state, button, now);
}

fn syncMovement(state: *c.game_t, side: usize) void {
    const drag = state.input.drags[side];
    const direction = if (drag.active and drag.direction != 0) drag.direction else state.input.keyboard_direction[side];
    const left = if (side == 0) c.SDLK_LEFT else c.SDLK_a;
    const right = if (side == 0) c.SDLK_RIGHT else c.SDLK_d;
    if (direction != 0) {
        _ = c.player_key(state, if (direction < 0) left else right, true);
    } else {
        _ = c.player_key(state, left, false);
        _ = c.player_key(state, right, false);
    }
}

fn keyboard(state: *c.game_t, event: c.SDL_KeyboardEvent, now: f64) void {
    const action: c_int = switch (event.keysym.sym) {
        c.SDLK_SPACE => c.UI_REPLAY,
        c.SDLK_p => c.UI_PAUSE,
        else => c.UI_NONE,
    };
    if (action != c.UI_NONE) {
        if (event.type == c.SDL_KEYDOWN and event.repeat == 0) activate(state, action, now);
        return;
    }
    if (!playing(state)) return;
    var side: usize = 0;
    const direction: c_int = switch (event.keysym.sym) {
        c.SDLK_LEFT => -1,
        c.SDLK_RIGHT => 1,
        c.SDLK_UP => 0,
        c.SDLK_a => direction: {
            side = 1;
            break :direction -1;
        },
        c.SDLK_d => direction: {
            side = 1;
            break :direction 1;
        },
        c.SDLK_w => direction: {
            side = 1;
            break :direction 0;
        },
        else => return, // Mode selection and Continue require a click or tap.
    };
    if (side == 1 and state.solo) return;
    const down = event.type == c.SDL_KEYDOWN;
    if (direction == 0) {
        if (down) _ = c.player_key(state, event.keysym.sym, true);
        return;
    }
    if (direction < 0) state.input.left[side] = down else state.input.right[side] = down;
    if (down) {
        state.input.keyboard_direction[side] = direction;
    } else if (state.input.keyboard_direction[side] == direction) {
        state.input.keyboard_direction[side] = if (direction < 0 and state.input.right[side]) 1 else if (direction > 0 and state.input.left[side]) -1 else 0;
    }
    syncMovement(state, side);
}

fn fingerEvent(state: *c.game_t, event: c.SDL_TouchFingerEvent, now: f64) void {
    if (event.touchId == c.SDL_MOUSE_TOUCHID) return;
    const point: Point = .{ .x = event.x * c.SCREEN_WIDTH, .y = event.y * c.SCREEN_HEIGHT };
    if (event.type == c.SDL_FINGERDOWN) {
        if (pointerDown(state, point, true, event.touchId, event.fingerId) or !playing(state)) return;
        const side: usize = if (state.solo or point.x < c.SCREEN_WIDTH / 2) 0 else 1;
        const drag = &state.input.drags[side];
        if (!drag.active) drag.* = .{ .active = true, .device = event.touchId, .finger = event.fingerId, .last_x = point.x, .anchor_y = point.y, .direction = 0 };
        return;
    }
    if (event.type == c.SDL_FINGERMOTION) pointerMove(state, point, event.touchId, event.fingerId);
    if (event.type == c.SDL_FINGERUP) {
        pointerMove(state, point, event.touchId, event.fingerId);
        pointerUp(state, point, true, event.touchId, event.fingerId, now);
    }
    for (&state.input.drags, 0..) |*drag, side| {
        if (!drag.active or drag.device != event.touchId or drag.finger != event.fingerId) continue;
        if (event.type == c.SDL_FINGERUP) {
            drag.active = false;
            drag.direction = 0;
            syncMovement(state, side);
        } else if (event.type == c.SDL_FINGERMOTION and playing(state)) {
            // Follow the latest horizontal motion, even a small reversal that
            // stays on the same side of the gesture's earlier positions.
            const dx = point.x - drag.last_x;
            drag.last_x = point.x;
            if (dx != 0) {
                drag.direction = if (dx < 0) -1 else 1;
                syncMovement(state, side);
            }
            if (drag.anchor_y - point.y >= jump_threshold) {
                _ = c.player_key(state, if (side == 0) c.SDLK_UP else c.SDLK_w, true);
                drag.anchor_y = point.y;
            } else if (point.y > drag.anchor_y) drag.anchor_y = point.y;
        }
    }
}

pub export fn handle_event(state: *c.game_t, window: ?*c.SDL_Window, event: *const c.SDL_Event, now: f64) bool {
    switch (event.type) {
        c.SDL_QUIT => return false,
        c.SDL_WINDOWEVENT => if (event.window.event == c.SDL_WINDOWEVENT_FOCUS_LOST) {
            clear_input(state);
        },
        c.SDL_KEYDOWN, c.SDL_KEYUP => keyboard(state, event.key, now),
        c.SDL_FINGERDOWN, c.SDL_FINGERMOTION, c.SDL_FINGERUP => fingerEvent(state, event.tfinger, now),
        c.SDL_MOUSEBUTTONDOWN, c.SDL_MOUSEBUTTONUP => {
            if (event.button.which == c.SDL_TOUCH_MOUSEID or event.button.button != c.SDL_BUTTON_LEFT) return true;
            var width: c_int = c.SCREEN_WIDTH;
            var height: c_int = c.SCREEN_HEIGHT;
            if (window != null) c.SDL_GetWindowSize(window, &width, &height);
            const point: Point = .{ .x = @as(f32, @floatFromInt(event.button.x)) * c.SCREEN_WIDTH / @as(f32, @floatFromInt(@max(width, 1))), .y = @as(f32, @floatFromInt(event.button.y)) * c.SCREEN_HEIGHT / @as(f32, @floatFromInt(@max(height, 1))) };
            if (event.type == c.SDL_MOUSEBUTTONDOWN) {
                _ = pointerDown(state, point, false, 0, 0);
            } else pointerUp(state, point, false, 0, 0, now);
        },
        else => {},
    }
    return true;
}
