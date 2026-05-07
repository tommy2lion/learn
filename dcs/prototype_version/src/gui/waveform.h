#ifndef DCS_WAVEFORM_H
#define DCS_WAVEFORM_H

#include "raylib.h"
#include "sim.h"

typedef struct {
    char    signal_name[SIM_NAME_LEN];
    Signal *values;        /* caller-owned, length = step_count */
    int     step_count;
} WaveformTrack;

/* Render `track_count` traces stacked vertically inside `bounds`.
   Each trace shows a step-function: HIGH = top, LOW = bottom, UNDEF = middle (red). */
void waveform_draw(const WaveformTrack *tracks, int track_count, Rectangle bounds);

#endif /* DCS_WAVEFORM_H */
