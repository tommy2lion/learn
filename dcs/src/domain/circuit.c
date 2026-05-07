#include "circuit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INIT_CAP 8

/* ── lifecycle ───────────────────────────────────────────────────── */

circuit_t *circuit_create(void) {
    circuit_t *c = (circuit_t *)calloc(1, sizeof(circuit_t));
    if (!c) return NULL;
    c->components    = (component_t **)malloc(INIT_CAP * sizeof(component_t *));
    c->wires         = (wire_t       *)malloc(INIT_CAP * sizeof(wire_t));
    if (!c->components || !c->wires) {
        free(c->components); free(c->wires); free(c);
        return NULL;
    }
    c->component_cap = INIT_CAP;
    c->wire_cap      = INIT_CAP;
    return c;
}

void circuit_destroy(circuit_t *self) {
    if (!self) return;
    for (int i = 0; i < self->component_count; i++)
        component_destroy(self->components[i]);
    free(self->components);
    free(self->wires);
    free(self);
}

/* ── internal helpers ────────────────────────────────────────────── */

static wire_t *find_wire(const circuit_t *c, const char *name) {
    for (int i = 0; i < c->wire_count; i++)
        if (strcmp(c->wires[i].name, name) == 0)
            return &c->wires[i];
    return NULL;
}

static wire_t *push_wire(circuit_t *c, const char *name) {
    if (find_wire(c, name)) return NULL;        /* duplicate */
    if (c->wire_count >= c->wire_cap) {
        int nc = c->wire_cap * 2;
        wire_t *nw = (wire_t *)realloc(c->wires, nc * sizeof(wire_t));
        if (!nw) return NULL;
        c->wires    = nw;
        c->wire_cap = nc;
    }
    wire_t *w = &c->wires[c->wire_count++];
    snprintf(w->name, DOMAIN_NAME_LEN, "%s", name);
    w->value = SIG_UNDEF;
    return w;
}

/* ── public API ──────────────────────────────────────────────────── */

int circuit_add_input(circuit_t *self, const char *name) {
    if (self->input_count >= DOMAIN_MAX_IO) return -1;
    if (find_wire(self, name)) return -1;
    if (!push_wire(self, name)) return -1;
    snprintf(self->input_names[self->input_count], DOMAIN_NAME_LEN, "%s", name);
    self->input_count++;
    return 0;
}

int circuit_add_output(circuit_t *self, const char *name) {
    if (self->output_count >= DOMAIN_MAX_IO) return -1;
    snprintf(self->output_names[self->output_count], DOMAIN_NAME_LEN, "%s", name);
    self->output_count++;
    return 0;
}

int circuit_add_component(circuit_t *self, component_t *c,
                          const char *in1, const char *in2) {
    if (!c || !in1) return -1;
    int n_in = component_pin_count_in(c);
    if (n_in >= 1 && !find_wire(self, in1)) return -1;
    if (n_in >= 2 && (!in2 || !find_wire(self, in2))) return -1;
    if (find_wire(self, c->name)) return -1;            /* duplicate output */
    if (!push_wire(self, c->name)) return -1;

    if (self->component_count >= self->component_cap) {
        int nc = self->component_cap * 2;
        component_t **nn = (component_t **)realloc(self->components, nc * sizeof(component_t *));
        if (!nn) return -1;
        self->components    = nn;
        self->component_cap = nc;
    }
    snprintf(c->in_wires[0], DOMAIN_NAME_LEN, "%s", in1);
    if (n_in >= 2) snprintf(c->in_wires[1], DOMAIN_NAME_LEN, "%s", in2);
    self->components[self->component_count++] = c;
    return 0;
}

void circuit_set_input(circuit_t *self, const char *name, signal_t v) {
    wire_t *w = find_wire(self, name);
    if (w) w->value = v;
}

signal_t circuit_get_wire(const circuit_t *self, const char *name) {
    const wire_t *w = find_wire(self, name);
    return w ? w->value : SIG_UNDEF;
}

signal_t circuit_get_output(const circuit_t *self, const char *name) {
    return circuit_get_wire(self, name);
}

int circuit_evaluate(circuit_t *self) {
    signal_t in[DOMAIN_MAX_PINS_IN];
    signal_t out[1];
    for (int i = 0; i < self->component_count; i++) {
        component_t *c = self->components[i];
        int n_in = component_pin_count_in(c);
        for (int p = 0; p < n_in; p++)
            in[p] = circuit_get_wire(self, c->in_wires[p]);
        c->vt->evaluate(c, in, out);
        wire_t *w = find_wire(self, c->name);
        if (w) w->value = out[0];
    }
    return 0;
}
