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

/* Append (not replace) validated segments to net_idx. Symmetric to
   set_segments — same validation, same atomicity on failure. */
static int append_segments(wire_geometry_t *self, int net_idx,
                           const wire_segment_t *segs, int count) {
    if (net_idx < 0 || net_idx >= self->net_count) return -1;
    if (count <= 0 || !segs)                       return -1;

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

    return append_segments(self, idx, segs, count);
}
