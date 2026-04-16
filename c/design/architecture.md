# Calc 24 GUI — Modular Refactoring Design

## 1. Motivation

`calc24_gui.c` is currently ~450 lines in a single translation unit. Every concern — solver math, expression parsing, card rendering, UI widgets, game rules, modal dialogs, and the main loop — lives side by side. This makes it hard to:

- Test the solver or parser in isolation
- Add a new game variant (e.g. target = 12, six numbers) without touching rendering code
- Swap the UI toolkit (raylib → SDL3) without rewriting game logic
- Extend the UI with new screens (settings, leaderboard) without growing `main()`

The refactoring goal is **separation of concerns**: each module owns one thing, exposes a stable interface, and hides its internals behind `static`.

---

## 2. Target File Structure

```
c/
├── terminal/
│   └── calc24.c            # terminal (character-mode) version
├── gui/
│   ├── calc24_gui.c        # monolithic raylib GUI (single-file reference)
│   └── modular/            # refactored architecture (lives inside gui/)
│       ├── main.c          # entry point, main loop, screen wiring
│       ├── solver.h / .c   # brute-force 24 solver
│       ├── parser.h / .c   # recursive-descent expression evaluator
│       ├── game.h  / .c    # game rules, state machine, scoring
│       ├── card.h  / .c    # card data, face-label helper, draw
│       ├── ui.h    / .c    # reusable UI primitives (Button, Modal, TextBox)
│       ├── screen_game.h/.c# main playing screen (layout + event routing)
│       └── screen_custom.h/.c  # custom-number modal screen
├── design/
│   └── architecture.md     # this file
└── Makefile
```

---

## 3. Module Responsibilities

### 3.1 `solver` — Pure Algorithm

**What it owns:** the combinatorial search that decides whether a set of numbers can reach a target.

**What it does NOT know:** raylib, game state, UI.

```c
/* solver.h */
typedef struct {
    int    target;        /* default 24 */
    int    num_count;     /* default 4  */
} SolverConfig;

/* Returns 1 if a solution exists; writes human-readable expr into out. */
int  solver_find(const SolverConfig *cfg, const int *nums,
                 char *out, int out_sz);
```

Keeping `target` and `num_count` as config data (not compile-time constants)
makes it trivial to support "make 12 with 3 numbers" without touching anything else.

---

### 3.2 `parser` — Expression Evaluation

**What it owns:** tokenising and evaluating arithmetic strings; verifying which numbers were consumed.

**Interface:**

```c
/* parser.h */
typedef struct {
    double  value;        /* result, or PARSER_INF on error */
    int     nums[8];      /* numbers found during parse     */
    int     num_count;
    int     error;        /* non-zero if syntax error        */
} ParseResult;

ParseResult parser_eval(const char *expr);
```

Returning a struct instead of using global state (`pp`, `pnum_cnt`, `perr`) eliminates the re-entrant hazard and makes unit testing straightforward.

---

### 3.3 `game` — Rules & State Machine

**What it owns:** game rules, the card set, the expression token list, scoring, and all transitions between game states.

This is the most important module for future expansion. It contains the **State Machine** pattern (see §4.2).

```c
/* game.h */
typedef enum {
    GS_PLAYING,           /* normal play                      */
    GS_FLASH_CORRECT,     /* brief win animation then new deal */
    GS_CUSTOM_INPUT,      /* custom-number modal is open       */
    GS_GAME_OVER          /* future: timed/competitive mode    */
} GameState;

typedef struct {
    int        cards[4];
    int        card_used[4];
    char       tokens[32][8]; /* text of each expression token  */
    int        token_card[32];/* card index (-1 = operator)     */
    int        tok_cnt;
    int        solved;
    int        skipped;
    GameState  state;
    float      state_timer;   /* time remaining in timed states */
    char       message[128];
    int        message_kind;  /* 0=none 1=ok 2=err 3=info       */
    float      message_timer;
    char       hint[160];
    int        hint_visible;
    SolverConfig solver_cfg;
} GameCtx;

void  game_init   (GameCtx *g);
void  game_new_round(GameCtx *g);      /* random deal            */
void  game_set_cards(GameCtx *g, const int *nums, int count);  /* custom */
void  game_push_token(GameCtx *g, const char *tok, int card_idx);
void  game_pop_token (GameCtx *g);
void  game_clear_expr(GameCtx *g);
void  game_check  (GameCtx *g);        /* evaluate & score       */
void  game_hint   (GameCtx *g);        /* run solver, fill hint  */
void  game_skip   (GameCtx *g);
void  game_update (GameCtx *g, float dt); /* advance timers/states */
```

All rendering code stays out. Screens read `GameCtx` and call these functions.

---

### 3.4 `card` — Card Display

**What it owns:** mapping a number to its display label and drawing a single card.

