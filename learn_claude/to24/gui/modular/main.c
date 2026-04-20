/*
 * CALC 24 GAME — Modular Graphical Edition
 *
 * main.c is a pure coordinator:
 *   - initialises raylib and game state
 *   - runs the main loop
 *   - dispatches to the appropriate screen based on g.state
 *
 * Build (from c/):
 *   gcc -O2 -std=c99 -Wall -Wextra \
 *       -I/c/msys64/mingw64/include -Igui/modular \
 *       gui/modular/main.c gui/modular/solver.c gui/modular/parser.c \
 *       gui/modular/card.c gui/modular/ui.c gui/modular/game.c \
 *       gui/modular/screen_game.c gui/modular/screen_custom.c \
 *       -o calc24_modular.exe -lm -lraylib -lopengl32 -lgdi32 -lwinmm
 */

#include <stdlib.h>
#include <time.h>

#include "ui.h"           /* SCREEN_W, SCREEN_H, COL_BG */
#include "game.h"         /* GameCtx, GameState          */
#include "screen_game.h"  /* screen_game_draw/update     */
#include "screen_custom.h"/* CustomInputCtx, screen_custom_* */

int main(void) {
    srand((unsigned)time(NULL));

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_W, SCREEN_H, "Calc 24  —  Modular Edition");
    SetTargetFPS(60);

    GameCtx        g  = {0};
    CustomInputCtx ci = {0};
    float          anim = 0.0f;

    game_init(&g);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        anim += dt;

        /* ── Update ── */
        game_update(&g, dt);   /* always: manages timers and state transitions */

        if (g.state == GS_CUSTOM_INPUT)
            screen_custom_update(&ci, &g, SCREEN_W / 2, SCREEN_H / 2);
        else
            screen_game_update(&g, &ci, dt);

        /* ── Draw ── */
        BeginDrawing();
            ClearBackground(COL_BG);

            /* Game board is always visible underneath everything */
            screen_game_draw(&g, anim);

            /* Custom modal rendered on top when active */
            if (g.state == GS_CUSTOM_INPUT)
                screen_custom_draw(&ci, SCREEN_W / 2, SCREEN_H / 2, anim);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
