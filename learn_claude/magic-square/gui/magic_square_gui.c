/*
 * magic_square_gui.c – Phase 6: HUD polish + timer + SOLVED banner.
 *
 * Timer: starts on the first committed user move, stops when the cube
 * becomes solved.  SPACE / ENTER reset the timer.
 *
 * Keys:
 *   U / D / L / R / F / B   – clockwise quarter-turn
 *   Shift + letter           – counter-clockwise quarter-turn
 *   Z                        – undo  (animated)
 *   Y                        – redo  (animated)
 *   SPACE                    – random 20-move scramble (resets timer)
 *   ENTER                    – reset to solved          (resets timer)
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
#define ANIM_SPEED          300.0f
#define CLICK_DRAG_THRESHOLD  5.0f

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
    bool  anim = (anim_face >= 0);
    float ax   = anim ? FACE_ROT[anim_face].ax : 0;
    float ay   = anim ? FACE_ROT[anim_face].ay : 0;
    float az   = anim ? FACE_ROT[anim_face].az : 0;
    float lift = FACE_OFF + STICKER_LIFT;

    for (int cx = -1; cx <= 1; cx++)
        for (int cy = -1; cy <= 1; cy++)
            for (int cz = -1; cz <= 1; cz++)
                if (!anim || !in_layer(anim_face, cx, cy, cz))
                    DrawCube((Vector3){(float)cx,(float)cy,(float)cz},
                             CUBIE_SIZE, CUBIE_SIZE, CUBIE_SIZE, BLACK);

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

/* ── history ─────────────────────────────────────────────────────── */
#define HIST_CAP 512

typedef struct {
    int moves[HIST_CAP];
    int len, pos;
} History;

static void hist_clear(History *h)  { h->len = h->pos = 0; }

static void hist_record(History *h, int move)
{
    h->len = h->pos;
    if (h->pos < HIST_CAP) {
        h->moves[h->pos++] = move;
        h->len = h->pos;
    }
}

/* ── mouse picking ───────────────────────────────────────────────── */
typedef struct { float drag_dist; } MousePick;

/* Returns front-facing face index 0-5 hit by ray, or -1 on miss.
 * Tests 6 axis-aligned planes at ±1.46 (= 1.0 + FACE_OFF).
 * Back-face culling guarantees only the visible face is returned. */
static int pick_face(Ray ray)
{
    static const struct { int axis; float K, ns; } PL[6] = {
        { 1, +1.46f, +1.0f },  /* U_FACE */
        { 1, -1.46f, -1.0f },  /* D_FACE */
        { 0, -1.46f, -1.0f },  /* L_FACE */
        { 0, +1.46f, +1.0f },  /* R_FACE */
        { 2, +1.46f, +1.0f },  /* F_FACE */
        { 2, -1.46f, -1.0f },  /* B_FACE */
    };
    float orig[3] = { ray.position.x,  ray.position.y,  ray.position.z  };
    float dir[3]  = { ray.direction.x, ray.direction.y, ray.direction.z };
    float best_t  = 1e30f;
    int   best    = -1;

    for (int f = 0; f < 6; f++) {
        int   a  = PL[f].axis;
        float K  = PL[f].K;
        float ns = PL[f].ns;
        if (dir[a] * ns >= 0.0f) continue;          /* back-face cull */
        float t = (K - orig[a]) / dir[a];
        if (t < 1e-4f) continue;
        float h[3] = { orig[0]+t*dir[0], orig[1]+t*dir[1], orig[2]+t*dir[2] };
        int t1 = (a+1)%3, t2 = (a+2)%3;
        if (h[t1] < -1.5f || h[t1] > 1.5f) continue;
        if (h[t2] < -1.5f || h[t2] > 1.5f) continue;
        if (t < best_t) { best_t = t; best = f; }
    }
    return best;
}

/* ── animation ───────────────────────────────────────────────────── */
typedef struct {
    int   move;
    float angle, target;
    bool  active, record;
} Anim;

static void anim_start(Anim *a, int move, bool record)
{
    int   face = move / 3;
    int   kind = move % 3;
    float cw   = FACE_ROT[face].cw;
    float targets[3] = { cw, -cw, cw * 2.0f };
    a->move   = move;
    a->angle  = 0.0f;
    a->target = targets[kind];
    a->active = true;
    a->record = record;
}

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

/* ── timer ───────────────────────────────────────────────────────
 *
 * IDLE    – not yet started (shown as "−−−")
 * RUNNING – counting up from first user move
 * STOPPED – cube solved; shows final elapsed time
 * ────────────────────────────────────────────────────────────────*/
typedef enum { TIMER_IDLE, TIMER_RUNNING, TIMER_STOPPED } TimerState;

typedef struct {
    TimerState state;
    double     start;    /* GetTime() when started */
    double     elapsed;  /* final time when solved  */
} Timer;

