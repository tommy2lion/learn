#include "panel.h"
#include <stdlib.h>
#include <string.h>

#define INIT_CHILD_CAP 8

static void panel_draw(widget_t *self, igraph_t *g) {
    panel_t *p = (panel_t *)self;
    if (p->bg_color)
        g->draw_rect(g->self, p->base.bounds, p->bg_color);
    if (p->border_color && p->border_thick > 0)
        g->draw_rect_lines(g->self, p->base.bounds, p->border_thick, p->border_color);
    for (int i = 0; i < p->child_count; i++)
        widget_draw(p->children[i], g);
}

static int panel_handle_event(widget_t *self, const event_t *ev) {
    /* Panel itself doesn't consume events; the frame's dispatch_mouse
       descended into us and only ended up here if no child handled. */
    (void)self; (void)ev;
    return 0;
}

static int      panel_child_count(widget_t *self) { return ((panel_t *)self)->child_count; }
static widget_t *panel_child_at  (widget_t *self, int i) {
    panel_t *p = (panel_t *)self;
    if (i < 0 || i >= p->child_count) return NULL;
    return p->children[i];
}

static void panel_destroy(widget_t *self) {
    panel_t *p = (panel_t *)self;
    for (int i = 0; i < p->child_count; i++)
        widget_destroy(p->children[i]);
    free(p->children);
    free(p);
}

static const widget_vt_t PANEL_VT = {
    .draw         = panel_draw,
    .handle_event = panel_handle_event,
    .destroy      = panel_destroy,
    .child_count  = panel_child_count,
    .child_at     = panel_child_at,
};

panel_t *panel_create(rect_t bounds) {
    panel_t *p = (panel_t *)calloc(1, sizeof(panel_t));
    if (!p) return NULL;
    p->base.vt      = &PANEL_VT;
    p->base.bounds  = bounds;
    p->base.visible = 1;
    p->children     = (widget_t **)malloc(INIT_CHILD_CAP * sizeof(widget_t *));
    p->child_cap    = INIT_CHILD_CAP;
    return p;
}

void panel_set_background(panel_t *self, uint32_t color) { self->bg_color = color; }
void panel_set_border    (panel_t *self, uint32_t color, float thick) {
    self->border_color = color;
    self->border_thick = thick;
}

void panel_add_child(panel_t *self, widget_t *child) {
    if (!child) return;
    if (self->child_count >= self->child_cap) {
        int nc = self->child_cap * 2;
        widget_t **nn = (widget_t **)realloc(self->children, nc * sizeof(widget_t *));
        if (!nn) return;
        self->children = nn;
        self->child_cap = nc;
    }
    self->children[self->child_count++] = child;
    child->parent = &self->base;
}
