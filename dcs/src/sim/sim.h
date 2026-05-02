#ifndef DCS_SIM_H
#define DCS_SIM_H

#include <stdint.h>

/* Signal values */
#define SIG_LOW   ((Signal)0)
#define SIG_HIGH  ((Signal)1)
#define SIG_UNDEF ((Signal)2)

typedef uint8_t Signal;

#define SIM_NAME_LEN    64
#define SIM_MAX_INPUTS  16
#define SIM_MAX_OUTPUTS 16

typedef struct {
    char   name[SIM_NAME_LEN];
    Signal value;
} Wire;

typedef enum {
    GATE_AND,
    GATE_OR,
    GATE_NOT
} GateType;

typedef struct {
    GateType type;
    char out_wire[SIM_NAME_LEN];
    char in_wires[2][SIM_NAME_LEN]; /* in_wires[1] unused for NOT */
    int  in_count;
} GateInst;

typedef struct {
    Wire     *wires;
    int       wire_count;
    int       wire_cap;
    GateInst *gates;       /* stored in topological order */
    int       gate_count;
    int       gate_cap;
    char      input_names [SIM_MAX_INPUTS ][SIM_NAME_LEN];
    int       input_count;
    char      output_names[SIM_MAX_OUTPUTS][SIM_NAME_LEN];
    int       output_count;
} Circuit;

Circuit *circuit_new(void);
void     circuit_free(Circuit *c);

/* Returns 0 on success, -1 on error (duplicate name or capacity exceeded) */
int circuit_add_input (Circuit *c, const char *name);
int circuit_add_output(Circuit *c, const char *name);

/* in2 must be NULL for GATE_NOT; in1/in2 must already exist as wires.
   Returns 0 on success, -1 if an input wire is missing or out_wire already exists. */
int circuit_add_gate(Circuit *c, GateType type, const char *out_wire,
                     const char *in1, const char *in2);

/* Set an input wire value before calling circuit_run. */
void   circuit_set_input(Circuit *c, const char *name, Signal value);

/* Read any wire value (inputs, internal, outputs). Returns SIG_UNDEF if not found. */
Signal circuit_get_wire(const Circuit *c, const char *name);

/* Convenience wrapper — same as circuit_get_wire for output names. */
Signal circuit_get_output(const Circuit *c, const char *name);

/* Evaluate all gates in stored order. Always returns 0 (reserved for future error codes). */
int circuit_run(Circuit *c);

const char *gate_type_name(GateType type);

#endif /* DCS_SIM_H */
