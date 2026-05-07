#include "igraph.h"
#include "../core/color.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

/* ── color conversion (0xRRGGBBAA → raylib Color) ─────────────────── */

static Color rgba_to_color(uint32_t c) {
    Color out = {
        (unsigned char)COLOR_R(c),
        (unsigned char)COLOR_G(c),
        (unsigned char)COLOR_B(c),
        (unsigned char)COLOR_A(c),
    };
    return out;
}

/* ── key/button mapping ───────────────────────────────────────────── */

static int rl_key(igraph_key_t ik) {
    switch (ik) {
        case IK_A: return KEY_A;
        case IK_S: return KEY_S;
        case IK_N: return KEY_N;
        case IK_O: return KEY_O;
        case IK_R: return KEY_R;
        case IK_F: return KEY_F;
        case IK_ESCAPE:      return KEY_ESCAPE;
        case IK_DELETE:      return KEY_DELETE;
        case IK_TAB:         return KEY_TAB;
        case IK_ENTER:       return KEY_ENTER;
        case IK_LEFT_CTRL:   return KEY_LEFT_CONTROL;
        case IK_RIGHT_CTRL:  return KEY_RIGHT_CONTROL;
        case IK_LEFT_SHIFT:  return KEY_LEFT_SHIFT;
        case IK_RIGHT_SHIFT: return KEY_RIGHT_SHIFT;
        case IK_UP:          return KEY_UP;
        case IK_DOWN:        return KEY_DOWN;
        case IK_LEFT:        return KEY_LEFT;
        case IK_RIGHT:       return KEY_RIGHT;
        default:             return 0;
    }
}

static int rl_mouse_btn(igraph_mouse_btn_t b) {
    switch (b) {
        case IM_LEFT:   return MOUSE_BUTTON_LEFT;
        case IM_RIGHT:  return MOUSE_BUTTON_RIGHT;
        case IM_MIDDLE: return MOUSE_BUTTON_MIDDLE;
    }
    return MOUSE_BUTTON_LEFT;
}

static int rl_cursor_kind(cursor_kind_t k) {
    switch (k) {
        case CURSOR_DEFAULT:   return MOUSE_CURSOR_DEFAULT;
        case CURSOR_HAND:      return MOUSE_CURSOR_POINTING_HAND;
        case CURSOR_NS_RESIZE: return MOUSE_CURSOR_RESIZE_NS;
        case CURSOR_EW_RESIZE: return MOUSE_CURSOR_RESIZE_EW;
        case CURSOR_TEXT:      return MOUSE_CURSOR_IBEAM;
    }
    return MOUSE_CURSOR_DEFAULT;
}

/* ── transient state (single-camera, single-scissor for now) ──────── */

static Camera2D g_cam;
static int      g_has_cam = 0;
static int      g_has_scissor = 0;

/* ── lifecycle ────────────────────────────────────────────────────── */

static int rl_init(void *self, int w, int h, const char *title) {
    (void)self;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(w, h, title);
    SetTargetFPS(60);
    return IsWindowReady() ? 0 : -1;
}

static void rl_shutdown(void *self) {
    (void)self;
    if (g_has_cam)     { EndMode2D();        g_has_cam = 0; }
    if (g_has_scissor) { EndScissorMode();   g_has_scissor = 0; }
    CloseWindow();
}

static int  rl_should_close(void *self) { (void)self; return WindowShouldClose(); }
static void rl_begin_frame (void *self) { (void)self; BeginDrawing(); ClearBackground(rgba_to_color(COLOR_BG)); }
static void rl_end_frame   (void *self) { (void)self; EndDrawing(); }

static void rl_screen_size(void *self, int *w, int *h) {
    (void)self;
    if (w) *w = GetScreenWidth();
    if (h) *h = GetScreenHeight();
}

/* ── drawing primitives ───────────────────────────────────────────── */

static void rl_draw_rect(void *self, rect_t r, uint32_t color) {
    (void)self;
    DrawRectangle((int)r.x, (int)r.y, (int)r.w, (int)r.h, rgba_to_color(color));
}

static void rl_draw_rect_lines(void *self, rect_t r, float thick, uint32_t color) {
    (void)self;
    Rectangle rr = { r.x, r.y, r.w, r.h };
    DrawRectangleLinesEx(rr, thick, rgba_to_color(color));
}

static void rl_draw_line(void *self, vec2_t a, vec2_t b, float thick, uint32_t color) {
    (void)self;
    DrawLineEx((Vector2){a.x, a.y}, (Vector2){b.x, b.y}, thick, rgba_to_color(color));
}

static void rl_draw_circle(void *self, vec2_t c, float r, uint32_t color) {
    (void)self;
    DrawCircleV((Vector2){c.x, c.y}, r, rgba_to_color(color));
}

static void rl_draw_circle_lines(void *self, vec2_t c, float r, float thick, uint32_t color) {
    (void)self;
    /* raylib's DrawCircleLines doesn't take thickness; emulate with DrawRing */
    DrawRing((Vector2){c.x, c.y}, r - thick * 0.5f, r + thick * 0.5f, 0, 360, 32,
             rgba_to_color(color));
}

