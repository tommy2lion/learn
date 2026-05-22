#include "shape.h"
#include <math.h>

/* Arc-segment density: this many line segments per full revolution
   (2*PI radians). 32 gives a visibly smooth curve at typical gate
   sizes without exploding the call count for small arcs. */
#define SHAPE_ARC_SEG_PER_2PI 32

#ifndef SHAPE_PI
#define SHAPE_PI 3.14159265358979323846f
#endif

static vec2_t xf(vec2_t p, vec2_t origin, float scale) {
    return (vec2_t){ origin.x + p.x * scale, origin.y + p.y * scale };
}

static void draw_arc(igraph_t *g, vec2_t center, float r, float a0, float a1,
                     vec2_t origin, float scale, float thick, uint32_t color) {
    float span = a1 - a0;
    if (span == 0.0f) return;
    float abs_span = span < 0 ? -span : span;
    int n = (int)((abs_span / (2.0f * SHAPE_PI)) * SHAPE_ARC_SEG_PER_2PI + 0.5f);
    if (n < 2) n = 2;

    vec2_t prev_local = {
        center.x + r * cosf(a0),
        center.y + r * sinf(a0),
    };
    vec2_t prev = xf(prev_local, origin, scale);
    for (int i = 1; i <= n; i++) {
        float t = (float)i / (float)n;
        float ang = a0 + span * t;
        vec2_t next_local = {
            center.x + r * cosf(ang),
            center.y + r * sinf(ang),
        };
        vec2_t next = xf(next_local, origin, scale);
        g->draw_line(g->self, prev, next, thick, color);
        prev = next;
    }
}

void shape_draw(const shape_t *self, igraph_t *g,
                vec2_t origin, float scale, float thick, uint32_t color) {
    if (!self || !g || !self->ops || self->n_ops <= 0) return;
    for (int i = 0; i < self->n_ops; i++) {
        const shape_op_t *op = &self->ops[i];
        switch (op->kind) {
            case SHAPE_OP_LINE: {
                vec2_t a = xf(op->a, origin, scale);
                vec2_t b = xf(op->b, origin, scale);
                g->draw_line(g->self, a, b, thick, color);
                break;
            }
            case SHAPE_OP_CIRCLE: {
                vec2_t c = xf(op->a, origin, scale);
                g->draw_circle_lines(g->self, c, op->r * scale, thick, color);
                break;
            }
            case SHAPE_OP_ARC:
                draw_arc(g, op->a, op->r, op->a0, op->a1,
                         origin, scale, thick, color);
                break;
        }
    }
}
