#ifndef SLIME_GAME_H
#define SLIME_GAME_H

#include <stdbool.h>

enum {
	SCREEN_WIDTH = 750,
	SCREEN_HEIGHT = 375,
	WINDOW_WIDTH = (SCREEN_WIDTH * 3 + 1) / 2,
	WINDOW_HEIGHT = (SCREEN_HEIGHT * 3 + 1) / 2,
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
	POINT_CENTER_Y = 30,
	REPLAY_FRAMES = 240,
	BOT_TIERS = 3,
	TEXT_CELL_WIDTH = 12,
	TEXT_HEIGHT = 14,
	TEXT_LINE_HEIGHT = 20,
	HUD_WIDTH = 280,
	HUD_SCORE_BASELINE = POINT_CENTER_Y - (TEXT_LINE_HEIGHT - TEXT_HEIGHT) / 2,
};

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
	// A LEFT/RIGHT command occurred, even if a later STOP cancelled it.
	bool moved;
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
	// Reference controllers retain their serve state between rallies.
	int bot_serve_type;
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

// Physics and simulation remain implemented in main.c.
extern const double FRAME_INTERVAL, POINT_DELAY;
extern const float PLAYER_START_X, SLIME_CONTACT_RADIUS, BALL_MAX_VY, BALL_GROUND_Y;
void advance_ball(body_t *volley);
void bounce_off_walls(body_t *volley);
void reset_rally(game_t *state);
void reset_match(game_t *state);
void step_game(game_t *state, double now);

// Match progression, scoring, and pause.
void start_match(game_t *state, bool solo);
bool match_over(const game_t *state);
void finish_point(game_t *state);
void continue_result(game_t *state);
void horizontal_input(game_t *state, int side);
void record_contact(game_t *state, int side);
void point_message(game_t *state);
void toggle_pause(game_t *state, double now);
// Returns true when this frame should advance the rally simulation.
bool advance_game_phase(game_t *state, double now);

// Computer input and position-only replay.
controls_t bot_input(game_t *state);
void record_frame(game_t *state);
void press_replay(game_t *state, double now);
void advance_replay(game_t *state);
positions_t display_positions(const game_t *state);

#endif
