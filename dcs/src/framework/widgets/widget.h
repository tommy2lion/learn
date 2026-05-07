#ifndef DCS_FW_WIDGET_H
#define DCS_FW_WIDGET_H

#include "../core/oo.h"
#include "../core/rect.h"
#include "../core/message.h"
#include "../graphics/igraph.h"

typedef struct tagt_widget widget_t;

/* Vtable: every widget exposes this. Unused slots may be NULL — the
   framework checks before dispatching. */
class tagt_widget_vt {
    void  (*draw)        (widget_t *self, igraph_t *g);
    int   (*handle_event)(widget_t *self, const event_t *ev);  /* return 1 if consumed */
    void  (*on_focus)    (widget_t *self);
    void  (*on_blur)     (widget_t *self);
    void  (*destroy)     (widget_t *self);

    /* Container support. NULL for leaf widgets. Children are returned in
       z-order: child_at(0) is bottom, child_at(n-1) is top (drawn last). */
    int        (*child_count)(widget_t *self);
    widget_t * (*child_at)   (widget_t *self, int idx);
};
typedef class tagt_widget_vt widget_vt_t;

class tagt_widget {
    const widget_vt_t *vt;
    rect_t   bounds;          /* in screen coords */
    widget_t *parent;
    int      visible;
    int      dirty;           /* reserved for Phase 2.7 partial refresh — UNUSED */
    void    *user;
};

/* Convenience polymorphic helpers — safe to call with NULL or a widget
   whose vt slot is NULL. */

static inline void widget_draw(widget_t *w, igraph_t *g) {
    if (w && w->visible && w->vt && w->vt->draw) w->vt->draw(w, g);
}

static inline int widget_handle_event(widget_t *w, const event_t *ev) {
    return (w && w->vt && w->vt->handle_event) ? w->vt->handle_event(w, ev) : 0;
}

static inline void widget_destroy(widget_t *w) {
    if (w && w->vt && w->vt->destroy) w->vt->destroy(w);
}

static inline int widget_child_count(widget_t *w) {
    return (w && w->vt && w->vt->child_count) ? w->vt->child_count(w) : 0;
}

static inline widget_t *widget_child_at(widget_t *w, int i) {
    return (w && w->vt && w->vt->child_at) ? w->vt->child_at(w, i) : NULL;
}

/* Hit-test: returns 1 if pt is inside w's bounds (and w is visible). */
static inline int widget_hits(const widget_t *w, vec2_t pt) {
    if (!w || !w->visible) return 0;
    return pt.x >= w->bounds.x && pt.x < w->bounds.x + w->bounds.w
        && pt.y >= w->bounds.y && pt.y < w->bounds.y + w->bounds.h;
}

#endif /* DCS_FW_WIDGET_H */
