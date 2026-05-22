#include "component.h"
#include <stdlib.h>
#include <stdio.h>

static signal_t xnor2(signal_t a, signal_t b) {
    if (a == SIG_UNDEF || b == SIG_UNDEF) return SIG_UNDEF;
    return (a == b) ? SIG_HIGH : SIG_LOW;
}

static void xnor_evaluate(component_t *self, const signal_t *in, signal_t *out) {
    (void)self;
    out[0] = xnor2(in[0], in[1]);
}

static void xnor_destroy(component_t *self) { free(self); }

/* XNOR = XOR shape with an inversion bubble at the output AND the
   front-arc tip shrunk to (+0.7, 0) so the (0.85, 0) r=0.15 bubble
   fits between the tip and the output pin (+1.0). Same shrunken
   front-arc geometry as NOR (centre (-1, ±0.945), radius 1.945,
   end angles ±atan2(0.945, 1.7)). Keeps XOR's outer back arc as the
   visual cue. */
static const shape_op_t XNOR_OPS[] = {
    /* inner (OR) back arc */
    SHAPE_ARC(-2.0f,  0.0f, 1.41421356f, -0.78539816f,  0.78539816f),
    /* shrunken front arcs ending at (+0.7, 0) — same as NOR */
    SHAPE_ARC(-1.0f,  0.945f, 1.945f, -1.57079633f, -0.50708930f),
    SHAPE_ARC(-1.0f, -0.945f, 1.945f,  1.57079633f,  0.50708930f),
    /* input leads */
    SHAPE_LINE(-1.0f, -0.5f, -0.65f, -0.5f),
    SHAPE_LINE(-1.0f,  0.5f, -0.65f,  0.5f),
    /* outer back arc (XOR visual cue) */
    SHAPE_ARC(-2.2f,  0.0f, 1.41421356f, -0.78539816f,  0.78539816f),
    /* inversion bubble */
    SHAPE_CIRCLE(0.85f, 0.0f, 0.15f),
};
static const shape_t XNOR_SHAPE = { .ops = XNOR_OPS, .n_ops = 7 };
static const shape_t *xnor_shape(void) { return &XNOR_SHAPE; }

static const component_vt_t XNOR_VT = {
    .kind          = COMP_XNOR,
    .pin_count_in  = 2,
    .pin_count_out = 1,
    .evaluate      = xnor_evaluate,
    .destroy       = xnor_destroy,
    .shape         = xnor_shape,
};

component_t *gate_xnor_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &XNOR_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}
