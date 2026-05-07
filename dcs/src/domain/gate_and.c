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

static const component_vt_t AND_VT = {
    .kind          = COMP_AND,
    .pin_count_in  = 2,
    .pin_count_out = 1,
    .evaluate      = and_evaluate,
    .destroy       = and_destroy,
};

component_t *gate_and_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &AND_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}
