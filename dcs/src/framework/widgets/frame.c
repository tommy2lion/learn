#include "frame.h"
#include <string.h>

/* ── helpers ───────────────────────────────────────────────────────── */

static unsigned current_mods(igraph_t *g) {
    unsigned m = 0;
    if (g->key_down(g->self, IK_LEFT_CTRL)  || g->key_down(g->self, IK_RIGHT_CTRL))  m |= MOD_CTRL;
    if (g->key_down(g->self, IK_LEFT_SHIFT) || g->key_down(g->self, IK_RIGHT_SHIFT)) m |= MOD_SHIFT;
    return m;
}

/* Recursive depth-first hit-test + dispatch for mouse events. Returns 1 if
   any widget consumed the event. */
static int dispatch_mouse(widget_t *w, const event_t *ev) {
    if (!w || !w->visible) return 0;
    if (!widget_hits(w, ev->mouse.pos)) return 0;

    /* try children top-to-bottom (last child = top) */
    int n = widget_child_count(w);
    for (int i = n - 1; i >= 0; i--) {
        widget_t *c = widget_child_at(w, i);
        if (dispatch_mouse(c, ev)) return 1;
    }
    /* fall through to self */
    return widget_handle_event(w, ev);
}

/* Mouse-move and mouse-wheel are broadcast to the widget under the cursor.
   We use the same depth-first descent. */
static int dispatch_mouse_at(widget_t *w, vec2_t pos, const event_t *ev) {
    if (!w || !w->visible) return 0;
    if (pos.x < w->bounds.x || pos.x >= w->bounds.x + w->bounds.w ||
        pos.y < w->bounds.y || pos.y >= w->bounds.y + w->bounds.h) return 0;
    int n = widget_child_count(w);
    for (int i = n - 1; i >= 0; i--) {
        widget_t *c = widget_child_at(w, i);
        if (dispatch_mouse_at(c, pos, ev)) return 1;
    }
    return widget_handle_event(w, ev);
}

/* ── public ────────────────────────────────────────────────────────── */

void frame_init(frame_t *self, igraph_t *g, iplatform_t *p, widget_t *root) {
    memset(self, 0, sizeof(*self));
    self->graph    = g;
    self->platform = p;
    self->root     = root;
    focus_manager_init(&self->focus);
    quit_manager_init (&self->quit);
}

focus_manager_t *frame_focus(frame_t *self) { return &self->focus; }
quit_manager_t  *frame_quit (frame_t *self) { return &self->quit;  }

void frame_tick(frame_t *self) {
    igraph_t *g = self->graph;

    /* Reset cursor each tick — widgets opt into NS_RESIZE / EW_RESIZE / etc.
       only while actively hovered. */
    g->set_cursor(g->self, CURSOR_DEFAULT);

    /* ── poll input and synthesise events ──────────────────────── */
    vec2_t   mp   = g->mouse_position(g->self);
    unsigned mods = current_mods(g);

    /* Mouse buttons — emit DOWN every frame held + edge events. Drives
       hover/hit tracking in widgets. */
    for (int b = 0; b < 3; b++) {
        igraph_mouse_btn_t btn = (igraph_mouse_btn_t)b;
        if (g->mouse_pressed(g->self, btn)) {
            event_t ev = { .kind = EV_MOUSE_PRESS, .mouse = { mp, btn, mods } };
            dispatch_mouse(self->root, &ev);
        }
        if (g->mouse_released(g->self, btn)) {
            event_t ev = { .kind = EV_MOUSE_RELEASE, .mouse = { mp, btn, mods } };
            dispatch_mouse(self->root, &ev);
        }
    }

    /* Mouse move (always — widgets that track hover need this). */
    {
        event_t ev = { .kind = EV_MOUSE_MOVE, .mouse = { mp, IM_LEFT, mods } };
        dispatch_mouse_at(self->root, mp, &ev);
    }

    /* Mouse wheel. */
    float wheel = g->mouse_wheel(g->self);
    if (wheel != 0.0f) {
        event_t ev = { .kind = EV_MOUSE_WHEEL };
        ev.wheel.dy  = wheel;
        ev.wheel.pos = mp;
        dispatch_mouse_at(self->root, mp, &ev);
    }

    /* Keyboard — route to focused widget. (We only emit edge events here;
       widgets that need held state should query igraph directly.) */
    {
        widget_t *focused = focus_manager_get(&self->focus);
        if (focused) {
            for (int k = 1; k < IK__COUNT; k++) {
                if (g->key_pressed(g->self, (igraph_key_t)k)) {
                    event_t ev = { .kind = EV_KEY_PRESS };
                    ev.key.key  = (igraph_key_t)k;
                    ev.key.mods = mods;
                    widget_handle_event(focused, &ev);
                }
            }
        }
    }

    /* ── render ─────────────────────────────────────────────────── */
    g->begin_frame(g->self);
    widget_draw(self->root, g);
    g->end_frame(g->self);
}

void frame_run(frame_t *self) {
    while (!quit_manager_should_quit(&self->quit, self->graph)) {
        frame_tick(self);
    }
}

void frame_shutdown(frame_t *self) {
    if (self->root) {
        widget_destroy(self->root);
        self->root = NULL;
    }
}
