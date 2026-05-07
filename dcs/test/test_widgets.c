/* Offline structural tests for the Phase 2.3 widget framework.
 *
 * Doesn't open a window. Validates: vtables populated, child-list
 * mechanics on panel, hit-test math on widget_hits, button on_click
 * fires only on press-then-release within bounds, focus_manager
 * blur/focus call ordering, quit_manager request/should_quit. */

#include "../src/framework/widgets/widget.h"
#include "../src/framework/widgets/panel.h"
#include "../src/framework/widgets/button.h"
#include "../src/framework/widgets/label.h"
#include "../src/framework/widgets/splitter.h"
#include "../src/framework/widgets/canvas_widget.h"
#include "../src/framework/widgets/menu.h"
#include "../src/framework/core/focus_manager.h"
#include "../src/framework/core/quit_manager.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* ── tracked button-click counter ─────────────────────────────────── */
static int g_clicks = 0;
static void on_click_inc(void *user) { (void)user; g_clicks++; }

/* ── tracked focus_manager spy widget ─────────────────────────────── */
static int g_focus_in = 0, g_focus_out = 0;
static void spy_on_focus(widget_t *self) { (void)self; g_focus_in++;  }
static void spy_on_blur (widget_t *self) { (void)self; g_focus_out++; }
static const widget_vt_t SPY_VT = { .on_focus = spy_on_focus, .on_blur = spy_on_blur };

/* mock igraph that returns "should_close = 0" — sufficient for quit_manager test */
static int  mock_should_close(void *self)               { (void)self; return 0; }
static int  mock_key_pressed (void *self, igraph_key_t k){ (void)self; (void)k; return 0; }

/* menu on_select stub */
static int g_picked = -1;
static void picked_callback(int idx, void *user) { (void)user; g_picked = idx; }

