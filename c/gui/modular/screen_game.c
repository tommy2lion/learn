#include "screen_game.h"
#include "card.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

/* ── Layout constants ─────────────────────────────────────────────────────── */
#define SG_CARD_W      120
#define SG_CARD_H      170
#define SG_CARD_GAP     18
#define SG_CARDS_Y      80
#define OP_W         66
#define OP_H         52
#define OP_GAP       10
#define ACT_W       100
#define ACT_H        46
#define ACT_GAP      10

/* ── Data-driven action buttons (Strategy + Component patterns) ───────────── */

/*
 * ActionFn: a callback that receives the game context and the custom-input
 * context. Most actions only need g; the CUSTOM action also needs ci.
 */
typedef void (*ActionFn)(GameCtx *g, CustomInputCtx *ci);

typedef struct {
    const char *label;
    Color       bg;   /* plain struct init — avoids static compound literal */
    Color       fg;
    ActionFn    on_click;
} ActionDef;

static void act_back  (GameCtx *g, CustomInputCtx *ci) { (void)ci; game_pop_token(g);   }
static void act_clear (GameCtx *g, CustomInputCtx *ci) { (void)ci; game_clear_expr(g);  }
static void act_check (GameCtx *g, CustomInputCtx *ci) { (void)ci; game_check(g);       }
static void act_hint  (GameCtx *g, CustomInputCtx *ci) { (void)ci; game_hint(g);        }
static void act_skip  (GameCtx *g, CustomInputCtx *ci) { (void)ci; game_skip(g);        }
static void act_custom(GameCtx *g, CustomInputCtx *ci) {
    screen_custom_open(ci, g);
    game_set_state(g, GS_CUSTOM_INPUT);
}

/*
 * Color values written as plain struct initializers so the array can have
 * automatic storage duration (compound literals cannot appear in static
 * initializers in C99).
 */
#define ACTIONS_N 6
static const ActionDef ACTIONS[ACTIONS_N] = {
    { "BACK",   {85,  85, 85,255}, {255,255,255,255}, act_back   },
    { "CLEAR",  {231, 76, 60,255}, {255,255,255,255}, act_clear  },
    { "CHECK",  {245,197, 24,255}, { 26, 26, 46,255}, act_check  },
    { "HINT",   {155, 89,182,255}, {255,255,255,255}, act_hint   },
    { "NEW",    { 46,204,113,255}, { 26, 26, 46,255}, act_skip   },
    { "CUSTOM", { 52,152,219,255}, { 26, 26, 46,255}, act_custom },
};

/* ── Derived layout ───────────────────────────────────────────────────────── */
static void layout(int *cards_x0, int *op_x0, int *op_y,
                   int *expr_y,   int *act_x0, int *act_y) {
    int cx = SCREEN_W / 2;
    int n_ops = 6;
    int n_acts = ACTIONS_N;

    *cards_x0 = cx - (GAME_MAX_CARDS * SG_CARD_W + (GAME_MAX_CARDS-1) * SG_CARD_GAP) / 2;
    *op_y     = SG_CARDS_Y + SG_CARD_H + 28;
    *op_x0    = cx - (n_ops * OP_W + (n_ops-1) * OP_GAP) / 2;
    *expr_y   = *op_y + OP_H + 16;
    *act_y    = *expr_y + 52 + 16;    /* 52 = expression box height */
    *act_x0   = cx - (n_acts * ACT_W + (n_acts-1) * ACT_GAP) / 2;
}

/* ── Draw ─────────────────────────────────────────────────────────────────── */

