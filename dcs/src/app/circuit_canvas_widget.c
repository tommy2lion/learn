#include "circuit_canvas_widget.h"
#include "external_view.h"
#include "../framework/core/color.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#define GATE_W   80.0f
#define GATE_H   60.0f
#define IO_R     12.0f
#define PIN_R     4.0f
#define PIN_HIT  9.0f
#define WIRE_HIT 5
#define DOT_R    3.0f      /* junction-dot radius (supplement Phase 5) */
#define BUBBLE_R 5.0f      /* NOT-gate output bubble radius (Phase 11) */

#define X_OFFSET 100.0f
#define X_STEP   180.0f
#define Y_CENTER 280.0f
#define Y_STEP   120.0f

/* ── small helpers ────────────────────────────────────────────────── */

static void status(circuit_canvas_widget_t *cw, const char *fmt, ...) {
    if (!cw->on_status) return;
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    cw->on_status(buf, cw->status_user);
}

static vec2_t screen_to_world(const circuit_canvas_widget_t *cw, vec2_t s) {
    return (vec2_t){
        (s.x - cw->cam_offset.x) / cw->cam_zoom + cw->cam_target.x,
        (s.y - cw->cam_offset.y) / cw->cam_zoom + cw->cam_target.y,
    };
}

/* world_to_screen helper would go here when needed; omitted for now. */

/* ── auto-layout ──────────────────────────────────────────────────── */

/* Compute a reasonable initial placement when positions are zero. Same
   topological-depth idea as the prototype: inputs left, gates in columns
   by depth, outputs right. Only runs when ALL positions are still zero. */
static int find_wire_idx(const circuit_t *c, const char *name) {
    for (int i = 0; i < c->wire_count; i++)
        if (strcmp(c->wires[i].name, name) == 0) return i;
    return -1;
}

static int component_depth(const circuit_t *c, int comp_idx, int *depths) {
    if (depths[comp_idx] >= 0) return depths[comp_idx];
    component_t *comp = c->components[comp_idx];
    int max_d = 0;
    int n_in = component_pin_count_in(comp);
    for (int p = 0; p < n_in; p++) {
        const char *wname = comp->in_wires[p];
        /* find producer: another component or an external input */
        int producer = -1;
        for (int j = 0; j < c->component_count; j++) {
            if (strcmp(c->components[j]->name, wname) == 0) { producer = j; break; }
        }
        if (producer >= 0) {
            int d = component_depth(c, producer, depths);
            if (d + 1 > max_d) max_d = d + 1;
        }
        /* external inputs are depth 0 → contribute max_d>=1 (at least one level) */
        else if (find_wire_idx(c, wname) >= 0) {
            if (max_d < 1) max_d = 1;
        }
    }
    depths[comp_idx] = max_d;
    return max_d;
}

static void auto_layout(circuit_t *c) {
    /* skip if anything already has a non-zero position */
    for (int i = 0; i < c->component_count; i++)
        if (c->components[i]->position.x != 0 || c->components[i]->position.y != 0) return;
    for (int i = 0; i < c->input_count; i++)
        if (c->input_positions[i].x != 0 || c->input_positions[i].y != 0) return;
    for (int i = 0; i < c->output_count; i++)
        if (c->output_positions[i].x != 0 || c->output_positions[i].y != 0) return;

    int *depths = (int *)malloc(sizeof(int) * (c->component_count > 0 ? c->component_count : 1));
    if (!depths) return;       /* OOM — leave positions at zero; caller can press F to refit */
    for (int i = 0; i < c->component_count; i++) depths[i] = -1;

    int max_depth = 0;
    for (int i = 0; i < c->component_count; i++) {
        int d = component_depth(c, i, depths);
        if (d > max_depth) max_depth = d;
    }
    int output_col = max_depth + 1;
    int total_cols = output_col + 1;

    int *col_total = (int *)calloc(total_cols, sizeof(int));
    int *col_idx   = (int *)calloc(total_cols, sizeof(int));
    if (!col_total || !col_idx) {
        free(depths); free(col_total); free(col_idx);
        return;                /* OOM */
    }
    col_total[0] = c->input_count;
    for (int i = 0; i < c->component_count; i++) col_total[depths[i] >= 1 ? depths[i] : 1]++;
    col_total[output_col] += c->output_count;

    /* place inputs */
    for (int i = 0; i < c->input_count; i++) {
        int row = col_idx[0]++;
        c->input_positions[i] = (vec2_t){
            X_OFFSET + 0 * X_STEP,
            Y_CENTER + (row - (col_total[0] - 1) * 0.5f) * Y_STEP,
        };
    }

    /* place components */
    for (int i = 0; i < c->component_count; i++) {
        int d = depths[i] >= 1 ? depths[i] : 1;
        int row = col_idx[d]++;
        c->components[i]->position = (vec2_t){
            X_OFFSET + d * X_STEP,
            Y_CENTER + (row - (col_total[d] - 1) * 0.5f) * Y_STEP,
        };
    }

    /* place outputs */
    for (int i = 0; i < c->output_count; i++) {
        int row = col_idx[output_col]++;
        c->output_positions[i] = (vec2_t){
            X_OFFSET + output_col * X_STEP,
            Y_CENTER + (row - (col_total[output_col] - 1) * 0.5f) * Y_STEP,
        };
    }

    free(depths); free(col_total); free(col_idx);
}

/* ── per-node geometry ────────────────────────────────────────────── */

static vec2_t node_position(const circuit_t *c, node_ref_t r) {
    switch (r.kind) {
        case NODE_COMPONENT: return c->components[r.index]->position;
        case NODE_INPUT:     return c->input_positions[r.index];
        case NODE_OUTPUT:    return c->output_positions[r.index];
        default:             return (vec2_t){0, 0};
    }
}

static void set_node_position(circuit_t *c, node_ref_t r, vec2_t p) {
    switch (r.kind) {
        case NODE_COMPONENT: c->components[r.index]->position = p; break;
        case NODE_INPUT:     c->input_positions[r.index]      = p; break;
        case NODE_OUTPUT:    c->output_positions[r.index]     = p; break;
        default: break;
    }
}

static vec2_t node_output_pin(const circuit_t *c, node_ref_t r) {
    vec2_t p = node_position(c, r);
    if (r.kind == NODE_COMPONENT) return (vec2_t){p.x + GATE_W / 2, p.y};
    if (r.kind == NODE_INPUT)     return (vec2_t){p.x + IO_R,       p.y};
    return p;  /* outputs have no output pin */
}

static vec2_t node_input_pin(const circuit_t *c, node_ref_t r, int pin_idx) {
    vec2_t p = node_position(c, r);
    if (r.kind == NODE_COMPONENT) {
        component_t *comp = c->components[r.index];
        int n_in = component_pin_count_in(comp);
        vec2_t pp = {p.x - GATE_W / 2, p.y};
        if (n_in == 2) pp.y += (pin_idx == 0 ? -GATE_H / 4 : GATE_H / 4);
        return pp;
    }
    if (r.kind == NODE_OUTPUT) return (vec2_t){p.x - IO_R, p.y};
    return p;
}

/* Find the producer node (input or component) for a wire name. */
static node_ref_t producer_for_wire(const circuit_t *c, const char *wire_name) {
    for (int i = 0; i < c->input_count; i++)
        if (strcmp(c->input_names[i], wire_name) == 0)
            return (node_ref_t){NODE_INPUT, i};
    for (int i = 0; i < c->component_count; i++)
        if (strcmp(c->components[i]->name, wire_name) == 0)
            return (node_ref_t){NODE_COMPONENT, i};
    return NODE_REF_NONE;
}

/* Returns 1 iff `pt` exactly matches the position of any pin terminal
   (input pin, output pin, or any component input/output pin) in the
   current circuit. Used to keep wire-edit drag away from pin endpoints —
   those move only when their owning component moves. (Phase 12) */
static int point_matches_pin(const circuit_t *c, vec2_t pt) {
    if (!c) return 0;
    for (int i = 0; i < c->input_count; i++) {
        vec2_t p = node_output_pin(c, (node_ref_t){NODE_INPUT, i});
        if (p.x == pt.x && p.y == pt.y) return 1;
    }
    for (int i = 0; i < c->component_count; i++) {
        node_ref_t r = {NODE_COMPONENT, i};
        component_t *comp = c->components[i];
        int n_in = component_pin_count_in(comp);
        for (int p = 0; p < n_in; p++) {
            vec2_t pp = node_input_pin(c, r, p);
            if (pp.x == pt.x && pp.y == pt.y) return 1;
        }
        vec2_t op = node_output_pin(c, r);
        if (op.x == pt.x && op.y == pt.y) return 1;
    }
    for (int i = 0; i < c->output_count; i++) {
        vec2_t p = node_input_pin(c, (node_ref_t){NODE_OUTPUT, i}, 0);
        if (p.x == pt.x && p.y == pt.y) return 1;
    }
    return 0;
}

/* Returns the number of vertical segments in the net at column x.
   Used by seg_is_draggable to recognise shared V buses (>= 2 V segs
   at the same column → draggable as a unit). (R-8 + drag) */
static int v_bus_count_at(const wire_net_geom_t *net, float column_x) {
    int n = 0;
    for (int i = 0; i < net->seg_count; i++) {
        const wire_segment_t *t = &net->segs[i];
        if (t->a.x == t->b.x && t->a.x == column_x) n++;
    }
    return n;
}

