/*
 * magic_square_gui.c – Phase 5: full input + undo/redo.
 *
 * History stack: every user-initiated move is recorded on animation
 * commit.  Undo/redo animate the inverse/original move but do NOT
 * modify the history cursor until they complete, so cancelling with
 * ENTER always leaves the state consistent.
 *
 * Keys:
 *   U / D / L / R / F / B   – clockwise quarter-turn
 *   Shift + letter           – counter-clockwise quarter-turn
 *   (press same letter twice to get a 180° turn via the queue)
 *   Z                        – undo last move (animated)
 *   Y                        – redo (animated)
 *   SPACE                    – random 20-move scramble (clears history)
 *   ENTER                    – reset to solved  (clears history)
 *   ESC                      – quit
 * Camera: left-drag to orbit, scroll to zoom.
 */
#include "raylib.h"
#include "rlgl.h"
#include "cube.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

/* ── colour palette ──────────────────────────────────────────────── */
static const Color STICKER_COLOR[6] = {
    { 255, 255, 255, 255 },   /* 0 WHITE  – U */
    { 255, 213,   0, 255 },   /* 1 YELLOW – D */
    { 255, 165,   0, 255 },   /* 2 ORANGE – L */
    { 196,  30,  58, 255 },   /* 3 RED    – R */
    {   0, 158,  96, 255 },   /* 4 GREEN  – F */
    {   0,  81, 186, 255 },   /* 5 BLUE   – B */
};

/* ── face descriptors ────────────────────────────────────────────── */
static const struct {
    int   nx, ny, nz;
    float srx, sry, srz;
    float sux, suy, suz;
} FACE[6] = {
    {  0, +1,  0,    1, 0,  0,    0,  0, -1 },  /* U */
    {  0, -1,  0,    1, 0,  0,    0,  0, +1 },  /* D */
    { -1,  0,  0,    0, 0, +1,    0, +1,  0 },  /* L */
    { +1,  0,  0,    0, 0, -1,    0, +1,  0 },  /* R */
    {  0,  0, +1,    1, 0,  0,    0, +1,  0 },  /* F */
    {  0,  0, -1,   -1, 0,  0,    0, +1,  0 },  /* B */
};

/* Rotation axis and CW base-angle per face (verified against cube.c). */
static const struct { float ax, ay, az, cw; } FACE_ROT[6] = {
    { 0, 1, 0,  90.0f },   /* U */
    { 0, 1, 0,  90.0f },   /* D */
    { 1, 0, 0,  90.0f },   /* L */
    { 1, 0, 0, -90.0f },   /* R */
    { 0, 0, 1, -90.0f },   /* F */
    { 0, 0, 1,  90.0f },   /* B */
};

/* ── sticker → cubie ─────────────────────────────────────────────── */
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

/* ── rendering ───────────────────────────────────────────────────── */
#define CUBIE_SIZE    0.92f
#define FACE_OFF      0.46f
#define STICKER_HALF  0.38f
#define STICKER_LIFT  0.002f
#define ANIM_SPEED   300.0f

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

static bool in_layer(int face_idx, int cx, int cy, int cz)
{
    switch (face_idx) {
        case U_FACE: return cy == +1;
        case D_FACE: return cy == -1;
        case L_FACE: return cx == -1;
        case R_FACE: return cx == +1;
        case F_FACE: return cz == +1;
        case B_FACE: return cz == -1;
        default:     return false;
    }
}

