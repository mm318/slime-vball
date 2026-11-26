const c = @import("c");
const replay = @import("replay.zig");
const support = @import("support.zig");
const text = support.text;
const index = support.index;

const names = [_][]const u8{
    "Inferior Human Controlled Slime ", "The Pathetic White Slime ",
    "Angry Red Slimons ",               "The Slime Master ",
};

pub export fn start_match(state: *c.game_t, solo: bool) void {
    state.solo = solo;
    state.bot_tier = 0;
    state.campaign_score = 0;
    c.reset_match(state);
}

pub export fn match_over(state: *const c.game_t) bool {
    return state.score[index(state.server)] >= c.MAX_SCORE;
}

pub export fn finish_point(state: *c.game_t) void {
    if (match_over(state)) {
        state.phase = c.RESULT;
        if (state.solo and state.server == 0) {
            const multiplier = @as(c_int, 1) << @intCast(state.bot_tier);
            state.level_bonus = @divTrunc(1000 * state.score[0] * multiplier, state.score[0] + state.score[1]);
            state.time_bonus = @intFromFloat(@max(0, 300 - state.elapsed) * @as(f64, @floatFromInt(multiplier)));
            state.flawless_bonus = if (state.score[1] == 0) 1000 * multiplier else 0;
            state.campaign_score += state.level_bonus + state.time_bonus + state.flawless_bonus;
        } else if (!state.solo) {
            const winner = state.server + 1;
            const loser = 3 - winner;
            _ = switch (@mod(c.rand(), 5)) {
                0 => text(&state.local_result, "C'mon player {d}, I'll take you on!", .{winner}),
                1 => text(&state.local_result, "Inferior human controlled slime {d} is insipid!", .{loser}),
                2 => text(&state.local_result, "Inferior human controlled slime {d} is rubbish!", .{loser}),
                3 => text(&state.local_result, "Super inferior human controlled slime {d} wins!", .{winner}),
                else => text(&state.local_result, "You're both yellow cowards. Play me instead!", .{}),
            };
        }
    } else c.reset_rally(state);
}

pub export fn continue_result(state: *c.game_t) void {
    if (state.solo and state.server == 0 and state.bot_tier + 1 < c.BOT_TIERS) {
        state.bot_tier += 1;
        c.reset_match(state);
    } else {
        if (state.solo and state.server == 0) {
            state.bot_tier = 0;
            state.campaign_score = 0;
        }
        state.phase = c.TITLE;
        _ = text(&state.message, "Click the mouse to play!", .{});
    }
}

pub export fn horizontal_input(state: *c.game_t, side: c_int) void {
    const start = if (side == 0) c.PLAYER_START_X else c.SCREEN_WIDTH - c.PLAYER_START_X;
    if (state.players[index(side)].pos.x == start and state.ball.pos.x == start and !state.touched[index(1 - side)]) {
        state.serve_moved = true;
    }
}

pub export fn record_contact(state: *c.game_t, side: c_int) void {
    if (state.serve_moved) {
        state.touched[index(side)] = true;
        state.contacts[index(side)] += 1;
        state.contacts[index(1 - side)] = 0;
    }
}

pub export fn point_message(state: *c.game_t) void {
    const winner = index(state.server);
    const loser = 1 - winner;
    const ace = state.touched[winner] and !state.touched[loser];
    const winning_hit = !ace and state.contacts[winner] > 0;
    if (state.solo and winner == 0) {
        const base: c_int = if (ace) 200 else if (winning_hit) 100 else 50;
        state.campaign_score += base << @intCast(state.bot_tier);
    }
    const ending: []const u8 = if (match_over(state)) "wins!" else if (ace) "aces the serve!" else if (winning_hit) "scores a winner!" else if (!state.touched[winner] and state.touched[loser]) "laughs at his opponent's inability to serve!" else if (state.score[0] == state.score[1]) "draws level!" else if (state.score[winner] == state.score[loser] + 1) "takes the lead!" else "scores!";
    var buffer: [96]u8 = undefined;
    const subject = if (state.solo)
        names[if (winner == 0) 0 else index(state.bot_tier + 1)]
    else
        text(&buffer, "{s}{d} ", .{ names[0], winner + 1 });
    const name = subject[0 .. subject.len - 1];
    const wrap = (name.len + 1 + ending.len) * c.TEXT_CELL_WIDTH - 2 > c.SCREEN_WIDTH - 30;
    _ = text(&state.message, "{s}{s}{s}", .{ name, if (wrap) "\n" else " ", ending });
}

pub export fn toggle_pause(state: *c.game_t, now: f64) void {
    if (state.phase == c.TITLE or state.phase == c.RESULT or state.phase == c.REPLAY) return;
    state.paused = !state.paused;
    if (state.paused) state.paused_at = now else if (state.phase == c.POINT_BREAK) {
        state.point_deadline += now - state.paused_at;
    }
}

pub export fn advance_game_phase(state: *c.game_t, now: f64) bool {
    if (state.last_step >= 0 and !state.paused and (state.phase == c.SERVE or state.phase == c.RALLY)) {
        state.elapsed += now - state.last_step;
    }
    state.last_step = now;
    if (state.phase == c.TITLE or state.phase == c.RESULT or state.paused) return false;
    if (state.phase == c.REPLAY) {
        replay.advance_replay(state);
        return false;
    }
    if (state.phase == c.POINT_BREAK) {
        if (now >= state.point_deadline) {
            if (state.replay_requested) {
                state.phase = c.REPLAY;
                state.replay_index = -1;
            } else finish_point(state);
        }
        return false;
    }
    return true;
}
