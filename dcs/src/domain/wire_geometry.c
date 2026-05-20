#include "wire_geometry.h"
#include <stdlib.h>
#include <string.h>

/* ── internal helpers ─────────────────────────────────────────────── */

static int segment_is_valid(const wire_segment_t *s) {
    int pure_h = (s->a.y == s->b.y);
    int pure_v = (s->a.x == s->b.x);
    if (!pure_h && !pure_v) return 0;                              /* diagonal */
    if (s->a.x == s->b.x && s->a.y == s->b.y) return 0;            /* zero-length */
    return 1;
}

/* Grow `nets[]` capacity; new slots are zeroed so a subsequent release()
   sees clean state on any slot that may not be filled before net_count is
   incremented. Returns 0 on success, -1 on allocation failure (g unchanged). */
static int nets_grow(wire_geometry_t *g) {
    int new_cap = g->net_cap == 0 ? 4 : g->net_cap * 2;
    wire_net_geom_t *tmp = (wire_net_geom_t *)realloc(g->nets,
                                                      sizeof(*tmp) * (size_t)new_cap);
    if (!tmp) return -1;
    memset(tmp + g->net_cap, 0,
           sizeof(*tmp) * (size_t)(new_cap - g->net_cap));
    g->nets    = tmp;
    g->net_cap = new_cap;
    return 0;
}

/* ── public API ───────────────────────────────────────────────────── */

void wire_geometry_init(wire_geometry_t *self) {
    self->nets      = NULL;
    self->net_count = 0;
    self->net_cap   = 0;
}

void wire_geometry_release(wire_geometry_t *self) {
    for (int i = 0; i < self->net_count; i++) {
        free(self->nets[i].segs);
    }
    free(self->nets);
    self->nets      = NULL;
    self->net_count = 0;
    self->net_cap   = 0;
}

int wire_geometry_find(const wire_geometry_t *self, const char *wire_name) {
    if (!wire_name) return -1;
    for (int i = 0; i < self->net_count; i++) {
        if (strcmp(self->nets[i].wire_name, wire_name) == 0)
            return i;
    }
    return -1;
}

int wire_geometry_get_or_create(wire_geometry_t *self, const char *wire_name) {
    if (!wire_name || !wire_name[0]) return -1;
    size_t name_len = strlen(wire_name);
    if (name_len >= DOMAIN_NAME_LEN) return -1;

    int idx = wire_geometry_find(self, wire_name);
    if (idx >= 0) return idx;

    if (self->net_count >= self->net_cap) {
        if (nets_grow(self) < 0) return -1;
    }

    idx = self->net_count;
    wire_net_geom_t *n = &self->nets[idx];
    memcpy(n->wire_name, wire_name, name_len);
    n->wire_name[name_len] = '\0';
    n->segs      = NULL;
    n->seg_count = 0;
    n->seg_cap   = 0;
    self->net_count++;
    return idx;
}

int wire_geometry_set_segments(wire_geometry_t *self, int net_idx,
                               const wire_segment_t *segs, int count) {
    if (net_idx < 0 || net_idx >= self->net_count) return -1;
    if (count < 0)                                  return -1;
    if (count > 0 && !segs)                         return -1;

    /* Validate first; leaves the net's existing segments untouched on failure. */
    for (int i = 0; i < count; i++) {
        if (!segment_is_valid(&segs[i])) return -1;
    }

    wire_net_geom_t *n = &self->nets[net_idx];

    if (count == 0) {
        free(n->segs);
        n->segs      = NULL;
        n->seg_count = 0;
        n->seg_cap   = 0;
        return 0;
    }

    if (count > n->seg_cap) {
        wire_segment_t *tmp = (wire_segment_t *)realloc(n->segs,
                                                       sizeof(*tmp) * (size_t)count);
        if (!tmp) return -1;
        n->segs    = tmp;
        n->seg_cap = count;
    }
    memcpy(n->segs, segs, sizeof(*segs) * (size_t)count);
    n->seg_count = count;
    return 0;
}

const wire_net_geom_t *wire_geometry_net(const wire_geometry_t *self, int idx) {
    if (idx < 0 || idx >= self->net_count) return NULL;
    return &self->nets[idx];
}