static void draw_cube_state(const Cube *cube, int anim_face, float anim_angle)
{
    bool  anim  = (anim_face >= 0);
    float ax    = anim ? FACE_ROT[anim_face].ax : 0;
    float ay    = anim ? FACE_ROT[anim_face].ay : 0;
    float az    = anim ? FACE_ROT[anim_face].az : 0;
    float lift  = FACE_OFF + STICKER_LIFT;

    /* pass 1: static cubies */
    for (int cx = -1; cx <= 1; cx++)
        for (int cy = -1; cy <= 1; cy++)
            for (int cz = -1; cz <= 1; cz++)
                if (!anim || !in_layer(anim_face, cx, cy, cz))
                    DrawCube((Vector3){(float)cx,(float)cy,(float)cz},
                             CUBIE_SIZE, CUBIE_SIZE, CUBIE_SIZE, BLACK);

    /* pass 2: rotating cubies */
    if (anim) {
        rlPushMatrix();
            rlRotatef(anim_angle, ax, ay, az);
            for (int cx = -1; cx <= 1; cx++)
                for (int cy = -1; cy <= 1; cy++)
                    for (int cz = -1; cz <= 1; cz++)
                        if (in_layer(anim_face, cx, cy, cz))
                            DrawCube((Vector3){(float)cx,(float)cy,(float)cz},
                                     CUBIE_SIZE, CUBIE_SIZE, CUBIE_SIZE, BLACK);
        rlPopMatrix();
    }

    rlDisableBackfaceCulling();

    /* pass 3: static stickers */
    for (int f = 0; f < 6; f++) {
        Vector3 normal = { (float)FACE[f].nx,(float)FACE[f].ny,(float)FACE[f].nz };
        Vector3 sr = { FACE[f].srx, FACE[f].sry, FACE[f].srz };
        Vector3 su = { FACE[f].sux, FACE[f].suy, FACE[f].suz };
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                int scx, scy, scz;
                sticker_to_cubie(f, row, col, &scx, &scy, &scz);
                if (anim && in_layer(anim_face, scx, scy, scz)) continue;
                Vector3 fc = { (float)scx + normal.x * lift,
                               (float)scy + normal.y * lift,
                               (float)scz + normal.z * lift };
                draw_sticker(fc, sr, su, STICKER_HALF,
                             STICKER_COLOR[cube->s[f*9 + row*3 + col]]);
            }
        }
    }

    /* pass 4: rotating stickers */
    if (anim) {
        rlPushMatrix();
            rlRotatef(anim_angle, ax, ay, az);
            for (int f = 0; f < 6; f++) {
                Vector3 normal = { (float)FACE[f].nx,(float)FACE[f].ny,(float)FACE[f].nz };
                Vector3 sr = { FACE[f].srx, FACE[f].sry, FACE[f].srz };
                Vector3 su = { FACE[f].sux, FACE[f].suy, FACE[f].suz };
                for (int row = 0; row < 3; row++) {
                    for (int col = 0; col < 3; col++) {
                        int scx, scy, scz;
                        sticker_to_cubie(f, row, col, &scx, &scy, &scz);
                        if (!in_layer(anim_face, scx, scy, scz)) continue;
                        Vector3 fc = { (float)scx + normal.x * lift,
                                       (float)scy + normal.y * lift,
                                       (float)scz + normal.z * lift };
                        draw_sticker(fc, sr, su, STICKER_HALF,
                                     STICKER_COLOR[cube->s[f*9 + row*3 + col]]);
                    }
                }
            }
        rlPopMatrix();
    }

    rlEnableBackfaceCulling();
}

/* ── move queue ──────────────────────────────────────────────────── */
#define QUEUE_CAP 32

typedef struct {
    int data[QUEUE_CAP];
    int head, count;
} Queue;

static void queue_push(Queue *q, int move)
{
    if (q->count >= QUEUE_CAP) return;
    q->data[(q->head + q->count) % QUEUE_CAP] = move;
    q->count++;
}

static int queue_pop(Queue *q)
{
    int move = q->data[q->head];
    q->head  = (q->head + 1) % QUEUE_CAP;
    q->count--;
    return move;
}

/* ── history (undo/redo) ─────────────────────────────────────────
 *
 * moves[0..pos-1] have been applied to the cube.
 * moves[pos..len-1] are undone (available for redo).
 * Recording a new move truncates any redo branch (sets len = pos).
 * ────────────────────────────────────────────────────────────────*/
#define HIST_CAP 512

typedef struct {
    int moves[HIST_CAP];
    int len, pos;
} History;

static void hist_clear(History *h)         { h->len = h->pos = 0; }

/* Record a committed user move.  Truncates redo branch first. */
static void hist_record(History *h, int move)
{
    h->len = h->pos;               /* discard any redo future */
    if (h->pos < HIST_CAP) {
        h->moves[h->pos++] = move;
        h->len = h->pos;
    }
}

/* ── animation state ─────────────────────────────────────────────
 *
 * record=true  → on commit, call hist_record (user-initiated move)
 * record=false → on commit, just cube_apply  (undo / redo move)
 * ────────────────────────────────────────────────────────────────*/
typedef struct {
    int   move;
    float angle, target;
    bool  active, record;
} Anim;

