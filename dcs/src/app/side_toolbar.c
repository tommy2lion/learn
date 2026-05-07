#include "side_toolbar.h"
#include "../framework/core/color.h"
#include <stdlib.h>
#include <string.h>

#define BTN_W 90
#define BTN_H 40

typedef struct { rect_t r; const char *label; place_kind_t kind; } stb_item_t;

static const stb_item_t ITEMS[] = {
    { {10, 50,  BTN_W, BTN_H}, "AND",      PLACE_AND    },
    { {10, 100, BTN_W, BTN_H}, "OR",       PLACE_OR     },
    { {10, 150, BTN_W, BTN_H}, "NOT",      PLACE_NOT    },
    { {10, 220, BTN_W, BTN_H}, "+ INPUT",  PLACE_INPUT  },
    { {10, 270, BTN_W, BTN_H}, "+ OUTPUT", PLACE_OUTPUT },
};
#define ITEM_COUNT ((int)(sizeof(ITEMS) / sizeof(ITEMS[0])))

static void stb_draw(widget_t *self, igraph_t *g) {
    side_toolbar_t *st = (side_toolbar_t *)self;
    g->draw_rect(g->self, self->bounds, 0x32323CFFu);
    g->draw_text(g->self, "DCS",
                 (vec2_t){self->bounds.x + 32, self->bounds.y + 14}, 22, 0xC8C8C8FFu);

    vec2_t mp = g->mouse_position(g->self);
    canvas_mode_t mode  = st->target ? st->target->mode       : CMODE_IDLE;
    place_kind_t  pkind = st->target ? st->target->place_kind : PLACE_NONE;

    for (int i = 0; i < ITEM_COUNT; i++) {
        rect_t r = { self->bounds.x + ITEMS[i].r.x,
                     self->bounds.y + ITEMS[i].r.y,
                     ITEMS[i].r.w, ITEMS[i].r.h };
        int active = (mode == CMODE_PLACING && pkind == ITEMS[i].kind);
        uint32_t bg = active                                  ? 0x5082B4FFu
                    : (mp.x >= r.x && mp.x < r.x + r.w
                    && mp.y >= r.y && mp.y < r.y + r.h)       ? 0x64646EFFu
                                                              : 0x50505AFFu;
        g->draw_rect      (g->self, r, bg);
        g->draw_rect_lines(g->self, r, 1, COLOR_LIGHTGRAY);
        float tw = g->measure_text(g->self, ITEMS[i].label, 14);
        g->draw_text(g->self, ITEMS[i].label,
                     (vec2_t){r.x + r.w * 0.5f - tw * 0.5f,
                              r.y + r.h * 0.5f - 7}, 14, COLOR_WHITE);
    }
}

static int stb_handle_event(widget_t *self, const event_t *ev) {
    side_toolbar_t *st = (side_toolbar_t *)self;
    if (ev->kind != EV_MOUSE_PRESS || ev->mouse.btn != IM_LEFT) return 0;
    vec2_t p = ev->mouse.pos;
    for (int i = 0; i < ITEM_COUNT; i++) {
        rect_t r = { self->bounds.x + ITEMS[i].r.x,
                     self->bounds.y + ITEMS[i].r.y,
                     ITEMS[i].r.w, ITEMS[i].r.h };
        if (p.x < r.x || p.x >= r.x + r.w) continue;
        if (p.y < r.y || p.y >= r.y + r.h) continue;
        if (!st->target) return 1;
        if (st->target->mode == CMODE_PLACING && st->target->place_kind == ITEMS[i].kind) {
            /* clicking the active button cancels placement */
            st->target->mode = CMODE_IDLE;
            st->target->place_kind = PLACE_NONE;
        } else {
            circuit_canvas_widget_arm_place(st->target, ITEMS[i].kind);
        }
        return 1;
    }
    return 1;
}

static void stb_destroy(widget_t *self) { free(self); }

static const widget_vt_t STB_VT = {
    .draw         = stb_draw,
    .handle_event = stb_handle_event,
    .destroy      = stb_destroy,
};

side_toolbar_t *side_toolbar_create(rect_t bounds, circuit_canvas_widget_t *target) {
    side_toolbar_t *st = (side_toolbar_t *)calloc(1, sizeof(*st));
    if (!st) return NULL;
    st->base.vt      = &STB_VT;
    st->base.bounds  = bounds;
    st->base.visible = 1;
    st->target       = target;
    return st;
}
