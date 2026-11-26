#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static const double FRAME_INTERVAL = 0.016;
static const double POINT_DELAY = 1.5;

enum {
	SCREEN_WIDTH = 750,
	SCREEN_HEIGHT = 375,
	COURT_UNITS = 1000,
	PLAY_HEIGHT = 300,
	NET_WIDTH = 4,
	NET_PADDING = 5,
	PLAYER_DRAW_RADIUS = 37,
	PLAYER_EYE_X = 17,
	PLAYER_EYE_Y = 13,
	PLAYER_EYE_RADIUS = 7,
	PLAYER_PUP_RADIUS = 3,
	BALL_DRAW_RADIUS = 11,
	MAX_SCORE = 6,
	POINT_PADDING = 35,
	POINT_RADIUS = 12,
	REPLAY_FRAMES = 240,
	BOT_TIERS = 5,
};

static const Uint32 NET_COLOR = 0xFFFFFFFF;

// CSS colors and wording from the pinned OneSlime.min.js snapshot.
typedef struct { Uint32 sky, ground, ball, opponent; } palette_t;
static const palette_t palettes[BOT_TIERS] = {
	{0x0000FFFF, 0x808080FF, 0xFFFF00FF, 0xFFFFFFFF},
	{0x1E5000FF, 0xA9A9A9FF, 0x8080FFFF, 0xFF0000FF},
	{0x623939FF, 0x00A800FF, 0xFFFFFFFF, 0x000000FF},
	{0x000000FF, 0xFF0000FF, 0xFFFF00FF, 0x0000FFFF},
	{0x000000FF, 0xFF0000FF, 0xFFFF00FF, 0x0000FFFF},
};
static const char *const names[] = {
	"Inferior Human Controlled Slime ", "The Pathetic White Slime ",
	"Angry Red Slimons ", "The Slime Master ", "Psycho Slime ", "The Big Blue Boss ",
};
static const char *const loss_lines[BOT_TIERS][2] = {
	{"You are a loser!", "Better luck next time."},
	{"Angry Red Slimons gives you the gong!", "So who has the red face bombing out on level 2, huh?"},
	{"The Slime Master says \"You are seriously inept.\"", "Congrats on reaching level 3."},
	{"Psycho Slime laughs at the pathetic slow opposition.", "Congrats on reaching level 4!"},
	{"The Big Blue Boss devours you!", "Yum."},
};

// The reference game uses upward-positive court units. These values convert
// its ball motion to pixels on the SDL court, where y increases downward.
static const float PIXELS_PER_X_UNIT = (float)SCREEN_WIDTH / COURT_UNITS;
static const float PIXELS_PER_Y_UNIT = (float)SCREEN_HEIGHT / COURT_UNITS;
static const float BALL_GRAVITY = (float)SCREEN_HEIGHT / COURT_UNITS;
static const float BALL_START_HEIGHT = 400.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float PLAYER_START_X = 200.0f * SCREEN_WIDTH / COURT_UNITS;
static const float SLIME_CONTACT_RADIUS = 125.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float SLIME_MIN_CONTACT_RADIUS = 5.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float BALL_MAX_VX = 15.0f * SCREEN_WIDTH / COURT_UNITS;
static const float BALL_MAX_VY = 22.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float BALL_WALL_INSET = 15.0f * SCREEN_WIDTH / COURT_UNITS;
static const float BALL_GROUND_Y = PLAY_HEIGHT - 35.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float NET_REACH_X = 20.0f * SCREEN_WIDTH / COURT_UNITS;
static const float NET_CONTACT_Y = PLAY_HEIGHT - 140.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float NET_TOP_REBOUND_Y = PLAY_HEIGHT - 130.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float PLAYER_RADIUS = 50.0f * SCREEN_WIDTH / COURT_UNITS;
static const float PLAYER_NET_CLEARANCE = 5.0f * SCREEN_WIDTH / COURT_UNITS;
static const float PLAYER_SPEED = 8.0f * SCREEN_WIDTH / COURT_UNITS;
static const float PLAYER_JUMP_SPEED = 31.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float PLAYER_GRAVITY = 2.0f * SCREEN_HEIGHT / COURT_UNITS;

typedef struct {
	float x;
	float y;
} vec_t;

typedef struct {
	vec_t pos;
	vec_t vel;
} body_t;

typedef struct {
	int direction;
	bool jump;
} controls_t;

typedef struct {
	vec_t players[2];
	vec_t ball;
} positions_t;

typedef enum {
	TITLE,
	SERVE,
	RALLY,
	POINT_BREAK,
	REPLAY,
	RESULT,
} phase_t;

typedef struct {
	body_t players[2];
	body_t ball;
	int score[2];
	int server;
	bool solo;
	int bot_tier;
	bool paused;
	phase_t phase;
	bool serve_moved;
	bool touched[2];
	int contacts[2];
	bool replay_requested;
	int campaign_score;
	int level_bonus, time_bonus, flawless_bonus;
	double elapsed, last_step;
	char message[256];
	char local_result[128];
	double paused_at;
	double point_deadline;
	positions_t history[REPLAY_FRAMES];
	int history_next;
	int history_count;
	int replay_index;
} game_t;

