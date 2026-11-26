#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

static const char *const names[] = {
	"Inferior Human Controlled Slime ", "The Pathetic White Slime ",
	"Angry Red Slimons ", "The Slime Master ",
};

void start_match(game_t *state, bool solo) {
	state->solo = solo;
	state->bot_tier = 0;
	state->campaign_score = 0;
	reset_match(state);
}

bool match_over(const game_t *state) {
	return state->score[state->server] >= MAX_SCORE;
}

void finish_point(game_t *state) {
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

void continue_result(game_t *state) {
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

void horizontal_input(game_t *state, int side) {
	float start = side == 0 ? PLAYER_START_X : SCREEN_WIDTH - PLAYER_START_X;
	if(state->players[side].pos.x == start && state->ball.pos.x == start
			&& !state->touched[1 - side]) {
		state->serve_moved = true;
	}
}

void record_contact(game_t *state, int side) {
	if(state->serve_moved) {
		state->touched[side] = true;
		state->contacts[side]++;
		state->contacts[1 - side] = 0;
	}
}

void point_message(game_t *state) {
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
	char subject[96];
	if(state->solo) snprintf(subject, sizeof(subject), "%s", names[winner == 0 ? 0 : state->bot_tier + 1]);
	else snprintf(subject, sizeof(subject), "%s%d ", names[0], winner + 1);
	// Long commentary keeps the name and the action on separate lines.
	size_t length = strlen(subject);
	subject[--length] = '\0'; // Each subject ends with one separating space.
	bool wrap = (length + 1 + strlen(ending)) * TEXT_CELL_WIDTH - 2 > SCREEN_WIDTH - 30;
	snprintf(state->message, sizeof(state->message), "%s%s%s", subject, wrap ? "\n" : " ", ending);
}

void toggle_pause(game_t *state, double now) {
	if(state->phase == TITLE || state->phase == RESULT || state->phase == REPLAY) return;
	state->paused = !state->paused;
	if(state->paused) {
		state->paused_at = now;
	} else if(state->phase == POINT_BREAK) {
		state->point_deadline += now - state->paused_at;
	}
}

bool advance_game_phase(game_t *state, double now) {
	if(state->last_step >= 0 && !state->paused
			&& (state->phase == SERVE || state->phase == RALLY)) {
		state->elapsed += now - state->last_step;
	}
	state->last_step = now;
	if(state->phase == TITLE || state->phase == RESULT) return false;
	if(state->paused) {
		return false;
	}
	if(state->phase == REPLAY) {
		advance_replay(state);
		return false;
	}
	if(state->phase == POINT_BREAK) {
		if(now >= state->point_deadline) {
			if(state->replay_requested) {
				state->phase = REPLAY;
				state->replay_index = -1;
			} else finish_point(state);
		}
		return false;
	}
	return true;
}
