/* framework_demo — validates Phase 2.3 (part 1) end-to-end with a real
 * window: a button increments a counter shown in a label.
 *
 * Flow exercised:
 *   - graph_create + iplatform_create (Phases 2.1/2.2)
 *   - frame_init / frame_run (event poll + dispatch + draw)
 *   - panel_t composing button + label + label
 *   - button_t hover/press/release + on_click callback
 *   - label_t live update via label_set_text
 *   - quit_manager: ESC quits, X-button quits
 *
 * Run with: ./demo/framework_demo.exe   (close the window or press ESC)
 */

#include "../src/framework/platform/iplatform.h"
#include "../src/framework/graphics/igraph.h"
#include "../src/framework/core/color.h"
#include "../src/framework/widgets/frame.h"
#include "../src/framework/widgets/panel.h"
#include "../src/framework/widgets/button.h"
#include "../src/framework/widgets/label.h"
#include <stdio.h>

static int      g_count = 0;
static label_t *g_count_label;

static void on_click_count(void *user) {
    (void)user;
    g_count++;
    char buf[64];
    snprintf(buf, sizeof(buf), "Clicked %d time%s", g_count, g_count == 1 ? "" : "s");
    label_set_text(g_count_label, buf);
}

static void on_click_reset(void *user) {
    (void)user;
    g_count = 0;
    label_set_text(g_count_label, "Clicked 0 times");
}

int main(void) {
    iplatform_t *p = platform_create();
    igraph_t    *g = graph_create();
    if (!p || !g) { fprintf(stderr, "platform/graph init failed\n"); return 1; }

    if (g->init(g->self, 800, 600, "DCS Framework Demo") != 0) {
        fprintf(stderr, "graph->init failed\n");
        return 1;
    }

    /* widget tree:
       root panel (full window)
         ├── title label
         ├── counter label
         ├── "+1"   button
         └── "reset" button
     */
    panel_t *root = panel_create((rect_t){0, 0, 800, 600});
    panel_set_background(root, COLOR_BG);

    label_t *title = label_create((rect_t){0, 40, 800, 40},
                                  "DCS framework demo (Phase 2.3 part 1)",
                                  22.0f, COLOR_BLACK);
    label_set_align(title, LABEL_ALIGN_CENTER);

    g_count_label = label_create((rect_t){0, 110, 800, 30},
                                 "Clicked 0 times", 18.0f, COLOR_DARKGRAY);
    label_set_align(g_count_label, LABEL_ALIGN_CENTER);

    button_t *plus  = button_create((rect_t){320, 200, 160, 50}, "+1",
                                    on_click_count, NULL);
    button_t *reset = button_create((rect_t){320, 270, 160, 50}, "Reset",
                                    on_click_reset, NULL);
    button_set_colors(plus, COLOR_GREEN,
                      0x60B080FFu /* hover */,
                      0x308050FFu /* active */, COLOR_WHITE);
    button_set_colors(reset, COLOR_RED,
                      0xE05050FFu, 0xA02020FFu, COLOR_WHITE);

    label_t *hint = label_create((rect_t){0, 540, 800, 24},
                                 "ESC or X to quit", 14.0f, COLOR_GRAY);
    label_set_align(hint, LABEL_ALIGN_CENTER);

    panel_add_child(root, &title->base);
    panel_add_child(root, &g_count_label->base);
    panel_add_child(root, &plus->base);
    panel_add_child(root, &reset->base);
    panel_add_child(root, &hint->base);

    frame_t f;
    frame_init(&f, g, p, &root->base);
    frame_quit(&f)->esc_quits = 1;     /* demo: ESC closes the window      */

    frame_run(&f);

    frame_shutdown(&f);                /* recursively destroys widget tree */
    g->shutdown(g->self);
    return 0;
}