static game_t game = {.solo = true};
static SDL_Window *window;
static SDL_Surface *screen, *slime1, *slime2, *ball;
static SDL_Rect ground, net;
static double clock_frequency;
static double next_frame;

static const palette_t *palette(const game_t *state) {
	return &palettes[state->solo ? state->bot_tier : 0];
}

static float clamp(float value, float low, float high) {
	return fminf(fmaxf(value, low), high);
}

static void jump_player(body_t *player) {
	if(player->pos.y == PLAY_HEIGHT) {
		player->vel.y = -PLAYER_JUMP_SPEED;
	}
}

static void move_player(body_t *player, int side) {
	const float net_edge = SCREEN_WIDTH / 2.0f + PLAYER_NET_CLEARANCE;
	const float low = side == 0 ? PLAYER_RADIUS : net_edge + PLAYER_RADIUS;
	const float high = side == 0 ? SCREEN_WIDTH - net_edge - PLAYER_RADIUS
		: SCREEN_WIDTH - PLAYER_RADIUS;
	player->pos.x = clamp(player->pos.x + player->vel.x, low, high);

	if(player->vel.y != 0) {
		player->vel.y += PLAYER_GRAVITY;
		player->pos.y += player->vel.y;
		if(player->pos.y > PLAY_HEIGHT) {
			player->pos.y = PLAY_HEIGHT;
			player->vel.y = 0;
		}
	}
}

static bool collide_with_player(body_t *volley, const body_t *player) {
	vec_t offset = {volley->pos.x - player->pos.x, volley->pos.y - player->pos.y};
	float distance_sq = offset.x * offset.x + offset.y * offset.y;
	if(offset.y >= 0 || distance_sq >= SLIME_CONTACT_RADIUS * SLIME_CONTACT_RADIUS
			|| distance_sq <= SLIME_MIN_CONTACT_RADIUS * SLIME_MIN_CONTACT_RADIUS) {
		return false;
	}

	float distance = sqrtf(distance_sq);
	vec_t normal = {offset.x / distance, offset.y / distance};
	volley->pos.x = player->pos.x + normal.x * SLIME_CONTACT_RADIUS;
	volley->pos.y = player->pos.y + normal.y * SLIME_CONTACT_RADIUS;

	// Project relative velocity in court units, where the axes scale differently.
	float relative_x = (volley->vel.x - player->vel.x) / PIXELS_PER_X_UNIT;
	float relative_y = (volley->vel.y - player->vel.y) / PIXELS_PER_Y_UNIT;
	float approach = relative_x * normal.x + relative_y * normal.y;
	if(approach <= 0) {
		float shift_x = -2 * approach * normal.x * PIXELS_PER_X_UNIT;
		float shift_y = -2 * approach * normal.y * PIXELS_PER_Y_UNIT;
		volley->vel.x = clamp(volley->vel.x + player->vel.x + shift_x, -BALL_MAX_VX, BALL_MAX_VX);
		volley->vel.y = clamp(volley->vel.y + player->vel.y + shift_y, -BALL_MAX_VY, BALL_MAX_VY);
	}
	return true;
}

static void collide_with_net(body_t *volley) {
	const float center = SCREEN_WIDTH / 2.0f;
	float offset_x = volley->pos.x - center;
	if(fabsf(offset_x) >= NET_REACH_X || volley->pos.y <= NET_CONTACT_Y) {
		return;
	}

	if(volley->vel.y > 0 && volley->pos.y < NET_TOP_REBOUND_Y) {
		volley->pos.y = NET_TOP_REBOUND_Y;
		volley->vel.y = -volley->vel.y;
	} else {
		float outward = offset_x < 0 ? -1.0f : 1.0f;
		volley->pos.x = center + outward * NET_REACH_X;
		volley->vel.x = outward * fabsf(volley->vel.x);
	}
}

static void bounce_off_walls(body_t *volley) {
	const float right = SCREEN_WIDTH - BALL_WALL_INSET;
	if(volley->pos.x < BALL_WALL_INSET || volley->pos.x > right) {
		volley->pos.x = clamp(volley->pos.x, BALL_WALL_INSET, right);
		volley->vel.x = -volley->vel.x;
	}
}

static void advance_ball(body_t *volley) {
	volley->vel.y += BALL_GRAVITY;
	volley->pos.x += volley->vel.x;
	volley->pos.y += volley->vel.y;
}

static float landing_x(body_t volley) {
	const float intercept_y = PLAY_HEIGHT - SLIME_CONTACT_RADIUS;
	for(int frame = 0; frame < 75; frame++) {
		if(volley.pos.y >= intercept_y && volley.vel.y >= 0) {
			break;
		}
		advance_ball(&volley);
		bounce_off_walls(&volley);
	}
	return volley.pos.x;
}

