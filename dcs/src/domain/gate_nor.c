#include "component.h"
#include <stdlib.h>
#include <stdio.h>

static signal_t nor2(signal_t a, signal_t b) {
    if (a == SIG_HIGH || b == SIG_HIGH) return SIG_LOW;           /* short-circuit */
    if (a == SIG_UNDEF || b == SIG_UNDEF) return SIG_UNDEF;
    return SIG_HIGH;
}

static void nor_evaluate(component_t *self, const signal_t *in, signal_t *out) {
    (void)self;
    out[0] = nor2(in[0], in[1]);
}

static void nor_destroy(component_t *self) { free(self); }

/* NOR = OR shape with an inversion bubble at the output. Same trick
   as NAND: the OR body is shrunk so the front-arc tip lands at +0.7
   instead of +1.0, then a (0.85, 0) r=0.15 bubble fills the gap to
   the output pin at +1.0. Front-arc geometry rederived: tip at
   (+0.7, 0), back corners at (-1, ±1) → centre (-1, ±0.945),
   radius 1.945. End angles ±atan2(0.945, 1.7) ≈ ±0.5071. */
static const shape_op_t NOR_OPS[] = {
    /* concave back arc — same as OR */
    SHAPE_ARC(-2.0f,  0.0f, 1.41421356f, -0.78539816f,  0.78539816f),
    /* shrunken front arcs ending at (+0.7, 0) */
    SHAPE_ARC(-1.0f,  0.945f, 1.945f, -1.57079633f, -0.50708930f),
    SHAPE_ARC(-1.0f, -0.945f, 1.945f,  1.57079633f,  0.50708930f),
    /* input lead stubs — same convention as OR */
    SHAPE_LINE(-1.0f, -0.5f, -0.65f, -0.5f),
    SHAPE_LINE(-1.0f,  0.5f, -0.65f,  0.5f),
    /* inversion bubble */
    SHAPE_CIRCLE(0.85f, 0.0f, 0.15f),
};
static const shape_t NOR_SHAPE = { .ops = NOR_OPS, .n_ops = 6 };
static const shape_t *nor_shape(void) { return &NOR_SHAPE; }

static const component_vt_t NOR_VT = {
    .kind          = COMP_NOR,
    .pin_count_in  = 2,
    .pin_count_out = 1,
    .evaluate      = nor_evaluate,
    .destroy       = nor_destroy,
    .shape         = nor_shape,
};

component_t *gate_nor_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &NOR_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}
