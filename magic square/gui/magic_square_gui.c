#include "raylib.h"
#include "raymath.h"

int main(void)
{
    const int SCREEN_W = 800;
    const int SCREEN_H = 600;

    InitWindow(SCREEN_W, SCREEN_H, "Magic Square – Rubik's Cube");
    SetTargetFPS(60);

    Camera3D camera = {
        .position   = { 4.0f, 4.0f, 6.0f },
        .target     = { 0.0f, 0.0f, 0.0f },
        .up         = { 0.0f, 1.0f, 0.0f },
        .fovy       = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawCube((Vector3){0.0f, 0.0f, 0.0f}, 2.0f, 2.0f, 2.0f, RED);
                DrawCubeWires((Vector3){0.0f, 0.0f, 0.0f}, 2.0f, 2.0f, 2.0f, MAROON);
                DrawGrid(10, 1.0f);
            EndMode3D();

            DrawFPS(10, 10);
            DrawText("Phase 1: skeleton OK", 10, 34, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