/* Returns 1 iff the segment can be safely dragged. Two paths:
     - A "V bus" segment (vertical, shares its column with at least one
       other V segment in the same net) is draggable as a unit; the
       shift_v_bus operation moves the whole bus together. Pin-terminal
       check still applies — V bus segs shouldn't touch a pin.
     - Otherwise: degree-2 corner rule (each endpoint shared with exactly
       one other segment). Single V drops, middle V's in Z-routes, and
       middle H's all qualify. Higher degree means a branch / junction
       — shift_segment would refuse anyway. (Phase 12 + R-8 drag) */
static int seg_is_draggable(const circuit_canvas_widget_t *cw,
                            int net_idx, int seg_idx) {
    const wire_net_geom_t *net = wire_geometry_net(&cw->wires, net_idx);
    if (!net || seg_idx < 0 || seg_idx >= net->seg_count) return 0;
    const wire_segment_t *s = &net->segs[seg_idx];

    if (point_matches_pin(cw->circuit, s->a)) return 0;
    if (point_matches_pin(cw->circuit, s->b)) return 0;

    /* V bus check: vertical segment with siblings at the same column. */
    if (s->a.x == s->b.x && v_bus_count_at(net, s->a.x) >= 2) {
        return 1;
    }

    int deg_a = 0, deg_b = 0;
    for (int i = 0; i < net->seg_count; i++) {
        if (i == seg_idx) continue;
        const wire_segment_t *t = &net->segs[i];
        if ((t->a.x == s->a.x && t->a.y == s->a.y) ||
            (t->b.x == s->a.x && t->b.y == s->a.y)) deg_a++;
        if ((t->a.x == s->b.x && t->a.y == s->b.y) ||
            (t->b.x == s->b.x && t->b.y == s->b.y)) deg_b++;
    }
    return deg_a == 1 && deg_b == 1;
}

/* ── auto-alignment of components to trunks (R-7 refinement) ───────
 *
 * After auto_layout has placed components in topological columns, small
 * pixel-level offsets between a producer's output pin and its consumer's
 * input pin still produce ugly tiny V drops + H stubs in the routing.
 * This pass snaps a component's y so one of its input pins lines up
 * exactly with its producer's output pin, eliminating the dog-leg.
 *
 * Threshold: 8 px (= 1 grid step). Larger offsets are presumed
 * intentional and left alone.
 *
 * For multi-input gates: align to the pin whose producer has the
 * highest fan-out (more visual benefit), break ties by smaller |Δ|.
 * Only one pin per component is aligned — snapping a 2-input gate to
 * one input can worsen the other's offset, accepted trade-off.
 *
 * Cascade: components are processed in storage order (which is
 * topological by construction in circuit_add_component). So aligning
 * gate G to its already-aligned upstream gives G's downstream consumers
 * a fresh, already-snapped y to align with.
 *
 * Runs only on initial seed (file open / set_circuit), not on drag-end
 * reseats — otherwise the user's drag would snap-back. */

#define AUTO_ALIGN_THRESHOLD 8.0f       /* matches wire_geometry's ROUTING_GRID */

static int fanout_of_wire(const circuit_t *c, const char *wire_name) {
    int n = 0;
    for (int i = 0; i < c->component_count; i++) {
        component_t *comp = c->components[i];
        int n_in = component_pin_count_in(comp);
        for (int p = 0; p < n_in; p++) {
            if (strcmp(comp->in_wires[p], wire_name) == 0) n++;
        }
    }
    for (int i = 0; i < c->output_count; i++) {
        if (strcmp(c->output_names[i], wire_name) == 0) n++;
    }
    return n;
}

int circuit_canvas_widget_auto_align(circuit_t *c) {
    if (!c) return 0;
    int n_shifts = 0;

    /* Pass 1 — shift components so one input pin aligns with its producer. */
    for (int i = 0; i < c->component_count; i++) {
        component_t *comp = c->components[i];
        int n_in = component_pin_count_in(comp);

        int   best_pin       = -1;
        float best_delta     = 0;
        int   best_fanout    = -1;
        float best_abs_delta = AUTO_ALIGN_THRESHOLD + 1.0f;

        for (int p = 0; p < n_in; p++) {
            const char *wn = comp->in_wires[p];
            if (!wn[0]) continue;
            node_ref_t src = producer_for_wire(c, wn);
            if (src.kind == NODE_NONE) continue;
            float producer_y = node_output_pin(c, src).y;
            float consumer_y = node_input_pin(c, (node_ref_t){NODE_COMPONENT, i}, p).y;
            float delta = consumer_y - producer_y;
            float abs_d = delta >= 0 ? delta : -delta;
            if (abs_d == 0 || abs_d > AUTO_ALIGN_THRESHOLD) continue;
            int fo = fanout_of_wire(c, wn);
            /* Prefer higher fan-out; on tie, smaller |Δ|. */
            if (fo > best_fanout
                || (fo == best_fanout && abs_d < best_abs_delta)) {
                best_pin       = p;
                best_delta     = delta;
                best_fanout    = fo;
                best_abs_delta = abs_d;
            }
        }

        if (best_pin >= 0) {
            comp->position.y -= best_delta;
            n_shifts++;
        }
    }

    /* Pass 2 — same idea for external outputs (their producer is some
       component / input we may have just moved). */
    for (int i = 0; i < c->output_count; i++) {
        const char *wn = c->output_names[i];
        if (!wn[0]) continue;
        node_ref_t src = producer_for_wire(c, wn);
        if (src.kind == NODE_NONE) continue;
        float producer_y = node_output_pin(c, src).y;
        float consumer_y = c->output_positions[i].y;
        float delta = consumer_y - producer_y;
        float abs_d = delta >= 0 ? delta : -delta;
        if (abs_d == 0 || abs_d > AUTO_ALIGN_THRESHOLD) continue;
        c->output_positions[i].y -= delta;
        n_shifts++;
    }

    return n_shifts;
}

/* (Re)build wire geometry from the current circuit's connectivity. Routes
   every (producer-output-pin → consumer-input-pin) pair via auto_route_wire,
   so fan-out nets accumulate one route per consumer in the same wire_net_geom_t.
   Called from create() / set_circuit() and from the Phase-4 mutation paths.
   Releases any previous geometry first, so calling twice is fine. */
/* Collect every consumer pin of `wire_name` into out[]; returns the count
   (0 if the wire has no consumers). Caller sizes out[] generously
   (MAX_CCW_FANOUT is fine). (Phase 13 helper) */
#define MAX_CCW_FANOUT 32
static int collect_consumer_pins(const circuit_t *c, const char *wire_name,
                                  vec2_t *out, int max_out) {
    int n = 0;
    for (int i = 0; i < c->component_count && n < max_out; i++) {
        component_t *comp = c->components[i];
        int n_in = component_pin_count_in(comp);
        for (int p = 0; p < n_in && n < max_out; p++) {
            if (strcmp(comp->in_wires[p], wire_name) != 0) continue;
            out[n++] = node_input_pin(c, (node_ref_t){NODE_COMPONENT, i}, p);
        }
    }
    for (int i = 0; i < c->output_count && n < max_out; i++) {
        if (strcmp(c->output_names[i], wire_name) != 0) continue;
        out[n++] = node_input_pin(c, (node_ref_t){NODE_OUTPUT, i}, 0);
    }
    return n;
}

static void seed_geometry_from_circuit(circuit_canvas_widget_t *cw) {
    wire_geometry_release(&cw->wires);
    wire_geometry_init   (&cw->wires);
    circuit_t *c = cw->circuit;
    if (!c) return;

    /* Producer-wire iteration: each input contributes its name; each
       component contributes its output-wire name. For each, gather every
       consumer pin and emit one Steiner-routed net. (Phase 13) */
    for (int i = 0; i < c->input_count; i++) {
        const char *wn = c->input_names[i];
        vec2_t producer = node_output_pin(c, (node_ref_t){NODE_INPUT, i});
        vec2_t consumers[MAX_CCW_FANOUT];
        int n = collect_consumer_pins(c, wn, consumers, MAX_CCW_FANOUT);
        if (n > 0) auto_route_net(&cw->wires, wn, producer, consumers, n);
    }
    for (int i = 0; i < c->component_count; i++) {
        const char *wn = c->components[i]->name;
        vec2_t producer = node_output_pin(c, (node_ref_t){NODE_COMPONENT, i});
        vec2_t consumers[MAX_CCW_FANOUT];
        int n = collect_consumer_pins(c, wn, consumers, MAX_CCW_FANOUT);
        if (n > 0) auto_route_net(&cw->wires, wn, producer, consumers, n);
    }
}

/* Erase one net's geometry and re-route it from the current circuit state.
   Used by the targeted mutation hooks (connect / disconnect) — preserves
   unrelated nets' geometry. NULL or empty wire_name is a no-op.
   Routes Steiner-style via auto_route_net. (Phase 13) */
static void reseat_wire_geometry(circuit_canvas_widget_t *cw, const char *wire_name) {
    if (!wire_name || !wire_name[0]) return;
    wire_geometry_remove_net(&cw->wires, wire_name);
    circuit_t *c = cw->circuit;
    if (!c) return;

    node_ref_t src = producer_for_wire(c, wire_name);
    if (src.kind == NODE_NONE) return;       /* producer gone: leave the net erased */
    vec2_t producer = node_output_pin(c, src);

    vec2_t consumers[MAX_CCW_FANOUT];
    int n = collect_consumer_pins(c, wire_name, consumers, MAX_CCW_FANOUT);
    if (n > 0) auto_route_net(&cw->wires, wire_name, producer, consumers, n);
}

