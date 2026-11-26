#include "game.h"

static positions_t positions(const game_t *state) {
	return (positions_t){{state->players[0].pos, state->players[1].pos}, state->ball.pos};
}

void record_frame(game_t *state) {
	state->history[state->history_next] = positions(state);
	state->history_next = (state->history_next + 1) % REPLAY_FRAMES;
	if(state->history_count < REPLAY_FRAMES) {
		state->history_count++;
	}
}

void press_replay(game_t *state, double now) {
	if(state->paused) {
		return;
	}
	if(state->phase == REPLAY) {
		finish_point(state);
	} else if(state->phase == POINT_BREAK && now < state->point_deadline) {
		state->replay_requested = true;
	}
}

positions_t display_positions(const game_t *state) {
	if(state->phase != REPLAY) {
		return positions(state);
	}
	int oldest = (state->history_next + REPLAY_FRAMES - state->history_count) % REPLAY_FRAMES;
	int index = state->replay_index < 0 ? 0 : state->replay_index;
	return state->history[(oldest + index) % REPLAY_FRAMES];
}

void advance_replay(game_t *state) {
	if(++state->replay_index == state->history_count) {
		state->replay_index = 0;
	}
}
