#include "menu.h"
#include "../core/color.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── geometry ────────────────────────────────────────────────────── */

static rect_t dropdown_rect(const menu_t *m) {
    return (rect_t){
        m->trigger_rect.x,
        m->trigger_rect.y + m->trigger_rect.h,
        (float)m->menu_w,
        (float)(m->item_h * m->item_count),
    };
}

static int hit_trigger(const menu_t *m, vec2_t p) {
    return p.x >= m->trigger_rect.x && p.x < m->trigger_rect.x + m->trigger_rect.w
        && p.y >= m->trigger_rect.y && p.y < m->trigger_rect.y + m->trigger_rect.h;
}

/* Returns item index under p, or -1 if not over the dropdown. */
static int hit_item(const menu_t *m, vec2_t p) {
    if (!m->open || m->item_count == 0) return -1;
    rect_t r = dropdown_rect(m);
    if (p.x < r.x || p.x >= r.x + r.w) return -1;
    if (p.y < r.y || p.y >= r.y + r.h) return -1;
    int idx = (int)((p.y - r.y) / m->item_h);
    if (idx < 0 || idx >= m->item_count) return -1;
    return idx;
}

static void menu_set_open(menu_t *m, int open) {
    m->open = open;
    if (open) {
        /* Expand to parent's bounds so clicks outside the dropdown route
           here and close it. If no parent (testing), stay at trigger_rect. */
        m->base.bounds = m->base.parent ? m->base.parent->bounds : m->trigger_rect;
    } else {
        m->base.bounds = m->trigger_rect;
        m->hovered_item = -1;
    }
}

/* ── widget vtable ───────────────────────────────────────────────── */

static void menu_draw(widget_t *self, igraph_t *g) {
    menu_t *m = (menu_t *)self;

    /* trigger appearance */
    vec2_t mp = g->mouse_position(g->self);
    int trig_hover = hit_trigger(m, mp);
    uint32_t bg = m->open       ? m->trigger_bg_active
                : trig_hover    ? m->trigger_bg_hover
                                : m->trigger_bg;
    g->draw_rect      (g->self, m->trigger_rect, bg);
    g->draw_rect_lines(g->self, m->trigger_rect, 1.0f, m->trigger_border);
    float tw = g->measure_text(g->self, m->trigger_label, 14.0f);
    g->draw_text(g->self, m->trigger_label,
                 (vec2_t){
                     m->trigger_rect.x + (m->trigger_rect.w - tw) * 0.5f,
                     m->trigger_rect.y + (m->trigger_rect.h - 14.0f) * 0.5f,
                 },
                 14.0f, m->text_color);

    if (!m->open) return;

    /* dropdown */
    rect_t dd = dropdown_rect(m);
    g->draw_rect      (g->self, dd, m->item_bg);
    g->draw_rect_lines(g->self, dd, 1.0f, COLOR_GRAY);

    m->hovered_item = hit_item(m, mp);
    for (int i = 0; i < m->item_count; i++) {
        rect_t r = { dd.x, dd.y + i * m->item_h, dd.w, (float)m->item_h };
        if (i == m->hovered_item) g->draw_rect(g->self, r, m->item_bg_hover);
        g->draw_text(g->self, m->items[i].label,
                     (vec2_t){r.x + 12, r.y + (r.h - 14.0f) * 0.5f + 1},
                     14.0f, m->item_text);
        if (m->items[i].shortcut[0]) {
            float sw = g->measure_text(g->self, m->items[i].shortcut, 12.0f);
            g->draw_text(g->self, m->items[i].shortcut,
                         (vec2_t){r.x + r.w - sw - 10, r.y + (r.h - 12.0f) * 0.5f + 2},
                         12.0f, COLOR_GRAY);
        }
    }
}

static int menu_handle_event(widget_t *self, const event_t *ev) {
    menu_t *m = (menu_t *)self;

    if (ev->kind == EV_KEY_PRESS && ev->key.key == IK_ESCAPE && m->open) {
        menu_set_open(m, 0);
        return 1;
    }

    if (ev->kind == EV_MOUSE_PRESS && ev->mouse.btn == IM_LEFT) {
        vec2_t p = ev->mouse.pos;
        if (hit_trigger(m, p)) {
            menu_set_open(m, !m->open);
            return 1;
        }
        if (m->open) {
            int idx = hit_item(m, p);
            if (idx >= 0) {
                if (m->on_select) m->on_select(idx, m->select_user);
                menu_set_open(m, 0);
                return 1;
            }
            /* click outside the dropdown but inside our (expanded) bounds: close */
            menu_set_open(m, 0);
            return 1;
        }
    }
    return 0;
}

static void menu_destroy(widget_t *self) { free(self); }

static const widget_vt_t MENU_VT = {
    .draw         = menu_draw,
    .handle_event = menu_handle_event,
    .destroy      = menu_destroy,
};

/* ── public API ──────────────────────────────────────────────────── */

menu_t *menu_create(rect_t trigger_rect, const char *label) {
    menu_t *m = (menu_t *)calloc(1, sizeof(menu_t));
    if (!m) return NULL;
    m->base.vt      = &MENU_VT;
    m->base.bounds  = trigger_rect;
    m->base.visible = 1;
    m->trigger_rect = trigger_rect;
    snprintf(m->trigger_label, sizeof(m->trigger_label), "%s", label ? label : "");
    m->item_h       = 26;
    m->menu_w       = 200;
    m->hovered_item = -1;
    m->trigger_bg          = 0xDCDCDCFFu;
    m->trigger_bg_hover    = 0xC8C8C8FFu;
    m->trigger_bg_active   = 0xAACCEEFFu;
    m->trigger_border      = COLOR_GRAY;
    m->text_color          = COLOR_BLACK;
    m->item_bg             = 0xFAFAFAFFu;
    m->item_bg_hover       = 0xB4D2F0FFu;
    m->item_text           = COLOR_BLACK;
    return m;
}

int menu_add_item(menu_t *self, const char *label, const char *shortcut) {
    if (self->item_count >= MENU_MAX_ITEMS) return -1;
    int idx = self->item_count++;
    menu_item_t *it = &self->items[idx];
    snprintf(it->label,    sizeof(it->label),    "%s", label    ? label    : "");
    snprintf(it->shortcut, sizeof(it->shortcut), "%s", shortcut ? shortcut : "");
    return idx;
}

void menu_set_on_select(menu_t *self, menu_on_select_t cb, void *user) {
    self->on_select   = cb;
    self->select_user = user;
}
