/* shape_draw interpreter tests (U-21 part A).
 *
 * Drives the DSL against a mock igraph that records per-primitive call
 * counts plus the most recent argument values. Lets us assert "this
 * 3-op shape produced exactly 3 draw_line calls at these endpoints"
 * etc., without touching raylib.
 */

#include "../src/framework/shape.h"
#include "../src/framework/core/oo.h"
#include "../src/framework/graphics/igraph.h"
#include <stdio.h>
#include <math.h>

#define PI_F     3.14159265358979323846f
#define PI_2_F   1.57079632679489661923f

static int failures = 0, total = 0;
static void check(const char *name, int cond) {
    total++;
    if (cond) printf("PASS  %s\n", name);
    else      { failures++; printf("FAIL  %s\n", name); }
}

typedef struct {
    int      line_calls;
    int      circle_lines_calls;
    /* most recent values, for shape-quality assertions */
    vec2_t   last_line_a, last_line_b;
    vec2_t   last_circle_c;
    float    last_circle_r;
    float    last_thick;
    uint32_t last_color;
} mk_counts_t;

static void mk_draw_rect(void *self, rect_t r, uint32_t color) {
    (void)self; (void)r; (void)color;
}
static void mk_draw_rect_lines(void *self, rect_t r, float th, uint32_t color) {
    (void)self; (void)r; (void)th; (void)color;
}
static void mk_draw_line(void *self, vec2_t a, vec2_t b, float thick, uint32_t color) {
    mk_counts_t *m = (mk_counts_t *)self;
    m->line_calls++;
    m->last_line_a = a;
    m->last_line_b = b;
    m->last_thick  = thick;
    m->last_color  = color;
}
static void mk_draw_circle(void *self, vec2_t c, float r, uint32_t color) {
    (void)self; (void)c; (void)r; (void)color;
}
static void mk_draw_circle_lines(void *self, vec2_t c, float r, float thick, uint32_t color) {
    mk_counts_t *m = (mk_counts_t *)self;
    m->circle_lines_calls++;
    m->last_circle_c = c;
    m->last_circle_r = r;
    m->last_thick    = thick;
    m->last_color    = color;
}
static void mk_draw_text(void *self, const char *s, vec2_t p, float sz, uint32_t color) {
    (void)self; (void)s; (void)p; (void)sz; (void)color;
}

static void mock_igraph_init(igraph_t *g, mk_counts_t *m) {
    *m = (mk_counts_t){0};
    *g = (igraph_t){0};
    g->self              = m;
    g->draw_rect         = mk_draw_rect;
    g->draw_rect_lines   = mk_draw_rect_lines;
    g->draw_line         = mk_draw_line;
    g->draw_circle       = mk_draw_circle;
    g->draw_circle_lines = mk_draw_circle_lines;
    g->draw_text         = mk_draw_text;
}

static int near(float a, float b) {
    float d = a - b;
    if (d < 0) d = -d;
    return d < 0.01f;
}