#ifndef NDEBUG
/* Debug-only invariant check: every wire-name currently referenced as a
   consumer (in_wires[p] or output_names[i]) should be present in geometry
   (since the seed / hooks should have routed it), and every geometry net
   should still have a producer in the circuit. Cheap at our scale. */
static void assert_geometry_consistent(const circuit_canvas_widget_t *cw) {
    const circuit_t *c = cw->circuit;
    if (!c) return;
    for (int i = 0; i < c->component_count; i++) {
        component_t *comp = c->components[i];
        int n_in = component_pin_count_in(comp);
        for (int p = 0; p < n_in; p++) {
            const char *wn = comp->in_wires[p];
            if (!wn[0]) continue;
            if (producer_for_wire(c, wn).kind == NODE_NONE) continue;
            if (wire_geometry_find(&cw->wires, wn) < 0) {
                fprintf(stderr, "geometry-consistency: consumer %s pin %d "
                        "refers to wire '%s' with no geometry entry\n",
                        comp->name, p, wn);
                abort();
            }
        }
    }
    for (int i = 0; i < cw->wires.net_count; i++) {
        const wire_net_geom_t *n = &cw->wires.nets[i];
        if (producer_for_wire(c, n->wire_name).kind == NODE_NONE) {
            fprintf(stderr, "geometry-consistency: net '%s' has no producer "
                    "in the circuit\n", n->wire_name);
            abort();
        }
    }
}
#else
#  define assert_geometry_consistent(cw) ((void)0)
#endif

/* ── hit testing ──────────────────────────────────────────────────── */

static node_ref_t hit_node(const circuit_canvas_widget_t *cw, vec2_t world) {
    /* iterate in reverse order so most-recent / topmost wins */
    for (int i = cw->circuit->output_count - 1; i >= 0; i--) {
        vec2_t p = cw->circuit->output_positions[i];
        float dx = world.x - p.x, dy = world.y - p.y;
        if (dx * dx + dy * dy <= IO_R * IO_R)
            return (node_ref_t){NODE_OUTPUT, i};
    }
    for (int i = cw->circuit->component_count - 1; i >= 0; i--) {
        vec2_t p = cw->circuit->components[i]->position;
        if (world.x >= p.x - GATE_W / 2 && world.x < p.x + GATE_W / 2 &&
            world.y >= p.y - GATE_H / 2 && world.y < p.y + GATE_H / 2)
            return (node_ref_t){NODE_COMPONENT, i};
    }
    for (int i = cw->circuit->input_count - 1; i >= 0; i--) {
        vec2_t p = cw->circuit->input_positions[i];
        float dx = world.x - p.x, dy = world.y - p.y;
        if (dx * dx + dy * dy <= IO_R * IO_R)
            return (node_ref_t){NODE_INPUT, i};
    }
    return NODE_REF_NONE;
}

static node_ref_t hit_output_pin(const circuit_canvas_widget_t *cw, vec2_t world) {
    /* an output pin exists on inputs (right side) and components (right side);
       outputs themselves have no output pin */
    for (int i = cw->circuit->component_count - 1; i >= 0; i--) {
        node_ref_t r = {NODE_COMPONENT, i};
        vec2_t p = node_output_pin(cw->circuit, r);
        float dx = world.x - p.x, dy = world.y - p.y;
        if (dx * dx + dy * dy <= PIN_HIT * PIN_HIT) return r;
    }
    for (int i = cw->circuit->input_count - 1; i >= 0; i--) {
        node_ref_t r = {NODE_INPUT, i};
        vec2_t p = node_output_pin(cw->circuit, r);
        float dx = world.x - p.x, dy = world.y - p.y;
        if (dx * dx + dy * dy <= PIN_HIT * PIN_HIT) return r;
    }
    return NODE_REF_NONE;
}

static node_ref_t hit_input_pin(const circuit_canvas_widget_t *cw, vec2_t world, int *pin_out) {
    for (int i = cw->circuit->component_count - 1; i >= 0; i--) {
        node_ref_t r = {NODE_COMPONENT, i};
        component_t *comp = cw->circuit->components[i];
        int n_in = component_pin_count_in(comp);
        for (int p = 0; p < n_in; p++) {
            vec2_t pp = node_input_pin(cw->circuit, r, p);
            float dx = world.x - pp.x, dy = world.y - pp.y;
            if (dx * dx + dy * dy <= PIN_HIT * PIN_HIT) { if (pin_out) *pin_out = p; return r; }
        }
    }
    for (int i = cw->circuit->output_count - 1; i >= 0; i--) {
        node_ref_t r = {NODE_OUTPUT, i};
        vec2_t pp = node_input_pin(cw->circuit, r, 0);
        float dx = world.x - pp.x, dy = world.y - pp.y;
        if (dx * dx + dy * dy <= PIN_HIT * PIN_HIT) { if (pin_out) *pin_out = 0; return r; }
    }
    return NODE_REF_NONE;
}

/* ── selection helpers ────────────────────────────────────────────── */

static int selection_contains(const circuit_canvas_widget_t *cw, node_ref_t r) {
    for (int i = 0; i < cw->selection_count; i++)
        if (node_ref_eq(cw->selection[i], r)) return 1;
    return 0;
}

static void selection_clear(circuit_canvas_widget_t *cw) { cw->selection_count = 0; }

static void selection_add(circuit_canvas_widget_t *cw, node_ref_t r) {
    if (cw->selection_count >= MAX_SELECTION) return;
    if (selection_contains(cw, r)) return;
    cw->selection[cw->selection_count++] = r;
}

static int selection_count(const circuit_canvas_widget_t *cw) { return cw->selection_count; }

/* Add nodes whose position is inside `rect` to the selection. */
static void selection_add_in_rect(circuit_canvas_widget_t *cw, rect_t rect) {
    for (int i = 0; i < cw->circuit->component_count; i++) {
        vec2_t p = cw->circuit->components[i]->position;
        if (p.x >= rect.x && p.x < rect.x + rect.w
         && p.y >= rect.y && p.y < rect.y + rect.h)
            selection_add(cw, (node_ref_t){NODE_COMPONENT, i});
    }
    for (int i = 0; i < cw->circuit->input_count; i++) {
        vec2_t p = cw->circuit->input_positions[i];
        if (p.x >= rect.x && p.x < rect.x + rect.w
         && p.y >= rect.y && p.y < rect.y + rect.h)
            selection_add(cw, (node_ref_t){NODE_INPUT, i});
    }
    for (int i = 0; i < cw->circuit->output_count; i++) {
        vec2_t p = cw->circuit->output_positions[i];
        if (p.x >= rect.x && p.x < rect.x + rect.w
         && p.y >= rect.y && p.y < rect.y + rect.h)
            selection_add(cw, (node_ref_t){NODE_OUTPUT, i});
    }
}

/* ── name generation + node mutators ──────────────────────────────── */

static int name_in_use(const circuit_t *c, const char *name) {
    for (int i = 0; i < c->wire_count; i++)
        if (strcmp(c->wires[i].name, name) == 0) return 1;
    for (int i = 0; i < c->output_count; i++)
        if (strcmp(c->output_names[i], name) == 0) return 1;
    return 0;
}

static void next_name(circuit_canvas_widget_t *cw, const char *prefix, int *counter, char *out, int max) {
    /* Bounded retry — if we somehow exhaust ~10000 candidates with the
       given prefix, fall back to a counter-based name and accept whatever
       collision the user sees. Practically this is never reached. */
    for (int tries = 0; tries < 10000; tries++) {
        snprintf(out, max, "%s%d", prefix, ++(*counter));
        if (!name_in_use(cw->circuit, out)) return;
    }
    snprintf(out, max, "%s_x%d", prefix, *counter);
}

static void place_at(circuit_canvas_widget_t *cw, vec2_t world) {
    char nm[DOMAIN_NAME_LEN];
    component_t *comp = NULL;
    switch (cw->place_kind) {
        case PLACE_AND: next_name(cw, "g", &cw->counter_gate, nm, sizeof(nm));
                        comp = gate_and_create(nm); break;
        case PLACE_OR:  next_name(cw, "g", &cw->counter_gate, nm, sizeof(nm));
                        comp = gate_or_create(nm);  break;
        case PLACE_NOT: next_name(cw, "g", &cw->counter_gate, nm, sizeof(nm));
                        comp = gate_not_create(nm); break;
        case PLACE_INPUT: {
            char in[DOMAIN_NAME_LEN];
            next_name(cw, "in", &cw->counter_in, in, sizeof(in));
            if (circuit_add_input(cw->circuit, in) == 0) {
                cw->circuit->input_positions[cw->circuit->input_count - 1] = world;
                status(cw, "Placed %s", in);
            }
            return;
        }
        case PLACE_OUTPUT: {
            char on[DOMAIN_NAME_LEN];
            next_name(cw, "out", &cw->counter_out, on, sizeof(on));
            if (circuit_add_output(cw->circuit, on) == 0) {
                cw->circuit->output_positions[cw->circuit->output_count - 1] = world;
                status(cw, "Placed %s", on);
            }
            return;
        }
        default: return;
    }
    if (!comp) return;
    /* For new components, leave in_wires empty; components added without input
       wires won't pass circuit_add_component's validation. So we use a back-door:
       directly add to the components array and create the output wire. */
    /* simpler path: don't go through circuit_add_component for new placements
       since they have no inputs yet; manually push */
    if (cw->circuit->component_count < cw->circuit->component_cap) {
        comp->position = world;
        /* We need the output wire to exist so other components can connect to it.
           Manually push the wire then the component. */
        if (cw->circuit->wire_count < cw->circuit->wire_cap) {
            wire_t *w = &cw->circuit->wires[cw->circuit->wire_count++];
            snprintf(w->name, DOMAIN_NAME_LEN, "%s", comp->name);
            w->value = SIG_UNDEF;
            cw->circuit->components[cw->circuit->component_count++] = comp;
            status(cw, "Placed %s", comp->name);
            return;
        }
    }
    component_destroy(comp);
}

