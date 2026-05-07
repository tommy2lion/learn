#ifndef DCS_DOMAIN_SIMULATION_H
#define DCS_DOMAIN_SIMULATION_H

#include "circuit.h"
#include "waveform.h"

/* Stimulus callback: returns the value to set for circuit input
   `input_idx` at simulation step `step`. */
typedef signal_t (*stimulus_fn_t)(int step, int input_idx, void *user);

class tagt_simulation {
    circuit_t  *circuit;     /* not owned */
    waveform_t  waves;       /* owned (released by simulation_release) */
};
typedef class tagt_simulation simulation_t;

void simulation_init   (simulation_t *self, circuit_t *c);
void simulation_release(simulation_t *self);

/* Run for `steps` steps. Allocates one waveform track per circuit input
   (in declaration order), then per circuit output (in declaration order),
   each with `steps` recorded values. */
void simulation_run(simulation_t *self, int steps,
                    stimulus_fn_t stim, void *user);

#endif /* DCS_DOMAIN_SIMULATION_H */
