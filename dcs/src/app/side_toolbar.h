#ifndef DCS_APP_SIDE_TOOLBAR_H
#define DCS_APP_SIDE_TOOLBAR_H

#include "../framework/widgets/widget.h"
#include "circuit_canvas_widget.h"

/* Sidebar gate-palette: AND, OR, NOT, +INPUT, +OUTPUT. Click a button to
   arm placement on the circuit_canvas. */

class tagt_side_toolbar {
    widget_t base;
    circuit_canvas_widget_t *target;     /* whose place_kind we set on click */
};
typedef class tagt_side_toolbar side_toolbar_t;

side_toolbar_t *side_toolbar_create(rect_t bounds, circuit_canvas_widget_t *target);

#endif /* DCS_APP_SIDE_TOOLBAR_H */
