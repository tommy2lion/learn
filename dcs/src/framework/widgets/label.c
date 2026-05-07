#include "label.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void label_draw(widget_t *self, igraph_t *g) {
    label_t *l = (label_t *)self;
    float tw = g->measure_text(g->self, l->text, l->font_size);
    float tx;
    switch (l->align) {
        case LABEL_ALIGN_CENTER: tx = l->base.bounds.x + (l->base.bounds.w - tw) * 0.5f; break;
        case LABEL_ALIGN_RIGHT:  tx = l->base.bounds.x + l->base.bounds.w - tw - 4.0f;   break;
        default:                 tx = l->base.bounds.x + 4.0f;                            break;
    }
    float ty = l->base.bounds.y + (l->base.bounds.h - l->font_size) * 0.5f;
    g->draw_text(g->self, l->text, (vec2_t){tx, ty}, l->font_size, l->color);
}

static int  label_handle_event(widget_t *self, const event_t *ev) { (void)self; (void)ev; return 0; }
static void label_destroy     (widget_t *self) { free(self); }

static const widget_vt_t LABEL_VT = {
    .draw         = label_draw,
    .handle_event = label_handle_event,
    .destroy      = label_destroy,
};

label_t *label_create(rect_t bounds, const char *text, float size, uint32_t color) {
    label_t *l = (label_t *)calloc(1, sizeof(label_t));
    if (!l) return NULL;
    l->base.vt      = &LABEL_VT;
    l->base.bounds  = bounds;
    l->base.visible = 1;
    snprintf(l->text, sizeof(l->text), "%s", text ? text : "");
    l->font_size = size;
    l->color     = color;
    l->align     = LABEL_ALIGN_LEFT;
    return l;
}

void label_set_text(label_t *self, const char *text) {
    snprintf(self->text, sizeof(self->text), "%s", text ? text : "");
}

void label_set_align(label_t *self, label_align_t a) { self->align = a; }