static controls_t bot_input(const game_t *state) {
	const body_t *player = &state->players[1];
	const body_t *volley = &state->ball;
	// Later stage palettes reuse the strongest existing controller.
	float aim = 22.0f - 4.0f * (state->bot_tier < 3 ? state->bot_tier : 2);
	float target = SCREEN_WIDTH * 0.72f;
	bool serving = state->phase == SERVE && state->server == 1;
	if(serving) {
		target = volley->pos.x + aim;
	} else {
		float intercept = landing_x(*volley);
		if(intercept >= SCREEN_WIDTH / 2.0f) {
			target = intercept + aim;
		}
	}
	float error = target - player->pos.x;
	float ball_dx = volley->pos.x - player->pos.x;
	bool jump = (serving ? volley->vel.y > 4.0f : volley->vel.y > 0.5f)
		&& volley->pos.y > 143.0f && volley->pos.y < 215.0f
		&& ball_dx > -44.0f && ball_dx < 7.0f;
	return (controls_t){fabsf(error) <= 3.0f ? 0 : error > 0 ? 1 : -1, jump};
}

static positions_t positions(const game_t *state) {
	return (positions_t){{state->players[0].pos, state->players[1].pos}, state->ball.pos};
}

static void record_frame(game_t *state) {
	state->history[state->history_next] = positions(state);
	state->history_next = (state->history_next + 1) % REPLAY_FRAMES;
	if(state->history_count < REPLAY_FRAMES) {
		state->history_count++;
	}
}

static void reset_rally(game_t *state) {
	state->players[0] = (body_t){.pos = {PLAYER_START_X, PLAY_HEIGHT}};
	state->players[1] = (body_t){.pos = {SCREEN_WIDTH - PLAYER_START_X, PLAY_HEIGHT}};
	state->ball = (body_t){.pos = {state->players[state->server].pos.x,
		PLAY_HEIGHT - BALL_START_HEIGHT}};
	state->phase = SERVE;
	state->serve_moved = false;
	state->touched[0] = state->touched[1] = false;
	state->contacts[0] = state->contacts[1] = 0;
	state->replay_requested = false;
	state->message[0] = '\0';
	state->history_next = 0;
	state->history_count = 0;
	record_frame(state);
}

static void reset_match(game_t *state) {
	state->score[0] = state->score[1] = 0;
	state->server = 0;
	state->paused = false;
	state->elapsed = 0;
	state->last_step = -1;
	state->level_bonus = state->time_bonus = state->flawless_bonus = 0;
	reset_rally(state);
}

static void start_match(game_t *state, bool solo) {
	state->solo = solo;
	state->bot_tier = 0;
	state->campaign_score = 0;
	reset_match(state);
}

static bool match_over(const game_t *state) {
	return state->score[state->server] >= MAX_SCORE;
}

static void finish_point(game_t *state) {
	if(match_over(state)) {
		state->phase = RESULT;
		if(state->solo && state->server == 0) {
			int multiplier = 1 << state->bot_tier;
			state->level_bonus = 1000 * state->score[0] * multiplier
				/ (state->score[0] + state->score[1]);
			state->time_bonus = (int)(fmax(0, 300 - state->elapsed) * multiplier);
			state->flawless_bonus = state->score[1] == 0 ? 1000 * multiplier : 0;
			state->campaign_score += state->level_bonus + state->time_bonus + state->flawless_bonus;
		} else if(!state->solo) {
			int winner = state->server + 1, loser = 3 - winner;
			switch(rand() % 5) {
				case 0: snprintf(state->local_result, sizeof(state->local_result),
					"C'mon player %d, I'll take you on!", winner); break;
				case 1: snprintf(state->local_result, sizeof(state->local_result),
					"Inferior human controlled slime %d is insipid!", loser); break;
				case 2: snprintf(state->local_result, sizeof(state->local_result),
					"Inferior human controlled slime %d is rubbish!", loser); break;
				case 3: snprintf(state->local_result, sizeof(state->local_result),
					"Super inferior human controlled slime %d wins!", winner); break;
				default: snprintf(state->local_result, sizeof(state->local_result),
					"You're both yellow cowards. Play me instead!"); break;
			}
		}
	} else {
		reset_rally(state);
	}
}

static void continue_result(game_t *state) {
	if(state->solo && state->server == 0 && state->bot_tier + 1 < BOT_TIERS) {
		state->bot_tier++;
		reset_match(state);
	} else {
		if(state->solo && state->server == 0) {
			state->bot_tier = 0;
			state->campaign_score = 0;
		}
		state->phase = TITLE;
		snprintf(state->message, sizeof(state->message), "Click the mouse to play!");
	}
}

static void horizontal_input(game_t *state, int side) {
	float start = side == 0 ? PLAYER_START_X : SCREEN_WIDTH - PLAYER_START_X;
	if(state->players[side].pos.x == start && state->ball.pos.x == start
			&& !state->touched[1 - side]) {
		state->serve_moved = true;
	}
}

static void record_contact(game_t *state, int side) {
	if(state->serve_moved) {
		state->touched[side] = true;
		state->contacts[side]++;
		state->contacts[1 - side] = 0;
	}
}

