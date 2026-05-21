/* App-layer scaffold tests for dcs_app (U-5 / Stage 3).

   Full init/release with stub iplatform + igraph — no raylib window
   actually opens. Tests:
     - lifecycle: init + release without leak
     - command_stack starts empty
     - last_snapshot is seeded after init
     - dcs_app_undo / dcs_app_redo on an empty stack are no-ops

   Scope kept small per the Stage 3 "scaffold" directive — incremental
   refinement adds more cases. Tests that need a fully-functional GUI
   (mouse events into widget tree, etc.) stay in the existing
   test_circuit_canvas_supplement.c. */

#include "../src/app/dcs_app.h"
#include "../src/framework/platform/iplatform.h"
#include "../src/framework/graphics/igraph.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;
static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* ── stub iplatform ────────────────────────────────────────────── */

static int      sp_open_file (void *s, void *o, const char *t, char *out, int m) {
    (void)s; (void)o; (void)t; (void)out; (void)m; return 0;
}
static int      sp_save_file (void *s, void *o, const char *t, char *out, int m) {
    (void)s; (void)o; (void)t; (void)out; (void)m; return 0;
}
static char    *sp_read_file (void *s, const char *p, int *l) {
    (void)s; (void)p; if (l) *l = 0; return NULL;     /* "file not found" */
}
static int      sp_write_file(void *s, const char *p, const char *b, int l) {
    (void)s; (void)p; (void)b; (void)l; return 0;
}
static uint64_t sp_time_ms   (void *s) { (void)s; return 0; }
static int      sp_set_clip  (void *s, const char *t) { (void)s; (void)t; return 0; }
static int      sp_get_clip  (void *s, char *o, int m) { (void)s; (void)o; (void)m; return -1; }
static dialog_result_t sp_confirm(void *s, void *o, const char *t, const char *m) {
    (void)s; (void)o; (void)t; (void)m; return DLG_NO;
}

static iplatform_t STUB_PLATFORM = {
    .self                  = NULL,
    .open_file             = sp_open_file,
    .save_file             = sp_save_file,
    .read_file             = sp_read_file,
    .write_file            = sp_write_file,
    .time_ms               = sp_time_ms,
    .set_clipboard         = sp_set_clip,
    .get_clipboard         = sp_get_clip,
    .confirm_yes_no_cancel = sp_confirm,
};

/* ── stub igraph ───────────────────────────────────────────────── */

static int sg_title_set_count = 0;
static void sg_screen_size(void *s, int *w, int *h) { (void)s; if (w) *w = 1280; if (h) *h = 800; }
static void sg_set_title  (void *s, const char *t) { (void)s; (void)t; sg_title_set_count++; }
static void *sg_native    (void *s) { (void)s; return NULL; }

/* Every other igraph function is unused during init/release; leave NULL.
   The few mouse / draw / key / scissor / camera slots are only invoked
   from frame_tick / widget_draw — not exercised in this test file. */

static igraph_t STUB_GRAPH = {
    .self                     = NULL,
    .screen_size              = sg_screen_size,
    .set_window_title         = sg_set_title,
    .get_native_window_handle = sg_native,
};

int main(void) {
    printf("=== dcs_app scaffold tests ===\n");

    /* ── init + release lifecycle ──────────────────────────────── */
    {
        sg_title_set_count = 0;
        dcs_app_t app;
        dcs_app_init(&app, &STUB_PLATFORM, &STUB_GRAPH, NULL);
        check("init: circuit non-NULL",          app.circuit != NULL);
        check("init: dirty starts 0",            app.dirty == 0);
        check("init: command_stack empty",       command_stack_undo_count(&app.cmds) == 0);
        check("init: redo stack empty",          command_stack_redo_count(&app.cmds) == 0);
        check("init: last_snapshot seeded",      app.last_snapshot != NULL);
        check("init: title was pushed",          sg_title_set_count >= 1);
        check("init: file_path defaults to untitled.dcs",
              strcmp(app.file_path, "untitled.dcs") == 0);
        check("init: path_is_explicit == 0",     app.path_is_explicit == 0);
        dcs_app_release(&app);
        check("release: circuit cleared",        app.circuit == NULL);
        check("release: last_snapshot freed",    app.last_snapshot == NULL);
    }

    /* ── dcs_app_undo on empty stack is a no-op ──────────────── */
    {
        dcs_app_t app;
        dcs_app_init(&app, &STUB_PLATFORM, &STUB_GRAPH, NULL);
        dcs_app_undo(&app);                /* must not crash */
        check("undo on empty stack: safe", 1);
        check("undo on empty stack: still 0 entries",
              command_stack_undo_count(&app.cmds) == 0);
        dcs_app_release(&app);
    }

    /* ── dcs_app_redo on empty stack is a no-op ──────────────── */
    {
        dcs_app_t app;
        dcs_app_init(&app, &STUB_PLATFORM, &STUB_GRAPH, NULL);
        dcs_app_redo(&app);
        check("redo on empty stack: safe", 1);
        check("redo on empty stack: still 0 entries",
              command_stack_redo_count(&app.cmds) == 0);
        dcs_app_release(&app);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
