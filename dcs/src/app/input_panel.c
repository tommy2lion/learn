#include "input_panel.h"
#include "../framework/core/color.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TOGGLE_H 24
#define TOGGLE_GAP 4

/* ── layout (relative to widget bounds) ───────────────────────────── */

static rect_t toggle_rect(const widget_t *w, int idx) {
    return (rect_t){ w->bounds.x + 8,
                     w->bounds.y + 22 + idx * (TOGGLE_H + TOGGLE_GAP),
                     w->bounds.w - 16, TOGGLE_H };
}
static rect_t steps_minus_rect(const widget_t *w) {
    return (rect_t){ w->bounds.x + 8, w->bounds.y + w->bounds.h - 78, 22, 22 };
}
static rect_t steps_plus_rect(const widget_t *w) {
    return (rect_t){ w->bounds.x + w->bounds.w - 30, w->bounds.y + w->bounds.h - 78, 22, 22 };
}
static rect_t run_btn_rect(const widget_t *w) {
    int half = ((int)w->bounds.w - 24) / 2;
    return (rect_t){ w->bounds.x + 8, w->bounds.y + w->bounds.h - 44,
                     (float)half, 36 };
}
static rect_t sweep_btn_rect(const widget_t *w) {
    int half = ((int)w->bounds.w - 24) / 2;
    return (rect_t){ w->bounds.x + 8 + half + 8, w->bounds.y + w->bounds.h - 44,
                     (float)half, 36 };
}

/* ── toggle map ───────────────────────────────────────────────────── */

static signal_t toggle_get(const input_panel_t *p, const char *name) {
    for (int i = 0; i < p->toggle_count; i++)
        if (strcmp(p->toggles[i].name, name) == 0) return p->toggles[i].value;
    return SIG_LOW;
}

static void toggle_flip(input_panel_t *p, const char *name) {
    for (int i = 0; i < p->toggle_count; i++) {
        if (strcmp(p->toggles[i].name, name) == 0) {
            p->toggles[i].value = (p->toggles[i].value == SIG_HIGH) ? SIG_LOW : SIG_HIGH;
            return;
        }
    }
    if (p->toggle_count < INPUT_PANEL_MAX_TOGGLES) {
        input_toggle_t *t = &p->toggles[p->toggle_count++];
        snprintf(t->name, DOMAIN_NAME_LEN, "%s", name);
        t->value = SIG_HIGH;
    }
}

/* ── widget vtable ───────────────────────────────────────────────── */

static int hit_rect(rect_t r, vec2_t p) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

