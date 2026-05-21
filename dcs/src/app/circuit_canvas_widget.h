#ifndef DCS_APP_CIRCUIT_CANVAS_H
#define DCS_APP_CIRCUIT_CANVAS_H

#include "../framework/widgets/widget.h"
#include "../domain/circuit.h"
#include "../domain/wire_geometry.h"
#include "editor_state.h"
#include "external_view.h"

/* Editor modes. Plain enum (a State-pattern refactor is a future polish). */
typedef enum {
    CMODE_IDLE,
    CMODE_PLACING,
    CMODE_WIRING,
    CMODE_DRAGGING,
    CMODE_MARQUEE,
    CMODE_WIRE_EDIT,         /* dragging a wire segment perpendicular (Phase 12) */
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

    /* orthogonal wire-segment sidecar — populated by seed_geometry_from_circuit
       at create / set_circuit time; renderer reads from here. Phase-3 minimum:
       mutations after seed leave geometry stale (Phase 4 hooks the mutation
       sites). */
    wire_geometry_t wires;

    /* Click-to-highlight: empty string = no highlight (Phase 6). */
    char highlighted_net[DOMAIN_NAME_LEN];

    /* Wire-edit drag state (CMODE_WIRE_EDIT) — Phase 12. we_hover_cursor
       is set by the event handler when hovering a draggable segment in
       IDLE; ccw_draw consumes it via g->set_cursor (the event handler
       doesn't have igraph). */
    int           we_net_idx;
    int           we_seg_idx;
    vec2_t        we_last_world;
    cursor_kind_t we_hover_cursor;

    /* External (black-box) vs internal (schematic) view — Phase 8/9.
       external_meta carries the display name (set to the file basename
       by dcs_app), per-pin style overrides, and an optional render
       hook that overrides the default rectangle renderer entirely. */
    display_mode_t           display_mode;
    external_view_metadata_t external_meta;
};
typedef class tagt_circuit_canvas_widget circuit_canvas_widget_t;

circuit_canvas_widget_t *circuit_canvas_widget_create(rect_t bounds, circuit_t *c);

void circuit_canvas_widget_set_circuit (circuit_canvas_widget_t *self, circuit_t *c);
void circuit_canvas_widget_set_status_cb(circuit_canvas_widget_t *self,
                                         ccw_status_fn_t cb, void *user);
void circuit_canvas_widget_fit_view    (circuit_canvas_widget_t *self);

/* Keyboard-zoom helpers — bound to Ctrl+= and Ctrl+- in dcs_app. Multiply
   cam_zoom by a fixed factor (one step ≈ 1.25× / 0.8×) and clamp to the
   same [0.1, 5.0] range used by the mouse-wheel zoom. The zoom focuses
   on whatever world point currently sits at cam_offset (typically the
   canvas centre after fit_view). */
void circuit_canvas_widget_zoom_in (circuit_canvas_widget_t *self);
void circuit_canvas_widget_zoom_out(circuit_canvas_widget_t *self);

/* Placement is armed by the side toolbar via this entry point. */
void circuit_canvas_widget_arm_place(circuit_canvas_widget_t *self, place_kind_t kind);

/* Reset visual + interaction state when the circuit is replaced (new/open). */
void circuit_canvas_widget_reset(circuit_canvas_widget_t *self);

/* Replace the selection with every input / component / output in the circuit
   (capped at MAX_SELECTION). Public so dcs_app's global Ctrl+A shortcut can
   reach it without the canvas needing focus. Idempotent. (R-12) */
void circuit_canvas_widget_select_all(circuit_canvas_widget_t *self);

/* Delete every node in the current selection (no-op if selection is empty).
   Public so dcs_app's global Del shortcut can reach it without the canvas
   needing focus — the framework's focused-widget key-event path is currently
   unused. (R-13) */
void circuit_canvas_widget_delete_selection(circuit_canvas_widget_t *self);

/* Cancel whatever mode the canvas is in and clear the selection. Superset
   of the two now-dead in-widget ESC handlers (CMODE_IDLE + CMODE_WIRE_EDIT)
   — resets every transient mode flag in one go. Public so dcs_app's global
   ESC shortcut can reach it without the canvas needing focus. (R-18) */
void circuit_canvas_widget_cancel_mode(circuit_canvas_widget_t *self);

/* Drop all wire geometry and re-route from the current circuit's connectivity.
   Called internally by the canvas's mutation paths; exposed for testing and
   for any future caller that has mutated the underlying circuit_t outside
   the widget's normal event flow. */
void circuit_canvas_widget_reseat_wires(circuit_canvas_widget_t *self);

/* Snap each component's y to align one of its input pins with its producer's
   output pin, when the existing offset is within 8 px. Also aligns external
   output positions to their producers. Returns the number of positions
   actually changed — useful for callers that need to know whether the
   file's persisted geometry is still valid (zero shifts) or has gone stale
   (any shift). Idempotent: a second call on an already-aligned circuit
   returns 0. (R-7 refinement) */
int circuit_canvas_widget_auto_align(circuit_t *c);

/* Highlight a net by wire name; NULL or empty string clears the highlight.
   The string is copied into the widget; the input pointer doesn't need to
   outlive the call. (Phase 6 — click-to-highlight) */
void circuit_canvas_widget_set_highlight(circuit_canvas_widget_t *self,
                                         const char *wire_name);

/* Read-only access to the canvas's wire-geometry sidecar — used by the save
   path to feed circuit_io_serialize_ex. (Phase 7) */
const wire_geometry_t *circuit_canvas_widget_geometry(const circuit_canvas_widget_t *self);

/* Replace the canvas's wire-geometry by transferring ownership from `src`.
   Frees any existing geometry first. After the call `src` is empty (as if
   freshly init'd). Used by the load path to install a `.dcs` file's
   parsed # @wires block, overriding the seed-from-routing default. */
void circuit_canvas_widget_load_geometry(circuit_canvas_widget_t *self,
                                         wire_geometry_t *src);

/* Display mode (internal schematic vs. external black-box). The setter
   is the one funnel called by every UI surface (menu, keyboard shortcut,
   sidebar button) so a future fourth surface plugs in without
   duplicating behaviour. (Phase 8) */
display_mode_t circuit_canvas_widget_display_mode(const circuit_canvas_widget_t *self);
void           circuit_canvas_widget_set_display_mode(circuit_canvas_widget_t *self,
                                                      display_mode_t mode);

/* Label shown inside the external-view box. Empty / NULL clears it.
   dcs_app sets this to the file basename on load/new. Convenience for
   the common case — for richer external-view customisation (per-pin
   styles, custom render hook) reach into external_meta directly via
   the accessor below. (Phase 8/9) */
void circuit_canvas_widget_set_display_name(circuit_canvas_widget_t *self,
                                            const char *name);

/* Mutable access to the external-view metadata struct. Callers may set
   per-pin styles, install a custom render function, or update the
   display name in place. (Phase 9) */
external_view_metadata_t *circuit_canvas_widget_external_meta(circuit_canvas_widget_t *self);

#endif /* DCS_APP_CIRCUIT_CANVAS_H */
