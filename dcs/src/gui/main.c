/* dcs_gui — Phase 2.5 entry point. Almost no logic here: build platform
 * and graphics interfaces, hand them to dcs_app, run the loop. */

#include "../framework/platform/iplatform.h"
#include "../framework/graphics/igraph.h"
#include "../app/dcs_app.h"
#include "../version.h"
#include "../build_info.h"
#include "../sha256.h"
#include <stdio.h>
#include <string.h>

/* R-15 part 3: verify the build tamper signature at startup. Recomputes
   SHA-256(version | date | commit | salt) and compares to dcs_build_sig.
   Mismatch is logged loudly to stderr (per CLAUDE.md §9.4) but does NOT
   abort — the binary still runs. Salt is embedded so this catches casual
   tampering only; serious attackers just recompute. */
static void verify_build_signature(void) {
    char concat[512];
    int n = snprintf(concat, sizeof(concat), "%s|%s|%s|%s",
                     DCS_VERSION_STR, dcs_build_date,
                     dcs_build_commit, dcs_build_salt);
    if (n <= 0 || n >= (int)sizeof(concat)) {
        fprintf(stderr, "WARNING: dcs build signature check skipped "
                        "(concat buffer too small: n=%d)\n", n);
        return;
    }
    uint8_t hash[32];
    char    hex[65];
    sha256((const uint8_t *)concat, (size_t)n, hash);
    sha256_hex(hash, hex);
    if (strcmp(hex, dcs_build_sig) != 0) {
        fprintf(stderr,
                "WARNING: dcs build signature mismatch\n"
                "  expected: %s\n"
                "  computed: %s\n"
                "  (binary may have been modified after build)\n",
                dcs_build_sig, hex);
    }
}

int main(int argc, char **argv) {
    verify_build_signature();

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
