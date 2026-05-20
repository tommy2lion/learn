#ifndef DCS_APP_EXTERNAL_VIEW_H
#define DCS_APP_EXTERNAL_VIEW_H

/* External (black-box) view renderer — supplement Phase 8.
 *
 * Draws a rectangle representing the whole circuit with its input pins on
 * the left edge and output pins on the right, plus a centered display
 * name. No internal components, no internal wires.
 *
 * The renderer draws in SCREEN coordinates (no camera2d transform).
 * Caller is responsible for the scissor / background. */

#include "../framework/graphics/igraph.h"
#include "../domain/circuit.h"

/* Render the default external view of `c` centered inside `viewport`.
   `display_name` may be NULL or empty — in which case nothing is drawn
   for the box label, but the rectangle and pins still render. */
void external_view_draw_default(igraph_t *g,
                                const circuit_t *c,
                                const char *display_name,
                                rect_t viewport);

#endif /* DCS_APP_EXTERNAL_VIEW_H */
