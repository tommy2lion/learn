#ifndef DCS_APP_DIVIDER_WIDGET_H
#define DCS_APP_DIVIDER_WIDGET_H

#include "../framework/widgets/widget.h"

typedef enum {
    DIVIDER_HORIZONTAL,   /* a horizontal line; drag changes a Y coordinate */
    DIVIDER_VERTICAL,     /* a vertical line;   drag changes an X coordinate */
} divider_orient_t;

/* Fired during drag with the new mouse coordinate (Y for horizontal lines,
   X for vertical lines), already clamped to [value_min, value_max]. The
   callback owns interpreting that coordinate (e.g., "Y is the new top-edge
   of the bottom panel; update panel_h and re-layout"). */
typedef void (*divider_change_fn_t)(int new_value, void *user);

class tagt_divider_widget {
    widget_t            base;        /* bounds = the HOVER band, not the visible line */
    divider_orient_t    orient;
    int                 value_min, value_max;
    divider_change_fn_t on_change;
    void               *user;
    int                 dragging;
};
typedef class tagt_divider_widget divider_widget_t;

divider_widget_t *divider_widget_create(rect_t hover_bounds, divider_orient_t orient);
void  divider_widget_set_range    (divider_widget_t *self, int min_v, int max_v);
void  divider_widget_set_change_cb(divider_widget_t *self, divider_change_fn_t cb, void *user);

#endif /* DCS_APP_DIVIDER_WIDGET_H */
