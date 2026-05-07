#include "circuit_canvas_widget.h"
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
    for (;;) {
        snprintf(out, max, "%s%d", prefix, ++(*counter));
        if (!name_in_use(cw->circuit, out)) return;
    }
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

/* Disconnect input pin `pin` of component `idx` (clear its in_wires entry). */
static void disconnect_input(circuit_canvas_widget_t *cw, int comp_idx, int pin) {
    component_t *c = cw->circuit->components[comp_idx];
    c->in_wires[pin][0] = '\0';
}

/* Connect: set component[dst].in_wires[pin] to wire produced by `src`. */
static void connect_wire(circuit_canvas_widget_t *cw, node_ref_t src, node_ref_t dst, int pin) {
    const char *wire_name;
    if (src.kind == NODE_INPUT)        wire_name = cw->circuit->input_names[src.index];
    else if (src.kind == NODE_COMPONENT) wire_name = cw->circuit->components[src.index]->name;
    else return;

    if (dst.kind == NODE_COMPONENT) {
        component_t *c = cw->circuit->components[dst.index];
        snprintf(c->in_wires[pin], DOMAIN_NAME_LEN, "%s", wire_name);
        status(cw, "Wired");
    } else if (dst.kind == NODE_OUTPUT) {
        /* Smart-rename: the OUTPUT's name should match the producer's wire so
           serialization round-trips. We rename the OUTPUT to the wire_name. */
        snprintf(cw->circuit->output_names[dst.index], DOMAIN_NAME_LEN, "%s", wire_name);
        status(cw, "Wired");
    }
}

/* Find a wire entry whose dst is component[dst_idx], pin `pin`. (Editor wires
   are implicit — given by component's in_wires[pin] non-empty value pointing
   to some producer.) Returns the producer node and pin, or NONE. */
static node_ref_t wire_at(const circuit_canvas_widget_t *cw, vec2_t world) {
    /* iterate every connection (component input pins + output destinations) and
       test the line from producer's out pin to consumer's in pin */
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
                /* return the consumer end so deletion clears the right pin */
                return (node_ref_t){NODE_COMPONENT, (i << 8) | (p & 0xFF)};
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
}

