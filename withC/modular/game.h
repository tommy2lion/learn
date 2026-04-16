#ifndef GAME_H
#define GAME_H

/*
 * game — rules, state machine, and scoring
 *
 * Intentionally has NO raylib dependency: logic is fully testable
 * with a plain C build.  All rendering stays in the screen_* modules.
 */

#include "solver.h"

/* ── State machine ──────────────────────────────────────────────────────── */
typedef enum {
    GS_PLAYING,        /* normal gameplay                        */
    GS_FLASH_CORRECT,  /* win-flash animation, then new deal     */
    GS_CUSTOM_INPUT,   /* custom-number modal is open            */
} GameState;

/* ── Message classification ─────────────────────────────────────────────── */
typedef enum {
    MK_NONE = 0,
    MK_OK,
    MK_ERR,
    MK_INFO,
} MsgKind;

/* ── Card slot ───────────────────────────────────────────────────────────── */
#define GAME_MAX_CARDS   4
#define GAME_MAX_TOKENS 32

typedef struct {
    int value;   /* the number on the card             */
    int used;    /* 1 = already inserted in expression */
} CardSlot;

/* ── Expression token ────────────────────────────────────────────────────── */
typedef struct {
    char text[12];  /* numeric string or operator symbol */
    int  card_idx;  /* >=0 = card index;  -1 = operator */
} ExprToken;

/* ── Game context ────────────────────────────────────────────────────────── */
typedef struct {
    /* Cards */
    CardSlot  cards[GAME_MAX_CARDS];
    int       card_count;

    /* Expression being built */
    ExprToken tokens[GAME_MAX_TOKENS];
    int       tok_cnt;

    /* Scoring */
    int       solved;
    int       skipped;

    /* State machine */
    GameState state;
    float     state_timer;    /* countdown (seconds) for timed states */

    /* Feedback message */
    char      message[128];
    MsgKind   message_kind;
    float     message_timer;  /* seconds left to display message      */

    /* Hint */
    char      hint[256];
    int       hint_visible;

    /* Configuration */
    SolverConfig solver_cfg;
} GameCtx;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Lifecycle */
void game_init  (GameCtx *g);                        /* zero + first deal    */
void game_update(GameCtx *g, float dt);              /* timers, state trans  */

/* Deals */
void game_new_round(GameCtx *g);                     /* random deal          */
void game_set_cards(GameCtx *g, const int *nums,
                    int count);                      /* custom deal          */
void game_skip     (GameCtx *g);                     /* skip, skipped++      */

/* Expression building */
void game_push_card(GameCtx *g, int card_idx);       /* insert numeric token */
void game_push_op  (GameCtx *g, const char *op);     /* insert operator/paren*/
void game_pop_token(GameCtx *g);                     /* undo last token      */
void game_clear_expr(GameCtx *g);                    /* reset expression     */

/* Read expression as a string */
void game_build_expr_str(const GameCtx *g, char *out, int outsz);

/* Actions */
void game_check    (GameCtx *g);                     /* evaluate + score     */
void game_hint     (GameCtx *g);                     /* run solver, fill hint*/
void game_set_state(GameCtx *g, GameState s);

#endif /* GAME_H */