int wire_geometry_remove_net(wire_geometry_t *self, const char *wire_name) {
    int idx = wire_geometry_find(self, wire_name);
    if (idx < 0) return 0;
    free(self->nets[idx].segs);
    for (int i = idx; i < self->net_count - 1; i++) {
        self->nets[i] = self->nets[i + 1];
    }
    /* zero the now-unused tail slot so future grow()s see clean state */
    memset(&self->nets[self->net_count - 1], 0, sizeof(self->nets[0]));
    self->net_count--;
    return 1;
}

/* ── junction derivation (supplement Phase 5) ─────────────────────── */

int wire_segments_collinear(const wire_segment_t *a, const wire_segment_t *b) {
    if (!a || !b) return 0;
    int a_h = (a->a.y == a->b.y);
    int a_v = (a->a.x == a->b.x);
    int b_h = (b->a.y == b->b.y);
    int b_v = (b->a.x == b->b.x);
    if (a_h && b_h && a->a.y == b->a.y) return 1;
    if (a_v && b_v && a->a.x == b->a.x) return 1;
    return 0;
}

/* Endpoint accessor: i=0..2N-1 picks the a-end of even segments and the
   b-end of odd-indexed endpoints. */
static vec2_t net_endpoint(const wire_net_geom_t *net, int i) {
    return (i & 1) ? net->segs[i / 2].b : net->segs[i / 2].a;
}

static int points_equal(vec2_t p, vec2_t q) {
    return p.x == q.x && p.y == q.y;
}

/* Squared distance from point p to the H-or-V segment s. Works for arbitrary
   segments too (general projection), so the H/V invariant isn't strictly
   required — but our routes are always H or V by construction. */
static float seg_dist_sq(vec2_t p, const wire_segment_t *s) {
    float dx = s->b.x - s->a.x;
    float dy = s->b.y - s->a.y;
    float len_sq = dx * dx + dy * dy;
    if (len_sq <= 0.0f) {
        /* Degenerate (zero-length) segment: distance to its anchor point. */
        float ex = p.x - s->a.x, ey = p.y - s->a.y;
        return ex * ex + ey * ey;
    }
    float t = ((p.x - s->a.x) * dx + (p.y - s->a.y) * dy) / len_sq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float fx = s->a.x + t * dx;
    float fy = s->a.y + t * dy;
    float ex = p.x - fx, ey = p.y - fy;
    return ex * ex + ey * ey;
}

int wire_geometry_pick(const wire_geometry_t *self, vec2_t world, float tol,
                       const char **net_name_out) {
    if (net_name_out) *net_name_out = NULL;
    if (!self || tol < 0.0f) return 0;

    float tol_sq = tol * tol;
    float best_sq = tol_sq;
    int   best_net = -1;

    for (int i = 0; i < self->net_count; i++) {
        const wire_net_geom_t *net = &self->nets[i];
        for (int s = 0; s < net->seg_count; s++) {
            float d_sq = seg_dist_sq(world, &net->segs[s]);
            if (d_sq <= best_sq) {
                best_sq  = d_sq;
                best_net = i;
            }
        }
    }
    if (best_net < 0) return 0;
    if (net_name_out) *net_name_out = self->nets[best_net].wire_name;
    return 1;
}

int wire_geometry_pick_segment(const wire_geometry_t *self, vec2_t world, float tol,
                               int *net_idx_out, int *seg_idx_out) {
    if (net_idx_out) *net_idx_out = -1;
    if (seg_idx_out) *seg_idx_out = -1;
    if (!self || tol < 0.0f) return 0;

    float tol_sq = tol * tol;
    float best_sq = tol_sq;
    int   best_net = -1, best_seg = -1;

    for (int i = 0; i < self->net_count; i++) {
        const wire_net_geom_t *net = &self->nets[i];
        for (int s = 0; s < net->seg_count; s++) {
            float d_sq = seg_dist_sq(world, &net->segs[s]);
            if (d_sq <= best_sq) {
                best_sq  = d_sq;
                best_net = i;
                best_seg = s;
            }
        }
    }
    if (best_net < 0) return 0;
    if (net_idx_out) *net_idx_out = best_net;
    if (seg_idx_out) *seg_idx_out = best_seg;
    return 1;
}

