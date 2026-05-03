#ifndef DCS_CANVAS_H
#define DCS_CANVAS_H

#include "raylib.h"
#include "sim.h"

typedef enum {
    NODE_INPUT,    /* circuit input  → drawn as a circle on the left edge  */
    NODE_OUTPUT,   /* circuit output → drawn as a circle on the right edge */
    NODE_GATE,     /* logic gate     → drawn as a labeled rectangle        */
} NodeKind;

typedef struct {
    NodeKind kind;
    Vector2  pos;                  /* center of the node in canvas/world space */
    char     wire_name[SIM_NAME_LEN];
                                    /* INPUT/OUTPUT: external wire name
                                       GATE:         out_wire (used to find producer) */
    GateType gate_type;             /* valid only for NODE_GATE */
    int      input_count;           /* 0 for I/O, 1 for NOT, 2 for AND/OR */
} CanvasNode;

typedef struct {
    int src_node;                   /* node providing the signal */
    int dst_node;                   /* node receiving the signal */
    int dst_pin;                    /* 0 or 1 (gate input pin index) */
} CanvasWire;

typedef struct {
    CanvasNode *nodes;
    int         node_count, node_cap;
    CanvasWire *wires;
    int         wire_count, wire_cap;
} CanvasState;

/* ── Build / teardown ─────────────────────────────────────────── */

/* Build CanvasState from Circuit: auto-layout nodes by topological depth,
   wire up gate inputs and circuit outputs to their producers. */
void canvas_init(CanvasState *cs, const Circuit *c);
void canvas_free(CanvasState *cs);

/* Render nodes and wires. Must be called inside BeginMode2D(cam). */
void canvas_draw(const CanvasState *cs, Camera2D cam);

/* ── Mutators (used by the editor) ────────────────────────────── */

int  canvas_add_input (CanvasState *cs, const char *name, Vector2 pos);
int  canvas_add_output(CanvasState *cs, const char *name, Vector2 pos);
int  canvas_add_gate  (CanvasState *cs, GateType gtype, const char *out_wire, Vector2 pos);
int  canvas_add_wire  (CanvasState *cs, int src, int dst, int dst_pin);
void canvas_remove_node(CanvasState *cs, int node_idx);
void canvas_remove_wire(CanvasState *cs, int wire_idx);

/* ── Geometry ─────────────────────────────────────────────────── */

Vector2 canvas_node_output_pin(const CanvasNode *n);
Vector2 canvas_node_input_pin (const CanvasNode *n, int pin_idx);

/* ── Hit-testing (returns -1 if nothing matches) ──────────────── */

int canvas_node_at      (const CanvasState *cs, Vector2 pt);
int canvas_output_pin_at(const CanvasState *cs, Vector2 pt);
int canvas_input_pin_at (const CanvasState *cs, Vector2 pt, int *pin_out);
int canvas_wire_at      (const CanvasState *cs, Vector2 pt);

#endif /* DCS_CANVAS_H */
