#ifndef SLIME_UI_H
#define SLIME_UI_H

#include <SDL.h>
#include "game.h"

typedef struct { Uint32 sky, ground, ball, opponent; } palette_t;
enum { UI_NONE, UI_CONTINUE, UI_SOLO, UI_LOCAL, UI_REPLAY, UI_PAUSE };
const palette_t *palette(const game_t *state);
int text_width(const char *text);
int draw_text(SDL_Surface *screen, const char *text, int center, int baseline, int max_width, Uint32 rgba);
bool menu_visible(const game_t *state);
SDL_Rect ui_button_rect(int button);
bool ui_button_enabled(const game_t *state, int button);
void draw_menu(SDL_Surface *screen, const game_t *state);
void draw_result(SDL_Surface *screen, const game_t *state);
void draw_hud(SDL_Surface *screen, const game_t *state);
void draw_toolbar(SDL_Surface *screen, const game_t *state);

#endif
