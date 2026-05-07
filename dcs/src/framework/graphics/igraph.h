#ifndef DCS_FW_IGRAPH_H
#define DCS_FW_IGRAPH_H

#include "../core/oo.h"
#include "../core/rect.h"
#include <stdint.h>

/* igraph: thin wrapper around the underlying graphics/window/input library
   (raylib for now; Cairo or nanoVG could replace it). Application code never
   includes raylib.h directly — only graph_raylib.c does. */

/* ── input enums (abstract codes; mapped to backend codes inside the impl) ── */

typedef enum {
    IK_NONE = 0,
    IK_A, IK_S, IK_N, IK_O, IK_R, IK_F,         /* alphabetic keys we use   */
    IK_ESCAPE, IK_DELETE, IK_TAB, IK_ENTER,
    IK_LEFT_CTRL, IK_RIGHT_CTRL,
    IK_LEFT_SHIFT, IK_RIGHT_SHIFT,
    IK_UP, IK_DOWN, IK_LEFT, IK_RIGHT,
    IK__COUNT
} igraph_key_t;

typedef enum {
    IM_LEFT = 0,
    IM_RIGHT,
    IM_MIDDLE,
} igraph_mouse_btn_t;

typedef enum {
    CURSOR_DEFAULT = 0,
    CURSOR_HAND,
    CURSOR_NS_RESIZE,
    CURSOR_EW_RESIZE,
    CURSOR_TEXT,
} cursor_kind_t;

/* ── interface ──────────────────────────────────────────────────────────── */

interface tagt_igraph {
    void *self;

    /* lifecycle */
    int   (*init)         (void *self, int w, int h, const char *title);
    void  (*shutdown)     (void *self);
    int   (*should_close) (void *self);
    void  (*begin_frame)  (void *self);
    void  (*end_frame)    (void *self);
    void  (*screen_size)  (void *self, int *w, int *h);

    /* drawing — colors are 0xRRGGBBAA (see core/color.h) */
    void  (*draw_rect)         (void *self, rect_t r, uint32_t color);
    void  (*draw_rect_lines)   (void *self, rect_t r, float thick, uint32_t color);
    void  (*draw_line)         (void *self, vec2_t a, vec2_t b, float thick, uint32_t color);
    void  (*draw_circle)       (void *self, vec2_t c, float r, uint32_t color);
    void  (*draw_circle_lines) (void *self, vec2_t c, float r, float thick, uint32_t color);
    void  (*draw_text)         (void *self, const char *s, vec2_t pos, float size, uint32_t color);
    float (*measure_text)      (void *self, const char *s, float size);

    /* input querying */
    vec2_t (*mouse_position)   (void *self);
    vec2_t (*mouse_delta)      (void *self);
    int    (*mouse_down)       (void *self, igraph_mouse_btn_t btn);
    int    (*mouse_pressed)    (void *self, igraph_mouse_btn_t btn);
    int    (*mouse_released)   (void *self, igraph_mouse_btn_t btn);
    float  (*mouse_wheel)      (void *self);
    int    (*key_down)         (void *self, igraph_key_t key);
    int    (*key_pressed)      (void *self, igraph_key_t key);

    /* 2D camera (pan/zoom). Phase 2.2 supports a single active camera —
       multiple pushes overwrite the previous (sufficient for DCS). */
    void  (*push_camera2d)  (void *self, vec2_t target, vec2_t offset, float zoom);
    void  (*pop_camera2d)   (void *self);
    vec2_t(*screen_to_world)(void *self, vec2_t screen);
    vec2_t(*world_to_screen)(void *self, vec2_t world);

    /* clipping */
    void  (*push_scissor)   (void *self, rect_t r);
    void  (*pop_scissor)    (void *self);

    /* mouse cursor */
    void  (*set_cursor)     (void *self, cursor_kind_t kind);
};
typedef interface tagt_igraph igraph_t;

/* Returns the graphics singleton for this build. */
igraph_t *graph_create(void);

#endif /* DCS_FW_IGRAPH_H */
