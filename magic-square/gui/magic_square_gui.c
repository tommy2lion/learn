/*
 * magic_square_gui.c – Phase 3: static 3-D Rubik's cube render.
 *
 * Renders all 27 cubies as black boxes plus 54 coloured sticker quads
 * read from the cube data model.  An orbit camera is controlled by
 * left-mouse-drag (pan) and scroll-wheel (zoom).
 */
#include "raylib.h"
#include "rlgl.h"
#include "cube.h"
#include <math.h>

/* ── colour palette (index matches CUBE_WHITE … CUBE_BLUE in cube.h) ─ */
static const Color STICKER_COLOR[6] = {
    { 255, 255, 255, 255 },   /* 0 WHITE  – U face */
    { 255, 213,   0, 255 },   /* 1 YELLOW – D face */
    { 255, 165,   0, 255 },   /* 2 ORANGE – L face */
    { 196,  30,  58, 255 },   /* 3 RED    – R face */
    {   0, 158,  96, 255 },   /* 4 GREEN  – F face */
    {   0,  81, 186, 255 },   /* 5 BLUE   – B face */
};

/* ── face descriptors ─────────────────────────────────────────────────
 *
 * For each of the 6 faces (U D L R F B in cube.h order):
 *   nx,ny,nz  – outward unit normal (integer, one component ±1 rest 0)
 *   srx/y/z   – world-space "right" = direction of increasing sticker col
 *   sux/y/z   – world-space "up"    = direction of decreasing sticker row
 *                (row 0 is the visual "top" of the face)
 *
 * Derived directly from the orientation notes in cube.h.
 * ────────────────────────────────────────────────────────────────────*/
static const struct {
    int   nx, ny, nz;
    float srx, sry, srz;   /* right tangent */
    float sux, suy, suz;   /* up    tangent */
} FACE[6] = {
    /* U  normal +y,  right +x,  up -z  (row-0 = back row, z=-1) */
    {  0, +1,  0,    1, 0,  0,    0,  0, -1 },
    /* D  normal -y,  right +x,  up +z  (row-0 = front row, z=+1) */
    {  0, -1,  0,    1, 0,  0,    0,  0, +1 },
    /* L  normal -x,  right +z,  up +y */
    { -1,  0,  0,    0, 0, +1,    0, +1,  0 },
    /* R  normal +x,  right -z,  up +y */
    { +1,  0,  0,    0, 0, -1,    0, +1,  0 },
    /* F  normal +z,  right +x,  up +y */
    {  0,  0, +1,    1, 0,  0,    0, +1,  0 },
    /* B  normal -z,  right -x,  up +y */
    {  0,  0, -1,   -1, 0,  0,    0, +1,  0 },
};

/* Map (face, row, col) → cubie integer grid position in {-1,0,+1}^3.
 * Each formula is derived from cube.h's orientation description for
 * that face so that sticker index 0 maps to the correct corner cubie. */
static void sticker_to_cubie(int f, int row, int col,
                              int *cx, int *cy, int *cz)
{
    switch (f) {
        case U_FACE: *cx = col-1; *cy =    +1; *cz = row-1; break;
        case D_FACE: *cx = col-1; *cy =    -1; *cz = 1-row; break;
        case L_FACE: *cx =    -1; *cy = 1-row; *cz = col-1; break;
        case R_FACE: *cx =    +1; *cy = 1-row; *cz = 1-col; break;
        case F_FACE: *cx = col-1; *cy = 1-row; *cz =    +1; break;
        case B_FACE: *cx = 1-col; *cy = 1-row; *cz =    -1; break;
    }
}

/* ── rendering constants ─────────────────────────────────────────── */
#define CUBIE_SIZE   0.92f   /* side length; ~0.04 gap on each side  */
#define FACE_OFF     0.46f   /* = CUBIE_SIZE/2; face-centre distance  */
#define STICKER_HALF 0.38f   /* half-extent of each sticker quad      */
#define STICKER_LIFT 0.002f  /* avoids z-fighting with cubie surface  */

/* Draw a single rectangular sticker using rlgl quads.
 * Backface culling is disabled by the caller so winding order
 * doesn't matter here.                                               */
