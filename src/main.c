#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define POW2(x) x*x

/*************
 * CONSTANTS *
 *************/

static const double FRAME_INTERVAL = 1.0 / 60.0;

const float EPSILON = 0.001f;

/* Colors */
const Uint32 BACKGROUND_COLOR = 0x0000FFFF;
const Uint32 GROUND_COLOR     = 0x808080FF;
const Uint32 BALL_COLOR       = 0xFFFF00FF;
const Uint32 PLAYER1_COLOR    = 0x00FFFFFF;
const Uint32 PLAYER2_COLOR    = 0xFF00FFFF;
const Uint32 NET_COLOR        = 0xFFFFFFFF;

/* Screen */
const int SCREEN_WIDTH  = 750;
const int SCREEN_HEIGHT = 375;

/* Height of play area */
const float PLAY_HEIGHT = 300.;

/* Ball */
const float BALL_RADIUS  = 10.;
const float BALL_GRAVITY = 0.4;

/* Slime */
const float PLAYER_RADIUS     = 36.;
const float PLAYER_EYE_X      = 17.;
const float PLAYER_EYE_Y      = 13.;
const float PLAYER_EYE_RADIUS = 7.;
const float PLAYER_PUP_X      = 14.;
const float PLAYER_PUP_Y      = 12.;
const float PLAYER_PUP_RADIUS = 3.;

/* Slime physics */
const float PLAYER_MOVE_ACC   = 2.;
const float PLAYER_MOVE_FRICT = 0.6;
const float PLAYER_JUMP_VEL   = 11.;
const float PLAYER_GRAVITY    = 0.7;

/* Bounce speeds */
const float MIN_BOUNCE_SPEED  = 3.;
const float MAX_BOUNCE_SPEED  = 13.;

/* Points */
const int MAX_SCORE = 6;
const int POINT_PADDING = 35;
const int POINT_RADIUS = 12;
const int POINT_DELAY = 1000;

/* Net */
const float NET_WIDTH   = 8.;
const float NET_HEIGHT  = 50.;
const float NET_PADDING = 8.;
const float NET_EPSILON = 5.;

/**************
 * STRUCTURES *
 **************/

typedef struct vect {
	float x;
	float y;
} vect_t;

typedef struct ent {
	vect_t pos;
	SDL_Rect rect;
	vect_t acc;
	vect_t vel;
} ent_t;

typedef struct player {
	int human;
	ent_t ent;
} player_t;

typedef struct match {
	player_t p1;
	player_t p2;
	ent_t ball;
} match_t;

typedef struct game {
	match_t match;
	int s1;
	int s2;
	int winner;
} game_t;

/*********
 * STATE *
 *********/

game_t game;

static SDL_Window *window;
static SDL_Surface *screen, *slime1, *slime2, *ball;

SDL_Rect ground, net;

ent_t net_full;

static double clock_frequency;
static double next_frame;
static double next_point;

/***********
 * METHODS *
 ***********/

/* Axis-aligned bounding-box collision */
int aabbox_coll(ent_t *e1, ent_t *e2) {
	if(e1->pos.y + e1->rect.h <= e2->pos.y) {
		return 0;
	}

	if(e1->pos.y >= e2->pos.y + e2->rect.h) {
		return 0;
	}

	if(e1->pos.x + e1->rect.w <= e2->pos.x) {
		return 0;
	}

	if(e1->pos.x >= e2->pos.x + e2->rect.w) {
		return 0;
	}

	return 1;
}

