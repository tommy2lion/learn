// sudoku.c — 9×9 Sudoku GUI in C + raylib
// ─────────────────────────────────────────────────────────────────────────────
// Controls:
//   Mouse click       — select a cell
//   1–9 / Numpad 1–9 — place a digit
//   0 / Backspace / Delete — erase a digit
//   Arrow keys        — move selection
//   N                 — toggle pencil-mark (notes) mode
// ─────────────────────────────────────────────────────────────────────────────

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// ── Layout constants ──────────────────────────────────────────────────────────
#define CELL     60              // pixels per cell
#define GX       30              // grid left origin
#define GY       80              // grid top  origin
#define GW       (CELL * 9)      // grid width = height = 540
#define PANEL_X  (GX + GW + 22) // right panel left edge = 592
#define WIN_W    775
#define WIN_H    700

// ── Color palette ─────────────────────────────────────────────────────────────
#define CBKG    CLITERAL(Color){245, 245, 250, 255}
#define CWHITE  CLITERAL(Color){255, 255, 255, 255}
#define CPEER   CLITERAL(Color){228, 234, 248, 255}  // same row/col/box as selection
#define CSNM    CLITERAL(Color){197, 224, 252, 255}  // same digit as selection
#define CSEL    CLITERAL(Color){164, 210, 255, 255}  // selected cell
#define CFIXED  CLITERAL(Color){ 24,  48, 100, 255}  // given clue digit
#define CUSER   CLITERAL(Color){ 37,  99, 235, 255}  // player digit
#define CERR    CLITERAL(Color){220,  38,  38, 255}  // wrong digit
#define CNOTE   CLITERAL(Color){120, 130, 165, 255}  // pencil marks
#define CLTHIN  CLITERAL(Color){180, 185, 205, 255}  // thin grid line
#define CLTHK   CLITERAL(Color){ 50,  55,  80, 255}  // thick box line
#define CPANEL  CLITERAL(Color){233, 237, 250, 255}  // panel background
#define CBTN    CLITERAL(Color){ 59, 130, 246, 255}  // button normal
#define CBTNHOV CLITERAL(Color){ 37,  99, 235, 255}  // button hover
#define CBTNOFF CLITERAL(Color){160, 185, 220, 255}  // button dimmed

// Difficulty colors (Easy=green, Medium=orange, Hard=red)
static const Color DCOL[3] = {{40,167,69,255}, {230,130,0,255}, {220,38,38,255}};
static const Color DHOV[3] = {{30,130,50,255}, {185,100,0,255}, {170,20,20,255}};

// ── Game state ────────────────────────────────────────────────────────────────
typedef struct {
    int  board   [9][9];    // current player board  (0 = empty)
    int  solution[9][9];    // the unique complete solution
    bool fixed   [9][9];    // true = given clue, cannot be edited
    bool error   [9][9];    // true = conflicts with solution
    bool notes   [9][9][9]; // pencil marks [row][col][digit-1]
    int  selRow, selCol;    // selected cell (-1 = none)
    bool won, notesMode;
    float elapsed, wonTime;
    int  difficulty;        // 0=easy 1=medium 2=hard
} Game;

// ── Solver helpers ────────────────────────────────────────────────────────────

