#ifndef SCREEN_GAME_H
#define SCREEN_GAME_H

/*
 * screen_game — main playing screen
 *
 * Handles layout, card/button rendering, and keyboard/mouse event routing.
 * Contains NO game logic — all rule calls go through game_* functions.
 *
 * The data-driven action-button table (ActionDef) is private to screen_game.c.
 */

#include "game.h"
#include "screen_custom.h"

void screen_game_draw  (const GameCtx *g, float anim);
void screen_game_update(GameCtx *g, CustomInputCtx *ci, float dt);

#endif /* SCREEN_GAME_H */
