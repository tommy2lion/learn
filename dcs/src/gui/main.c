#include "raylib.h"
#include "sim.h"
#include "parser.h"
#include "canvas.h"
#include <stdio.h>
#include <stdlib.h>

#define WIN_W 1024
#define WIN_H 768

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
    if (argc < 2) {
        fprintf(stderr, "usage: dcs_gui <file.dcs>\n");
        return 1;
    }

    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    char err[256] = {0};
    Circuit *c = parse_circuit(text, err, sizeof(err));
    free(text);
    if (!c) { fprintf(stderr, "parse error: %s\n", err); return 1; }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "DCS Viewer");
    SetTargetFPS(60);

    CanvasState cs;
    canvas_init(&cs, c);

    Camera2D cam = {0};
    cam.target   = (Vector2){WIN_W / 2.0f, WIN_H / 2.0f};
    cam.offset   = (Vector2){WIN_W / 2.0f, WIN_H / 2.0f};
    cam.zoom     = 1.0f;
    cam.rotation = 0.0f;

    while (!WindowShouldClose()) {
        /* Pan: right or middle mouse drag */
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
            IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 d = GetMouseDelta();
            cam.target.x -= d.x / cam.zoom;
            cam.target.y -= d.y / cam.zoom;
        }

        /* Zoom toward mouse cursor with scroll wheel */
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            Vector2 mw = GetScreenToWorld2D(GetMousePosition(), cam);
            cam.offset = GetMousePosition();
            cam.target = mw;
            cam.zoom += wheel * 0.1f * cam.zoom;
            if (cam.zoom < 0.1f) cam.zoom = 0.1f;
            if (cam.zoom > 5.0f) cam.zoom = 5.0f;
        }

        BeginDrawing();
        ClearBackground((Color){240, 240, 240, 255});

        BeginMode2D(cam);
        canvas_draw(&cs, cam);
        EndMode2D();

        /* Header bar (screen space, drawn on top) */
        DrawRectangle(0, 0, GetScreenWidth(), 30, (Color){200, 200, 200, 255});
        DrawText(TextFormat("File: %s", argv[1]), 10, 8, 16, BLACK);
        DrawText("R/M-drag: pan  |  Scroll: zoom  |  ESC: quit",
                 GetScreenWidth() - 380, 10, 14, DARKGRAY);

        EndDrawing();
    }

    canvas_free(&cs);
    circuit_free(c);
    CloseWindow();
    return 0;
}
