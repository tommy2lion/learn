#include "component.h"
#include <stdlib.h>
#include <stdio.h>

static signal_t xor2(signal_t a, signal_t b) {
    if (a == SIG_UNDEF || b == SIG_UNDEF) return SIG_UNDEF;
    return (a == b) ? SIG_LOW : SIG_HIGH;
}

static void xor_evaluate(component_t *self, const signal_t *in, signal_t *out) {
    (void)self;
    out[0] = xor2(in[0], in[1]);
}

static void xor_destroy(component_t *self) { free(self); }

/* XOR = OR shape PLUS an extra outer back arc parallel to and to the
   LEFT of the OR's inner back arc. The double-back is the standard
   MIL-STD identifier for XOR. Geometry: outer arc shifted by -0.2 in
   x (centre (-2.2, 0), same radius sqrt(2), same angle span), so its
   midpoint sits at x ≈ -0.79 and endpoints at (-1.2, ±1) — slightly
   outside the gate box, which is the expected visual. */
static const shape_op_t XOR_OPS[] = {
    /* inner (OR) back arc */
    SHAPE_ARC(-2.0f,  0.0f, 1.41421356f, -0.78539816f,  0.78539816f),
    /* OR front arcs, tip at (+1, 0) */
    SHAPE_ARC(-1.0f,  1.5f, 2.5f,        -1.57079633f, -0.64350111f),
    SHAPE_ARC(-1.0f, -1.5f, 2.5f,         1.57079633f,  0.64350111f),
    /* input leads — cross BOTH the outer and inner back arcs */
    SHAPE_LINE(-1.0f, -0.5f, -0.65f, -0.5f),
    SHAPE_LINE(-1.0f,  0.5f, -0.65f,  0.5f),
    /* outer back arc — visual cue distinguishing XOR from OR */
    SHAPE_ARC(-2.2f,  0.0f, 1.41421356f, -0.78539816f,  0.78539816f),
};
static const shape_t XOR_SHAPE = { .ops = XOR_OPS, .n_ops = 6 };
static const shape_t *xor_shape(void) { return &XOR_SHAPE; }

static const component_vt_t XOR_VT = {
    .kind          = COMP_XOR,
    .pin_count_in  = 2,
    .pin_count_out = 1,
    .evaluate      = xor_evaluate,
    .destroy       = xor_destroy,
    .shape         = xor_shape,
};

component_t *gate_xor_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &XOR_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}
