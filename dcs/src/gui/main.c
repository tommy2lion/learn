/* dcs_gui — Phase 2.5 entry point. Almost no logic here: build platform
 * and graphics interfaces, hand them to dcs_app, run the loop. */

#include "../framework/platform/iplatform.h"
#include "../framework/graphics/igraph.h"
#include "../app/dcs_app.h"
#include <stdio.h>

int main(int argc, char **argv) {
    iplatform_t *p = platform_create();
    igraph_t    *g = graph_create();
    if (!p || !g) { fprintf(stderr, "init failed\n"); return 1; }
    if (g->init(g->self, 1280, 800, "DCS") != 0) {
        fprintf(stderr, "graph init failed\n");
        return 1;
    }

    dcs_app_t app;
    dcs_app_init(&app, p, g, argc >= 2 ? argv[1] : NULL);
    dcs_app_run(&app);
    dcs_app_release(&app);
    g->shutdown(g->self);
    return 0;
}