static void remove_output_at(circuit_canvas_widget_t *cw, int idx) {
    circuit_t *c = cw->circuit;
    for (int i = idx; i < c->output_count - 1; i++) {
        memcpy(c->output_names[i], c->output_names[i + 1], DOMAIN_NAME_LEN);
        c->output_positions[i] = c->output_positions[i + 1];
    }
    c->output_count--;
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
        rect_t box = { p.x - GATE_W / 2, p.y - GATE_H / 2, GATE_W, GATE_H };
        g->draw_rect      (g->self, box, COLOR_WHITE);
        g->draw_rect_lines(g->self, box, 2.0f, COLOR_BLACK);

        /* gate name in upper-case */
        const char *kn = component_kind_name(component_kind(comp));
        char up[8] = {0};
        for (int i = 0; kn[i] && i < 7; i++) up[i] = (char)toupper((unsigned char)kn[i]);
        float tw = g->measure_text(g->self, up, 18);
        g->draw_text(g->self, up, (vec2_t){p.x - tw / 2, p.y - 9}, 18, COLOR_BLACK);

        /* pins */
        int n_in = component_pin_count_in(comp);
        for (int j = 0; j < n_in; j++)
            g->draw_circle(g->self, node_input_pin(c, r, j), PIN_R, COLOR_BLACK);
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

    /* wires first (under nodes) */
    for (int i = 0; i < c->component_count; i++) {
        component_t *comp = c->components[i];
        int n_in = component_pin_count_in(comp);
        for (int p = 0; p < n_in; p++) {
            if (!comp->in_wires[p][0]) continue;
            node_ref_t src = producer_for_wire(c, comp->in_wires[p]);
            if (src.kind == NODE_NONE) continue;
            vec2_t a = node_output_pin(c, src);
            vec2_t b = node_input_pin (c, (node_ref_t){NODE_COMPONENT, i}, p);
            g->draw_line(g->self, a, b, 2.0f, COLOR_DARKGRAY);
        }
    }
    for (int i = 0; i < c->output_count; i++) {
        const char *wn = c->output_names[i];
        node_ref_t src = producer_for_wire(c, wn);
        if (src.kind == NODE_NONE) continue;
        vec2_t a = node_output_pin(c, src);
        vec2_t b = node_input_pin (c, (node_ref_t){NODE_OUTPUT, i}, 0);
        g->draw_line(g->self, a, b, 2.0f, COLOR_DARKGRAY);
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

    g->push_scissor(g->self, self->bounds);
    g->push_camera2d(g->self, cw->cam_target, cw->cam_offset, cw->cam_zoom);
    draw_world(cw, g);
    g->pop_camera2d(g->self);
    g->pop_scissor(g->self);
}

/* ── widget vtable: handle_event ─────────────────────────────────── */

static int ccw_handle_event(widget_t *self, const event_t *ev) {
    circuit_canvas_widget_t *cw = (circuit_canvas_widget_t *)self;
    vec2_t world = screen_to_world(cw, ev->mouse.pos);

    /* refresh hover on every mouse move; while dragging, move the dragged
       node (and any other selected nodes by the same delta). */
    if (ev->kind == EV_MOUSE_MOVE) {
        cw->hover_node = hit_node(cw, world);
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

    /* keys: ESC cancels current mode + clears selection; Delete removes selection;
       Ctrl+A selects all */
    if (ev->kind == EV_KEY_PRESS) {
        if (ev->key.key == IK_ESCAPE) {
            cw->mode = CMODE_IDLE;
            cw->place_kind = PLACE_NONE;
            cw->wire_src = NODE_REF_NONE;
            cw->drag_node = NODE_REF_NONE;
            selection_clear(cw);
            return 1;
        }
        if (ev->key.key == IK_DELETE && selection_count(cw) > 0) {
            int n = selection_count(cw);
            remove_selection(cw);
            status(cw, "Deleted %d node%s", n, n == 1 ? "" : "s");
            return 1;
        }
        if (ev->key.key == IK_A && (ev->key.mods & MOD_CTRL)) {
            selection_clear(cw);
            for (int i = 0; i < cw->circuit->component_count; i++)
                selection_add(cw, (node_ref_t){NODE_COMPONENT, i});
            for (int i = 0; i < cw->circuit->input_count; i++)
                selection_add(cw, (node_ref_t){NODE_INPUT, i});
            for (int i = 0; i < cw->circuit->output_count; i++)
                selection_add(cw, (node_ref_t){NODE_OUTPUT, i});
            status(cw, "Selected %d node%s", cw->selection_count, cw->selection_count == 1 ? "" : "s");
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
        node_ref_t w = wire_at(cw, world);
        if (w.kind != NODE_NONE) {
            /* the hit's index encodes (comp_idx<<8) | pin for component dest, or output index for output dest */
            if (w.kind == NODE_COMPONENT) {
                int comp_idx = w.index >> 8;
                int pin      = w.index & 0xFF;
                disconnect_input(cw, comp_idx, pin);
            } else if (w.kind == NODE_OUTPUT) {
                cw->circuit->output_names[w.index][0] = '\0';
            }
            status(cw, "Wire deleted");
            return 1;
        }
        return 1;
    }

    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_LEFT) {
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
        /* empty canvas → start marquee */
        selection_clear(cw);
        cw->mode = CMODE_MARQUEE;
        cw->marquee_start = world;
        return 1;
    }

    return 1;  /* consume any other event so it doesn't fall through */
}

static void ccw_destroy(widget_t *self) { free(self); }

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
    if (c) {
        auto_layout(c);
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
        circuit_canvas_widget_fit_view(self);
        self->counter_in   = c->input_count;
        self->counter_out  = c->output_count;
        self->counter_gate = c->component_count;
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
