#include "canvas.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define GATE_W   80.0f
#define GATE_H   60.0f
#define X_OFFSET 100.0f
#define X_STEP   180.0f
#define Y_CENTER 380.0f
#define Y_STEP   120.0f
#define IO_R     12.0f         /* radius of input/output circles */
#define PIN_HIT_R 9.0f         /* hit-test radius around a pin */
#define WIRE_HIT_R 5
#define INIT_CAP 16

/* ── lookup helpers ───────────────────────────────────────────── */

static int find_wire_idx(const Circuit *c, const char *name) {
    for (int i = 0; i < c->wire_count; i++)
        if (strcmp(c->wires[i].name, name) == 0) return i;
    return -1;
}

/* The "producer" of a wire is whatever node emits it: an INPUT node or a GATE node. */
static int find_producer_node(const CanvasState *cs, const char *wire_name) {
    for (int i = 0; i < cs->node_count; i++) {
        if (cs->nodes[i].kind == NODE_OUTPUT) continue;
        if (strcmp(cs->nodes[i].wire_name, wire_name) == 0) return i;
    }
    return -1;
}

static int find_output_node(const CanvasState *cs, const char *wire_name) {
    for (int i = 0; i < cs->node_count; i++)
        if (cs->nodes[i].kind == NODE_OUTPUT &&
            strcmp(cs->nodes[i].wire_name, wire_name) == 0) return i;
    return -1;
}

static int find_gate_by_out_wire(const CanvasState *cs, const char *out_wire) {
    for (int i = 0; i < cs->node_count; i++)
        if (cs->nodes[i].kind == NODE_GATE &&
            strcmp(cs->nodes[i].wire_name, out_wire) == 0) return i;
    return -1;
}

/* ── builders ─────────────────────────────────────────────────── */

static int add_node(CanvasState *cs, NodeKind kind, Vector2 pos,
                    const char *name, GateType gtype, int input_count) {
    if (cs->node_count >= cs->node_cap) {
        int nc = cs->node_cap ? cs->node_cap * 2 : INIT_CAP;
        CanvasNode *nn = realloc(cs->nodes, nc * sizeof(CanvasNode));
        if (!nn) return -1;
        cs->nodes = nn; cs->node_cap = nc;
    }
    int idx = cs->node_count++;
    CanvasNode *n = &cs->nodes[idx];
    n->kind = kind;
    n->pos = pos;
    n->gate_type = gtype;
    n->input_count = input_count;
    n->selected = 0;
    strncpy(n->wire_name, name, SIM_NAME_LEN - 1);
    n->wire_name[SIM_NAME_LEN - 1] = '\0';
    return idx;
}

/* ── selection helpers ───────────────────────────────────────── */

void canvas_clear_selection(CanvasState *cs) {
    for (int i = 0; i < cs->node_count; i++) cs->nodes[i].selected = 0;
}

void canvas_select_all(CanvasState *cs) {
    for (int i = 0; i < cs->node_count; i++) cs->nodes[i].selected = 1;
}

void canvas_select_in_rect(CanvasState *cs, Rectangle rect, int additive) {
    if (!additive) canvas_clear_selection(cs);
    for (int i = 0; i < cs->node_count; i++) {
        if (CheckCollisionPointRec(cs->nodes[i].pos, rect))
            cs->nodes[i].selected = 1;
    }
}

int canvas_selection_count(const CanvasState *cs) {
    int n = 0;
    for (int i = 0; i < cs->node_count; i++) if (cs->nodes[i].selected) n++;
    return n;
}

void canvas_remove_selected(CanvasState *cs) {
    /* Remove from end to avoid index shifting issues. */
    for (int i = cs->node_count - 1; i >= 0; i--)
        if (cs->nodes[i].selected) canvas_remove_node(cs, i);
}

int canvas_add_input(CanvasState *cs, const char *name, Vector2 pos) {
    return add_node(cs, NODE_INPUT, pos, name, GATE_AND, 0);
}

int canvas_add_output(CanvasState *cs, const char *name, Vector2 pos) {
    return add_node(cs, NODE_OUTPUT, pos, name, GATE_AND, 0);
}

int canvas_add_gate(CanvasState *cs, GateType gtype, const char *name, Vector2 pos) {
    int input_count = (gtype == GATE_NOT) ? 1 : 2;
    return add_node(cs, NODE_GATE, pos, name, gtype, input_count);
}