int main(void) {
    printf("=== framework/shape tests ===\n");

    igraph_t g;
    mk_counts_t m;

    /* ── NULL + empty inputs ───────────────────────────────────────── */
    {
        mock_igraph_init(&g, &m);
        shape_draw(NULL, &g, (vec2_t){0,0}, 1.0f, 1.0f, 0xFF);
        check("NULL shape: no draws", m.line_calls == 0 && m.circle_lines_calls == 0);

        shape_t empty = { .ops = NULL, .n_ops = 0 };
        shape_draw(&empty, &g, (vec2_t){0,0}, 1.0f, 1.0f, 0xFF);
        check("empty shape: no draws", m.line_calls == 0 && m.circle_lines_calls == 0);
    }

    /* ── LINE op: single segment, scaled + translated correctly ───── */
    {
        mock_igraph_init(&g, &m);
        const shape_op_t ops[] = { SHAPE_LINE(0, 0, 10, 0) };
        shape_t s = { .ops = ops, .n_ops = 1 };
        shape_draw(&s, &g, (vec2_t){100, 50}, 2.0f, 1.5f, 0xAABBCCDD);
        check("LINE: emits 1 draw_line",        m.line_calls == 1);
        check("LINE: a translated by origin",   near(m.last_line_a.x, 100) && near(m.last_line_a.y, 50));
        check("LINE: b scaled and translated",  near(m.last_line_b.x, 120) && near(m.last_line_b.y, 50));
        check("LINE: thickness forwarded",      near(m.last_thick, 1.5f));
        check("LINE: color forwarded",          m.last_color == 0xAABBCCDD);
    }

    /* ── CIRCLE op: single circle, radius scaled, no LINE calls ───── */
    {
        mock_igraph_init(&g, &m);
        const shape_op_t ops[] = { SHAPE_CIRCLE(5, 5, 4) };
        shape_t s = { .ops = ops, .n_ops = 1 };
        shape_draw(&s, &g, (vec2_t){200, 0}, 3.0f, 1.0f, 0xFF0000FF);
        check("CIRCLE: emits 1 draw_circle_lines", m.circle_lines_calls == 1);
        check("CIRCLE: emits 0 draw_line",         m.line_calls == 0);
        check("CIRCLE: center translated",
              near(m.last_circle_c.x, 215) && near(m.last_circle_c.y, 15));
        check("CIRCLE: radius scaled",             near(m.last_circle_r, 12));
    }

    /* ── ARC op: a quarter-circle approximates with multiple lines ── */
    {
        mock_igraph_init(&g, &m);
        const shape_op_t ops[] = {
            /* quarter arc from 0 to pi/2 = 32/4 = 8 segments */
            SHAPE_ARC(0, 0, 10, 0.0f, PI_2_F),
        };
        shape_t s = { .ops = ops, .n_ops = 1 };
        shape_draw(&s, &g, (vec2_t){0, 0}, 1.0f, 1.0f, 0xFF);
        check("ARC: quarter arc emits 8 line segments", m.line_calls == 8);
        check("ARC: emits 0 circle_lines",              m.circle_lines_calls == 0);
    }

    /* ── 3-op shape (toy triangle): exactly 3 LINE calls ──────────── */
    {
        mock_igraph_init(&g, &m);
        const shape_op_t ops[] = {
            SHAPE_LINE(0, 0, 10,  0),
            SHAPE_LINE(10, 0,  5, 10),
            SHAPE_LINE(5, 10,  0,  0),
        };
        shape_t s = { .ops = ops, .n_ops = 3 };
        shape_draw(&s, &g, (vec2_t){0, 0}, 1.0f, 1.0f, 0xFF);
        check("triangle: 3 draw_line calls",     m.line_calls == 3);
        check("triangle: 0 circle_lines calls",  m.circle_lines_calls == 0);
    }

    /* ── Mixed shape (line + circle + arc): each kind dispatches ─── */
    {
        mock_igraph_init(&g, &m);
        const shape_op_t ops[] = {
            SHAPE_LINE(0, 0, 10, 0),
            SHAPE_CIRCLE(12, 0, 2),
            SHAPE_ARC(0, 0, 5, 0.0f, PI_F),     /* half arc, 16 segments */
        };
        shape_t s = { .ops = ops, .n_ops = 3 };
        shape_draw(&s, &g, (vec2_t){0, 0}, 1.0f, 1.0f, 0xFF);
        check("mixed: 1 line + 16 arc = 17 draw_line", m.line_calls == 17);
        check("mixed: 1 draw_circle_lines",            m.circle_lines_calls == 1);
    }

    /* ── NULL igraph is a no-op (sanity) ───────────────────────────── */
    {
        const shape_op_t ops[] = { SHAPE_LINE(0, 0, 1, 1) };
        shape_t s = { .ops = ops, .n_ops = 1 };
        shape_draw(&s, NULL, (vec2_t){0, 0}, 1.0f, 1.0f, 0xFF);
        check("NULL igraph: no crash", 1);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