/* Disconnect input pin `pin` of component `idx` (clear its in_wires entry).
   Reseats the affected net's geometry so the now-orphan consumer's segments
   disappear and any remaining consumers re-route cleanly. */
static void disconnect_input(circuit_canvas_widget_t *cw, int comp_idx, int pin) {
    component_t *c = cw->circuit->components[comp_idx];
    char prev[DOMAIN_NAME_LEN];
    snprintf(prev, sizeof(prev), "%s", c->in_wires[pin]);
    c->in_wires[pin][0] = '\0';
    if (prev[0]) reseat_wire_geometry(cw, prev);
    assert_geometry_consistent(cw);
}

/* Connect: set component[dst].in_wires[pin] to wire produced by `src`.
   For OUTPUT destinations, the output is renamed to the wire (smart-rename
   from existing Step-2 behaviour). Either way, reseat the wire's geometry. */
static void connect_wire(circuit_canvas_widget_t *cw, node_ref_t src, node_ref_t dst, int pin) {
    const char *wire_name;
    if (src.kind == NODE_INPUT)        wire_name = cw->circuit->input_names[src.index];
    else if (src.kind == NODE_COMPONENT) wire_name = cw->circuit->components[src.index]->name;
    else return;

    /* Snapshot wire_name before any mutation — it points into circuit state
       that is also the source of the upcoming snprintf, which is fine, but
       safer to capture a private copy for the reseat call below. */
    char wn[DOMAIN_NAME_LEN];
    snprintf(wn, sizeof(wn), "%s", wire_name);

    if (dst.kind == NODE_COMPONENT) {
        component_t *c = cw->circuit->components[dst.index];
        /* If this pin already had a different wire, reseat its previous net too
           so its (now-orphaned) consumer segments are removed. */
        char prev[DOMAIN_NAME_LEN];
        snprintf(prev, sizeof(prev), "%s", c->in_wires[pin]);
        snprintf(c->in_wires[pin], DOMAIN_NAME_LEN, "%s", wn);
        if (prev[0] && strcmp(prev, wn) != 0) reseat_wire_geometry(cw, prev);
        reseat_wire_geometry(cw, wn);
        status(cw, "Wired");
    } else if (dst.kind == NODE_OUTPUT) {
        /* Smart-rename: the OUTPUT's name should match the producer's wire so
           serialization round-trips. We rename the OUTPUT to the wire_name. */
        char prev[DOMAIN_NAME_LEN];
        snprintf(prev, sizeof(prev), "%s", cw->circuit->output_names[dst.index]);
        snprintf(cw->circuit->output_names[dst.index], DOMAIN_NAME_LEN, "%s", wn);
        if (prev[0] && strcmp(prev, wn) != 0) reseat_wire_geometry(cw, prev);
        reseat_wire_geometry(cw, wn);
        status(cw, "Wired");
    }
    assert_geometry_consistent(cw);
}

/* Find a wire entry whose dst is component[dst_idx], pin `pin`. (Editor wires
   are implicit — given by component's in_wires[pin] non-empty value pointing
   to some producer.) Returns the producer node and pin, or NONE. */
/* If `world` is near a drawn wire, return the consumer end as a node_ref:
   - NODE_COMPONENT with `*pin_out` set to the input-pin index that the wire feeds
   - NODE_OUTPUT for an external-output sink (pin_out is set to 0)
   Returns NODE_REF_NONE if no wire is near. */
static node_ref_t wire_at(const circuit_canvas_widget_t *cw, vec2_t world, int *pin_out) {
    if (pin_out) *pin_out = 0;
    /* component inputs */
    for (int i = 0; i < cw->circuit->component_count; i++) {
        component_t *c = cw->circuit->components[i];
        int n_in = component_pin_count_in(c);
        for (int p = 0; p < n_in; p++) {
            if (!c->in_wires[p][0]) continue;
            node_ref_t src = producer_for_wire(cw->circuit, c->in_wires[p]);
            if (src.kind == NODE_NONE) continue;
            vec2_t a = node_output_pin(cw->circuit, src);
            vec2_t b = node_input_pin (cw->circuit, (node_ref_t){NODE_COMPONENT, i}, p);
            /* point-near-segment test */
            float dx = b.x - a.x, dy = b.y - a.y;
            float ll = dx * dx + dy * dy;
            if (ll < 1) continue;
            float t = ((world.x - a.x) * dx + (world.y - a.y) * dy) / ll;
            if (t < 0 || t > 1) continue;
            float px = a.x + t * dx, py = a.y + t * dy;
            float ddx = world.x - px, ddy = world.y - py;
            if (ddx * ddx + ddy * ddy <= WIRE_HIT * WIRE_HIT) {
                if (pin_out) *pin_out = p;
                return (node_ref_t){NODE_COMPONENT, i};
            }
        }
    }
    /* outputs (their input pin is fed by some wire matching output_names[i]) */
    for (int i = 0; i < cw->circuit->output_count; i++) {
        const char *wn = cw->circuit->output_names[i];
        node_ref_t src = producer_for_wire(cw->circuit, wn);
        if (src.kind == NODE_NONE) continue;
        vec2_t a = node_output_pin(cw->circuit, src);
        vec2_t b = node_input_pin (cw->circuit, (node_ref_t){NODE_OUTPUT, i}, 0);
        float dx = b.x - a.x, dy = b.y - a.y;
        float ll = dx * dx + dy * dy;
        if (ll < 1) continue;
        float t = ((world.x - a.x) * dx + (world.y - a.y) * dy) / ll;
        if (t < 0 || t > 1) continue;
        float px = a.x + t * dx, py = a.y + t * dy;
        float ddx = world.x - px, ddy = world.y - py;
        if (ddx * ddx + ddy * ddy <= WIRE_HIT * WIRE_HIT) {
            return (node_ref_t){NODE_OUTPUT, i};
        }
    }
    return NODE_REF_NONE;
}

/* ── delete ───────────────────────────────────────────────────────── */

static void remove_component_at(circuit_canvas_widget_t *cw, int idx) {
    circuit_t *c = cw->circuit;
    component_t *comp = c->components[idx];
    /* find the wire produced by this component and remove it */
    int wi = -1;
    for (int i = 0; i < c->wire_count; i++)
        if (strcmp(c->wires[i].name, comp->name) == 0) { wi = i; break; }
    if (wi >= 0) {
        for (int i = wi; i < c->wire_count - 1; i++) c->wires[i] = c->wires[i + 1];
        c->wire_count--;
    }
    /* clear references in other components' in_wires */
    for (int i = 0; i < c->component_count; i++) {
        if (i == idx) continue;
        for (int p = 0; p < DOMAIN_MAX_PINS_IN; p++)
            if (strcmp(c->components[i]->in_wires[p], comp->name) == 0)
                c->components[i]->in_wires[p][0] = '\0';
    }
    /* clear output references */
    for (int i = 0; i < c->output_count; i++)
        if (strcmp(c->output_names[i], comp->name) == 0)
            c->output_names[i][0] = '\0';
    /* remove from components array */
    component_destroy(comp);
    for (int i = idx; i < c->component_count - 1; i++) c->components[i] = c->components[i + 1];
    c->component_count--;
    /* Many nets touched (output wire gone, every fan-out into this component
       lost a consumer). Full reseed is the simplest correct response. */
    seed_geometry_from_circuit(cw);
    assert_geometry_consistent(cw);
}

static void remove_input_at(circuit_canvas_widget_t *cw, int idx) {
    circuit_t *c = cw->circuit;
    /* clear references in components */
    const char *nm = c->input_names[idx];
    for (int i = 0; i < c->component_count; i++)
        for (int p = 0; p < DOMAIN_MAX_PINS_IN; p++)
            if (strcmp(c->components[i]->in_wires[p], nm) == 0)
                c->components[i]->in_wires[p][0] = '\0';
    for (int i = 0; i < c->output_count; i++)
        if (strcmp(c->output_names[i], nm) == 0)
            c->output_names[i][0] = '\0';
    /* remove the wire (named same as input) */
    for (int i = 0; i < c->wire_count; i++)
        if (strcmp(c->wires[i].name, nm) == 0) {
            for (int j = i; j < c->wire_count - 1; j++) c->wires[j] = c->wires[j + 1];
            c->wire_count--;
            break;
        }
    /* shift input_names + positions */
    for (int i = idx; i < c->input_count - 1; i++) {
        memcpy(c->input_names[i], c->input_names[i + 1], DOMAIN_NAME_LEN);
        c->input_positions[i] = c->input_positions[i + 1];
    }
    c->input_count--;
    seed_geometry_from_circuit(cw);
    assert_geometry_consistent(cw);
}

static void remove_output_at(circuit_canvas_widget_t *cw, int idx) {
    circuit_t *c = cw->circuit;
    for (int i = idx; i < c->output_count - 1; i++) {
        memcpy(c->output_names[i], c->output_names[i + 1], DOMAIN_NAME_LEN);
        c->output_positions[i] = c->output_positions[i + 1];
    }
    c->output_count--;
    /* The output was a consumer on some net; that net's geometry has stale
       segments going to the now-removed output position. Reseed handles it. */
    seed_geometry_from_circuit(cw);
    assert_geometry_consistent(cw);
}

