// Exercise prompt selection, progression, input, and actual SDL surface colors.
const std = @import("std");
const c = @import("c");
const expect = std.testing.expect;
comptime {
    _ = @import("game_modules");
}

fn expectMessage(state: *c.game_t, ending: []const u8) !void {
    c.point_message(state);
    try expect(std.mem.indexOf(u8, std.mem.sliceTo(&state.message, 0), ending) != null);
}

fn pixel(graphics: *const c.graphics_t, x: c_int, y: c_int) u32 {
    const surface = graphics.screen[0];
    const format = surface.format[0];
    const pixels: [*]const u8 = @ptrCast(surface.pixels.?);
    const offset: usize = @intCast(y * surface.pitch + x * 4);
    var value: u32 = undefined;
    @memcpy(std.mem.asBytes(&value), pixels[offset..][0..4]);
    return value & (format.Rmask | format.Gmask | format.Bmask | format.Amask);
}

fn saveScreenshot(graphics: *const c.graphics_t, directory: []const u8, filename: []const u8) !void {
    var buffer: [512]u8 = undefined;
    const path = try std.mem.printSentinel(&buffer, "{s}/{s}", .{ directory, filename }, 0);
    try expect(c.SDL_SaveBMP(graphics.screen, path.ptr) == 0);
}