/* Player-ball collision handler */
void pb_coll_handler(player_t *p, ent_t *ball) {
	vect_t new_vel;
	float speed, tmp;

	if(aabbox_coll(&p->ent, ball)) {
		/* Magnitude of current ball + slime speed */
		speed = sqrt( POW2(ball->vel.x) + POW2(ball->vel.y) )
				+ sqrt( POW2(p->ent.vel.x)
				+ POW2(p->ent.vel.y) ) + EPSILON;
		if(speed < MIN_BOUNCE_SPEED) {
			speed = MIN_BOUNCE_SPEED;
		} else if(speed > MAX_BOUNCE_SPEED) {
			speed = MAX_BOUNCE_SPEED;
		}

		/* Direction of movement */
		new_vel.y = (ball->pos.y + BALL_RADIUS) -
				(p->ent.pos.y + PLAYER_RADIUS);
		new_vel.x = (ball->pos.x + BALL_RADIUS) -
				(p->ent.pos.x + PLAYER_RADIUS);


		/* Hit the flat side of the slime; adjust the normal vector */
		if(new_vel.y > 0) {
			if(ball->pos.x >= p->ent.pos.x + 2*PLAYER_RADIUS -
					EPSILON) {
				ball->vel.x = speed;
				ball->vel.y = 0;
			} else if(ball->pos.x + 2*BALL_RADIUS <= p->ent.pos.x -
					EPSILON) {
				ball->vel.x = -speed;
				ball->vel.y = 0;
			} else {
				ball->vel.x = 0;
				ball->vel.y = speed;
			}
		} else {
			tmp = POW2(ball->pos.x + BALL_RADIUS
					- (p->ent.pos.x + PLAYER_RADIUS))
					+ POW2(ball->pos.y + BALL_RADIUS
					- (p->ent.pos.y + PLAYER_RADIUS));
			if(tmp < POW2(BALL_RADIUS + PLAYER_RADIUS)) {
				ball->vel.x = new_vel.x;
				ball->vel.y = new_vel.y;

				tmp = sqrt( POW2(ball->vel.x)
						+ POW2(ball->vel.y) )
						+ EPSILON;

				ball->vel.x = speed * ball->vel.x / tmp;
				ball->vel.y = speed * ball->vel.y / tmp;
			}
		}
	}
}

/* Update entity */
void update_ent(ent_t *ent) {
	ent->vel.x += ent->acc.x;
	ent->vel.y += ent->acc.y;

	ent->pos.x += ent->vel.x;
	ent->pos.y += ent->vel.y;
}

/* Update entity rectangle */
void update_rect(ent_t *ent) {
	ent->rect.x = round(ent->pos.x);
	ent->rect.y = round(ent->pos.y);
}

/* Update player position */
void update_player(player_t *player) {
	update_ent(&player->ent);

	player->ent.vel.x *= PLAYER_MOVE_FRICT;

	if(player->ent.pos.y > PLAY_HEIGHT - PLAYER_RADIUS) {
		player->ent.pos.y = PLAY_HEIGHT - PLAYER_RADIUS;
		player->ent.vel.y = 0;
	}
}

/* Update all rectangles in a match */
void update_match(game_t *game) {
	int *score;

	update_ent(&game->match.ball);

	if(game->match.ball.pos.y > PLAY_HEIGHT - BALL_RADIUS*2) {
		game->match.ball.pos.y = PLAY_HEIGHT - BALL_RADIUS*2;

		if(game->match.ball.pos.x < SCREEN_WIDTH/2 - BALL_RADIUS) {
			score = &game->s2;
			game->winner = 2;
		} else {
			score = &game->s1;
			game->winner = 1;
		}

		(*score)++;
	}

	/* Global player updates */
	update_player(&game->match.p1);
	update_player(&game->match.p2);

	/* Player 1 x bounds */
	if(game->match.p1.ent.pos.x < 0.) {
		game->match.p1.ent.pos.x = 0.;
	} else if(game->match.p1.ent.pos.x
			> SCREEN_WIDTH/2 - NET_WIDTH/2 - 2*PLAYER_RADIUS - 2) {
		game->match.p1.ent.pos.x
			= SCREEN_WIDTH/2 - NET_WIDTH/2 - 2*PLAYER_RADIUS - 2;
	}

	/* Player 2 x bounds */
	if(game->match.p2.ent.pos.x > SCREEN_WIDTH - 2*PLAYER_RADIUS - 1) {
		game->match.p2.ent.pos.x = SCREEN_WIDTH - 2*PLAYER_RADIUS - 1;
	} else if(game->match.p2.ent.pos.x <
			SCREEN_WIDTH/2 + NET_WIDTH/2 + 1) {
		game->match.p2.ent.pos.x = SCREEN_WIDTH/2 + NET_WIDTH/2 + 1;
	}

	/* Ball x bounds */
	if(game->match.ball.pos.x <= 0) {
		game->match.ball.pos.x = 0;
		game->match.ball.vel.x *= -1;
	} else if(game->match.ball.pos.x >= SCREEN_WIDTH - 2*BALL_RADIUS) {
		game->match.ball.pos.x = SCREEN_WIDTH - 2*BALL_RADIUS;
		game->match.ball.vel.x *= -1;
	}

	/* Player-ball collision */
	pb_coll_handler(&game->match.p1, &game->match.ball);
	pb_coll_handler(&game->match.p2, &game->match.ball);

	/* Net collision */
	if(aabbox_coll(&net_full, &game->match.ball)) {
		if(game->match.ball.pos.y + 2 * BALL_RADIUS - NET_EPSILON >
			net_full.pos.y) {
			if(game->match.ball.vel.x > 0.) {
			game->match.ball.pos.x = SCREEN_WIDTH/2
				- NET_WIDTH/2 - 2*BALL_RADIUS;
			} else {
				game->match.ball.pos.x = SCREEN_WIDTH/2
					+ NET_WIDTH/2;
			}
			game->match.ball.vel.x *= -1;
		} else {
			game->match.ball.vel.y *= -1;
			game->match.ball.pos.y = PLAY_HEIGHT - NET_HEIGHT
				- 2*BALL_RADIUS;
		}
	}

	update_rect(&game->match.ball);
	update_rect(&game->match.p1.ent);
	update_rect(&game->match.p2.ent);
}