static void anim_start(Anim *a, int move, bool record)
{
    int   face    = move / 3;
    int   kind    = move % 3;
    float cw      = FACE_ROT[face].cw;
    float targets[3] = { cw, -cw, cw * 2.0f };
    a->move   = move;
    a->angle  = 0.0f;
    a->target = targets[kind];
    a->active = true;
    a->record = record;
}

/* Advance animation by one frame; commits to cube+history on completion. */
static void anim_update(Anim *a, Cube *cube, History *hist)
{
    if (!a->active) return;
    float dir = (a->target >= 0.0f) ? +1.0f : -1.0f;
    a->angle += dir * ANIM_SPEED * GetFrameTime();
    if (dir * a->angle >= dir * a->target) {
        cube_apply(cube, a->move);
        if (a->record) hist_record(hist, a->move);
        a->active = false;
    }
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

    Cube    cube;    cube_reset(&cube);
    Queue   queue  = { 0 };
    History hist   = { 0 };
    Anim    anim   = { .active = false };

    OrbitCam oc = { .radius = 7.0f, .azimuth = 0.6f, .elevation = 0.5f };
    Camera3D camera = {
        .target = { 0, 0, 0 }, .up = { 0, 1, 0 },
        .fovy   = 45.0f,        .projection = CAMERA_PERSPECTIVE,
    };
    orbit_update(&oc, &camera);

    static const struct { int key, cw, ccw; } BINDS[6] = {
        { KEY_U, M_U, M_Up }, { KEY_D, M_D, M_Dp },
        { KEY_L, M_L, M_Lp }, { KEY_R, M_R, M_Rp },
        { KEY_F, M_F, M_Fp }, { KEY_B, M_B, M_Bp },
    };

    while (!WindowShouldClose()) {

        /* ── input ──────────────────────────────────────────────── */
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool idle  = !anim.active && queue.count == 0;

        /* face turns → queue (record=true, handled at dequeue) */
        for (int i = 0; i < 6; i++)
            if (IsKeyPressed(BINDS[i].key))
                queue_push(&queue, shift ? BINDS[i].ccw : BINDS[i].cw);

        /* undo: only when fully idle, cursor > 0 */
        if (IsKeyPressed(KEY_Z) && idle && hist.pos > 0) {
            hist.pos--;
            anim_start(&anim, cube_inverse(hist.moves[hist.pos]), false);
        }

        /* redo: only when fully idle, cursor < len */
        if (IsKeyPressed(KEY_Y) && idle && hist.pos < hist.len) {
            anim_start(&anim, hist.moves[hist.pos], false);
            hist.pos++;
        }

        /* scramble: instant, clears history */
        if (IsKeyPressed(KEY_SPACE)) {
            srand((unsigned)(GetTime() * 1000.0));
            cube_scramble(&cube, 20);
            queue.head = queue.count = 0;
            anim.active = false;
            hist_clear(&hist);
        }

        /* reset: clears everything */
        if (IsKeyPressed(KEY_ENTER)) {
            cube_reset(&cube);
            queue.head = queue.count = 0;
            anim.active = false;
            hist_clear(&hist);
        }

        /* ── dequeue → animation ────────────────────────────────── */
        if (!anim.active && queue.count > 0)
            anim_start(&anim, queue_pop(&queue), true);

        /* ── advance animation ──────────────────────────────────── */
        anim_update(&anim, &cube, &hist);

        /* ── camera ─────────────────────────────────────────────── */
        orbit_update(&oc, &camera);

        /* ── render ─────────────────────────────────────────────── */
        BeginDrawing();
            ClearBackground((Color){ 30, 30, 30, 255 });

            BeginMode3D(camera);
                draw_cube_state(&cube,
                                anim.active ? anim.move / 3 : -1,
                                anim.active ? anim.angle    :  0.0f);
            EndMode3D();

            DrawFPS(10, 10);

            /* move counter */
            DrawText(TextFormat("Moves: %d", hist.pos), 10, 36, 18, LIGHTGRAY);

            /* undo/redo availability hint */
            if (hist.pos > 0)
                DrawText(TextFormat("Z: undo (%d)", hist.pos), 10, 58, 16, GRAY);
            if (hist.pos < hist.len)
                DrawText(TextFormat("Y: redo (%d)", hist.len - hist.pos),
                         150, 58, 16, GRAY);

            DrawText("U/D/L/R/F/B (Shift=CCW)  Z: undo  Y: redo  "
                     "SPACE: scramble  ENTER: reset  Drag: orbit  Scroll: zoom",
                     10, SCREEN_H - 26, 14, GRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
