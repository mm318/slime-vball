const c = @import("c");

// Stage order in OneSlime.min.js: _t (white), ot (red), pt (master).
// The controller uses JavaScript's double-precision court coordinates.
pub const Snapshot = struct {
    ball_x: f64,
    ball_y: f64,
    ball_vx: f64,
    ball_vy: f64,
    p1_x: f64,
    p1_y: f64,
    p1_vx: f64,
    p1_vy: f64,
    p2_x: f64,
    p2_y: f64,
    p2_vx: f64,
    p2_vy: f64,

    pub fn fromArray(v: [12]f64) Snapshot {
        return .{ .ball_x = v[0], .ball_y = v[1], .ball_vx = v[2], .ball_vy = v[3], .p1_x = v[4], .p1_y = v[5], .p1_vx = v[6], .p1_vy = v[7], .p2_x = v[8], .p2_y = v[9], .p2_vx = v[10], .p2_vy = v[11] };
    }

    fn fromGame(state: *const c.game_t) Snapshot {
        return fromArray(.{
            @as(f64, state.ball.pos.x) * 1000 / c.SCREEN_WIDTH,
            (c.PLAY_HEIGHT - @as(f64, state.ball.pos.y)) * 1000 / c.SCREEN_HEIGHT,
            @as(f64, state.ball.vel.x) * 1000 / c.SCREEN_WIDTH,
            -@as(f64, state.ball.vel.y) * 1000 / c.SCREEN_HEIGHT,
            @as(f64, state.players[0].pos.x) * 1000 / c.SCREEN_WIDTH,
            (c.PLAY_HEIGHT - @as(f64, state.players[0].pos.y)) * 1000 / c.SCREEN_HEIGHT,
            @as(f64, state.players[0].vel.x) * 1000 / c.SCREEN_WIDTH,
            -@as(f64, state.players[0].vel.y) * 1000 / c.SCREEN_HEIGHT,
            @as(f64, state.players[1].pos.x) * 1000 / c.SCREEN_WIDTH,
            (c.PLAY_HEIGHT - @as(f64, state.players[1].pos.y)) * 1000 / c.SCREEN_HEIGHT,
            @as(f64, state.players[1].vel.x) * 1000 / c.SCREEN_WIDTH,
            -@as(f64, state.players[1].vel.y) * 1000 / c.SCREEN_HEIGHT,
        });
    }
};

pub const Action = enum(u8) { left, right, jump, stop };

fn square(value: f64) f64 {
    return value * value;
}

fn trunc(value: f64) f64 {
    return @trunc(value);
}

fn simpleIntercept(v: Snapshot) f64 {
    var frames: usize = 0;
    var height = v.ball_y;
    var vy = v.ball_vy;
    while (true) {
        vy -= 1;
        height += vy;
        if (!(height > 125)) break;
        frames += 1;
    }
    var x = v.ball_x;
    var vx = v.ball_vx;
    for (0..frames) |_| {
        x += vx;
        if (x < 0) {
            x = 0;
            vx = -vx;
        } else if (x > 1000) {
            x = 1000;
            vx = -vx;
        }
    }
    return x;
}

fn masterIntercept(v: Snapshot) f64 {
    const target_y = v.p2_y + v.p2_vy + 30;
    var x = v.ball_x;
    var height = v.ball_y;
    var vy = v.ball_vy;
    while (true) {
        vy -= 1;
        height += vy;
        if (!(height > target_y)) break;
        // pt.xAtY writes a reversed velocity to an unused local. Preserve
        // its continued use of the outer ball VX (y in the minified bundle).
        x += v.ball_vx;
        if (x <= 0) x = 0 else if (x >= 1000) x = 1000;
    }
    return x;
}

fn nearEdge(v: Snapshot) bool {
    return ((v.p2_x >= 900 and v.ball_x > 830) or (v.p2_x <= 580 and v.ball_x < 530)) and @abs(v.ball_x - v.p2_x) < 100;
}

fn lowSpeed(v: Snapshot) bool {
    return square(v.ball_vx) + square(v.ball_vy) < 20 and v.ball_x - v.p2_x < 30 and v.ball_x != v.p2_x;
}

