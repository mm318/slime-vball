const std = @import("std");
const c = @import("c");
const glyphs = @import("font.zig").glyphs;
const support = @import("support.zig");
const text = support.text;
const index = support.index;

const palettes = [_]c.palette_t{
    .{ .sky = 0x0000FFFF, .ground = 0x808080FF, .ball = 0xFFFF00FF, .opponent = 0xFFFFFFFF },
    .{ .sky = 0x1E5000FF, .ground = 0xA9A9A9FF, .ball = 0x8080FFFF, .opponent = 0xFF0000FF },
    .{ .sky = 0x623939FF, .ground = 0x00A800FF, .ball = 0xFFFFFFFF, .opponent = 0x000000FF },
};
const loss_lines = [_][2][]const u8{
    .{ "You are a loser!", "Better luck next time." },
    .{ "Angry Red Slimons gives you the gong!", "So who has the red face bombing out on level 2, huh?" },
    .{ "The Slime Master says \"You are seriously inept.\"", "Congrats on reaching level 3." },
};

pub export fn palette(state: *const c.game_t) *const c.palette_t {
    return &palettes[if (state.solo) index(state.bot_tier) else 0];
}

const Line = struct { length: usize, next: usize };
fn nextLine(value: []const u8, max_width: c_int) Line {
    const capacity: usize = if (max_width >= c.TEXT_CELL_WIDTH - 2) @intCast(@divTrunc(max_width + 2, c.TEXT_CELL_WIDTH)) else 1;
    var length: usize = 0;
    var last_space: usize = 0;
    while (length < value.len and value[length] != '\n' and length < capacity) : (length += 1) {
        if (value[length] == ' ') last_space = length;
    }
    if (length == capacity and length < value.len and value[length] != '\n' and value[length] != ' ' and last_space > 0) length = last_space;
    var next = length;
    while (next < value.len and value[next] == ' ') : (next += 1) {}
    if (next < value.len and value[next] == '\n') next += 1;
    while (length > 0 and value[length - 1] == ' ') : (length -= 1) {}
    return .{ .length = length, .next = next };
}

fn textWidth(value: []const u8) c_int {
    var width: c_int = 0;
    var lines = std.mem.splitScalar(u8, value, '\n');
    while (lines.next()) |line| {
        const line_width: c_int = if (line.len > 0) @as(c_int, @intCast(line.len)) * c.TEXT_CELL_WIDTH - 2 else 0;
        width = @max(width, line_width);
    }
    return width;
}

pub export fn text_width(value: [*:0]const u8) c_int {
    return textWidth(std.mem.span(value));
}

fn drawSlice(screen: *c.SDL_Surface, value: []const u8, center: c_int, first_baseline: c_int, max_width: c_int, rgba: u32) c_int {
    const color = c.map_color(screen, rgba);
    var remaining = value;
    var baseline = first_baseline;
    var lines: c_int = 0;
    while (remaining.len > 0) {
        const line = nextLine(remaining, max_width);
        const width: c_int = if (line.length > 0) @as(c_int, @intCast(line.length)) * c.TEXT_CELL_WIDTH - 2 else 0;
        var x = center - @divTrunc(width, 2);
        for (remaining[0..line.length]) |raw_code| {
            const code = if (raw_code < 32 or raw_code > 126) '?' else raw_code;
            for (glyphs[code - 32], 0..) |row, y| {
                for (0..5) |col| {
                    if (row & (@as(u8, 16) >> @intCast(col)) != 0) {
                        const pixel: c.SDL_Rect = .{ .x = x + @as(c_int, @intCast(col)) * 2, .y = baseline - c.TEXT_HEIGHT + @as(c_int, @intCast(y)) * 2, .w = 2, .h = 2 };
                        _ = c.SDL_FillRect(screen, &pixel, color);
                    }
                }
            }
            x += c.TEXT_CELL_WIDTH;
        }
        lines += 1;
        baseline += c.TEXT_LINE_HEIGHT;
        remaining = remaining[line.next..];
    }
    return lines;
}

