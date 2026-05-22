#include "component.h"
#include <stdlib.h>
#include <stdio.h>

static signal_t and2(signal_t a, signal_t b) {
    if (a == SIG_LOW  || b == SIG_LOW)  return SIG_LOW;          /* short-circuit */
    if (a == SIG_UNDEF || b == SIG_UNDEF) return SIG_UNDEF;
    return SIG_HIGH;
}

static void and_evaluate(component_t *self, const signal_t *in, signal_t *out) {
    (void)self;
    out[0] = and2(in[0], in[1]);
}

static void and_destroy(component_t *self) { free(self); }

/* ANSI/IEEE MIL-STD-806 "D" shape in normalised local coords
   x ∈ [-1, +1], y ∈ [-1, +1] (screen-y-down). Inputs sit on the
   flat back at x=-1; output exits at (+1, 0) through the apex of
   the semicircular front. */
static const shape_op_t AND_OPS[] = {
    SHAPE_LINE(-1.0f, -1.0f, -1.0f,  1.0f),     /* flat back */
    SHAPE_LINE(-1.0f, -1.0f,  0.0f, -1.0f),     /* top edge */
    SHAPE_LINE(-1.0f,  1.0f,  0.0f,  1.0f),     /* bottom edge */
    SHAPE_ARC ( 0.0f,  0.0f, 1.0f, -1.5707963f, 1.5707963f),   /* right semicircle */
};
static const shape_t AND_SHAPE = { .ops = AND_OPS, .n_ops = 4 };
static const shape_t *and_shape(void) { return &AND_SHAPE; }

static const component_vt_t AND_VT = {
    .kind          = COMP_AND,
    .pin_count_in  = 2,
    .pin_count_out = 1,
    .evaluate      = and_evaluate,
    .destroy       = and_destroy,
    .shape         = and_shape,
};

component_t *gate_and_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &AND_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}
