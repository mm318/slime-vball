#ifndef SLIME_APP_H
#define SLIME_APP_H

#include <SDL.h>
#include "game.h"

#define NET_COLOR 0xFFFFFFFFu

typedef struct {
	SDL_Window *window;
	SDL_Surface *screen, *slime1, *slime2, *ball;
	SDL_Rect ground, net;
} graphics_t;

// SDL drawing and movement primitives remain in main.c.
Uint32 map_color(SDL_Surface *surface, Uint32 rgba);
SDL_Surface *create_circle_sprite(int radius, int height, Uint32 color);
void render_game(graphics_t *graphics, const game_t *state);
void free_resources(graphics_t *graphics);
bool player_key(game_t *state, SDL_Keycode key, bool pressed);
// Mouse/touch menus and keyboard/gesture routing are implemented in input.zig.
bool handle_event(game_t *state, SDL_Window *window, const SDL_Event *event, double now);

#endif