int canvas_add_wire(CanvasState *cs, int src, int dst, int dst_pin) {
    if (cs->wire_count >= cs->wire_cap) {
        int nc = cs->wire_cap ? cs->wire_cap * 2 : INIT_CAP;
        CanvasWire *nw = realloc(cs->wires, nc * sizeof(CanvasWire));
        if (!nw) return -1;
        cs->wires = nw; cs->wire_cap = nc;
    }
    cs->wires[cs->wire_count++] = (CanvasWire){src, dst, dst_pin};
    return 0;
}

void canvas_remove_wire(CanvasState *cs, int wire_idx) {
    if (wire_idx < 0 || wire_idx >= cs->wire_count) return;
    for (int i = wire_idx; i < cs->wire_count - 1; i++)
        cs->wires[i] = cs->wires[i + 1];
    cs->wire_count--;
}

void canvas_remove_node(CanvasState *cs, int node_idx) {
    if (node_idx < 0 || node_idx >= cs->node_count) return;

    /* Drop wires touching this node; renumber the rest. */
    int w = 0;
    for (int i = 0; i < cs->wire_count; i++) {
        if (cs->wires[i].src_node == node_idx || cs->wires[i].dst_node == node_idx)
            continue;
        if (cs->wires[i].src_node > node_idx) cs->wires[i].src_node--;
        if (cs->wires[i].dst_node > node_idx) cs->wires[i].dst_node--;
        cs->wires[w++] = cs->wires[i];
    }
    cs->wire_count = w;

    for (int i = node_idx; i < cs->node_count - 1; i++)
        cs->nodes[i] = cs->nodes[i + 1];
    cs->node_count--;
}

static Vector2 layout_pos(int col, int row, int total) {
    return (Vector2){
        X_OFFSET + col * X_STEP,
        Y_CENTER + (row - (total - 1) * 0.5f) * Y_STEP
    };
}

/* ── pin geometry (used by both layout and drawing) ──────────── */

Vector2 canvas_node_output_pin(const CanvasNode *n) {
    if (n->kind == NODE_GATE)  return (Vector2){n->pos.x + GATE_W / 2, n->pos.y};
    if (n->kind == NODE_INPUT) return (Vector2){n->pos.x + IO_R,       n->pos.y};
    return n->pos;
}

Vector2 canvas_node_input_pin(const CanvasNode *n, int pin_idx) {
    if (n->kind == NODE_GATE) {
        Vector2 p = {n->pos.x - GATE_W / 2, n->pos.y};
        if (n->input_count == 2)
            p.y += (pin_idx == 0 ? -GATE_H / 4 : GATE_H / 4);
        return p;
    }
    if (n->kind == NODE_OUTPUT) return (Vector2){n->pos.x - IO_R, n->pos.y};
    return n->pos;
}

/* ── hit-testing ──────────────────────────────────────────────── */

int canvas_node_at(const CanvasState *cs, Vector2 pt) {
    /* Iterate from the end so most-recently-added (drawn on top) wins. */
    for (int i = cs->node_count - 1; i >= 0; i--) {
        const CanvasNode *n = &cs->nodes[i];
        if (n->kind == NODE_GATE) {
            Rectangle r = {n->pos.x - GATE_W / 2, n->pos.y - GATE_H / 2, GATE_W, GATE_H};
            if (CheckCollisionPointRec(pt, r)) return i;
        } else {
            if (CheckCollisionPointCircle(pt, n->pos, IO_R)) return i;
        }
    }
    return -1;
}

int canvas_output_pin_at(const CanvasState *cs, Vector2 pt) {
    for (int i = cs->node_count - 1; i >= 0; i--) {
        const CanvasNode *n = &cs->nodes[i];
        if (n->kind == NODE_OUTPUT) continue; /* OUTPUT nodes have no output pin */
        Vector2 p = canvas_node_output_pin(n);
        if (CheckCollisionPointCircle(pt, p, PIN_HIT_R)) return i;
    }
    return -1;
}