/* node_ref indices shift after removals; for delete-many we sort in
   decreasing component / input / output index order before removing. */
static int cmp_node_desc(const void *a, const void *b) {
    const node_ref_t *na = (const node_ref_t *)a, *nb = (const node_ref_t *)b;
    if (na->kind != nb->kind) return (int)nb->kind - (int)na->kind;
    return nb->index - na->index;
}

static void remove_selection(circuit_canvas_widget_t *cw) {
    int n = cw->selection_count;
    if (n == 0) return;
    /* sort copy in (kind desc, index desc) order */
    node_ref_t buf[MAX_SELECTION];
    memcpy(buf, cw->selection, sizeof(node_ref_t) * n);
    qsort(buf, n, sizeof(node_ref_t), cmp_node_desc);
    for (int i = 0; i < n; i++) {
        switch (buf[i].kind) {
            case NODE_COMPONENT: remove_component_at(cw, buf[i].index); break;
            case NODE_INPUT:     remove_input_at    (cw, buf[i].index); break;
            case NODE_OUTPUT:    remove_output_at   (cw, buf[i].index); break;
            default: break;
        }
    }
    cw->selection_count = 0;
}

/* ── widget vtable: draw ──────────────────────────────────────────── */

static void draw_node(igraph_t *g, const circuit_t *c, node_ref_t r, int hovered, int selected) {
    vec2_t p = node_position(c, r);
    if (r.kind == NODE_INPUT) {
        g->draw_circle      (g->self, p, IO_R, COLOR_BLUE);
        g->draw_circle_lines(g->self, p, IO_R, 1.5f, COLOR_BLACK);
        const char *nm = c->input_names[r.index];
        float tw = g->measure_text(g->self, nm, 16);
        g->draw_text(g->self, nm, (vec2_t){p.x - tw / 2, p.y - 30}, 16, COLOR_BLACK);
        if (selected) g->draw_circle_lines(g->self, p, IO_R + 4, 3, COLOR_ORANGE);
        if (hovered)  g->draw_circle_lines(g->self, p, IO_R + 2, 1.5f, COLOR_BLUE);
    } else if (r.kind == NODE_OUTPUT) {
        g->draw_circle      (g->self, p, IO_R, COLOR_GREEN);
        g->draw_circle_lines(g->self, p, IO_R, 1.5f, COLOR_BLACK);
        const char *nm = c->output_names[r.index];
        float tw = g->measure_text(g->self, nm, 16);
        g->draw_text(g->self, nm, (vec2_t){p.x - tw / 2, p.y - 30}, 16, COLOR_BLACK);
        if (selected) g->draw_circle_lines(g->self, p, IO_R + 4, 3, COLOR_ORANGE);
        if (hovered)  g->draw_circle_lines(g->self, p, IO_R + 2, 1.5f, COLOR_BLUE);
    } else if (r.kind == NODE_COMPONENT) {
        component_t *comp = c->components[r.index];
        component_kind_t kind = component_kind(comp);
        rect_t box = { p.x - GATE_W / 2, p.y - GATE_H / 2, GATE_W, GATE_H };

        /* Body — IEC-style rectangle for AND / OR / chipsets; ANSI-style
           triangle with output bubble for NOT. (supplement Phase 11) */
        if (kind == COMP_NOT) {
            /* Triangle: apex on the right (just inside the bubble), base on
               the left edge. Outline-only (no fill primitive available);
               wires terminate at the right edge of the bubble = the existing
               node_output_pin position, so wire-routing is unaffected. */
            float left   = p.x - GATE_W / 2;
            float top    = p.y - GATE_H / 2;
            float bot    = p.y + GATE_H / 2;
            vec2_t apex  = { p.x + GATE_W / 2 - BUBBLE_R * 2.0f, p.y };
            vec2_t tl    = { left, top };
            vec2_t bl    = { left, bot };
            g->draw_line(g->self, tl, bl,    2.0f, COLOR_BLACK);
            g->draw_line(g->self, tl, apex,  2.0f, COLOR_BLACK);
            g->draw_line(g->self, bl, apex,  2.0f, COLOR_BLACK);
            /* Output bubble — white fill so it punches a clean hole through
               any wire that ends here; then the outline on top. */
            vec2_t bubble = { p.x + GATE_W / 2 - BUBBLE_R, p.y };
            g->draw_circle      (g->self, bubble, BUBBLE_R,         COLOR_WHITE);
            g->draw_circle_lines(g->self, bubble, BUBBLE_R, 1.5f,   COLOR_BLACK);
        } else {
            g->draw_rect      (g->self, box, COLOR_WHITE);
            g->draw_rect_lines(g->self, box, 2.0f, COLOR_BLACK);
            /* IEC-style glyph for AND ('&'). For OR we'd ideally use '≥1'
               but raylib's default font renders the ≥ (U+2265) as a tofu
               '?'; using the ASCII "OR" keeps the look consistent across
               platforms. Switch back to "\xe2\x89\xa5""1" when a font
               with the IEC glyph is in use. Unknown kinds fall back to
               the uppercased kind name (chipsets / future primitives). */
            const char *glyph;
            float       glyph_size;
            char        fallback[8] = {0};
            switch (kind) {
                case COMP_AND: glyph = "&";                    glyph_size = 22.0f; break;
                case COMP_OR:  glyph = "OR";                   glyph_size = 18.0f; break;
                default: {
                    const char *kn = component_kind_name(kind);
                    for (int i = 0; kn[i] && i < 7; i++)
                        fallback[i] = (char)toupper((unsigned char)kn[i]);
                    glyph = fallback;
                    glyph_size = 16.0f;
                } break;
            }
            float gw = g->measure_text(g->self, glyph, glyph_size);
            g->draw_text(g->self, glyph,
                         (vec2_t){p.x - gw / 2, p.y - glyph_size / 2},
                         glyph_size, COLOR_BLACK);
        }

        /* pins — NOT's output is visually represented by the bubble itself,
           so skip the duplicate small circle there. */
        int n_in = component_pin_count_in(comp);
        for (int j = 0; j < n_in; j++)
            g->draw_circle(g->self, node_input_pin(c, r, j), PIN_R, COLOR_BLACK);
        if (kind != COMP_NOT)
            g->draw_circle(g->self, node_output_pin(c, r), PIN_R, COLOR_BLACK);

        /* instance name above the box */
        float lw = g->measure_text(g->self, comp->name, 12);
        g->draw_text(g->self, comp->name, (vec2_t){p.x - lw / 2, p.y - GATE_H / 2 - 16}, 12, COLOR_GRAY);

        if (selected) {
            rect_t hr = { box.x - 4, box.y - 4, box.w + 8, box.h + 8 };
            g->draw_rect_lines(g->self, hr, 3, COLOR_ORANGE);
        }
        if (hovered) {
            rect_t hr = { box.x - 2, box.y - 2, box.w + 4, box.h + 4 };
            g->draw_rect_lines(g->self, hr, 1.5f, COLOR_BLUE);
        }
    }
}