int main(void) {
    printf("=== widget framework offline tests ===\n");

    /* ── widget_hits ──────────────────────────────────────────── */
    {
        widget_t w = { .bounds = {10, 20, 100, 50}, .visible = 1 };
        check("hits inside",        widget_hits(&w, (vec2_t){50,  40}) == 1);
        check("hits at top-left",   widget_hits(&w, (vec2_t){10,  20}) == 1);
        check("misses just past",   widget_hits(&w, (vec2_t){110, 70}) == 0);
        w.visible = 0;
        check("invisible doesn't hit", widget_hits(&w, (vec2_t){50, 40}) == 0);
    }

    /* ── panel: children + recursive destroy ──────────────────── */
    {
        panel_t *root = panel_create((rect_t){0, 0, 200, 200});
        button_t *b1  = button_create((rect_t){10, 10, 50, 30}, "a", NULL, NULL);
        label_t  *l1  = label_create ((rect_t){10, 50, 50, 30}, "x", 16, 0xFF);
        panel_add_child(root, &b1->base);
        panel_add_child(root, &l1->base);
        check("panel child_count==2", widget_child_count(&root->base) == 2);
        check("panel child_at[0]==button", widget_child_at(&root->base, 0) == &b1->base);
        check("panel child_at[1]==label",  widget_child_at(&root->base, 1) == &l1->base);
        check("button.parent == panel", b1->base.parent == &root->base);
        widget_destroy(&root->base);   /* should free b1, l1, root without leaks */
    }

    /* ── button: press+release inside fires on_click ──────────── */
    {
        g_clicks = 0;
        button_t *b = button_create((rect_t){0, 0, 100, 40}, "go", on_click_inc, NULL);
        event_t press = { .kind = EV_MOUSE_PRESS };
        press.mouse.pos = (vec2_t){50, 20}; press.mouse.btn = IM_LEFT;
        widget_handle_event(&b->base, &press);
        event_t rel   = { .kind = EV_MOUSE_RELEASE };
        rel.mouse.pos = (vec2_t){50, 20}; rel.mouse.btn = IM_LEFT;
        widget_handle_event(&b->base, &rel);
        check("button click in-bounds fires on_click", g_clicks == 1);

        /* press inside, release outside → no click */
        event_t rel_out = { .kind = EV_MOUSE_RELEASE };
        rel_out.mouse.pos = (vec2_t){500, 500}; rel_out.mouse.btn = IM_LEFT;
        widget_handle_event(&b->base, &press);
        widget_handle_event(&b->base, &rel_out);
        check("button release outside doesn't fire", g_clicks == 1);

        widget_destroy(&b->base);
    }

    /* ── label_set_text round-trips ───────────────────────────── */
    {
        label_t *l = label_create((rect_t){0,0,100,30}, "hi", 14, 0xFF);
        label_set_text(l, "world");
        check("label_set_text", strcmp(l->text, "world") == 0);
        widget_destroy(&l->base);
    }

    /* ── focus_manager calls on_blur then on_focus ────────────── */
    {
        widget_t w1 = { .vt = &SPY_VT, .visible = 1 };
        widget_t w2 = { .vt = &SPY_VT, .visible = 1 };
        focus_manager_t fm;
        focus_manager_init(&fm);
        g_focus_in = g_focus_out = 0;

        focus_manager_set(&fm, &w1);
        check("set to w1 → 1 focus_in",         g_focus_in  == 1);
        check("set to w1 → 0 focus_out",        g_focus_out == 0);

        focus_manager_set(&fm, &w2);
        check("switch to w2 → 1 focus_out",     g_focus_out == 1);
        check("switch to w2 → 2 focus_in",      g_focus_in  == 2);

        focus_manager_set(&fm, NULL);
        check("clear → 2 focus_out",            g_focus_out == 2);
        check("clear → focus is NULL",          focus_manager_get(&fm) == NULL);
    }

    /* ── quit_manager basics ──────────────────────────────────── */
    {
        igraph_t mock_g = {
            .self = NULL,
            .should_close = mock_should_close,
            .key_pressed  = mock_key_pressed,
        };
        quit_manager_t qm;
        quit_manager_init(&qm);
        check("quit clean: not yet quitting", quit_manager_should_quit(&qm, &mock_g) == 0);
        quit_manager_request(&qm);
        check("quit after request",           quit_manager_should_quit(&qm, &mock_g) == 1);
    }

    /* ── splitter children + size accessors + min clamping ────── */
    {
        splitter_t *s = splitter_create((rect_t){0, 0, 400, 300}, /*vertical=*/1, 100);
        button_t   *t = button_create((rect_t){0,0,1,1}, "top",    NULL, NULL);
        label_t    *b = label_create ((rect_t){0,0,1,1}, "bottom", 14, 0xFF);
        splitter_set_children(s, &t->base, &b->base);
        check("splitter has 2 children",         widget_child_count(&s->base) == 2);
        check("splitter first_size==100",        splitter_get_first_size(s) == 100);
        splitter_set_min_sizes(s, 50, 50);
        splitter_set_first_size(s, 10);
        /* clamp happens during draw/layout — nothing exercised it yet, so just
           ensure the setter stored the raw value. */
        check("splitter set_first_size raw",     splitter_get_first_size(s) == 10);
        widget_destroy(&s->base);  /* should free top + bottom + s */
    }

    /* ── canvas_widget camera math ────────────────────────────── */
    {
        canvas_widget_t *c = canvas_widget_create((rect_t){50, 80, 200, 150}, NULL, NULL);
        /* default: target (0,0), offset = (50,80), zoom 1 → world == screen-offset */
        vec2_t w = canvas_widget_screen_to_world(c, (vec2_t){75, 90});
        check("default cam screen->world", w.x == 25.0f && w.y == 10.0f);
        vec2_t s2 = canvas_widget_world_to_screen(c, (vec2_t){25, 10});
        check("default cam world->screen", s2.x == 75.0f && s2.y == 90.0f);
        /* zoom 2.0, target (100,200) */
        canvas_widget_set_camera(c, (vec2_t){100, 200}, 2.0f);
        s2 = canvas_widget_world_to_screen(c, (vec2_t){100, 200});
        check("zoomed cam: target maps to offset", s2.x == 50.0f && s2.y == 80.0f);
        widget_destroy(&c->base);
    }

    /* ── menu open / select / close via synthetic events ──────── */
    {
        g_picked = -1;
        menu_t *m = menu_create((rect_t){10, 10, 80, 24}, "File");
        menu_add_item(m, "New",  "Ctrl+N");
        menu_add_item(m, "Open", "Ctrl+O");
        menu_add_item(m, "Save", "Ctrl+S");
        check("menu item_count==3",      m->item_count == 3);
        check("menu starts closed",      m->open == 0);

        /* parent ref so menu_set_open knows the expanded bounds */
        widget_t fake_parent = { .bounds = {0, 0, 800, 600}, .visible = 1 };
        m->base.parent = &fake_parent;

        menu_set_on_select(m, picked_callback, NULL);

        /* click trigger → opens */
        event_t pr_trig = { .kind = EV_MOUSE_PRESS };
        pr_trig.mouse.pos = (vec2_t){50, 22}; pr_trig.mouse.btn = IM_LEFT;
        widget_handle_event(&m->base, &pr_trig);
        check("menu opens after trigger click", m->open == 1);

        /* click 2nd item (idx=1).  Dropdown starts at y = 10+24 = 34, item_h=26 →
           item 1 covers y=60..86. */
        event_t pr_item = { .kind = EV_MOUSE_PRESS };
        pr_item.mouse.pos = (vec2_t){30, 70};
        pr_item.mouse.btn = IM_LEFT;
        widget_handle_event(&m->base, &pr_item);
        check("menu fired callback with idx=1", g_picked == 1);
        check("menu closed after select",       m->open == 0);

        /* re-open and close via a click far outside the dropdown */
        widget_handle_event(&m->base, &pr_trig);
        check("menu re-opens",                  m->open == 1);
        event_t pr_outside = { .kind = EV_MOUSE_PRESS };
        pr_outside.mouse.pos = (vec2_t){700, 500};
        pr_outside.mouse.btn = IM_LEFT;
        widget_handle_event(&m->base, &pr_outside);
        check("click outside closes menu",      m->open == 0);

        widget_destroy(&m->base);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures;
}