/* Format seconds as  M:SS.cc  or  S.cc */
static const char *fmt_time(double t)
{
    int mins = (int)(t / 60.0);
    int secs = (int)(t) % 60;
    int cs   = (int)(t * 100.0) % 100;
    if (mins > 0)
        return TextFormat("%d:%02d.%02d", mins, secs, cs);
    return TextFormat("%d.%02d", secs, cs);
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

    Cube    cube;   cube_reset(&cube);
    Queue   queue = { 0 };
    History hist  = { 0 };
    Anim    anim  = { .active = false };
    Timer     timer = { .state = TIMER_IDLE };
    MousePick mpick = { 0 };

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

        for (int i = 0; i < 6; i++)
            if (IsKeyPressed(BINDS[i].key))
                queue_push(&queue, shift ? BINDS[i].ccw : BINDS[i].cw);

        if (IsKeyPressed(KEY_Z) && idle && hist.pos > 0) {
            hist.pos--;
            anim_start(&anim, cube_inverse(hist.moves[hist.pos]), false);
        }
        if (IsKeyPressed(KEY_Y) && idle && hist.pos < hist.len) {
            anim_start(&anim, hist.moves[hist.pos], false);
            hist.pos++;
        }
        if (IsKeyPressed(KEY_SPACE)) {
            srand((unsigned)(GetTime() * 1000.0));
            cube_scramble(&cube, 20);
            queue.head = queue.count = 0;
            anim.active = false;
            hist_clear(&hist);
            timer = (Timer){ TIMER_IDLE, 0, 0 };
        }
        if (IsKeyPressed(KEY_ENTER)) {
            cube_reset(&cube);
            queue.head = queue.count = 0;
            anim.active = false;
            hist_clear(&hist);
            timer = (Timer){ TIMER_IDLE, 0, 0 };
        }

        /* ── mouse picking ──────────────────────────────────────── */
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            mpick.drag_dist = 0.0f;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta();
            mpick.drag_dist += sqrtf(d.x*d.x + d.y*d.y);
        }
        if (idle) {
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)
                && mpick.drag_dist < CLICK_DRAG_THRESHOLD) {
                int f = pick_face(GetMouseRay(GetMousePosition(), camera));
                if (f >= 0) queue_push(&queue, f * 3);
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
                int f = pick_face(GetMouseRay(GetMousePosition(), camera));
                if (f >= 0) queue_push(&queue, f * 3 + 1);
            }
        }

        /* ── dequeue → animation ────────────────────────────────── */
        if (!anim.active && queue.count > 0)
            anim_start(&anim, queue_pop(&queue), true);

        /* ── advance animation; detect commit ───────────────────── */
        bool was_active = anim.active;
        bool was_record = anim.record;
        anim_update(&anim, &cube, &hist);
        bool just_committed = was_active && !anim.active;

        /* start timer on first committed user move */
        if (just_committed && was_record && timer.state == TIMER_IDLE)
            timer = (Timer){ TIMER_RUNNING, GetTime(), 0 };

        /* stop timer when cube becomes solved */
        if (timer.state == TIMER_RUNNING && cube_is_solved(&cube))
            timer = (Timer){ TIMER_STOPPED, timer.start,
                             GetTime() - timer.start };

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

            /* ── HUD ─────────────────────────────────────────────── */
            DrawFPS(10, 10);

            /* move counter */
            DrawText(TextFormat("Moves: %d", hist.pos), 10, 36, 20, LIGHTGRAY);

            /* timer – top right */
            {
                const char *ts;
                Color       tc;
                if (timer.state == TIMER_IDLE) {
                    ts = "---";
                    tc = DARKGRAY;
                } else if (timer.state == TIMER_RUNNING) {
                    ts = fmt_time(GetTime() - timer.start);
                    tc = WHITE;
                } else {
                    ts = fmt_time(timer.elapsed);
                    tc = (Color){ 80, 220, 80, 255 };  /* green */
                }
                int tw = MeasureText(ts, 28);
                DrawText(ts, SCREEN_W - tw - 12, 10, 28, tc);
            }

            /* SOLVED! banner – centre screen */
            if (cube_is_solved(&cube) && timer.state == TIMER_STOPPED) {
                const char *s1 = "SOLVED!";
                const char *s2 = fmt_time(timer.elapsed);
                int w1 = MeasureText(s1, 72);
                int w2 = MeasureText(s2, 36);
                int bx = (SCREEN_W - w1) / 2 - 20;
                int bw = w1 + 40;
                DrawRectangle(bx, SCREEN_H/2 - 72, bw, 120,
                              (Color){ 0, 0, 0, 180 });
                DrawText(s1, (SCREEN_W - w1) / 2, SCREEN_H/2 - 66, 72, YELLOW);
                DrawText(s2, (SCREEN_W - w2) / 2, SCREEN_H/2 + 16, 36, WHITE);
            }

            /* undo / redo availability */
            if (hist.pos > 0)
                DrawText(TextFormat("Z undo ×%d", hist.pos),
                         10, 62, 14, DARKGRAY);
            if (hist.pos < hist.len)
                DrawText(TextFormat("Y redo ×%d", hist.len - hist.pos),
                         130, 62, 14, DARKGRAY);

            /* bottom legend */
            DrawText("U/D/L/R/F/B (Shift=CCW)  Z: undo  Y: redo  "
                     "SPACE: scramble  ENTER: reset  "
                     "LClick: CW  RClick: CCW  Drag: orbit  Scroll: zoom",
                     10, SCREEN_H - 24, 13, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
