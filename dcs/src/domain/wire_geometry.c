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

int wire_geometry_shift_v_bus(wire_geometry_t *self, int net_idx,
                              float column_x, float delta) {
    if (!self || net_idx < 0 || net_idx >= self->net_count) return -1;
    if (delta == 0.0f) return 0;

    wire_net_geom_t *n = &self->nets[net_idx];

    /* Verify at least one V segment exists at column_x. */
    int v_count = 0;
    for (int i = 0; i < n->seg_count; i++) {
        const wire_segment_t *s = &n->segs[i];
        if (s->a.x == s->b.x && s->a.x == column_x) v_count++;
    }
    if (v_count == 0) return -1;

    /* Snapshot + mark V bus segments so we don't double-update them. */
    wire_segment_t *backup =
        (wire_segment_t *)malloc(sizeof(*backup) * (size_t)n->seg_count);
    if (!backup) return -1;
    memcpy(backup, n->segs, sizeof(*backup) * (size_t)n->seg_count);

    int *is_vbus = (int *)malloc(sizeof(int) * (size_t)n->seg_count);
    if (!is_vbus) { free(backup); return -1; }
    for (int i = 0; i < n->seg_count; i++) {
        const wire_segment_t *s = &n->segs[i];
        is_vbus[i] = (s->a.x == s->b.x && s->a.x == column_x);
    }

    /* Apply: V bus segs shift both endpoints by delta in x; other segments
       update only endpoints that land exactly at column_x. */
    for (int i = 0; i < n->seg_count; i++) {
        wire_segment_t *s = &n->segs[i];
        if (is_vbus[i]) {
            s->a.x += delta;
            s->b.x += delta;
        } else {
            if (s->a.x == column_x) s->a.x += delta;
            if (s->b.x == column_x) s->b.x += delta;
        }
    }

    int ok = 1;
    for (int i = 0; i < n->seg_count; i++) {
        if (!segment_is_valid(&n->segs[i])) { ok = 0; break; }
    }
    if (!ok) {
        memcpy(n->segs, backup, sizeof(*backup) * (size_t)n->seg_count);
    }
    free(is_vbus);
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

/* True if the vertical column at x (covering y-range [y_lo, y_hi]) lands
   STRICTLY inside any obstacle whose own y-range overlaps it. Endpoints
   on obstacle edges are NOT considered hits — pins typically sit on
   gate edges. (U-40, Stage 4) */
static int v_column_hits_obstacle(float x, float y_lo, float y_hi,
                                  const route_context_t *ctx) {
    if (!ctx || ctx->n_obstacles == 0) return 0;
    if (y_hi < y_lo) { float t = y_lo; y_lo = y_hi; y_hi = t; }
    for (int i = 0; i < ctx->n_obstacles; i++) {
        const rect_t *o = &ctx->obstacles[i];
        if (x <= o->x || x >= o->x + o->w) continue;        /* outside column */
        if (y_hi <= o->y || y_lo >= o->y + o->h) continue;  /* y ranges disjoint */
        return 1;
    }
    return 0;
}

/* True if the horizontal segment from (x1, y) to (x2, y) crosses
   STRICTLY into any obstacle. Symmetric to v_column_hits_obstacle.
   (Stage 4B.4) */
static int h_seg_hits_obstacle(float x1, float x2, float y,
                               const route_context_t *ctx) {
    if (!ctx || ctx->n_obstacles == 0) return 0;
    if (x2 < x1) { float t = x1; x1 = x2; x2 = t; }
    for (int i = 0; i < ctx->n_obstacles; i++) {
        const rect_t *o = &ctx->obstacles[i];
        if (y <= o->y || y >= o->y + o->h) continue;          /* y outside */
        if (x2 <= o->x || x1 >= o->x + o->w) continue;        /* x disjoint */
        return 1;
    }
    return 0;
}

/* Sentinel "no channel found". Float NAN would also work but a fixed
   out-of-bounds value avoids math.h and silences any FP-comparison
   compilers might warn on. */
#define WG_NO_CHANNEL_Y  (-1.0e30f)

/* Pick the H-channel whose centre y is closest to `prefer_y` and whose
   x-range fully covers [x_lo, x_hi]. Returns WG_NO_CHANNEL_Y if none
   fits. (Stage 4B.4) */
static float find_h_channel_for_stub(float x_lo, float x_hi, float prefer_y,
                                     const route_context_t *ctx) {
    if (!ctx || ctx->n_h_channels == 0) return WG_NO_CHANNEL_Y;
    if (x_hi < x_lo) { float t = x_lo; x_lo = x_hi; x_hi = t; }
    float best_y    = WG_NO_CHANNEL_Y;
    float best_dist = 1.0e30f;
    for (int i = 0; i < ctx->n_h_channels; i++) {
        const rect_t *ch = &ctx->h_channels[i];
        /* The channel must straddle the stub's x-range entirely so the
           detour's H segment fits. Channels span the full canvas width
           today (see compute_layout_channels), so this is usually true. */
        if (ch->x > x_lo || ch->x + ch->w < x_hi) continue;
        float ch_y = ch->y + ch->h * 0.5f;
        float d    = ch_y > prefer_y ? ch_y - prefer_y : prefer_y - ch_y;
        if (d < best_dist) { best_dist = d; best_y = ch_y; }
    }
    return best_y;
}

/* Snap V_bus_x to the nearest V-channel whose centre falls in the
   [producer-side, consumer-side] range. Returns the unchanged input
   when no channel fits — keeps the obstacle-shift fallback intact.
   (Stage 4B.4) */
static float snap_v_bus_to_channel(float naive_x, float prod_x, float min_cx,
                                   const route_context_t *ctx) {
    if (!ctx || ctx->n_v_channels == 0) return naive_x;
    float lo = prod_x < min_cx ? prod_x : min_cx;
    float hi = prod_x > min_cx ? prod_x : min_cx;
    float best_x    = naive_x;
    float best_dist = 1.0e30f;
    int   found     = 0;
    for (int i = 0; i < ctx->n_v_channels; i++) {
        const rect_t *ch = &ctx->v_channels[i];
        float ch_x = ch->x + ch->w * 0.5f;
        if (ch_x <= lo || ch_x >= hi) continue;       /* outside producer-consumer span */
        float d = ch_x > naive_x ? ch_x - naive_x : naive_x - ch_x;
        if (d < best_dist) { best_dist = d; best_x = ch_x; found = 1; }
    }
    return found ? best_x : naive_x;
}

/* Find a V-column x that doesn't hit any obstacle, starting from the
   naive midpoint and walking outward in grid-step increments. Caps the
   search to avoid runaway; on cap reached, returns the original mid_x
   (router silently emits a known-bad route — better than no wire). */
static float shift_mid_x_away_from_obstacles(
    float naive_mid_x, float prod_x, float cons_x,
    float y_lo, float y_hi, const route_context_t *ctx) {
    if (!v_column_hits_obstacle(naive_mid_x, y_lo, y_hi, ctx)) return naive_mid_x;
    /* Try walking left toward producer, then right toward consumer.
       16 attempts ≈ ±128 px which covers any reasonable gate width. */
    for (int step = 1; step <= 16; step++) {
        float dx = step * ROUTING_GRID;
        float lx = naive_mid_x - dx;
        if (lx != prod_x && lx != cons_x
            && !v_column_hits_obstacle(lx, y_lo, y_hi, ctx))
            return lx;
        float rx = naive_mid_x + dx;
        if (rx != prod_x && rx != cons_x
            && !v_column_hits_obstacle(rx, y_lo, y_hi, ctx))
            return rx;
    }
    return naive_mid_x;
}

int auto_route_wire(wire_geometry_t *self, const char *wire_name,
                    vec2_t producer_pin, vec2_t consumer_pin) {
    return auto_route_wire_ctx(self, wire_name, producer_pin, consumer_pin, NULL);
}

int auto_route_wire_ctx(wire_geometry_t *self, const char *wire_name,
                        vec2_t producer_pin, vec2_t consumer_pin,
                        const route_context_t *ctx) {
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

        /* U-40: avoid component bodies if a route context is supplied. */
        mid_x = shift_mid_x_away_from_obstacles(
            mid_x, producer_pin.x, consumer_pin.x,
            producer_pin.y, consumer_pin.y, ctx);

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
    return auto_route_net_ctx(self, wire_name, producer, consumers, n, NULL);
}

int auto_route_net_ctx(wire_geometry_t *self, const char *wire_name,
                       vec2_t producer, const vec2_t *consumers, int n,
                       const route_context_t *ctx) {
    if (!self || !wire_name || !wire_name[0]) return -1;
    if (n > 0 && !consumers) return -1;

    /* Drop any existing geometry for this net; we replace it wholesale. */
    wire_geometry_remove_net(self, wire_name);

    if (n <= 0) {
        /* Net has no consumers — leave geometry absent. */
        return 0;
    }
    if (n == 1) {
        /* Single consumer: fall back to the Z-router (with the same ctx
           so obstacle avoidance applies). */
        return auto_route_wire_ctx(self, wire_name, producer, consumers[0], ctx);
    }

    int net_idx = wire_geometry_get_or_create(self, wire_name);
    if (net_idx < 0) return -1;

    int eff_n = n < MAX_FANOUT ? n : MAX_FANOUT;

    /* Shared V-bus topology (R-8). Three pieces:
       - H trunk from producer.x to V_bus_x at producer.y.
       - One vertical bus at V_bus_x, broken at every unique y
         (producer.y + every consumer.y).
       - One H stub per consumer from (V_bus_x, c.y) to (c.x, c.y).

       When every consumer is collinear with the producer (cy == py for
       all), the V bus would degenerate to zero length, so we shortcut
       to a single H trunk broken at each consumer.x — visually the
       same and one segment cheaper. */
    int any_off_trunk = 0;
    for (int i = 0; i < eff_n; i++) {
        if (consumers[i].y != producer.y) { any_off_trunk = 1; break; }
    }

    if (!any_off_trunk) {
        /* All-collinear: H trunk only, broken at each consumer.x. */
        float xs[MAX_FANOUT + 1];
        int n_xs = 0;
        insert_sorted_unique(xs, &n_xs, producer.x);
        for (int i = 0; i < eff_n; i++) {
            insert_sorted_unique(xs, &n_xs, consumers[i].x);
        }
        for (int i = 0; i + 1 < n_xs; i++) {
            wire_segment_t h;
            h.a.x = xs[i];     h.a.y = producer.y;
            h.b.x = xs[i + 1]; h.b.y = producer.y;
            wire_geometry_append_segments(self, net_idx, &h, 1);
        }
        return 0;
    }

    /* Shared V bus: pick V_bus_x as snap-midpoint(producer.x, leftmost cx).
       For standard left-to-right flow this places the bus between the
       producer and all consumers. For interior-producer cases (some
       consumers to the left) the H stubs simply run in both directions
       from V_bus_x — still topologically valid, may not be the prettiest
       routing but doesn't corrupt anything. */
    float min_cx = consumers[0].x;
    for (int i = 1; i < eff_n; i++) {
        if (consumers[i].x < min_cx) min_cx = consumers[i].x;
    }
    float V_bus_x = snap_to_grid(producer.x + (min_cx - producer.x) * 0.5f);
    float dir = (min_cx > producer.x) ? ROUTING_GRID : -ROUTING_GRID;
    if (V_bus_x == producer.x) V_bus_x += dir;
    if (V_bus_x == min_cx)     V_bus_x -= dir;

    /* Collision avoidance: shift V_bus_x leftward by 2*GRID whenever it
       lands within 2*GRID of another net's existing V segment in an
       overlapping y range. Gives the eye a visible gap between adjacent
       buses. Caps the shifts so we don't crawl past the producer's
       x; on overflow, accept the collision (user can drag via Phase 12).
       (R-8 collision avoidance — user feedback.) */
    const float MIN_VBUS_SPACING = 2.0f * ROUTING_GRID;
    float net_y_lo = producer.y, net_y_hi = producer.y;
    for (int i = 0; i < eff_n; i++) {
        if (consumers[i].y < net_y_lo) net_y_lo = consumers[i].y;
        if (consumers[i].y > net_y_hi) net_y_hi = consumers[i].y;
    }
    for (int attempt = 0; attempt < 16; attempt++) {
        int collide = 0;
        for (int ni = 0; ni < self->net_count && !collide; ni++) {
            if (ni == net_idx) continue;     /* skip the net we're routing */
            const wire_net_geom_t *other = &self->nets[ni];
            for (int si = 0; si < other->seg_count; si++) {
                const wire_segment_t *s = &other->segs[si];
                if (s->a.x != s->b.x) continue;          /* not a V segment */
                float dx = s->a.x - V_bus_x;
                if (dx < 0) dx = -dx;
                if (dx >= MIN_VBUS_SPACING) continue;    /* far enough */
                float o_lo = s->a.y < s->b.y ? s->a.y : s->b.y;
                float o_hi = s->a.y > s->b.y ? s->a.y : s->b.y;
                if (o_hi < net_y_lo || o_lo > net_y_hi) continue;  /* no y overlap */
                collide = 1;
                break;
            }
        }
        if (!collide) break;
        /* Shift leftward (toward producer side); stop if we'd cross the
           producer's x or go too far away. */
        float next = V_bus_x - MIN_VBUS_SPACING;
        if ((dir > 0 && next <= producer.x) ||
            (dir < 0 && next >= producer.x)) break;
        V_bus_x = next;
    }

    /* U-40 obstacle avoidance: same shift pattern, but checking against
       component bbox columns instead of other nets' V segments. Runs
       AFTER the R-8 net-vs-net shift so the two layers compose. */
    for (int attempt = 0; attempt < 16; attempt++) {
        if (!v_column_hits_obstacle(V_bus_x, net_y_lo, net_y_hi, ctx)) break;
        float next = V_bus_x - MIN_VBUS_SPACING;
        if ((dir > 0 && next <= producer.x) ||
            (dir < 0 && next >= producer.x)) break;
        V_bus_x = next;
    }

    /* U-41 Stage 4B.4: snap V_bus_x to the nearest V-channel between
       producer and the leftmost consumer when one is available. Channels
       are obstacle-free by construction, so a channel-snap won't
       reintroduce a crossing. Falls back to the U-40-shifted x when no
       channel fits the range. */
    V_bus_x = snap_v_bus_to_channel(V_bus_x, producer.x, min_cx, ctx);

    /* H trunk from producer to V_bus_x (skipped if zero-length). */
    if (V_bus_x != producer.x) {
        wire_segment_t h;
        h.a.x = producer.x; h.a.y = producer.y;
        h.b.x = V_bus_x;    h.b.y = producer.y;
        wire_geometry_append_segments(self, net_idx, &h, 1);
    }

    /* V bus broken at every unique y: producer.y + each consumer's y. */
    float ys[MAX_FANOUT + 1];
    int n_ys = 0;
    insert_sorted_unique(ys, &n_ys, producer.y);
    for (int i = 0; i < eff_n; i++) {
        insert_sorted_unique(ys, &n_ys, consumers[i].y);
    }
    for (int i = 0; i + 1 < n_ys; i++) {
        wire_segment_t v;
        v.a.x = V_bus_x; v.a.y = ys[i];
        v.b.x = V_bus_x; v.b.y = ys[i + 1];
        wire_geometry_append_segments(self, net_idx, &v, 1);
    }

    /* One stub per consumer from (V_bus_x, c.y) to the pin.
       - Naive case: a single H segment at c.y.
       - When that H would cross a component body (U-40 caught V columns
         but not H rows): detour through the nearest H channel via a
         5-vertex path V -> H -> V. The channels are guaranteed
         obstacle-free by construction, so the detour's H segment is
         always clean.
       - If no channel fits, fall back to the naive H (will visibly
         cross — at this point the layout itself is too cramped for
         channelled routing to help; the user can drag manually).
       Skip zero-length stubs (consumer sits on the V bus).
       (Stage 4B.4, U-41) */
    for (int i = 0; i < eff_n; i++) {
        float cx = consumers[i].x;
        float cy = consumers[i].y;
        if (cx == V_bus_x) continue;

        if (h_seg_hits_obstacle(V_bus_x, cx, cy, ctx)) {
            float lo = V_bus_x < cx ? V_bus_x : cx;
            float hi = V_bus_x > cx ? V_bus_x : cx;
            float ch_y = find_h_channel_for_stub(lo, hi, cy, ctx);
            if (ch_y != WG_NO_CHANNEL_Y) {
                wire_segment_t segs[3];
                /* V from V_bus to channel y */
                segs[0].a.x = V_bus_x; segs[0].a.y = cy;
                segs[0].b.x = V_bus_x; segs[0].b.y = ch_y;
                /* H through channel */
                segs[1].a.x = V_bus_x; segs[1].a.y = ch_y;
                segs[1].b.x = cx;      segs[1].b.y = ch_y;
                /* V from channel back to consumer pin y */
                segs[2].a.x = cx;      segs[2].a.y = ch_y;
                segs[2].b.x = cx;      segs[2].b.y = cy;
                wire_geometry_append_segments(self, net_idx, segs, 3);
                continue;
            }
            /* No channel fits the stub's x-range — fall through to
               naive H (will cross; better than no wire). */
        }

        wire_segment_t h;
        h.a.x = V_bus_x; h.a.y = cy;
        h.b.x = cx;      h.b.y = cy;
        wire_geometry_append_segments(self, net_idx, &h, 1);
    }
    return 0;
}
