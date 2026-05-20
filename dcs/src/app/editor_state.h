#ifndef DCS_APP_EDITOR_STATE_H
#define DCS_APP_EDITOR_STATE_H

#include "../framework/core/oo.h"
#include "../framework/core/rect.h"
#include "../domain/circuit.h"

/* Identifies a node visually shown on the circuit canvas. There are three
   kinds of nodes — components, external inputs, external outputs — each
   addressed by an index into the corresponding circuit_t array. We pack
   "kind + index" into a single int so selection/hover can be uniform. */

typedef enum {
    NODE_NONE   = 0,
    NODE_COMPONENT,
    NODE_INPUT,
    NODE_OUTPUT,
} node_kind_t;

class tagt_node_ref {
    node_kind_t kind;
    int         index;
};
typedef class tagt_node_ref node_ref_t;

#define NODE_REF_NONE ((node_ref_t){NODE_NONE, -1})

static inline int node_ref_eq(node_ref_t a, node_ref_t b) {
    return a.kind == b.kind && a.index == b.index;
}

/* Two views of the same circuit: the full internal schematic, or a
   black-box rectangle exposing only the top-level I/O pins. (Phase 8) */
typedef enum {
    DISPLAY_INTERNAL = 0,
    DISPLAY_EXTERNAL,
} display_mode_t;

#endif /* DCS_APP_EDITOR_STATE_H */
