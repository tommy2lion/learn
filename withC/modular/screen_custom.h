#ifndef SCREEN_CUSTOM_H
#define SCREEN_CUSTOM_H

/*
 * screen_custom — modal dialog for entering custom card numbers
 *
 * Usage pattern:
 *   screen_custom_open(&ci, g->cards[*].value)  — called when CUSTOM clicked
 *   screen_custom_update(&ci, &g, cx, cy)        — called each frame while open
 *   screen_custom_draw(&ci, cx, cy, anim)        — drawn on top of game board
 *
 * When the user confirms, game_set_cards() is called internally and
 * g->state is returned to GS_PLAYING.
 * When the user cancels, g->state is returned to GS_PLAYING unchanged.
 */

#include "ui.h"
#include "game.h"

/* Modal layout constants (shared between draw and update) */
#define CM_PANEL_W   520
#define CM_PANEL_H   300
#define CM_FIELD_W    90
#define CM_FIELD_H    60
#define CM_FIELD_GAP  18

typedef struct {
    UiTextBox fields[GAME_MAX_CARDS];
    int       focused;          /* 0–3: which field has keyboard focus */
    char      error[80];        /* validation error message            */
} CustomInputCtx;

void screen_custom_open  (CustomInputCtx *ctx, const GameCtx *g);
void screen_custom_update(CustomInputCtx *ctx, GameCtx *g, int cx, int cy);
void screen_custom_draw  (const CustomInputCtx *ctx, int cx, int cy,
                          float anim);

#endif /* SCREEN_CUSTOM_H */
