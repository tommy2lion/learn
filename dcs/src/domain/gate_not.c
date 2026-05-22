#include "component.h"
#include <stdlib.h>
#include <stdio.h>

static signal_t not1(signal_t a) {
    if (a == SIG_UNDEF) return SIG_UNDEF;
    return (a == SIG_LOW) ? SIG_HIGH : SIG_LOW;
}

static void not_evaluate(component_t *self, const signal_t *in, signal_t *out) {
    (void)self;
    out[0] = not1(in[0]);
}

static void not_destroy(component_t *self) { free(self); }

/* Inverter: triangle with output bubble. Local coords [-1, +1] (screen-y-down).
   Triangle apex sits at (+0.7, 0); bubble centre at (+0.85, 0), radius 0.15
   so the output pin lands exactly at (+1, 0). */
static const shape_op_t NOT_OPS[] = {
    SHAPE_LINE(-1.0f, -1.0f, -1.0f,  1.0f),    /* back edge */
    SHAPE_LINE(-1.0f, -1.0f,  0.7f,  0.0f),    /* top diagonal */
    SHAPE_LINE(-1.0f,  1.0f,  0.7f,  0.0f),    /* bottom diagonal */
    SHAPE_CIRCLE(0.85f, 0.0f, 0.15f),          /* inversion bubble */
};
static const shape_t NOT_SHAPE = { .ops = NOT_OPS, .n_ops = 4 };
static const shape_t *not_shape(void) { return &NOT_SHAPE; }

static const component_vt_t NOT_VT = {
    .kind          = COMP_NOT,
    .pin_count_in  = 1,
    .pin_count_out = 1,
    .evaluate      = not_evaluate,
    .destroy       = not_destroy,
    .shape         = not_shape,
};

component_t *gate_not_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &NOT_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}

const char *component_kind_name(component_kind_t k) {
    switch (k) {
        case COMP_AND:     return "and";
        case COMP_OR:      return "or";
        case COMP_NOT:     return "not";
        case COMP_CHIPSET: return "chipset";
    }
    return "?";
}
