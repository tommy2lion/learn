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

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
