#include "splitter.h"
#include "../core/color.h"
#include <stdlib.h>

#define HOVER_PAD 5.0f   /* extra px each side of divider that counts as "hover" */

/* ── geometry helpers ────────────────────────────────────────────── */

static rect_t divider_rect(const splitter_t *s) {
    if (s->vertical) {
        return (rect_t){
            s->base.bounds.x,
            s->base.bounds.y + s->first_size,
            s->base.bounds.w,
            (float)s->divider_thick,
        };
    } else {
        return (rect_t){
            s->base.bounds.x + s->first_size,
            s->base.bounds.y,
            (float)s->divider_thick,
            s->base.bounds.h,
        };
    }
}

static int over_divider(const splitter_t *s, vec2_t pt) {
    rect_t d = divider_rect(s);
    if (s->vertical)
        return pt.x >= d.x && pt.x < d.x + d.w
            && pt.y >= d.y - HOVER_PAD && pt.y < d.y + d.h + HOVER_PAD;
    return pt.x >= d.x - HOVER_PAD && pt.x < d.x + d.w + HOVER_PAD
        && pt.y >= d.y && pt.y < d.y + d.h;
}

static void clamp_first_size(splitter_t *s) {
    int total = (int)(s->vertical ? s->base.bounds.h : s->base.bounds.w) - s->divider_thick;
    int min_f = s->min_first;
    int max_f = total - s->min_second;
    if (max_f < min_f) max_f = min_f;
    if (s->first_size < min_f) s->first_size = min_f;
    if (s->first_size > max_f) s->first_size = max_f;
}

static void layout_children(splitter_t *s) {
    clamp_first_size(s);
    if (s->vertical) {
        if (s->first) {
            s->first->bounds = (rect_t){
                s->base.bounds.x, s->base.bounds.y,
                s->base.bounds.w, (float)s->first_size,
            };
        }
        if (s->second) {
            float y2 = s->base.bounds.y + s->first_size + s->divider_thick;
            s->second->bounds = (rect_t){
                s->base.bounds.x, y2,
                s->base.bounds.w, s->base.bounds.y + s->base.bounds.h - y2,
            };
        }
    } else {
        if (s->first) {
            s->first->bounds = (rect_t){
                s->base.bounds.x, s->base.bounds.y,
                (float)s->first_size, s->base.bounds.h,
            };
        }
        if (s->second) {
            float x2 = s->base.bounds.x + s->first_size + s->divider_thick;
            s->second->bounds = (rect_t){
                x2, s->base.bounds.y,
                s->base.bounds.x + s->base.bounds.w - x2, s->base.bounds.h,
            };
        }
    }
}

/* ── widget vtable ───────────────────────────────────────────────── */

static void splitter_draw(widget_t *self, igraph_t *g) {
    splitter_t *s = (splitter_t *)self;
    layout_children(s);

    /* Track drag state in draw — the press handler set s->dragging=1; here we
       update the size as the cursor moves and clear when the button released. */
    vec2_t mp = g->mouse_position(g->self);
    if (s->dragging) {
        if (g->mouse_down(g->self, IM_LEFT)) {
            int new_size = s->vertical
                ? (int)(mp.y - s->base.bounds.y) - s->divider_thick / 2
                : (int)(mp.x - s->base.bounds.x) - s->divider_thick / 2;
            s->first_size = new_size;
            clamp_first_size(s);
        } else {
            s->dragging = 0;
        }
    }

    widget_draw(s->first,  g);
    widget_draw(s->second, g);

    int hovered = over_divider(s, mp);
    rect_t dr = divider_rect(s);
    g->draw_rect(g->self, dr,
                 (hovered || s->dragging) ? s->divider_hover_color : s->divider_color);

    if (hovered || s->dragging)
        g->set_cursor(g->self, s->vertical ? CURSOR_NS_RESIZE : CURSOR_EW_RESIZE);
}

static int splitter_handle_event(widget_t *self, const event_t *ev) {
    splitter_t *s = (splitter_t *)self;
    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_LEFT) {
        if (over_divider(s, ev->mouse.pos)) {
            s->dragging = 1;
            return 1;
        }
    }
    if (ev->kind == EV_MOUSE_RELEASE && ev->mouse.btn == IM_LEFT) {
        if (s->dragging) { s->dragging = 0; return 1; }
    }
    return 0;
}

static int      splitter_child_count(widget_t *self) {
    splitter_t *s = (splitter_t *)self;
    return (s->first ? 1 : 0) + (s->second ? 1 : 0);
}

static widget_t *splitter_child_at(widget_t *self, int idx) {
    splitter_t *s = (splitter_t *)self;
    if (idx == 0) return s->first ? s->first : s->second;
    if (idx == 1 && s->first) return s->second;
    return NULL;
}

static void splitter_destroy(widget_t *self) {
    splitter_t *s = (splitter_t *)self;
    if (s->first)  widget_destroy(s->first);
    if (s->second) widget_destroy(s->second);
    free(s);
}

static const widget_vt_t SPLITTER_VT = {
    .draw         = splitter_draw,
    .handle_event = splitter_handle_event,
    .destroy      = splitter_destroy,
    .child_count  = splitter_child_count,
    .child_at     = splitter_child_at,
};

splitter_t *splitter_create(rect_t bounds, int vertical, int initial_first_size) {
    splitter_t *s = (splitter_t *)calloc(1, sizeof(splitter_t));
    if (!s) return NULL;
    s->base.vt      = &SPLITTER_VT;
    s->base.bounds  = bounds;
    s->base.visible = 1;
    s->vertical     = vertical;
    s->first_size   = initial_first_size;
    s->divider_thick = 6;
    s->min_first    = 40;
    s->min_second   = 40;
    s->divider_color       = COLOR_LIGHTGRAY;
    s->divider_hover_color = COLOR_BLUE;
    return s;
}

void splitter_set_children(splitter_t *self, widget_t *first, widget_t *second) {
    self->first  = first;  if (first)  first->parent  = &self->base;
    self->second = second; if (second) second->parent = &self->base;
    layout_children(self);
}

void splitter_set_min_sizes(splitter_t *self, int mf, int ms) {
    self->min_first  = mf;
    self->min_second = ms;
}

void splitter_set_first_size(splitter_t *self, int size) {
    self->first_size = size;
}

int splitter_get_first_size(const splitter_t *self) { return self->first_size; }
