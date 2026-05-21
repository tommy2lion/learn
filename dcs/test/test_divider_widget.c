/* App-layer scaffold tests for divider_widget (U-5 / Stage 3).
   Drag math, range clamping, change-callback wiring. Uses a mock
   igraph with scripted mouse state — same pattern as
   test_circuit_canvas_supplement.c. */

#include "../src/app/divider_widget.h"
#include "../src/framework/widgets/widget.h"
#include "../src/framework/core/message.h"
#include "../src/framework/graphics/igraph.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;
static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* ── mock igraph: scripted mouse position + button-down state ──── */

typedef struct {
    vec2_t mouse_pos;
    int    left_down;
    int    cursor_set_count;
    cursor_kind_t last_cursor;
} mock_state_t;

static vec2_t mk_mouse_position(void *self) { return ((mock_state_t *)self)->mouse_pos; }
static int    mk_mouse_down    (void *self, igraph_mouse_btn_t b) {
    (void)b;
    return ((mock_state_t *)self)->left_down;
}
static void   mk_draw_rect     (void *self, rect_t r, uint32_t c) { (void)self; (void)r; (void)c; }
static void   mk_set_cursor    (void *self, cursor_kind_t k) {
    mock_state_t *m = (mock_state_t *)self;
    m->cursor_set_count++;
    m->last_cursor = k;
}

static void init_mock_igraph(igraph_t *g, mock_state_t *m) {
    memset(g, 0, sizeof(*g));
    memset(m, 0, sizeof(*m));
    g->self = m;
    g->mouse_position = mk_mouse_position;
    g->mouse_down     = mk_mouse_down;
    g->draw_rect      = mk_draw_rect;
    g->set_cursor     = mk_set_cursor;
}

/* ── change-callback shim ───────────────────────────────────── */
typedef struct {
    int last_value;
    int call_count;
} cb_state_t;
static void capture_change(int v, void *user) {
    cb_state_t *s = (cb_state_t *)user;
    s->last_value = v;
    s->call_count++;
}