static void draw_world(circuit_canvas_widget_t *cw, igraph_t *g) {
    circuit_t *c = cw->circuit;
    const char *hn = cw->highlighted_net;
    int has_hl = hn[0] != '\0';

    /* wires first (under nodes). Pass 1: routed segments from the geometry
       sidecar (orthogonal). Stale entries — those whose producer was removed
       from the circuit — are filtered so they don't render as ghosts. */
    for (int n = 0; n < cw->wires.net_count; n++) {
        const wire_net_geom_t *net = wire_geometry_net(&cw->wires, n);
        if (producer_for_wire(c, net->wire_name).kind == NODE_NONE) continue;
        int hi = has_hl && strcmp(net->wire_name, hn) == 0;
        uint32_t color = hi ? COLOR_ORANGE   : COLOR_DARKGRAY;
        float    thick = hi ? 3.5f           : 2.0f;
        for (int s = 0; s < net->seg_count; s++) {
            g->draw_line(g->self, net->segs[s].a, net->segs[s].b, thick, color);
        }
    }
    /* Pass 2: direct-line fallback for any consumer whose net has no geometry
       (e.g. a wire created in the GUI after seed time — Phase 4 will hook the
       mutation sites so this fallback becomes rare). */
    for (int i = 0; i < c->component_count; i++) {
        component_t *comp = c->components[i];
        int n_in = component_pin_count_in(comp);
        for (int p = 0; p < n_in; p++) {
            const char *wn = comp->in_wires[p];
            if (!wn[0]) continue;
            if (wire_geometry_find(&cw->wires, wn) >= 0) continue;
            node_ref_t src = producer_for_wire(c, wn);
            if (src.kind == NODE_NONE) continue;
            vec2_t a = node_output_pin(c, src);
            vec2_t b = node_input_pin (c, (node_ref_t){NODE_COMPONENT, i}, p);
            int hi = has_hl && strcmp(wn, hn) == 0;
            uint32_t color = hi ? COLOR_ORANGE   : COLOR_DARKGRAY;
            float    thick = hi ? 3.5f           : 2.0f;
            g->draw_line(g->self, a, b, thick, color);
        }
    }
    for (int i = 0; i < c->output_count; i++) {
        const char *wn = c->output_names[i];
        if (wire_geometry_find(&cw->wires, wn) >= 0) continue;
        node_ref_t src = producer_for_wire(c, wn);
        if (src.kind == NODE_NONE) continue;
        vec2_t a = node_output_pin(c, src);
        vec2_t b = node_input_pin (c, (node_ref_t){NODE_OUTPUT, i}, 0);
        int hi = has_hl && strcmp(wn, hn) == 0;
        uint32_t color = hi ? COLOR_ORANGE   : COLOR_DARKGRAY;
        float    thick = hi ? 3.5f           : 2.0f;
        g->draw_line(g->self, a, b, thick, color);
    }
    /* Pass 3: junction dots at fan-out / branch points (count >= 3 endpoints
       of the same net coinciding). Skipped for stale-producer nets. */
    for (int ni = 0; ni < cw->wires.net_count; ni++) {
        const wire_net_geom_t *net = wire_geometry_net(&cw->wires, ni);
        if (producer_for_wire(c, net->wire_name).kind == NODE_NONE) continue;
        int hi = has_hl && strcmp(net->wire_name, hn) == 0;
        uint32_t color = hi ? COLOR_ORANGE  : COLOR_BLACK;
        float    r     = hi ? DOT_R * 1.5f  : DOT_R;
        vec2_t dots[16];
        int n = wire_geometry_junctions(net, dots, 16);
        if (n > 16) n = 16;
        for (int j = 0; j < n; j++) {
            g->draw_circle(g->self, dots[j], r, color);
        }
    }

    /* nodes — inputs, components, outputs (so components draw on top of
       wire endpoints; selection highlights are part of draw_node) */
    for (int i = 0; i < c->input_count; i++) {
        node_ref_t r = {NODE_INPUT, i};
        int hov = node_ref_eq(r, cw->hover_node);
        int sel = selection_contains(cw, r);
        draw_node(g, c, r, hov, sel);
    }
    for (int i = 0; i < c->component_count; i++) {
        node_ref_t r = {NODE_COMPONENT, i};
        int hov = node_ref_eq(r, cw->hover_node);
        int sel = selection_contains(cw, r);
        draw_node(g, c, r, hov, sel);
    }
    for (int i = 0; i < c->output_count; i++) {
        node_ref_t r = {NODE_OUTPUT, i};
        int hov = node_ref_eq(r, cw->hover_node);
        int sel = selection_contains(cw, r);
        draw_node(g, c, r, hov, sel);
    }

    /* Pin-emphasis pass for the highlighted net: enlarged orange dots on
       every pin terminal that touches the net (producer's output pin +
       every consumer's input pin). Drawn after draw_node so it sits on
       top of the small black pin circles. */
    if (has_hl) {
        node_ref_t prod = producer_for_wire(c, hn);
        if (prod.kind != NODE_NONE) {
            g->draw_circle(g->self, node_output_pin(c, prod),
                           PIN_R * 1.5f, COLOR_ORANGE);
        }
        for (int i = 0; i < c->component_count; i++) {
            component_t *comp = c->components[i];
            int n_in = component_pin_count_in(comp);
            for (int p = 0; p < n_in; p++) {
                if (strcmp(comp->in_wires[p], hn) != 0) continue;
                g->draw_circle(g->self,
                               node_input_pin(c, (node_ref_t){NODE_COMPONENT, i}, p),
                               PIN_R * 1.5f, COLOR_ORANGE);
            }
        }
        for (int i = 0; i < c->output_count; i++) {
            if (strcmp(c->output_names[i], hn) != 0) continue;
            g->draw_circle(g->self,
                           node_input_pin(c, (node_ref_t){NODE_OUTPUT, i}, 0),
                           PIN_R * 1.5f, COLOR_ORANGE);
        }
    }

    /* wiring rubber-band (in world coords inside camera) */
    if (cw->mode == CMODE_WIRING && cw->wire_src.kind != NODE_NONE) {
        vec2_t a = node_output_pin(c, cw->wire_src);
        /* end at current mouse world */
        vec2_t mp = g->mouse_position(g->self);
        vec2_t mw = screen_to_world(cw, mp);
        g->draw_line(g->self, a, mw, 2.0f, COLOR_BLUE);
    }

    /* marquee rectangle (also world coords) */
    if (cw->mode == CMODE_MARQUEE) {
        vec2_t mp = g->mouse_position(g->self);
        vec2_t mw = screen_to_world(cw, mp);
        float x0 = (cw->marquee_start.x < mw.x) ? cw->marquee_start.x : mw.x;
        float y0 = (cw->marquee_start.y < mw.y) ? cw->marquee_start.y : mw.y;
        float x1 = (cw->marquee_start.x > mw.x) ? cw->marquee_start.x : mw.x;
        float y1 = (cw->marquee_start.y > mw.y) ? cw->marquee_start.y : mw.y;
        rect_t mr = {x0, y0, x1 - x0, y1 - y0};
        g->draw_rect      (g->self, mr, 0x5080B040u);     /* semi-transparent fill */
        g->draw_rect_lines(g->self, mr, 1.5f, 0x5080B0DC);
    }
}

static void ccw_draw(widget_t *self, igraph_t *g) {
    circuit_canvas_widget_t *cw = (circuit_canvas_widget_t *)self;

    /* Pan polling — middle drag pans regardless of cursor's current position
       (so a pan started over the canvas continues even if the cursor leaves). */
    int down = g->mouse_down(g->self, IM_MIDDLE);
    if (cw->panning) {
        if (!down) cw->panning = 0;
        else {
            vec2_t d = g->mouse_delta(g->self);
            cw->cam_target.x -= d.x / cw->cam_zoom;
            cw->cam_target.y -= d.y / cw->cam_zoom;
        }
    }

    /* background */
    g->draw_rect      (g->self, self->bounds, COLOR_BG);
    g->draw_rect_lines(g->self, self->bounds, 1.0f, COLOR_LIGHTGRAY);

    /* Apply hover cursor stashed by the event handler. Wire-edit mode keeps
       the resize cursor pinned through the drag. (Phase 12) */
    if (g->set_cursor) {
        cursor_kind_t cursor = cw->we_hover_cursor;
        if (cw->mode == CMODE_WIRE_EDIT && cw->we_seg_idx >= 0) {
            const wire_net_geom_t *net = wire_geometry_net(&cw->wires, cw->we_net_idx);
            if (net && cw->we_seg_idx < net->seg_count) {
                const wire_segment_t *s = &net->segs[cw->we_seg_idx];
                cursor = (s->a.y == s->b.y) ? CURSOR_NS_RESIZE : CURSOR_EW_RESIZE;
            }
        }
        g->set_cursor(g->self, cursor);
    }

    g->push_scissor(g->self, self->bounds);
    if (cw->display_mode == DISPLAY_EXTERNAL && cw->circuit) {
        /* Black-box view: no camera transform, draw centered in viewport.
           Dispatches through external_meta.render — Phase 9. */
        external_view_draw(g, cw->circuit, &cw->external_meta, self->bounds);
    } else {
        g->push_camera2d(g->self, cw->cam_target, cw->cam_offset, cw->cam_zoom);
        draw_world(cw, g);
        g->pop_camera2d(g->self);
    }
    g->pop_scissor(g->self);
}

/* ── widget vtable: handle_event ─────────────────────────────────── */