fn run(directory: ?[]const u8) !void {
    try expect(c.BOT_TIERS == 3);
    try expect(c.text_width("MM") == 22);
    try expect(c.text_width("MM\nM") == 22);
    var state = std.mem.zeroes(c.game_t);
    c.start_match(&state, false);
    state.score[0] = 1;
    state.server = 0;
    try expectMessage(&state, "takes the lead!");
    state.touched[0] = true;
    state.contacts[0] = 1;
    try expectMessage(&state, "aces the serve!");
    state.touched[1] = true;
    try expectMessage(&state, "scores a winner!");
    state.contacts[0] = 0;
    state.score[1] = 1;
    try expectMessage(&state, "draws level!");
    state.score[0] = 3;
    try expectMessage(&state, "scores!");
    state.touched[0] = false;
    try expectMessage(&state, "laughs at his opponent's inability to serve!");
    try std.testing.expectEqualStrings("Inferior Human Controlled Slime 1\nlaughs at his opponent's inability to serve!", std.mem.sliceTo(&state.message, 0));
    state.score[0] = 6;
    try expectMessage(&state, "wins!");
    try expect(std.mem.indexOf(u8, std.mem.sliceTo(&state.message, 0), "Inferior Human Controlled Slime 1 ") != null);

    c.start_match(&state, true);
    for (0..c.BOT_TIERS) |tier| {
        try expect(state.bot_tier == tier);
        state.touched[0] = true;
        state.contacts[0] = 1;
        state.score[0] = 6;
        state.elapsed = 10;
        const before = state.campaign_score;
        c.point_message(&state);
        try expect(state.campaign_score == before + (@as(c_int, 200) << @intCast(tier)));
        c.finish_point(&state);
        try expect(state.phase == c.RESULT);
        try expect(state.level_bonus == (@as(c_int, 1000) << @intCast(tier)));
        try expect(state.time_bonus == (@as(c_int, 290) << @intCast(tier)));
        try expect(state.flawless_bonus == (@as(c_int, 1000) << @intCast(tier)));
        try expect(state.campaign_score == before + (@as(c_int, 2490) << @intCast(tier)));
        c.continue_result(&state);
        try expect(state.phase == c.SERVE);
        try expect(state.bot_tier == (if (tier == c.BOT_TIERS - 1) 0 else tier + 1));
    }

    // AI state survives a rally reset, but a new match recreates its controller.
    c.start_match(&state, true);
    try expect(state.bot_serve_type == -1);
    state.bot_serve_type = 4;
    c.reset_rally(&state);
    try expect(state.bot_serve_type == 4);
    c.reset_match(&state);
    try expect(state.bot_serve_type == -1);
    // A serve routine can issue no movement command: keep its previous velocity.
    state.bot_serve_type = 0;
    state.players[1].vel.x = 6;
    state.ball.pos.x = 600;
    var controls = c.bot_input(&state);
    try expect(controls.direction == 1 and !controls.moved and !controls.jump);
    // Translate court units and apply the reference's RIGHT/JUMP commands.
    const y_unit: f32 = 0.375;
    state.ball.pos.x = 600;
    state.ball.pos.y = @as(f32, c.PLAY_HEIGHT) - 290 * y_unit;
    state.ball.vel.y = 4 * y_unit;
    controls = c.bot_input(&state);
    try expect(controls.direction == 1 and controls.moved and controls.jump);
    c.step_game(&state, 0);
    try expect(state.players[1].pos.x == 606);
    try expect(state.players[1].pos.y == @as(f32, c.PLAY_HEIGHT) - 29 * y_unit);
    try expect(state.serve_moved);

    // An idle vertical serve does not earn ace/winner credit until moved.
    c.start_match(&state, false);
    for (0..23) |i| c.step_game(&state, @as(f64, @floatFromInt(i)) * c.FRAME_INTERVAL);
    try expect(!state.touched[0]);
    try expect(state.ball.vel.y == -c.BALL_MAX_VY);
    try expect(c.player_key(&state, c.SDLK_RIGHT, true));
    try expect(state.serve_moved);

    // Replay waits for the point break, loops, then exits on another request.
    state.phase = c.POINT_BREAK;
    state.point_deadline = 2;
    c.press_replay(&state, 1);
    try expect(state.phase == c.POINT_BREAK);
    c.step_game(&state, 2);
    try expect(state.phase == c.REPLAY);
    state.replay_index = state.history_count - 1;
    c.step_game(&state, 2.016);
    try expect(state.phase == c.REPLAY and state.replay_index == 0);
    c.press_replay(&state, 2.032);
    try expect(state.phase == c.SERVE);

    state.phase = c.POINT_BREAK;
    state.point_deadline = 10;
    c.toggle_pause(&state, 1);
    c.step_game(&state, 20);
    try expect(state.phase == c.POINT_BREAK);
    c.toggle_pause(&state, 21);
    try expect(state.point_deadline == 30);

    c.SDL_SetMainReady();
    try expect(c.SDL_Init(0) == 0);
    var graphics = std.mem.zeroes(c.graphics_t);
    defer c.free_resources(&graphics);
    graphics.screen = c.SDL_CreateRGBSurfaceWithFormat(0, c.SCREEN_WIDTH, c.SCREEN_HEIGHT, 32, c.SDL_PIXELFORMAT_RGBA32);
    try expect(graphics.screen != null);
    // A narrow text region wraps whole glyphs, retaining their 2x2 pixels.
    _ = c.SDL_FillRect(graphics.screen, null, c.map_color(graphics.screen, 0x000000FF));
    try expect(c.draw_text(graphics.screen, "HH", 100, 20, 10, c.NET_COLOR) == 2);
    try expect(pixel(&graphics, 95, 6) == c.map_color(graphics.screen, c.NET_COLOR));
    try expect(pixel(&graphics, 96, 6) == c.map_color(graphics.screen, c.NET_COLOR));
    try expect(pixel(&graphics, 97, 6) == c.map_color(graphics.screen, 0x000000FF));
    try expect(pixel(&graphics, 95, 26) == c.map_color(graphics.screen, c.NET_COLOR));
    try expect(c.draw_text(graphics.screen, "one two three", 200, 80, c.text_width("one two"), c.NET_COLOR) == 2);
    try expect(c.draw_text(graphics.screen, "one\ntwo", 200, 120, c.SCREEN_WIDTH, c.NET_COLOR) == 2);
    graphics.slime1 = c.create_circle_sprite(c.PLAYER_DRAW_RADIUS, c.PLAYER_DRAW_RADIUS, c.NET_COLOR);
    graphics.slime2 = c.create_circle_sprite(c.PLAYER_DRAW_RADIUS, c.PLAYER_DRAW_RADIUS, c.NET_COLOR);
    graphics.ball = c.create_circle_sprite(c.BALL_DRAW_RADIUS, 2 * c.BALL_DRAW_RADIUS + 1, c.NET_COLOR);
    try expect(graphics.slime1 != null and graphics.slime2 != null and graphics.ball != null);
    graphics.ground = .{ .x = 0, .y = c.PLAY_HEIGHT, .w = c.SCREEN_WIDTH, .h = c.SCREEN_HEIGHT - c.PLAY_HEIGHT };
    graphics.net = .{ .x = 373, .y = 262, .w = 4, .h = 43 };
    var game = std.mem.zeroes(c.game_t);
    for (0..c.BOT_TIERS) |tier| {
        c.start_match(&game, true);
        game.bot_tier = @intCast(tier);
        c.render_game(&graphics, &game);
        const palette = c.palette(&game)[0];
        try expect(pixel(&graphics, 0, 60) == c.map_color(graphics.screen, palette.sky));
        try expect(pixel(&graphics, 0, 350) == c.map_color(graphics.screen, palette.ground));
        try expect(pixel(&graphics, 150, 150) == c.map_color(graphics.screen, palette.ball));
        try expect(pixel(&graphics, 130, 290) == c.map_color(graphics.screen, 0xFFFF00FF));
        try expect(pixel(&graphics, 620, 290) == c.map_color(graphics.screen, palette.opponent));
        if (directory) |dir| {
            var buffer: [32]u8 = undefined;
            try saveScreenshot(&graphics, dir, try std.mem.print(&buffer, "stage-{d}.bmp", .{tier + 1}));
        }
        game.score[0] = 6;
        c.finish_point(&game);
        c.render_game(&graphics, &game);
        if (directory) |dir| {
            var buffer: [32]u8 = undefined;
            try saveScreenshot(&graphics, dir, try std.mem.print(&buffer, "win-{d}.bmp", .{tier + 1}));
        }
        c.reset_match(&game);
        game.score[1] = 6;
        game.server = 1;
        c.finish_point(&game);
        c.render_game(&graphics, &game);
    }
    c.start_match(&game, false);
    c.render_game(&graphics, &game);
    try expect(pixel(&graphics, 620, 290) == c.map_color(graphics.screen, 0xFFFF00FF));
    game.phase = c.TITLE;
    c.render_game(&graphics, &game);
    const button = c.ui_button_rect(c.UI_SOLO);
    try expect(pixel(&graphics, button.x, button.y) == c.map_color(graphics.screen, c.NET_COLOR));
    if (directory) |dir| {
        try saveScreenshot(&graphics, dir, "menu.bmp");
        game.solo = true;
        game.bot_tier = 1;
        c.render_game(&graphics, &game);
        try saveScreenshot(&graphics, dir, "continue-menu.bmp");
        c.start_match(&game, true);
        game.campaign_score = 12345;
        game.touched[1] = true;
        c.point_message(&game);
        game.phase = c.POINT_BREAK;
        c.render_game(&graphics, &game);
        try saveScreenshot(&graphics, dir, "wrapped-prompts.bmp");
        c.start_match(&game, false);
        game.phase = c.TITLE;
    }

    try checkInputAndMenus(&graphics, directory);
}

