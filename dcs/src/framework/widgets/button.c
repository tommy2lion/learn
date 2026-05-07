#include "button.h"
#include "../core/color.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void button_draw(widget_t *self, igraph_t *g) {
    button_t *b = (button_t *)self;
    /* refresh hover from current mouse pos so we don't depend on event flow */
    vec2_t mp = g->mouse_position(g->self);
    b->hovered = widget_hits(self, mp);

    uint32_t bg = b->pressed ? b->color_active
                : b->hovered ? b->color_hover
                             : b->color_normal;
    g->draw_rect      (g->self, b->base.bounds, bg);
    g->draw_rect_lines(g->self, b->base.bounds, 1.0f, b->color_border);

    float tw = g->measure_text(g->self, b->label, b->font_size);
    vec2_t tp = {
        b->base.bounds.x + (b->base.bounds.w - tw) * 0.5f,
        b->base.bounds.y + (b->base.bounds.h - b->font_size) * 0.5f,
    };
    g->draw_text(g->self, b->label, tp, b->font_size, b->color_text);
}

static int button_handle_event(widget_t *self, const event_t *ev) {
    button_t *b = (button_t *)self;
    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_LEFT) {
        b->pressed = 1;
        return 1;
    }
    if (ev->kind == EV_MOUSE_RELEASE && ev->mouse.btn == IM_LEFT) {
        if (b->pressed && widget_hits(self, ev->mouse.pos)) {
            b->pressed = 0;
            if (b->on_click) b->on_click(b->click_user);
        } else {
            b->pressed = 0;
        }
        return 1;
    }
    return 0;
}

static void button_destroy(widget_t *self) { free(self); }

static const widget_vt_t BUTTON_VT = {
    .draw         = button_draw,
    .handle_event = button_handle_event,
    .destroy      = button_destroy,
};

button_t *button_create(rect_t bounds, const char *label,
                        button_on_click_t on_click, void *user) {
    button_t *b = (button_t *)calloc(1, sizeof(button_t));
    if (!b) return NULL;
    b->base.vt      = &BUTTON_VT;
    b->base.bounds  = bounds;
    b->base.visible = 1;
    snprintf(b->label, sizeof(b->label), "%s", label ? label : "");
    b->font_size    = 16.0f;
    b->color_normal = 0xC8C8C8FFu;
    b->color_hover  = 0xE0E0E0FFu;
    b->color_active = 0xA0A0A0FFu;
    b->color_text   = COLOR_BLACK;
    b->color_border = COLOR_GRAY;
    b->on_click     = on_click;
    b->click_user   = user;
    return b;
}

void button_set_label(button_t *self, const char *label) {
    snprintf(self->label, sizeof(self->label), "%s", label ? label : "");
}

void button_set_colors(button_t *self,
                       uint32_t normal, uint32_t hover, uint32_t active,
                       uint32_t text) {
    self->color_normal = normal;
    self->color_hover  = hover;
    self->color_active = active;
    self->color_text   = text;
}
