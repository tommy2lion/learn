#include "waveform.h"
#include <stdio.h>
#include <string.h>

#define WAVE_NAME_W 70.0f
#define WAVE_PAD     8.0f
#define WAVE_ROW_H_MIN 20.0f

static float signal_y(Signal v, float top, float bot) {
    if (v == SIG_HIGH)  return top;
    if (v == SIG_LOW)   return bot;
    return (top + bot) * 0.5f; /* SIG_UNDEF */
}

static Color signal_color(Signal v) {
    if (v == SIG_UNDEF) return (Color){200, 60, 60, 255};
    return (Color){50, 90, 160, 255};
}

static void draw_track(const WaveformTrack *t, Rectangle row) {
    /* signal name on the left */
    DrawText(t->signal_name,
             row.x + 6,
             row.y + (int)row.height / 2 - 7,
             14, BLACK);

    float trace_x = row.x + WAVE_NAME_W;
    float trace_w = row.width - WAVE_NAME_W;
    if (t->step_count <= 0 || trace_w <= 0) return;

    float top = row.y + 4;
    float bot = row.y + row.height - 4;
    float step_w = trace_w / t->step_count;

    /* light frame */
    DrawLine((int)trace_x, (int)top, (int)(trace_x + trace_w), (int)top, LIGHTGRAY);
    DrawLine((int)trace_x, (int)bot, (int)(trace_x + trace_w), (int)bot, LIGHTGRAY);

    /* step boundary tick marks */
    for (int i = 0; i <= t->step_count; i++) {
        float x = trace_x + i * step_w;
        DrawLine((int)x, (int)bot, (int)x, (int)bot + 3, GRAY);
    }

    Signal prev = t->values[0];
    for (int i = 0; i < t->step_count; i++) {
        float x0 = trace_x + i * step_w;
        float x1 = trace_x + (i + 1) * step_w;
        Signal v = t->values[i];
        float y  = signal_y(v, top, bot);
        Color  c = signal_color(v);

        /* horizontal segment for this step */
        DrawLineEx((Vector2){x0, y}, (Vector2){x1, y}, 2.0f, c);

        /* vertical edge if value changed from previous step */
        if (i > 0 && v != prev) {
            float y_prev = signal_y(prev, top, bot);
            DrawLineEx((Vector2){x0, y_prev}, (Vector2){x0, y}, 2.0f, c);
        }
        prev = v;
    }
}

void waveform_draw(const WaveformTrack *tracks, int track_count, Rectangle bounds) {
    /* Title strip */
    DrawText("Timing Diagram", (int)(bounds.x + WAVE_PAD), (int)(bounds.y + 4), 14, DARKGRAY);

    Rectangle inner = {
        bounds.x + WAVE_PAD,
        bounds.y + 22,
        bounds.width  - WAVE_PAD * 2,
        bounds.height - 22 - WAVE_PAD
    };

    if (track_count <= 0) {
        DrawText("(no run yet — set inputs and click RUN)",
                 (int)(inner.x), (int)(inner.y + inner.height / 2 - 7), 13, GRAY);
        return;
    }

    float row_h = inner.height / track_count;
    if (row_h < WAVE_ROW_H_MIN) row_h = WAVE_ROW_H_MIN;

    for (int i = 0; i < track_count; i++) {
        Rectangle row = { inner.x, inner.y + i * row_h, inner.width, row_h };
        draw_track(&tracks[i], row);
    }
}
