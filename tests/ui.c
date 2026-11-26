// Exercise prompt selection, progression, input, and actual SDL surface colors.
#include <assert.h>
#include <string.h>
#include "../src/app.h"
#include "../src/ui.h"

static graphics_t graphics;
static game_t game;

static void expect_message(game_t *state, const char *ending) {
	point_message(state);
	assert(strstr(state->message, ending));
}

static Uint32 pixel(int x, int y) {
	Uint32 value;
	memcpy(&value, (Uint8 *)graphics.screen->pixels + y * graphics.screen->pitch + x * 4, 4);
	return value & (graphics.screen->format->Rmask | graphics.screen->format->Gmask | graphics.screen->format->Bmask | graphics.screen->format->Amask);
}

int ui_checks_main(int argc, char **argv) {
	assert(BOT_TIERS == 3);
	assert(text_width("MM") == 22);
	assert(text_width("MM\nM") == 22);
	game_t state = {0};
	start_match(&state, false);
	state.score[0] = 1;
	state.server = 0;
	expect_message(&state, "takes the lead!");
	state.touched[0] = true;
	state.contacts[0] = 1;
	expect_message(&state, "aces the serve!");
	state.touched[1] = true;
	expect_message(&state, "scores a winner!");
	state.contacts[0] = 0;
	state.score[1] = 1;
	expect_message(&state, "draws level!");
	state.score[0] = 3;
	expect_message(&state, "scores!");
	state.touched[0] = false;
	expect_message(&state, "laughs at his opponent's inability to serve!");
	assert(strcmp(state.message, "Inferior Human Controlled Slime 1\n"
		"laughs at his opponent's inability to serve!") == 0);
	state.score[0] = 6;
	expect_message(&state, "wins!");
	assert(strstr(state.message, "Inferior Human Controlled Slime 1 "));

	start_match(&state, true);
	for(int tier = 0; tier < BOT_TIERS; tier++) {
		assert(state.bot_tier == tier);
		state.touched[0] = true;
		state.contacts[0] = 1;
		state.score[0] = 6;
		state.elapsed = 10;
		int before = state.campaign_score;
		point_message(&state);
		assert(state.campaign_score == before + (200 << tier));
		finish_point(&state);
		assert(state.phase == RESULT);
		assert(state.level_bonus == (1000 << tier));
		assert(state.time_bonus == (290 << tier));
		assert(state.flawless_bonus == (1000 << tier));
		assert(state.campaign_score == before + (2490 << tier));
		continue_result(&state);
		assert(state.phase == (tier == BOT_TIERS - 1 ? TITLE : SERVE));
	}

	// An idle vertical serve does not earn ace/winner credit until moved.
	start_match(&state, false);
	for(int i = 0; i < 23; i++) step_game(&state, i * FRAME_INTERVAL);
	assert(!state.touched[0]);
	assert(state.ball.vel.y == -BALL_MAX_VY);
	assert(player_key(&state, SDLK_RIGHT, true));
	assert(state.serve_moved);

	// Replay waits for the point break, loops, then exits on Space.
	state.phase = POINT_BREAK;
	state.point_deadline = 2;
	press_replay(&state, 1);
	assert(state.phase == POINT_BREAK);
	step_game(&state, 2);
	assert(state.phase == REPLAY);
	state.replay_index = state.history_count - 1;
	step_game(&state, 2.016);
	assert(state.phase == REPLAY && state.replay_index == 0);
	press_replay(&state, 2.032);
	assert(state.phase == SERVE);

	state.phase = POINT_BREAK;
	state.point_deadline = 10;
	toggle_pause(&state, 1);
	step_game(&state, 20);
	assert(state.phase == POINT_BREAK);
	toggle_pause(&state, 21);
	assert(state.point_deadline == 30);

	SDL_SetMainReady();
	assert(SDL_Init(0) == 0);
	graphics.screen = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);
	assert(graphics.screen);
	// A narrow text region wraps whole glyphs, retaining their 2x2 pixels.
	SDL_FillRect(graphics.screen, NULL, map_color(graphics.screen, 0x000000FF));
	assert(draw_text(graphics.screen, "HH", 100, 20, 10, NET_COLOR) == 2);
	assert(pixel(95, 6) == map_color(graphics.screen, NET_COLOR));
	assert(pixel(96, 6) == map_color(graphics.screen, NET_COLOR));
	assert(pixel(97, 6) == map_color(graphics.screen, 0x000000FF));
	assert(pixel(95, 26) == map_color(graphics.screen, NET_COLOR));
	assert(draw_text(graphics.screen, "one two three", 200, 80, text_width("one two"), NET_COLOR) == 2);
	assert(draw_text(graphics.screen, "one\ntwo", 200, 120, SCREEN_WIDTH, NET_COLOR) == 2);
	graphics.slime1 = create_circle_sprite(PLAYER_DRAW_RADIUS, PLAYER_DRAW_RADIUS, NET_COLOR);
	graphics.slime2 = create_circle_sprite(PLAYER_DRAW_RADIUS, PLAYER_DRAW_RADIUS, NET_COLOR);
	graphics.ball = create_circle_sprite(BALL_DRAW_RADIUS, 2 * BALL_DRAW_RADIUS + 1, NET_COLOR);
	assert(graphics.slime1 && graphics.slime2 && graphics.ball);
	graphics.ground = (SDL_Rect){0, PLAY_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - PLAY_HEIGHT};
	graphics.net = (SDL_Rect){373, 262, 4, 43};
	for(int tier = 0; tier < BOT_TIERS; tier++) {
		start_match(&game, true);
		game.bot_tier = tier;
		render_game(&graphics, &game);
		assert(pixel(0, 60) == map_color(graphics.screen, palette(&game)->sky));
		assert(pixel(0, 350) == map_color(graphics.screen, palette(&game)->ground));
		assert(pixel(150, 150) == map_color(graphics.screen, palette(&game)->ball));
		assert(pixel(130, 290) == map_color(graphics.screen, 0xFFFF00FF));
		assert(pixel(620, 290) == map_color(graphics.screen, palette(&game)->opponent));

		if(argc > 1) {
			char path[512];
			snprintf(path, sizeof(path), "%s/stage-%d.bmp", argv[1], tier + 1);
			assert(SDL_SaveBMP(graphics.screen, path) == 0);
		}
		game.score[0] = 6;
		finish_point(&game);
		render_game(&graphics, &game);
		if(argc > 1) {
			char path[512];
			snprintf(path, sizeof(path), "%s/win-%d.bmp", argv[1], tier + 1);
			assert(SDL_SaveBMP(graphics.screen, path) == 0);
		}
		reset_match(&game);
		game.score[1] = 6;
		game.server = 1;
		finish_point(&game);
		render_game(&graphics, &game);
	}
	start_match(&game, false);
	render_game(&graphics, &game);
	assert(pixel(620, 290) == map_color(graphics.screen, 0xFFFF00FF));
	game.phase = TITLE;
	snprintf(game.message, sizeof(game.message), "Click the mouse to play!");
	render_game(&graphics, &game);
	SDL_Rect button = menu_button(true);
	assert(pixel(button.x, button.y) == map_color(graphics.screen, NET_COLOR));
	if(argc > 1) {
		char path[512];
		snprintf(path, sizeof(path), "%s/menu.bmp", argv[1]);
		assert(SDL_SaveBMP(graphics.screen, path) == 0);
		game.solo = true;
		game.bot_tier = 1;
		render_game(&graphics, &game);
		snprintf(path, sizeof(path), "%s/continue-menu.bmp", argv[1]);
		assert(SDL_SaveBMP(graphics.screen, path) == 0);
		start_match(&game, true);
		game.campaign_score = 12345;
		game.touched[1] = true;
		point_message(&game);
		game.phase = POINT_BREAK;
		render_game(&graphics, &game);
		snprintf(path, sizeof(path), "%s/wrapped-prompts.bmp", argv[1]);
		assert(SDL_SaveBMP(graphics.screen, path) == 0);
		start_match(&game, false);
		game.phase = TITLE;
	}

	// Drive the actual mouse event handler: mode selection and result continuation.
	SDL_Event click = {0};
	click.type = SDL_MOUSEBUTTONDOWN;
	click.button.button = SDL_BUTTON_LEFT;
	click.button.x = 600;
	click.button.y = 240;
	assert(handle_event(&game, NULL, click, 0));
	assert(!game.solo && game.phase == SERVE);
	game.score[0] = 5;
	game.ball.pos = (vec_t){700, BALL_GROUND_Y};
	game.ball.vel = (vec_t){0, 1};
	step_game(&game, 0);
	assert(game.phase == POINT_BREAK && game.score[0] == 6);
	assert(strstr(game.message, "Slime 1 wins!"));
	step_game(&game, POINT_DELAY);
	assert(game.phase == RESULT && game.local_result[0]);
	assert(handle_event(&game, NULL, click, 0));
	assert(game.phase == TITLE);
	click.button.x = 100;
	assert(handle_event(&game, NULL, click, 0));
	assert(game.solo && game.phase == SERVE && game.bot_tier == 0);
	free_resources(&graphics);
	puts("PASS prompts, campaign, replay, pause, and three SDL palettes");
	return 0;
}