static void point_message(game_t *state) {
	int winner = state->server, loser = 1 - winner;
	bool ace = state->touched[winner] && !state->touched[loser];
	bool winning_hit = !ace && state->contacts[winner] > 0;
	if(state->solo && winner == 0) {
		state->campaign_score += (ace ? 200 : winning_hit ? 100 : 50) << state->bot_tier;
	}
	const char *ending = match_over(state) ? "wins!"
		: ace ? "aces the serve!" : winning_hit ? "scores a winner!"
		: !state->touched[winner] && state->touched[loser]
			? "laughs at his opponent's inability to serve!"
		: state->score[0] == state->score[1] ? "draws level!"
		: state->score[winner] == state->score[loser] + 1 ? "takes the lead!" : "scores!";
	if(state->solo) {
		snprintf(state->message, sizeof(state->message), "%s%s",
			names[winner == 0 ? 0 : state->bot_tier + 1], ending);
	} else {
		snprintf(state->message, sizeof(state->message), "%s%d %s", names[0], winner + 1, ending);
	}
}

static void press_replay(game_t *state, double now) {
	if(state->paused) {
		return;
	}
	if(state->phase == REPLAY) {
		finish_point(state);
	} else if(state->phase == POINT_BREAK && now < state->point_deadline) {
		state->replay_requested = true;
	}
}

static void toggle_pause(game_t *state, double now) {
	if(state->phase == TITLE || state->phase == RESULT || state->phase == REPLAY) return;
	state->paused = !state->paused;
	if(state->paused) {
		state->paused_at = now;
	} else if(state->phase == POINT_BREAK) {
		state->point_deadline += now - state->paused_at;
	}
}

static void step_game(game_t *state, double now) {
	if(state->last_step >= 0 && !state->paused
			&& (state->phase == SERVE || state->phase == RALLY)) {
		state->elapsed += now - state->last_step;
	}
	state->last_step = now;
	if(state->phase == TITLE || state->phase == RESULT) return;
	if(state->paused) {
		return;
	}
	if(state->phase == REPLAY) {
		if(++state->replay_index == state->history_count) {
			state->replay_index = 0;
		}
		return;
	}
	if(state->phase == POINT_BREAK) {
		if(now >= state->point_deadline) {
			if(state->replay_requested) {
				state->phase = REPLAY;
				state->replay_index = -1;
			} else finish_point(state);
		}
		return;
	}

	if(state->solo) {
		controls_t right = bot_input(state);
		if(right.direction) horizontal_input(state, 1);
		state->players[1].vel.x = right.direction * PLAYER_SPEED;
		if(right.jump) {
			jump_player(&state->players[1]);
		}
	}
	move_player(&state->players[0], 0);
	move_player(&state->players[1], 1);

	body_t *volley = &state->ball;
	advance_ball(volley);
	bool left_contact = collide_with_player(volley, &state->players[0]);
	bool right_contact = collide_with_player(volley, &state->players[1]);
	if(left_contact) record_contact(state, 0);
	if(right_contact) record_contact(state, 1);
	if(state->phase == SERVE && (left_contact || right_contact)) {
		state->phase = RALLY;
	}
	bounce_off_walls(volley);
	collide_with_net(volley);

	if(volley->pos.y > BALL_GROUND_Y) {
		state->server = volley->pos.x > SCREEN_WIDTH / 2.0f ? 0 : 1;
		state->score[state->server]++;
		point_message(state);
		state->phase = POINT_BREAK;
		state->point_deadline = now + POINT_DELAY;
	}
	record_frame(state);
}

static Uint32 map_color(SDL_Surface *surface, Uint32 rgba) {
	return SDL_MapRGBA(surface->format, rgba >> 24, (rgba >> 16) & 0xff,
			(rgba >> 8) & 0xff, rgba & 0xff);
}

static void fill_circle(SDL_Surface *surface, int x, int y, int radius,
		Uint32 rgba) {
	Uint32 color = map_color(surface, rgba);
	for(int dy = -radius; dy <= radius; dy++) {
		int half_width = (int)sqrt(radius * radius - dy * dy);
		SDL_Rect row = {x - half_width, y + dy, 2 * half_width + 1, 1};
		SDL_FillRect(surface, &row, color);
	}
}

static void outline_circle(SDL_Surface *surface, int cx, int cy, int radius) {
	Uint32 color = map_color(surface, 0xFFFFFFFF);
	int x = radius, y = 0, error = 1 - radius;
	while(y <= x) {
		const SDL_Rect pixels[] = {
			{cx + x, cy + y, 1, 1}, {cx + y, cy + x, 1, 1},
			{cx - y, cy + x, 1, 1}, {cx - x, cy + y, 1, 1},
			{cx - x, cy - y, 1, 1}, {cx - y, cy - x, 1, 1},
			{cx + y, cy - x, 1, 1}, {cx + x, cy - y, 1, 1},
		};
		SDL_FillRects(surface, pixels, SDL_arraysize(pixels), color);

		y++;
		if(error >= 0) {
			x--;
			error -= 2 * x;
		}
		error += 2 * y + 1;
	}
}