int wire_geometry_shift_segment(wire_geometry_t *self, int net_idx,
                                int seg_idx, float delta) {
    if (!self || net_idx < 0 || net_idx >= self->net_count) return -1;
    wire_net_geom_t *n = &self->nets[net_idx];
    if (seg_idx < 0 || seg_idx >= n->seg_count) return -1;
    if (delta == 0.0f) return 0;

    /* Snapshot for rollback. */
    wire_segment_t *backup =
        (wire_segment_t *)malloc(sizeof(*backup) * (size_t)n->seg_count);
    if (!backup) return -1;
    memcpy(backup, n->segs, sizeof(*backup) * (size_t)n->seg_count);

    wire_segment_t *s = &n->segs[seg_idx];
    int is_h = (s->a.y == s->b.y);
    int is_v = (s->a.x == s->b.x);
    if (!is_h && !is_v) { free(backup); return -1; }

    vec2_t old_a = s->a, old_b = s->b;

    /* Shift the target segment perpendicular to its axis. */
    if (is_h) { s->a.y += delta; s->b.y += delta; }
    else      { s->a.x += delta; s->b.x += delta; }

    /* Update every other segment whose endpoint matched the target's
       pre-shift endpoint, so connectivity is preserved. */
    for (int i = 0; i < n->seg_count; i++) {
        if (i == seg_idx) continue;
        wire_segment_t *t = &n->segs[i];
        if (points_equal(t->a, old_a)) {
            if (is_h) t->a.y += delta; else t->a.x += delta;
        }
        if (points_equal(t->b, old_a)) {
            if (is_h) t->b.y += delta; else t->b.x += delta;
        }
        if (points_equal(t->a, old_b)) {
            if (is_h) t->a.y += delta; else t->a.x += delta;
        }
        if (points_equal(t->b, old_b)) {
            if (is_h) t->b.y += delta; else t->b.x += delta;
        }
    }

    /* Validate: every segment must remain H/V and non-zero. */
    int ok = 1;
    for (int i = 0; i < n->seg_count; i++) {
        if (!segment_is_valid(&n->segs[i])) { ok = 0; break; }
    }
    if (!ok) {
        memcpy(n->segs, backup, sizeof(*backup) * (size_t)n->seg_count);
    }
    free(backup);
    return ok ? 0 : -1;
}

int wire_geometry_junctions(const wire_net_geom_t *net,
                            vec2_t *out, int max_out) {
    if (!net || net->seg_count == 0 || !out || max_out <= 0) return 0;

    int n_eps = net->seg_count * 2;
    int total = 0;     /* total junctions found (may exceed max_out) */

    for (int i = 0; i < n_eps; i++) {
        vec2_t p = net_endpoint(net, i);

        /* First-occurrence rule: only inspect this point when i is the
           earliest endpoint that equals p (skips duplicate counting). */
        int first_seen = 1;
        for (int k = 0; k < i; k++) {
            if (points_equal(net_endpoint(net, k), p)) {
                first_seen = 0;
                break;
            }
        }
        if (!first_seen) continue;

        /* Count total occurrences of p across all endpoints. */
        int count = 1;
        for (int j = i + 1; j < n_eps; j++) {
            if (points_equal(net_endpoint(net, j), p)) count++;
        }

        /* Junction iff three or more endpoints coincide here (T/+ join).
           A count of 2 is just a polyline corner — no dot, per the demo
           convention shown in step2-supplement-demo2.jpg. */
        if (count >= 3) {
            if (total < max_out) out[total] = p;
            total++;
        }
    }
    return total;
}

/* ── Z-router (supplement Phase 2) ────────────────────────────────── */

/* Routing-grid step. Matches the visual grid in circuit_canvas_widget.c;
   kept file-local for now so it can be passed in or shared via a header
   constant later if needed. */
static const float ROUTING_GRID = 8.0f;

static float snap_to_grid(float v) {
    float n = v / ROUTING_GRID;
    int rounded = (int)(n + (n >= 0.0f ? 0.5f : -0.5f));
    return (float)rounded * ROUTING_GRID;
}

