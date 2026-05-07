#ifndef DCS_FW_MESSAGE_H
#define DCS_FW_MESSAGE_H

#include "oo.h"
#include "rect.h"
#include "../graphics/igraph.h"  /* for igraph_key_t / igraph_mouse_btn_t */

/* Event kinds. Edge events (*_PRESS) are derived inside frame_dispatch
   by comparing this frame's igraph snapshot to the previous frame's. */
typedef enum {
    EV_MOUSE_MOVE,                /* fired every frame the cursor moves        */
    EV_MOUSE_DOWN,                /* held: a button is held this frame         */
    EV_MOUSE_UP,                  /* held: button is up                        */
    EV_MOUSE_PRESS,               /* edge: up→down transition (fired once)     */
    EV_MOUSE_RELEASE,             /* edge: down→up transition                  */
    EV_MOUSE_WHEEL,               /* fired when scroll wheel moves             */
    EV_KEY_DOWN,                  /* held: key is held this frame              */
    EV_KEY_UP,                    /* held: key is up                           */
    EV_KEY_PRESS,                 /* edge: up→down transition                  */
    EV_RESIZE,
    EV_FOCUS_IN, EV_FOCUS_OUT,
} event_kind_t;

/* Modifier bitmask for both mouse and key events. */
#define MOD_CTRL   (1u << 0)
#define MOD_SHIFT  (1u << 1)
#define MOD_ALT    (1u << 2)

class tagt_event {
    event_kind_t kind;
    union {
        struct { vec2_t pos; igraph_mouse_btn_t btn; unsigned mods; } mouse;
        struct { float dy; vec2_t pos; }                              wheel;
        struct { igraph_key_t key; unsigned mods; }                   key;
        struct { int w; int h; }                                      resize;
    };
};
typedef class tagt_event event_t;

#endif /* DCS_FW_MESSAGE_H */