fn key(state: *c.game_t, kind: u32, code: c.SDL_Keycode, now: f64) !void {
    var event = std.mem.zeroes(c.SDL_Event);
    event.type = kind;
    event.key.keysym.sym = code;
    try expect(c.handle_event(state, null, &event, now));
}

fn mouse(state: *c.game_t, kind: u32, x: c_int, y: c_int, now: f64) !void {
    var event = std.mem.zeroes(c.SDL_Event);
    event.type = kind;
    event.button.button = c.SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    try expect(c.handle_event(state, null, &event, now));
}

fn finger(state: *c.game_t, kind: u32, id: i64, x: f32, y: f32, now: f64) !void {
    var event = std.mem.zeroes(c.SDL_Event);
    event.type = kind;
    event.tfinger.touchId = 1;
    event.tfinger.fingerId = id;
    event.tfinger.x = x / c.SCREEN_WIDTH;
    event.tfinger.y = y / c.SCREEN_HEIGHT;
    try expect(c.handle_event(state, null, &event, now));
}

fn clickButton(state: *c.game_t, button: c_int, touch: bool, now: f64) !void {
    const rect = c.ui_button_rect(button);
    const x = rect.x + @divTrunc(rect.w, 2);
    const y = rect.y + @divTrunc(rect.h, 2);
    if (touch) {
        try finger(state, c.SDL_FINGERDOWN, 99, @floatFromInt(x), @floatFromInt(y), now);
        try finger(state, c.SDL_FINGERUP, 99, @floatFromInt(x), @floatFromInt(y), now);
    } else {
        try mouse(state, c.SDL_MOUSEBUTTONDOWN, x, y, now);
        try mouse(state, c.SDL_MOUSEBUTTONUP, x, y, now);
    }
}