```c
/* card.h */
const char *card_face(int n);          /* 1→"A", 11→"J", 25→"25"   */

void card_draw(int n, Rectangle rect,  /* all drawing via raylib    */
               int used, int hovered, float flash_t);
```

Keeping drawing here (rather than inlining it in `screen_game.c`) means we can change card appearance — suits, colour, animation — in one place.

---

### 3.5 `ui` — Reusable Widgets

**What it owns:** stateless drawing primitives and the hit-test logic used by every screen.

This is the **Component pattern** (see §4.3).

```c
/* ui.h */

/* ── Button ── */
typedef struct {
    Rectangle rect;
    const char *label;
    Color bg, fg;
} UiButton;

void ui_button_draw   (const UiButton *b, int hovered);
int  ui_button_hovered(const UiButton *b);
int  ui_button_clicked(const UiButton *b);   /* pressed this frame? */

/* ── TextBox (single-line editable field) ── */
typedef struct {
    char   buf[16];
    int    focused;
    int    max_digits;
} UiTextBox;

void ui_textbox_draw        (const UiTextBox *tb, Rectangle rect, float anim);
void ui_textbox_handle_input(UiTextBox *tb);   /* call once per frame */

/* ── Modal overlay shell ── */
typedef struct {
    Rectangle panel;
    const char *title;
    const char *subtitle;
} UiModal;

void ui_modal_begin(const UiModal *m);  /* draws overlay + panel frame */
void ui_modal_end  (void);
```

Because `ui_button_clicked` is **stateless** (it just queries raylib), the same `UiButton` struct can be drawn and tested from any screen without ownership concerns.

---

### 3.6 `screen_game` — Main Playing Screen

**What it owns:** layout constants, assembling `UiButton`s, routing mouse/keyboard events to `GameCtx`, and calling `card_draw`.

It does **not** contain game logic — it calls `game_*` functions.

```c
/* screen_game.h */
void screen_game_draw  (GameCtx *g, float anim);
void screen_game_update(GameCtx *g, float dt);
```

---

### 3.7 `screen_custom` — Custom Number Modal

**What it owns:** the four `UiTextBox` fields, validation feedback, and the OK/Cancel buttons.

```c
/* screen_custom.h */
typedef struct {
    UiTextBox fields[4];
    char      error[80];
    int       focused;
} CustomInputCtx;

void screen_custom_open  (CustomInputCtx *ctx, const int *current_cards);
void screen_custom_draw  (CustomInputCtx *ctx, int cx, int cy, float anim);
void screen_custom_update(CustomInputCtx *ctx, GameCtx *g);
/* Returns 1 and fills g->cards if the user confirms valid input */
```

Moving the modal into its own screen module means adding future modals
(settings, leaderboard, difficulty picker) follows the exact same pattern.

---

### 3.8 `main` — Entry Point & Loop

After refactoring, `main.c` shrinks to roughly:

```c
int main(void) {
    srand((unsigned)time(NULL));
    InitWindow(SCREEN_W, SCREEN_H, "Calc 24 Game");
    SetTargetFPS(60);

    GameCtx       g   = {0};
    CustomInputCtx ci = {0};
    game_init(&g);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        game_update(&g, dt);

        if (g.state == GS_CUSTOM_INPUT)
            screen_custom_update(&ci, &g);
        else
            screen_game_update(&g, dt);

        BeginDrawing();
            ClearBackground(COL_BG);
            screen_game_draw(&g, anim);
            if (g.state == GS_CUSTOM_INPUT)
                screen_custom_draw(&ci, SCREEN_W/2, SCREEN_H/2, anim);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

`main` is now a **coordinator only** — it owns no logic.

---

## 4. Design Patterns

### 4.1 Module Pattern (C idiom)

Each `.c` file exposes only what is declared in its `.h`. All helpers are `static`. This is C's substitute for classes with private members.

```
solver.c      — static perms[], static gen_perms(), static eval_struct()
                public: solver_find()

parser.c      — static pp, pnum_cnt, perr, static pe_*()
                public: parser_eval()
```

**Why:** prevents name collisions, makes `grep`-based navigation unambiguous, and keeps compile times short (changes to `solver.c` internals don't force recompilation of `ui.c`).

---

### 4.2 State Machine (in `game`)

`GameState` makes all legal transitions explicit and prevents impossible combinations (e.g., the user cannot click cards while the win-flash is animating, because `screen_game_update` skips card hit-tests when `g.state != GS_PLAYING`).

```
              new_round()
    ┌─────────────────────────────────────────┐
    │                                         │
    ▼                                         │
GS_PLAYING ──check() correct──► GS_FLASH_CORRECT
    │                                         │
    │ CUSTOM button                 timer expires
    ▼                                         │
GS_CUSTOM_INPUT ──confirm──► GS_PLAYING ◄────┘
    │
    └── cancel ──► GS_PLAYING