pub export fn draw_text(screen: *c.SDL_Surface, value: [*:0]const u8, center: c_int, baseline: c_int, max_width: c_int, rgba: u32) c_int {
    return drawSlice(screen, std.mem.span(value), center, baseline, max_width, rgba);
}

fn centered(screen: *c.SDL_Surface, value: []const u8, baseline: c_int) void {
    _ = drawSlice(screen, value, c.SCREEN_WIDTH / 2, baseline, c.SCREEN_WIDTH - 30, c.NET_COLOR);
}

pub export fn menu_visible(state: *const c.game_t) bool {
    return state.phase == c.TITLE or state.phase == c.RESULT or state.paused;
}

pub export fn ui_button_rect(button: c_int) c.SDL_Rect {
    const toolbar_size = 32;
    return switch (button) {
        c.UI_CONTINUE, c.UI_SOLO, c.UI_LOCAL => .{ .x = 155, .y = 210 + (button - c.UI_CONTINUE) * 56, .w = 440, .h = 46 },
        c.UI_REPLAY, c.UI_PAUSE => .{ .x = if (button == c.UI_REPLAY) 8 else c.SCREEN_WIDTH - 8 - toolbar_size, .y = c.POINT_CENTER_Y - toolbar_size / 2, .w = toolbar_size, .h = toolbar_size },
        else => .{ .x = 0, .y = 0, .w = 0, .h = 0 },
    };
}

pub export fn ui_button_enabled(state: *const c.game_t, button: c_int) bool {
    return switch (button) {
        c.UI_CONTINUE => state.paused or state.phase == c.RESULT,
        c.UI_SOLO, c.UI_LOCAL => true,
        c.UI_REPLAY => !state.paused and state.history_count > 1 and
            (state.phase == c.REPLAY or (state.phase == c.POINT_BREAK and !state.replay_requested and state.last_step < state.point_deadline)),
        c.UI_PAUSE => state.phase != c.TITLE and state.phase != c.RESULT,
        else => false,
    };
}

fn drawButton(screen: *c.SDL_Surface, state: *const c.game_t, button: c_int, label: []const u8) void {
    const rect = ui_button_rect(button);
    const enabled = ui_button_enabled(state, button);
    _ = c.SDL_FillRect(screen, &rect, c.map_color(screen, if (enabled) c.NET_COLOR else 0x666666FF));
    var lines: c_int = 1;
    for (label) |char| if (char == '\n') {
        lines += 1;
    };
    const height = c.TEXT_HEIGHT + (lines - 1) * c.TEXT_LINE_HEIGHT;
    const baseline = rect.y + @divTrunc(rect.h - height, 2) + c.TEXT_HEIGHT;
    _ = drawSlice(screen, label, rect.x + @divTrunc(rect.w, 2), baseline, rect.w - 12, if (enabled) palette(state).sky else 0xB0B0B0FF);
}

fn menuText(screen: *c.SDL_Surface, value: []const u8) void {
    const width = c.SCREEN_WIDTH - 30;
    var remaining = value;
    var lines: c_int = 0;
    while (remaining.len > 0) {
        const line = nextLine(remaining, width);
        lines += 1;
        remaining = remaining[line.next..];
    }
    if (lines == 0) return;

    // Center the complete block between the HUD and buttons, spreading its
    // lines when space permits. Measure wrapping before choosing the spacing.
    const top = c.HUD_SCORE_BASELINE + c.TEXT_LINE_HEIGHT + 16;
    const bottom = ui_button_rect(c.UI_CONTINUE).y - 18;
    const spacing = if (lines > 1) @min(c.TEXT_LINE_HEIGHT + 14, @divTrunc(bottom - top - c.TEXT_HEIGHT, lines - 1)) else 0;
    const height = c.TEXT_HEIGHT + (lines - 1) * spacing;
    var baseline = top + @divTrunc(bottom - top - height, 2) + c.TEXT_HEIGHT;
    remaining = value;
    while (remaining.len > 0) {
        const line = nextLine(remaining, width);
        _ = drawSlice(screen, remaining[0..line.length], c.SCREEN_WIDTH / 2, baseline, width, c.NET_COLOR);
        baseline += spacing;
        remaining = remaining[line.next..];
    }
}

