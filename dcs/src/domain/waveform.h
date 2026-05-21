#ifndef DCS_DOMAIN_WAVEFORM_H
#define DCS_DOMAIN_WAVEFORM_H

#include "component.h"

/* Multi-channel time-series storage. Each track holds N step values for one
   named signal (an external input or output). Used as simulation_t's output
   and as the timing-canvas widget's input. */

class tagt_waveform_track {
    char      name[DOMAIN_NAME_LEN];
    /* Heap-allocated, length == step_count (owned by this track).
       INVARIANT: values is non-NULL iff step_count > 0. Callers iterating
       a track must check step_count > 0 before reading values[]. */
    signal_t *values;
    int       step_count;
};
typedef class tagt_waveform_track waveform_track_t;

class tagt_waveform {
    waveform_track_t *tracks;
    int               track_count;
    int               track_cap;
    int               step_count;     /* shared by all tracks; set by waveform_init */
};
typedef class tagt_waveform waveform_t;

void waveform_init   (waveform_t *self, int step_count);
void waveform_release(waveform_t *self);   /* frees tracks + values */
int  waveform_add_track  (waveform_t *self, const char *name);
void waveform_set_value  (waveform_t *self, int track_idx, int step, signal_t v);
const waveform_track_t *waveform_get_track(const waveform_t *self, int track_idx);

#endif /* DCS_DOMAIN_WAVEFORM_H */
