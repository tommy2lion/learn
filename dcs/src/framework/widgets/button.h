#ifndef DCS_FW_BUTTON_H
#define DCS_FW_BUTTON_H

#include "widget.h"
#include <stdint.h>

typedef void (*button_on_click_t)(void *user);

class tagt_button {
    widget_t base;
    char     label[64];
    float    font_size;
    uint32_t color_normal, color_hover, color_active;
    uint32_t color_text, color_border;
    int      hovered;
    int      pressed;
    button_on_click_t on_click;
    void    *click_user;
};
typedef class tagt_button button_t;

button_t *button_create(rect_t bounds, const char *label,
                        button_on_click_t on_click, void *user);
void      button_set_label(button_t *self, const char *label);
void      button_set_colors(button_t *self,
                            uint32_t normal, uint32_t hover, uint32_t active,
                            uint32_t text);

#endif /* DCS_FW_BUTTON_H */
