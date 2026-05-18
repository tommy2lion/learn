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
