#include "divider_widget.h"
#include "../framework/core/color.h"
#include <stdlib.h>

#define LINE_THICK 2

static int hits_widget(const widget_t *w, vec2_t pt) {
    return pt.x >= w->bounds.x && pt.x < w->bounds.x + w->bounds.w
        && pt.y >= w->bounds.y && pt.y < w->bounds.y + w->bounds.h;
}

static void dw_draw(widget_t *self, igraph_t *g) {
    divider_widget_t *d = (divider_widget_t *)self;
    vec2_t mp = g->mouse_position(g->self);
    int hovered = hits_widget(self, mp);

    /* Track drag state — handle_event set dragging=1 on PRESS over us;
       here, while the button is held, we report new coordinates each frame
       even if the cursor leaves our bounds (matches splitter behavior). */
    if (d->dragging) {
        if (g->mouse_down(g->self, IM_LEFT)) {
            int v = (d->orient == DIVIDER_HORIZONTAL) ? (int)mp.y : (int)mp.x;
            if (v < d->value_min) v = d->value_min;
            if (v > d->value_max) v = d->value_max;
            if (d->on_change) d->on_change(v, d->user);
        } else {
            d->dragging = 0;
        }
    }

    /* Draw a thin line in the centre of the hover band. */
    rect_t line;
    if (d->orient == DIVIDER_HORIZONTAL) {
        line = (rect_t){
            self->bounds.x,
            self->bounds.y + (self->bounds.h - LINE_THICK) * 0.5f,
            self->bounds.w, LINE_THICK,
        };
    } else {
        line = (rect_t){
            self->bounds.x + (self->bounds.w - LINE_THICK) * 0.5f,
            self->bounds.y,
            LINE_THICK, self->bounds.h,
        };
    }
    uint32_t color = (hovered || d->dragging) ? COLOR_BLUE : 0xAAAAAAFFu;
    g->draw_rect(g->self, line, color);

    if (hovered || d->dragging) {
        g->set_cursor(g->self,
                      d->orient == DIVIDER_HORIZONTAL ? CURSOR_NS_RESIZE : CURSOR_EW_RESIZE);
    }
}

static int dw_handle_event(widget_t *self, const event_t *ev) {
    divider_widget_t *d = (divider_widget_t *)self;
    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_LEFT
        && hits_widget(self, ev->mouse.pos)) {
        d->dragging = 1;
        return 1;
    }
    if (ev->kind == EV_MOUSE_RELEASE && d->dragging) {
        d->dragging = 0;
        return 1;
    }
    /* swallow mouse events while dragging so the canvas underneath doesn't
       see them (e.g., starting an unrelated wire) */
    if (d->dragging) return 1;
    return 0;
}

static void dw_destroy(widget_t *self) { free(self); }

static const widget_vt_t DW_VT = {
    .draw         = dw_draw,
    .handle_event = dw_handle_event,
    .destroy      = dw_destroy,
};

divider_widget_t *divider_widget_create(rect_t hover_bounds, divider_orient_t orient) {
    divider_widget_t *d = (divider_widget_t *)calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->base.vt      = &DW_VT;
    d->base.bounds  = hover_bounds;
    d->base.visible = 1;
    d->orient       = orient;
    d->value_min    = 0;
    d->value_max    = 1 << 30;
    return d;
}

void divider_widget_set_range(divider_widget_t *self, int mn, int mx) {
    self->value_min = mn;
    self->value_max = mx;
}

void divider_widget_set_change_cb(divider_widget_t *self,
                                  divider_change_fn_t cb, void *user) {
    self->on_change = cb;
    self->user      = user;
}