pub export fn draw_menu(screen: *c.SDL_Surface, state: *const c.game_t) void {
    if (state.paused) {
        menuText(screen, "Paused");
    } else if (state.phase == c.RESULT) {
        draw_result(screen, state);
    } else {
        centered(screen, "Slime Volleyball: One Slime", 106);
        centered(screen, "Written by Quin Pendragon and Daniel Wedge", 153);
        centered(screen, "http://oneslime.net/", 181);
    }
    drawButton(screen, state, c.UI_CONTINUE, "Continue");
    drawButton(screen, state, c.UI_SOLO, "Click here to start a\none player game!");
    drawButton(screen, state, c.UI_LOCAL, "Click here to start a\ntwo player game!");
}

pub export fn draw_result(screen: *c.SDL_Surface, state: *const c.game_t) void {
    var buffer: [256]u8 = undefined;
    if (state.solo and state.server == 0) {
        if (state.bot_tier == c.BOT_TIERS - 1) {
            menuText(screen, "YOU WIN!\nThe Slimes bow down before the new Slime King!");
        } else {
            var bonus_buffer: [64]u8 = undefined;
            const bonus = if (state.flawless_bonus != 0) text(&bonus_buffer, "\nFlawless Victory: {d} points", .{state.flawless_bonus}) else "";
            menuText(screen, text(&buffer, "Level {d} clear!\nYour score: {d}\nLevel bonus: {d} points\nTime bonus: {d} points{s}", .{
                state.bot_tier + 1, state.campaign_score, state.level_bonus, state.time_bonus, bonus,
            }));
        }
    } else {
        if (state.solo) {
            const commentary = loss_lines[index(state.bot_tier)];
            menuText(screen, text(&buffer, "GAME OVER\n{s}\n{s}", .{ commentary[0], commentary[1] }));
        } else menuText(screen, text(&buffer, "GAME OVER\n{s}", .{std.mem.sliceTo(&state.local_result, 0)}));
    }
}

pub export fn draw_toolbar(screen: *c.SDL_Surface, state: *const c.game_t) void {
    drawButton(screen, state, c.UI_REPLAY, "R");
    drawButton(screen, state, c.UI_PAUSE, "P");
}

pub export fn draw_hud(screen: *c.SDL_Surface, state: *const c.game_t) void {
    var time_buffer: [32]u8 = undefined;
    var buffer: [128]u8 = undefined;
    const seconds: u32 = @intFromFloat(state.elapsed);
    const time = if (seconds >= 3600)
        text(&time_buffer, "{d}:{d:0>2}:{d:0>2}", .{ @divTrunc(seconds, 3600), @mod(@divTrunc(seconds, 60), 60), @mod(seconds, 60) })
    else
        text(&time_buffer, "{d}:{d:0>2}", .{ @divTrunc(seconds, 60), @mod(seconds, 60) });
    const running = state.phase != c.TITLE and state.phase != c.RESULT;
    const label = if (state.solo)
        (if (running) text(&buffer, "Score: {d}\nTime: {s}", .{ state.campaign_score, time }) else text(&buffer, "Score: {d}", .{state.campaign_score}))
    else
        (if (running) text(&buffer, "Time: {s}", .{time}) else "");
    const width = textWidth(label);
    const backing: c.SDL_Rect = .{ .x = @divTrunc(c.SCREEN_WIDTH - width - 20, 2), .y = 0, .w = width + 20, .h = c.HUD_SCORE_BASELINE + c.TEXT_LINE_HEIGHT + 4 };
    _ = c.SDL_FillRect(screen, &backing, c.map_color(screen, palette(state).sky));
    const baseline = if (state.solo) c.HUD_SCORE_BASELINE else c.HUD_SCORE_BASELINE + c.TEXT_LINE_HEIGHT;
    _ = drawSlice(screen, label, c.SCREEN_WIDTH / 2, baseline, c.HUD_WIDTH, c.NET_COLOR);
    if (!menu_visible(state)) centered(screen, if (state.phase == c.REPLAY) "Replay" else std.mem.sliceTo(&state.message, 0), 329);
}