pub const Controller = struct {
    serve_type: c_int = -1,

    // random.next() and sink.move() are injected so actions and random-call
    // ordering can be checked directly against the original JavaScript.
    pub fn step(self: *Controller, stage: c_int, v: Snapshot, random: anytype, sink: anytype) void {
        @setFloatMode(.strict);
        if (stage == 2) self.master(v, random, sink) else self.simple(stage, v, random, sink);
    }

    fn simple(self: *Controller, stage: c_int, v: Snapshot, random: anytype, sink: anytype) void {
        if (v.ball_x < 500 and self.serve_type != -1) self.serve_type = -1;
        const target = simpleIntercept(v);
        const margin: f64 = if (v.p2_y != 0 and v.p2_x < 575) 0 else if (stage == 0) 25 + trunc(10 * random.next()) else 23 + trunc(15 * random.next());
        if ((v.ball_vx == 0 and v.ball_x == 800) or self.serve_type != -1) {
            if (self.serve_type == -1) {
                if (stage == 0) {
                    self.serve_type = if (v.p1_x > 250) 0 else 1;
                    if (random.next() < 0.35) {
                        // _t assigns this.serveType, not this.i_serveType.
                        // The draw is consumed but does not change the serve.
                        _ = trunc(2 * random.next());
                    }
                } else {
                    if (v.p1_x > 250) self.serve_type = 0 else if (v.p1_x < 200) self.serve_type = 1 else if (v.p1_x < 250) self.serve_type = 2;
                    if (random.next() < 0.35) self.serve_type = @intFromFloat(trunc(3 * random.next()));
                }
            }
            switch (self.serve_type) {
                0 => if (v.ball_y < 300 and v.ball_vy < -3) {
                    sink.move(.right);
                    sink.move(.jump);
                },
                1 => if (v.ball_y < 300 and v.ball_vy < 0) {
                    sink.move(.left);
                    sink.move(.jump);
                },
                2 => if (stage == 1) {
                    if (v.ball_vy > 12 and v.p2_x < 860) sink.move(.right);
                    if (v.p2_x >= 860) sink.move(.stop);
                    if (v.ball_vy == -3 and v.p2_x != 800) sink.move(.jump);
                    if (v.ball_vy < -12 and v.p2_y != 0 and v.p2_x >= 845) sink.move(.left);
                },
                else => {},
            }
        } else if (target < 500) {
            const home: f64 = if (stage == 0) 666 else 800;
            if (@abs(v.p2_x - home) < 20) sink.move(.stop) else if (v.p2_x > home) sink.move(.left) else if (v.p2_x < home) sink.move(.right);
        } else {
            if (@abs(v.p2_x - target) < margin) {
                // Both classes test an undefined i_p2Fire property; it is false.
                if (v.p2_y != 0) return;
                if (nearEdge(v) or
                    (2 * square(v.ball_x - v.p2_x) + square(v.ball_y - v.p2_y) < square(170) and v.ball_x != v.p2_x) or
                    lowSpeed(v) or
                    (@abs(v.ball_x - v.p2_x) < 150 and v.ball_y > 50 and v.ball_y < 400 and random.next() < (if (stage == 0) @as(f64, 0.666) else 0.5)))
                {
                    if (random.next() < (if (stage == 0) @as(f64, 0.85) else 0.4)) sink.move(.jump);
                }
            }
            if (v.p2_y == 0 and self.serve_type == -1) {
                if (@abs(v.p2_x - target) < margin) sink.move(.stop) else if (target + margin < v.p2_x) sink.move(.left) else if (target + margin > v.p2_x) sink.move(.right);
            } else if (self.serve_type == -1) {
                if (v.p2_x < 575) return;
                if (v.p2_x > 900) {
                    sink.move(.right);
                    return;
                }
                if (@abs(v.p2_x - v.ball_x) < margin) sink.move(.stop) else if (v.ball_x < v.p2_x) sink.move(.left) else if (v.ball_x > v.p2_x) sink.move(.right);
            }
        }
    }

    fn master(self: *Controller, v: Snapshot, random: anytype, sink: anytype) void {
        if (self.serve_type != -1 or (v.ball_x == 800 and v.ball_vx == 0)) {
            self.serve(v, random, sink);
            return;
        }
        const target = masterIntercept(v);
        const offset: f64 = if (target < 600) 0 else if (target < 700) 10 else 20;
        if (target < 450) {
            if (@abs(v.p2_x - 666) < 10) sink.move(.stop) else if (666 < v.p2_x) sink.move(.left) else if (666 > v.p2_x) sink.move(.right);
        } else if (@abs(v.p2_x - target - offset) < 10) {
            sink.move(.stop);
        } else if (target + offset < v.p2_x) {
            sink.move(.left);
        } else if (target + offset > v.p2_x) sink.move(.right);
        if ((v.p2_x > 900 and random.next() < 0.4) or target < 620 or (v.ball_y < 130 and v.ball_vy < 0)) return;
        if (nearEdge(v) or
            (2 * square(v.ball_x - v.p2_x) + square(v.ball_y - v.p2_y) < square(185) and v.ball_x != v.p2_x) or
            lowSpeed(v) or
            (@abs(v.ball_x - v.p2_x) < 150 and v.ball_y > 50 and v.ball_y < 250)) sink.move(.jump);
    }

    fn serve(self: *Controller, v: Snapshot, random: anytype, sink: anytype) void {
        if (self.serve_type == -1) {
            if (random.next() < 0.3) {
                self.serve_type = if (v.p1_x < 300) 0 else if (v.p1_x > 200) 1 else 2;
            } else self.serve_type = 2;
            if (self.serve_type == -1 or random.next() < 0.3) self.serve_type = @intFromFloat(trunc(3 * random.next()));
        }
        switch (self.serve_type) {
            0, 1 => {
                const target: f64 = if (self.serve_type == 0) 860 else 840;
                if (v.ball_vy > 12 and v.p2_x < target) sink.move(.right);
                if (v.p2_x >= target) sink.move(.stop);
                if (v.ball_vy == -3 and v.p2_x != 800) sink.move(.jump);
                if (v.ball_vy < -12 and v.p2_y != 0 and v.p2_x >= target - 15 and self.serve_type == 0) sink.move(.left);
                if (v.ball_x < 700) self.serve_type = -1;
            },
            2 => {
                if (v.ball_vy > 12 and v.p2_x > 770) sink.move(.left);
                if (v.p2_x <= 770) sink.move(.stop);
                if (v.ball_vy == -2 and v.p2_x != 800) sink.move(.jump);
                if (v.p2_y != 0 and v.ball_x > 800) self.serve_type = 3 + self.fakeJump(v, random);
            },
            3 => {
                if (v.p2_x > 585) sink.move(.left);
                if (v.p2_x <= 585) sink.move(.stop);
                if (v.ball_x <= 730) sink.move(.jump);
                if (v.ball_x < 540) self.serve_type = -1;
            },
            4 => {
                if (v.p2_x > 585) sink.move(.left);
                if (v.p2_x <= 585) sink.move(.stop);
                if (v.ball_x <= 730) sink.move(.jump);
                if (v.ball_x < 600) sink.move(.right);
                if (v.ball_x < 580) sink.move(.stop);
                if (v.ball_x < 540) self.serve_type = -1;
            },
            else => {},
        }
    }

    fn fakeJump(_: *Controller, v: Snapshot, random: anytype) c_int {
        const chance: f64 = if (v.p1_x < 200) 0.7 else if (v.p1_x > 300) 0.3 else 0.5;
        return if (random.next() < chance) 1 else 0;
    }
};

const Random = struct {
    fn next(_: *Random) f64 {
        return @as(f64, @floatFromInt(c.rand())) / (@as(f64, c.RAND_MAX) + 1.0);
    }
};

const Input = struct {
    controls: c.controls_t,

    fn move(self: *Input, action: Action) void {
        switch (action) {
            .left, .right => {
                self.controls.direction = if (action == .left) -1 else 1;
                self.controls.moved = true;
            },
            .jump => self.controls.jump = true,
            .stop => self.controls.direction = 0,
        }
    }
};

pub export fn bot_input(state: *c.game_t) c.controls_t {
    var controller = Controller{ .serve_type = state.bot_serve_type };
    var random: Random = .{};
    // No command means keep moving, as in the reference's input handlers.
    const vx = state.players[1].vel.x;
    var input: Input = .{ .controls = .{ .direction = if (vx < 0) -1 else if (vx > 0) 1 else 0, .jump = false, .moved = false } };
    controller.step(state.bot_tier, Snapshot.fromGame(state), &random, &input);
    state.bot_serve_type = controller.serve_type;
    return input.controls;
}