static SDL_Surface *create_circle_sprite(int radius, int height, Uint32 color) {
	SDL_Surface *sprite = SDL_CreateRGBSurfaceWithFormat(0,
			2 * radius + 1, height, 32, SDL_PIXELFORMAT_RGBA32);
	if(sprite) {
		SDL_FillRect(sprite, NULL, map_color(sprite, 0));
		fill_circle(sprite, radius, radius, radius, color);
	}
	return sprite;
}

static void draw_slime(SDL_Rect rect, SDL_Surface *sprite, bool left, vec_t ball_pos) {
	Uint32 color = left || !game.solo ? 0xFFFF00FF : palette(&game)->opponent;
	if(!left && game.solo && game.bot_tier == 3 && ((int)(game.elapsed / FRAME_INTERVAL) & 1)) color = NET_COLOR;
	SDL_SetSurfaceColorMod(sprite, color >> 24, (color >> 16) & 0xff, (color >> 8) & 0xff);
	int x = rect.x + (left ? 2 * PLAYER_DRAW_RADIUS - PLAYER_EYE_X : PLAYER_EYE_X);
	int y = rect.y + PLAYER_EYE_Y;
	SDL_BlitSurface(sprite, NULL, screen, &rect);

	float dx = ball_pos.x - x;
	float dy = ball_pos.y - y;
	float distance = hypotf(dx, dy);
	int pupil_x = x;
	int pupil_y = y;
	if(distance > 0) {
		const int travel = PLAYER_EYE_RADIUS - PLAYER_PUP_RADIUS;
		pupil_x += (int)(travel * dx / distance);
		pupil_y += (int)(travel * dy / distance);
	}
	fill_circle(screen, x, y, PLAYER_EYE_RADIUS, 0xFFFFFFFF);
	fill_circle(screen, pupil_x, pupil_y, PLAYER_PUP_RADIUS, 0x000000FF);
}

static void draw_score(SDL_Surface *surface, const game_t *state, bool left) {
	int x0 = left ? POINT_PADDING : SCREEN_WIDTH - POINT_PADDING;
	int dx = left ? POINT_PADDING : -POINT_PADDING;
	int score = state->score[left ? 0 : 1];
	Uint32 color = left || !state->solo ? 0xFFFF00FF : palette(state)->opponent;
	for(int i = 0; i < MAX_SCORE; i++) {
		int x = x0 + i * dx;
		if(i < score) {
			fill_circle(surface, x, 30, POINT_RADIUS, color);
		}
		outline_circle(surface, x, 30, POINT_RADIUS);
	}
}

