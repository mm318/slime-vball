const c = @import("c");

fn landingX(initial: c.body_t) f32 {
    var volley = initial;
    const intercept_y = c.PLAY_HEIGHT - c.SLIME_CONTACT_RADIUS;
    for (0..75) |_| {
        if (volley.pos.y >= intercept_y and volley.vel.y >= 0) break;
        c.advance_ball(&volley);
        c.bounce_off_walls(&volley);
    }
    return volley.pos.x;
}

pub export fn bot_input(state: *const c.game_t) c.controls_t {
    const player = &state.players[1];
    const volley = &state.ball;
    const aim: f32 = 22.0 - 4.0 * @as(f32, @floatFromInt(state.bot_tier));
    var target: f32 = c.SCREEN_WIDTH * @as(f32, 0.72);
    const serving = state.phase == c.SERVE and state.server == 1;
    if (serving) {
        target = volley.pos.x + aim;
    } else {
        const intercept = landingX(volley.*);
        if (intercept >= @as(f32, c.SCREEN_WIDTH) / 2.0) target = intercept + aim;
    }
    const error_x = target - player.pos.x;
    const ball_dx = volley.pos.x - player.pos.x;
    const jump = (if (serving) volley.vel.y > 4.0 else volley.vel.y > 0.5) and
        volley.pos.y > 143.0 and volley.pos.y < 215.0 and
        ball_dx > -44.0 and ball_dx < 7.0;
    return .{ .direction = if (@abs(error_x) <= 3.0) 0 else if (error_x > 0) 1 else -1, .jump = jump };
}
