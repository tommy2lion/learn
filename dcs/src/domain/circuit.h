#ifndef DCS_DOMAIN_CIRCUIT_H
#define DCS_DOMAIN_CIRCUIT_H

#include "component.h"

class tagt_wire {
    char     name[DOMAIN_NAME_LEN];
    signal_t value;
};
typedef class tagt_wire wire_t;

class tagt_circuit {
    component_t **components;          /* owned (destroyed with circuit) */
    int           component_count;
    int           component_cap;

    wire_t       *wires;               /* every named signal: inputs + comp outputs */
    int           wire_count;
    int           wire_cap;

    char  input_names [DOMAIN_MAX_IO][DOMAIN_NAME_LEN];
    int   input_count;
    char  output_names[DOMAIN_MAX_IO][DOMAIN_NAME_LEN];
    int   output_count;
};
typedef class tagt_circuit circuit_t;

circuit_t *circuit_create(void);
void       circuit_destroy(circuit_t *self);

/* Returns 0 on success, -1 on error. */
int circuit_add_input (circuit_t *self, const char *name);
int circuit_add_output(circuit_t *self, const char *name);

/* Add a component (already created by gate_*_create). The component's
   `name` becomes its output wire's name. `in1` (and `in2` for 2-input
   gates) must already exist as wires. Takes ownership: the component
   is freed when the circuit is destroyed. Returns 0 on success, -1 if
   any input wire is missing or the output wire name is already taken. */
int circuit_add_component(circuit_t *self, component_t *c,
                          const char *in1, const char *in2);

/* Wire I/O. */
void     circuit_set_input  (circuit_t *self, const char *name, signal_t v);
signal_t circuit_get_wire   (const circuit_t *self, const char *name);
signal_t circuit_get_output (const circuit_t *self, const char *name);

/* Evaluate every component in stored order (which is topological by
   construction — circuit_add_component requires inputs to already exist). */
int circuit_evaluate(circuit_t *self);

#endif /* DCS_DOMAIN_CIRCUIT_H */