int wire_geometry_append_segments(wire_geometry_t *self, int net_idx,
                                  const wire_segment_t *segs, int count) {
    if (net_idx < 0 || net_idx >= self->net_count) return -1;
    if (count < 0)                                  return -1;
    if (count == 0)                                 return 0;     /* no-op */
    if (!segs)                                      return -1;

    /* Validate first; leaves the net's existing segments untouched on failure. */
    for (int i = 0; i < count; i++) {
        if (!segment_is_valid(&segs[i])) return -1;
    }

    wire_net_geom_t *n = &self->nets[net_idx];
    int new_count = n->seg_count + count;
    if (new_count > n->seg_cap) {
        int new_cap = n->seg_cap == 0 ? 4 : n->seg_cap;
        while (new_cap < new_count) new_cap *= 2;
        wire_segment_t *tmp = (wire_segment_t *)realloc(n->segs,
                                                       sizeof(*tmp) * (size_t)new_cap);
        if (!tmp) return -1;
        n->segs    = tmp;
        n->seg_cap = new_cap;
    }
    memcpy(n->segs + n->seg_count, segs, sizeof(*segs) * (size_t)count);
    n->seg_count = new_count;
    return 0;
}

void wire_geometry_move(wire_geometry_t *dst, wire_geometry_t *src) {
    if (!dst || !src || dst == src) return;
    wire_geometry_release(dst);
    dst->nets      = src->nets;
    dst->net_count = src->net_count;
    dst->net_cap   = src->net_cap;
    src->nets      = NULL;
    src->net_count = 0;
    src->net_cap   = 0;
}

int auto_route_wire(wire_geometry_t *self, const char *wire_name,
                    vec2_t producer_pin, vec2_t consumer_pin) {
    int idx = wire_geometry_get_or_create(self, wire_name);
    if (idx < 0) return -1;

    /* Degenerate: same point. Net is created (above), no segments emitted. */
    if (producer_pin.x == consumer_pin.x && producer_pin.y == consumer_pin.y) {
        return 0;
    }

    wire_segment_t segs[3];
    int count;

    if (producer_pin.y == consumer_pin.y) {
        /* horizontal straight-shot */
        segs[0].a = producer_pin;
        segs[0].b = consumer_pin;
        count = 1;
    } else if (producer_pin.x == consumer_pin.x) {
        /* vertical straight-shot */
        segs[0].a = producer_pin;
        segs[0].b = consumer_pin;
        count = 1;
    } else {
        /* Z-shape: horiz out from producer, vertical to consumer's y, horiz in to consumer */
        float mid_x = snap_to_grid(producer_pin.x
                                   + (consumer_pin.x - producer_pin.x) * 0.5f);
        /* If snap collapsed mid_x onto either endpoint, the first or third
           segment would be zero-length. Nudge one grid step in the right
           direction so all three segments stay non-degenerate. */
        float dir = (consumer_pin.x > producer_pin.x) ? ROUTING_GRID : -ROUTING_GRID;
        if (mid_x == producer_pin.x) mid_x += dir;
        if (mid_x == consumer_pin.x) mid_x -= dir;

        vec2_t corner_top, corner_bot;
        corner_top.x = mid_x; corner_top.y = producer_pin.y;
        corner_bot.x = mid_x; corner_bot.y = consumer_pin.y;

        segs[0].a = producer_pin; segs[0].b = corner_top;
        segs[1].a = corner_top;   segs[1].b = corner_bot;
        segs[2].a = corner_bot;   segs[2].b = consumer_pin;
        count = 3;
    }

    return wire_geometry_append_segments(self, idx, segs, count);
}

/* ── Steiner-trunk router (supplement Phase 13) ───────────────────── */

/* Insert v into xs[] keeping it sorted and de-duplicated. *n is the
   current count and is updated. Returns 1 if inserted, 0 if duplicate.
   xs must have room for at least *n + 1 entries. */
static int insert_sorted_unique(float *xs, int *n, float v) {
    for (int i = 0; i < *n; i++) {
        if (xs[i] == v) return 0;
    }
    int pos = *n;
    while (pos > 0 && xs[pos - 1] > v) {
        xs[pos] = xs[pos - 1];
        pos--;
    }
    xs[pos] = v;
    (*n)++;
    return 1;
}