```

**Why:** adding a new mode (timed challenge, two-player, tutorial) means adding a new enum value and a new arm in `game_update` — zero changes to existing arms.

---

### 4.3 Component Pattern (in `ui`)

`UiButton` and `UiTextBox` are plain structs. They carry no behaviour of their own — they are drawn by `ui_button_draw` and queried by `ui_button_clicked`. Screens compose them freely.

This is the C equivalent of React-style "dumb components": UI elements are data, behaviour is injected by the caller.

**Why:** adding a slider, checkbox, or dropdown later means adding one new struct + two new functions in `ui.h/c` without touching any existing screen.

---

### 4.4 Strategy Pattern (in `solver`)

The `SolverConfig` struct decouples the algorithm from its parameters. The solving *strategy* (brute-force permutation search) is fixed, but its *inputs* — target number, card count — vary at runtime.

```c
SolverConfig easy   = { .target = 24, .num_count = 4 };
SolverConfig hard   = { .target = 24, .num_count = 5 };  /* future */
SolverConfig custom = { .target = 12, .num_count = 4 };  /* future */
```

A more advanced future step: make the solver a function pointer inside `GameCtx`, so alternative algorithms (e.g. a heuristic solver, or one that also allows exponentiation) can be plugged in without touching `game.c`.

---

### 4.5 Data-Driven Buttons (replaces if/else chains)

Currently `main()` has:
```c
if (i==0) { pop_token(); }
if (i==1) { clear_expr(); }
...
```

After refactoring, action buttons are described as data:

```c
typedef void (*ActionFn)(GameCtx *);

typedef struct {
    const char *label;
    Color       bg, fg;
    ActionFn    on_click;
} ActionButton;

static const ActionButton ACTIONS[] = {
    { "BACK",   COL_GRAY,   COL_WHITE, game_pop_token   },
    { "CLEAR",  COL_RED,    COL_WHITE, game_clear_expr  },
    { "CHECK",  COL_GOLD,   COL_DARK,  game_check       },
    { "HINT",   COL_PURPLE, COL_WHITE, game_hint        },
    { "NEW",    COL_GREEN,  COL_DARK,  game_skip        },
    { "CUSTOM", COL_CYAN,   COL_DARK,  action_open_custom },
};
```

Adding a button is one array entry. Removing one is one deletion. The draw/click loop is generic and never changes.

---

## 5. Dependency Graph

```
main.c
 ├── screen_game.h
 │    ├── game.h
 │    │    ├── solver.h
 │    │    └── parser.h
 │    ├── card.h
 │    └── ui.h
 └── screen_custom.h
      ├── game.h
      └── ui.h

card.h   → raylib only
ui.h     → raylib only
solver.h → math.h only
parser.h → math.h only
game.h   → solver.h, parser.h  (NO raylib)
```

Key rule: **`game`, `solver`, and `parser` must never include `raylib.h`.**  
This boundary makes it possible to test the entire game logic layer with a plain `gcc` test harness, no graphics window required.

---

## 6. Future Expansion Points

| Feature | Where to add | Pattern used |
|---|---|---|
| Different target (e.g. make 12) | `SolverConfig.target` + `GameCtx` | Strategy |
| More than 4 cards | `SolverConfig.num_count` | Strategy |
| Timed challenge mode | New `GS_TIMED` state in `GameState` | State Machine |
| Settings screen | New `screen_settings.h/c` | Component + State Machine |
| Leaderboard / high score | New `screen_leaderboard.h/c` | Component + State Machine |
| Exponentiation operator | New op in `solver.c` + `parser.c` | Module (isolated change) |
| Swap raylib for SDL3 | Rewrite `ui.c`, `card.c`, screen draw fns only | Module (game logic untouched) |
| Sound effects | Add `sound.h/c`, call from `game_update` on state changes | Observer (game emits events) |
| Unit tests | Link `solver.c` + `parser.c` + `game.c` with a test main | Module (no raylib dependency) |

---

## 7. Refactoring Order (Recommended Steps)

Refactor incrementally so the game stays runnable at every step:

1. **Extract `solver.h/c`** — move solver functions out, update includes in `calc24_gui.c`.
2. **Extract `parser.h/c`** — replace globals with `ParseResult` struct, update callers.
3. **Extract `card.h/c`** — move `face()` and `card_draw()`.
4. **Extract `ui.h/c`** — move `UiButton`, `UiTextBox`, and draw helpers.
5. **Extract `game.h/c`** — introduce `GameCtx` and `GameState`; move all state and rule functions.
6. **Extract `screen_custom.h/c`** — move modal draw/update into its own screen.
7. **Extract `screen_game.h/c`** — move layout + event routing out of `main`.
8. **Slim down `main.c`** — coordinator only.

Each step compiles and runs correctly before the next step begins.