static void rl_draw_text(void *self, const char *s, vec2_t pos, float size, uint32_t color) {
    (void)self;
    DrawText(s, (int)pos.x, (int)pos.y, (int)size, rgba_to_color(color));
}

static float rl_measure_text(void *self, const char *s, float size) {
    (void)self;
    return (float)MeasureText(s, (int)size);
}

/* ── input ────────────────────────────────────────────────────────── */

static vec2_t rl_mouse_position(void *self) {
    (void)self;
    Vector2 p = GetMousePosition();
    return (vec2_t){p.x, p.y};
}
static vec2_t rl_mouse_delta(void *self) {
    (void)self;
    Vector2 d = GetMouseDelta();
    return (vec2_t){d.x, d.y};
}
static int   rl_mouse_down    (void *self, igraph_mouse_btn_t b) { (void)self; return IsMouseButtonDown    (rl_mouse_btn(b)); }
static int   rl_mouse_pressed (void *self, igraph_mouse_btn_t b) { (void)self; return IsMouseButtonPressed (rl_mouse_btn(b)); }
static int   rl_mouse_released(void *self, igraph_mouse_btn_t b) { (void)self; return IsMouseButtonReleased(rl_mouse_btn(b)); }
static float rl_mouse_wheel   (void *self)                       { (void)self; return GetMouseWheelMove(); }
static int   rl_key_down      (void *self, igraph_key_t k)       { (void)self; return IsKeyDown   (rl_key(k)); }
static int   rl_key_pressed   (void *self, igraph_key_t k)       { (void)self; return IsKeyPressed(rl_key(k)); }

/* ── camera ───────────────────────────────────────────────────────── */

static void rl_push_camera2d(void *self, vec2_t target, vec2_t offset, float zoom) {
    (void)self;
    if (g_has_cam) EndMode2D();
    g_cam = (Camera2D){
        .offset   = (Vector2){offset.x, offset.y},
        .target   = (Vector2){target.x, target.y},
        .rotation = 0.0f,
        .zoom     = zoom,
    };
    BeginMode2D(g_cam);
    g_has_cam = 1;
}

static void rl_pop_camera2d(void *self) {
    (void)self;
    if (g_has_cam) { EndMode2D(); g_has_cam = 0; }
}

static vec2_t rl_screen_to_world(void *self, vec2_t s) {
    (void)self;
    if (!g_has_cam) return s;
    Vector2 w = GetScreenToWorld2D((Vector2){s.x, s.y}, g_cam);
    return (vec2_t){w.x, w.y};
}

static vec2_t rl_world_to_screen(void *self, vec2_t w) {
    (void)self;
    if (!g_has_cam) return w;
    Vector2 s = GetWorldToScreen2D((Vector2){w.x, w.y}, g_cam);
    return (vec2_t){s.x, s.y};
}

/* ── scissor ──────────────────────────────────────────────────────── */

static void rl_push_scissor(void *self, rect_t r) {
    (void)self;
    if (g_has_scissor) EndScissorMode();
    BeginScissorMode((int)r.x, (int)r.y, (int)r.w, (int)r.h);
    g_has_scissor = 1;
}

static void rl_pop_scissor(void *self) {
    (void)self;
    if (g_has_scissor) { EndScissorMode(); g_has_scissor = 0; }
}

/* ── cursor ───────────────────────────────────────────────────────── */

static void rl_set_cursor(void *self, cursor_kind_t kind) {
    (void)self;
    SetMouseCursor(rl_cursor_kind(kind));
}

/* ── singleton ────────────────────────────────────────────────────── */

static igraph_t g_graph = {
    .self              = NULL,
    .init              = rl_init,
    .shutdown          = rl_shutdown,
    .should_close      = rl_should_close,
    .begin_frame       = rl_begin_frame,
    .end_frame         = rl_end_frame,
    .screen_size       = rl_screen_size,

    .draw_rect         = rl_draw_rect,
    .draw_rect_lines   = rl_draw_rect_lines,
    .draw_line         = rl_draw_line,
    .draw_circle       = rl_draw_circle,
    .draw_circle_lines = rl_draw_circle_lines,
    .draw_text         = rl_draw_text,
    .measure_text      = rl_measure_text,

    .mouse_position    = rl_mouse_position,
    .mouse_delta       = rl_mouse_delta,
    .mouse_down        = rl_mouse_down,
    .mouse_pressed     = rl_mouse_pressed,
    .mouse_released    = rl_mouse_released,
    .mouse_wheel       = rl_mouse_wheel,
    .key_down          = rl_key_down,
    .key_pressed       = rl_key_pressed,

    .push_camera2d     = rl_push_camera2d,
    .pop_camera2d      = rl_pop_camera2d,
    .screen_to_world   = rl_screen_to_world,
    .world_to_screen   = rl_world_to_screen,

    .push_scissor      = rl_push_scissor,
    .pop_scissor       = rl_pop_scissor,

    .set_cursor        = rl_set_cursor,
};

igraph_t *graph_create(void) {
    return &g_graph;
}
