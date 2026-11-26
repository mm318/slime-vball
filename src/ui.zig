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

pub export fn menu_button(solo: bool) c.SDL_Rect {
    return .{ .x = if (solo) 10 else c.SCREEN_WIDTH / 2 + 10, .y = 210, .w = c.SCREEN_WIDTH / 2 - 20, .h = 74 };
}

pub export fn draw_menu(screen: *c.SDL_Surface, state: *const c.game_t) void {
    centered(screen, "Slime Volleyball: One Slime", 106);
    centered(screen, "Written by Quin Pendragon and Daniel Wedge", 153);
    centered(screen, "http://oneslime.net/", 181);
    for (0..2) |side| {
        const rect = menu_button(side == 0);
        _ = c.SDL_FillRect(screen, &rect, c.map_color(screen, c.NET_COLOR));
        const continuing = state.bot_tier > 0 and side == 0;
        var buffer: [128]u8 = undefined;
        const label = text(&buffer, "Click here to start a\n{s} player game!{s}", .{ if (side == 0) "one" else "two", if (continuing) "\nor press C to continue..." else "" });
        const height = c.TEXT_HEIGHT + @as(c_int, if (continuing) 2 else 1) * c.TEXT_LINE_HEIGHT;
        const baseline = rect.y + @divTrunc(rect.h - height, 2) + c.TEXT_HEIGHT;
        _ = drawSlice(screen, label, rect.x + @divTrunc(rect.w, 2), baseline, rect.w - 16, palette(state).sky);
    }
}

pub export fn draw_result(screen: *c.SDL_Surface, state: *const c.game_t) void {
    var buffer: [128]u8 = undefined;
    if (state.solo and state.server == 0) {
        if (state.bot_tier == c.BOT_TIERS - 1) {
            centered(screen, "YOU WIN!", 181);
            centered(screen, "The Slimes bow down before the new Slime King!", 204);
        } else {
            centered(screen, text(&buffer, "Level {d} clear!", .{state.bot_tier + 1}), 125);
            centered(screen, text(&buffer, "Your score: {d}", .{state.campaign_score}), 169);
            centered(screen, text(&buffer, "Level bonus: {d} points", .{state.level_bonus}), 207);
            centered(screen, text(&buffer, "Time bonus: {d} points", .{state.time_bonus}), 226);
            if (state.flawless_bonus != 0) centered(screen, text(&buffer, "Flawless Victory: {d} points", .{state.flawless_bonus}), 245);
        }
    } else {
        const panel: c.SDL_Rect = .{ .x = 15, .y = 87, .w = c.SCREEN_WIDTH - 30, .h = 140 };
        _ = c.SDL_FillRect(screen, &panel, c.map_color(screen, palette(state).ground));
        if (state.solo) {
            centered(screen, loss_lines[index(state.bot_tier)][0], 131);
            centered(screen, loss_lines[index(state.bot_tier)][1], 150);
        } else centered(screen, std.mem.sliceTo(&state.local_result, 0), 131);
        centered(screen, "GAME OVER", 207);
    }
    centered(screen, "Click the mouse to continue...", 329);
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
    const message = if (state.paused) "Press P to continue . . ." else if (state.phase == c.REPLAY) "Press space to continue..." else std.mem.sliceTo(&state.message, 0);
    if (state.phase != c.RESULT) centered(screen, message, 329);
}