fn checkInputAndMenus(graphics: *c.graphics_t, directory: ?[]const u8) !void {
    var state = std.mem.zeroes(c.game_t);
    c.start_match(&state, true);
    state.phase = c.TITLE;
    try expect(c.menu_visible(&state));
    try expect(!c.ui_button_enabled(&state, c.UI_CONTINUE));
    try expect(!c.ui_button_enabled(&state, c.UI_REPLAY));
    try expect(!c.ui_button_enabled(&state, c.UI_PAUSE));
    c.render_game(graphics, &state);
    const disabled = c.ui_button_rect(c.UI_CONTINUE);
    try expect(pixel(graphics, disabled.x, disabled.y) == c.map_color(graphics.screen, 0x666666FF));
    for ([_]c.SDL_Keycode{ c.SDLK_1, c.SDLK_2, c.SDLK_c, c.SDLK_p, c.SDLK_r, c.SDLK_k, c.SDLK_SPACE, c.SDLK_UP }) |code| {
        try key(&state, c.SDL_KEYDOWN, code, 0);
    }
    try expect(state.phase == c.TITLE and !state.paused);
    try clickButton(&state, c.UI_CONTINUE, true, 0);
    try expect(state.phase == c.TITLE);
    // Moving a touch over a menu button is a swipe, not a tap.
    const solo = c.ui_button_rect(c.UI_SOLO);
    const sx: f32 = @floatFromInt(solo.x + @divTrunc(solo.w, 2));
    const sy: f32 = @floatFromInt(solo.y + @divTrunc(solo.h, 2));
    try finger(&state, c.SDL_FINGERDOWN, 77, sx, sy, 0);
    try finger(&state, c.SDL_FINGERMOTION, 77, sx + 40, sy, 0);
    try finger(&state, c.SDL_FINGERUP, 77, sx + 40, sy, 0);
    try expect(state.phase == c.TITLE);
    try mouse(&state, c.SDL_MOUSEBUTTONDOWN, @intFromFloat(sx), @intFromFloat(sy), 0);
    try expect(state.phase == c.TITLE);
    try mouse(&state, c.SDL_MOUSEBUTTONUP, @intFromFloat(sx), @intFromFloat(sy), 0);
    try expect(state.phase == c.SERVE and state.solo);

    // Keyboard movement remains; mode selection and Continue have no shortcuts.
    for ([_]c.SDL_Keycode{ c.SDLK_1, c.SDLK_2, c.SDLK_c, c.SDLK_r, c.SDLK_k }) |code| {
        try key(&state, c.SDL_KEYDOWN, code, 0);
    }
    try expect(state.phase == c.SERVE and state.solo and !state.paused);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_RIGHT, 0);
    try expect(state.players[0].vel.x == 6);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_LEFT, 0);
    try expect(state.players[0].vel.x == -6);
    try key(&state, c.SDL_KEYUP, c.SDLK_LEFT, 0);
    try expect(state.players[0].vel.x == 6);
    try key(&state, c.SDL_KEYUP, c.SDLK_RIGHT, 0);
    try expect(state.players[0].vel.x == 0);
    // Space is unavailable during a rally; P opens and closes the pause menu.
    try key(&state, c.SDL_KEYDOWN, c.SDLK_SPACE, 0);
    try expect(state.phase == c.SERVE and !state.replay_requested);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_p, 0);
    try expect(state.paused and c.menu_visible(&state));
    try key(&state, c.SDL_KEYUP, c.SDLK_p, 0);
    var repeat = std.mem.zeroes(c.SDL_Event);
    repeat.type = c.SDL_KEYDOWN;
    repeat.key.keysym.sym = c.SDLK_p;
    repeat.key.repeat = 1;
    try expect(c.handle_event(&state, null, &repeat, 0));
    try expect(state.paused);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_p, 0);
    try expect(!state.paused and state.phase == c.SERVE);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_UP, 0);
    try expect(state.players[0].vel.y < 0);
    c.reset_match(&state);
    // Small movements and reversals apply immediately, including before the
    // next physics step. Reversing does not require crossing an older anchor.
    try finger(&state, c.SDL_FINGERDOWN, 1, 600, 250, 0);
    try expect(state.players[0].vel.x == 0);
    try finger(&state, c.SDL_FINGERMOTION, 1, 601, 250, 0);
    try expect(state.players[0].vel.x == 6);
    try finger(&state, c.SDL_FINGERMOTION, 1, 608, 250, 0);
    try finger(&state, c.SDL_FINGERMOTION, 1, 614, 250, 0);
    try finger(&state, c.SDL_FINGERMOTION, 1, 613, 250, 0);
    try expect(state.players[0].vel.x == -6);
    try finger(&state, c.SDL_FINGERMOTION, 1, 613.5, 250, 0);
    try expect(state.players[0].vel.x == 6);
    try finger(&state, c.SDL_FINGERMOTION, 1, 613.25, 250, 0);
    try expect(state.players[0].vel.x == -6);
    const start_x = state.players[0].pos.x;
    c.step_game(&state, 0);
    try expect(state.players[0].pos.x == start_x - 6);
    // An upward drag needs 18 court pixels to jump. Vertical-only
    // motion preserves horizontal movement; airborne swipes do not restart a jump.
    try finger(&state, c.SDL_FINGERMOTION, 1, 613.25, 233, 0);
    try expect(state.players[0].vel.y == 0 and state.players[0].vel.x == -6);
    try finger(&state, c.SDL_FINGERMOTION, 1, 613.25, 231, 0);
    try expect(state.players[0].vel.y == -11.625);
    c.step_game(&state, c.FRAME_INTERVAL);
    const airborne_vy = state.players[0].vel.y;
    try finger(&state, c.SDL_FINGERMOTION, 1, 613.25, 211, 0);
    try expect(state.players[0].vel.y == airborne_vy);
    try finger(&state, c.SDL_FINGERUP, 1, 613.25, 211, 0);
    try expect(state.players[0].vel.x == 0);
    // A held keyboard direction resumes when a drag ends.
    try key(&state, c.SDL_KEYDOWN, c.SDLK_RIGHT, 0);
    try finger(&state, c.SDL_FINGERDOWN, 2, 200, 250, 0);
    try finger(&state, c.SDL_FINGERMOTION, 2, 180, 250, 0);
    try expect(state.players[0].vel.x == -6);
    try finger(&state, c.SDL_FINGERUP, 2, 180, 250, 0);
    try expect(state.players[0].vel.x == 6);
    try key(&state, c.SDL_KEYUP, c.SDLK_RIGHT, 0);
    try expect(state.players[0].vel.x == 0);
    // SDL's synthetic touch mouse events must not activate P a second time.
    var synthetic = std.mem.zeroes(c.SDL_Event);
    synthetic.type = c.SDL_MOUSEBUTTONDOWN;
    synthetic.button.which = c.SDL_TOUCH_MOUSEID;
    synthetic.button.button = c.SDL_BUTTON_LEFT;
    synthetic.button.x = c.SCREEN_WIDTH - 26;
    synthetic.button.y = 26;
    try expect(c.handle_event(&state, null, &synthetic, 0));
    synthetic.type = c.SDL_MOUSEBUTTONUP;
    try expect(c.handle_event(&state, null, &synthetic, 0));
    try expect(!state.paused);

    try clickButton(&state, c.UI_PAUSE, true, 1);
    try expect(state.paused and c.menu_visible(&state));
    try clickButton(&state, c.UI_LOCAL, true, 1);
    try expect(!state.solo and state.phase == c.SERVE and !state.paused);
    // Separate fingers control each player without stealing the other gesture.
    try finger(&state, c.SDL_FINGERDOWN, 11, 200, 250, 1);
    try finger(&state, c.SDL_FINGERDOWN, 12, 600, 250, 1);
    try finger(&state, c.SDL_FINGERMOTION, 11, 230, 220, 1);
    try finger(&state, c.SDL_FINGERMOTION, 12, 570, 250, 1);
    try expect(state.players[0].vel.x == 6 and state.players[1].vel.x == -6);
    try expect(state.players[0].vel.y == -11.625);
    try finger(&state, c.SDL_FINGERMOTION, 11, 229.5, 220, 1);
    try expect(state.players[0].vel.x == -6 and state.players[1].vel.x == -6);
    try finger(&state, c.SDL_FINGERMOTION, 12, 570.5, 250, 1);
    try expect(state.players[0].vel.x == -6 and state.players[1].vel.x == 6);
    try finger(&state, c.SDL_FINGERMOTION, 11, 230, 220, 1);
    try finger(&state, c.SDL_FINGERMOTION, 12, 570, 250, 1);
    try finger(&state, c.SDL_FINGERDOWN, 13, 620, 250, 1);
    try finger(&state, c.SDL_FINGERMOTION, 13, 650, 250, 1);
    try finger(&state, c.SDL_FINGERUP, 13, 650, 250, 1);
    try expect(state.players[1].vel.x == -6);
    try finger(&state, c.SDL_FINGERUP, 11, 230, 220, 1);
    try expect(state.players[0].vel.x == 0 and state.players[1].vel.x == -6);
    state.elapsed = 1;
    try clickButton(&state, c.UI_PAUSE, false, 1);
    try expect(state.paused and state.players[1].vel.x == 0);
    c.render_game(graphics, &state);
    if (directory) |dir| try saveScreenshot(graphics, dir, "pause.bmp");
    const x = state.players[0].pos.x;
    c.step_game(&state, 10);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_RIGHT, 10);
    try expect(state.players[0].pos.x == x and state.players[0].vel.x == 0);
    try clickButton(&state, c.UI_CONTINUE, true, 12);
    try expect(!state.paused and state.phase == c.SERVE);
    c.step_game(&state, 12);
    try expect(state.elapsed == 1);

    // R is active only during a recorded point break or replay; pause resumes it.
    c.reset_match(&state);
    for (0..23) |i| c.step_game(&state, @as(f64, @floatFromInt(i)) * c.FRAME_INTERVAL);
    state.phase = c.POINT_BREAK;
    state.point_deadline = 3;
    state.last_step = 1.5;
    try expect(c.ui_button_enabled(&state, c.UI_REPLAY));
    try clickButton(&state, c.UI_REPLAY, true, 1.5);
    try expect(state.replay_requested and state.phase == c.POINT_BREAK);
    try expect(!c.ui_button_enabled(&state, c.UI_REPLAY));
    c.step_game(&state, 3);
    try expect(state.phase == c.REPLAY and c.ui_button_enabled(&state, c.UI_REPLAY));
    c.render_game(graphics, &state);
    if (directory) |dir| try saveScreenshot(graphics, dir, "replay.bmp");
    try key(&state, c.SDL_KEYUP, c.SDLK_SPACE, 3);
    try expect(state.phase == c.REPLAY);
    try clickButton(&state, c.UI_PAUSE, true, 3);
    try expect(state.paused and !c.ui_button_enabled(&state, c.UI_REPLAY));
    try clickButton(&state, c.UI_CONTINUE, false, 4);
    try expect(!state.paused and state.phase == c.REPLAY);
    try clickButton(&state, c.UI_REPLAY, false, 4);
    try expect(state.phase == c.SERVE and !c.ui_button_enabled(&state, c.UI_REPLAY));

    // Space requests and exits replay, while holding it cannot skip playback.
    c.step_game(&state, 4);
    state.phase = c.POINT_BREAK;
    state.point_deadline = 6;
    try key(&state, c.SDL_KEYUP, c.SDLK_SPACE, 4);
    try expect(!state.replay_requested);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_SPACE, 4);
    try expect(state.replay_requested and state.phase == c.POINT_BREAK);
    c.step_game(&state, 6);
    try expect(state.phase == c.REPLAY);
    repeat.key.keysym.sym = c.SDLK_SPACE;
    try expect(c.handle_event(&state, null, &repeat, 6));
    try expect(state.phase == c.REPLAY);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_p, 6);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_SPACE, 6);
    try expect(state.paused and state.phase == c.REPLAY);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_p, 7);
    try expect(!state.paused and state.phase == c.REPLAY);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_SPACE, 7);
    try expect(state.phase == c.SERVE);

    c.start_match(&state, true);
    state.bot_tier = 1;
    state.score[1] = 6;
    state.server = 1;
    c.finish_point(&state);
    try expect(state.phase == c.RESULT and c.ui_button_enabled(&state, c.UI_CONTINUE));
    c.render_game(graphics, &state);
    if (directory) |dir| try saveScreenshot(graphics, dir, "loss.bmp");
    try mouse(&state, c.SDL_MOUSEBUTTONDOWN, 20, 200, 0);
    try mouse(&state, c.SDL_MOUSEBUTTONUP, 20, 200, 0);
    try key(&state, c.SDL_KEYDOWN, c.SDLK_c, 0);
    try expect(state.phase == c.RESULT);
    try clickButton(&state, c.UI_CONTINUE, true, 0);
    try expect(state.solo and state.bot_tier == 1 and state.phase == c.SERVE and state.score[1] == 0 and state.campaign_score == 0);
    // Continue preserves stage advancement after wins and rematches local play.
    c.start_match(&state, true);
    state.score[0] = 6;
    state.server = 0;
    c.finish_point(&state);
    const campaign_score = state.campaign_score;
    try clickButton(&state, c.UI_CONTINUE, false, 0);
    try expect(state.bot_tier == 1 and state.campaign_score == campaign_score);
    state.bot_tier = c.BOT_TIERS - 1;
    state.score[0] = 6;
    c.finish_point(&state);
    try clickButton(&state, c.UI_CONTINUE, true, 0);
    try expect(state.solo and state.bot_tier == 0 and state.phase == c.SERVE and state.campaign_score == 0);
    c.start_match(&state, false);
    state.score[1] = 6;
    state.server = 1;
    c.finish_point(&state);
    try clickButton(&state, c.UI_CONTINUE, true, 0);
    try expect(!state.solo and state.phase == c.SERVE and state.score[1] == 0);
    // Mode choices always start fresh, whether on initial, pause or result menus.
    for (0..3) |screen_kind| {
        for ([_]c_int{ c.UI_SOLO, c.UI_LOCAL }) |button| {
            c.start_match(&state, true);
            state.bot_tier = 2;
            state.score[0] = 4;
            state.campaign_score = 1234;
            switch (screen_kind) {
                0 => state.phase = c.TITLE,
                1 => c.toggle_pause(&state, 0),
                else => state.phase = c.RESULT,
            }
            try clickButton(&state, button, screen_kind != 1, 1);
            try expect(state.solo == (button == c.UI_SOLO));
            try expect(state.phase == c.SERVE and !state.paused and state.bot_tier == 0);
            try expect(state.score[0] == 0 and state.score[1] == 0 and state.campaign_score == 0);
        }
    }
    // Focus loss cancels a drag, preventing a stuck slime.
    try finger(&state, c.SDL_FINGERDOWN, 14, 200, 250, 0);
    try finger(&state, c.SDL_FINGERMOTION, 14, 220, 250, 0);
    var focus = std.mem.zeroes(c.SDL_Event);
    focus.type = c.SDL_WINDOWEVENT;
    focus.window.event = c.SDL_WINDOWEVENT_FOCUS_LOST;
    try expect(c.handle_event(&state, null, &focus, 0));
    try expect(!state.input.drags[0].active and state.players[0].vel.x == 0);
}

pub export fn main(argc: c_int, argv: [*c][*c]u8) c_int {
    const directory = if (argc > 1) std.mem.span(argv[1]) else null;
    run(directory) catch |err| {
        std.debug.print("FAIL UI checks: {s}\n", .{@errorName(err)});
        if (@errorReturnTrace()) |trace| std.debug.dumpErrorReturnTrace(trace);
        return 1;
    };
    std.debug.print("PASS prompts, campaign, replay, pause, and three SDL palettes\n", .{});
    return 0;
}
