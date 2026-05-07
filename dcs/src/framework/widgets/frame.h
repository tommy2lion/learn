#ifndef DCS_FW_FRAME_H
#define DCS_FW_FRAME_H

#include "../core/oo.h"
#include "../core/focus_manager.h"
#include "../core/quit_manager.h"
#include "../graphics/igraph.h"
#include "../platform/iplatform.h"
#include "widget.h"

class tagt_frame {
    igraph_t        *graph;
    iplatform_t     *platform;
    widget_t        *root;
    focus_manager_t  focus;
    quit_manager_t   quit;
};
typedef class tagt_frame frame_t;

void frame_init    (frame_t *self, igraph_t *g, iplatform_t *p, widget_t *root);
/* Single tick: poll input, dispatch events, redraw the widget tree. */
void frame_tick    (frame_t *self);
/* Blocking main loop. Returns when quit_manager says to. */
void frame_run     (frame_t *self);
void frame_shutdown(frame_t *self);

/* Convenience accessors for app code. */
focus_manager_t *frame_focus(frame_t *self);
quit_manager_t  *frame_quit (frame_t *self);

#endif /* DCS_FW_FRAME_H */
