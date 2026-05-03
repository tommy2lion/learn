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

/* Build CanvasState from Circuit: auto-layout nodes by topological depth,
   wire up gate inputs and circuit outputs to their producers. */
void canvas_init(CanvasState *cs, const Circuit *c);
void canvas_free(CanvasState *cs);

/* Render nodes and wires. Must be called inside BeginMode2D(cam). */
void canvas_draw(const CanvasState *cs, Camera2D cam);

#endif /* DCS_CANVAS_H */