#define MAX_FANOUT 32  /* hard cap; consumers beyond this are dropped on the floor */

int auto_route_net(wire_geometry_t *self, const char *wire_name,
                   vec2_t producer, const vec2_t *consumers, int n) {
    if (!self || !wire_name || !wire_name[0]) return -1;
    if (n > 0 && !consumers) return -1;

    /* Drop any existing geometry for this net; we replace it wholesale. */
    wire_geometry_remove_net(self, wire_name);

    if (n <= 0) {
        /* Net has no consumers — leave geometry absent. */
        return 0;
    }
    if (n == 1) {
        /* Single consumer: fall back to the Z-router. */
        return auto_route_wire(self, wire_name, producer, consumers[0]);
    }

    int net_idx = wire_geometry_get_or_create(self, wire_name);
    if (net_idx < 0) return -1;

    int eff_n = n < MAX_FANOUT ? n : MAX_FANOUT;

    /* Steiner Z topology: per consumer, decide where it joins the trunk
       and how the trunk-to-pin path looks.

         - cy == py:        consumer sits on the trunk; just extend the
                            trunk to cx. No drop, no stub.
         - cx == px:        consumer is directly below the producer;
                            single V drop, no H stub.
         - otherwise:       per-consumer Z arrival — V drop at a midpoint
                            column (snap-to-grid), then a final H stub
                            into the pin from the left or right. This is
                            what the single-consumer Z-router does;
                            multi-consumer mode shares the trunk while
                            preserving each consumer's H approach. */
    float anchor_x[MAX_FANOUT];
    int   needs_v [MAX_FANOUT];
    int   needs_h [MAX_FANOUT];

    for (int i = 0; i < eff_n; i++) {
        float cx = consumers[i].x, cy = consumers[i].y;
        if (cy == producer.y) {
            anchor_x[i] = cx;
            needs_v[i]  = 0;
            needs_h[i]  = 0;
        } else if (cx == producer.x) {
            anchor_x[i] = producer.x;
            needs_v[i]  = 1;
            needs_h[i]  = 0;
        } else {
            float mid = snap_to_grid(producer.x + (cx - producer.x) * 0.5f);
            /* Same nudge as the single-consumer Z-router — never let
               the H stub or trunk piece collapse to zero length. */
            float dir = (cx > producer.x) ? ROUTING_GRID : -ROUTING_GRID;
            if (mid == producer.x) mid += dir;
            if (mid == cx)         mid -= dir;
            anchor_x[i] = mid;
            needs_v[i]  = 1;
            needs_h[i]  = 1;
        }
    }

    /* Collect unique trunk-break xs: producer.x plus every anchor. */
    float xs[MAX_FANOUT + 1];
    int   n_xs = 0;
    insert_sorted_unique(xs, &n_xs, producer.x);
    for (int i = 0; i < eff_n; i++) {
        insert_sorted_unique(xs, &n_xs, anchor_x[i]);
    }

    /* Trunk H segments between consecutive xs at producer.y. */
    for (int i = 0; i + 1 < n_xs; i++) {
        wire_segment_t h;
        h.a.x = xs[i];     h.a.y = producer.y;
        h.b.x = xs[i + 1]; h.b.y = producer.y;
        wire_geometry_append_segments(self, net_idx, &h, 1);
    }

    /* Per-consumer V drop + H stub. */
    for (int i = 0; i < eff_n; i++) {
        if (needs_v[i]) {
            wire_segment_t v;
            v.a.x = anchor_x[i]; v.a.y = producer.y;
            v.b.x = anchor_x[i]; v.b.y = consumers[i].y;
            wire_geometry_append_segments(self, net_idx, &v, 1);
        }
        if (needs_h[i]) {
            wire_segment_t h;
            h.a.x = anchor_x[i];      h.a.y = consumers[i].y;
            h.b.x = consumers[i].x;    h.b.y = consumers[i].y;
            wire_geometry_append_segments(self, net_idx, &h, 1);
        }
    }
    return 0;
}
