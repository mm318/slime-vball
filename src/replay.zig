const c = @import("c");
const game = @import("game.zig");
const index = @import("support.zig").index;

fn positions(state: *const c.game_t) c.positions_t {
    return .{ .players = .{ state.players[0].pos, state.players[1].pos }, .ball = state.ball.pos };
}

pub export fn record_frame(state: *c.game_t) void {
    state.history[index(state.history_next)] = positions(state);
    state.history_next = @mod(state.history_next + 1, c.REPLAY_FRAMES);
    if (state.history_count < c.REPLAY_FRAMES) state.history_count += 1;
}

pub export fn press_replay(state: *c.game_t, now: f64) void {
    if (state.paused) return;
    if (state.phase == c.REPLAY) {
        game.finish_point(state);
    } else if (state.phase == c.POINT_BREAK and now < state.point_deadline) {
        state.replay_requested = true;
    }
}

pub export fn display_positions(state: *const c.game_t) c.positions_t {
    if (state.phase != c.REPLAY) return positions(state);
    const oldest = @mod(state.history_next + c.REPLAY_FRAMES - state.history_count, c.REPLAY_FRAMES);
    const frame = if (state.replay_index < 0) 0 else state.replay_index;
    return state.history[index(@mod(oldest + frame, c.REPLAY_FRAMES))];
}

pub export fn advance_replay(state: *c.game_t) void {
    state.replay_index += 1;
    if (state.replay_index == state.history_count) state.replay_index = 0;
}