static int ccw_handle_event(widget_t *self, const event_t *ev) {
    circuit_canvas_widget_t *cw = (circuit_canvas_widget_t *)self;

    /* External (black-box) view: schematic-level interactions don't make
       sense without internal nodes / wires on screen. Consume mouse events
       (so they don't fall through to underlying widgets) but do nothing.
       Pan polling lives in ccw_draw and continues to work via mouse_down. */
    if (cw->display_mode == DISPLAY_EXTERNAL) {
        return (ev->kind == EV_MOUSE_PRESS  ||
                ev->kind == EV_MOUSE_RELEASE ||
                ev->kind == EV_MOUSE_MOVE   ||
                ev->kind == EV_MOUSE_WHEEL  ||
                ev->kind == EV_KEY_PRESS) ? 1 : 0;
    }

    vec2_t world = screen_to_world(cw, ev->mouse.pos);

    /* refresh hover on every mouse move; while dragging, move the dragged
       node (and any other selected nodes by the same delta). */
    if (ev->kind == EV_MOUSE_MOVE) {
        cw->hover_node = hit_node(cw, world);
        /* Wire-edit drag: shift the active segment perpendicular by the
           cursor delta this frame. V bus segments (R-8 shared bus) move
           as a unit via shift_v_bus; everything else uses shift_segment.
           (Phase 12 + R-8 drag) */
        if (cw->mode == CMODE_WIRE_EDIT) {
            const wire_net_geom_t *net = wire_geometry_net(&cw->wires, cw->we_net_idx);
            if (net && cw->we_seg_idx >= 0 && cw->we_seg_idx < net->seg_count) {
                const wire_segment_t *s = &net->segs[cw->we_seg_idx];
                int is_h     = (s->a.y == s->b.y);
                int is_v_bus = !is_h && v_bus_count_at(net, s->a.x) >= 2;
                int rc = 0;
                if (is_v_bus) {
                    float delta = world.x - cw->we_last_world.x;
                    if (delta != 0.0f) {
                        rc = wire_geometry_shift_v_bus(&cw->wires,
                                                       cw->we_net_idx,
                                                       s->a.x, delta);
                    }
                } else {
                    float delta = is_h ? (world.y - cw->we_last_world.y)
                                       : (world.x - cw->we_last_world.x);
                    if (delta != 0.0f) {
                        rc = wire_geometry_shift_segment(&cw->wires,
                                                          cw->we_net_idx,
                                                          cw->we_seg_idx, delta);
                    }
                }
                if (rc == 0) cw->we_last_world = world;
            }
            return 1;
        }
        if (cw->mode == CMODE_DRAGGING && cw->drag_node.kind != NODE_NONE) {
            vec2_t old_pos = node_position(cw->circuit, cw->drag_node);
            vec2_t new_pos = {world.x - cw->drag_offset.x, world.y - cw->drag_offset.y};
            vec2_t delta   = {new_pos.x - old_pos.x, new_pos.y - old_pos.y};
            set_node_position(cw->circuit, cw->drag_node, new_pos);
            if (selection_contains(cw, cw->drag_node)) {
                for (int i = 0; i < cw->selection_count; i++) {
                    if (node_ref_eq(cw->selection[i], cw->drag_node)) continue;
                    vec2_t p = node_position(cw->circuit, cw->selection[i]);
                    set_node_position(cw->circuit, cw->selection[i],
                                      (vec2_t){p.x + delta.x, p.y + delta.y});
                }
            }
        }
        /* Hover cursor: when in IDLE and hovering a draggable wire segment,
           stash an E-W cursor (vertical seg → drag horizontally) or N-S
           cursor (horizontal seg → drag vertically). ccw_draw applies it
           via g->set_cursor since the event handler doesn't have an
           igraph_t. (Phase 12) */
        if (cw->mode == CMODE_IDLE && cw->hover_node.kind == NODE_NONE) {
            cw->we_hover_cursor = CURSOR_DEFAULT;
            float tol = 4.0f / (cw->cam_zoom > 0.0f ? cw->cam_zoom : 1.0f);
            int hit_net = -1, hit_seg = -1;
            if (wire_geometry_pick_segment(&cw->wires, world, tol,
                                            &hit_net, &hit_seg)
                && seg_is_draggable(cw, hit_net, hit_seg)) {
                const wire_segment_t *s = &cw->wires.nets[hit_net].segs[hit_seg];
                cw->we_hover_cursor = (s->a.y == s->b.y)
                                    ? CURSOR_NS_RESIZE : CURSOR_EW_RESIZE;
            }
        }
        return 1;
    }

    /* zoom toward cursor */
    if (ev->kind == EV_MOUSE_WHEEL) {
        vec2_t mp  = ev->wheel.pos;
        vec2_t mw  = screen_to_world(cw, mp);
        cw->cam_offset = mp;
        cw->cam_target = mw;
        cw->cam_zoom += ev->wheel.dy * 0.1f * cw->cam_zoom;
        if (cw->cam_zoom < 0.1f) cw->cam_zoom = 0.1f;
        if (cw->cam_zoom > 5.0f) cw->cam_zoom = 5.0f;
        return 1;
    }

    /* middle press → start pan */
    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_MIDDLE) {
        cw->panning = 1;
        return 1;
    }

    /* keys: ESC cancels current mode + clears selection. (Del delete-selection
       and Ctrl+A select-all are handled globally by dcs_app's
       poll_global_shortcuts because the framework's focused-widget key-event
       dispatch is currently unused — see R-12 / R-13.) */
    if (ev->kind == EV_KEY_PRESS) {
        if (ev->key.key == IK_ESCAPE) {
            cw->mode = CMODE_IDLE;
            cw->place_kind = PLACE_NONE;
            cw->wire_src = NODE_REF_NONE;
            cw->drag_node = NODE_REF_NONE;
            selection_clear(cw);
            return 1;
        }
    }

    /* Mode-specific behavior. */
    if (cw->mode == CMODE_PLACING) {
        if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_RIGHT) {
            cw->mode = CMODE_IDLE; cw->place_kind = PLACE_NONE;
            status(cw, "Cancelled");
            return 1;
        }
        if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_LEFT) {
            place_at(cw, world);
            cw->mode = CMODE_IDLE; cw->place_kind = PLACE_NONE;
            return 1;
        }
        return 1;
    }

    if (cw->mode == CMODE_WIRING) {
        if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_RIGHT) {
            cw->mode = CMODE_IDLE; cw->wire_src = NODE_REF_NONE;
            return 1;
        }
        if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_LEFT) {
            int pin = 0;
            node_ref_t dst = hit_input_pin(cw, world, &pin);
            if (dst.kind != NODE_NONE && !node_ref_eq(dst, cw->wire_src)) {
                connect_wire(cw, cw->wire_src, dst, pin);
                cw->mode = CMODE_IDLE; cw->wire_src = NODE_REF_NONE;
            }
            return 1;
        }
        return 1;
    }

    if (cw->mode == CMODE_DRAGGING) {
        if (ev->kind == EV_MOUSE_RELEASE && ev->mouse.btn == IM_LEFT) {
            cw->mode = CMODE_IDLE; cw->drag_node = NODE_REF_NONE;
            /* Multi-select drag can touch many wires at once; full reseed is
               cheaper than tracking the touched set. */
            seed_geometry_from_circuit(cw);
            assert_geometry_consistent(cw);
            return 1;
        }
        return 1;
    }

    if (cw->mode == CMODE_WIRE_EDIT) {
        /* MOUSE_MOVE for wire-edit drag is handled in the early MOVE block
           above (which has access to `world` and returns). Press/release
           and cancel keys are handled here. */
        if (ev->kind == EV_MOUSE_RELEASE && ev->mouse.btn == IM_LEFT) {
            cw->mode       = CMODE_IDLE;
            cw->we_net_idx = -1;
            cw->we_seg_idx = -1;
            assert_geometry_consistent(cw);
            return 1;
        }
        /* ESC / right-click cancels — original geometry isn't restored
           since incremental shifts have already mutated; the user can
           drag back to a similar shape. */
        if (ev->kind == EV_KEY_PRESS && ev->key.key == IK_ESCAPE) {
            cw->mode = CMODE_IDLE;
            cw->we_net_idx = -1;
            cw->we_seg_idx = -1;
            return 1;
        }
        if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_RIGHT) {
            cw->mode = CMODE_IDLE;
            cw->we_net_idx = -1;
            cw->we_seg_idx = -1;
            return 1;
        }
        return 1;
    }

    if (cw->mode == CMODE_MARQUEE) {
        if (ev->kind == EV_MOUSE_RELEASE && ev->mouse.btn == IM_LEFT) {
            float x0 = (cw->marquee_start.x < world.x) ? cw->marquee_start.x : world.x;
            float y0 = (cw->marquee_start.y < world.y) ? cw->marquee_start.y : world.y;
            float x1 = (cw->marquee_start.x > world.x) ? cw->marquee_start.x : world.x;
            float y1 = (cw->marquee_start.y > world.y) ? cw->marquee_start.y : world.y;
            rect_t mr = {x0, y0, x1 - x0, y1 - y0};
            selection_add_in_rect(cw, mr);
            int n = selection_count(cw);
            if (n > 0) status(cw, "Selected %d node%s", n, n == 1 ? "" : "s");
            cw->mode = CMODE_IDLE;
            return 1;
        }
        return 1;
    }

    /* IDLE */
    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_RIGHT) {
        node_ref_t hit = hit_node(cw, world);
        if (hit.kind != NODE_NONE) {
            if (selection_contains(cw, hit)) {
                int n = selection_count(cw);
                remove_selection(cw);
                status(cw, "Deleted %d node%s", n, n == 1 ? "" : "s");
            } else {
                /* delete just this one */
                switch (hit.kind) {
                    case NODE_COMPONENT: remove_component_at(cw, hit.index); break;
                    case NODE_INPUT:     remove_input_at    (cw, hit.index); break;
                    case NODE_OUTPUT:    remove_output_at   (cw, hit.index); break;
                    default: break;
                }
                status(cw, "Node deleted");
            }
            cw->hover_node = NODE_REF_NONE;
            return 1;
        }
        int wire_pin = 0;
        node_ref_t w = wire_at(cw, world, &wire_pin);
        if (w.kind != NODE_NONE) {
            if (w.kind == NODE_COMPONENT) {
                disconnect_input(cw, w.index, wire_pin);
            } else if (w.kind == NODE_OUTPUT) {
                cw->circuit->output_names[w.index][0] = '\0';
            }
            status(cw, "Wire deleted");
            return 1;
        }
        return 1;
    }

    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_LEFT) {
        /* Any new left-click in IDLE drops the previous wire highlight; it's
           re-applied below only if the click lands on a wire segment. */
        circuit_canvas_widget_set_highlight(cw, NULL);

        /* output pin → start wiring */
        node_ref_t opin = hit_output_pin(cw, world);
        if (opin.kind != NODE_NONE) {
            cw->mode = CMODE_WIRING;
            cw->wire_src = opin;
            status(cw, "Click an input pin to wire (ESC/right-click cancels)");
            return 1;
        }
        /* input pin → disconnect existing wire */
        int pin = 0;
        node_ref_t ipin = hit_input_pin(cw, world, &pin);
        if (ipin.kind == NODE_COMPONENT) {
            disconnect_input(cw, ipin.index, pin);
            status(cw, "Disconnected");
            return 1;
        }
        if (ipin.kind == NODE_OUTPUT) {
            cw->circuit->output_names[ipin.index][0] = '\0';
            status(cw, "Disconnected");
            return 1;
        }
        /* node body → drag */
        node_ref_t hit = hit_node(cw, world);
        if (hit.kind != NODE_NONE) {
            if (!selection_contains(cw, hit)) { selection_clear(cw); selection_add(cw, hit); }
            cw->mode = CMODE_DRAGGING;
            cw->drag_node = hit;
            vec2_t p = node_position(cw->circuit, hit);
            cw->drag_offset = (vec2_t){world.x - p.x, world.y - p.y};
            return 1;
        }
        /* wire segment hit: either start a bend-edit drag (Phase 12) or
           just highlight the net (Phase 6) — depends on draggability. */
        {
            float tol = 4.0f / (cw->cam_zoom > 0.0f ? cw->cam_zoom : 1.0f);
            int hit_net = -1, hit_seg = -1;
            if (wire_geometry_pick_segment(&cw->wires, world, tol,
                                            &hit_net, &hit_seg)) {
                if (seg_is_draggable(cw, hit_net, hit_seg)) {
                    cw->mode         = CMODE_WIRE_EDIT;
                    cw->we_net_idx   = hit_net;
                    cw->we_seg_idx   = hit_seg;
                    cw->we_last_world = world;
                    return 1;
                }
                /* Not draggable — fall back to highlight behaviour. */
                const wire_net_geom_t *net = wire_geometry_net(&cw->wires, hit_net);
                if (net) {
                    circuit_canvas_widget_set_highlight(cw, net->wire_name);
                    status(cw, "Selected net %s", net->wire_name);
                }
                return 1;
            }
        }
        /* empty canvas → start marquee */
        selection_clear(cw);
        cw->mode = CMODE_MARQUEE;
        cw->marquee_start = world;
        return 1;
    }

    return 1;  /* consume any other event so it doesn't fall through */
}

