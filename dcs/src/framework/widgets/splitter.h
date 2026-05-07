#ifndef DCS_FW_SPLITTER_H
#define DCS_FW_SPLITTER_H

#include "widget.h"
#include <stdint.h>

/* A two-pane container with a draggable divider between them.
   - vertical=1: panes stacked vertically; divider is horizontal; first_size = first child's HEIGHT.
   - vertical=0: panes side-by-side;     divider is vertical;   first_size = first child's WIDTH.
   The widget owns its children (recursively destroyed). */

class tagt_splitter {
    widget_t   base;
    int        vertical;
    widget_t  *first;
    widget_t  *second;
    int        first_size;        /* px allocated to first child along the split axis */
    int        divider_thick;
    int        min_first;
    int        min_second;
    int        dragging;
    uint32_t   divider_color;
    uint32_t   divider_hover_color;
};
typedef class tagt_splitter splitter_t;

splitter_t *splitter_create(rect_t bounds, int vertical, int initial_first_size);
void        splitter_set_children(splitter_t *self, widget_t *first, widget_t *second);
void        splitter_set_min_sizes(splitter_t *self, int min_first, int min_second);
void        splitter_set_first_size(splitter_t *self, int size);
int         splitter_get_first_size(const splitter_t *self);

#endif /* DCS_FW_SPLITTER_H */
