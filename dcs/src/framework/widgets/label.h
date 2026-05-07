#ifndef DCS_FW_LABEL_H
#define DCS_FW_LABEL_H

#include "widget.h"
#include <stdint.h>

typedef enum {
    LABEL_ALIGN_LEFT,
    LABEL_ALIGN_CENTER,
    LABEL_ALIGN_RIGHT,
} label_align_t;

class tagt_label {
    widget_t      base;
    char          text[256];
    float         font_size;
    uint32_t      color;
    label_align_t align;
};
typedef class tagt_label label_t;

label_t *label_create   (rect_t bounds, const char *text, float size, uint32_t color);
void     label_set_text (label_t *self, const char *text);
void     label_set_align(label_t *self, label_align_t a);

#endif /* DCS_FW_LABEL_H */
