#include "component.h"
#include <stdlib.h>
#include <stdio.h>

static signal_t nand2(signal_t a, signal_t b) {
    if (a == SIG_LOW  || b == SIG_LOW)  return SIG_HIGH;          /* short-circuit */
    if (a == SIG_UNDEF || b == SIG_UNDEF) return SIG_UNDEF;
    return SIG_LOW;
}

static void nand_evaluate(component_t *self, const signal_t *in, signal_t *out) {
    (void)self;
    out[0] = nand2(in[0], in[1]);
}

static void nand_destroy(component_t *self) { free(self); }

/* NAND = AND shape with a small inversion bubble at the output. To
   make room for the bubble (centre 0.85, r 0.15 — same convention as
   NOT) the AND body is shrunk: the right semicircle's apex sits at
   x=+0.7 instead of x=+1.0, so the bubble's right edge lands at the
   output-pin position (+1.0). Geometry: arc centred at (-0.3, 0),
   radius 1.0, sweeping -π/2 → +π/2; top/bottom edges shorten to
   (-1,±1) → (-0.3,±1). */
static const shape_op_t NAND_OPS[] = {
    SHAPE_LINE(-1.0f, -1.0f, -1.0f,  1.0f),         /* flat back */
    SHAPE_LINE(-1.0f, -1.0f, -0.3f, -1.0f),         /* top edge (shorter) */
    SHAPE_LINE(-1.0f,  1.0f, -0.3f,  1.0f),         /* bottom edge */
    SHAPE_ARC (-0.3f,  0.0f, 1.0f, -1.5707963f, 1.5707963f),
    SHAPE_CIRCLE(0.85f, 0.0f, 0.15f),               /* inversion bubble */
};
static const shape_t NAND_SHAPE = { .ops = NAND_OPS, .n_ops = 5 };
static const shape_t *nand_shape(void) { return &NAND_SHAPE; }

static const component_vt_t NAND_VT = {
    .kind          = COMP_NAND,
    .pin_count_in  = 2,
    .pin_count_out = 1,
    .evaluate      = nand_evaluate,
    .destroy       = nand_destroy,
    .shape         = nand_shape,
};

component_t *gate_nand_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &NAND_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}
