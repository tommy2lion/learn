#include "screen_custom.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Shared layout helper ─────────────────────────────────────────────────── */
/* Returns the top-left corner of the modal panel. */
static void modal_origin(int cx, int cy, int *px, int *py) {
    *px = cx - CM_PANEL_W / 2;
    *py = cy - CM_PANEL_H / 2;
}

/* Returns the rect of field i relative to the modal. */
static Rectangle field_rect(int cx, int cy, int i) {
    int px, py;
    modal_origin(cx, cy, &px, &py);
    int fields_w = GAME_MAX_CARDS * CM_FIELD_W
                 + (GAME_MAX_CARDS - 1) * CM_FIELD_GAP;
    int fx0 = cx - fields_w / 2;
    int fy  = py + 88;
    return (Rectangle){
        (float)(fx0 + i * (CM_FIELD_W + CM_FIELD_GAP)),
        (float)fy,
        (float)CM_FIELD_W,
        (float)CM_FIELD_H
    };
}

/* Returns the OK / Cancel button rects */
static Rectangle ok_rect(int cx, int cy) {
    int px, py; modal_origin(cx, cy, &px, &py);
    int bgy = py + CM_PANEL_H - 60;
    return (Rectangle){(float)(cx - 110 - 10), (float)bgy, 110, 42};
}
static Rectangle cancel_rect(int cx, int cy) {
    int px, py; modal_origin(cx, cy, &px, &py);
    int bgy = py + CM_PANEL_H - 60;
    return (Rectangle){(float)(cx + 10), (float)bgy, 110, 42};
}

/* ── Confirm action ───────────────────────────────────────────────────────── */
static void do_confirm(CustomInputCtx *ctx, GameCtx *g) {
    int vals[GAME_MAX_CARDS];
    for (int i = 0; i < GAME_MAX_CARDS; i++) {
        if (ctx->fields[i].buf[0] == '\0') {
            snprintf(ctx->error, sizeof(ctx->error),
                     "Card %d is empty.", i + 1);
            return;
        }
        int v = atoi(ctx->fields[i].buf);
        if (v < 1 || v > 999) {
            snprintf(ctx->error, sizeof(ctx->error),
                     "Card %d: enter a number between 1 and 999.", i + 1);
            return;
        }
        vals[i] = v;
    }
    /* game_set_cards also resets the expression and sets state to GS_PLAYING */
    game_set_cards(g, vals, GAME_MAX_CARDS);
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void screen_custom_open(CustomInputCtx *ctx, const GameCtx *g) {
    for (int i = 0; i < GAME_MAX_CARDS; i++) {
        snprintf(ctx->fields[i].buf, sizeof(ctx->fields[i].buf),
                 "%d", g->cards[i].value);
    }
    ctx->focused  = 0;
    ctx->error[0] = '\0';
}

void screen_custom_update(CustomInputCtx *ctx, GameCtx *g, int cx, int cy) {
    /* ── Keyboard input for focused field ── */
    int key = GetCharPressed();
    while (key > 0) {
        char ch = (char)key;
        if (ch >= '0' && ch <= '9') {
            /* Enforce max 3 digits (1–999) */
            if ((int)strlen(ctx->fields[ctx->focused].buf) < 3) {
                ui_textbox_append(&ctx->fields[ctx->focused], ch);
            }
            ctx->error[0] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        ui_textbox_backspace(&ctx->fields[ctx->focused]);
        ctx->error[0] = '\0';
    }
    if (IsKeyPressed(KEY_TAB))
        ctx->focused = (ctx->focused + 1) % GAME_MAX_CARDS;
    if (IsKeyPressed(KEY_ENTER))
        do_confirm(ctx, g);
    if (IsKeyPressed(KEY_ESCAPE))
        g->state = GS_PLAYING;

    /* ── Mouse: click field to focus ── */
    for (int i = 0; i < GAME_MAX_CARDS; i++) {
        Rectangle fr = field_rect(cx, cy, i);
        if (CheckCollisionPointRec(GetMousePosition(), fr)
            && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            ctx->focused = i;
        }
    }

    /* ── OK / Cancel buttons ── */
    UiButton ok  = {ok_rect    (cx, cy), "OK",     COL_GREEN, COL_DARK };
    UiButton can = {cancel_rect(cx, cy), "Cancel", COL_GRAY,  COL_WHITE};

    if (ui_button_clicked(&ok))  do_confirm(ctx, g);
    if (ui_button_clicked(&can)) g->state = GS_PLAYING;
}

void screen_custom_draw(const CustomInputCtx *ctx, int cx, int cy,
                        float anim) {
    /* Dim the game board underneath */
    ui_draw_overlay();

    /* Panel */
    int px, py;
    modal_origin(cx, cy, &px, &py);
    Rectangle panel = {(float)px, (float)py, CM_PANEL_W, CM_PANEL_H};
    DrawRectangleRounded(panel, 0.08f, 10, COL_PANEL2);
    DrawRectangleRoundedLinesEx(panel, 0.08f, 10, 2.0f, COL_CYAN);

    /* Title & subtitle */
    ui_draw_text_centred("Custom Numbers",
                         cx, py + 18, 26, COL_GOLD);
    ui_draw_text_centred("Type any 4 numbers (1 – 999), then press OK or Enter",
                         cx, py + 52, 14, (Color){170, 170, 200, 255});

    /* Input fields */
    for (int i = 0; i < GAME_MAX_CARDS; i++) {
        Rectangle fr = field_rect(cx, cy, i);

        /* Label above each box */
        char lbl[10];
        snprintf(lbl, sizeof(lbl), "Card %d", i + 1);
        ui_draw_text_centred(lbl,
                             (int)(fr.x + fr.width / 2),
                             (int)(fr.y - 20), 13,
                             (Color){160, 160, 190, 255});

        ui_textbox_draw(&ctx->fields[i], fr, (i == ctx->focused), anim);
    }

    /* Keyboard hint */
    ui_draw_text_centred("Tab / click to switch field",
                         cx,
                         (int)(field_rect(cx,cy,0).y + CM_FIELD_H + 10),
                         13, (Color){100, 100, 130, 255});

    /* Validation error */
    if (ctx->error[0]) {
        ui_draw_text_centred(ctx->error,
                             cx,
                             (int)(field_rect(cx,cy,0).y + CM_FIELD_H + 32),
                             15, COL_RED);
    }

    /* OK / Cancel */
    UiButton ok  = {ok_rect    (cx, cy), "OK",     COL_GREEN, COL_DARK };
    UiButton can = {cancel_rect(cx, cy), "Cancel", COL_GRAY,  COL_WHITE};
    ui_button_draw(&ok,  ui_button_hovered(&ok));
    ui_button_draw(&can, ui_button_hovered(&can));
}
