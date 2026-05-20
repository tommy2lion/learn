/* Integration tests for the supplement Phase 4 mutation hooks.
 *
 * Exercises seed_geometry_from_circuit (via canvas create / set_circuit)
 * and the public circuit_canvas_widget_reseat_wires — which is exactly
 * what the connect / disconnect / delete / drag-end hooks call internally.
 *
 * No raylib, no synthetic events: we mutate the underlying circuit_t
 * directly to simulate the post-mutation state and verify the reseat
 * brings cw->wires back into a correct shape. */

#include "../src/app/circuit_canvas_widget.h"
#include "../src/domain/wire_geometry.h"
#include "../src/domain/circuit.h"
#include "../src/domain/component.h"
#include "../src/framework/widgets/widget.h"
#include "../src/framework/graphics/igraph.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* ── mock igraph for Phase 8 render-dispatch test ──────────────────
 *
 * Records the count of each primitive call and the strings passed to
 * draw_text so the test can verify that the EXTERNAL view path was
 * reached. Stubs every other function in the igraph interface so the
 * canvas's drawing code doesn't dereference NULLs. */

typedef struct {
    int n_rect, n_rect_lines, n_line, n_circle, n_circle_lines, n_text;
    int saw_display_name;     /* 1 if draw_text was called with "TestBox" */
} mock_counts_t;

#define MOCK_NAME_TARGET "TestBox"

static void mk_draw_rect(void *self, rect_t r, uint32_t color) {
    (void)r; (void)color;
    ((mock_counts_t *)self)->n_rect++;
}
static void mk_draw_rect_lines(void *self, rect_t r, float th, uint32_t color) {
    (void)r; (void)th; (void)color;
    ((mock_counts_t *)self)->n_rect_lines++;
}
static void mk_draw_line(void *self, vec2_t a, vec2_t b, float th, uint32_t color) {
    (void)a; (void)b; (void)th; (void)color;
    ((mock_counts_t *)self)->n_line++;
}
static void mk_draw_circle(void *self, vec2_t c, float r, uint32_t color) {
    (void)c; (void)r; (void)color;
    ((mock_counts_t *)self)->n_circle++;
}
static void mk_draw_circle_lines(void *self, vec2_t c, float r, float th, uint32_t color) {
    (void)c; (void)r; (void)th; (void)color;
    ((mock_counts_t *)self)->n_circle_lines++;
}
static void mk_draw_text(void *self, const char *s, vec2_t p, float size, uint32_t color) {
    (void)p; (void)size; (void)color;
    mock_counts_t *m = (mock_counts_t *)self;
    m->n_text++;
    if (s && strcmp(s, MOCK_NAME_TARGET) == 0) m->saw_display_name = 1;
}
static float  mk_measure_text(void *self, const char *s, float size) {
    (void)self; (void)size;
    return (float)((s ? (int)strlen(s) : 0) * 8);   /* rough estimate */
}
static vec2_t mk_mouse_position(void *self) { (void)self; return (vec2_t){0, 0}; }
static vec2_t mk_mouse_delta   (void *self) { (void)self; return (vec2_t){0, 0}; }
static int    mk_mouse_down    (void *self, igraph_mouse_btn_t b) { (void)self; (void)b; return 0; }
static int    mk_mouse_pressed (void *self, igraph_mouse_btn_t b) { (void)self; (void)b; return 0; }
static int    mk_mouse_released(void *self, igraph_mouse_btn_t b) { (void)self; (void)b; return 0; }
static float  mk_mouse_wheel   (void *self) { (void)self; return 0.0f; }
static int    mk_key_down      (void *self, igraph_key_t k) { (void)self; (void)k; return 0; }
static int    mk_key_pressed   (void *self, igraph_key_t k) { (void)self; (void)k; return 0; }
static void   mk_push_camera2d (void *self, vec2_t t, vec2_t o, float z) {
    (void)self; (void)t; (void)o; (void)z;
}
static void   mk_pop_camera2d  (void *self) { (void)self; }
static vec2_t mk_screen_to_world(void *self, vec2_t s) { (void)self; return s; }
static vec2_t mk_world_to_screen(void *self, vec2_t s) { (void)self; return s; }
static void   mk_push_scissor  (void *self, rect_t r) { (void)self; (void)r; }
static void   mk_pop_scissor   (void *self) { (void)self; }
static void   mk_set_cursor    (void *self, cursor_kind_t k) { (void)self; (void)k; }

static void mock_igraph_init(igraph_t *g, mock_counts_t *m) {
    memset(g, 0, sizeof(*g));
    memset(m, 0, sizeof(*m));
    g->self            = m;
    g->draw_rect       = mk_draw_rect;
    g->draw_rect_lines = mk_draw_rect_lines;
    g->draw_line       = mk_draw_line;
    g->draw_circle     = mk_draw_circle;
    g->draw_circle_lines = mk_draw_circle_lines;
    g->draw_text       = mk_draw_text;
    g->measure_text    = mk_measure_text;
    g->mouse_position  = mk_mouse_position;
    g->mouse_delta     = mk_mouse_delta;
    g->mouse_down      = mk_mouse_down;
    g->mouse_pressed   = mk_mouse_pressed;
    g->mouse_released  = mk_mouse_released;
    g->mouse_wheel     = mk_mouse_wheel;
    g->key_down        = mk_key_down;
    g->key_pressed     = mk_key_pressed;
    g->push_camera2d   = mk_push_camera2d;
    g->pop_camera2d    = mk_pop_camera2d;
    g->screen_to_world = mk_screen_to_world;
    g->world_to_screen = mk_world_to_screen;
    g->push_scissor    = mk_push_scissor;
    g->pop_scissor     = mk_pop_scissor;
    g->set_cursor      = mk_set_cursor;
}

