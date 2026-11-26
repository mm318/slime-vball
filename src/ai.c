#include <math.h>

#include "game.h"

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

controls_t bot_input(const game_t *state) {
	const body_t *player = &state->players[1];
	const body_t *volley = &state->ball;
	float aim = 22.0f - 4.0f * state->bot_tier;
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
