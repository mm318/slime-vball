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
        try expect(state.phase == (if (tier == c.BOT_TIERS - 1) c.TITLE else c.SERVE));
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

    // Replay waits for the point break, loops, then exits on Space.
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
    _ = try std.mem.printSentinel(&game.message, "Click the mouse to play!", .{}, 0);
    c.render_game(&graphics, &game);
    const button = c.menu_button(true);
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

    // Drive the actual mouse event handler: mode selection and result continuation.
    var click = std.mem.zeroes(c.SDL_Event);
    click.type = c.SDL_MOUSEBUTTONDOWN;
    click.button.button = c.SDL_BUTTON_LEFT;
    click.button.x = 600;
    click.button.y = 240;
    try expect(c.handle_event(&game, null, click, 0));
    try expect(!game.solo and game.phase == c.SERVE);
    game.score[0] = 5;
    game.ball.pos = .{ .x = 700, .y = c.BALL_GROUND_Y };
    game.ball.vel = .{ .x = 0, .y = 1 };
    c.step_game(&game, 0);
    try expect(game.phase == c.POINT_BREAK and game.score[0] == 6);
    try expect(std.mem.indexOf(u8, std.mem.sliceTo(&game.message, 0), "Slime 1 wins!") != null);
    c.step_game(&game, c.POINT_DELAY);
    try expect(game.phase == c.RESULT and game.local_result[0] != 0);
    try expect(c.handle_event(&game, null, click, 0));
    try expect(game.phase == c.TITLE);
    click.button.x = 100;
    try expect(c.handle_event(&game, null, click, 0));
    try expect(game.solo and game.phase == c.SERVE and game.bot_tier == 0);
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
