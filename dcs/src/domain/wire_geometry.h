#ifndef DCS_APP_WIRE_GEOMETRY_H
#define DCS_APP_WIRE_GEOMETRY_H

/* Per-circuit wire geometry sidecar — app layer.
 *
 * Holds, for each electrical net (identified by its producer wire-name),
 * the orthogonal poly-line segments that the renderer draws. Net identity
 * stays implicit (the producer's name string from circuit_t.components[].name
 * or an input/output name); this module just associates a segment list with
 * that string.
 *
 * The header is raylib-free and framework-widget-free; it pulls only
 * geometric primitives (vec2_t) and DOMAIN_NAME_LEN. */

#include "../framework/core/oo.h"
#include "../framework/core/rect.h"     /* vec2_t */
#include "component.h"                   /* DOMAIN_NAME_LEN */

class tagt_wire_segment {
    vec2_t a, b;                          /* one of (a.x == b.x) or (a.y == b.y) */
};
typedef class tagt_wire_segment wire_segment_t;

class tagt_wire_net_geom {
    char            wire_name[DOMAIN_NAME_LEN];   /* = producer's name */
    wire_segment_t *segs;                          /* owned */
    int             seg_count, seg_cap;
};
typedef class tagt_wire_net_geom wire_net_geom_t;

class tagt_wire_geometry {
    wire_net_geom_t *nets;                         /* one per producer wire-name */
    int              net_count, net_cap;
};
typedef class tagt_wire_geometry wire_geometry_t;

/* Lifecycle. After init the struct holds no allocated storage; release()
   on an init'd-but-empty struct is a no-op and is also safe to call twice. */
void wire_geometry_init   (wire_geometry_t *self);
void wire_geometry_release(wire_geometry_t *self);

/* O(net_count). Returns net index, or -1 if not present (or wire_name is NULL). */
int wire_geometry_find(const wire_geometry_t *self, const char *wire_name);

/* Lookup-or-insert. Returns net index (existing or new), or -1 on:
   invalid name (NULL, empty, or length >= DOMAIN_NAME_LEN) or allocation failure. */
int wire_geometry_get_or_create(wire_geometry_t *self, const char *wire_name);

/* Replace the net's segment list with a copy of segs[0..count-1]. Each
   segment must be purely H (a.y == b.y) or purely V (a.x == b.x) and
   non-zero-length (a != b). On invalid input the net's existing segments
   are left unchanged. count == 0 is allowed and clears the list.
   Returns 0 on success, -1 on error. */
int wire_geometry_set_segments(wire_geometry_t *self, int net_idx,
                               const wire_segment_t *segs, int count);

/* Append (do not replace) validated segments to the net at net_idx. Same
   H/V invariant as set_segments. count == 0 is a successful no-op.
   Returns 0 on success, -1 on error (existing segments unchanged). */
int wire_geometry_append_segments(wire_geometry_t *self, int net_idx,
                                  const wire_segment_t *segs, int count);

/* Transfer every net from src to dst. Frees dst's previous contents first;
   afterward `src` is left in the state of a freshly init'd wire_geometry_t
   (no allocated storage). Safe to call wire_geometry_release on either
   side after the move. */
void wire_geometry_move(wire_geometry_t *dst, wire_geometry_t *src);

/* Read-only access. Returns NULL if idx is out of range. */
const wire_net_geom_t *wire_geometry_net(const wire_geometry_t *self, int idx);

/* Remove a net by wire_name (frees its segment list). No-op if not present
   or wire_name is NULL. Returns 1 if a net was removed, 0 otherwise.
   Net indices of remaining nets may change after this call. */
int wire_geometry_remove_net(wire_geometry_t *self, const char *wire_name);

/* Two segments are collinear iff both are horizontal at the same y OR both
   are vertical at the same x. Used to distinguish a polyline bend (corner)
   from a straight continuation. */
int wire_segments_collinear(const wire_segment_t *a, const wire_segment_t *b);

/* Compute one net's junction points: points where three or more segment
   endpoints of THIS net coincide. These are the fan-out / branch locations
   where the renderer should draw a black join-dot.

   Simple polyline corners (count == 2) are NOT junctions — by convention
   they're just bends and need no dot. Crossings between segments of
   DIFFERENT nets are also never junctions; this function only inspects
   one net at a time.

   Writes up to max_out points into `out` and returns the total count of
   junctions found. If the return value exceeds max_out, the buffer was
   truncated. Returns 0 if net is NULL, empty, or out is NULL. */
int wire_geometry_junctions(const wire_net_geom_t *net,
                            vec2_t *out, int max_out);

/* Hit-test: find the net whose nearest segment is within `tol` world-space
   units of `world`. If several nets are within tolerance, the one with the
   smallest distance wins (ties broken by net index — earlier net wins).

   On hit, `*net_name_out` (if non-NULL) is set to a pointer into the net's
   own wire_name buffer (valid until the geometry is mutated). Returns 1
   on hit, 0 on miss. */
int wire_geometry_pick(const wire_geometry_t *self, vec2_t world, float tol,
                       const char **net_name_out);

/* Like wire_geometry_pick, but also returns the index of the closest
   segment within the winning net. Out-params may be NULL (caller-driven).
   Returns 1 on hit, 0 on miss. (Phase 12 — bend-point editing) */
