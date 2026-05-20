#ifndef DCS_APP_EXTERNAL_VIEW_H
#define DCS_APP_EXTERNAL_VIEW_H

/* External (black-box) view renderer — supplement Phase 8 / Phase 9.
 *
 * Two entry points:
 *   external_view_draw          — dispatcher; consults meta->render and
 *                                 falls back to the default renderer.
 *   external_view_draw_default  — the built-in renderer; exposed so a
 *                                 custom render hook can delegate to it
 *                                 for the parts it doesn't override.
 *
 * Both draw in SCREEN coordinates (no camera2d transform). Caller is
 * responsible for the scissor / background. */

#include "../framework/graphics/igraph.h"
#include "../framework/core/oo.h"
#include "../domain/circuit.h"
#include "../domain/component.h"      /* DOMAIN_NAME_LEN, DOMAIN_MAX_IO */

/* Per-pin visual style. NORMAL is the default; CLOCK draws a small
   triangular "edge mark" inside the box (D-flip-flop convention).
   INVERTED is reserved for Step 3 NAND/NOR-style bubbles. */
typedef enum {
    PIN_STYLE_NORMAL = 0,
    PIN_STYLE_CLOCK,
    PIN_STYLE_INVERTED,   /* reserved — recognised by the enum but the
                              default renderer treats it like NORMAL */
} pin_style_t;

/* Forward declaration so the function-pointer typedef can refer to it. */
typedef struct tagt_external_view_metadata external_view_metadata_t;

typedef void (*external_render_fn)(igraph_t *g,
                                   const circuit_t *c,
                                   const external_view_metadata_t *meta,
                                   rect_t viewport);

class tagt_external_view_metadata {
    char               display_name[DOMAIN_NAME_LEN];
    pin_style_t        input_styles [DOMAIN_MAX_IO];
    pin_style_t        output_styles[DOMAIN_MAX_IO];
    /* When non-NULL, the dispatcher calls this instead of the default
       renderer. The custom function may delegate to
       external_view_draw_default for the parts it doesn't override. */
    external_render_fn render;
};

/* Zero out the struct to "defaults": empty display_name, all pins
   PIN_STYLE_NORMAL, render == NULL. */
void external_view_metadata_init(external_view_metadata_t *m);

/* Dispatcher: calls meta->render if set, else external_view_draw_default.
   `meta` must be non-NULL (callers always own a metadata struct). */
void external_view_draw(igraph_t *g,
                        const circuit_t *c,
                        const external_view_metadata_t *meta,
                        rect_t viewport);

/* Default renderer — plain rectangle, name centered, labeled I/O pins
   with PIN_STYLE_CLOCK triangles where requested. Exposed so a custom
   renderer can call into it for the bits it doesn't override. */
void external_view_draw_default(igraph_t *g,
                                const circuit_t *c,
                                const external_view_metadata_t *meta,
                                rect_t viewport);

#endif /* DCS_APP_EXTERNAL_VIEW_H */
