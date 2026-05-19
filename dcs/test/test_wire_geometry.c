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

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 2 — Z-router
       ────────────────────────────────────────────────────────────────── */

    /* ── auto_route_wire: same row → one horizontal segment ───────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t p; p.x = 100; p.y = 100;
        vec2_t c; c.x = 300; c.y = 100;
        check("route H-only returns 0",
              auto_route_wire(&g, "h1", p, c) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, wire_geometry_find(&g, "h1"));
        check("H-only: 1 segment", n && n->seg_count == 1);
        check("H-only: endpoints copied",
              n && n->segs[0].a.x == 100 && n->segs[0].a.y == 100
                && n->segs[0].b.x == 300 && n->segs[0].b.y == 100);
        wire_geometry_release(&g);
    }

    /* ── auto_route_wire: same column → one vertical segment ──────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t p; p.x = 100; p.y = 100;
        vec2_t c; c.x = 100; c.y = 300;
        check("route V-only returns 0",
              auto_route_wire(&g, "v1", p, c) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, wire_geometry_find(&g, "v1"));
        check("V-only: 1 segment", n && n->seg_count == 1);
        check("V-only: endpoints copied",
              n && n->segs[0].a.x == 100 && n->segs[0].a.y == 100
                && n->segs[0].b.x == 100 && n->segs[0].b.y == 300);
        wire_geometry_release(&g);
    }

    /* ── auto_route_wire: Z-shape with on-grid midpoint ───────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t p; p.x = 100; p.y = 100;
        vec2_t c; c.x = 300; c.y = 200;
        check("route Z returns 0", auto_route_wire(&g, "z1", p, c) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, wire_geometry_find(&g, "z1"));
        check("Z: 3 segments", n && n->seg_count == 3);
        /* mid_x = (100 + 300) / 2 = 200, already grid-aligned */
        check("Z seg0: producer → (200, prod.y) horizontal",
              n && n->segs[0].a.x == 100 && n->segs[0].a.y == 100
                && n->segs[0].b.x == 200 && n->segs[0].b.y == 100);
        check("Z seg1: vertical at mid_x=200",
              n && n->segs[1].a.x == 200 && n->segs[1].b.x == 200
                && n->segs[1].a.y == 100 && n->segs[1].b.y == 200);
        check("Z seg2: (200, cons.y) → consumer horizontal",
              n && n->segs[2].a.x == 200 && n->segs[2].a.y == 200
                && n->segs[2].b.x == 300 && n->segs[2].b.y == 200);
        wire_geometry_release(&g);
    }

    /* ── auto_route_wire: mid_x snaps to grid (8 px step) ─────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        /* raw mid_x = (0 + 20) / 2 = 10 → snap to 8 */
        vec2_t p; p.x = 0;  p.y = 0;
        vec2_t c; c.x = 20; c.y = 40;
        check("route Z snap returns 0",
              auto_route_wire(&g, "z2", p, c) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, wire_geometry_find(&g, "z2"));
        check("Z snap: 3 segments", n && n->seg_count == 3);
        check("Z snap: mid_x == 8 (grid-aligned)",
              n && n->segs[1].a.x == 8.0f && n->segs[1].b.x == 8.0f);
        wire_geometry_release(&g);
    }

    /* ── auto_route_wire: snap collapse → nudge keeps all 3 segs non-zero ── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        /* producer.x = 0, consumer.x = 4 → raw mid = 2 → snap to 0 (== producer);
           nudge +grid → mid_x = 8. Third segment is (8, cons.y) → (4, cons.y). */
        vec2_t p; p.x = 0; p.y = 0;
        vec2_t c; c.x = 4; c.y = 10;
        check("route close-pins returns 0",
              auto_route_wire(&g, "z3", p, c) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, wire_geometry_find(&g, "z3"));
        check("close-pins: 3 segments still", n && n->seg_count == 3);
        check("close-pins: mid_x nudged off producer.x",
              n && n->segs[1].a.x != p.x);
        wire_geometry_release(&g);
    }

    /* ── auto_route_wire: fan-out — two consumers append to same net ── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 0; prod.y = 0;
        vec2_t c1;   c1.x   = 200; c1.y   = 100;
        vec2_t c2;   c2.x   = 200; c2.y   = 200;
        check("fan-out consumer 1", auto_route_wire(&g, "fo", prod, c1) == 0);
        check("fan-out consumer 2", auto_route_wire(&g, "fo", prod, c2) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, wire_geometry_find(&g, "fo"));
        /* Both consumers form Z-shapes (different y from producer) → 3 + 3 = 6 segments */
        check("fan-out: 6 total segments accumulated", n && n->seg_count == 6);
        wire_geometry_release(&g);
    }

    /* ── auto_route_wire: degenerate (producer == consumer) → no segs ── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t p; p.x = 100; p.y = 100;
        check("route same-point returns 0",
              auto_route_wire(&g, "p1", p, p) == 0);
        int idx = wire_geometry_find(&g, "p1");
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("same-point: net created but 0 segments",
              idx >= 0 && n && n->seg_count == 0);
        wire_geometry_release(&g);
    }

    /* ── auto_route_wire: invalid wire_name rejected ──────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t p; p.x = 0; p.y = 0;
        vec2_t c; c.x = 100; c.y = 100;
        check("route NULL  name == -1",
              auto_route_wire(&g, NULL, p, c) == -1);
        check("route empty name == -1",
              auto_route_wire(&g, "",   p, c) == -1);
        check("no nets created on invalid name", g.net_count == 0);
        wire_geometry_release(&g);
    }

    /* ── auto_route_wire: every emitted segment is purely H or V ──── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        struct { vec2_t a, b; const char *nm; } cases[] = {
            { { 0,   0}, {123,  47}, "a" },
            { {55,  33}, {200,  99}, "b" },
            { {10,  10}, { 10, 100}, "c" },     /* vertical-only */
            { { 7,  92}, {310,  92}, "d" },     /* horizontal-only */
            { {-30, -10}, {77, -42}, "e" },     /* negative coords */
        };
        int n_cases = (int)(sizeof(cases) / sizeof(cases[0]));
        for (int i = 0; i < n_cases; i++) {
            auto_route_wire(&g, cases[i].nm, cases[i].a, cases[i].b);
        }
        int all_hv = 1;
        int total_segs = 0;
        for (int i = 0; i < g.net_count; i++) {
            const wire_net_geom_t *n = &g.nets[i];
            total_segs += n->seg_count;
            for (int j = 0; j < n->seg_count; j++) {
                int hv = (n->segs[j].a.x == n->segs[j].b.x)
                      || (n->segs[j].a.y == n->segs[j].b.y);
                if (!hv) { all_hv = 0; break; }
            }
        }
        check("router emits only H or V segments",         all_hv);
        check("router actually emitted segments",          total_segs > 0);
        wire_geometry_release(&g);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
