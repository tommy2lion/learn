#include "component.h"
#include <stdlib.h>
#include <stdio.h>

static signal_t or2(signal_t a, signal_t b) {
    if (a == SIG_HIGH || b == SIG_HIGH) return SIG_HIGH;         /* short-circuit */
    if (a == SIG_UNDEF || b == SIG_UNDEF) return SIG_UNDEF;
    return SIG_LOW;
}

static void or_evaluate(component_t *self, const signal_t *in, signal_t *out) {
    (void)self;
    out[0] = or2(in[0], in[1]);
}

static void or_destroy(component_t *self) { free(self); }

static const component_vt_t OR_VT = {
    .kind          = COMP_OR,
    .pin_count_in  = 2,
    .pin_count_out = 1,
    .evaluate      = or_evaluate,
    .destroy       = or_destroy,
};

component_t *gate_or_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &OR_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}
