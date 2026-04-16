#include "game.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define GAME_EPS 1e-9

/* ── Internal helpers ─────────────────────────────────────────────────────── */

static void set_message(GameCtx *g, const char *txt, MsgKind kind) {
    strncpy(g->message, txt, sizeof(g->message) - 1);
    g->message[sizeof(g->message) - 1] = '\0';
    g->message_kind  = kind;
    g->message_timer = 3.0f;
}

static void reset_expr(GameCtx *g) {
    g->tok_cnt = 0;
    for (int i = 0; i < g->card_count; i++) g->cards[i].used = 0;
}

/*
 * Sort-and-compare two integer arrays of length n.
 * Uses insertion sort on local copies to avoid modifying originals.
 */
static int int_arrays_equal(const int *a, const int *b, int n) {
    int sa[8] = {0}, sb[8] = {0};
    if (n > 8) n = 8;
    memcpy(sa, a, (size_t)n * sizeof(int));
    memcpy(sb, b, (size_t)n * sizeof(int));
    for (int i = 1; i < n; i++) {
        int t, j;
        t = sa[i]; for (j=i; j>0 && sa[j-1]>t; j--) sa[j]=sa[j-1]; sa[j]=t;
        t = sb[i]; for (j=i; j>0 && sb[j-1]>t; j--) sb[j]=sb[j-1]; sb[j]=t;
    }
    for (int i = 0; i < n; i++) if (sa[i] != sb[i]) return 0;
    return 1;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

void game_init(GameCtx *g) {
    memset(g, 0, sizeof(*g));
    g->solver_cfg.target    = 24;
    g->solver_cfg.num_count = GAME_MAX_CARDS;
    g->state = GS_PLAYING;
    game_new_round(g);
}

void game_update(GameCtx *g, float dt) {
    if (g->message_timer > 0.0f) g->message_timer -= dt;

    if (g->state == GS_FLASH_CORRECT) {
        g->state_timer -= dt;
        if (g->state_timer <= 0.0f) {
            g->state_timer = 0.0f;
            g->state       = GS_PLAYING;
            game_new_round(g);
        }
    }
}

/* ── Deals ────────────────────────────────────────────────────────────────── */

void game_new_round(GameCtx *g) {
    g->card_count = GAME_MAX_CARDS;
    for (int i = 0; i < g->card_count; i++) {
        g->cards[i].value = (rand() % 13) + 1;
        g->cards[i].used  = 0;
    }
    g->tok_cnt       = 0;
    g->message[0]    = '\0';
    g->message_kind  = MK_NONE;
    g->message_timer = 0.0f;
    g->hint[0]       = '\0';
    g->hint_visible  = 0;
}

void game_set_cards(GameCtx *g, const int *nums, int count) {
    g->card_count = (count > GAME_MAX_CARDS) ? GAME_MAX_CARDS : count;
    for (int i = 0; i < g->card_count; i++) {
        g->cards[i].value = nums[i];
        g->cards[i].used  = 0;
    }
    reset_expr(g);
    g->message[0]    = '\0';
    g->message_kind  = MK_NONE;
    g->message_timer = 0.0f;
    g->hint[0]       = '\0';
    g->hint_visible  = 0;
    g->state         = GS_PLAYING;
}

void game_skip(GameCtx *g) {
    g->skipped++;
    game_new_round(g);
}

/* ── Expression building ──────────────────────────────────────────────────── */

void game_push_card(GameCtx *g, int card_idx) {
    if (card_idx < 0 || card_idx >= g->card_count) return;
    if (g->cards[card_idx].used)       return;
    if (g->tok_cnt >= GAME_MAX_TOKENS) return;

    g->cards[card_idx].used = 1;

    /* Always store the numeric value, not the face label, so the parser works */
    char num_str[12];
    snprintf(num_str, sizeof(num_str), "%d", g->cards[card_idx].value);
    snprintf(g->tokens[g->tok_cnt].text,
             sizeof(g->tokens[0].text), "%s", num_str);
    g->tokens[g->tok_cnt].card_idx = card_idx;
    g->tok_cnt++;

    g->message[0]   = '\0';
    g->hint_visible = 0;
}

void game_push_op(GameCtx *g, const char *op) {
    if (g->tok_cnt >= GAME_MAX_TOKENS) return;
    strncpy(g->tokens[g->tok_cnt].text, op,
            sizeof(g->tokens[0].text) - 1);
    g->tokens[g->tok_cnt].card_idx = -1;
    g->tok_cnt++;
    g->message[0] = '\0';
}

void game_pop_token(GameCtx *g) {
    if (!g->tok_cnt) return;
    g->tok_cnt--;
    int ci = g->tokens[g->tok_cnt].card_idx;
    if (ci >= 0 && ci < g->card_count) g->cards[ci].used = 0;
    g->message[0] = '\0';
}

void game_clear_expr(GameCtx *g) {
    reset_expr(g);
    g->message[0] = '\0';
}

void game_build_expr_str(const GameCtx *g, char *out, int outsz) {
    out[0] = '\0';
    for (int i = 0; i < g->tok_cnt; i++) {
        if (i) strncat(out, " ", (size_t)(outsz - (int)strlen(out) - 1));
        strncat(out, g->tokens[i].text,
                (size_t)(outsz - (int)strlen(out) - 1));
    }
}

/* ── Actions ──────────────────────────────────────────────────────────────── */

void game_check(GameCtx *g) {
    if (!g->tok_cnt) {
        set_message(g, "Build an expression first!", MK_ERR);
        return;
    }

    char expr[256];
    game_build_expr_str(g, expr, sizeof(expr));

    ParseResult pr = parser_eval(expr);
    if (pr.error || pr.value > PARSER_INF / 2.0) {
        set_message(g, "Invalid expression!", MK_ERR);
        return;
    }

    if (pr.num_count != g->card_count) {
        set_message(g, "Use all 4 numbers exactly once!", MK_ERR);
        return;
    }

    int card_vals[GAME_MAX_CARDS];
    for (int i = 0; i < g->card_count; i++) card_vals[i] = g->cards[i].value;

    if (!int_arrays_equal(card_vals, pr.nums, g->card_count)) {
        set_message(g, "Use all 4 numbers exactly once!", MK_ERR);
        return;
    }

    if (fabs(pr.value - (double)g->solver_cfg.target) < GAME_EPS) {
        char msg[64];
        snprintf(msg, sizeof(msg), "CORRECT!  = %d", g->solver_cfg.target);
        set_message(g, msg, MK_OK);
        g->solved++;
        g->state       = GS_FLASH_CORRECT;
        g->state_timer = 1.2f;
        g->hint_visible = 0;
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "= %.5g  — not %d. Try again!",
                 pr.value, g->solver_cfg.target);
        set_message(g, msg, MK_ERR);
    }
}

void game_hint(GameCtx *g) {
    int nums[GAME_MAX_CARDS];
    for (int i = 0; i < g->card_count; i++) nums[i] = g->cards[i].value;

    char sol[200];
    if (solver_find(&g->solver_cfg, nums, sol, sizeof(sol))) {
        snprintf(g->hint, sizeof(g->hint),
                 "Hint: %s = %d", sol, g->solver_cfg.target);
        g->hint_visible = 1;
    } else {
        snprintf(g->hint, sizeof(g->hint), "No solution — skipping!");
        g->hint_visible = 1;
        g->skipped++;
        g->state       = GS_FLASH_CORRECT;
        g->state_timer = 0.02f;  /* immediately triggers new_round via update */
    }
}

void game_set_state(GameCtx *g, GameState s) {
    g->state = s;
}
