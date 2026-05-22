#ifndef DCS_FW_SHAPE_H
#define DCS_FW_SHAPE_H

#include "core/oo.h"
#include "core/rect.h"
#include "graphics/igraph.h"
#include <stdint.h>

/* Shape DSL (U-21 part A).
 *
 * A `shape_t` is a const list of primitive drawing operations in
 * shape-local coordinates. `shape_draw` walks the list, applies an
 * origin + uniform scale + colour + line thickness, and emits the
 * resulting primitives through an `igraph_t*`. Used by the canvas
 * (Stage 7) to render ANSI/IEEE gate shapes and by the toolbar
 * (Stage 8) for icon previews — both reuse the same op list.
 *
 * Three op kinds cover what the gate shapes need today:
 *   - LINE   straight segment from `a` to `b`
 *   - CIRCLE outline circle centred at `a`, radius `r`
 *   - ARC    arc on the circle centred at `a`, radius `r`, from angle
 *            `a0` to `a1` (radians). Approximated by a short polyline.
 *
 * The struct fields are unioned-by-convention rather than tagged:
 *   - LINE   uses a, b
 *   - CIRCLE uses a, r
 *   - ARC    uses a, r, a0, a1
 */

typedef enum {
    SHAPE_OP_LINE,
    SHAPE_OP_CIRCLE,
    SHAPE_OP_ARC,
} shape_op_kind_t;

typedef struct {
    shape_op_kind_t kind;
    vec2_t a, b;
    float  r;
    float  a0, a1;
} shape_op_t;

class tagt_shape {
    const shape_op_t *ops;
    int               n_ops;
};
typedef class tagt_shape shape_t;

/* Convenience macros for designated-init literals. */
#define SHAPE_LINE(ax, ay, bx, by) \
    { .kind = SHAPE_OP_LINE,   .a = {(ax), (ay)}, .b = {(bx), (by)} }
#define SHAPE_CIRCLE(cx, cy, rad) \
    { .kind = SHAPE_OP_CIRCLE, .a = {(cx), (cy)}, .r = (rad) }
#define SHAPE_ARC(cx, cy, rad, ang0, ang1) \
    { .kind = SHAPE_OP_ARC,    .a = {(cx), (cy)}, .r = (rad), \
      .a0 = (ang0), .a1 = (ang1) }

/* Draw every op through `g`. (origin + shape-local * scale) gives the
   world-space coordinates. `thick` and `color` are uniform across ops.
   NULL self or NULL g is a no-op. */
void shape_draw(const shape_t *self, igraph_t *g,
                vec2_t origin, float scale, float thick, uint32_t color);

#endif /* DCS_FW_SHAPE_H */
