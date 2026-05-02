#include "sim.h"
#include <stdlib.h>
#include <string.h>

#define INIT_CAP 8

Circuit *circuit_new(void) {
    Circuit *c = calloc(1, sizeof(Circuit));
    if (!c) return NULL;
    c->wires = malloc(INIT_CAP * sizeof(Wire));
    c->gates = malloc(INIT_CAP * sizeof(GateInst));
    if (!c->wires || !c->gates) {
        free(c->wires);
        free(c->gates);
        free(c);
        return NULL;
    }
    c->wire_cap = INIT_CAP;
    c->gate_cap = INIT_CAP;
    return c;
}

void circuit_free(Circuit *c) {
    if (!c) return;
    free(c->wires);
    free(c->gates);
    free(c);
}

/* ---------- internal helpers ---------- */

static Wire *find_wire(const Circuit *c, const char *name) {
    for (int i = 0; i < c->wire_count; i++)
        if (strcmp(c->wires[i].name, name) == 0)
            return &c->wires[i];
    return NULL;
}

static Wire *push_wire(Circuit *c, const char *name) {
    if (c->wire_count >= c->wire_cap) {
        int new_cap = c->wire_cap * 2;
        Wire *nw = realloc(c->wires, new_cap * sizeof(Wire));
        if (!nw) return NULL;
        c->wires    = nw;
        c->wire_cap = new_cap;
    }
    Wire *w = &c->wires[c->wire_count++];
    strncpy(w->name, name, SIM_NAME_LEN - 1);
    w->name[SIM_NAME_LEN - 1] = '\0';
    w->value = SIG_UNDEF;
    return w;
}

/* ---------- public API ---------- */

int circuit_add_input(Circuit *c, const char *name) {
    if (c->input_count >= SIM_MAX_INPUTS) return -1;
    if (find_wire(c, name)) return -1;
    if (!push_wire(c, name)) return -1;
    strncpy(c->input_names[c->input_count], name, SIM_NAME_LEN - 1);
    c->input_names[c->input_count][SIM_NAME_LEN - 1] = '\0';
    c->input_count++;
    return 0;
}

int circuit_add_output(Circuit *c, const char *name) {
    if (c->output_count >= SIM_MAX_OUTPUTS) return -1;
    /* Output names are recorded but the wire itself is created by circuit_add_gate.
       We do not create a wire here because the output wire is the gate's out_wire. */
    strncpy(c->output_names[c->output_count], name, SIM_NAME_LEN - 1);
    c->output_names[c->output_count][SIM_NAME_LEN - 1] = '\0';
    c->output_count++;
    return 0;
}

int circuit_add_gate(Circuit *c, GateType type, const char *out_wire,
                     const char *in1, const char *in2) {
    if (!find_wire(c, in1)) return -1;
    if (type != GATE_NOT && (!in2 || !find_wire(c, in2))) return -1;
    if (find_wire(c, out_wire)) return -1; /* duplicate output */
    if (!push_wire(c, out_wire)) return -1;

    if (c->gate_count >= c->gate_cap) {
        int new_cap = c->gate_cap * 2;
        GateInst *ng = realloc(c->gates, new_cap * sizeof(GateInst));
        if (!ng) return -1;
        c->gates    = ng;
        c->gate_cap = new_cap;
    }
    GateInst *g = &c->gates[c->gate_count++];
    g->type = type;
    strncpy(g->out_wire,    out_wire, SIM_NAME_LEN - 1);
    g->out_wire[SIM_NAME_LEN - 1] = '\0';
    strncpy(g->in_wires[0], in1,      SIM_NAME_LEN - 1);
    g->in_wires[0][SIM_NAME_LEN - 1] = '\0';
    if (in2) {
        strncpy(g->in_wires[1], in2, SIM_NAME_LEN - 1);
        g->in_wires[1][SIM_NAME_LEN - 1] = '\0';
        g->in_count = 2;
    } else {
        g->in_wires[1][0] = '\0';
        g->in_count = 1;
    }
    return 0;
}

void circuit_set_input(Circuit *c, const char *name, Signal value) {
    Wire *w = find_wire(c, name);
    if (w) w->value = value;
}

Signal circuit_get_wire(const Circuit *c, const char *name) {
    const Wire *w = find_wire(c, name);
    return w ? w->value : SIG_UNDEF;
}

Signal circuit_get_output(const Circuit *c, const char *name) {
    return circuit_get_wire(c, name);
}

/* ---------- gate evaluation ---------- */

static Signal eval_and(Signal a, Signal b) {
    if (a == SIG_LOW  || b == SIG_LOW)  return SIG_LOW;
    if (a == SIG_UNDEF || b == SIG_UNDEF) return SIG_UNDEF;
    return SIG_HIGH;
}

static Signal eval_or(Signal a, Signal b) {
    if (a == SIG_HIGH || b == SIG_HIGH) return SIG_HIGH;
    if (a == SIG_UNDEF || b == SIG_UNDEF) return SIG_UNDEF;
    return SIG_LOW;
}

static Signal eval_not(Signal a) {
    if (a == SIG_UNDEF) return SIG_UNDEF;
    return (a == SIG_LOW) ? SIG_HIGH : SIG_LOW;
}

int circuit_run(Circuit *c) {
    for (int i = 0; i < c->gate_count; i++) {
        GateInst *g = &c->gates[i];
        Signal in1 = circuit_get_wire(c, g->in_wires[0]);
        Signal in2 = (g->in_count > 1) ? circuit_get_wire(c, g->in_wires[1]) : SIG_UNDEF;
        Signal out;
        switch (g->type) {
            case GATE_AND: out = eval_and(in1, in2); break;
            case GATE_OR:  out = eval_or (in1, in2); break;
            case GATE_NOT: out = eval_not(in1);       break;
            default:       out = SIG_UNDEF;           break;
        }
        Wire *w = find_wire(c, g->out_wire);
        if (w) w->value = out;
    }
    return 0;
}

const char *gate_type_name(GateType type) {
    switch (type) {
        case GATE_AND: return "and";
        case GATE_OR:  return "or";
        case GATE_NOT: return "not";
        default:       return "?";
    }
}
