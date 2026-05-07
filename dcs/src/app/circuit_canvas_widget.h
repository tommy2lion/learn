#ifndef DCS_APP_CIRCUIT_CANVAS_H
#define DCS_APP_CIRCUIT_CANVAS_H

#include "../framework/widgets/widget.h"
#include "../domain/circuit.h"
#include "editor_state.h"

/* Editor modes. Plain enum (a State-pattern refactor is a future polish). */
typedef enum {
    CMODE_IDLE,
    CMODE_PLACING,
    CMODE_WIRING,
    CMODE_DRAGGING,
    CMODE_MARQUEE,
} canvas_mode_t;

typedef enum {
    PLACE_NONE,
    PLACE_AND, PLACE_OR, PLACE_NOT,
    PLACE_INPUT, PLACE_OUTPUT,
} place_kind_t;

/* Selection bookkeeping — same kind/index as node_ref_t but stored as a set. */
#define MAX_SELECTION 64

typedef void (*ccw_status_fn_t)(const char *msg, void *user);

class tagt_circuit_canvas_widget {
    widget_t        base;            /* must be first */
    circuit_t      *circuit;         /* not owned; outlives the widget */

    /* camera (managed directly so editor interactions can use cam_zoom) */
    vec2_t          cam_target;
    vec2_t          cam_offset;
    float           cam_zoom;
    int             panning;

    /* mode + place state */
    canvas_mode_t   mode;
    place_kind_t    place_kind;

    /* selection */
    node_ref_t      selection[MAX_SELECTION];
    int             selection_count;

    /* hover */
    node_ref_t      hover_node;

    /* wiring (CMODE_WIRING) */
    node_ref_t      wire_src;        /* output side of an in-progress wire */

    /* dragging (CMODE_DRAGGING) */
    node_ref_t      drag_node;
    vec2_t          drag_offset;

    /* marquee (CMODE_MARQUEE) */
    vec2_t          marquee_start;

    /* auto-name counters */
    int counter_in, counter_out, counter_gate;

    /* status reporting hook (set by dcs_app) */
    ccw_status_fn_t on_status;
    void           *status_user;

    /* visual constants (filled in create) */
    float gate_w, gate_h, io_r;
};
typedef class tagt_circuit_canvas_widget circuit_canvas_widget_t;

circuit_canvas_widget_t *circuit_canvas_widget_create(rect_t bounds, circuit_t *c);

void circuit_canvas_widget_set_circuit (circuit_canvas_widget_t *self, circuit_t *c);
void circuit_canvas_widget_set_status_cb(circuit_canvas_widget_t *self,
                                         ccw_status_fn_t cb, void *user);
void circuit_canvas_widget_fit_view    (circuit_canvas_widget_t *self);

/* Placement is armed by the side toolbar via this entry point. */
void circuit_canvas_widget_arm_place(circuit_canvas_widget_t *self, place_kind_t kind);

/* Reset visual + interaction state when the circuit is replaced (new/open). */
void circuit_canvas_widget_reset(circuit_canvas_widget_t *self);

#endif /* DCS_APP_CIRCUIT_CANVAS_H */
