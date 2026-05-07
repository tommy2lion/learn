#include "simulation.h"

void simulation_init(simulation_t *self, circuit_t *c) {
    self->circuit = c;
    waveform_init(&self->waves, 0);
}

void simulation_release(simulation_t *self) {
    waveform_release(&self->waves);
    self->circuit = NULL;
}

void simulation_run(simulation_t *self, int steps,
                    stimulus_fn_t stim, void *user) {
    circuit_t *c = self->circuit;
    if (!c || steps <= 0) return;

    /* Reset and prepare tracks: inputs first, then outputs. */
    waveform_release(&self->waves);
    waveform_init(&self->waves, steps);
    for (int i = 0; i < c->input_count; i++)  waveform_add_track(&self->waves, c->input_names[i]);
    for (int i = 0; i < c->output_count; i++) waveform_add_track(&self->waves, c->output_names[i]);

    for (int s = 0; s < steps; s++) {
        for (int i = 0; i < c->input_count; i++) {
            signal_t v = stim ? stim(s, i, user) : SIG_UNDEF;
            circuit_set_input(c, c->input_names[i], v);
        }
        circuit_evaluate(c);
        for (int i = 0; i < c->input_count; i++)
            waveform_set_value(&self->waves, i, s,
                               circuit_get_wire(c, c->input_names[i]));
        for (int i = 0; i < c->output_count; i++)
            waveform_set_value(&self->waves, c->input_count + i, s,
                               circuit_get_output(c, c->output_names[i]));
    }
}