/* New match */
void new_match(game_t *game) {
	game->match.p1.ent.pos.x  = SCREEN_WIDTH/4 - PLAYER_RADIUS;
	game->match.p1.ent.pos.y  = PLAY_HEIGHT - PLAYER_RADIUS;
	game->match.p1.human = 1;
	game->match.p1.ent.acc.x = 0;
	game->match.p1.ent.vel.x = 0;
	game->match.p1.ent.vel.y = 0;

	game->match.p2.ent.pos.x  = 3*SCREEN_WIDTH/4 - PLAYER_RADIUS;
	game->match.p2.ent.pos.y  = PLAY_HEIGHT - PLAYER_RADIUS;
	game->match.p2.human = 1;
	game->match.p2.ent.acc.x = 0;
	game->match.p2.ent.vel.x = 0;
	game->match.p2.ent.vel.y = 0;

	if(game->winner == 1) {
		game->match.ball.pos.x = SCREEN_WIDTH/4 - BALL_RADIUS;
	} else {
		game->match.ball.pos.x = 3*SCREEN_WIDTH/4 - BALL_RADIUS;
	}

	game->match.ball.pos.y = PLAY_HEIGHT/2;
	game->match.ball.vel.x = 0;
	game->match.ball.vel.y = 0;

	game->winner = 0;

	update_match(game);
}

