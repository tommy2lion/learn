/* Offline unit tests for the app-layer wire_geometry sidecar (supplement Phase 1).
 *
 * Validates: init/release lifecycle, get_or_create idempotency, find/lookup,
 * set_segments H/V invariant + atomicity on rejection, capacity growth.
 * No raylib, no Win32 — pure data-structure exercise. */

#include "../src/app/wire_geometry.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

static wire_segment_t seg(float ax, float ay, float bx, float by) {
    wire_segment_t s;
    s.a.x = ax; s.a.y = ay;
    s.b.x = bx; s.b.y = by;
    return s;
}

int main(void) {
    printf("=== wire_geometry offline tests ===\n");

    /* ── init / release on empty ──────────────────────────────────── */
    {
        wire_geometry_t g;
        wire_geometry_init(&g);
        check("init: net_count == 0", g.net_count == 0);
        check("init: net_cap   == 0", g.net_cap   == 0);
        check("init: nets == NULL",   g.nets == NULL);
        wire_geometry_release(&g);
        check("release leaves nets NULL",       g.nets == NULL);
        check("release leaves net_count == 0",  g.net_count == 0);
    }

    /* ── get_or_create idempotency + distinct names ───────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int a1 = wire_geometry_get_or_create(&g, "g1");
        int a2 = wire_geometry_get_or_create(&g, "g1");
        int b1 = wire_geometry_get_or_create(&g, "g2");
        check("get_or_create g1 >= 0",  a1 >= 0);
        check("get_or_create idempotent on same name", a1 == a2);
        check("get_or_create distinct names yield distinct indices", b1 != a1);
        check("net_count == 2 after two distinct adds", g.net_count == 2);
        wire_geometry_release(&g);
    }

    /* ── find: known / unknown / NULL ─────────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        wire_geometry_get_or_create(&g, "foo");
        check("find existing >= 0",    wire_geometry_find(&g, "foo") >= 0);
        check("find unknown == -1",    wire_geometry_find(&g, "bar") == -1);
        check("find NULL safe == -1",  wire_geometry_find(&g, NULL)  == -1);
        wire_geometry_release(&g);
    }

    /* ── get_or_create rejects invalid names ──────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        check("reject NULL  name",  wire_geometry_get_or_create(&g, NULL) == -1);
        check("reject empty name",  wire_geometry_get_or_create(&g, "")   == -1);
        char too_long[DOMAIN_NAME_LEN + 8];
        memset(too_long, 'x', sizeof(too_long) - 1);
        too_long[sizeof(too_long) - 1] = '\0';
        check("reject overlong name",
              wire_geometry_get_or_create(&g, too_long) == -1);
        check("no nets added on rejection", g.net_count == 0);
        wire_geometry_release(&g);
    }

    /* ── set_segments: pure-H, pure-V, mixed accepted ─────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "g1");
        wire_segment_t ok[] = {
            seg(  0,  0, 100,  0),   /* H */
            seg(100,  0, 100, 50),   /* V */
            seg(100, 50, 200, 50),   /* H */
        };
        check("set_segments accepts H/V mix",
              wire_geometry_set_segments(&g, idx, ok, 3) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("net seg_count == 3", n && n->seg_count == 3);
        check("first seg endpoints copied",
              n && n->segs[0].a.x ==   0 && n->segs[0].b.x == 100);
        check("second seg is vertical",
              n && n->segs[1].a.x == 100 && n->segs[1].b.x == 100);
        wire_geometry_release(&g);
    }

    /* ── set_segments rejects diagonal — list unchanged ───────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "g1");
        wire_segment_t pre[] = { seg(0, 0, 50, 0) };
        check("setup: seed with valid H segment",
              wire_geometry_set_segments(&g, idx, pre, 1) == 0);

        wire_segment_t bad[] = {
            seg(  0,  0, 100,  0),
            seg(100,  0, 200, 50),   /* diagonal */
        };
        check("set_segments rejects diagonal",
              wire_geometry_set_segments(&g, idx, bad, 2) == -1);
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("rejected: existing seg list preserved (count)",
              n && n->seg_count == 1);
        check("rejected: existing seg endpoints preserved",
              n && n->segs[0].a.x ==  0 && n->segs[0].b.x == 50);
        wire_geometry_release(&g);
    }

    /* ── set_segments rejects zero-length ─────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "g1");
        wire_segment_t bad[] = { seg(10, 10, 10, 10) };
        check("set_segments rejects zero-length",
              wire_geometry_set_segments(&g, idx, bad, 1) == -1);
        wire_geometry_release(&g);
    }

    /* ── set_segments rejects out-of-range index ──────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        wire_segment_t s[] = { seg(0, 0, 100, 0) };
        check("set_segments rejects negative idx",
              wire_geometry_set_segments(&g, -1, s, 1) == -1);
        check("set_segments rejects unknown idx",
              wire_geometry_set_segments(&g,  0, s, 1) == -1);
        wire_geometry_release(&g);
    }

    /* ── set_segments: count == 0 clears the list ─────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "g1");
        wire_segment_t s[] = { seg(0, 0, 100, 0) };
        wire_geometry_set_segments(&g, idx, s, 1);
        check("set_segments(NULL, 0) returns 0",
              wire_geometry_set_segments(&g, idx, NULL, 0) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("cleared: seg_count == 0", n && n->seg_count == 0);
        check("cleared: segs pointer NULL", n && n->segs == NULL);
        wire_geometry_release(&g);
    }

    /* ── wire_geometry_net: out-of-range returns NULL ─────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        check("net(-1) == NULL", wire_geometry_net(&g, -1) == NULL);
        check("net(0) on empty == NULL", wire_geometry_net(&g, 0) == NULL);
        wire_geometry_get_or_create(&g, "g1");
        check("net(0) on 1-net != NULL", wire_geometry_net(&g, 0) != NULL);
        check("net(1) on 1-net == NULL", wire_geometry_net(&g, 1) == NULL);
        wire_geometry_release(&g);
    }

    /* ── capacity growth: 64 distinct nets, each with one segment ── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        char nm[16];
        int add_ok = 1;
        for (int i = 0; i < 64; i++) {
            snprintf(nm, sizeof(nm), "n%d", i);
            int idx = wire_geometry_get_or_create(&g, nm);
            if (idx < 0) { add_ok = 0; break; }
            wire_segment_t s[] = { seg((float)(i * 10), 0.0f, (float)(i * 10), 50.0f) };
            if (wire_geometry_set_segments(&g, idx, s, 1) != 0) { add_ok = 0; break; }
        }
        check("add 64 nets succeeds",  add_ok);
        check("net_count == 64",        g.net_count == 64);
        check("net_cap   >= 64",        g.net_cap   >= 64);

        int find_all_ok = 1;
        for (int i = 0; i < 64; i++) {
            snprintf(nm, sizeof(nm), "n%d", i);
            int idx = wire_geometry_find(&g, nm);
            const wire_net_geom_t *n = wire_geometry_net(&g, idx);
            if (idx < 0 || !n || n->seg_count != 1) { find_all_ok = 0; break; }
        }
        check("every net found, holds its segment", find_all_ok);
        wire_geometry_release(&g);
    }

    /* ── double release is safe (release on a released-empty struct) ── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        for (int i = 0; i < 8; i++) {
            char nm[8];
            snprintf(nm, sizeof(nm), "x%d", i);
            int idx = wire_geometry_get_or_create(&g, nm);
            wire_segment_t s[] = { seg(0.0f, (float)(i * 10), 50.0f, (float)(i * 10)) };
            wire_geometry_set_segments(&g, idx, s, 1);
        }
        wire_geometry_release(&g);
        wire_geometry_release(&g);     /* second release: must not crash / double-free */
        check("double release safe",
              g.nets == NULL && g.net_count == 0 && g.net_cap == 0);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