int main(void) {
    printf("=== divider_widget tests ===\n");

    /* ── create defaults ──────────────────────────────────────── */
    {
        divider_widget_t *d = divider_widget_create((rect_t){100, 200, 50, 10}, DIVIDER_HORIZONTAL);
        check("create: non-NULL",                d != NULL);
        check("create: bounds preserved",        d->base.bounds.x == 100 && d->base.bounds.h == 10);
        check("create: orient stored",           d->orient == DIVIDER_HORIZONTAL);
        check("create: default range covers 0..2^30",
              d->value_min == 0 && d->value_max == (1 << 30));
        check("create: not dragging initially",  d->dragging == 0);
        widget_destroy(&d->base);
    }

    /* ── set_range ───────────────────────────────────────────── */
    {
        divider_widget_t *d = divider_widget_create((rect_t){0, 0, 10, 10}, DIVIDER_VERTICAL);
        divider_widget_set_range(d, 100, 500);
        check("set_range: min stored",  d->value_min == 100);
        check("set_range: max stored",  d->value_max == 500);
        widget_destroy(&d->base);
    }

    /* ── press inside bounds starts a drag ─────────────────── */
    {
        divider_widget_t *d = divider_widget_create((rect_t){50, 50, 100, 20}, DIVIDER_HORIZONTAL);
        event_t ev = {0};
        ev.kind = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){100, 60};   /* inside */
        check("press inside: handle_event consumes",
              widget_handle_event(&d->base, &ev) == 1);
        check("press inside: dragging flag set", d->dragging == 1);
        widget_destroy(&d->base);
    }

    /* ── press outside bounds does not start a drag ────────── */
    {
        divider_widget_t *d = divider_widget_create((rect_t){50, 50, 100, 20}, DIVIDER_HORIZONTAL);
        event_t ev = {0};
        ev.kind = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){10, 10};    /* outside */
        check("press outside: handle_event passes (0)",
              widget_handle_event(&d->base, &ev) == 0);
        check("press outside: not dragging",          d->dragging == 0);
        widget_destroy(&d->base);
    }

    /* ── horizontal drag fires callback with mouse.y, clamped ── */
    {
        divider_widget_t *d = divider_widget_create((rect_t){0, 0, 200, 20}, DIVIDER_HORIZONTAL);
        divider_widget_set_range(d, 50, 150);
        cb_state_t cb = {0};
        divider_widget_set_change_cb(d, capture_change, &cb);
        /* Start a drag with a press, then poll a draw with the
           mouse held + at y=120 (within range — should report 120). */
        event_t press = {0};
        press.kind = EV_MOUSE_PRESS;
        press.mouse.btn = IM_LEFT;
        press.mouse.pos = (vec2_t){100, 10};
        widget_handle_event(&d->base, &press);

        igraph_t g; mock_state_t m;
        init_mock_igraph(&g, &m);
        m.mouse_pos = (vec2_t){100, 120};
        m.left_down = 1;
        widget_draw(&d->base, &g);
        check("h drag: callback received y=120",  cb.last_value == 120);
        check("h drag: callback fired once",      cb.call_count == 1);

        /* Drag below the min — should clamp to 50. */
        m.mouse_pos = (vec2_t){100, 10};
        widget_draw(&d->base, &g);
        check("h drag: clamped to value_min",     cb.last_value == 50);

        /* Drag above the max — should clamp to 150. */
        m.mouse_pos = (vec2_t){100, 9999};
        widget_draw(&d->base, &g);
        check("h drag: clamped to value_max",     cb.last_value == 150);
        widget_destroy(&d->base);
    }

    /* ── vertical drag fires callback with mouse.x, clamped ──── */
    {
        divider_widget_t *d = divider_widget_create((rect_t){0, 0, 20, 200}, DIVIDER_VERTICAL);
        divider_widget_set_range(d, 80, 240);
        cb_state_t cb = {0};
        divider_widget_set_change_cb(d, capture_change, &cb);
        event_t press = {0};
        press.kind = EV_MOUSE_PRESS;
        press.mouse.btn = IM_LEFT;
        press.mouse.pos = (vec2_t){10, 100};
        widget_handle_event(&d->base, &press);

        igraph_t g; mock_state_t m;
        init_mock_igraph(&g, &m);
        m.mouse_pos = (vec2_t){180, 100};
        m.left_down = 1;
        widget_draw(&d->base, &g);
        check("v drag: callback received x=180",  cb.last_value == 180);
        m.mouse_pos = (vec2_t){50, 100};
        widget_draw(&d->base, &g);
        check("v drag: clamped to value_min",     cb.last_value == 80);
        m.mouse_pos = (vec2_t){9999, 100};
        widget_draw(&d->base, &g);
        check("v drag: clamped to value_max",     cb.last_value == 240);
        widget_destroy(&d->base);
    }

    /* ── release ends the drag ────────────────────────────────── */
    {
        divider_widget_t *d = divider_widget_create((rect_t){0, 0, 100, 20}, DIVIDER_HORIZONTAL);
        event_t press = {0};
        press.kind = EV_MOUSE_PRESS;
        press.mouse.btn = IM_LEFT;
        press.mouse.pos = (vec2_t){50, 10};
        widget_handle_event(&d->base, &press);
        check("setup: dragging started", d->dragging == 1);
        event_t rel = {0};
        rel.kind = EV_MOUSE_RELEASE;
        rel.mouse.btn = IM_LEFT;
        check("release: handle_event consumes",  widget_handle_event(&d->base, &rel) == 1);
        check("release: dragging cleared",       d->dragging == 0);
        widget_destroy(&d->base);
    }

    /* ── while dragging, all mouse events get swallowed ────── */
    {
        divider_widget_t *d = divider_widget_create((rect_t){0, 0, 100, 20}, DIVIDER_HORIZONTAL);
        event_t press = {0};
        press.kind = EV_MOUSE_PRESS;
        press.mouse.btn = IM_LEFT;
        press.mouse.pos = (vec2_t){50, 10};
        widget_handle_event(&d->base, &press);
        /* A stray mouse_move while dragging — must be consumed. */
        event_t mv = {0};
        mv.kind = EV_MOUSE_MOVE;
        mv.mouse.pos = (vec2_t){9999, 9999};
        check("dragging: stray mouse-move consumed",
              widget_handle_event(&d->base, &mv) == 1);
        widget_destroy(&d->base);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
