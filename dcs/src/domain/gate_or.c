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

/* ANSI/IEEE MIL-STD-806 "shield" shape in normalised local coords
   x ∈ [-1, +1], y ∈ [-1, +1] (screen-y-down).
   - Back arc bulges concave-right (centre at x=-2, radius sqrt(2)).
   - Two convex front arcs meet at the pointed tip (+1, 0); centres
     are at (-1, ±2) with radius 3.
   The arc angle ranges are derived from atan2 of each endpoint
   relative to its centre — see the per-line comments. */
/* Geometry note: front arcs are sized so both meet exactly at the
   output pin (+1, 0). Solving "centre on the bottom-left back corner's
   vertical, radius = distance to BOTH the upper-back corner (-1, -1)
   AND the tip (+1, 0)" yields centre (-1, 1.5) and radius 2.5 for the
   top arc, mirrored for the bottom. The end angles are atan2(-1.5, 2)
   ≈ -0.6435 rad (top) and its positive mirror (bottom). */
static const shape_op_t OR_OPS[] = {
    /* concave back arc, from (-1,-1) up over to (-1,+1) bulging right.
       centre (-2,0), radius sqrt(2) ≈ 1.414, angles ±π/4 */
    SHAPE_ARC(-2.0f,  0.0f, 1.41421356f, -0.78539816f,  0.78539816f),
    /* convex top arc (-1,-1) → (+1,0). centre (-1,+1.5), radius 2.5 */
    SHAPE_ARC(-1.0f,  1.5f, 2.5f,        -1.57079633f, -0.64350111f),
    /* convex bottom arc (-1,+1) → (+1,0). centre (-1,-1.5), radius 2.5,
       negative span so the sweep is clockwise in math coords. */
    SHAPE_ARC(-1.0f, -1.5f, 2.5f,         1.57079633f,  0.64350111f),
    /* input lead stubs — at y = ±0.5 the back arc sits at x ≈ -0.68,
       so a stub from the canvas's pin (x=-1) to x=-0.65 crosses the
       arc just enough to read as a real lead (~14 px world). */
    SHAPE_LINE(-1.0f, -0.5f, -0.65f, -0.5f),
    SHAPE_LINE(-1.0f,  0.5f, -0.65f,  0.5f),
};
static const shape_t OR_SHAPE = { .ops = OR_OPS, .n_ops = 5 };
static const shape_t *or_shape(void) { return &OR_SHAPE; }

static const component_vt_t OR_VT = {
    .kind          = COMP_OR,
    .pin_count_in  = 2,
    .pin_count_out = 1,
    .evaluate      = or_evaluate,
    .destroy       = or_destroy,
    .shape         = or_shape,
};

component_t *gate_or_create(const char *name) {
    component_t *c = (component_t *)calloc(1, sizeof(component_t));
    if (!c) return NULL;
    c->vt = &OR_VT;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    return c;
}
