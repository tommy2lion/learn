#include "raylib.h"
#include "sim.h"
#include "parser.h"
#include "canvas.h"
#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 1280
#define WIN_H 800

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return NULL; }
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, len, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    const char *file_path = (argc >= 2) ? argv[1] : "untitled.dcs";

    /* Try to load the file; if missing, start with an empty canvas. */
    CanvasState cs = {0};
    if (argc >= 2) {
        char *text = read_file(argv[1]);
        if (text) {
            char err[256] = {0};
            Circuit *c = parse_circuit(text, err, sizeof(err));
            free(text);
            if (c) {
                canvas_init(&cs, c);
                circuit_free(c);
            } else {
                fprintf(stderr, "parse error in %s: %s\n", argv[1], err);
                return 1;
            }
        }
        /* file doesn't exist → empty canvas, will create it on save */
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "DCS Editor");
    SetTargetFPS(60);

    EditorState ed;
    editor_init(&ed, &cs, file_path);

    Camera2D cam = {0};
    cam.target   = (Vector2){WIN_W / 2.0f, WIN_H / 2.0f};
    cam.offset   = (Vector2){WIN_W / 2.0f, WIN_H / 2.0f};
    cam.zoom     = 1.0f;
    cam.rotation = 0.0f;

    while (!WindowShouldClose()) {
        editor_update(&ed, &cs, &cam);

        BeginDrawing();
        ClearBackground((Color){240, 240, 240, 255});

        /* Clip canvas drawing to the area outside the sidebar/header/status bar */
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        BeginScissorMode(EDITOR_SIDEBAR_W, EDITOR_HEADER_H,
                         sw - EDITOR_SIDEBAR_W,
                         sh - EDITOR_HEADER_H - EDITOR_STATUS_H);
        BeginMode2D(cam);
        canvas_draw(&cs, cam);
        EndMode2D();
        EndScissorMode();

        editor_draw(&ed, &cs, cam, sw, sh);

        EndDrawing();
    }

    canvas_free(&cs);
    CloseWindow();
    return 0;
}