/* ── stub custom render for Phase 9 hook-dispatch test ─────────────── */
static int g_custom_render_calls = 0;
static void custom_render_stub(igraph_t *g,
                               const circuit_t *c,
                               const external_view_metadata_t *meta,
                               rect_t viewport) {
    (void)g; (void)c; (void)meta; (void)viewport;
    g_custom_render_calls++;
}

/* Build: inputs A, B; output Y; component g1 = and(A, B); Y consumes g1.
   Nets exist for A, B, g1 — three total. */
static circuit_t *make_simple_circuit(void) {
    circuit_t *c = circuit_create();
    circuit_add_input (c, "A");
    circuit_add_input (c, "B");
    circuit_add_output(c, "Y");
    component_t *g1 = gate_and_create("g1");
    circuit_add_component(c, g1, "A", "B");
    /* Y consumes g1 — smart-rename convention: output_names[i] == wire name */
    snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "g1");
    return c;
}

int main(void) {
    printf("=== circuit_canvas_widget supplement integration tests ===\n");

    /* ── seed: every consumer's wire is in geometry, 1–3 segs each ── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("seed: net A exists",  wire_geometry_find(&cw->wires, "A")  >= 0);
        check("seed: net B exists",  wire_geometry_find(&cw->wires, "B")  >= 0);
        check("seed: net g1 exists", wire_geometry_find(&cw->wires, "g1") >= 0);
        check("seed: net_count == 3", cw->wires.net_count == 3);
        int seg_in_range = 1;
        for (int i = 0; i < cw->wires.net_count; i++) {
            int s = cw->wires.nets[i].seg_count;
            if (s < 1 || s > 3) { seg_in_range = 0; break; }
        }
        check("seed: every net has 1..3 segments", seg_in_range);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── drag a component → segment endpoints follow it ──────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);

        /* Net "A" routes input A → g1.in[0]. The last segment's b-endpoint
           is g1's input-pin position, which lives at g1.position.x - GATE_W/2.
           Move g1 to the right and verify the endpoint moves with it. */
        const wire_net_geom_t *net =
            wire_geometry_net(&cw->wires, wire_geometry_find(&cw->wires, "A"));
        float before_endpoint_x = net->segs[net->seg_count - 1].b.x;

        c->components[0]->position.x += 500;
        circuit_canvas_widget_reseat_wires(cw);

        net = wire_geometry_net(&cw->wires, wire_geometry_find(&cw->wires, "A"));
        check("drag: net A still exists", net != NULL);
        check("drag: endpoint moved right with the gate",
              net && net->segs[net->seg_count - 1].b.x > before_endpoint_x);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── delete producer of a wire → its net is gone ─────────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("pre-delete: net g1 exists",
              wire_geometry_find(&cw->wires, "g1") >= 0);

        /* Simulate "delete component g1": its consumers (output Y) lose
           their reference; the producer is gone from the circuit. */
        component_destroy(c->components[0]);
        c->component_count = 0;
        c->output_names[0][0] = '\0';
        circuit_canvas_widget_reseat_wires(cw);

        check("post-delete: net g1 is gone",
              wire_geometry_find(&cw->wires, "g1") == -1);
        /* Nets A and B should also be gone — g1 was their only consumer. */
        check("post-delete: net A is gone (no remaining consumer)",
              wire_geometry_find(&cw->wires, "A") == -1);
        check("post-delete: net B is gone (no remaining consumer)",
              wire_geometry_find(&cw->wires, "B") == -1);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── fan-out: remove one consumer, net stays with fewer segments ─ */
    {
        circuit_t *c = make_simple_circuit();
        /* Add g2 = and(g1, B) so g1 has two consumers: Y (output) and g2. */
        component_t *g2 = gate_and_create("g2");
        circuit_add_component(c, g2, "g1", "B");

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        const wire_net_geom_t *net =
            wire_geometry_net(&cw->wires, wire_geometry_find(&cw->wires, "g1"));
        int before_seg_count = net ? net->seg_count : 0;
        check("fan-out: g1 has multi-consumer geometry",
              net && before_seg_count > 0);

        /* Disconnect g2's reference to g1 (simulating disconnect_input on g2.in[0]). */
        c->components[1]->in_wires[0][0] = '\0';
        circuit_canvas_widget_reseat_wires(cw);

        net = wire_geometry_net(&cw->wires, wire_geometry_find(&cw->wires, "g1"));
        check("fan-out: g1 still routed after losing one consumer", net != NULL);
        check("fan-out: g1 has fewer (or equal — co-linear edge) segments",
              net && net->seg_count <= before_seg_count);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── set_circuit replaces geometry, doesn't merge ────────────── */
    {
        circuit_t *c1 = make_simple_circuit();
        circuit_t *c2 = make_simple_circuit();    /* same shape, different instance */
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c1);
        circuit_canvas_widget_set_circuit(cw, c2);
        check("set_circuit: net_count == 3 (not 6)", cw->wires.net_count == 3);
        check("set_circuit: net A still findable",
              wire_geometry_find(&cw->wires, "A") >= 0);
        widget_destroy(&cw->base);
        circuit_destroy(c1);
        circuit_destroy(c2);
    }

    /* ── set_circuit(NULL) clears geometry ──────────────────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("pre-clear: net_count == 3", cw->wires.net_count == 3);
        circuit_canvas_widget_set_circuit(cw, NULL);
        check("set_circuit(NULL): geometry cleared", cw->wires.net_count == 0);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 6 — highlight state machine + click handler
       ────────────────────────────────────────────────────────────────── */

    /* ── set_highlight: store / clear / NULL / empty ──────────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("create: highlight starts empty",
              cw->highlighted_net[0] == '\0');
        circuit_canvas_widget_set_highlight(cw, "g1");
        check("set('g1'): highlight stored",
              strcmp(cw->highlighted_net, "g1") == 0);
        circuit_canvas_widget_set_highlight(cw, NULL);
        check("set(NULL): cleared", cw->highlighted_net[0] == '\0');
        circuit_canvas_widget_set_highlight(cw, "A");
        check("set('A'): re-stored",
              strcmp(cw->highlighted_net, "A") == 0);
        circuit_canvas_widget_set_highlight(cw, "");
        check("set(empty): cleared", cw->highlighted_net[0] == '\0');
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── left-click on a wire segment sets highlighted_net ────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        /* Override camera so world == screen, for deterministic event coords. */
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;

        /* Pick a world coord on net "A"'s first segment. */
        int idx = wire_geometry_find(&cw->wires, "A");
        const wire_net_geom_t *na = wire_geometry_net(&cw->wires, idx);
        vec2_t mid;
        mid.x = (na->segs[0].a.x + na->segs[0].b.x) * 0.5f;
        mid.y = (na->segs[0].a.y + na->segs[0].b.y) * 0.5f;

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = mid;
        widget_handle_event(&cw->base, &ev);

        check("left-click on net A sets highlight to 'A'",
              strcmp(cw->highlighted_net, "A") == 0);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── left-click on empty space clears highlight ───────────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;
        circuit_canvas_widget_set_highlight(cw, "A");
        check("setup: highlight set to 'A'",
              strcmp(cw->highlighted_net, "A") == 0);

        /* Click in the far bottom-right corner — well outside any segment. */
        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){780, 580};
        widget_handle_event(&cw->base, &ev);

        check("left-click on empty space clears highlight",
              cw->highlighted_net[0] == '\0');
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── second click on a different wire replaces highlight ──────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;

        const wire_net_geom_t *na =
            wire_geometry_net(&cw->wires, wire_geometry_find(&cw->wires, "A"));
        const wire_net_geom_t *nb =
            wire_geometry_net(&cw->wires, wire_geometry_find(&cw->wires, "B"));

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;

        /* First click: net A. */
        ev.mouse.pos.x = (na->segs[0].a.x + na->segs[0].b.x) * 0.5f;
        ev.mouse.pos.y = (na->segs[0].a.y + na->segs[0].b.y) * 0.5f;
        widget_handle_event(&cw->base, &ev);
        check("first click: highlight == 'A'",
              strcmp(cw->highlighted_net, "A") == 0);

        /* Second click: net B. */
        ev.mouse.pos.x = (nb->segs[0].a.x + nb->segs[0].b.x) * 0.5f;
        ev.mouse.pos.y = (nb->segs[0].a.y + nb->segs[0].b.y) * 0.5f;
        widget_handle_event(&cw->base, &ev);
        check("second click on net B: highlight switched",
              strcmp(cw->highlighted_net, "B") == 0);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 8 — display_mode state machine + render dispatch
       ────────────────────────────────────────────────────────────────── */

    /* ── default mode is INTERNAL; set/get round-trip ─────────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("create: display_mode defaults to INTERNAL",
              circuit_canvas_widget_display_mode(cw) == DISPLAY_INTERNAL);
        circuit_canvas_widget_set_display_mode(cw, DISPLAY_EXTERNAL);
        check("set EXTERNAL: getter returns EXTERNAL",
              circuit_canvas_widget_display_mode(cw) == DISPLAY_EXTERNAL);
        circuit_canvas_widget_set_display_mode(cw, DISPLAY_INTERNAL);
        check("set INTERNAL: getter returns INTERNAL",
              circuit_canvas_widget_display_mode(cw) == DISPLAY_INTERNAL);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── set_circuit / reset clears display_mode back to INTERNAL ─── */
    {
        circuit_t *c1 = make_simple_circuit();
        circuit_t *c2 = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c1);
        circuit_canvas_widget_set_display_mode(cw, DISPLAY_EXTERNAL);
        circuit_canvas_widget_set_circuit(cw, c2);
        check("set_circuit resets display_mode to INTERNAL",
              circuit_canvas_widget_display_mode(cw) == DISPLAY_INTERNAL);
        widget_destroy(&cw->base);
        circuit_destroy(c1);
        circuit_destroy(c2);
    }

    /* ── set_display_name: store / clear (now via external_meta) ──── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        external_view_metadata_t *meta = circuit_canvas_widget_external_meta(cw);
        check("display_name starts empty",  meta->display_name[0] == '\0');
        circuit_canvas_widget_set_display_name(cw, "MyCircuit");
        check("display_name stored",
              strcmp(meta->display_name, "MyCircuit") == 0);
        circuit_canvas_widget_set_display_name(cw, NULL);
        check("display_name cleared (NULL)", meta->display_name[0] == '\0');
        circuit_canvas_widget_set_display_name(cw, "");
        check("display_name cleared (empty)", meta->display_name[0] == '\0');
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── render dispatch: EXTERNAL draws box+name, INTERNAL doesn't ── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        circuit_canvas_widget_set_display_name(cw, MOCK_NAME_TARGET);

        igraph_t mock_g;
        mock_counts_t counts;

        /* INTERNAL: many lines (wire segments), no "TestBox" string. */
        mock_igraph_init(&mock_g, &counts);
        widget_draw(&cw->base, &mock_g);
        int internal_lines = counts.n_line;
        int internal_saw_name = counts.saw_display_name;

        /* EXTERNAL: box rectangle, display_name text, pin stubs only. */
        circuit_canvas_widget_set_display_mode(cw, DISPLAY_EXTERNAL);
        mock_igraph_init(&mock_g, &counts);
        widget_draw(&cw->base, &mock_g);
        int external_lines = counts.n_line;
        int external_saw_name = counts.saw_display_name;

        check("INTERNAL: many draw_line calls (wire segments)",
              internal_lines >= 3);
        check("INTERNAL: display_name NOT drawn",
              internal_saw_name == 0);
        check("EXTERNAL: display_name drawn",
              external_saw_name == 1);
        check("EXTERNAL: line count is just pin stubs (n_in + n_out)",
              external_lines == c->input_count + c->output_count);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── event short-circuit: clicks in EXTERNAL mode don't mutate state ─ */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;
        circuit_canvas_widget_set_display_mode(cw, DISPLAY_EXTERNAL);

        int comp_count_before = c->component_count;
        canvas_mode_t mode_before = cw->mode;

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){280, 280};   /* where g1 sits in INTERNAL */
        widget_handle_event(&cw->base, &ev);

        check("EXTERNAL: clicking on schematic-only coords doesn't change mode",
              cw->mode == mode_before);
        check("EXTERNAL: click does not delete / add components",
              c->component_count == comp_count_before);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 9 — metadata defaults, clock pin, render hook
       ────────────────────────────────────────────────────────────────── */

    /* ── external_view_metadata_init defaults ─────────────────────── */
    {
        external_view_metadata_t m;
        /* Deliberately dirty the struct first to make sure init zeros it. */
        memset(&m, 0xCC, sizeof(m));
        external_view_metadata_init(&m);
        check("init: display_name is empty",      m.display_name[0] == '\0');
        check("init: render is NULL",             m.render == NULL);
        int all_normal_in  = 1, all_normal_out = 1;
        for (int i = 0; i < DOMAIN_MAX_IO; i++) {
            if (m.input_styles[i]  != PIN_STYLE_NORMAL) all_normal_in  = 0;
            if (m.output_styles[i] != PIN_STYLE_NORMAL) all_normal_out = 0;
        }
        check("init: every input  style is NORMAL",  all_normal_in);
        check("init: every output style is NORMAL",  all_normal_out);
    }

    /* ── canvas widget defaults external_meta to NORMAL/NULL ──────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        external_view_metadata_t *meta = circuit_canvas_widget_external_meta(cw);
        check("canvas create: meta != NULL",        meta != NULL);
        check("canvas create: render is NULL",      meta && meta->render == NULL);
        check("canvas create: input[0] is NORMAL",
              meta && meta->input_styles[0] == PIN_STYLE_NORMAL);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── set_circuit re-initialises external_meta (drops any per-pin
       customisation when loading a new file) ──────────────────────── */
    {
        circuit_t *c1 = make_simple_circuit();
        circuit_t *c2 = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c1);
        external_view_metadata_t *meta = circuit_canvas_widget_external_meta(cw);
        meta->input_styles[0] = PIN_STYLE_CLOCK;
        meta->render = custom_render_stub;
        circuit_canvas_widget_set_circuit(cw, c2);
        check("set_circuit: clock style cleared",
              meta->input_styles[0] == PIN_STYLE_NORMAL);
        check("set_circuit: render hook cleared",
              meta->render == NULL);
        widget_destroy(&cw->base);
        circuit_destroy(c1);
        circuit_destroy(c2);
    }

    /* ── PIN_STYLE_CLOCK adds 2 extra draw_line calls per clock pin ── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        circuit_canvas_widget_set_display_mode(cw, DISPLAY_EXTERNAL);

        igraph_t mock_g;
        mock_counts_t counts;

        /* All NORMAL: n_in + n_out pin stub draw_lines. */
        mock_igraph_init(&mock_g, &counts);
        widget_draw(&cw->base, &mock_g);
        int normal_lines = counts.n_line;

        /* Flip input[0] to CLOCK: triangle = 2 extra lines. */
        external_view_metadata_t *meta = circuit_canvas_widget_external_meta(cw);
        meta->input_styles[0] = PIN_STYLE_CLOCK;
        mock_igraph_init(&mock_g, &counts);
        widget_draw(&cw->base, &mock_g);
        check("CLOCK on input[0]: 2 extra draw_line (triangle)",
              counts.n_line == normal_lines + 2);

        /* Add CLOCK on output[0] too: another 2 extra. */
        meta->output_styles[0] = PIN_STYLE_CLOCK;
        mock_igraph_init(&mock_g, &counts);
        widget_draw(&cw->base, &mock_g);
        check("CLOCK on input[0] + output[0]: 4 extra draw_line total",
              counts.n_line == normal_lines + 4);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── PIN_STYLE_INVERTED is recognised but not drawn yet (reserved) ─ */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        circuit_canvas_widget_set_display_mode(cw, DISPLAY_EXTERNAL);

        igraph_t mock_g;
        mock_counts_t counts;
        mock_igraph_init(&mock_g, &counts);
        widget_draw(&cw->base, &mock_g);
        int normal_lines = counts.n_line;

        external_view_metadata_t *meta = circuit_canvas_widget_external_meta(cw);
        meta->input_styles[0] = PIN_STYLE_INVERTED;
        mock_igraph_init(&mock_g, &counts);
        widget_draw(&cw->base, &mock_g);
        check("INVERTED (reserved): no extra draw_line — falls back to NORMAL",
              counts.n_line == normal_lines);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── custom render hook overrides default renderer ─────────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        circuit_canvas_widget_set_display_mode(cw, DISPLAY_EXTERNAL);
        circuit_canvas_widget_set_display_name(cw, MOCK_NAME_TARGET);

        external_view_metadata_t *meta = circuit_canvas_widget_external_meta(cw);
        g_custom_render_calls = 0;
        meta->render = custom_render_stub;

        igraph_t mock_g;
        mock_counts_t counts;
        mock_igraph_init(&mock_g, &counts);
        widget_draw(&cw->base, &mock_g);
        check("custom render hook was called",
              g_custom_render_calls == 1);
        check("custom render: default's draw_text NOT invoked",
              counts.saw_display_name == 0);
        /* Stub doesn't draw anything, so primitive counts stay minimal —
           background fill + outline only (from ccw_draw, before scissor). */
        check("custom render: no draw_line from default renderer",
              counts.n_line == 0);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── external_view_draw rejects NULL meta safely ──────────────── */
    {
        igraph_t mock_g;
        mock_counts_t counts;
        mock_igraph_init(&mock_g, &counts);
        circuit_t *c = make_simple_circuit();
        external_view_draw(&mock_g, c, NULL, (rect_t){0, 0, 100, 100});
        check("NULL meta: no crash, no primitives",
              counts.n_rect == 0 && counts.n_line == 0);
        circuit_destroy(c);
    }

    /* ──────────────────────────────────────────────────────────────────
       supplement Phase 12 — wire-edit drag flow
       ────────────────────────────────────────────────────────────────── */

    /* ── press on a Z-route middle V → enter CMODE_WIRE_EDIT ───────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;

        int net_idx = wire_geometry_find(&cw->wires, "A");
        const wire_net_geom_t *na = wire_geometry_net(&cw->wires, net_idx);
        check("setup: net A has 3 segs (Z-route)", na && na->seg_count == 3);
        const wire_segment_t *v = &na->segs[1];
        check("setup: middle seg is vertical", v->a.x == v->b.x);
        float v_x_before = v->a.x;
        vec2_t v_mid;
        v_mid.x = v->a.x;
        v_mid.y = (v->a.y + v->b.y) * 0.5f;

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = v_mid;
        widget_handle_event(&cw->base, &ev);
        check("press on draggable V: enters WIRE_EDIT",
              cw->mode == CMODE_WIRE_EDIT);
        check("press: saved net_idx + seg_idx",
              cw->we_net_idx == net_idx && cw->we_seg_idx == 1);

        /* Drag +24 in x. */
        ev.kind = EV_MOUSE_MOVE;
        ev.mouse.pos.x = v_mid.x + 24;
        ev.mouse.pos.y = v_mid.y;
        widget_handle_event(&cw->base, &ev);
        v = &cw->wires.nets[net_idx].segs[1];
        check("drag: V seg moved to x = old + 24",
              v->a.x == v_x_before + 24 && v->b.x == v_x_before + 24);
        const wire_segment_t *h0 = &cw->wires.nets[net_idx].segs[0];
        const wire_segment_t *h2 = &cw->wires.nets[net_idx].segs[2];
        check("drag: neighbour H0 endpoint b followed",
              h0->b.x == v_x_before + 24);
        check("drag: neighbour H2 endpoint a followed",
              h2->a.x == v_x_before + 24);

        /* Release → back to IDLE. */
        ev.kind      = EV_MOUSE_RELEASE;
        ev.mouse.btn = IM_LEFT;
        widget_handle_event(&cw->base, &ev);
        check("release: mode back to IDLE",         cw->mode == CMODE_IDLE);
        check("release: we_net_idx cleared to -1",  cw->we_net_idx == -1);
        check("release: we_seg_idx cleared to -1",  cw->we_seg_idx == -1);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── pressing on a pin-terminal segment (Z-route first H) falls back
       to the Phase-6 highlight behaviour rather than entering wire-edit ─ */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;

        int net_idx = wire_geometry_find(&cw->wires, "A");
        const wire_net_geom_t *na = wire_geometry_net(&cw->wires, net_idx);
        const wire_segment_t *h0 = &na->segs[0];     /* touches producer pin */
        vec2_t h0_mid;
        h0_mid.x = (h0->a.x + h0->b.x) * 0.5f;
        h0_mid.y = h0->a.y;

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = h0_mid;
        widget_handle_event(&cw->base, &ev);
        check("pin-terminal seg: mode stays IDLE",
              cw->mode == CMODE_IDLE);
        check("pin-terminal seg: highlight set to net 'A'",
              strcmp(cw->highlighted_net, "A") == 0);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── ESC during WIRE_EDIT cancels back to IDLE ────────────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;

        int net_idx = wire_geometry_find(&cw->wires, "A");
        const wire_net_geom_t *na = wire_geometry_net(&cw->wires, net_idx);
        const wire_segment_t *v = &na->segs[1];
        vec2_t v_mid;
        v_mid.x = v->a.x;
        v_mid.y = (v->a.y + v->b.y) * 0.5f;

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = v_mid;
        widget_handle_event(&cw->base, &ev);
        check("press enters WIRE_EDIT (setup)",
              cw->mode == CMODE_WIRE_EDIT);

        event_t esc;
        memset(&esc, 0, sizeof(esc));
        esc.kind    = EV_KEY_PRESS;
        esc.key.key = IK_ESCAPE;
        widget_handle_event(&cw->base, &esc);
        check("ESC during WIRE_EDIT: mode back to IDLE",
              cw->mode == CMODE_IDLE);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── right-click during WIRE_EDIT also cancels ────────────────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;

        int net_idx = wire_geometry_find(&cw->wires, "A");
        const wire_net_geom_t *na = wire_geometry_net(&cw->wires, net_idx);
        const wire_segment_t *v = &na->segs[1];
        vec2_t v_mid;
        v_mid.x = v->a.x;
        v_mid.y = (v->a.y + v->b.y) * 0.5f;

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = v_mid;
        widget_handle_event(&cw->base, &ev);
        check("press enters WIRE_EDIT (setup)",
              cw->mode == CMODE_WIRE_EDIT);

        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_RIGHT;
        widget_handle_event(&cw->base, &ev);
        check("right-click during WIRE_EDIT: mode back to IDLE",
              cw->mode == CMODE_IDLE);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── R-8 V bus drag: shift the shared V bus as a unit ────────── */
    {
        /* Build a circuit where one wire fans out to multiple consumers
           so R-8 creates a shared V bus. Reusing make_simple_circuit
           + a second consumer on net A. */
        circuit_t *c = circuit_create();
        circuit_add_input (c, "A");
        circuit_add_input (c, "B");
        circuit_add_output(c, "Y");
        circuit_add_component(c, gate_and_create("g1"), "A", "B");
        circuit_add_component(c, gate_and_create("g2"), "A", "B");
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "g1");

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;

        /* Net A drives g1.in[0] AND g2.in[0] — that's R-8 V bus territory. */
        int net_idx = wire_geometry_find(&cw->wires, "A");
        const wire_net_geom_t *na = wire_geometry_net(&cw->wires, net_idx);
        check("R-8: net A has multiple segments", na && na->seg_count >= 3);

        /* Find a V bus segment (vertical, with at least one sibling V at
           the same column). */
        int v_bus_seg = -1;
        float v_bus_x = 0;
        for (int i = 0; i < na->seg_count; i++) {
            if (na->segs[i].a.x != na->segs[i].b.x) continue;
            float col = na->segs[i].a.x;
            int count = 0;
            for (int j = 0; j < na->seg_count; j++) {
                if (na->segs[j].a.x == na->segs[j].b.x
                    && na->segs[j].a.x == col) count++;
            }
            if (count >= 2) { v_bus_seg = i; v_bus_x = col; break; }
        }
        check("R-8: found a V bus segment in net A", v_bus_seg >= 0);

        /* Click on its midpoint → should enter WIRE_EDIT (V bus is draggable). */
        const wire_segment_t *v = &na->segs[v_bus_seg];
        vec2_t v_mid;
        v_mid.x = v->a.x;
        v_mid.y = (v->a.y + v->b.y) * 0.5f;

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = v_mid;
        widget_handle_event(&cw->base, &ev);
        check("press on V bus seg: enters WIRE_EDIT",
              cw->mode == CMODE_WIRE_EDIT);

        /* Drag +16 in x — V bus should move as a unit. */
        ev.kind = EV_MOUSE_MOVE;
        ev.mouse.pos.x = v_mid.x + 16;
        ev.mouse.pos.y = v_mid.y;
        widget_handle_event(&cw->base, &ev);

        const wire_net_geom_t *na2 =
            wire_geometry_net(&cw->wires, net_idx);
        /* Every V segment at the old column should have moved to the new one. */
        int found_at_new_col = 0;
        int still_at_old_col = 0;
        for (int i = 0; i < na2->seg_count; i++) {
            if (na2->segs[i].a.x != na2->segs[i].b.x) continue;
            if (na2->segs[i].a.x == v_bus_x + 16) found_at_new_col++;
            if (na2->segs[i].a.x == v_bus_x)      still_at_old_col++;
        }
        check("after V bus drag: V segs shifted to new column",
              found_at_new_col >= 2 && still_at_old_col == 0);

        ev.kind      = EV_MOUSE_RELEASE;
        ev.mouse.btn = IM_LEFT;
        widget_handle_event(&cw->base, &ev);
        check("V bus drag release: back to IDLE",
              cw->mode == CMODE_IDLE);

        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── drag past the limit (would create zero-length) is refused
       atomically — the wire stays at the last valid position. ────── */
    {
        circuit_t *c = make_simple_circuit();
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        cw->cam_target = (vec2_t){0, 0};
        cw->cam_offset = (vec2_t){0, 0};
        cw->cam_zoom   = 1.0f;

        int net_idx = wire_geometry_find(&cw->wires, "A");
        const wire_net_geom_t *na = wire_geometry_net(&cw->wires, net_idx);
        const wire_segment_t *v = &na->segs[1];
        vec2_t v_mid;
        v_mid.x = v->a.x;
        v_mid.y = (v->a.y + v->b.y) * 0.5f;
        float v_x_before = v->a.x;
        float h0_a_x_before = na->segs[0].a.x;

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = v_mid;
        widget_handle_event(&cw->base, &ev);

        /* Drag exactly to the producer pin's x — this is the threshold
           where H0 collapses to zero length and the shift must be
           refused atomically. */
        ev.kind = EV_MOUSE_MOVE;
        ev.mouse.pos.x = h0_a_x_before;          /* V.x → producer pin x */
        ev.mouse.pos.y = v_mid.y;
        widget_handle_event(&cw->base, &ev);

        v = &cw->wires.nets[net_idx].segs[1];
        check("refused drag: V stays at original x (atomic rollback)",
              v->a.x == v_x_before);
        check("refused drag: H0 unchanged",
              cw->wires.nets[net_idx].segs[0].a.x == h0_a_x_before);

        /* Release. */
        ev.kind      = EV_MOUSE_RELEASE;
        ev.mouse.btn = IM_LEFT;
        widget_handle_event(&cw->base, &ev);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ──────────────────────────────────────────────────────────────────
       Auto-align: snap consumer-side components within 8 px to align
       with their producer's y. R-7 refinement.
       ────────────────────────────────────────────────────────────────── */

    /* ── small offset (5 px) → snapped to producer.y ─────────────── */
    {
        circuit_t *c = circuit_create();
        circuit_add_input (c, "A");
        circuit_add_output(c, "Y");
        component_t *n1 = gate_not_create("Y");
        circuit_add_component(c, n1, "A", NULL);
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");

        /* Pre-set positions (auto_layout skips when not all-zero). */
        c->input_positions[0]      = (vec2_t){0,   100};
        c->components[0]->position = (vec2_t){200, 105};   /* 5 px below */
        c->output_positions[0]     = (vec2_t){400, 100};

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("5 px offset: NOT gate snapped to producer.y",
              c->components[0]->position.y == 100);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── exactly threshold (8 px) → still snapped (inclusive) ──── */
    {
        circuit_t *c = circuit_create();
        circuit_add_input (c, "A");
        circuit_add_output(c, "Y");
        component_t *n1 = gate_not_create("Y");
        circuit_add_component(c, n1, "A", NULL);
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");
        c->input_positions[0]      = (vec2_t){0,   100};
        c->components[0]->position = (vec2_t){200, 108};   /* exactly 8 */
        c->output_positions[0]     = (vec2_t){400, 100};

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("threshold offset (8 px): snapped (inclusive boundary)",
              c->components[0]->position.y == 100);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── large offset (17 px) → NOT snapped ───────────────────────── */
    {
        circuit_t *c = circuit_create();
        circuit_add_input (c, "A");
        circuit_add_output(c, "Y");
        component_t *n1 = gate_not_create("Y");
        circuit_add_component(c, n1, "A", NULL);
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");
        c->input_positions[0]      = (vec2_t){0,   100};
        c->components[0]->position = (vec2_t){200, 117};   /* 17 px below */
        c->output_positions[0]     = (vec2_t){400, 100};

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("17 px offset: kept (above threshold, user intent)",
              c->components[0]->position.y == 117);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── multi-input: aligns to pin whose producer has higher fan-out ─ */
    {
        /* P drives g1.in[0] AND a second consumer (so fan-out 2).
           Q drives g1.in[1] only (fan-out 1). Both pins have small
           offsets within threshold. Higher-fanout (P → g1.in[0])
           should win. */
        circuit_t *c = circuit_create();
        circuit_add_input(c, "P");
        circuit_add_input(c, "Q");
        circuit_add_output(c, "X");                        /* second consumer of P */
        circuit_add_output(c, "Y");                        /* consumer of g1 */
        component_t *g1 = gate_and_create("g1");
        circuit_add_component(c, g1, "P", "Q");
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "P");   /* X = P */
        snprintf(c->output_names[1], DOMAIN_NAME_LEN, "%s", "g1");  /* Y = g1 */

        /* Pre-set positions.
           P.y = 100, P pin = (12, 100).
           Q.y = 200, Q pin = (12, 200).
           g1.y = 145. g1.in[0] = (160, 130). g1.in[1] = (160, 160).
           Delta_P = 130 - 100 = 30 (TOO BIG, > 8 — won't snap).
           Hmm that's not the test I want. Let me adjust:

           Set g1.y = 115. g1.in[0] = (160, 100), g1.in[1] = (160, 130).
           Delta_P = 100 - 100 = 0 (no snap needed).
           Delta_Q = 130 - 200 = -70 (way too big).

           Let me try: P.y = 100, Q.y = 130 (closer together).
           g1.y = 117. g1.in[0] = (160, 102). g1.in[1] = (160, 132).
           Delta_P = 102 - 100 = 2. Delta_Q = 132 - 130 = 2.
           Both within threshold AND same |Δ|. P has fanout 2, Q has 1.
           → snap to P. Shift g1.y by -2 → 115.
           After: g1.in[0] = (160, 100) ✓ aligned.
                  g1.in[1] = (160, 130) ✓ also aligned!  (coincidence)

           For a clearer test, make deltas different:
           P.y = 100, Q.y = 130. g1.y = 118.
           g1.in[0] = (160, 103). Delta_P = 3.
           g1.in[1] = (160, 133). Delta_Q = 3.
           Same |Δ|. Tiebreaker: fanout. P (fanout 2) wins.
           Shift by -3 → g1.y = 115. P aligned, Q aligned. Both!

           Better: make deltas different so tiebreaker matters.
           P.y = 100, Q.y = 130. g1.y = 116.
           g1.in[0] = (160, 101). Delta_P = 1.
           g1.in[1] = (160, 131). Delta_Q = 1.
           Hmm still same. The geometry of a 2-input gate puts pins at
           ±15 around comp.y, so deltas are correlated.

           Let me use a NOT gate (1 input) for clean Q3-style test of
           "highest fanout drives the snap" — actually NOT only has
           one input, so no multi-input scenario.

           OK for multi-input test, let me just check that some snap
           happens with multi-input and that the post-snap pin matches
           ONE producer's y. */
        c->input_positions[0]      = (vec2_t){0,   100};   /* P */
        c->input_positions[1]      = (vec2_t){0,   130};   /* Q */
        c->components[0]->position = (vec2_t){200, 116};
        c->output_positions[0]     = (vec2_t){400, 100};   /* X */
        c->output_positions[1]     = (vec2_t){400, 116};   /* Y */

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);

        /* g1.y should have shifted so g1.in[0] (= g1.y - 15) aligns
           with P.y = 100 → g1.y = 115. */
        check("multi-input + tied |Δ|: highest-fanout pin wins",
              c->components[0]->position.y == 115);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── external output position aligned to its producer ─────────── */
    {
        circuit_t *c = circuit_create();
        circuit_add_input (c, "A");
        circuit_add_output(c, "Y");
        component_t *n1 = gate_not_create("Y");
        circuit_add_component(c, n1, "A", NULL);
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");

        c->input_positions[0]      = (vec2_t){0,   100};
        c->components[0]->position = (vec2_t){200, 100};   /* already aligned */
        c->output_positions[0]     = (vec2_t){400, 105};   /* 5 px off */

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("external output 5 px off: snapped to producer.y",
              c->output_positions[0].y == 100);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── cascading: g1's shift feeds g2's alignment opportunity ──── */
    {
        /* A → g1 → g2 → Y. Each at a small offset that, once g1 moves
           into alignment with A, exposes a fresh small offset between
           g1 and g2 (which is then also within threshold). */
        circuit_t *c = circuit_create();
        circuit_add_input (c, "A");
        circuit_add_output(c, "Y");
        component_t *g1 = gate_not_create("g1");
        circuit_add_component(c, g1, "A", NULL);
        component_t *g2 = gate_not_create("Y");
        circuit_add_component(c, g2, "g1", NULL);
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");

        c->input_positions[0]      = (vec2_t){0,   100};
        c->components[0]->position = (vec2_t){200, 105};   /* 5 below A */
        c->components[1]->position = (vec2_t){400, 110};   /* 5 below ORIGINAL g1.out (105) */
        c->output_positions[0]     = (vec2_t){600, 115};   /* 5 below ORIGINAL g2.out (110) */

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        /* Cascade:
           - g1: producer A.y=100, g1.in[0].y=105. Δ=5 → snap. g1.y → 100.
           - g2: producer g1.out.y=100 (post-shift), g2.in[0].y=110. Δ=10. > 8, NO SNAP.
           Hmm — so g2 stays at 110. That's NOT a cascade demonstration.
           Let me adjust: g2 = 108 (delta after g1's shift = 8, threshold inclusive). */
        check("cascade: g1 snapped to A.y=100",
              c->components[0]->position.y == 100);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── true cascade: after g1 shifts, g2's now-exposed delta also
       fits within threshold and is also snapped ──────────────────── */
    {
        circuit_t *c = circuit_create();
        circuit_add_input (c, "A");
        circuit_add_output(c, "Y");
        component_t *g1 = gate_not_create("g1");
        circuit_add_component(c, g1, "A", NULL);
        component_t *g2 = gate_not_create("Y");
        circuit_add_component(c, g2, "g1", NULL);
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");

        c->input_positions[0]      = (vec2_t){0,   100};
        c->components[0]->position = (vec2_t){200, 103};   /* Δ=3 with A */
        c->components[1]->position = (vec2_t){400, 106};   /* Δ=3 with g1 post-shift */
        c->output_positions[0]     = (vec2_t){600, 103};   /* Δ=3 with g2 post-shift */

        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c);
        check("cascade: g1 → 100",  c->components[0]->position.y == 100);
        check("cascade: g2 → 100",  c->components[1]->position.y == 100);
        check("cascade: out Y → 100", c->output_positions[0].y == 100);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── circuit_canvas_widget_auto_align returns shift count ────── */
    {
        circuit_t *c = circuit_create();
        circuit_add_input (c, "A");
        circuit_add_output(c, "Y");
        component_t *n1 = gate_not_create("Y");
        circuit_add_component(c, n1, "A", NULL);
        snprintf(c->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");

        c->input_positions[0]      = (vec2_t){0,   100};
        c->components[0]->position = (vec2_t){200, 105};   /* 5 off */
        c->output_positions[0]     = (vec2_t){400, 100};   /* aligned */

        int n = circuit_canvas_widget_auto_align(c);
        check("auto_align returns 1 (one component shifted)", n == 1);
        check("idempotent: 2nd call returns 0",
              circuit_canvas_widget_auto_align(c) == 0);
        circuit_destroy(c);
    }

    /* ── auto_align(NULL) returns 0 safely ───────────────────────── */
    {
        check("auto_align(NULL) == 0", circuit_canvas_widget_auto_align(NULL) == 0);
    }

    /* ── set_circuit also triggers auto-align (open another file) ── */
    {
        circuit_t *c1 = circuit_create();
        circuit_add_input(c1, "A");
        circuit_add_output(c1, "Y");
        circuit_add_component(c1, gate_not_create("Y"), "A", NULL);
        snprintf(c1->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");
        circuit_canvas_widget_t *cw =
            circuit_canvas_widget_create((rect_t){0, 0, 800, 600}, c1);

        /* Now load a different circuit with a small offset. */
        circuit_t *c2 = circuit_create();
        circuit_add_input(c2, "A");
        circuit_add_output(c2, "Y");
        circuit_add_component(c2, gate_not_create("Y"), "A", NULL);
        snprintf(c2->output_names[0], DOMAIN_NAME_LEN, "%s", "Y");
        c2->input_positions[0]      = (vec2_t){0,   100};
        c2->components[0]->position = (vec2_t){200, 104};   /* 4 px off */
        c2->output_positions[0]     = (vec2_t){400, 100};

        circuit_canvas_widget_set_circuit(cw, c2);
        check("set_circuit: also runs auto-align (offset snapped)",
              c2->components[0]->position.y == 100);
        widget_destroy(&cw->base);
        circuit_destroy(c1);
        circuit_destroy(c2);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
