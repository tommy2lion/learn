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
#include "../domain/component.h"         /* DOMAIN_NAME_LEN */

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

/* Read-only access. Returns NULL if idx is out of range. */
const wire_net_geom_t *wire_geometry_net(const wire_geometry_t *self, int idx);

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

#endif /* DCS_APP_WIRE_GEOMETRY_H */
