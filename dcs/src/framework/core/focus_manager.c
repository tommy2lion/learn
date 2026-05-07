#include "focus_manager.h"
#include "../widgets/widget.h"

void focus_manager_init(focus_manager_t *self) {
    self->current = NULL;
}

void focus_manager_set(focus_manager_t *self, widget_t *w) {
    if (self->current == w) return;
    if (self->current && self->current->vt && self->current->vt->on_blur)
        self->current->vt->on_blur(self->current);
    self->current = w;
    if (w && w->vt && w->vt->on_focus)
        w->vt->on_focus(w);
}

widget_t *focus_manager_get(const focus_manager_t *self) {
    return self->current;
}