static void shuffle(int a[], int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

// Returns true if placing `v` at (r,c) violates no constraint
static bool valid(int b[9][9], int r, int c, int v) {
    for (int i = 0; i < 9; i++)
        if (b[r][i] == v || b[i][c] == v) return false;
    int R = r/3*3, C = c/3*3;
    for (int dr = 0; dr < 3; dr++)
        for (int dc = 0; dc < 3; dc++)
            if (b[R+dr][C+dc] == v) return false;
    return true;
}

// Randomised backtracking fill — builds a complete, legal grid
static bool fillGrid(int b[9][9]) {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (!b[r][c]) {
                int v[9] = {1,2,3,4,5,6,7,8,9};
                shuffle(v, 9);
                for (int i = 0; i < 9; i++) {
                    if (valid(b, r, c, v[i])) {
                        b[r][c] = v[i];
                        if (fillGrid(b)) return true;
                        b[r][c] = 0;
                    }
                }
                return false;
            }
    return true;
}

// Count solutions up to `lim`; pass lim=2 for a uniqueness check
static int countSol(int b[9][9], int lim) {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (!b[r][c]) {
                int cnt = 0;
                for (int v = 1; v <= 9; v++) {
                    if (valid(b, r, c, v)) {
                        b[r][c] = v;
                        cnt += countSol(b, lim - cnt);
                        b[r][c] = 0;
                        if (cnt >= lim) return cnt;
                    }
                }
                return cnt;
            }
    return 1; // all cells filled → one solution found
}

// ── Game lifecycle ────────────────────────────────────────────────────────────

static void newGame(Game *g, int diff) {
    memset(g, 0, sizeof *g);
    g->selRow = g->selCol = -1;
    g->difficulty = diff;

    // 1. Build a complete solved grid
    fillGrid(g->solution);
    memcpy(g->board, g->solution, sizeof g->board);

    // 2. Remove cells one by one, keeping the solution unique
    static const int TARGET[] = {45, 36, 28}; // clues remaining per difficulty
    int target = TARGET[diff];
    int order[81];
    for (int i = 0; i < 81; i++) order[i] = i;
    shuffle(order, 81);

    int filled = 81;
    for (int i = 0; i < 81 && filled > target; i++) {
        int r = order[i] / 9, c = order[i] % 9;
        int backup = g->board[r][c];
        g->board[r][c] = 0;
        int tmp[9][9]; memcpy(tmp, g->board, sizeof tmp);
        if (countSol(tmp, 2) == 1) filled--;
        else                        g->board[r][c] = backup; // restore if ambiguous
    }

    // 3. Lock the remaining clues
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            g->fixed[r][c] = (g->board[r][c] != 0);
}

// ── Logic helpers ─────────────────────────────────────────────────────────────

static void refreshErrors(Game *g) {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            g->error[r][c] = g->board[r][c] && g->board[r][c] != g->solution[r][c];
}

static bool isWon(const Game *g) {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (g->board[r][c] != g->solution[r][c]) return false;
    return true;
}

static void applyNum(Game *g, int num) {
    if (g->selRow < 0 || g->fixed[g->selRow][g->selCol] || g->won) return;
    if (g->notesMode) {
        g->notes[g->selRow][g->selCol][num - 1] ^= 1;
    } else {
        int *cell = &g->board[g->selRow][g->selCol];
        *cell = (*cell == num) ? 0 : num;          // toggle: same digit clears it
        memset(g->notes[g->selRow][g->selCol], 0, 9);
        refreshErrors(g);
        if (*cell && isWon(g)) { g->won = true; g->wonTime = g->elapsed; }
    }
}

static void eraseCell(Game *g) {
    if (g->selRow < 0 || g->fixed[g->selRow][g->selCol] || g->won) return;
    g->board[g->selRow][g->selCol] = 0;
    memset(g->notes[g->selRow][g->selCol], 0, 9);
    refreshErrors(g);
}

// ── Immediate-mode UI helpers ─────────────────────────────────────────────────

// Draw a rounded button; returns true if clicked this frame
static bool btn(const char *txt, int x, int y, int w, int h,
                Color bg, Color hov) {
    Rectangle r = {x, y, w, h};
    bool over = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRounded(r, 0.25f, 6, over ? hov : bg);
    int fs = h * 58 / 100;
    int tw = MeasureText(txt, fs);
    DrawText(txt, x + (w - tw) / 2, y + (h - fs) / 2, fs, WHITE);
    return over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Draw text centred in a bounding rectangle
static void txtC(const char *s, int x, int y, int w, int h, int fs, Color col) {
    DrawText(s, x + (w - MeasureText(s, fs)) / 2, y + (h - fs) / 2, fs, col);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(void) {
    srand((unsigned)time(NULL));
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIN_W, WIN_H, "Sudoku");
    SetTargetFPS(60);

    Game g;
    newGame(&g, 1); // start on Medium

    while (!WindowShouldClose()) {

        // ── Update ────────────────────────────────────────────────────────────

        if (!g.won) g.elapsed += GetFrameTime();

        // Mouse → select cell
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mp = GetMousePosition();
            int mc = (int)((mp.x - GX) / CELL);
            int mr = (int)((mp.y - GY) / CELL);
            if (mp.x >= GX && mp.x < GX + GW &&
                mp.y >= GY && mp.y < GY + GW &&
                mc >= 0 && mc < 9 && mr >= 0 && mr < 9) {
                // clicking the already-selected cell deselects it
                if (mr == g.selRow && mc == g.selCol) g.selRow = g.selCol = -1;
                else { g.selRow = mr; g.selCol = mc; }
            }
        }

        // Keyboard input (only when a cell is selected)
        if (g.selRow >= 0) {
            if (IsKeyPressed(KEY_UP)    && g.selRow > 0) g.selRow--;
            if (IsKeyPressed(KEY_DOWN)  && g.selRow < 8) g.selRow++;
            if (IsKeyPressed(KEY_LEFT)  && g.selCol > 0) g.selCol--;
            if (IsKeyPressed(KEY_RIGHT) && g.selCol < 8) g.selCol++;

            for (int k = 0; k < 9; k++)
                if (IsKeyPressed(KEY_ONE + k) || IsKeyPressed(KEY_KP_1 + k))
                    applyNum(&g, k + 1);

            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE) ||
                IsKeyPressed(KEY_ZERO)      || IsKeyPressed(KEY_KP_0))
                eraseCell(&g);

            if (IsKeyPressed(KEY_N)) g.notesMode ^= 1;
        }

        // ── Draw ──────────────────────────────────────────────────────────────

        BeginDrawing();
        ClearBackground(CBKG);

        // ── Header bar ────────────────────────────────────────────────────────
        DrawText("SUDOKU", GX, 22, 30, CLTHK);
        {
            int s = (int)(g.won ? g.wonTime : g.elapsed);
            char tb[16]; snprintf(tb, sizeof tb, "%02d:%02d", s / 60, s % 60);
            DrawText(tb, GX + GW - MeasureText(tb, 26), 24, 26, CLTHK);
        }

        // ── Cell background highlights ────────────────────────────────────────
        int selNum = (g.selRow >= 0) ? g.board[g.selRow][g.selCol] : 0;
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                int x = GX + c * CELL, y = GY + r * CELL;
                Color bg = CWHITE;
                if (r == g.selRow && c == g.selCol)
                    bg = CSEL;
                else if (selNum && g.board[r][c] == selNum)
                    bg = CSNM;
                else if (g.selRow >= 0 &&
                         (r == g.selRow || c == g.selCol ||
                          (r/3 == g.selRow/3 && c/3 == g.selCol/3)))
                    bg = CPEER;
                DrawRectangle(x + 1, y + 1, CELL - 1, CELL - 1, bg);
            }
        }

        // ── Digits and pencil marks ───────────────────────────────────────────
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                int x = GX + c * CELL, y = GY + r * CELL;
                if (g.board[r][c]) {
                    char buf[2] = {(char)('0' + g.board[r][c]), 0};
                    Color col = g.fixed[r][c] ? CFIXED
                              : g.error[r][c] ? CERR : CUSER;
                    txtC(buf, x, y, CELL, CELL, 34, col);
                } else {
                    // Draw 3×3 pencil marks inside empty cell
                    for (int n = 0; n < 9; n++) {
                        if (!g.notes[r][c][n]) continue;
                        char nb[2] = {(char)('1' + n), 0};
                        DrawText(nb,
                            x + 3 + (n % 3) * (CELL / 3),
                            y + 2 + (n / 3) * (CELL / 3),
                            15, CNOTE);
                    }
                }
            }
        }

        // ── Grid lines ────────────────────────────────────────────────────────
        for (int i = 0; i <= 9; i++) {
            bool thick = (i % 3 == 0);
            Color lc = thick ? CLTHK : CLTHIN;
            float lw = thick ? 2.5f : 1.0f;
            DrawLineEx((Vector2){GX + i * CELL, GY},
                       (Vector2){GX + i * CELL, GY + GW}, lw, lc);
            DrawLineEx((Vector2){GX,       GY + i * CELL},
                       (Vector2){GX + GW,  GY + i * CELL}, lw, lc);
        }

        // ── Right panel ───────────────────────────────────────────────────────
        int PX = PANEL_X;
        int PW = WIN_W - PANEL_X - 12;
        int PY = GY;
        DrawRectangleRounded(
            (Rectangle){PX - 6, PY - 8, PW + 12, WIN_H - PY + 2},
            0.08f, 6, CPANEL);

        // Difficulty badge
        {
            static const char *dName[] = {"Easy", "Medium", "Hard"};
            const char *dn = dName[g.difficulty];
            int tw = MeasureText(dn, 18);
            int bw = tw + 18, bx = PX + (PW - bw) / 2;
            DrawRectangleRounded((Rectangle){bx, PY, bw, 26}, 0.5f, 6,
                                 DCOL[g.difficulty]);
            DrawText(dn, bx + 9, PY + 4, 18, WHITE);
        }
        PY += 36;

        // Count how many of each digit remain to be placed
        int rem[9] = {9,9,9,9,9,9,9,9,9};
        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++)
                if (g.board[r][c]) rem[g.board[r][c] - 1]--;

        // Number buttons 1–9 in a 3×3 pad
        {
            const int BW = 44, BH = 44, GAP = 5;
            for (int n = 0; n < 9; n++) {
                int bx = PX + (n % 3) * (BW + GAP);
                int by = PY + (n / 3) * (BH + GAP);
                char buf[2] = {(char)('1' + n), 0};
                Color bg  = (rem[n] == 0) ? CBTNOFF : CBTN;
                Color hov = (rem[n] == 0) ? CBTNOFF : CBTNHOV;
                if (btn(buf, bx, by, BW, BH, bg, hov)) applyNum(&g, n + 1);
                // Small remaining-count badge in bottom-right corner
                if (rem[n] > 0) {
                    char rb[12]; snprintf(rb, sizeof rb, "%d", rem[n]);
                    DrawText(rb, bx + BW - 11, by + BH - 13, 11,
                             CLITERAL(Color){200, 230, 255, 255});
                }
            }
            PY += 3 * (BH + GAP) + 6;
        }

        // Erase button
        if (btn("Erase", PX, PY, PW, 34, CBTN, CBTNHOV)) eraseCell(&g);
        PY += 42;

        // Notes toggle button (highlights when active)
        {
            const char *lbl = g.notesMode ? "Notes: ON" : "Notes: OFF";
            Color bg  = g.notesMode ? CBTNHOV                          : CBTN;
            Color hov = g.notesMode ? CLITERAL(Color){20, 70, 200, 255}: CBTNHOV;
            if (btn(lbl, PX, PY, PW, 34, bg, hov)) g.notesMode ^= 1;
        }
        PY += 50;

        // New Game section
        DrawText("New Game:", PX, PY, 16, CLTHK);
        PY += 22;
        static const char *dBtnLabel[] = {"Easy", "Medium", "Hard"};
        for (int d = 0; d < 3; d++) {
            if (btn(dBtnLabel[d], PX, PY, PW, 32, DCOL[d], DHOV[d]))
                newGame(&g, d);
            PY += 40;
        }

        // Controls hint at bottom of panel
        PY += 8;
        static const char *hints[] = {
            "[N] Notes", "[Del] Erase", "Arrows move"
        };
        for (int i = 0; i < 3; i++) {
            DrawText(hints[i], PX, PY, 13,
                     CLITERAL(Color){120, 130, 160, 255});
            PY += 17;
        }

        // ── Win overlay ───────────────────────────────────────────────────────
        if (g.won) {
            DrawRectangle(GX, GY, GW, GW, CLITERAL(Color){0, 20, 60, 175});

            int ws = (int)g.wonTime;
            char wb[32]; snprintf(wb, sizeof wb, "SOLVED!  %02d:%02d", ws/60, ws%60);
            DrawText(wb, GX + (GW - MeasureText(wb, 38)) / 2,
                     GY + GW / 2 - 30, 38, YELLOW);

            const char *hint = "Pick a difficulty to play again";
            DrawText(hint, GX + (GW - MeasureText(hint, 17)) / 2,
                     GY + GW / 2 + 22, 17,
                     CLITERAL(Color){180, 200, 255, 255});
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
