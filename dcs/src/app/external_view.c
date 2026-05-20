#include "external_view.h"
#include "../framework/core/color.h"
#include <string.h>

/* Layout constants. Tuned to match the schematic's gate look so the box
   feels consistent with internal-view rendering. */
#define EV_PIN_SPACING   40.0f       /* vertical gap between adjacent pins */
#define EV_BOX_W         200.0f      /* default box width */
#define EV_BOX_MIN_H     120.0f      /* never narrower than this vertically */
#define EV_PIN_STUB      14.0f       /* line length poking out of the box */
#define EV_PIN_R          4.0f
#define EV_LABEL_PAD      8.0f       /* gap between pin tip and its label */
#define EV_NAME_SIZE     22.0f       /* font size for the centered name */
#define EV_PIN_SIZE      14.0f       /* font size for pin labels */

static float fmax2(float a, float b) { return a > b ? a : b; }

void external_view_draw_default(igraph_t *g,
                                const circuit_t *c,
                                const char *display_name,
                                rect_t viewport) {
    if (!g || !c) return;

    int n_in  = c->input_count;
    int n_out = c->output_count;
    int max_pins = n_in > n_out ? n_in : n_out;
    if (max_pins < 2) max_pins = 2;

    /* Box dimensions: tall enough to fit all pins; default width unless
       the display name overflows it. */
    float box_h = fmax2(EV_BOX_MIN_H,
                        (float)(max_pins - 1) * EV_PIN_SPACING + EV_PIN_SPACING);
    float box_w = EV_BOX_W;
    if (display_name && display_name[0]) {
        float nw = g->measure_text(g->self, display_name, EV_NAME_SIZE);
        if (nw + 2 * EV_LABEL_PAD * 2 > box_w) {
            box_w = nw + 2 * EV_LABEL_PAD * 2;
        }
    }

    rect_t box = {
        viewport.x + (viewport.w - box_w) * 0.5f,
        viewport.y + (viewport.h - box_h) * 0.5f,
        box_w,
        box_h,
    };

    /* Body */
    g->draw_rect      (g->self, box, COLOR_WHITE);
    g->draw_rect_lines(g->self, box, 2.0f, COLOR_BLACK);

    /* Centered display name */
    if (display_name && display_name[0]) {
        float nw = g->measure_text(g->self, display_name, EV_NAME_SIZE);
        vec2_t p;
        p.x = box.x + (box.w - nw) * 0.5f;
        p.y = box.y + (box.h - EV_NAME_SIZE) * 0.5f;
        g->draw_text(g->self, display_name, p, EV_NAME_SIZE, COLOR_BLACK);
    }

    /* Input pins along the left edge — distributed vertically. */
    for (int i = 0; i < n_in; i++) {
        float frac = n_in == 1 ? 0.5f : (float)i / (float)(n_in - 1);
        float y = box.y + EV_PIN_SPACING * 0.5f
                + frac * (box.h - EV_PIN_SPACING);
        vec2_t pin_inside = { box.x,              y };
        vec2_t pin_tip    = { box.x - EV_PIN_STUB, y };
        g->draw_line  (g->self, pin_inside, pin_tip, 2.0f, COLOR_BLACK);
        g->draw_circle(g->self, pin_tip,    EV_PIN_R, COLOR_BLACK);
        const char *nm = c->input_names[i];
        float tw = g->measure_text(g->self, nm, EV_PIN_SIZE);
        vec2_t label;
        label.x = pin_tip.x - EV_LABEL_PAD - tw;
        label.y = pin_tip.y - EV_PIN_SIZE * 0.5f;
        g->draw_text(g->self, nm, label, EV_PIN_SIZE, COLOR_BLACK);
    }

    /* Output pins along the right edge. */
    for (int i = 0; i < n_out; i++) {
        float frac = n_out == 1 ? 0.5f : (float)i / (float)(n_out - 1);
        float y = box.y + EV_PIN_SPACING * 0.5f
                + frac * (box.h - EV_PIN_SPACING);
        vec2_t pin_inside = { box.x + box.w,                y };
        vec2_t pin_tip    = { box.x + box.w + EV_PIN_STUB,  y };
        g->draw_line  (g->self, pin_inside, pin_tip, 2.0f, COLOR_BLACK);
        g->draw_circle(g->self, pin_tip,    EV_PIN_R, COLOR_BLACK);
        const char *nm = c->output_names[i];
        if (!nm[0]) continue;
        vec2_t label;
        label.x = pin_tip.x + EV_LABEL_PAD;
        label.y = pin_tip.y - EV_PIN_SIZE * 0.5f;
        g->draw_text(g->self, nm, label, EV_PIN_SIZE, COLOR_BLACK);
    }
}