int canvas_input_pin_at(const CanvasState *cs, Vector2 pt, int *pin_out) {
    for (int i = cs->node_count - 1; i >= 0; i--) {
        const CanvasNode *n = &cs->nodes[i];
        if (n->kind == NODE_INPUT) continue; /* INPUT nodes have no input pin */
        if (n->kind == NODE_OUTPUT) {
            if (CheckCollisionPointCircle(pt, canvas_node_input_pin(n, 0), PIN_HIT_R)) {
                if (pin_out) *pin_out = 0;
                return i;
            }
        } else { /* GATE */
            for (int j = 0; j < n->input_count; j++) {
                if (CheckCollisionPointCircle(pt, canvas_node_input_pin(n, j), PIN_HIT_R)) {
                    if (pin_out) *pin_out = j;
                    return i;
                }
            }
        }
    }
    return -1;
}

int canvas_wire_at(const CanvasState *cs, Vector2 pt) {
    for (int i = cs->wire_count - 1; i >= 0; i--) {
        const CanvasWire *w = &cs->wires[i];
        Vector2 p1 = canvas_node_output_pin(&cs->nodes[w->src_node]);
        Vector2 p2 = canvas_node_input_pin (&cs->nodes[w->dst_node], w->dst_pin);
        if (CheckCollisionPointLine(pt, p1, p2, WIRE_HIT_R)) return i;
    }
    return -1;
}

/* ── public: init / free ──────────────────────────────────────── */

void canvas_init(CanvasState *cs, const Circuit *c) {
    cs->nodes = NULL; cs->node_count = 0; cs->node_cap = 0;
    cs->wires = NULL; cs->wire_count = 0; cs->wire_cap = 0;
    if (c->wire_count == 0 && c->input_count == 0 && c->output_count == 0) return;

    /* Step 1: depth of each wire (inputs = 0; gate outputs = max(input depths) + 1) */
    int *depth = calloc(c->wire_count > 0 ? c->wire_count : 1, sizeof(int));
    for (int i = 0; i < c->gate_count; i++) {
        const GateInst *g = &c->gates[i];
        int max_d = 0;
        for (int j = 0; j < g->in_count; j++) {
            int idx = find_wire_idx(c, g->in_wires[j]);
            if (idx >= 0 && depth[idx] > max_d) max_d = depth[idx];
        }
        int out_idx = find_wire_idx(c, g->out_wire);
        if (out_idx >= 0) depth[out_idx] = max_d + 1;
    }

    /* Step 2: outputs go in column max_depth + 1 */
    int max_depth = 0;
    for (int i = 0; i < c->wire_count; i++)
        if (depth[i] > max_depth) max_depth = depth[i];
    int output_col = max_depth + 1;
    int total_cols = output_col + 1;

    /* Step 3: count nodes per column for vertical centering */
    int *col_total = calloc(total_cols, sizeof(int));
    int *col_idx   = calloc(total_cols, sizeof(int));
    col_total[0] = c->input_count;
    for (int i = 0; i < c->gate_count; i++) {
        int d = depth[find_wire_idx(c, c->gates[i].out_wire)];
        col_total[d]++;
    }
    col_total[output_col] += c->output_count;

    /* Step 4: place input nodes (column 0) */
    for (int i = 0; i < c->input_count; i++) {
        Vector2 p = layout_pos(0, col_idx[0]++, col_total[0]);
        add_node(cs, NODE_INPUT, p, c->input_names[i], GATE_AND, 0);
    }

    /* Step 5: place gate nodes (column = depth) */
    for (int i = 0; i < c->gate_count; i++) {
        const GateInst *g = &c->gates[i];
        int d = depth[find_wire_idx(c, g->out_wire)];
        Vector2 p = layout_pos(d, col_idx[d]++, col_total[d]);
        add_node(cs, NODE_GATE, p, g->out_wire, g->type, g->in_count);
    }

    /* Step 6: place output nodes (rightmost column) */
    for (int i = 0; i < c->output_count; i++) {
        Vector2 p = layout_pos(output_col, col_idx[output_col]++, col_total[output_col]);
        add_node(cs, NODE_OUTPUT, p, c->output_names[i], GATE_AND, 0);
    }

    /* Step 7: wires from each gate's inputs to the gate */
    for (int i = 0; i < c->gate_count; i++) {
        const GateInst *g = &c->gates[i];
        int dst = find_gate_by_out_wire(cs, g->out_wire);
        if (dst < 0) continue;
        for (int j = 0; j < g->in_count; j++) {
            int src = find_producer_node(cs, g->in_wires[j]);
            if (src >= 0) canvas_add_wire(cs, src, dst, j);
        }
    }
    /* Step 8: wires from output producers to output sink nodes */
    for (int i = 0; i < c->output_count; i++) {
        int src = find_producer_node(cs, c->output_names[i]);
        int dst = find_output_node(cs, c->output_names[i]);
        if (src >= 0 && dst >= 0) canvas_add_wire(cs, src, dst, 0);
    }

    free(depth);
    free(col_total);
    free(col_idx);
}

