/* widget_showcase — Phase 2.3 part 2 demo: splitter, canvas_widget, menu.
 *
 * Layout:
 *   ┌──────────────────────────────────┐
 *   │ [File]                           │  ← header w/ menu
 *   ├──────────────────────────────────┤
 *   │  Canvas (panable + zoomable)     │  ← top half
 *   │   ──── draggable splitter ────   │
 *   │  Status / instructions           │  ← bottom half
 *   └──────────────────────────────────┘
 */

#include "../src/framework/platform/iplatform.h"
#include "../src/framework/graphics/igraph.h"
#include "../src/framework/core/color.h"
#include "../src/framework/widgets/frame.h"
#include "../src/framework/widgets/panel.h"
#include "../src/framework/widgets/label.h"
#include "../src/framework/widgets/splitter.h"
#include "../src/framework/widgets/canvas_widget.h"
#include "../src/framework/widgets/menu.h"
#include <stdio.h>

static label_t *g_status;

static void on_menu_select(int idx, void *user) {
    (void)user;
    static const char *items[] = { "New", "Open...", "Save", "Save As..." };
    char buf[64];
    snprintf(buf, sizeof(buf), "Menu pick: %s", items[idx]);
    label_set_text(g_status, buf);
}

/* draw_world callback: paints in the canvas_widget's world coordinates,
   so panning and zooming affect what we see. */
static void draw_circuit_world(igraph_t *g, void *user) {
    (void)user;
    /* a cross of axes for orientation */
    g->draw_line(g->self, (vec2_t){-400, 0}, (vec2_t){400, 0}, 1.0f, COLOR_LIGHTGRAY);
    g->draw_line(g->self, (vec2_t){0, -300}, (vec2_t){0, 300}, 1.0f, COLOR_LIGHTGRAY);
    /* a few shapes */
    g->draw_rect      (g->self, (rect_t){-80, -40, 80, 80}, COLOR_BLUE);
    g->draw_rect_lines(g->self, (rect_t){-80, -40, 80, 80}, 2.0f, COLOR_BLACK);
    g->draw_text      (g->self, "world (0,0)", (vec2_t){10, -8}, 14.0f, COLOR_BLACK);
    g->draw_circle    (g->self, (vec2_t){180, 100}, 30.0f, COLOR_GREEN);
    g->draw_circle    (g->self, (vec2_t){-200, 80}, 20.0f, COLOR_RED);
    g->draw_text      (g->self, "middle-drag pan, wheel = zoom",
                       (vec2_t){-200, 200}, 16.0f, COLOR_DARKGRAY);
}

int main(void) {
    iplatform_t *p = platform_create();
    igraph_t    *g = graph_create();
    if (!p || !g) { fprintf(stderr, "init failed\n"); return 1; }
    if (g->init(g->self, 900, 650, "DCS Widget Showcase (Phase 2.3 part 2)") != 0)
        return 1;

    /* root panel covers the whole window */
    panel_t *root = panel_create((rect_t){0, 0, 900, 650});
    panel_set_background(root, COLOR_BG);

    /* canvas occupies the top half, status label the bottom */
    canvas_widget_t *canvas = canvas_widget_create(
        (rect_t){0, 30, 900, 400}, draw_circuit_world, NULL);
    canvas_widget_set_bg(canvas, 0xFAFAFAFFu);
    /* center the world origin in the canvas */
    canvas_widget_set_camera(canvas, (vec2_t){0, 0}, 1.0f);

    g_status = label_create((rect_t){0, 0, 900, 220},
                            "Drag the divider; pan/zoom the canvas; click File.",
                            16.0f, COLOR_DARKGRAY);
    label_set_align(g_status, LABEL_ALIGN_CENTER);

    splitter_t *split = splitter_create((rect_t){0, 30, 900, 620}, /*vertical=*/1, 400);
    splitter_set_min_sizes(split, 100, 80);
    splitter_set_children(split, &canvas->base, &g_status->base);

    panel_add_child(root, &split->base);

    /* header menu (added LAST so it's on top in z-order — clicks anywhere
       on the screen route to it once it's open) */
    menu_t *file = menu_create((rect_t){8, 4, 80, 22}, "File");
    menu_add_item(file, "New",        "Ctrl+N");
    menu_add_item(file, "Open...",    "Ctrl+O");
    menu_add_item(file, "Save",       "Ctrl+S");
    menu_add_item(file, "Save As...", "Ctrl+Shift+S");
    menu_set_on_select(file, on_menu_select, NULL);
    panel_add_child(root, &file->base);

    /* small header background under the menu so the screen doesn't look bare */
    /* (drawn by the panel's bg color above the splitter; nothing to do) */

    frame_t f;
    frame_init(&f, g, p, &root->base);
    frame_quit(&f)->esc_quits = 1;

    frame_run(&f);

    frame_shutdown(&f);
    g->shutdown(g->self);
    return 0;
}
