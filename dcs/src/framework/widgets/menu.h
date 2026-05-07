#ifndef DCS_FW_MENU_H
#define DCS_FW_MENU_H

#include "widget.h"
#include <stdint.h>

#define MENU_MAX_ITEMS 16

typedef void (*menu_on_select_t)(int index, void *user);

class tagt_menu_item {
    char label[48];
    char shortcut[16];
};
typedef class tagt_menu_item menu_item_t;

class tagt_menu {
    widget_t    base;
    /* trigger button (always rendered; the widget's bounds equals trigger_rect
       when closed, and expands to cover the parent when open so clicks
       outside the dropdown route here and close it). */
    rect_t      trigger_rect;
    char        trigger_label[32];
    /* dropdown items */
    menu_item_t items[MENU_MAX_ITEMS];
    int         item_count;
    int         item_h;
    int         menu_w;
    int         hovered_item;          /* -1 if none */
    int         open;
    /* callbacks */
    menu_on_select_t on_select;
    void            *select_user;
    /* visuals */
    uint32_t    trigger_bg, trigger_bg_hover, trigger_bg_active;
    uint32_t    trigger_border, text_color;
    uint32_t    item_bg, item_bg_hover, item_text;
};
typedef class tagt_menu menu_t;

menu_t *menu_create(rect_t trigger_rect, const char *label);
/* Returns the index of the newly-added item, or -1 if the menu is full
   (MENU_MAX_ITEMS reached). */
int     menu_add_item(menu_t *self, const char *label, const char *shortcut);
void    menu_set_on_select(menu_t *self, menu_on_select_t cb, void *user);

#endif /* DCS_FW_MENU_H */
