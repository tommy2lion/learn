#ifndef DCS_FW_FOCUS_MANAGER_H
#define DCS_FW_FOCUS_MANAGER_H

#include "oo.h"

typedef struct tagt_widget widget_t;

class tagt_focus_manager {
    widget_t *current;
};
typedef class tagt_focus_manager focus_manager_t;

void      focus_manager_init(focus_manager_t *self);
/* Sets the focus, calling on_blur on the previous focus and on_focus on
   the new one. NULL is allowed (clears focus). */
void      focus_manager_set (focus_manager_t *self, widget_t *w);
widget_t *focus_manager_get (const focus_manager_t *self);

#endif /* DCS_FW_FOCUS_MANAGER_H */
