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
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
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

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