static void ccw_destroy(widget_t *self) {
    circuit_canvas_widget_t *cw = (circuit_canvas_widget_t *)self;
    wire_geometry_release(&cw->wires);
    free(cw);
}

static const widget_vt_t CCW_VT = {
    .draw         = ccw_draw,
    .handle_event = ccw_handle_event,
    .destroy      = ccw_destroy,
};

/* ── public API ──────────────────────────────────────────────────── */

circuit_canvas_widget_t *circuit_canvas_widget_create(rect_t bounds, circuit_t *c) {
    circuit_canvas_widget_t *cw = (circuit_canvas_widget_t *)calloc(1, sizeof(*cw));
    if (!cw) return NULL;
    cw->base.vt      = &CCW_VT;
    cw->base.bounds  = bounds;
    cw->base.visible = 1;
    cw->circuit      = c;
    cw->cam_target   = (vec2_t){0, 0};
    cw->cam_offset   = (vec2_t){bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f};
    cw->cam_zoom     = 1.0f;
    cw->mode         = CMODE_IDLE;
    cw->place_kind   = PLACE_NONE;
    cw->wire_src     = NODE_REF_NONE;
    cw->drag_node    = NODE_REF_NONE;
    cw->hover_node   = NODE_REF_NONE;
    cw->gate_w       = GATE_W;
    cw->gate_h       = GATE_H;
    cw->io_r         = IO_R;
    cw->we_net_idx   = -1;
    cw->we_seg_idx   = -1;
    cw->we_hover_cursor = CURSOR_DEFAULT;
    wire_geometry_init(&cw->wires);
    if (c) {
        auto_layout(c);
        circuit_canvas_widget_auto_align(c);
        seed_geometry_from_circuit(cw);
        circuit_canvas_widget_fit_view(cw);
        cw->counter_in   = c->input_count;
        cw->counter_out  = c->output_count;
        cw->counter_gate = c->component_count;
    }
    return cw;
}

void circuit_canvas_widget_set_circuit(circuit_canvas_widget_t *self, circuit_t *c) {
    self->circuit = c;
    circuit_canvas_widget_reset(self);
    if (c) {
        auto_layout(c);
        circuit_canvas_widget_auto_align(c);
        seed_geometry_from_circuit(self);
        circuit_canvas_widget_fit_view(self);
        self->counter_in   = c->input_count;
        self->counter_out  = c->output_count;
        self->counter_gate = c->component_count;
    } else {
        seed_geometry_from_circuit(self);   /* clears geometry when circuit goes NULL */
    }
}

void circuit_canvas_widget_set_status_cb(circuit_canvas_widget_t *self,
                                         ccw_status_fn_t cb, void *user) {
    self->on_status = cb;
    self->status_user = user;
}

void circuit_canvas_widget_arm_place(circuit_canvas_widget_t *self, place_kind_t kind) {
    self->mode = CMODE_PLACING;
    self->place_kind = kind;
}

void circuit_canvas_widget_reseat_wires(circuit_canvas_widget_t *self) {
    seed_geometry_from_circuit(self);
    assert_geometry_consistent(self);
}

void circuit_canvas_widget_select_all(circuit_canvas_widget_t *self) {
    if (!self || !self->circuit) return;
    selection_clear(self);
    for (int i = 0; i < self->circuit->component_count; i++)
        selection_add(self, (node_ref_t){NODE_COMPONENT, i});
    for (int i = 0; i < self->circuit->input_count; i++)
        selection_add(self, (node_ref_t){NODE_INPUT, i});
    for (int i = 0; i < self->circuit->output_count; i++)
        selection_add(self, (node_ref_t){NODE_OUTPUT, i});
    status(self, "Selected %d node%s",
           self->selection_count, self->selection_count == 1 ? "" : "s");
}

void circuit_canvas_widget_delete_selection(circuit_canvas_widget_t *self) {
    if (!self || !self->circuit) return;
    int n = selection_count(self);
    if (n <= 0) return;
    remove_selection(self);
    status(self, "Deleted %d node%s", n, n == 1 ? "" : "s");
}

void circuit_canvas_widget_set_highlight(circuit_canvas_widget_t *self,
                                         const char *wire_name) {
    if (!wire_name || !wire_name[0]) {
        self->highlighted_net[0] = '\0';
        return;
    }
    snprintf(self->highlighted_net, DOMAIN_NAME_LEN, "%s", wire_name);
}

const wire_geometry_t *circuit_canvas_widget_geometry(const circuit_canvas_widget_t *self) {
    return &self->wires;
}

void circuit_canvas_widget_load_geometry(circuit_canvas_widget_t *self,
                                         wire_geometry_t *src) {
    wire_geometry_move(&self->wires, src);
    assert_geometry_consistent(self);
}

display_mode_t circuit_canvas_widget_display_mode(const circuit_canvas_widget_t *self) {
    return self->display_mode;
}

void circuit_canvas_widget_set_display_mode(circuit_canvas_widget_t *self,
                                            display_mode_t mode) {
    self->display_mode = mode;
}

void circuit_canvas_widget_set_display_name(circuit_canvas_widget_t *self,
                                            const char *name) {
    if (!name || !name[0]) {
        self->external_meta.display_name[0] = '\0';
        return;
    }
    snprintf(self->external_meta.display_name, DOMAIN_NAME_LEN, "%s", name);
}

external_view_metadata_t *circuit_canvas_widget_external_meta(circuit_canvas_widget_t *self) {
    return &self->external_meta;
}

void circuit_canvas_widget_reset(circuit_canvas_widget_t *self) {
    self->mode = CMODE_IDLE;
    self->place_kind = PLACE_NONE;
    self->wire_src = NODE_REF_NONE;
    self->drag_node = NODE_REF_NONE;
    self->hover_node = NODE_REF_NONE;
    self->selection_count = 0;
    self->panning = 0;
    self->counter_in = 0;
    self->counter_out = 0;
    self->counter_gate = 0;
    self->highlighted_net[0] = '\0';
    self->display_mode = DISPLAY_INTERNAL;
    external_view_metadata_init(&self->external_meta);
    self->we_net_idx      = -1;
    self->we_seg_idx      = -1;
    self->we_last_world   = (vec2_t){0, 0};
    self->we_hover_cursor = CURSOR_DEFAULT;
}

void circuit_canvas_widget_zoom_in(circuit_canvas_widget_t *self) {
    self->cam_zoom *= 1.25f;
    if (self->cam_zoom > 5.0f) self->cam_zoom = 5.0f;
}

void circuit_canvas_widget_zoom_out(circuit_canvas_widget_t *self) {
    self->cam_zoom *= 0.8f;
    if (self->cam_zoom < 0.1f) self->cam_zoom = 0.1f;
}

void circuit_canvas_widget_fit_view(circuit_canvas_widget_t *self) {
    rect_t b = self->base.bounds;
    self->cam_offset = (vec2_t){b.x + b.w * 0.5f, b.y + b.h * 0.5f};
    self->cam_zoom   = 1.0f;
    if (!self->circuit) { self->cam_target = (vec2_t){0, 0}; return; }
    int any = 0;
    float min_x = 0, max_x = 0, min_y = 0, max_y = 0;

#define ACCUM(P) do {                                  \
        vec2_t _p = (P);                               \
        if (!any) { min_x = max_x = _p.x;              \
                    min_y = max_y = _p.y; any = 1; }   \
        else {                                         \
            if (_p.x < min_x) min_x = _p.x;            \
            if (_p.x > max_x) max_x = _p.x;            \
            if (_p.y < min_y) min_y = _p.y;            \
            if (_p.y > max_y) max_y = _p.y;            \
        }                                              \
    } while (0)

    for (int i = 0; i < self->circuit->component_count; i++)
        ACCUM(self->circuit->components[i]->position);
    for (int i = 0; i < self->circuit->input_count; i++)
        ACCUM(self->circuit->input_positions[i]);
    for (int i = 0; i < self->circuit->output_count; i++)
        ACCUM(self->circuit->output_positions[i]);

#undef ACCUM
    if (!any) { self->cam_target = (vec2_t){0, 0}; return; }
    self->cam_target = (vec2_t){(min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f};
}
