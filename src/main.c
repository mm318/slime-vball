#include <math.h>

#include "app.h"
#include "ui.h"

const double FRAME_INTERVAL = 0.016;
const double POINT_DELAY = 1.5;

// The reference game uses upward-positive court units. These values convert
// its ball motion to pixels on the SDL court, where y increases downward.
static const float PIXELS_PER_X_UNIT = (float)SCREEN_WIDTH / COURT_UNITS;
static const float PIXELS_PER_Y_UNIT = (float)SCREEN_HEIGHT / COURT_UNITS;
static const float BALL_GRAVITY = (float)SCREEN_HEIGHT / COURT_UNITS;
static const float BALL_START_HEIGHT = 400.0f * SCREEN_HEIGHT / COURT_UNITS;
const float PLAYER_START_X = 200.0f * SCREEN_WIDTH / COURT_UNITS;
const float SLIME_CONTACT_RADIUS = 125.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float SLIME_MIN_CONTACT_RADIUS = 5.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float BALL_MAX_VX = 15.0f * SCREEN_WIDTH / COURT_UNITS;
const float BALL_MAX_VY = 22.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float BALL_WALL_INSET = 15.0f * SCREEN_WIDTH / COURT_UNITS;
const float BALL_GROUND_Y = PLAY_HEIGHT - 35.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float NET_REACH_X = 20.0f * SCREEN_WIDTH / COURT_UNITS;
static const float NET_CONTACT_Y = PLAY_HEIGHT - 140.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float NET_TOP_REBOUND_Y = PLAY_HEIGHT - 130.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float PLAYER_RADIUS = 50.0f * SCREEN_WIDTH / COURT_UNITS;
static const float PLAYER_NET_CLEARANCE = 5.0f * SCREEN_WIDTH / COURT_UNITS;
static const float PLAYER_SPEED = 8.0f * SCREEN_WIDTH / COURT_UNITS;
static const float PLAYER_JUMP_SPEED = 31.0f * SCREEN_HEIGHT / COURT_UNITS;
static const float PLAYER_GRAVITY = 2.0f * SCREEN_HEIGHT / COURT_UNITS;

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

void bounce_off_walls(body_t *volley) {
	const float right = SCREEN_WIDTH - BALL_WALL_INSET;
	if(volley->pos.x < BALL_WALL_INSET || volley->pos.x > right) {
		volley->pos.x = clamp(volley->pos.x, BALL_WALL_INSET, right);
		volley->vel.x = -volley->vel.x;
	}
}

void advance_ball(body_t *volley) {
	volley->vel.y += BALL_GRAVITY;
	volley->pos.x += volley->vel.x;
	volley->pos.y += volley->vel.y;
}