int wire_geometry_pick_segment(const wire_geometry_t *self, vec2_t world, float tol,
                               int *net_idx_out, int *seg_idx_out);

/* Shift the seg_idx-th segment of net_idx perpendicular to its axis by
   `delta`. For a horizontal segment (a.y == b.y), delta changes y; for
   vertical, x. All other segments in the same net that share an endpoint
   with the shifted segment are updated so connectivity is maintained.

   Atomic: validates the H/V invariant + non-zero-length on every affected
   segment after the shift; on failure the net's segments are restored to
   their pre-call state and -1 is returned. delta == 0 is a successful no-op.

   wire_geometry has no knowledge of pin terminal positions — the caller
   is responsible for refusing to shift segments whose endpoints sit on a
   producer/consumer pin (those should track the component instead).
   (Phase 12 — bend-point editing) */
int wire_geometry_shift_segment(wire_geometry_t *self, int net_idx,
                                int seg_idx, float delta);

/* Shift an entire vertical bus as a unit. All V segments in the net at
   x == column_x are moved by `delta` along x; every non-V segment with
   an endpoint at x == column_x has that endpoint updated by the same
   delta (so trunk + stub connections follow). Used by the canvas
   widget to make R-8 shared V buses draggable — single shift_segment
   can't do this because V bus segments have degree-3 endpoints at
   stub branch-offs. Atomic with rollback on H/V invariant or
   zero-length validation failure. delta == 0 is a successful no-op.
   Returns -1 if no V segment exists at column_x. */
int wire_geometry_shift_v_bus(wire_geometry_t *self, int net_idx,
                              float column_x, float delta);

/* Optional context for obstacle-aware routing (U-40, Stage 4) and
   channelled routing (U-41, Stage 4B). The router consults the channel
   rects when deciding where to put V buses and how to detour H stubs
   around component bodies.

   As of Stage 4B.3 the channels are populated but the router still
   ignores them — they get wired up in Stage 4B.4. */
typedef struct tagt_route_context {
    /* U-40: component bounding boxes — pin edges count as touching,
       not crossing (strict-inequality test in the router). */
    const rect_t *obstacles;
    int           n_obstacles;

    /* U-41 Stage 4B.3 — reserved routing channels. Each rect is a
       strip of "free space" (no gates, no other channels overlapping)
       through which wires can flow. v_channels are vertical strips
       between depth columns; h_channels are horizontal strips between
       row y-positions. Empty / NULL fields = "no channels, fall back
       to obstacle-shift behaviour from Stage 4". */
    const rect_t *v_channels;
    int           n_v_channels;
    const rect_t *h_channels;
    int           n_h_channels;
} route_context_t;

/* Compute an orthogonal route from producer_pin to consumer_pin and append
   the resulting segments to the net identified by wire_name. Three cases:
     - producer_pin.y == consumer_pin.y  → one horizontal segment.
     - producer_pin.x == consumer_pin.x  → one vertical   segment.
     - otherwise                         → three-segment Z-shape with the
       midpoint column snapped to the routing grid (8 px).
   The function APPENDS to the net's segment list — one net can carry the
   routes of several consumers from one producer (just call this once per
   consumer). The net is auto-created on first use.
   Degenerate input (producer == consumer) creates the net but emits no
   segments and returns 0.
   Returns 0 on success, -1 on invalid wire_name or allocation failure. */
int auto_route_wire(wire_geometry_t *self, const char *wire_name,
                    vec2_t producer_pin, vec2_t consumer_pin);

/* Obstacle-aware variant. With ctx == NULL behaves identically to
   auto_route_wire above. With ctx != NULL and obstacles populated, the
   V column of the Z-route is shifted until it doesn't intersect any
   component bbox whose y-range overlaps [producer.y, consumer.y].
   (U-40, Stage 4) */
int auto_route_wire_ctx(wire_geometry_t *self, const char *wire_name,
                        vec2_t producer_pin, vec2_t consumer_pin,
                        const route_context_t *ctx);

/* Route a whole net at once using a Steiner-trunk topology:
   - For n == 0 the net is removed.
   - For n == 1 it falls back to auto_route_wire (a Z-route or straight
     line, identical to the previous per-consumer behaviour).
   - For n >= 2 it emits a horizontal trunk at producer_pin.y spanning
     [min, max] of {producer.x, consumers[].x}, split into pieces at
     every unique x so each drop's top sits on a trunk-segment endpoint
     (which makes the junction-dot derivation pick up T-joins
     automatically). One vertical drop per consumer whose y differs
     from producer.y completes the route.

   Replaces any existing geometry for `wire_name`. Returns 0 on success,
   -1 on invalid input or allocation failure. (Phase 13 — Steiner trunks) */
int auto_route_net(wire_geometry_t *self, const char *wire_name,
                   vec2_t producer_pin,
                   const vec2_t *consumers, int n);

/* Obstacle-aware variant of auto_route_net. ctx == NULL is identical to
   auto_route_net. ctx != NULL shifts the V-bus column to avoid component
   bodies (U-40). */
int auto_route_net_ctx(wire_geometry_t *self, const char *wire_name,
                       vec2_t producer_pin,
                       const vec2_t *consumers, int n,
                       const route_context_t *ctx);

#endif /* DCS_APP_WIRE_GEOMETRY_H */
