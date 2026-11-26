#ifndef SLIME_UI_H
#define SLIME_UI_H

#include <SDL.h>
#include "game.h"

typedef struct { Uint32 sky, ground, ball, opponent; } palette_t;
const palette_t *palette(const game_t *state);
int text_width(const char *text);
int draw_text(SDL_Surface *screen, const char *text, int center, int baseline, int max_width, Uint32 rgba);
SDL_Rect menu_button(bool solo);
void draw_menu(SDL_Surface *screen, const game_t *state);
void draw_result(SDL_Surface *screen, const game_t *state);
void draw_hud(SDL_Surface *screen, const game_t *state);

#endif