static void free_resources(void) {
	SDL_FreeSurface(slime1);
	SDL_FreeSurface(slime2);
	SDL_FreeSurface(ball);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

static bool player_key(game_t *state, SDL_Keycode key, bool pressed) {
	int side;
	int direction;
	switch(key) {
		case SDLK_LEFT:  side = 0; direction = -1; break;
		case SDLK_RIGHT: side = 0; direction =  1; break;
		case SDLK_UP:    side = 0; direction =  0; break;
		case SDLK_a:     side = 1; direction = -1; break;
		case SDLK_d:     side = 1; direction =  1; break;
		case SDLK_w:     side = 1; direction =  0; break;
		default: return false;
	}
	if(state->phase == TITLE || state->phase == RESULT || state->phase == REPLAY) return true;
	if(side == 1 && state->solo) {
		return true;
	}

	body_t *player = &state->players[side];
	if(pressed && direction == 0) {
		jump_player(player);
	} else if(pressed) {
		horizontal_input(state, side);
		player->vel.x = direction * PLAYER_SPEED;
	} else if(direction * player->vel.x > 0) {
		player->vel.x = 0;
	}
	return true;
}

// The original five-by-seven bitmap font, extended for the reference copy.
static const Uint8 glyphs[95][7] = {
	['A' - ' '] = {14,17,17,31,17,17,17},
	['B' - ' '] = {30,17,17,30,17,17,30},
	['C' - ' '] = {14,17,16,16,16,17,14},
	['D' - ' '] = {30,17,17,17,17,17,30},
	['E' - ' '] = {31,16,16,30,16,16,31},
	['F' - ' '] = {31,16,16,30,16,16,16},
	['G' - ' '] = {14,17,16,23,17,17,15},
	['H' - ' '] = {17,17,17,31,17,17,17},
	['I' - ' '] = {14,4,4,4,4,4,14},
	['J' - ' '] = {7,2,2,2,2,18,12},
	['K' - ' '] = {17,18,20,24,20,18,17},
	['L' - ' '] = {16,16,16,16,16,16,31},
	['M' - ' '] = {17,27,21,21,17,17,17},
	['N' - ' '] = {17,25,21,19,17,17,17},
	['O' - ' '] = {14,17,17,17,17,17,14},
	['P' - ' '] = {30,17,17,30,16,16,16},
	['Q' - ' '] = {14,17,17,17,21,18,13},
	['R' - ' '] = {30,17,17,30,20,18,17},
	['S' - ' '] = {15,16,16,14,1,1,30},
	['T' - ' '] = {31,4,4,4,4,4,4},
	['U' - ' '] = {17,17,17,17,17,17,14},
	['V' - ' '] = {17,17,17,17,17,10,4},
	['W' - ' '] = {17,17,17,21,21,27,17},
	['X' - ' '] = {17,17,10,4,10,17,17},
	['Y' - ' '] = {17,17,10,4,4,4,4},
	['Z' - ' '] = {31,1,2,4,8,16,31},
	['a' - ' '] = {0,0,14,1,15,17,15},
	['b' - ' '] = {16,16,30,17,17,17,30},
	['c' - ' '] = {0,0,14,17,16,17,14},
	['d' - ' '] = {1,1,15,17,17,17,15},
	['e' - ' '] = {0,0,14,17,31,16,14},
	['f' - ' '] = {6,9,8,28,8,8,8},
	['g' - ' '] = {0,15,17,17,15,1,14},
	['h' - ' '] = {16,16,30,17,17,17,17},
	['i' - ' '] = {4,0,12,4,4,4,14},
	['j' - ' '] = {2,0,6,2,2,18,12},
	['k' - ' '] = {16,16,18,20,24,20,18},
	['l' - ' '] = {12,4,4,4,4,4,14},
	['m' - ' '] = {0,0,26,21,21,17,17},
	['n' - ' '] = {0,0,30,17,17,17,17},
	['o' - ' '] = {0,0,14,17,17,17,14},
	['p' - ' '] = {0,30,17,17,30,16,16},
	['q' - ' '] = {0,15,17,17,15,1,1},
	['r' - ' '] = {0,0,22,25,16,16,16},
	['s' - ' '] = {0,0,15,16,14,1,30},
	['t' - ' '] = {8,8,28,8,8,9,6},
	['u' - ' '] = {0,0,17,17,17,19,13},
	['v' - ' '] = {0,0,17,17,17,10,4},
	['w' - ' '] = {0,0,17,17,21,21,10},
	['x' - ' '] = {0,0,17,10,4,10,17},
	['y' - ' '] = {0,17,17,17,15,1,14},
	['z' - ' '] = {0,0,31,2,4,8,31},
	['0' - ' '] = {14,17,19,21,25,17,14},
	['1' - ' '] = {4,12,4,4,4,4,14},
	['2' - ' '] = {14,17,1,2,4,8,31},
	['3' - ' '] = {30,1,1,14,1,1,30},
	['4' - ' '] = {2,6,10,18,31,2,2},
	['5' - ' '] = {31,16,16,30,1,1,30},
	['6' - ' '] = {14,16,16,30,17,17,14},
	['7' - ' '] = {31,1,2,4,8,8,8},
	['8' - ' '] = {14,17,17,14,17,17,14},
	['9' - ' '] = {14,17,17,15,1,1,14},
	['!' - ' '] = {4,4,4,4,4,0,4},
	['?' - ' '] = {14,17,1,2,4,0,4},
	['.' - ' '] = {0,0,0,0,0,0,4},
	[',' - ' '] = {0,0,0,0,0,4,8},
	[':' - ' '] = {0,4,0,0,4,0,0},
	['\'' - ' '] = {4,4,8,0,0,0,0},
	['"' - ' '] = {10,10,0,0,0,0,0},
	['/' - ' '] = {1,2,2,4,8,8,16},
	['-' - ' '] = {0,0,0,31,0,0,0},
	['+' - ' '] = {0,4,4,31,4,4,0},
	['(' - ' '] = {2,4,8,8,8,4,2},
	[')' - ' '] = {8,4,2,2,2,4,8},
};

static void draw_text(const char *text, int center, int baseline, int max_width, Uint32 rgba) {
	int length = (int)SDL_strlen(text);
	if(length == 0) return;
	// Keep the existing 2x font height; fit long reference prompts to the court.
	float cell = fminf(12, (float)max_width / length);
	float x = center - length * cell / 2;
	Uint32 color = map_color(screen, rgba);
	for(; *text; text++, x += cell) {
		unsigned char code = (unsigned char)*text;
		if(code < 32 || code > 126) code = '?';
		const Uint8 *rows = glyphs[code - 32];
		for(int row = 0; row < 7; row++) {
			for(int col = 0; col < 5; col++) {
				if(rows[row] & (16 >> col)) {
					int left = (int)(x + col * cell / 6);
					int right = (int)(x + (col + 1) * cell / 6);
					SDL_Rect pixel = {left, baseline - 14 + row * 2, right - left, 2};
					SDL_FillRect(screen, &pixel, color);
				}
			}
		}
	}
}

static void centered_text(const char *text, int baseline) {
	draw_text(text, SCREEN_WIDTH / 2, baseline, SCREEN_WIDTH - 30, NET_COLOR);
}

static SDL_Rect menu_button(bool solo) {
	return (SDL_Rect){solo ? 10 : SCREEN_WIDTH / 2 + 10, 210, SCREEN_WIDTH / 2 - 20, 57};
}

static void draw_menu(void) {
	centered_text("Slime Volleyball: One Slime", 106);
	centered_text("Written by Quin Pendragon and Daniel Wedge", 153);
	centered_text("http://oneslime.net/", 181);
	for(int side = 0; side < 2; side++) {
		SDL_Rect rect = menu_button(side == 0);
		SDL_FillRect(screen, &rect, map_color(screen, NET_COLOR));
		draw_text(side == 0 ? "Click here to start a one player game!"
			: "Click here to start a two player game!", rect.x + rect.w / 2,
			game.bot_tier > 0 && side == 0 ? 234 : 248, rect.w - 16, palette(&game)->sky);
		if(game.bot_tier > 0 && side == 0) {
			draw_text("or press C to continue...", rect.x + rect.w / 2, 257, rect.w - 16, palette(&game)->sky);
		}
	}
}

static void draw_result(void) {
	char text[128];
	if(game.solo && game.server == 0) {
		if(game.bot_tier == BOT_TIERS - 1) {
			centered_text("YOU WIN!", 181);
			centered_text("The Slimes bow down before the new Slime King!", 204);
		} else {
			snprintf(text, sizeof(text), "Level %d clear!", game.bot_tier + 1);
			centered_text(text, 125);
			snprintf(text, sizeof(text), "Your score: %d", game.campaign_score);
			centered_text(text, 169);
			snprintf(text, sizeof(text), "Level bonus: %d points", game.level_bonus);
			centered_text(text, 207);
			snprintf(text, sizeof(text), "Time bonus: %d points", game.time_bonus);
			centered_text(text, 226);
			if(game.flawless_bonus) {
				snprintf(text, sizeof(text), "Flawless Victory: %d points", game.flawless_bonus);
				centered_text(text, 245);
			}
		}
	} else {
		SDL_Rect panel = {15, 87, SCREEN_WIDTH - 30, 140};
		SDL_FillRect(screen, &panel, map_color(screen, palette(&game)->ground));
		if(game.solo) {
			centered_text(loss_lines[game.bot_tier][0], 131);
			centered_text(loss_lines[game.bot_tier][1], 150);
		} else centered_text(game.local_result, 131);
		centered_text("GAME OVER", 207);
	}
	centered_text("Click the mouse to continue...", 329);
}

static void draw_hud(void) {
	char text[128], time[32];
	int seconds = (int)game.elapsed;
	if(seconds >= 3600) snprintf(time, sizeof(time), "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else snprintf(time, sizeof(time), "%d:%02d", seconds / 60, seconds % 60);
	bool running = game.phase != TITLE && game.phase != RESULT;
	if(game.solo) snprintf(text, sizeof(text), running ? "Score: %d   Time: %s" : "Score: %d", game.campaign_score, time);
	else snprintf(text, sizeof(text), running ? "   Time: %s" : "", time);
	SDL_Rect backing = {(SCREEN_WIDTH - (int)SDL_strlen(text) * 8 - 20) / 2, 0, (int)SDL_strlen(text) * 8 + 20, 42};
	SDL_FillRect(screen, &backing, map_color(screen, palette(&game)->sky));
	draw_text(text, SCREEN_WIDTH / 2, 38, backing.w - 20, NET_COLOR);
	const char *message = game.paused ? "Press P to continue . . ."
		: game.phase == REPLAY ? "Press space to continue..." : game.message;
	if(game.phase != RESULT) centered_text(message, 329);
}

static positions_t display_positions(const game_t *state) {
	if(state->phase != REPLAY) {
		return positions(state);
	}
	int oldest = (state->history_next + REPLAY_FRAMES - state->history_count) % REPLAY_FRAMES;
	int index = state->replay_index < 0 ? 0 : state->replay_index;
	return state->history[(oldest + index) % REPLAY_FRAMES];
}

static void render_game(void) {
	SDL_FillRect(screen, NULL, map_color(screen, palette(&game)->sky));
	SDL_FillRect(screen, &ground, map_color(screen, palette(&game)->ground));
	SDL_FillRect(screen, &net, map_color(screen, NET_COLOR));
	draw_score(screen, &game, true);
	draw_score(screen, &game, false);

	if(game.phase != TITLE && game.phase != RESULT) {
		positions_t display = display_positions(&game);
		SDL_Rect p1 = {(int)roundf(display.players[0].x) - PLAYER_DRAW_RADIUS,
			(int)roundf(display.players[0].y) - PLAYER_DRAW_RADIUS, 0, 0};
		SDL_Rect p2 = {(int)roundf(display.players[1].x) - PLAYER_DRAW_RADIUS,
			(int)roundf(display.players[1].y) - PLAYER_DRAW_RADIUS, 0, 0};
		SDL_Rect ball_rect = {(int)roundf(display.ball.x) - BALL_DRAW_RADIUS,
			(int)roundf(display.ball.y) - BALL_DRAW_RADIUS, 0, 0};
		draw_slime(p1, slime1, true, display.ball);
		draw_slime(p2, slime2, false, display.ball);
		Uint32 ball_color = palette(&game)->ball;
		SDL_SetSurfaceColorMod(ball, ball_color >> 24, (ball_color >> 16) & 0xff, (ball_color >> 8) & 0xff);
		SDL_BlitSurface(ball, NULL, screen, &ball_rect);
	}

	if(game.phase == TITLE) draw_menu();
	else if(game.phase == RESULT) draw_result();
	draw_hud();

}

static bool draw_frame(void) {
	render_game();
	return SDL_UpdateWindowSurface(window) == 0;
}

static double frame_time(void) {
	return SDL_GetPerformanceCounter() / clock_frequency;
}

static bool handle_event(SDL_Event event, double now) {
	if(event.type == SDL_QUIT) {
		return false;
	}
	if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
		if(game.phase == RESULT) continue_result(&game);
		else if(game.phase == TITLE) {
			SDL_Point point = {event.button.x, event.button.y};
			SDL_Rect solo = menu_button(true), local = menu_button(false);
			if(SDL_PointInRect(&point, &solo)) start_match(&game, true);
			else if(SDL_PointInRect(&point, &local)) start_match(&game, false);
		}
		return true;
	}
	if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
		game.players[0].vel.x = game.players[1].vel.x = 0;
	}
	if(event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) {
		return true;
	}
	bool pressed = event.type == SDL_KEYDOWN;
	if(player_key(&game, event.key.keysym.sym, pressed) || !pressed || event.key.repeat) {
		return true;
	}
	switch(event.key.keysym.sym) {
		case SDLK_1:
		case SDLK_2:
			start_match(&game, event.key.keysym.sym == SDLK_1);
			break;
		case SDLK_c:
			if(game.phase == TITLE && game.bot_tier > 0) {
				game.solo = true;
				game.campaign_score = 0;
				reset_match(&game);
			}
			break;
		case SDLK_k:
		case SDLK_r:
			game.campaign_score = 0;
			reset_match(&game);
			break;
		case SDLK_p:
			toggle_pause(&game, now);
			break;
		case SDLK_SPACE:
			press_replay(&game, now);
			break;
	}
	return true;
}

