#ifndef DCS_FW_PANEL_H
#define DCS_FW_PANEL_H

#include "widget.h"
#include <stdint.h>

/* Simple absolute-positioning container. Children are drawn back-to-front
   (last added = topmost) and hit-tested top-to-bottom. */

class tagt_panel {
    widget_t  base;
    widget_t **children;
    int       child_count;
    int       child_cap;
    uint32_t  bg_color;       /* 0 == transparent (no background drawn)  */
    uint32_t  border_color;   /* 0 == no border                           */
    float     border_thick;
};
typedef class tagt_panel panel_t;

panel_t *panel_create(rect_t bounds);
void     panel_set_background(panel_t *self, uint32_t color);
void     panel_set_border    (panel_t *self, uint32_t color, float thick);
/* Adds child; takes ownership (panel_destroy will free it). */
void     panel_add_child     (panel_t *self, widget_t *child);

#endif /* DCS_FW_PANEL_H */