/* New game */
void new_game(game_t *game) {
	game->winner = 1;

	game->s1 = 0;
	game->s2 = 0;

	game->match.ball.rect.w = BALL_RADIUS*2;
	game->match.ball.rect.h = BALL_RADIUS*2;
	game->match.ball.acc.y  = BALL_GRAVITY;
	game->match.ball.acc.x  = 0;

	game->match.p1.ent.rect.w = 2*PLAYER_RADIUS;
	game->match.p1.ent.rect.h = PLAYER_RADIUS;
	game->match.p1.ent.acc.y  = PLAYER_GRAVITY;

	game->match.p2.ent.rect.w = 2*PLAYER_RADIUS;
	game->match.p2.ent.rect.h = PLAYER_RADIUS;
	game->match.p2.ent.acc.y  = PLAYER_GRAVITY;

	new_match(game);
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

static SDL_Surface *create_slime(Uint32 color, bool left) {
	SDL_Surface *slime = create_circle_sprite(PLAYER_RADIUS, PLAYER_RADIUS, color);
	if(slime) {
		fill_circle(slime, left ? 2 * PLAYER_RADIUS - PLAYER_EYE_X : PLAYER_EYE_X,
				PLAYER_EYE_Y, PLAYER_EYE_RADIUS, 0xFFFFFFFF);
		fill_circle(slime, left ? 2 * PLAYER_RADIUS - PLAYER_PUP_X : PLAYER_PUP_X,
				PLAYER_PUP_Y, PLAYER_PUP_RADIUS, 0x000000FF);
	}
	return slime;
}

static void draw_score(SDL_Surface *surface, const game_t *state, bool left) {
	int x0 = left ? POINT_PADDING : SCREEN_WIDTH - POINT_PADDING;
	int dx = left ? POINT_PADDING : -POINT_PADDING;
	int score = left ? state->s1 : state->s2;
	Uint32 color = left ? PLAYER1_COLOR : PLAYER2_COLOR;
	for(int i = 0; i < MAX_SCORE; i++) {
		int x = x0 + i * dx;
		if(i < score) {
			fill_circle(surface, x, POINT_PADDING, POINT_RADIUS, color);
		}
		outline_circle(surface, x, POINT_PADDING, POINT_RADIUS);
	}
}

static void free_resources(void) {
	SDL_FreeSurface(slime1);
	SDL_FreeSurface(slime2);
	SDL_FreeSurface(ball);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

static void update_input(player_t *player, const Uint8 *keys,
		SDL_Keycode left, SDL_Keycode right, SDL_Keycode jump) {
	player->ent.acc.x = 0;
	if(keys[SDL_GetScancodeFromKey(left)]) {
		player->ent.acc.x = -PLAYER_MOVE_ACC;
	} else if(keys[SDL_GetScancodeFromKey(right)]) {
		player->ent.acc.x = PLAYER_MOVE_ACC;
	}

	if(keys[SDL_GetScancodeFromKey(jump)] &&
			player->ent.pos.y > PLAY_HEIGHT - PLAYER_RADIUS - EPSILON) {
		player->ent.vel.y = -PLAYER_JUMP_VEL;
	}
}

static bool draw_frame(void) {
	SDL_FillRect(screen, NULL, map_color(screen, BACKGROUND_COLOR));
	SDL_FillRect(screen, &ground, map_color(screen, GROUND_COLOR));
	SDL_FillRect(screen, &net, map_color(screen, NET_COLOR));
	draw_score(screen, &game, true);
	draw_score(screen, &game, false);

	// SDL_BlitSurface can clip its destination rectangle.
	SDL_Rect p1 = game.match.p1.ent.rect;
	SDL_Rect p2 = game.match.p2.ent.rect;
	SDL_Rect ball_rect = game.match.ball.rect;
	SDL_BlitSurface(slime1, NULL, screen, &p1);
	SDL_BlitSurface(slime2, NULL, screen, &p2);
	SDL_BlitSurface(ball, NULL, screen, &ball_rect);
	return SDL_UpdateWindowSurface(window) == 0;
}

static double frame_time(void) {
	return SDL_GetPerformanceCounter() / clock_frequency;
}

static bool game_frame(void) {
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		if(event.type == SDL_QUIT) {
			return false;
		}
	}

	double now = frame_time();
	if(now < next_frame) {
		return true;
	}
	next_frame += FRAME_INTERVAL;
	// Drop the backlog after the window or browser tab has been suspended.
	if(next_frame <= now) {
		next_frame = now + FRAME_INTERVAL;
	}

	if(game.winner) {
		if(now < next_point) {
			return true;
		}
		if(game.s1 >= MAX_SCORE || game.s2 >= MAX_SCORE) {
			new_game(&game);
		} else {
			new_match(&game);
		}
	}

	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	update_input(&game.match.p1, keys, SDLK_a, SDLK_d, SDLK_w);
	update_input(&game.match.p2, keys, SDLK_LEFT, SDLK_RIGHT, SDLK_UP);
	update_match(&game);
	if(game.winner) {
		next_point = now + POINT_DELAY / 1000.0;
	}

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

	window = SDL_CreateWindow("Slime Volleyball",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	if(!window || !(screen = SDL_GetWindowSurface(window))) {
		fprintf(stderr, "Could not create the window: %s\n", SDL_GetError());
		free_resources();
		return EXIT_FAILURE;
	}

	slime1 = create_slime(PLAYER1_COLOR, true);
	slime2 = create_slime(PLAYER2_COLOR, false);
	ball = create_circle_sprite(BALL_RADIUS, 2 * BALL_RADIUS + 1, BALL_COLOR);
	if(!slime1 || !slime2 || !ball) {
		fprintf(stderr, "Could not create the sprites: %s\n", SDL_GetError());
		free_resources();
		return EXIT_FAILURE;
	}

	ground = (SDL_Rect){0, PLAY_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - PLAY_HEIGHT};
	net = (SDL_Rect){SCREEN_WIDTH / 2 - NET_WIDTH / 2,
			PLAY_HEIGHT - NET_HEIGHT, NET_WIDTH, NET_HEIGHT + NET_PADDING};
	net_full.pos = (vect_t){net.x, net.y};
	net_full.rect.w = NET_WIDTH;
	net_full.rect.h = NET_HEIGHT;

	new_game(&game);
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