static void draw_sticker(Vector3 fc, Vector3 sr, Vector3 su,
                         float half, Color col)
{
    float c0x = fc.x - half*sr.x - half*su.x;
    float c0y = fc.y - half*sr.y - half*su.y;
    float c0z = fc.z - half*sr.z - half*su.z;
    float c1x = fc.x + half*sr.x - half*su.x;
    float c1y = fc.y + half*sr.y - half*su.y;
    float c1z = fc.z + half*sr.z - half*su.z;
    float c2x = fc.x + half*sr.x + half*su.x;
    float c2y = fc.y + half*sr.y + half*su.y;
    float c2z = fc.z + half*sr.z + half*su.z;
    float c3x = fc.x - half*sr.x + half*su.x;
    float c3y = fc.y - half*sr.y + half*su.y;
    float c3z = fc.z - half*sr.z + half*su.z;

    rlBegin(RL_QUADS);
        rlColor4ub(col.r, col.g, col.b, col.a);
        rlVertex3f(c0x, c0y, c0z);
        rlVertex3f(c1x, c1y, c1z);
        rlVertex3f(c2x, c2y, c2z);
        rlVertex3f(c3x, c3y, c3z);
    rlEnd();
}

static void draw_cube_state(const Cube *cube)
{
    /* Pass 1 – 27 black cubies (the gaps between them show as black). */
    for (int cx = -1; cx <= 1; cx++)
        for (int cy = -1; cy <= 1; cy++)
            for (int cz = -1; cz <= 1; cz++)
                DrawCube((Vector3){(float)cx, (float)cy, (float)cz},
                         CUBIE_SIZE, CUBIE_SIZE, CUBIE_SIZE, BLACK);

    /* Pass 2 – 54 coloured sticker quads, one per entry in the model. */
    rlDisableBackfaceCulling();
    for (int f = 0; f < 6; f++) {
        Vector3 normal = { (float)FACE[f].nx,
                           (float)FACE[f].ny,
                           (float)FACE[f].nz };
        Vector3 sr = { FACE[f].srx, FACE[f].sry, FACE[f].srz };
        Vector3 su = { FACE[f].sux, FACE[f].suy, FACE[f].suz };
        float lift = FACE_OFF + STICKER_LIFT;

        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                int cx, cy, cz;
                sticker_to_cubie(f, row, col, &cx, &cy, &cz);

                /* Face-centre = cubie-centre + outward normal * (half-size + lift) */
                Vector3 fc = {
                    (float)cx + normal.x * lift,
                    (float)cy + normal.y * lift,
                    (float)cz + normal.z * lift,
                };
                Color sticker_col = STICKER_COLOR[cube->s[f * 9 + row * 3 + col]];
                draw_sticker(fc, sr, su, STICKER_HALF, sticker_col);
            }
        }
    }
    rlEnableBackfaceCulling();
}

/* ── orbit camera ────────────────────────────────────────────────── */
typedef struct { float radius, azimuth, elevation; } OrbitCam;

static void orbit_update(OrbitCam *oc, Camera3D *cam)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 d = GetMouseDelta();
        oc->azimuth   -= d.x * 0.005f;
        oc->elevation += d.y * 0.005f;
        if (oc->elevation >  1.50f) oc->elevation =  1.50f;
        if (oc->elevation < -1.50f) oc->elevation = -1.50f;
    }
    float scroll = GetMouseWheelMove();
    oc->radius -= scroll * 0.5f;
    if (oc->radius <  3.0f) oc->radius =  3.0f;
    if (oc->radius > 20.0f) oc->radius = 20.0f;

    cam->position.x = oc->radius * cosf(oc->elevation) * sinf(oc->azimuth);
    cam->position.y = oc->radius * sinf(oc->elevation);
    cam->position.z = oc->radius * cosf(oc->elevation) * cosf(oc->azimuth);
}

/* ── main ────────────────────────────────────────────────────────── */
int main(void)
{
    const int SCREEN_W = 800;
    const int SCREEN_H = 600;

    InitWindow(SCREEN_W, SCREEN_H, "Magic Square – Rubik's Cube");
    SetTargetFPS(60);

    Cube cube;
    cube_reset(&cube);

    OrbitCam oc = { .radius = 7.0f, .azimuth = 0.6f, .elevation = 0.5f };
    Camera3D camera = {
        .target     = { 0, 0, 0 },
        .up         = { 0, 1, 0 },
        .fovy       = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
    orbit_update(&oc, &camera);   /* set initial position before first frame */

    while (!WindowShouldClose()) {
        orbit_update(&oc, &camera);

        BeginDrawing();
            ClearBackground((Color){ 30, 30, 30, 255 });

            BeginMode3D(camera);
                draw_cube_state(&cube);
            EndMode3D();

            DrawFPS(10, 10);
            DrawText("Left-drag: orbit  |  Scroll: zoom",
                     10, SCREEN_H - 28, 18, GRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