void canvas_free(CanvasState *cs) {
    free(cs->nodes); cs->nodes = NULL;
    free(cs->wires); cs->wires = NULL;
    cs->node_count = cs->node_cap = 0;
    cs->wire_count = cs->wire_cap = 0;
}

/* ── public: draw ─────────────────────────────────────────────── */

void canvas_draw(const CanvasState *cs, Camera2D cam) {
    (void)cam; /* reserved for future culling/LOD */

    /* Wires first (under nodes) */
    for (int i = 0; i < cs->wire_count; i++) {
        const CanvasWire *w = &cs->wires[i];
        Vector2 p1 = canvas_node_output_pin(&cs->nodes[w->src_node]);
        Vector2 p2 = canvas_node_input_pin (&cs->nodes[w->dst_node], w->dst_pin);
        DrawLineEx(p1, p2, 2.0f, DARKGRAY);
    }

    /* Nodes on top */
    for (int i = 0; i < cs->node_count; i++) {
        const CanvasNode *n = &cs->nodes[i];
        switch (n->kind) {
            case NODE_INPUT:
                DrawCircleV(n->pos, IO_R, BLUE);
                DrawCircleLines(n->pos.x, n->pos.y, IO_R, BLACK);
                DrawText(n->wire_name,
                         n->pos.x - MeasureText(n->wire_name, 18) / 2,
                         n->pos.y - 36, 18, BLACK);
                break;

            case NODE_OUTPUT:
                DrawCircleV(n->pos, IO_R, GREEN);
                DrawCircleLines(n->pos.x, n->pos.y, IO_R, BLACK);
                DrawText(n->wire_name,
                         n->pos.x - MeasureText(n->wire_name, 18) / 2,
                         n->pos.y - 36, 18, BLACK);
                break;

            case NODE_GATE: {
                Rectangle r = {n->pos.x - GATE_W / 2, n->pos.y - GATE_H / 2,
                               GATE_W, GATE_H};
                DrawRectangleRec(r, RAYWHITE);
                DrawRectangleLinesEx(r, 2, BLACK);

                /* Uppercase the gate type for display */
                const char *lower = gate_type_name(n->gate_type);
                char upper[8] = {0};
                for (int k = 0; lower[k] && k < 7; k++)
                    upper[k] = (char)toupper((unsigned char)lower[k]);
                int tw = MeasureText(upper, 20);
                DrawText(upper, n->pos.x - tw / 2, n->pos.y - 10, 20, BLACK);

                /* Pins */
                for (int j = 0; j < n->input_count; j++) {
                    Vector2 p = canvas_node_input_pin(n, j);
                    DrawCircleV(p, 4, BLACK);
                }
                DrawCircleV(canvas_node_output_pin(n), 4, BLACK);

                /* Out-wire label above the box (small grey text) */
                int lw = MeasureText(n->wire_name, 12);
                DrawText(n->wire_name, n->pos.x - lw / 2,
                         n->pos.y - GATE_H / 2 - 16, 12, GRAY);
                break;
            }
        }
    }

    /* Selection highlights — drawn last so they sit on top of everything. */
    Color hi = (Color){255, 140, 0, 255}; /* orange */
    for (int i = 0; i < cs->node_count; i++) {
        const CanvasNode *n = &cs->nodes[i];
        if (!n->selected) continue;
        if (n->kind == NODE_GATE) {
            Rectangle r = {n->pos.x - GATE_W / 2 - 4, n->pos.y - GATE_H / 2 - 4,
                           GATE_W + 8, GATE_H + 8};
            DrawRectangleLinesEx(r, 3, hi);
        } else {
            DrawCircleLines(n->pos.x, n->pos.y, IO_R + 4, hi);
            DrawCircleLines(n->pos.x, n->pos.y, IO_R + 5, hi);
        }
    }
}
