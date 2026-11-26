#include <stdio.h>

#include "app.h"
#include "ui.h"

// CSS colors and wording from the pinned OneSlime.min.js snapshot.
static const palette_t palettes[BOT_TIERS] = {
	{0x0000FFFF, 0x808080FF, 0xFFFF00FF, 0xFFFFFFFF},
	{0x1E5000FF, 0xA9A9A9FF, 0x8080FFFF, 0xFF0000FF},
	{0x623939FF, 0x00A800FF, 0xFFFFFFFF, 0x000000FF},
};

static const char *const loss_lines[BOT_TIERS][2] = {
	{"You are a loser!", "Better luck next time."},
	{"Angry Red Slimons gives you the gong!", "So who has the red face bombing out on level 2, huh?"},
	{"The Slime Master says \"You are seriously inept.\"", "Congrats on reaching level 3."},
};

const palette_t *palette(const game_t *state) {
	return &palettes[state->solo ? state->bot_tier : 0];
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

typedef struct {
	size_t length;
	const char *next;
} text_line_t;

static text_line_t text_line(const char *text, int max_width) {
	size_t capacity = max_width >= TEXT_CELL_WIDTH - 2 ? (max_width + 2) / TEXT_CELL_WIDTH : 1;
	size_t length = 0, last_space = 0;
	while(text[length] && text[length] != '\n' && length < capacity) {
		if(text[length] == ' ') last_space = length;
		length++;
	}
	if(length == capacity && text[length] && text[length] != '\n'
			&& text[length] != ' ' && last_space > 0) length = last_space;
	const char *next = text + length;
	while(*next == ' ') next++;
	if(*next == '\n') next++;
	while(length > 0 && text[length - 1] == ' ') length--;
	return (text_line_t){length, next};
}

int text_width(const char *text) {
	int width = 0, length = 0;
	for(;; text++) {
		if(*text == '\n' || *text == '\0') {
			int line_width = length > 0 ? length * TEXT_CELL_WIDTH - 2 : 0;
			if(line_width > width) width = line_width;
			length = 0;
			if(!*text) return width;
		} else length++;
	}
}

int draw_text(SDL_Surface *screen, const char *text, int center, int baseline, int max_width, Uint32 rgba) {
	Uint32 color = map_color(screen, rgba);
	int lines = 0;
	while(*text) {
		text_line_t line = text_line(text, max_width);
		int width = line.length > 0 ? (int)line.length * TEXT_CELL_WIDTH - 2 : 0;
		int x = center - width / 2;
		for(size_t i = 0; i < line.length; i++, x += TEXT_CELL_WIDTH) {
			unsigned char code = (unsigned char)text[i];
			if(code < 32 || code > 126) code = '?';
			const Uint8 *rows = glyphs[code - 32];
			for(int row = 0; row < 7; row++) {
				for(int col = 0; col < 5; col++) {
					if(rows[row] & (16 >> col)) {
						// Always render the original 2x2 pixels on both axes.
						SDL_Rect pixel = {x + col * 2, baseline - TEXT_HEIGHT + row * 2, 2, 2};
						SDL_FillRect(screen, &pixel, color);
					}
				}
			}
		}
		lines++;
		baseline += TEXT_LINE_HEIGHT;
		text = line.next;
	}
	return lines;
}

static void centered_text(SDL_Surface *screen, const char *text, int baseline) {
	draw_text(screen, text, SCREEN_WIDTH / 2, baseline, SCREEN_WIDTH - 30, NET_COLOR);
}

SDL_Rect menu_button(bool solo) {
	return (SDL_Rect){solo ? 10 : SCREEN_WIDTH / 2 + 10, 210, SCREEN_WIDTH / 2 - 20, 74};
}

void draw_menu(SDL_Surface *screen, const game_t *state) {
	centered_text(screen, "Slime Volleyball: One Slime", 106);
	centered_text(screen, "Written by Quin Pendragon and Daniel Wedge", 153);
	centered_text(screen, "http://oneslime.net/", 181);
	for(int side = 0; side < 2; side++) {
		SDL_Rect rect = menu_button(side == 0);
		SDL_FillRect(screen, &rect, map_color(screen, NET_COLOR));
		bool continuing = state->bot_tier > 0 && side == 0;
		char label[128];
		snprintf(label, sizeof(label), "Click here to start a\n%s player game!%s",
			side == 0 ? "one" : "two", continuing ? "\nor press C to continue..." : "");
		int height = TEXT_HEIGHT + (continuing ? 2 : 1) * TEXT_LINE_HEIGHT;
		int baseline = rect.y + (rect.h - height) / 2 + TEXT_HEIGHT;
		draw_text(screen, label, rect.x + rect.w / 2, baseline, rect.w - 16, palette(state)->sky);
	}
}

void draw_result(SDL_Surface *screen, const game_t *state) {
	char text[128];
	if(state->solo && state->server == 0) {
		if(state->bot_tier == BOT_TIERS - 1) {
			centered_text(screen, "YOU WIN!", 181);
			centered_text(screen, "The Slimes bow down before the new Slime King!", 204);
		} else {
			snprintf(text, sizeof(text), "Level %d clear!", state->bot_tier + 1);
			centered_text(screen, text, 125);
			snprintf(text, sizeof(text), "Your score: %d", state->campaign_score);
			centered_text(screen, text, 169);
			snprintf(text, sizeof(text), "Level bonus: %d points", state->level_bonus);
			centered_text(screen, text, 207);
			snprintf(text, sizeof(text), "Time bonus: %d points", state->time_bonus);
			centered_text(screen, text, 226);
			if(state->flawless_bonus) {
				snprintf(text, sizeof(text), "Flawless Victory: %d points", state->flawless_bonus);
				centered_text(screen, text, 245);
			}
		}
	} else {
		SDL_Rect panel = {15, 87, SCREEN_WIDTH - 30, 140};
		SDL_FillRect(screen, &panel, map_color(screen, palette(state)->ground));
		if(state->solo) {
			centered_text(screen, loss_lines[state->bot_tier][0], 131);
			centered_text(screen, loss_lines[state->bot_tier][1], 150);
		} else centered_text(screen, state->local_result, 131);
		centered_text(screen, "GAME OVER", 207);
	}
	centered_text(screen, "Click the mouse to continue...", 329);
}

void draw_hud(SDL_Surface *screen, const game_t *state) {
	char text[128], time[32];
	int seconds = (int)state->elapsed;
	if(seconds >= 3600) snprintf(time, sizeof(time), "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else snprintf(time, sizeof(time), "%d:%02d", seconds / 60, seconds % 60);
	bool running = state->phase != TITLE && state->phase != RESULT;
	if(state->solo) {
		if(running) {
			snprintf(text, sizeof(text), "Score: %d\nTime: %s", state->campaign_score, time);
		} else snprintf(text, sizeof(text), "Score: %d", state->campaign_score);
	} else snprintf(text, sizeof(text), running ? "Time: %s" : "", time);
	int width = text_width(text);
	SDL_Rect backing = {(SCREEN_WIDTH - width - 20) / 2, 0, width + 20,
		HUD_SCORE_BASELINE + TEXT_LINE_HEIGHT + 4};
	SDL_FillRect(screen, &backing, map_color(screen, palette(state)->sky));
	int baseline = state->solo ? HUD_SCORE_BASELINE : HUD_SCORE_BASELINE + TEXT_LINE_HEIGHT;
	draw_text(screen, text, SCREEN_WIDTH / 2, baseline, HUD_WIDTH, NET_COLOR);
	const char *message = state->paused ? "Press P to continue . . ."
		: state->phase == REPLAY ? "Press space to continue..." : state->message;
	if(state->phase != RESULT) centered_text(screen, message, 329);
}

