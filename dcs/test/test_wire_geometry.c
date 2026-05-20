/* Offline unit tests for the app-layer wire_geometry sidecar (supplement Phase 1).
 *
 * Validates: init/release lifecycle, get_or_create idempotency, find/lookup,
 * set_segments H/V invariant + atomicity on rejection, capacity growth.
 * No raylib, no Win32 — pure data-structure exercise. */

#include "../src/domain/wire_geometry.h"
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

    /* ── wire_geometry_remove_net ─────────────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int a = wire_geometry_get_or_create(&g, "a");
        int b = wire_geometry_get_or_create(&g, "b");
        int c = wire_geometry_get_or_create(&g, "c");
        (void)a; (void)b; (void)c;
        wire_segment_t s[] = { seg(0, 0, 10, 0) };
        wire_geometry_set_segments(&g, a, s, 1);
        wire_geometry_set_segments(&g, b, s, 1);
        wire_geometry_set_segments(&g, c, s, 1);

        check("remove existing returns 1", wire_geometry_remove_net(&g, "b") == 1);
        check("net_count drops by 1 after remove", g.net_count == 2);
        check("removed net not findable", wire_geometry_find(&g, "b") == -1);
        check("other nets still findable (a)", wire_geometry_find(&g, "a") >= 0);
        check("other nets still findable (c)", wire_geometry_find(&g, "c") >= 0);

        check("remove unknown returns 0", wire_geometry_remove_net(&g, "zzz") == 0);
        check("remove NULL returns 0",    wire_geometry_remove_net(&g, NULL)  == 0);

        /* remove all and check empty */
        wire_geometry_remove_net(&g, "a");
        wire_geometry_remove_net(&g, "c");
        check("net_count == 0 after removing all", g.net_count == 0);

        wire_geometry_release(&g);
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

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 5 — junction derivation
       ────────────────────────────────────────────────────────────────── */

    /* ── wire_segments_collinear ─────────────────────────────────── */
    {
        wire_segment_t h1 = seg(0,   10, 100, 10);   /* H at y=10 */
        wire_segment_t h2 = seg(100, 10, 200, 10);   /* H at y=10 (same line) */
        wire_segment_t h3 = seg(0,   20, 100, 20);   /* H at y=20 (different line) */
        wire_segment_t v1 = seg(50,   0,  50, 100);  /* V at x=50 */
        wire_segment_t v2 = seg(50, 100,  50, 200);  /* V at x=50 (same line) */
        check("collinear: two H at same y",       wire_segments_collinear(&h1, &h2) == 1);
        check("collinear: two V at same x",       wire_segments_collinear(&v1, &v2) == 1);
        check("not collinear: H + V",             wire_segments_collinear(&h1, &v1) == 0);
        check("not collinear: H at different y",  wire_segments_collinear(&h1, &h3) == 0);
        check("collinear: NULL safe",             wire_segments_collinear(NULL, &h1) == 0);
    }

    /* ── junctions: empty net → 0 ─────────────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        wire_geometry_get_or_create(&g, "empty");
        vec2_t out[8];
        const wire_net_geom_t *n = wire_geometry_net(&g, 0);
        check("junctions empty net == 0",
              wire_geometry_junctions(n, out, 8) == 0);
        wire_geometry_release(&g);
    }

    /* ── single segment: 0 junctions (count = 1 at each endpoint) ─── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "one");
        wire_segment_t s[] = { seg(0, 0, 100, 0) };
        wire_geometry_set_segments(&g, idx, s, 1);
        vec2_t out[8];
        check("junctions 1-seg net == 0",
              wire_geometry_junctions(wire_geometry_net(&g, idx), out, 8) == 0);
        wire_geometry_release(&g);
    }

    /* ── polyline corner (Z-route, 3 segs): 0 junctions ─────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "z");
        /* H from (0,0)→(100,0), V from (100,0)→(100,50), H from (100,50)→(200,50). */
        wire_segment_t s[] = {
            seg(0,   0, 100,  0),
            seg(100, 0, 100, 50),
            seg(100, 50, 200, 50),
        };
        wire_geometry_set_segments(&g, idx, s, 3);
        vec2_t out[8];
        check("Z-route: 0 junctions (corners aren't joins)",
              wire_geometry_junctions(wire_geometry_net(&g, idx), out, 8) == 0);
        wire_geometry_release(&g);
    }

    /* ── two collinear H segments end-to-end: 0 junctions ─────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "hh");
        wire_segment_t s[] = {
            seg(  0, 10, 100, 10),
            seg(100, 10, 200, 10),
        };
        wire_geometry_set_segments(&g, idx, s, 2);
        vec2_t out[8];
        check("collinear end-to-end: 0 junctions",
              wire_geometry_junctions(wire_geometry_net(&g, idx), out, 8) == 0);
        wire_geometry_release(&g);
    }

    /* ── T-junction (3 segments meeting at a point): 1 junction ──── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "T");
        /* Two H segments butt together at (100, 10); a V drops from (100, 10). */
        wire_segment_t s[] = {
            seg(  0, 10, 100, 10),
            seg(100, 10, 200, 10),
            seg(100, 10, 100, 50),
        };
        wire_geometry_set_segments(&g, idx, s, 3);
        vec2_t out[8];
        int n = wire_geometry_junctions(wire_geometry_net(&g, idx), out, 8);
        check("T-junction: 1 junction", n == 1);
        check("T-junction: point at (100, 10)",
              n == 1 && out[0].x == 100 && out[0].y == 10);
        wire_geometry_release(&g);
    }

    /* ── + intersection (4 endpoints at center): 1 junction ──────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "plus");
        /* Two H butt at (100, 50) and two V butt at (100, 50). */
        wire_segment_t s[] = {
            seg(  0,  50, 100,  50),
            seg(100,  50, 200,  50),
            seg(100,   0, 100,  50),
            seg(100,  50, 100, 100),
        };
        wire_geometry_set_segments(&g, idx, s, 4);
        vec2_t out[8];
        int n = wire_geometry_junctions(wire_geometry_net(&g, idx), out, 8);
        check("+-intersection: 1 junction", n == 1);
        check("+-intersection: point at (100, 50)",
              n == 1 && out[0].x == 100 && out[0].y == 50);
        wire_geometry_release(&g);
    }

    /* ── fan-out via Z-router: producer drives two consumers ──────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        /* producer at (0, 100). Consumers at (160, 50) and (160, 150).
           Mid = 80, already on the 8 px grid, so corner_top = (80, 100). */
        vec2_t p;  p.x =   0; p.y = 100;
        vec2_t c1; c1.x = 160; c1.y =  50;
        vec2_t c2; c2.x = 160; c2.y = 150;
        auto_route_wire(&g, "fanout", p, c1);
        auto_route_wire(&g, "fanout", p, c2);
        int idx = wire_geometry_find(&g, "fanout");
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        vec2_t out[8];
        int j = wire_geometry_junctions(n, out, 8);
        check("fan-out Z-router: exactly 1 junction", j == 1);
        check("fan-out junction: at the shared branch corner (80, 100)",
              j == 1 && out[0].x == 80 && out[0].y == 100);
        wire_geometry_release(&g);
    }

    /* ── crossings between different nets are NEVER junctions ─────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int a = wire_geometry_get_or_create(&g, "a");
        int b = wire_geometry_get_or_create(&g, "b");
        wire_segment_t sa[] = { seg(0,  50, 200, 50) };     /* H through (100, 50) */
        wire_segment_t sb[] = { seg(100, 0, 100, 100) };    /* V through (100, 50) */
        wire_geometry_set_segments(&g, a, sa, 1);
        wire_geometry_set_segments(&g, b, sb, 1);
        vec2_t out[8];
        check("net A has 0 junctions",
              wire_geometry_junctions(wire_geometry_net(&g, a), out, 8) == 0);
        check("net B has 0 junctions",
              wire_geometry_junctions(wire_geometry_net(&g, b), out, 8) == 0);
        wire_geometry_release(&g);
    }

    /* ── truncation: more junctions than buffer can hold ──────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "many");
        /* Build three T-junctions at (100,0), (200,0), (300,0) on a shared
           horizontal trunk with three vertical drops. */
        wire_segment_t s[] = {
            seg(  0,  0, 100, 0),
            seg(100,  0, 200, 0),
            seg(200,  0, 300, 0),
            seg(300,  0, 400, 0),
            seg(100,  0, 100, 50),
            seg(200,  0, 200, 50),
            seg(300,  0, 300, 50),
        };
        wire_geometry_set_segments(&g, idx, s, 7);
        vec2_t small_out[2];
        int total = wire_geometry_junctions(wire_geometry_net(&g, idx), small_out, 2);
        check("truncation: returned total == 3", total == 3);
        check("truncation: caller can detect (total > max_out)", total > 2);
        wire_geometry_release(&g);
    }

    /* ── NULL / boundary safety ───────────────────────────────────── */
    {
        vec2_t out[4];
        check("junctions NULL net == 0",   wire_geometry_junctions(NULL, out, 4) == 0);
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "x");
        wire_segment_t s[] = { seg(0, 0, 10, 0) };
        wire_geometry_set_segments(&g, idx, s, 1);
        check("junctions NULL out == 0",
              wire_geometry_junctions(wire_geometry_net(&g, idx), NULL, 4) == 0);
        check("junctions max_out == 0",
              wire_geometry_junctions(wire_geometry_net(&g, idx), out, 0) == 0);
        wire_geometry_release(&g);
    }

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 6 — pick (click-to-highlight)
       ────────────────────────────────────────────────────────────────── */

    /* Build a fresh single-net scenario for the distance tests below. */
    #define MAKE_ONE_NET(g, idx, seg_array, seg_count_value)               \
        wire_geometry_init(&(g));                                          \
        int idx = wire_geometry_get_or_create(&(g), "n1");                 \
        wire_geometry_set_segments(&(g), idx, (seg_array), (seg_count_value))

    /* ── pick: exact-on-segment hits ──────────────────────────────── */
    {
        wire_geometry_t g;
        wire_segment_t s[] = { seg(100, 50, 300, 50) };   /* H at y=50, x=100..300 */
        MAKE_ONE_NET(g, idx, s, 1); (void)idx;
        const char *name = NULL;
        vec2_t p; p.x = 200; p.y = 50;
        check("pick on-segment returns 1",
              wire_geometry_pick(&g, p, 4.0f, &name) == 1);
        check("pick on-segment returns net name",
              name && strcmp(name, "n1") == 0);
        wire_geometry_release(&g);
    }

    /* ── pick: exact endpoint hits ────────────────────────────────── */
    {
        wire_geometry_t g;
        wire_segment_t s[] = { seg(100, 50, 300, 50) };
        MAKE_ONE_NET(g, idx, s, 1); (void)idx;
        const char *name = NULL;
        vec2_t p; p.x = 100; p.y = 50;
        check("pick at endpoint a hits",  wire_geometry_pick(&g, p, 4.0f, &name) == 1);
        p.x = 300;
        check("pick at endpoint b hits",  wire_geometry_pick(&g, p, 4.0f, &name) == 1);
        wire_geometry_release(&g);
    }

    /* ── pick: exactly `tol` away — hit (inclusive boundary) ──────── */
    {
        wire_geometry_t g;
        wire_segment_t s[] = { seg(100, 50, 300, 50) };
        MAKE_ONE_NET(g, idx, s, 1); (void)idx;
        const char *name = NULL;
        /* Move perpendicular to segment by exactly 4.0 (= tol). */
        vec2_t p; p.x = 200; p.y = 54;
        check("pick at exactly tol distance hits (inclusive)",
              wire_geometry_pick(&g, p, 4.0f, &name) == 1);
        wire_geometry_release(&g);
    }

    /* ── pick: just outside `tol` — miss ──────────────────────────── */
    {
        wire_geometry_t g;
        wire_segment_t s[] = { seg(100, 50, 300, 50) };
        MAKE_ONE_NET(g, idx, s, 1); (void)idx;
        const char *name = (const char *)0xdeadbeef;   /* sentinel for "not set" */
        vec2_t p; p.x = 200; p.y = 55;
        check("pick just outside tol misses",
              wire_geometry_pick(&g, p, 4.0f, &name) == 0);
        check("pick miss clears out-name",
              name == NULL);
        wire_geometry_release(&g);
    }

    /* ── pick: beyond segment extent (off the end) ────────────────── */
    {
        wire_geometry_t g;
        wire_segment_t s[] = { seg(100, 50, 300, 50) };
        MAKE_ONE_NET(g, idx, s, 1); (void)idx;
        const char *name = NULL;
        vec2_t p; p.x = 350; p.y = 50;   /* 50 px past endpoint b */
        check("pick well past endpoint misses",
              wire_geometry_pick(&g, p, 4.0f, &name) == 0);
        /* But within tol of the endpoint, it should hit. */
        p.x = 302;
        check("pick near endpoint b hits",
              wire_geometry_pick(&g, p, 4.0f, &name) == 1);
        wire_geometry_release(&g);
    }

    /* ── pick: multi-net — closest one wins ───────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int a = wire_geometry_get_or_create(&g, "near");
        int b = wire_geometry_get_or_create(&g, "far");
        wire_segment_t sa[] = { seg(0,  50, 200,  50) };
        wire_segment_t sb[] = { seg(0, 100, 200, 100) };
        wire_geometry_set_segments(&g, a, sa, 1);
        wire_geometry_set_segments(&g, b, sb, 1);
        const char *name = NULL;
        vec2_t p; p.x = 100; p.y = 52;     /* 2 px from "near", 48 px from "far" */
        check("pick multi-net returns closest",
              wire_geometry_pick(&g, p, 10.0f, &name) == 1);
        check("pick multi-net: 'near' won",
              name && strcmp(name, "near") == 0);
        wire_geometry_release(&g);
    }

    /* ── pick: empty geometry / no nets ───────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        const char *name = NULL;
        vec2_t p; p.x = 50; p.y = 50;
        check("pick empty geometry == 0", wire_geometry_pick(&g, p, 4.0f, &name) == 0);
        check("pick empty: name still NULL", name == NULL);
        wire_geometry_release(&g);
    }

    /* ── pick: NULL / negative-tol safety ─────────────────────────── */
    {
        vec2_t p; p.x = 0; p.y = 0;
        const char *name = NULL;
        check("pick NULL self == 0", wire_geometry_pick(NULL, p, 4.0f, &name) == 0);
        wire_geometry_t g; wire_geometry_init(&g);
        check("pick negative tol == 0",
              wire_geometry_pick(&g, p, -1.0f, &name) == 0);
        check("pick out-name NULL is fine",
              wire_geometry_pick(&g, p, 4.0f, NULL) == 0);
        wire_geometry_release(&g);
    }

    /* ── pick: V-segment, distance measured horizontally ──────────── */
    {
        wire_geometry_t g;
        wire_segment_t s[] = { seg(100, 50, 100, 200) };    /* V at x=100, y=50..200 */
        MAKE_ONE_NET(g, idx, s, 1); (void)idx;
        const char *name = NULL;
        vec2_t p; p.x = 103; p.y = 100;
        check("pick near V segment hits",
              wire_geometry_pick(&g, p, 4.0f, &name) == 1);
        p.x = 110;
        check("pick 10px from V segment misses (tol=4)",
              wire_geometry_pick(&g, p, 4.0f, &name) == 0);
        wire_geometry_release(&g);
    }

    #undef MAKE_ONE_NET

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 7 — append_segments + move (public API)
       ────────────────────────────────────────────────────────────────── */

    /* ── append: adds to existing list, validates H/V invariant ────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "a");
        wire_segment_t s1[] = { seg(0, 0, 10, 0) };
        check("set_segments seeds",
              wire_geometry_set_segments(&g, idx, s1, 1) == 0);
        wire_segment_t s2[] = { seg(10, 0, 10, 50) };
        check("append H+V seg",
              wire_geometry_append_segments(&g, idx, s2, 1) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("appended: seg_count == 2", n && n->seg_count == 2);

        wire_segment_t bad[] = { seg(0, 0, 10, 20) };   /* diagonal */
        check("append rejects diagonal",
              wire_geometry_append_segments(&g, idx, bad, 1) == -1);
        check("rejected: seg_count unchanged",
              n && n->seg_count == 2);

        check("append count==0 returns 0 (no-op)",
              wire_geometry_append_segments(&g, idx, NULL, 0) == 0);
        check("append negative count returns -1",
              wire_geometry_append_segments(&g, idx, s2, -1) == -1);
        check("append bad idx returns -1",
              wire_geometry_append_segments(&g, 99, s2, 1) == -1);
        wire_geometry_release(&g);
    }

    /* ── move: transfers ownership; src becomes empty ──────────────── */
    {
        wire_geometry_t src; wire_geometry_init(&src);
        wire_geometry_get_or_create(&src, "x");
        wire_geometry_get_or_create(&src, "y");
        wire_segment_t s[] = { seg(0, 0, 10, 0) };
        wire_geometry_set_segments(&src, 0, s, 1);
        wire_geometry_set_segments(&src, 1, s, 1);

        wire_geometry_t dst; wire_geometry_init(&dst);
        wire_geometry_get_or_create(&dst, "stale");   /* will be freed by move */

        wire_geometry_move(&dst, &src);
        check("move: dst now has 2 nets",         dst.net_count == 2);
        check("move: dst has 'x'",                wire_geometry_find(&dst, "x") >= 0);
        check("move: dst has 'y'",                wire_geometry_find(&dst, "y") >= 0);
        check("move: dst lost 'stale' (released)", wire_geometry_find(&dst, "stale") == -1);
        check("move: src is empty",                src.net_count == 0);
        check("move: src nets pointer NULL",       src.nets == NULL);

        /* Release on src after move is a safe no-op. */
        wire_geometry_release(&src);
        wire_geometry_release(&dst);
    }

    /* ── move: self-move is a no-op ────────────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        wire_geometry_get_or_create(&g, "x");
        wire_geometry_move(&g, &g);
        check("self-move: net retained", g.net_count == 1);
        wire_geometry_release(&g);
    }

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 12 — pick_segment + shift_segment
       ────────────────────────────────────────────────────────────────── */

    /* ── pick_segment returns matching net + segment indices ─────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int a = wire_geometry_get_or_create(&g, "a");
        int b = wire_geometry_get_or_create(&g, "b");
        wire_segment_t sa[] = {
            seg(  0,  50, 100,  50),
            seg(100,  50, 100, 150),
        };
        wire_segment_t sb[] = { seg(200, 50, 200, 150) };
        wire_geometry_set_segments(&g, a, sa, 2);
        wire_geometry_set_segments(&g, b, sb, 1);

        int net_idx = -1, seg_idx = -1;
        check("pick_segment: hit on net a seg 0",
              wire_geometry_pick_segment(&g, (vec2_t){50, 50}, 4.0f,
                                          &net_idx, &seg_idx) == 1
              && net_idx == a && seg_idx == 0);
        check("pick_segment: hit on net a seg 1 (vertical)",
              wire_geometry_pick_segment(&g, (vec2_t){100, 100}, 4.0f,
                                          &net_idx, &seg_idx) == 1
              && net_idx == a && seg_idx == 1);
        check("pick_segment: hit on net b seg 0",
              wire_geometry_pick_segment(&g, (vec2_t){200, 100}, 4.0f,
                                          &net_idx, &seg_idx) == 1
              && net_idx == b && seg_idx == 0);
        check("pick_segment: miss → out-params cleared to -1",
              wire_geometry_pick_segment(&g, (vec2_t){500, 500}, 4.0f,
                                          &net_idx, &seg_idx) == 0
              && net_idx == -1 && seg_idx == -1);
        check("pick_segment: NULL outs are safe",
              wire_geometry_pick_segment(&g, (vec2_t){50, 50}, 4.0f,
                                          NULL, NULL) == 1);
        wire_geometry_release(&g);
    }

    /* ── shift_segment: Z-route middle V — both H neighbours follow ── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "z");
        wire_segment_t s[] = {
            seg(  0,   0, 100,   0),    /* H — producer → corner_top */
            seg(100,   0, 100,  50),    /* V — middle */
            seg(100,  50, 200,  50),    /* H — corner_bot → consumer */
        };
        wire_geometry_set_segments(&g, idx, s, 3);

        check("shift V middle by +20 succeeds",
              wire_geometry_shift_segment(&g, idx, 1, +20.0f) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("shifted V seg now at x=120",
              n->segs[1].a.x == 120 && n->segs[1].b.x == 120);
        check("H seg 0 endpoint b followed to x=120",
              n->segs[0].b.x == 120);
        check("H seg 2 endpoint a followed to x=120",
              n->segs[2].a.x == 120);
        check("H seg 0 endpoint a stays at producer (x=0)",
              n->segs[0].a.x == 0);
        check("H seg 2 endpoint b stays at consumer (x=200)",
              n->segs[2].b.x == 200);
        check("all segments still H or V",
              n->segs[0].a.y == n->segs[0].b.y
              && n->segs[1].a.x == n->segs[1].b.x
              && n->segs[2].a.y == n->segs[2].b.y);
        wire_geometry_release(&g);
    }

    /* ── shift_segment: H middle — both V neighbours follow in y ─── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "z");
        wire_segment_t s[] = {
            seg(  0,   0,   0,  50),    /* V */
            seg(  0,  50, 100,  50),    /* H — middle */
            seg(100,  50, 100, 100),    /* V */
        };
        wire_geometry_set_segments(&g, idx, s, 3);

        check("shift H middle by -15 succeeds",
              wire_geometry_shift_segment(&g, idx, 1, -15.0f) == 0);
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("shifted H seg now at y=35",
              n->segs[1].a.y == 35 && n->segs[1].b.y == 35);
        check("V seg 0 endpoint b followed to y=35",  n->segs[0].b.y == 35);
        check("V seg 2 endpoint a followed to y=35",  n->segs[2].a.y == 35);
        wire_geometry_release(&g);
    }

    /* ── shift_segment: zero-delta is a no-op success ─────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "z");
        wire_segment_t s[] = {
            seg(  0,   0, 100,   0),
            seg(100,   0, 100,  50),
            seg(100,  50, 200,  50),
        };
        wire_geometry_set_segments(&g, idx, s, 3);
        check("shift delta=0 returns 0 (no-op)",
              wire_geometry_shift_segment(&g, idx, 1, 0.0f) == 0);
        wire_geometry_release(&g);
    }

    /* ── shift_segment rejects shift that creates a zero-length seg ─ */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "z");
        wire_segment_t s[] = {
            seg(  0,   0, 100,   0),
            seg(100,   0, 100,  50),
            seg(100,  50, 200,  50),
        };
        wire_geometry_set_segments(&g, idx, s, 3);

        /* Shifting middle V by -100 would collapse first H to zero-length
           (its b would equal its a). Should be rejected with full rollback. */
        check("shift over limit returns -1",
              wire_geometry_shift_segment(&g, idx, 1, -100.0f) == -1);
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("rollback: V seg back at x=100",
              n->segs[1].a.x == 100 && n->segs[1].b.x == 100);
        check("rollback: H seg 0 unchanged",
              n->segs[0].b.x == 100);
        check("rollback: H seg 2 unchanged",
              n->segs[2].a.x == 100);
        wire_geometry_release(&g);
    }

    /* ── shift_segment: invalid indices ───────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "z");
        wire_segment_t s[] = { seg(0, 0, 100, 0) };
        wire_geometry_set_segments(&g, idx, s, 1);
        check("shift bad net_idx returns -1",
              wire_geometry_shift_segment(&g, 99, 0, 10.0f) == -1);
        check("shift bad seg_idx returns -1",
              wire_geometry_shift_segment(&g,  0, 99, 10.0f) == -1);
        check("shift NULL self returns -1",
              wire_geometry_shift_segment(NULL, 0, 0, 10.0f) == -1);
        wire_geometry_release(&g);
    }

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 13 — Steiner-trunk router (auto_route_net)
       ────────────────────────────────────────────────────────────────── */

    /* ── n=0 removes the net ──────────────────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        wire_geometry_get_or_create(&g, "ghost");
        check("setup: net ghost exists", wire_geometry_find(&g, "ghost") >= 0);
        vec2_t prod; prod.x = 0; prod.y = 0;
        check("auto_route_net(n=0) returns 0",
              auto_route_net(&g, "ghost", prod, NULL, 0) == 0);
        check("net ghost removed", wire_geometry_find(&g, "ghost") == -1);
        wire_geometry_release(&g);
    }

    /* ── n=1 falls back to Z-router ───────────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 0;   prod.y = 0;
        vec2_t cs[1];
        cs[0].x = 160; cs[0].y = 40;
        check("auto_route_net(n=1) returns 0",
              auto_route_net(&g, "n", prod, cs, 1) == 0);
        const wire_net_geom_t *net = wire_geometry_net(&g, wire_geometry_find(&g, "n"));
        check("n=1: 3 segments (Z-route)",
              net && net->seg_count == 3);
        wire_geometry_release(&g);
    }

    /* ── n=2 fan-out: shared V bus topology (R-8) ───────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 0;   prod.y = 100;
        vec2_t cs[2];
        cs[0].x = 200; cs[0].y =  50;
        cs[1].x = 300; cs[1].y = 150;
        check("auto_route_net(n=2) returns 0",
              auto_route_net(&g, "fan2", prod, cs, 2) == 0);
        const wire_net_geom_t *net = wire_geometry_net(&g, wire_geometry_find(&g, "fan2"));
        /* V_bus_x = snap((0 + 200) / 2) = snap(100) = 104.
           Segments emitted in order:
             0: trunk H  (0,   100) → (104, 100)
             1: V bus  0 (104,  50) → (104, 100)
             2: V bus  1 (104, 100) → (104, 150)
             3: H stub 0 (104,  50) → (200,  50)
             4: H stub 1 (104, 150) → (300, 150)
           Total: 5 segments. */
        check("n=2: 5 segments (1 trunk + 2 V bus + 2 H stubs)",
              net && net->seg_count == 5);
        check("trunk H: (0, 100) → (104, 100)",
              net && net->segs[0].a.x == 0 && net->segs[0].a.y == 100
                  && net->segs[0].b.x == 104 && net->segs[0].b.y == 100);
        check("V bus seg 0: (104, 50) → (104, 100)",
              net && net->segs[1].a.x == 104 && net->segs[1].a.y == 50
                  && net->segs[1].b.x == 104 && net->segs[1].b.y == 100);
        check("V bus seg 1: (104, 100) → (104, 150)",
              net && net->segs[2].a.x == 104 && net->segs[2].a.y == 100
                  && net->segs[2].b.x == 104 && net->segs[2].b.y == 150);
        check("H stub 0: (104, 50) → (200, 50)",
              net && net->segs[3].a.x == 104 && net->segs[3].a.y == 50
                  && net->segs[3].b.x == 200 && net->segs[3].b.y == 50);
        check("H stub 1: (104, 150) → (300, 150)",
              net && net->segs[4].a.x == 104 && net->segs[4].a.y == 150
                  && net->segs[4].b.x == 300 && net->segs[4].b.y == 150);
        wire_geometry_release(&g);
    }

    /* ── junction at trunk-meets-V-bus when consumers straddle py ── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 0;   prod.y = 100;
        vec2_t cs[2];
        cs[0].x = 200; cs[0].y =  50;     /* above producer.y */
        cs[1].x = 300; cs[1].y = 150;     /* below producer.y */
        auto_route_net(&g, "j", prod, cs, 2);
        const wire_net_geom_t *net = wire_geometry_net(&g, wire_geometry_find(&g, "j"));
        vec2_t junctions[8];
        int nj = wire_geometry_junctions(net, junctions, 8);
        /* (104, 100) is degree-3: trunk.b + V bus seg 0.b + V bus seg 1.a.
           V bus is broken at producer.y (= 100) so the trunk meeting point
           is an interior break, not an endpoint. */
        check("V-bus straddle: 1 junction at trunk-meets-bus", nj == 1);
        check("junction at (104, 100)",
              nj == 1 && junctions[0].x == 104 && junctions[0].y == 100);
        wire_geometry_release(&g);
    }

    /* ── all consumers same y (below producer): junction is on bus ─ */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 0;   prod.y = 100;
        vec2_t cs[2];
        cs[0].x = 200; cs[0].y = 200;
        cs[1].x = 300; cs[1].y = 200;
        auto_route_net(&g, "j2", prod, cs, 2);
        const wire_net_geom_t *net = wire_geometry_net(&g, wire_geometry_find(&g, "j2"));
        vec2_t junctions[8];
        int nj = wire_geometry_junctions(net, junctions, 8);
        /* ys = {100, 200}. V bus is one segment ending at (104, 200).
           At (104, 200): V bus end + stub 0 a + stub 1 a = 3. JUNCTION. */
        check("all-below-py: 1 junction at bus terminus", nj == 1);
        check("junction at (104, 200)",
              nj == 1 && junctions[0].x == 104 && junctions[0].y == 200);
        wire_geometry_release(&g);
    }

    /* ── consumer on the trunk + off-trunk: 4 segs ───────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 0;   prod.y = 100;
        vec2_t cs[2];
        cs[0].x = 200; cs[0].y = 100;     /* collinear */
        cs[1].x = 300; cs[1].y = 150;     /* off-trunk */
        auto_route_net(&g, "mix", prod, cs, 2);
        const wire_net_geom_t *net = wire_geometry_net(&g, wire_geometry_find(&g, "mix"));
        /* V_bus_x = snap((0+200)/2) = 104.
           ys = {100, 150}. V bus segs: (104, 100)→(104, 150). 1 V.
           Trunk: (0,100)→(104,100). 1 trunk.
           Stubs: (104,100)→(200,100), (104,150)→(300,150). 2 stubs.
           Total: 1 + 1 + 2 = 4 segs. */
        check("mix collinear + off-trunk: 4 segs (1 trunk + 1 V + 2 H)",
              net && net->seg_count == 4);
        wire_geometry_release(&g);
    }

    /* ── all consumers collinear: H trunk only, no V bus ─────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 0;   prod.y = 100;
        vec2_t cs[2];
        cs[0].x = 200; cs[0].y = 100;
        cs[1].x = 300; cs[1].y = 100;
        auto_route_net(&g, "co", prod, cs, 2);
        const wire_net_geom_t *net = wire_geometry_net(&g, wire_geometry_find(&g, "co"));
        /* No off-trunk consumers → H trunk shortcut.
           xs = {0, 200, 300}. 2 H segs. */
        check("all-collinear: 2 trunk H segs (no V bus)",
              net && net->seg_count == 2);
        wire_geometry_release(&g);
    }

    /* ── producer interior to consumer xs ────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 200; prod.y = 100;
        vec2_t cs[2];
        cs[0].x =   0; cs[0].y = 200;     /* LEFT of producer */
        cs[1].x = 400; cs[1].y = 200;     /* RIGHT */
        auto_route_net(&g, "split", prod, cs, 2);
        const wire_net_geom_t *net = wire_geometry_net(&g, wire_geometry_find(&g, "split"));
        /* min_cx = 0. V_bus_x = snap((200+0)/2) = 104.
           Trunk: (200, 100) → (104, 100). H going LEFT. 1 seg.
           ys = {100, 200}. V bus: (104, 100)→(104, 200). 1 seg.
           Stubs: (104, 200)→(0, 200) leftward,
                  (104, 200)→(400, 200) rightward. 2 stubs.
           Total: 1 + 1 + 2 = 4 segs. */
        check("producer interior: 4 segs (1 trunk + 1 V + 2 H stubs)",
              net && net->seg_count == 4);
        wire_geometry_release(&g);
    }

    /* ── replaces existing geometry (not append) ──────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        int idx = wire_geometry_get_or_create(&g, "r");
        wire_segment_t stale[] = { seg(0, 0, 100, 0) };
        wire_geometry_set_segments(&g, idx, stale, 1);
        check("setup: 1 stale segment", wire_geometry_net(&g, idx)->seg_count == 1);

        vec2_t prod; prod.x = 0;   prod.y = 100;
        vec2_t cs[2];
        cs[0].x = 200; cs[0].y =  50;
        cs[1].x = 300; cs[1].y = 150;
        auto_route_net(&g, "r", prod, cs, 2);
        const wire_net_geom_t *net = wire_geometry_net(&g, wire_geometry_find(&g, "r"));
        check("auto_route_net replaces old geometry (not append)",
              net && net->seg_count == 5);
        wire_geometry_release(&g);
    }

    /* ── V-bus collision avoidance: second net shifts left ────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        /* Two nets with identical producer / leftmost-consumer geometry
           would naturally pick the same V_bus_x. The second net should
           shift leftward by 2*GRID (= 16 px) to keep a visible gap. */
        vec2_t prod; prod.x = 0;   prod.y = 100;
        vec2_t cs[2];
        cs[0].x = 200; cs[0].y =  50;
        cs[1].x = 300; cs[1].y = 150;
        auto_route_net(&g, "first",  prod, cs, 2);
        const wire_net_geom_t *first =
            wire_geometry_net(&g, wire_geometry_find(&g, "first"));
        /* First net: V_bus_x = snap((0+200)/2) = 104. */
        check("first net: V bus at x=104",
              first && first->segs[1].a.x == 104);

        /* Second net with the same input geometry; should collide and
           shift to 104 - 16 = 88. */
        auto_route_net(&g, "second", prod, cs, 2);
        const wire_net_geom_t *second =
            wire_geometry_net(&g, wire_geometry_find(&g, "second"));
        check("second net: V bus shifted to x=88 (collision avoidance)",
              second && second->segs[1].a.x == 88);

        /* Third net: collides with both 104 and 88 → shifts to 88-16=72. */
        auto_route_net(&g, "third",  prod, cs, 2);
        const wire_net_geom_t *third =
            wire_geometry_net(&g, wire_geometry_find(&g, "third"));
        check("third net: V bus shifted further to x=72",
              third && third->segs[1].a.x == 72);

        /* Non-overlapping y range — no collision even at same x. */
        vec2_t prod2; prod2.x = 0;   prod2.y = 500;
        vec2_t cs2[2];
        cs2[0].x = 200; cs2[0].y = 450;
        cs2[1].x = 300; cs2[1].y = 550;
        auto_route_net(&g, "elsewhere", prod2, cs2, 2);
        const wire_net_geom_t *elsewhere =
            wire_geometry_net(&g, wire_geometry_find(&g, "elsewhere"));
        check("non-overlapping y range: keeps natural V_bus_x = 104",
              elsewhere && elsewhere->segs[1].a.x == 104);

        wire_geometry_release(&g);
    }

    /* ── NULL / empty wire_name returns -1 ────────────────────────── */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        vec2_t prod; prod.x = 0; prod.y = 0;
        vec2_t cs[1];
        cs[0].x = 100; cs[0].y = 50;
        check("NULL name returns -1",
              auto_route_net(&g, NULL, prod, cs, 1) == -1);
        check("empty name returns -1",
              auto_route_net(&g, "", prod, cs, 1) == -1);
        wire_geometry_release(&g);
    }

    /* ── shift_segment: fan-out — algorithm conservatively refuses when
       the target shares an endpoint with multiple routes (corrupts the
       other route into a diagonal). The app layer should detect this
       and not initiate the drag. v1 limitation; the Steiner-trunk
       router (Phase 13) would change the topology and make this work. */
    {
        wire_geometry_t g; wire_geometry_init(&g);
        auto_route_wire(&g, "fan", (vec2_t){0, 100}, (vec2_t){160,  50});
        auto_route_wire(&g, "fan", (vec2_t){0, 100}, (vec2_t){160, 150});
        int idx = wire_geometry_find(&g, "fan");
        const wire_net_geom_t *n = wire_geometry_net(&g, idx);
        check("setup: fan-out has 6 segments (3+3)", n->seg_count == 6);

        /* seg 4 = second route's middle V. Shifting would corrupt the
           first route's middle V (which shares corner_top at (80,100)). */
        int rc = wire_geometry_shift_segment(&g, idx, 4, +24.0f);
        check("fan-out V shift refused (would corrupt sibling route)",
              rc == -1);
        check("fan-out rollback: both Vs still at x=80 (unchanged)",
              n->segs[1].a.x == 80 && n->segs[4].a.x == 80);
        wire_geometry_release(&g);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
