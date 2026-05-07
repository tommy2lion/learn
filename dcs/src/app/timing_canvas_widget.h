#ifndef DCS_APP_TIMING_CANVAS_H
#define DCS_APP_TIMING_CANVAS_H

#include "../framework/widgets/widget.h"
#include "../domain/waveform.h"

class tagt_timing_canvas_widget {
    widget_t          base;
    const waveform_t *waves;     /* not owned */
};
typedef class tagt_timing_canvas_widget timing_canvas_widget_t;

timing_canvas_widget_t *timing_canvas_widget_create(rect_t bounds);
void timing_canvas_widget_set_waves(timing_canvas_widget_t *self, const waveform_t *w);

#endif /* DCS_APP_TIMING_CANVAS_H */