void screen_game_draw(const GameCtx *g, float anim) {
    int cx = SCREEN_W / 2;
    int cards_x0, op_x0, op_y, expr_y, act_x0, act_y;
    layout(&cards_x0, &op_x0, &op_y, &expr_y, &act_x0, &act_y);

    int msg_y  = act_y + ACT_H + 20;
    int hint_y = msg_y + 34;

    /* Title */
    ui_draw_text_centred("CALC  24", cx, 16, 42, COL_GOLD);

    /* Score */
    char score[64];
    snprintf(score, sizeof(score), "Solved: %d     Skipped: %d",
             g->solved, g->skipped);
    ui_draw_text_centred(score, cx, 64, 18, (Color){170, 170, 170, 255});

    /* Cards */
    float flash_t = (g->state == GS_FLASH_CORRECT) ? g->state_timer : 0.0f;
    for (int i = 0; i < g->card_count; i++) {
        Rectangle cr = {
            (float)(cards_x0 + i * (SG_CARD_W + SG_CARD_GAP)),
            (float)SG_CARDS_Y,
            (float)SG_CARD_W, (float)SG_CARD_H
        };
        int hovered = (g->state == GS_PLAYING)
                   && !g->cards[i].used
                   && CheckCollisionPointRec(GetMousePosition(), cr);

        card_draw(g->cards[i].value, cr,
                  g->cards[i].used, hovered, flash_t, i);
    }

    /* Operator buttons */
    static const char *op_labels[] = {"+","-","*","/","(",")"};
    int n_ops = 6;
    int interactive = (g->state == GS_PLAYING);
    for (int i = 0; i < n_ops; i++) {
        UiButton b = {
            {(float)(op_x0 + i*(OP_W+OP_GAP)), (float)op_y,
             (float)OP_W,  (float)OP_H},
            op_labels[i], COL_PANEL, COL_GOLD
        };
        ui_button_draw(&b, interactive && ui_button_hovered(&b));
    }

    /* Expression display box */
    Rectangle expr_rect = {(float)(cx-320), (float)expr_y, 640, 52};
    Color expr_border = COL_BORDER, expr_col = COL_GOLD;
    if (g->message_kind == MK_OK  && g->message_timer > 0)
        { expr_border = COL_GREEN; expr_col = COL_GREEN; }
    if (g->message_kind == MK_ERR && g->message_timer > 0)
        { expr_border = COL_RED;   expr_col = COL_RED;   }

    DrawRectangleRounded(expr_rect, 0.12f, 8, COL_PANEL);
    DrawRectangleRoundedLinesEx(expr_rect, 0.12f, 8, 2.0f, expr_border);

    char expr_str[256];
    game_build_expr_str(g, expr_str, sizeof(expr_str));

    char disp[264];
    int show_cursor = interactive && ((int)(anim * 2) % 2);
    if (g->tok_cnt)
        snprintf(disp, sizeof(disp), "%s%s", expr_str, show_cursor ? " |" : "");
    else
        snprintf(disp, sizeof(disp), "_");

    int dw = MeasureText(disp, 22);
    int max_w = (int)expr_rect.width - 20;
    if (dw > max_w) {
        /* Scroll right-aligned for long expressions */
        BeginScissorMode((int)expr_rect.x + 8, (int)expr_rect.y,
                         (int)expr_rect.width - 16, (int)expr_rect.height);
        DrawText(disp,
                 (int)(expr_rect.x + expr_rect.width - 10 - dw),
                 (int)(expr_rect.y + (expr_rect.height - 22) / 2),
                 22, expr_col);
        EndScissorMode();
    } else {
        DrawText(disp,
                 (int)(expr_rect.x + 10),
                 (int)(expr_rect.y + (expr_rect.height - 22) / 2),
                 22, expr_col);
    }

    /* Action buttons */
    for (int i = 0; i < ACTIONS_N; i++) {
        UiButton b = {
            {(float)(act_x0 + i*(ACT_W+ACT_GAP)), (float)act_y,
             (float)ACT_W, (float)ACT_H},
            ACTIONS[i].label, ACTIONS[i].bg, ACTIONS[i].fg
        };
        ui_button_draw(&b, interactive && ui_button_hovered(&b));
    }

    /* Feedback message */
    if (g->message_timer > 0 && g->message[0]) {
        Color mc = (g->message_kind == MK_OK)  ? COL_GREEN
                 : (g->message_kind == MK_ERR) ? COL_RED
                 : COL_GOLD;
        ui_draw_text_centred(g->message, cx, msg_y, 22, mc);
    }

    /* Hint */
    if (g->hint_visible && g->hint[0]) {
        int hw = MeasureText(g->hint, 18);
        int hx = cx - hw / 2;
        DrawRectangleRounded(
            (Rectangle){(float)(hx-12), (float)(hint_y-8),
                        (float)(hw+24), 34},
            0.3f, 6, COL_PANEL);
        DrawText(g->hint, hx, hint_y, 18, (Color){200, 200, 200, 255});
    }

    /* Footer */
    ui_draw_text_centred(
        "Click cards or type  |  + - * / ( )  |"
        "  Enter=Check  Esc=Clear  Backspace=Undo  |  CUSTOM=set your numbers",
        cx, SCREEN_H - 24, 12, (Color){75, 75, 100, 255});
}

/* ── Update ───────────────────────────────────────────────────────────────── */

void screen_game_update(GameCtx *g, CustomInputCtx *ci, float dt) {
    (void)dt;

    /* No player interaction during flash animation */
    if (g->state != GS_PLAYING) return;

    int cards_x0, op_x0, op_y, expr_y, act_x0, act_y;
    layout(&cards_x0, &op_x0, &op_y, &expr_y, &act_x0, &act_y);

    /* Keyboard: operators and control keys */
    int key = GetCharPressed();
    while (key > 0) {
        char ch = (char)key;
        if (ch=='+' || ch=='-' || ch=='*' || ch=='/' ||
            ch=='(' || ch==')') {
            char s[3] = {ch, '\0'};
            game_push_op(g, s);
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) game_pop_token(g);
    if (IsKeyPressed(KEY_ENTER))     game_check(g);
    if (IsKeyPressed(KEY_ESCAPE))    game_clear_expr(g);

    /* Card clicks */
    for (int i = 0; i < g->card_count; i++) {
        Rectangle cr = {
            (float)(cards_x0 + i * (SG_CARD_W + SG_CARD_GAP)),
            (float)SG_CARDS_Y, (float)SG_CARD_W, (float)SG_CARD_H
        };
        if (!g->cards[i].used
            && CheckCollisionPointRec(GetMousePosition(), cr)
            && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            game_push_card(g, i);
        }
    }

    /* Operator button clicks */
    static const char *op_labels[] = {"+","-","*","/","(",")"};
    int n_ops = 6;
    for (int i = 0; i < n_ops; i++) {
        UiButton b = {
            {(float)(op_x0 + i*(OP_W+OP_GAP)), (float)op_y,
             (float)OP_W, (float)OP_H},
            op_labels[i], COL_PANEL, COL_GOLD
        };
        if (ui_button_clicked(&b)) game_push_op(g, op_labels[i]);
    }

    /* Action button clicks — dispatch through the data-driven table */
    for (int i = 0; i < ACTIONS_N; i++) {
        UiButton b = {
            {(float)(act_x0 + i*(ACT_W+ACT_GAP)), (float)act_y,
             (float)ACT_W, (float)ACT_H},
            ACTIONS[i].label, ACTIONS[i].bg, ACTIONS[i].fg
        };
        if (ui_button_clicked(&b)) {
            ACTIONS[i].on_click(g, ci);
            break;  /* handle at most one action per frame */
        }
    }
}
