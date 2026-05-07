/* Offline structural test for igraph.
 *
 * We deliberately do NOT call init()/begin_frame()/draw_*() because that
 * would open a window and require a display. Instead we verify that the
 * vtable is fully populated, color encoding round-trips, and the rect/vec2
 * types have the expected layout. End-to-end window tests will come with
 * Phase 2.3's framework_demo. */

#include "../src/framework/graphics/igraph.h"
#include "../src/framework/core/color.h"
#include "../src/framework/core/rect.h"
#include <stdio.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

int main(void) {
    printf("=== igraph offline tests ===\n");

    igraph_t *g = graph_create();
    check("graph_create non-NULL", g != NULL);
    if (!g) { printf("\n0 / 1 passed\n"); return 1; }

    /* ── vtable populated (every pointer must be non-NULL) ── */
    check("vt: init",              g->init              != NULL);
    check("vt: shutdown",          g->shutdown          != NULL);
    check("vt: should_close",      g->should_close      != NULL);
    check("vt: begin_frame",       g->begin_frame       != NULL);
    check("vt: end_frame",         g->end_frame         != NULL);
    check("vt: screen_size",       g->screen_size       != NULL);
    check("vt: draw_rect",         g->draw_rect         != NULL);
    check("vt: draw_rect_lines",   g->draw_rect_lines   != NULL);
    check("vt: draw_line",         g->draw_line         != NULL);
    check("vt: draw_circle",       g->draw_circle       != NULL);
    check("vt: draw_circle_lines", g->draw_circle_lines != NULL);
    check("vt: draw_text",         g->draw_text         != NULL);
    check("vt: measure_text",      g->measure_text      != NULL);
    check("vt: mouse_position",    g->mouse_position    != NULL);
    check("vt: mouse_delta",       g->mouse_delta       != NULL);
    check("vt: mouse_down",        g->mouse_down        != NULL);
    check("vt: mouse_pressed",     g->mouse_pressed     != NULL);
    check("vt: mouse_released",    g->mouse_released    != NULL);
    check("vt: mouse_wheel",       g->mouse_wheel       != NULL);
    check("vt: key_down",          g->key_down          != NULL);
    check("vt: key_pressed",       g->key_pressed       != NULL);
    check("vt: push_camera2d",     g->push_camera2d     != NULL);
    check("vt: pop_camera2d",      g->pop_camera2d      != NULL);
    check("vt: screen_to_world",   g->screen_to_world   != NULL);
    check("vt: world_to_screen",   g->world_to_screen   != NULL);
    check("vt: push_scissor",      g->push_scissor      != NULL);
    check("vt: pop_scissor",       g->pop_scissor       != NULL);
    check("vt: set_cursor",        g->set_cursor        != NULL);

    /* ── color macros round-trip ────────────────────────────── */
    uint32_t c = COLOR_RGBA(0x12, 0x34, 0x56, 0x78);
    check("COLOR_R", COLOR_R(c) == 0x12);
    check("COLOR_G", COLOR_G(c) == 0x34);
    check("COLOR_B", COLOR_B(c) == 0x56);
    check("COLOR_A", COLOR_A(c) == 0x78);
    check("COLOR_BLACK is opaque black", COLOR_BLACK == 0x000000FFu);
    check("COLOR_WHITE is opaque white", COLOR_WHITE == 0xFFFFFFFFu);

    /* ── rect / vec2 layout ─────────────────────────────────── */
    rect_t r = { 10, 20, 100, 200 };
    check("rect_t.x", r.x == 10);
    check("rect_t.h", r.h == 200);
    vec2_t v = { 3.5f, -7.25f };
    check("vec2_t.x", v.x == 3.5f);
    check("vec2_t.y", v.y == -7.25f);

    printf("\n%d / %d passed\n", total - failures, total);
    return failures;
}
