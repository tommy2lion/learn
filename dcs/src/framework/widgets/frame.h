#ifndef DCS_FW_FRAME_H
#define DCS_FW_FRAME_H

#include "../core/oo.h"
#include "../core/focus_manager.h"
#include "../core/quit_manager.h"
#include "../graphics/igraph.h"
#include "../platform/iplatform.h"
#include "widget.h"

typedef void (*frame_resize_fn_t)(int new_w, int new_h, void *user);

class tagt_frame {
    igraph_t        *graph;
    iplatform_t     *platform;
    widget_t        *root;
    focus_manager_t  focus;
    quit_manager_t   quit;

    /* window-size tracking; on_resize fires when the screen size changes
       between ticks. Initial size on the first tick does NOT fire the
       callback (so apps can build their initial layout themselves). */
    int               last_w, last_h;
    frame_resize_fn_t on_resize;
    void             *resize_user;
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

/* Register a callback invoked whenever the window dimensions change
   between ticks. NULL clears the callback. */
void frame_set_resize_cb(frame_t *self, frame_resize_fn_t cb, void *user);

#endif /* DCS_FW_FRAME_H */
