#include "side_toolbar.h"
#include "../framework/core/color.h"
#include <stdlib.h>
#include <string.h>

/* Buttons span the full width of the sidebar minus a small horizontal pad,
   so widening the sidebar widens the buttons (no more "SWEEP" overflow). */

#define HPAD          12     /* horizontal padding inside the sidebar */
#define BTN_H         40
#define BTN_FONT      15
#define VIEW_BTN_H    32     /* the black-box-toggle row at the bottom */
#define VIEW_BTN_PAD  10     /* gap above the toggle button             */

typedef struct tagt_stb_item { float y; const char *label; place_kind_t kind; } stb_item_t;

static const stb_item_t ITEMS[] = {
    {  60, "AND",      PLACE_AND    },
    { 110, "OR",       PLACE_OR     },
    { 160, "NOT",      PLACE_NOT    },
    { 230, "+ INPUT",  PLACE_INPUT  },
    { 280, "+ OUTPUT", PLACE_OUTPUT },
};
#define ITEM_COUNT ((int)(sizeof(ITEMS) / sizeof(ITEMS[0])))

/* Compute the screen-space rectangle of the i-th button from the toolbar's
   current bounds. Width = bounds.w - 2*HPAD so buttons widen with the sidebar. */
static rect_t item_rect(const widget_t *self, int i) {
    return (rect_t){
        self->bounds.x + HPAD,
        self->bounds.y + ITEMS[i].y,
        self->bounds.w - 2 * HPAD,
        BTN_H,
    };
}

/* The black-box-view toggle button sits at the bottom of the toolbar
   regardless of how tall the sidebar is — so it doesn't get hidden
   when the panel is short. (Phase 8) */
static rect_t view_btn_rect(const widget_t *self) {
    return (rect_t){
        self->bounds.x + HPAD,
        self->bounds.y + self->bounds.h - VIEW_BTN_PAD - VIEW_BTN_H,
        self->bounds.w - 2 * HPAD,
        VIEW_BTN_H,
    };
}

static int hits_rect(rect_t r, vec2_t p) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

static void stb_draw(widget_t *self, igraph_t *g) {
    side_toolbar_t *st = (side_toolbar_t *)self;
    g->draw_rect(g->self, self->bounds, 0x32323CFFu);

    /* Centred title — re-centres as the sidebar is dragged wider/narrower. */
    const char *title = "DCS";
    float tw = g->measure_text(g->self, title, 22);
    g->draw_text(g->self, title,
                 (vec2_t){self->bounds.x + (self->bounds.w - tw) * 0.5f,
                          self->bounds.y + 14},
                 22, 0xC8C8C8FFu);

    vec2_t mp = g->mouse_position(g->self);
    canvas_mode_t mode  = st->target ? st->target->mode       : CMODE_IDLE;
    place_kind_t  pkind = st->target ? st->target->place_kind : PLACE_NONE;

    for (int i = 0; i < ITEM_COUNT; i++) {
        rect_t r = item_rect(self, i);
        int active = (mode == CMODE_PLACING && pkind == ITEMS[i].kind);
        uint32_t bg = active            ? 0x5082B4FFu
                    : hits_rect(r, mp)  ? 0x64646EFFu
                                        : 0x50505AFFu;
        g->draw_rect      (g->self, r, bg);
        g->draw_rect_lines(g->self, r, 1, COLOR_LIGHTGRAY);
        float ltw = g->measure_text(g->self, ITEMS[i].label, BTN_FONT);
        g->draw_text(g->self, ITEMS[i].label,
                     (vec2_t){r.x + (r.w - ltw) * 0.5f,
                              r.y + (r.h - BTN_FONT) * 0.5f},
                     BTN_FONT, COLOR_WHITE);
    }

    /* Black-box / schematic toggle (Phase 8). Label shows the CURRENT mode
       with a square-bracket marker; the background turns blue when active
       (external view) so the user has an at-a-glance state indicator. */
    if (st->target) {
        rect_t r = view_btn_rect(self);
        int external = (st->target->display_mode == DISPLAY_EXTERNAL);
        uint32_t bg = external         ? 0x5082B4FFu
                    : hits_rect(r, mp) ? 0x64646EFFu
                                       : 0x50505AFFu;
        g->draw_rect      (g->self, r, bg);
        g->draw_rect_lines(g->self, r, 1, COLOR_LIGHTGRAY);
        const char *label = external ? "[\xe2\x96\xa0] BLACK-BOX"
                                     : "[ ] BLACK-BOX";
        float ltw = g->measure_text(g->self, label, BTN_FONT);
        g->draw_text(g->self, label,
                     (vec2_t){r.x + (r.w - ltw) * 0.5f,
                              r.y + (r.h - BTN_FONT) * 0.5f},
                     BTN_FONT, COLOR_WHITE);
    }
}

static int stb_handle_event(widget_t *self, const event_t *ev) {
    side_toolbar_t *st = (side_toolbar_t *)self;
    if (ev->kind != EV_MOUSE_PRESS || ev->mouse.btn != IM_LEFT) return 0;
    vec2_t p = ev->mouse.pos;
    for (int i = 0; i < ITEM_COUNT; i++) {
        rect_t r = item_rect(self, i);
        if (!hits_rect(r, p)) continue;
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
    /* Black-box toggle row (Phase 8). */
    if (st->target && hits_rect(view_btn_rect(self), p)) {
        display_mode_t cur = circuit_canvas_widget_display_mode(st->target);
        circuit_canvas_widget_set_display_mode(st->target,
            cur == DISPLAY_EXTERNAL ? DISPLAY_INTERNAL : DISPLAY_EXTERNAL);
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