void reset_rally(game_t *state) {
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

void reset_match(game_t *state) {
	state->score[0] = state->score[1] = 0;
	state->server = 0;
	state->paused = false;
	state->bot_serve_type = -1;
	state->elapsed = 0;
	state->last_step = -1;
	state->level_bonus = state->time_bonus = state->flawless_bonus = 0;
	reset_rally(state);
}

void step_game(game_t *state, double now) {
	if(!advance_game_phase(state, now)) return;

	if(state->solo) {
		controls_t right = bot_input(state);
		if(right.moved) horizontal_input(state, 1);
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

Uint32 map_color(SDL_Surface *surface, Uint32 rgba) {
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

SDL_Surface *create_circle_sprite(int radius, int height, Uint32 color) {
	SDL_Surface *sprite = SDL_CreateRGBSurfaceWithFormat(0,
			2 * radius + 1, height, 32, SDL_PIXELFORMAT_RGBA32);
	if(sprite) {
		SDL_FillRect(sprite, NULL, map_color(sprite, 0));
		fill_circle(sprite, radius, radius, radius, color);
	}
	return sprite;
}

static void draw_slime(SDL_Surface *screen, const game_t *state, SDL_Rect rect, SDL_Surface *sprite, bool left, vec_t ball_pos) {
	Uint32 color = left || !state->solo ? 0xFFFF00FF : palette(state)->opponent;
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
			fill_circle(surface, x, POINT_CENTER_Y, POINT_RADIUS, color);
		}
		outline_circle(surface, x, POINT_CENTER_Y, POINT_RADIUS);
	}
}

void free_resources(graphics_t *graphics) {
	SDL_FreeSurface(graphics->slime1);
	SDL_FreeSurface(graphics->slime2);
	SDL_FreeSurface(graphics->ball);
	SDL_FreeSurface(graphics->screen);
	SDL_DestroyWindow(graphics->window);
	SDL_Quit();
}

bool player_key(game_t *state, SDL_Keycode key, bool pressed) {
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

void render_game(graphics_t *graphics, const game_t *state) {
	SDL_FillRect(graphics->screen, NULL, map_color(graphics->screen, palette(state)->sky));
	SDL_FillRect(graphics->screen, &graphics->ground, map_color(graphics->screen, palette(state)->ground));
	SDL_FillRect(graphics->screen, &graphics->net, map_color(graphics->screen, NET_COLOR));
	draw_score(graphics->screen, state, true);
	draw_score(graphics->screen, state, false);

	if(state->phase != TITLE && state->phase != RESULT) {
		positions_t display = display_positions(state);
		SDL_Rect p1 = {(int)roundf(display.players[0].x) - PLAYER_DRAW_RADIUS,
			(int)roundf(display.players[0].y) - PLAYER_DRAW_RADIUS, 0, 0};
		SDL_Rect p2 = {(int)roundf(display.players[1].x) - PLAYER_DRAW_RADIUS,
			(int)roundf(display.players[1].y) - PLAYER_DRAW_RADIUS, 0, 0};
		SDL_Rect ball_rect = {(int)roundf(display.ball.x) - BALL_DRAW_RADIUS,
			(int)roundf(display.ball.y) - BALL_DRAW_RADIUS, 0, 0};
		draw_slime(graphics->screen, state, p1, graphics->slime1, true, display.ball);
		draw_slime(graphics->screen, state, p2, graphics->slime2, false, display.ball);
		Uint32 ball_color = palette(state)->ball;
		SDL_SetSurfaceColorMod(graphics->ball, ball_color >> 24, (ball_color >> 16) & 0xff, (ball_color >> 8) & 0xff);
		SDL_BlitSurface(graphics->ball, NULL, graphics->screen, &ball_rect);
	}

	if(state->phase == TITLE) draw_menu(graphics->screen, state);
	else if(state->phase == RESULT) draw_result(graphics->screen, state);
	draw_hud(graphics->screen, state);
}

bool handle_event(game_t *state, SDL_Window *window, SDL_Event event, double now) {
	if(event.type == SDL_QUIT) {
		return false;
	}
	if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
		if(state->phase == RESULT) continue_result(state);
		else if(state->phase == TITLE) {
			int width = SCREEN_WIDTH, height = SCREEN_HEIGHT;
			if(window) SDL_GetWindowSize(window, &width, &height);
			SDL_Point point = {event.button.x * SCREEN_WIDTH / width,
				event.button.y * SCREEN_HEIGHT / height};
			SDL_Rect solo = menu_button(true), local = menu_button(false);
			if(SDL_PointInRect(&point, &solo)) start_match(state, true);
			else if(SDL_PointInRect(&point, &local)) start_match(state, false);
		}
		return true;
	}
	if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
		state->players[0].vel.x = state->players[1].vel.x = 0;
	}
	if(event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) {
		return true;
	}
	bool pressed = event.type == SDL_KEYDOWN;
	if(player_key(state, event.key.keysym.sym, pressed) || !pressed || event.key.repeat) {
		return true;
	}
	switch(event.key.keysym.sym) {
		case SDLK_1:
		case SDLK_2:
			start_match(state, event.key.keysym.sym == SDLK_1);
			break;
		case SDLK_c:
			if(state->phase == TITLE && state->bot_tier > 0) {
				state->solo = true;
				state->campaign_score = 0;
				reset_match(state);
			}
			break;
		case SDLK_k:
		case SDLK_r:
			state->campaign_score = 0;
			reset_match(state);
			break;
		case SDLK_p:
			toggle_pause(state, now);
			break;
		case SDLK_SPACE:
			press_replay(state, now);
			break;
	}
	return true;
}
