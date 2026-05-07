#include "waveform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INIT_TRACK_CAP 8

void waveform_init(waveform_t *self, int step_count) {
    self->tracks      = NULL;
    self->track_count = 0;
    self->track_cap   = 0;
    self->step_count  = step_count;
}

void waveform_release(waveform_t *self) {
    for (int i = 0; i < self->track_count; i++) free(self->tracks[i].values);
    free(self->tracks);
    self->tracks      = NULL;
    self->track_count = 0;
    self->track_cap   = 0;
}

int waveform_add_track(waveform_t *self, const char *name) {
    if (self->track_count >= self->track_cap) {
        int nc = self->track_cap ? self->track_cap * 2 : INIT_TRACK_CAP;
        waveform_track_t *nt = (waveform_track_t *)realloc(self->tracks,
                                                           nc * sizeof(*nt));
        if (!nt) return -1;
        self->tracks    = nt;
        self->track_cap = nc;
    }
    int idx = self->track_count;
    waveform_track_t *t = &self->tracks[idx];
    snprintf(t->name, DOMAIN_NAME_LEN, "%s", name);
    t->step_count = self->step_count;
    if (self->step_count > 0) {
        t->values = (signal_t *)calloc(self->step_count, sizeof(signal_t));
        if (!t->values) return -1;
    } else {
        t->values = NULL;
    }
    self->track_count++;
    return idx;
}

void waveform_set_value(waveform_t *self, int track_idx, int step, signal_t v) {
    if (track_idx < 0 || track_idx >= self->track_count) return;
    if (step < 0 || step >= self->step_count) return;
    self->tracks[track_idx].values[step] = v;
}

const waveform_track_t *waveform_get_track(const waveform_t *self, int idx) {
    if (idx < 0 || idx >= self->track_count) return NULL;
    return &self->tracks[idx];
}