static void ip_draw(widget_t *self, igraph_t *g) {
    input_panel_t *p = (input_panel_t *)self;
    g->draw_rect(g->self, self->bounds, 0x24242CFFu);
    g->draw_text(g->self, "INPUTS", (vec2_t){self->bounds.x + 8, self->bounds.y + 4},
                 13, 0xC8C8C8FFu);

    vec2_t mp = g->mouse_position(g->self);

    int total_inputs = p->circuit ? p->circuit->input_count : 0;
    int visible = total_inputs > INPUT_PANEL_MAX_VISIBLE ? INPUT_PANEL_MAX_VISIBLE : total_inputs;
    for (int i = 0; i < visible; i++) {
        const char *nm = p->circuit->input_names[i];
        signal_t v = toggle_get(p, nm);
        rect_t r = toggle_rect(self, i);
        uint32_t bg = (v == SIG_HIGH) ? 0x46965AFFu : 0x965A5AFFu;
        if (hit_rect(r, mp)) bg += 0x141414000u;       /* slight lighten */
        g->draw_rect      (g->self, r, bg);
        g->draw_rect_lines(g->self, r, 1, COLOR_BLACK);
        char lbl[80];
        snprintf(lbl, sizeof(lbl), "%s = %d", nm, v == SIG_HIGH ? 1 : 0);
        g->draw_text(g->self, lbl, (vec2_t){r.x + 6, r.y + 5}, 13, COLOR_WHITE);
    }
    if (total_inputs > INPUT_PANEL_MAX_VISIBLE) {
        rect_t r = toggle_rect(self, INPUT_PANEL_MAX_VISIBLE);
        char m[32]; snprintf(m, sizeof(m), "+%d more", total_inputs - INPUT_PANEL_MAX_VISIBLE);
        g->draw_text(g->self, m, (vec2_t){r.x + 6, r.y + 5}, 12, COLOR_GRAY);
    }
    if (total_inputs == 0) {
        g->draw_text(g->self, "(no inputs)",
                     (vec2_t){self->bounds.x + 12, self->bounds.y + 30}, 12, COLOR_GRAY);
    }

    /* steps row */
    rect_t mr = steps_minus_rect(self);
    rect_t pr = steps_plus_rect(self);
    g->draw_text(g->self, "Steps", (vec2_t){self->bounds.x + 8, mr.y - 16}, 12, 0xB4B4B4FFu);
    g->draw_rect      (g->self, mr, 0x50505AFFu);
    g->draw_rect_lines(g->self, mr, 1, COLOR_LIGHTGRAY);
    g->draw_text(g->self, "-", (vec2_t){mr.x + mr.w * 0.5f - 3, mr.y + 3}, 16, COLOR_WHITE);
    g->draw_rect      (g->self, pr, 0x50505AFFu);
    g->draw_rect_lines(g->self, pr, 1, COLOR_LIGHTGRAY);
    g->draw_text(g->self, "+", (vec2_t){pr.x + pr.w * 0.5f - 4, pr.y + 3}, 16, COLOR_WHITE);
    char step_lbl[16]; snprintf(step_lbl, sizeof(step_lbl), "%d", p->steps);
    float tw = g->measure_text(g->self, step_lbl, 16);
    g->draw_text(g->self, step_lbl,
                 (vec2_t){self->bounds.x + self->bounds.w * 0.5f - tw * 0.5f, mr.y + 3},
                 16, COLOR_WHITE);

    /* RUN + SWEEP */
    rect_t rr = run_btn_rect(self);
    uint32_t run_bg = hit_rect(rr, mp) ? 0x50B46EFFu : 0x328C50FFu;
    g->draw_rect      (g->self, rr, run_bg);
    g->draw_rect_lines(g->self, rr, 2, COLOR_BLACK);
    float rtw = g->measure_text(g->self, "RUN", 16);
    g->draw_text(g->self, "RUN",
                 (vec2_t){rr.x + rr.w * 0.5f - rtw * 0.5f, rr.y + rr.h * 0.5f - 8},
                 16, COLOR_WHITE);

    rect_t sr = sweep_btn_rect(self);
    uint32_t sw_bg = hit_rect(sr, mp) ? 0x5AAAC8FFu : 0x3C8CAAFFu;
    g->draw_rect      (g->self, sr, sw_bg);
    g->draw_rect_lines(g->self, sr, 2, COLOR_BLACK);
    float stw = g->measure_text(g->self, "SWEEP", 14);
    g->draw_text(g->self, "SWEEP",
                 (vec2_t){sr.x + sr.w * 0.5f - stw * 0.5f, sr.y + sr.h * 0.5f - 7},
                 14, COLOR_WHITE);
}

static int ip_handle_event(widget_t *self, const event_t *ev) {
    input_panel_t *p = (input_panel_t *)self;
    if (ev->kind != EV_MOUSE_PRESS || ev->mouse.btn != IM_LEFT) return 0;
    vec2_t mp = ev->mouse.pos;

    /* run / sweep */
    if (hit_rect(run_btn_rect(self), mp)) {
        if (p->on_run) p->on_run(0, p->run_user);
        return 1;
    }
    if (hit_rect(sweep_btn_rect(self), mp)) {
        if (p->on_run) p->on_run(1, p->run_user);
        return 1;
    }
    /* steps - / + */
    if (hit_rect(steps_minus_rect(self), mp)) {
        if (p->steps > 1) p->steps--;
        return 1;
    }
    if (hit_rect(steps_plus_rect(self), mp)) {
        if (p->steps < INPUT_PANEL_MAX_STEPS) p->steps++;
        return 1;
    }
    /* per-input toggles */
    int total_inputs = p->circuit ? p->circuit->input_count : 0;
    int visible = total_inputs > INPUT_PANEL_MAX_VISIBLE ? INPUT_PANEL_MAX_VISIBLE : total_inputs;
    for (int i = 0; i < visible; i++) {
        if (hit_rect(toggle_rect(self, i), mp)) {
            toggle_flip(p, p->circuit->input_names[i]);
            return 1;
        }
    }
    return 1;   /* swallow clicks within the panel */
}

static void ip_destroy(widget_t *self) { free(self); }

static const widget_vt_t IP_VT = {
    .draw         = ip_draw,
    .handle_event = ip_handle_event,
    .destroy      = ip_destroy,
};

input_panel_t *input_panel_create(rect_t bounds, const circuit_t *c) {
    input_panel_t *p = (input_panel_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->base.vt      = &IP_VT;
    p->base.bounds  = bounds;
    p->base.visible = 1;
    p->circuit      = c;
    p->steps        = 8;
    return p;
}

void input_panel_set_run_cb(input_panel_t *self, ip_run_fn_t cb, void *user) {
    self->on_run   = cb;
    self->run_user = user;
}

signal_t input_panel_value(const input_panel_t *self, const char *name) {
    return toggle_get(self, name);
}

int input_panel_steps(const input_panel_t *self) { return self->steps; }
