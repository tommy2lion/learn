#include "timing_canvas_widget.h"
#include "../framework/core/color.h"
#include <stdlib.h>

#define NAME_W   70.0f
#define PAD       8.0f
#define ROW_H_MIN 20.0f

static float signal_y(signal_t v, float top, float bot) {
    if (v == SIG_HIGH) return top;
    if (v == SIG_LOW)  return bot;
    return (top + bot) * 0.5f;          /* SIG_UNDEF */
}
static uint32_t signal_color(signal_t v) {
    return (v == SIG_UNDEF) ? COLOR_RED : 0x325AA0FFu;
}

static void draw_track(igraph_t *g, const waveform_track_t *t, rect_t row) {
    g->draw_text(g->self, t->name,
                 (vec2_t){row.x + 6, row.y + (row.h * 0.5f - 7)}, 14, COLOR_BLACK);

    float trace_x = row.x + NAME_W;
    float trace_w = row.w - NAME_W;
    if (t->step_count <= 0 || trace_w <= 0) return;
    float top = row.y + 4, bot = row.y + row.h - 4;
    float step_w = trace_w / t->step_count;

    g->draw_line(g->self, (vec2_t){trace_x, top}, (vec2_t){trace_x + trace_w, top}, 1, COLOR_LIGHTGRAY);
    g->draw_line(g->self, (vec2_t){trace_x, bot}, (vec2_t){trace_x + trace_w, bot}, 1, COLOR_LIGHTGRAY);
    for (int i = 0; i <= t->step_count; i++) {
        float x = trace_x + i * step_w;
        g->draw_line(g->self, (vec2_t){x, bot}, (vec2_t){x, bot + 3}, 1, COLOR_GRAY);
    }
    signal_t prev = t->values[0];
    for (int i = 0; i < t->step_count; i++) {
        float x0 = trace_x + i * step_w;
        float x1 = trace_x + (i + 1) * step_w;
        signal_t v = t->values[i];
        float y = signal_y(v, top, bot);
        uint32_t c = signal_color(v);
        g->draw_line(g->self, (vec2_t){x0, y}, (vec2_t){x1, y}, 2, c);
        if (i > 0 && v != prev) {
            float yp = signal_y(prev, top, bot);
            g->draw_line(g->self, (vec2_t){x0, yp}, (vec2_t){x0, y}, 2, c);
        }
        prev = v;
    }
}

static void tcw_draw(widget_t *self, igraph_t *g) {
    timing_canvas_widget_t *tw = (timing_canvas_widget_t *)self;
    g->draw_rect      (g->self, self->bounds, 0xFAFAFCFFu);
    g->draw_rect_lines(g->self, self->bounds, 1.0f, COLOR_LIGHTGRAY);

    g->draw_text(g->self, "Timing Diagram",
                 (vec2_t){self->bounds.x + PAD, self->bounds.y + 4}, 14, COLOR_DARKGRAY);

    rect_t inner = {
        self->bounds.x + PAD,
        self->bounds.y + 22,
        self->bounds.w - PAD * 2,
        self->bounds.h - 22 - PAD,
    };

    if (!tw->waves || tw->waves->track_count <= 0) {
        g->draw_text(g->self, "(no run yet — toggle inputs and click RUN or SWEEP)",
                     (vec2_t){inner.x, inner.y + inner.h * 0.5f - 7}, 13, COLOR_GRAY);
        return;
    }

    float row_h = inner.h / tw->waves->track_count;
    if (row_h < ROW_H_MIN) row_h = ROW_H_MIN;

    for (int i = 0; i < tw->waves->track_count; i++) {
        rect_t row = { inner.x, inner.y + i * row_h, inner.w, row_h };
        draw_track(g, &tw->waves->tracks[i], row);
    }
}

static int  tcw_handle_event(widget_t *self, const event_t *ev) { (void)self; (void)ev; return 0; }
static void tcw_destroy     (widget_t *self) { free(self); }

static const widget_vt_t TCW_VT = {
    .draw         = tcw_draw,
    .handle_event = tcw_handle_event,
    .destroy      = tcw_destroy,
};

timing_canvas_widget_t *timing_canvas_widget_create(rect_t bounds) {
    timing_canvas_widget_t *tw = (timing_canvas_widget_t *)calloc(1, sizeof(*tw));
    if (!tw) return NULL;
    tw->base.vt      = &TCW_VT;
    tw->base.bounds  = bounds;
    tw->base.visible = 1;
    return tw;
}

void timing_canvas_widget_set_waves(timing_canvas_widget_t *self, const waveform_t *w) {
    self->waves = w;
}