static bool game_frame(void) {
	double now = frame_time();
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		if(!handle_event(event, now)) return false;
	}

	if(now < next_frame) {
		return true;
	}
	// Each accepted frame schedules one step, even after a long gap.
	next_frame = now + FRAME_INTERVAL;
	step_game(&game, now);

	if(!draw_frame()) {
		fprintf(stderr, "Could not present the game: %s\n", SDL_GetError());
		free_resources();
		exit(EXIT_FAILURE);
	}
	return true;
}

#ifdef __EMSCRIPTEN__
static void browser_frame(void) {
	if(!game_frame()) {
		emscripten_cancel_main_loop();
		free_resources();
	}
}
#endif

int main(void) {
	SDL_SetMainReady();
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
#ifdef __EMSCRIPTEN__
	SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif
	if(SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	window = SDL_CreateWindow("Slime Volleyball: One Slime",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	if(!window || !(screen = SDL_GetWindowSurface(window))) {
		fprintf(stderr, "Could not create the window: %s\n", SDL_GetError());
		free_resources();
		return EXIT_FAILURE;
	}

	slime1 = create_circle_sprite(PLAYER_DRAW_RADIUS, PLAYER_DRAW_RADIUS, NET_COLOR);
	slime2 = create_circle_sprite(PLAYER_DRAW_RADIUS, PLAYER_DRAW_RADIUS, NET_COLOR);
	ball = create_circle_sprite(BALL_DRAW_RADIUS, 2 * BALL_DRAW_RADIUS + 1, NET_COLOR);
	if(!slime1 || !slime2 || !ball) {
		fprintf(stderr, "Could not create the sprites: %s\n", SDL_GetError());
		free_resources();
		return EXIT_FAILURE;
	}

	ground = (SDL_Rect){0, PLAY_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - PLAY_HEIGHT};
	int net_top = (int)roundf(0.7f * SCREEN_HEIGHT);
	net = (SDL_Rect){SCREEN_WIDTH / 2 - NET_WIDTH / 2,
			net_top, NET_WIDTH, PLAY_HEIGHT - net_top + NET_PADDING};

	reset_match(&game);
	game.phase = TITLE;
	snprintf(game.message, sizeof(game.message), "Click the mouse to play!");
	clock_frequency = (double)SDL_GetPerformanceFrequency();
	next_frame = frame_time();

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(browser_frame, 0, 1);
#else
	while(game_frame()) {
		double remaining = (next_frame - frame_time()) * 1000.0;
		if(remaining > 0) {
			SDL_Delay((Uint32)ceil(remaining));
		}
	}
	free_resources();
#endif
	return EXIT_SUCCESS;
}
