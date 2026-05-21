#ifndef DCS_APP_HELP_DIALOG_H
#define DCS_APP_HELP_DIALOG_H

#include "../framework/widgets/widget.h"

/* Modal help dialog (R-14). When visible, captures every mouse event over
   its full-screen bounds so the underlying canvas / menus / toolbar are
   inert; click outside the centred box closes it. Keyboard close (F1
   toggle / ESC) is handled globally by dcs_app's poll_global_shortcuts
   because frame.c's focused-widget key dispatch is unused (R-18).

   The widget must be added LAST to the root panel so it sits on top in
   z-order. Bounds must equal the window's content rect; dcs_app's
   relayout() updates them on resize. */

class tagt_help_dialog {
    widget_t base;
    /* Computed at draw time from bounds — kept in the struct so
       handle_event can hit-test against the box without recomputing. */
    rect_t   box;
};
typedef class tagt_help_dialog help_dialog_t;

help_dialog_t *help_dialog_create(rect_t bounds);
void           help_dialog_show       (help_dialog_t *self);
void           help_dialog_hide       (help_dialog_t *self);
int            help_dialog_is_visible (const help_dialog_t *self);
void           help_dialog_set_bounds (help_dialog_t *self, rect_t bounds);

#endif /* DCS_APP_HELP_DIALOG_H */
