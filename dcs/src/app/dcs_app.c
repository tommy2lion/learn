#include "dcs_app.h"
#include "../framework/core/color.h"
#include "../domain/circuit_io.h"
#include "../version.h"
#include "../build_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ── layout constants ────────────────────────────────────────────── */

#define HEADER_H               30
#define STATUS_H               24
#define DIVIDER_HOVER          10        /* hover band thickness in px */

#define SIDEBAR_W_DEFAULT     140        /* wider initial sidebar — fits SWEEP/etc */
#define SIDEBAR_W_MIN         110
#define BOTTOM_PANEL_H_DEFAULT 240
#define PANEL_H_MIN            80
#define CANVAS_H_MIN          100
#define RIGHT_W_MIN           240        /* minimum width for canvas/timing column */

/* ── status line ─────────────────────────────────────────────────── */

static void set_status(dcs_app_t *app, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(app->status_text, DCS_APP_STATUS_LEN, fmt, ap);
    va_end(ap);
    if (app->status_label) label_set_text(app->status_label, app->status_text);
}

static void on_canvas_status(const char *msg, void *user) {
    set_status((dcs_app_t *)user, "%s", msg);
}

/* Forward decl — extract_basename's body sits a few helpers down. */
static void extract_basename(const char *path, char *dst, int dst_len);

/* Returns 1 if the canvas's display_name is set AND differs from the file's
   basename (i.e. it overrides the basename rather than mirroring it).
   Writes the name into `out_dn` when 1; writes "" otherwise. Safe with a
   NULL / uninitialised canvas — returns 0. (R-1) */
static int has_custom_display_name(const dcs_app_t *app,
                                   char *out_dn, int out_len) {
    if (out_len > 0) out_dn[0] = '\0';
    if (!app->circuit_canvas) return 0;
    external_view_metadata_t *em =
        circuit_canvas_widget_external_meta(app->circuit_canvas);
    if (!em || !em->display_name[0]) return 0;
    char basename[DOMAIN_NAME_LEN];
    extract_basename(app->file_path, basename, sizeof(basename));
    if (strcmp(em->display_name, basename) == 0) return 0;
    snprintf(out_dn, out_len, "%s", em->display_name);
    return 1;
}

/* Build the window title from the current file basename + dirty flag and
   push it to the graphics layer. (R-10) Title formats:
     "DCS — <basename>"               clean, no display-name override
     "DCS — <basename> *"             dirty,  no display-name override
     "DCS — <basename> (<display>)"   clean, custom display name (R-1)
     "DCS — <basename> (<display>) *" dirty,  custom display name (R-1)
   The em-dash is intentional — keeps the title scannable. */
static void refresh_window_title(dcs_app_t *app) {
    if (!app->graph || !app->graph->set_window_title) return;
    char base[DOMAIN_NAME_LEN];
    extract_basename(app->file_path, base, sizeof(base));
    if (!base[0]) snprintf(base, sizeof(base), "%s", "untitled");
    char dn[DOMAIN_NAME_LEN];
    int has_dn = has_custom_display_name(app, dn, sizeof(dn));
    char title[DCS_APP_FILE_PATH_LEN + DOMAIN_NAME_LEN + 32];
    const char *star = app->dirty ? " *" : "";
    if (has_dn) {
        snprintf(title, sizeof(title),
                 "DCS \xe2\x80\x94 %s (%s)%s", base, dn, star);
    } else {
        snprintf(title, sizeof(title),
                 "DCS \xe2\x80\x94 %s%s", base, star);
    }
    app->graph->set_window_title(app->graph->self, title);
}

/* Single funnel for dirty-flag changes — keeps the title in sync. (R-10)
   Always refreshes the title (not just on a dirty-flag flip) because the
   title also depends on file_path and display_name, which can change
   while dirty stays 0 (e.g. opening a fresh file: dirty was 0, stays 0,
   but basename + display-name swap). SetWindowTitle is cheap enough that
   we don't need a change-detector here. */
static void dcs_app_set_dirty(dcs_app_t *app, int dirty) {
    app->dirty = dirty ? 1 : 0;
    refresh_window_title(app);
}

/* Canvas mutation hook — any structural / geometric change marks dirty
   AND (R-5) snapshots the post-mutation state into an undo command. */
static void on_canvas_mutated(void *user);

/* "<verb> <path>" + optional "(display: <name>)" when the file's
   @display_name overrides the basename. Used on open/save so the user
   sees the distinction at the moment they touch the file. (R-1) */
static void set_file_status(dcs_app_t *app, const char *verb, const char *path) {
    char dn[DOMAIN_NAME_LEN];
    if (has_custom_display_name(app, dn, sizeof(dn))) {
        set_status(app, "%s %s (display: %s)", verb, path, dn);
    } else {
        set_status(app, "%s %s", verb, path);
    }
}

/* Strip directory and ".dcs" extension. dst must be at least DOMAIN_NAME_LEN
   bytes. Empty-input safe — writes "" to dst. */
static void extract_basename(const char *path, char *dst, int dst_len) {
    if (!dst || dst_len <= 0) return;
    dst[0] = '\0';
    if (!path || !path[0]) return;
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    snprintf(dst, dst_len, "%s", base);
    int n = (int)strlen(dst);
    if (n >= 4 && strcmp(dst + n - 4, ".dcs") == 0) {
        dst[n - 4] = '\0';
    }
}

/* Extract the file basename (no directory, no .dcs extension) and feed it
   to the canvas for use as the external-view box label. (Phase 8) */
static void apply_display_name_from_path(dcs_app_t *app, const char *path) {
    char buf[DOMAIN_NAME_LEN];
    extract_basename(path, buf, sizeof(buf));
    circuit_canvas_widget_set_display_name(app->circuit_canvas, buf);
}

/* Pack the canvas's external metadata into a persistable circuit_meta_t
   for the save path. display_name is omitted (left empty) when it equals
   the file basename — that way renaming the file changes the visible
   label rather than the file silently re-asserting the old name. (Phase 10) */
static void pack_meta_for_save(const dcs_app_t *app, circuit_meta_t *meta) {
    memset(meta, 0, sizeof(*meta));
    meta->display_mode =
        (int)circuit_canvas_widget_display_mode(app->circuit_canvas);
    external_view_metadata_t *em =
        circuit_canvas_widget_external_meta(app->circuit_canvas);
    char basename[DOMAIN_NAME_LEN];
    extract_basename(app->file_path, basename, sizeof(basename));
    if (em->display_name[0] && strcmp(em->display_name, basename) != 0) {
        snprintf(meta->display_name, sizeof(meta->display_name),
                 "%s", em->display_name);
    }
    for (int i = 0; i < DOMAIN_MAX_IO; i++) {
        meta->input_styles[i]  = (int)em->input_styles[i];
        meta->output_styles[i] = (int)em->output_styles[i];
    }
}

/* ── file actions ────────────────────────────────────────────────── */

/* Copy parsed metadata into the canvas widget. Called after set_circuit
   (which resets external_meta to defaults), so any non-default value
   in the file overrides the defaults. (Phase 10) */
static void apply_meta_from_load(dcs_app_t *app, const circuit_meta_t *meta) {
    if (!meta) return;
    circuit_canvas_widget_set_display_mode(app->circuit_canvas,
        (display_mode_t)meta->display_mode);
    if (meta->display_name[0]) {
        /* File-supplied name overrides the basename. */
        circuit_canvas_widget_set_display_name(app->circuit_canvas,
                                               meta->display_name);
    }
    external_view_metadata_t *em =
        circuit_canvas_widget_external_meta(app->circuit_canvas);
    for (int i = 0; i < DOMAIN_MAX_IO; i++) {
        em->input_styles[i]  = (pin_style_t)meta->input_styles[i];
        em->output_styles[i] = (pin_style_t)meta->output_styles[i];
    }
}

/* Serialise the canvas's current state to a fresh malloc'd string. Used
   for undo snapshots — captures everything that round-trips through the
   .dcs format (gates, wires, layout, wire geometry, display mode/name,
   pin styles). Caller frees. (R-5) */
static char *serialize_snapshot(const dcs_app_t *app) {
    if (!app->circuit) return NULL;
    const wire_geometry_t *geom = circuit_canvas_widget_geometry(app->circuit_canvas);
    /* For internal snapshots we want the literal display_name (no
       basename-comparison strip — that's a save-to-file niceity). */
    circuit_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.display_mode =
        (int)circuit_canvas_widget_display_mode(app->circuit_canvas);
    external_view_metadata_t *em =
        circuit_canvas_widget_external_meta(app->circuit_canvas);
    snprintf(meta.display_name, sizeof(meta.display_name),
             "%s", em->display_name);
    for (int i = 0; i < DOMAIN_MAX_IO; i++) {
        meta.input_styles[i]  = (int)em->input_styles[i];
        meta.output_styles[i] = (int)em->output_styles[i];
    }
    return circuit_io_serialize_ex(app->circuit, geom, &meta);
}

/* Parse + install circuit text. Returns 0 on success, -1 on parse error.
   When path != NULL: treats this as a user-initiated file load — updates
   file_path, prints "Opened path" status, clears the undo/redo stacks
   (opening a new document discards the previous one's history). When
   path == NULL: silent snapshot restore (used by undo/redo) — file_path
   stays, stacks stay, status is set by the caller. (R-5) */
static int install_circuit_text(dcs_app_t *app, const char *text, const char *path) {
    char err[256] = {0};
    wire_geometry_t parsed_wires;
    wire_geometry_init(&parsed_wires);
    circuit_meta_t parsed_meta;
    memset(&parsed_meta, 0, sizeof(parsed_meta));
    circuit_t *c = circuit_io_parse_ex(text, err, sizeof(err),
                                       &parsed_wires, &parsed_meta);
    if (!c) {
        wire_geometry_release(&parsed_wires);
        if (path) set_status(app, "Parse error: %s", err);
        else      set_status(app, "Undo/redo parse error: %s", err);
        return -1;
    }
    if (app->circuit) circuit_destroy(app->circuit);
    app->circuit = c;
    int n_align_shifts = circuit_canvas_widget_auto_align(c);
    circuit_canvas_widget_set_circuit(app->circuit_canvas, c);
    if (n_align_shifts == 0 && parsed_wires.net_count > 0) {
        circuit_canvas_widget_load_geometry(app->circuit_canvas, &parsed_wires);
    }
    wire_geometry_release(&parsed_wires);
    input_panel_set_circuit(app->input_panel, c);
    if (path) {
        snprintf(app->file_path, sizeof(app->file_path), "%s", path);
        app->path_is_explicit = 1;
        apply_display_name_from_path(app, path);
    }
    apply_meta_from_load(app, &parsed_meta);
    if (path) {
        set_file_status(app, "Opened", path);
        dcs_app_set_dirty(app, 0);
        /* Opening a new document discards prior history. */
        command_stack_release(&app->cmds);
        command_stack_init(&app->cmds);
    } else {
        /* Snapshot restore — caller (dcs_app_undo/redo) handles dirty
           + status + title refresh. */
    }
    /* The snapshot cache always equals the current state. */
    free(app->last_snapshot);
    app->last_snapshot = serialize_snapshot(app);
    return 0;
}

/* Thin wrapper kept for call-site readability; identical to install with
   a non-NULL path. */
static void load_circuit_from_text(dcs_app_t *app, const char *path, const char *text) {
    (void)install_circuit_text(app, text, path);
}

/* ── R-5 snapshot command ────────────────────────────────────────── */

typedef struct {
    command_t  base;
    char      *before;
    char      *after;
    dcs_app_t *app;
} snapshot_cmd_t;

static void snapshot_execute(command_t *self) {
    snapshot_cmd_t *s = (snapshot_cmd_t *)self;
    (void)install_circuit_text(s->app, s->after, NULL);
}
static void snapshot_undo(command_t *self) {
    snapshot_cmd_t *s = (snapshot_cmd_t *)self;
    (void)install_circuit_text(s->app, s->before, NULL);
}
static void snapshot_destroy(command_t *self) {
    snapshot_cmd_t *s = (snapshot_cmd_t *)self;
    free(s->before);
    free(s->after);
    free(s);
}
static const char *snapshot_describe(const command_t *self) {
    (void)self;
    return "edit";
}

static const command_vt_t SNAPSHOT_VT = {
    .execute  = snapshot_execute,
    .undo     = snapshot_undo,
    .destroy  = snapshot_destroy,
    .describe = snapshot_describe,
};

/* Real on_canvas_mutated body (the forward-decl is up at line ~67).
   Builds a snapshot_cmd from the previously-cached state (the "before")
   plus the freshly-serialised current state (the "after"), pushes to
   the undo stack, and refreshes the cache. (R-5) */
static void on_canvas_mutated(void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    char *after = serialize_snapshot(app);
    if (!after) {
        /* Serialise failed — skip the snapshot but still set dirty so
           the user knows something changed. */
        dcs_app_set_dirty(app, 1);
        return;
    }
    if (app->last_snapshot) {
        snapshot_cmd_t *cmd = (snapshot_cmd_t *)calloc(1, sizeof(*cmd));
        if (cmd) {
            cmd->base.vt = &SNAPSHOT_VT;
            cmd->before  = app->last_snapshot;   /* transfer ownership   */
            cmd->after   = strdup(after);        /* cache also keeps `after` */
            cmd->app     = app;
            app->last_snapshot = NULL;           /* cache about to be replaced */
            command_stack_push(&app->cmds, &cmd->base);
        } else {
            free(app->last_snapshot);
            app->last_snapshot = NULL;
        }
    }
    app->last_snapshot = after;                  /* transfer; cache := after */
    dcs_app_set_dirty(app, 1);
}

static void action_new(dcs_app_t *app) {
    if (app->circuit) { circuit_destroy(app->circuit); app->circuit = NULL; }
    app->circuit = circuit_create();
    circuit_canvas_widget_set_circuit(app->circuit_canvas, app->circuit);
    input_panel_set_circuit(app->input_panel, app->circuit);
    snprintf(app->file_path, sizeof(app->file_path), "untitled.dcs");
    app->path_is_explicit = 0;
    apply_display_name_from_path(app, app->file_path);
    /* clear simulation results */
    simulation_release(&app->sim);
    simulation_init(&app->sim, app->circuit);
    timing_canvas_widget_set_waves(app->timing_canvas, NULL);
    set_status(app, "New circuit");
    dcs_app_set_dirty(app, 0);
    /* Reset undo/redo history; refresh snapshot cache to the new empty
       state so the next mutation builds a correct command. (R-5) */
    command_stack_release(&app->cmds);
    command_stack_init(&app->cmds);
    free(app->last_snapshot);
    app->last_snapshot = serialize_snapshot(app);
}

/* Native HWND/etc. of the program window, used to centre OS dialogs
   on our window rather than the screen. NULL when the backend doesn't
   expose one. */
static void *native_owner(const dcs_app_t *app) {
    return (app->graph && app->graph->get_native_window_handle)
        ? app->graph->get_native_window_handle(app->graph->self)
        : NULL;
}

static void action_open(dcs_app_t *app) {
    char path[DCS_APP_FILE_PATH_LEN] = {0};
    if (!app->platform->open_file(app->platform->self, native_owner(app),
                                  "Open .dcs", path, sizeof(path))) return;
    int len = 0;
    char *text = app->platform->read_file(app->platform->self, path, &len);
    if (!text) { set_status(app, "Cannot read %s", path); return; }
    load_circuit_from_text(app, path, text);
    free(text);
    /* clear simulation results */
    simulation_release(&app->sim);
    simulation_init(&app->sim, app->circuit);
    timing_canvas_widget_set_waves(app->timing_canvas, NULL);
}

static void action_save_as(dcs_app_t *app) {
    char path[DCS_APP_FILE_PATH_LEN] = {0};
    if (!app->platform->save_file(app->platform->self, native_owner(app),
                                  "Save .dcs", path, sizeof(path))) return;
    /* Update file_path BEFORE packing meta so the basename comparison
       in pack_meta_for_save uses the new filename. */
    snprintf(app->file_path, sizeof(app->file_path), "%s", path);
    app->path_is_explicit = 1;
    const wire_geometry_t *geom = circuit_canvas_widget_geometry(app->circuit_canvas);
    circuit_meta_t meta;
    pack_meta_for_save(app, &meta);
    char *text = circuit_io_serialize_ex(app->circuit, geom, &meta);
    if (!text) { set_status(app, "Serialize failed"); return; }
    int n = (int)strlen(text);
    int rc = app->platform->write_file(app->platform->self, path, text, n);
    free(text);
    if (rc != 0) { set_status(app, "Save failed"); return; }
    set_file_status(app, "Saved to", path);
    dcs_app_set_dirty(app, 0);
}

static void action_save(dcs_app_t *app) {
    if (!app->path_is_explicit) { action_save_as(app); return; }
    const wire_geometry_t *geom = circuit_canvas_widget_geometry(app->circuit_canvas);
    circuit_meta_t meta;
    pack_meta_for_save(app, &meta);
    char *text = circuit_io_serialize_ex(app->circuit, geom, &meta);
    if (!text) { set_status(app, "Serialize failed"); return; }
    int n = (int)strlen(text);
    int rc = app->platform->write_file(app->platform->self, app->file_path, text, n);
    free(text);
    if (rc == 0) {
        set_file_status(app, "Saved to", app->file_path);
        dcs_app_set_dirty(app, 0);
    } else {
        set_status(app, "Save failed");
    }
}

/* ── save-on-close prompt (R-11) ─────────────────────────────────── */

/* Quit-attempt gate: if the canvas is clean, allow the quit immediately.
   If dirty, ask the user. Yes -> save then allow (only if save succeeded —
   `dirty` remains 1 on serializer / write failure and on save-as cancel,
   either of which veto the quit). No -> discard and allow. Cancel / error
   -> veto. */
static int on_attempt_quit(void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    if (!app->dirty) return 1;
    if (!app->platform || !app->platform->confirm_yes_no_cancel) {
        /* Platform doesn't support a confirm dialog (e.g. Linux stub);
           safest default is to veto so the user can save manually. */
        set_status(app, "Unsaved changes — save before closing.");
        return 0;
    }
    char base[DOMAIN_NAME_LEN];
    extract_basename(app->file_path, base, sizeof(base));
    if (!base[0]) snprintf(base, sizeof(base), "%s", "untitled");
    char msg[256];
    snprintf(msg, sizeof(msg),
             "Save changes to %s before closing?", base);
    dialog_result_t r = app->platform->confirm_yes_no_cancel(
        app->platform->self, native_owner(app), "Unsaved changes", msg);
    if (r == DLG_YES) {
        action_save(app);
        return app->dirty ? 0 : 1;   /* save failure / save-as cancel → veto */
    }
    if (r == DLG_NO) return 1;       /* discard changes, allow quit */
    return 0;                        /* DLG_CANCEL or DLG_ERROR → veto */
}

/* ── menu callback ───────────────────────────────────────────────── */

static void on_menu_select(int idx, void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    switch (idx) {
        case 0: action_new    (app); break;
        case 1: action_open   (app); break;
        case 2: action_save   (app); break;
        case 3: action_save_as(app); break;
    }
}

/* Toggle the canvas's display mode. Single funnel called by every UX
   surface (menu, Ctrl+B, sidebar button); a future fourth surface plugs
   in with no change. (Phase 8) */
static void action_toggle_display_mode(dcs_app_t *app) {
    display_mode_t cur = circuit_canvas_widget_display_mode(app->circuit_canvas);
    display_mode_t next = (cur == DISPLAY_EXTERNAL) ? DISPLAY_INTERNAL : DISPLAY_EXTERNAL;
    circuit_canvas_widget_set_display_mode(app->circuit_canvas, next);
    set_status(app, next == DISPLAY_EXTERNAL ? "Black-box view" : "Schematic view");
    /* display_mode is persisted in the .dcs file, so toggling it is a
       user-driven mutation; mark dirty. The canvas's setter doesn't fire
       its own mutated_cb because the same setter is also called by
       apply_meta_from_load (file load path), which must NOT mark dirty. */
    dcs_app_set_dirty(app, 1);
}

static void on_view_menu_select(int idx, void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    if (idx == 0) action_toggle_display_mode(app);
}

static void on_edit_menu_select(int idx, void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    if (idx == 0)      dcs_app_undo(app);
    else if (idx == 1) dcs_app_redo(app);
}

static void on_help_menu_select(int idx, void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    if (idx == 0) help_dialog_show(app->help_dialog);
    else if (idx == 1) {
        /* About: show the full version-stamp in the status bar (R-15
           part 3). A proper modal can come later — this gets the user
           past "what version am I running?" with zero new widget code. */
        set_status(app, "DCS %s:%s:%s:%s",
                   DCS_VERSION_STR, dcs_build_date,
                   dcs_build_commit, dcs_build_sig);
    }
}

/* ── run / sweep ─────────────────────────────────────────────────── */

typedef struct tagt_stim_ctx { dcs_app_t *app; int sweep; } stim_ctx_t;

static signal_t stim_callback(int step, int input_idx, void *user) {
    stim_ctx_t *sc = (stim_ctx_t *)user;
    if (sc->sweep) return (signal_t)((step >> input_idx) & 1);
    /* static toggle values from input panel */
    const char *nm = sc->app->circuit->input_names[input_idx];
    return input_panel_value(sc->app->input_panel, nm);
}

static void on_run(int sweep_mode, void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    if (!app->circuit) { set_status(app, "No circuit"); return; }
    int n_in  = app->circuit->input_count;
    int steps;
    int sweep_full = 0;
    if (sweep_mode) {
        if (n_in <= 0) steps = 1;
        else if (n_in >= 30) steps = INPUT_PANEL_MAX_STEPS;
        else {
            int full = 1 << n_in;
            if (full <= INPUT_PANEL_MAX_STEPS) { steps = full; sweep_full = 1; }
            else                                steps = INPUT_PANEL_MAX_STEPS;
        }
    } else {
        steps = input_panel_steps(app->input_panel);
        if (steps < 1) steps = 1;
    }
    stim_ctx_t sc = { app, sweep_mode };
    simulation_run(&app->sim, steps, stim_callback, &sc);
    timing_canvas_widget_set_waves(app->timing_canvas, &app->sim.waves);
    if (sweep_mode) {
        if (sweep_full) set_status(app, "Swept all 2^%d = %d combinations", n_in, steps);
        else            set_status(app, "Swept first %d of 2^%d combinations", steps, n_in);
    } else {
        set_status(app, "Ran %d step%s", steps, steps == 1 ? "" : "s");
    }
}

/* ── global shortcut polling ─────────────────────────────────────── */

static void poll_global_shortcuts(dcs_app_t *app) {
    igraph_t *g = app->graph;
    int ctrl  = g->key_down(g->self, IK_LEFT_CTRL)  || g->key_down(g->self, IK_RIGHT_CTRL);
    int shift = g->key_down(g->self, IK_LEFT_SHIFT) || g->key_down(g->self, IK_RIGHT_SHIFT);
    /* Help dialog is modal — when open, only F1 / ESC close it; everything
       else is swallowed so the underlying canvas / menus stay inert (R-14). */
    if (help_dialog_is_visible(app->help_dialog)) {
        if (g->key_pressed(g->self, IK_F1) || g->key_pressed(g->self, IK_ESCAPE))
            help_dialog_hide(app->help_dialog);
        return;
    }
    /* F1 opens the help dialog. */
    if (!ctrl && g->key_pressed(g->self, IK_F1))
        help_dialog_show(app->help_dialog);
    if (ctrl && g->key_pressed(g->self, IK_N)) action_new(app);
    if (ctrl && g->key_pressed(g->self, IK_O)) action_open(app);
    if (ctrl && g->key_pressed(g->self, IK_S)) {
        if (shift) action_save_as(app);
        else       action_save   (app);
    }
    if (ctrl && g->key_pressed(g->self, IK_B)) action_toggle_display_mode(app);
    /* Ctrl+A selects every node (R-12). Routed through the global shortcut
       path so it fires regardless of which widget owns focus. */
    if (ctrl && g->key_pressed(g->self, IK_A))
        circuit_canvas_widget_select_all(app->circuit_canvas);
    /* Undo / Redo (R-5). Ctrl+Z = undo; Ctrl+Y = redo; Ctrl+Shift+Z is
       the muscle-memory alternate redo (common in editors). */
    if (ctrl && g->key_pressed(g->self, IK_Z)) {
        if (shift) dcs_app_redo(app);
        else       dcs_app_undo(app);
    }
    if (ctrl && g->key_pressed(g->self, IK_Y)) dcs_app_redo(app);
    /* Del deletes the current canvas selection (R-13). Global for the same
       reason as Ctrl+A — frame.c key-dispatch needs a focused widget and
       focus_manager_set is never called by any widget today. */
    if (!ctrl && g->key_pressed(g->self, IK_DELETE))
        circuit_canvas_widget_delete_selection(app->circuit_canvas);
    /* ESC cancels whatever canvas mode is active and clears the selection
       (R-18). Global for the same focus reason; the in-widget ESC handlers
       were dead code before this. */
    if (!ctrl && g->key_pressed(g->self, IK_ESCAPE))
        circuit_canvas_widget_cancel_mode(app->circuit_canvas);
    /* Arrow keys nudge the canvas selection by 1 px each press (R-19).
       Edge-triggered (one tap = one pixel) so alignment stays precise.
       Diagonal nudge works naturally — both arrows can fire in one frame. */
    if (!ctrl) {
        int dx = 0, dy = 0;
        if (g->key_pressed(g->self, IK_LEFT))  dx = -1;
        if (g->key_pressed(g->self, IK_RIGHT)) dx = +1;
        if (g->key_pressed(g->self, IK_UP))    dy = -1;
        if (g->key_pressed(g->self, IK_DOWN))  dy = +1;
        if (dx || dy)
            circuit_canvas_widget_nudge_selection(app->circuit_canvas, dx, dy);
    }
    /* Keyboard zoom — Ctrl+= (the '+' key without Shift) zooms in,
       Ctrl+- zooms out. Mirrors the mouse-wheel zoom but focuses on
       the current cam_offset (typically the canvas centre). */
    if (ctrl && g->key_pressed(g->self, IK_EQUAL))
        circuit_canvas_widget_zoom_in(app->circuit_canvas);
    if (ctrl && g->key_pressed(g->self, IK_MINUS))
        circuit_canvas_widget_zoom_out(app->circuit_canvas);
    /* non-Ctrl keyboard shortcuts intended for the canvas: */
    if (!ctrl) {
        if (g->key_pressed(g->self, IK_R)) on_run(shift ? 1 : 0, app);
        if (g->key_pressed(g->self, IK_F)) circuit_canvas_widget_fit_view(app->circuit_canvas);
    }
}

/* ── layout helpers ──────────────────────────────────────────────── */

/* Clamp panel_h and sidebar_w against the current screen dimensions so a
   small window can't squeeze any region below its minimum. */
static void clamp_layout(dcs_app_t *app, int sw, int sh) {
    int panel_h_max = sh - HEADER_H - STATUS_H - CANVAS_H_MIN;
    if (panel_h_max < PANEL_H_MIN) panel_h_max = PANEL_H_MIN;
    if (app->panel_h < PANEL_H_MIN) app->panel_h = PANEL_H_MIN;
    if (app->panel_h > panel_h_max) app->panel_h = panel_h_max;

    int sidebar_w_max = sw - RIGHT_W_MIN;
    if (sidebar_w_max < SIDEBAR_W_MIN) sidebar_w_max = SIDEBAR_W_MIN;
    if (app->sidebar_w < SIDEBAR_W_MIN) app->sidebar_w = SIDEBAR_W_MIN;
    if (app->sidebar_w > sidebar_w_max) app->sidebar_w = sidebar_w_max;
}

/* Recompute every widget's bounds from current screen size + runtime
   sidebar_w / panel_h. Called on init, on resize, and on divider drag. */
static void relayout(dcs_app_t *app, int sw, int sh) {
    clamp_layout(app, sw, sh);
    int sb_w  = app->sidebar_w;
    int pan_h = app->panel_h;
    int panel_top = sh - STATUS_H - pan_h;

    app->root->base.bounds          = (rect_t){0, 0, sw, sh};
    app->header_bg->base.bounds     = (rect_t){0, 0, sw, HEADER_H};
    app->status_bg->base.bounds     = (rect_t){0, sh - STATUS_H, sw, STATUS_H};
    app->status_label->base.bounds  = (rect_t){0, sh - STATUS_H, sw, STATUS_H};
    app->toolbar->base.bounds       = (rect_t){0, HEADER_H, sb_w, panel_top - HEADER_H};
    app->circuit_canvas->base.bounds= (rect_t){sb_w, HEADER_H,
                                               sw - sb_w, panel_top - HEADER_H};
    app->timing_canvas->base.bounds = (rect_t){sb_w, panel_top, sw - sb_w, pan_h};
    app->input_panel->base.bounds   = (rect_t){0, panel_top, sb_w, pan_h};
    /* horizontal divider: a hover band centred on panel_top */
    if (app->div_h) {
        app->div_h->base.bounds = (rect_t){0, panel_top - DIVIDER_HOVER * 0.5f,
                                           sw, DIVIDER_HOVER};
        divider_widget_set_range(app->div_h,
                                 HEADER_H + CANVAS_H_MIN,
                                 sh - STATUS_H - PANEL_H_MIN);
    }
    /* vertical divider: a hover band centred on sb_w (only over the canvas
       area, not the bottom panel — keeps the input panel's clicks clean) */
    if (app->div_v) {
        app->div_v->base.bounds = (rect_t){sb_w - DIVIDER_HOVER * 0.5f, HEADER_H,
                                           DIVIDER_HOVER, sh - HEADER_H - STATUS_H};
        int sw_max = sw - RIGHT_W_MIN;
        if (sw_max < SIDEBAR_W_MIN) sw_max = SIDEBAR_W_MIN;
        divider_widget_set_range(app->div_v, SIDEBAR_W_MIN, sw_max);
    }
    /* Help dialog (R-14) is the modal overlay — bounds always equal the
       entire window so its centred box stays centred on resize. */
    if (app->help_dialog) {
        help_dialog_set_bounds(app->help_dialog, (rect_t){0, 0, sw, sh});
    }
}

/* Divider drag callbacks. The divider reports the new mouse coordinate;
   we translate it to the panel_h / sidebar_w field and re-layout. */
static void on_divider_h(int new_y, void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    int sw = 0, sh = 0;
    app->graph->screen_size(app->graph->self, &sw, &sh);
    app->panel_h = sh - STATUS_H - new_y;
    relayout(app, sw, sh);
}

static void on_divider_v(int new_x, void *user) {
    dcs_app_t *app = (dcs_app_t *)user;
    int sw = 0, sh = 0;
    app->graph->screen_size(app->graph->self, &sw, &sh);
    app->sidebar_w = new_x;
    relayout(app, sw, sh);
}

/* ── widget tree construction ────────────────────────────────────── */

static void build_widgets(dcs_app_t *app) {
    int sw = 1280, sh = 800;
    if (app->graph->screen_size) app->graph->screen_size(app->graph->self, &sw, &sh);

    /* Initial layout values */
    app->sidebar_w = SIDEBAR_W_DEFAULT;
    app->panel_h   = BOTTOM_PANEL_H_DEFAULT;
    clamp_layout(app, sw, sh);

    int sb_w  = app->sidebar_w;
    int pan_h = app->panel_h;
    int panel_top = sh - STATUS_H - pan_h;

    app->root = panel_create((rect_t){0, 0, sw, sh});
    panel_set_background(app->root, COLOR_BG);

    app->circuit_canvas = circuit_canvas_widget_create(
        (rect_t){sb_w, HEADER_H, sw - sb_w, panel_top - HEADER_H},
        app->circuit);
    circuit_canvas_widget_set_status_cb (app->circuit_canvas, on_canvas_status,  app);
    circuit_canvas_widget_set_mutated_cb(app->circuit_canvas, on_canvas_mutated, app);

    app->toolbar = side_toolbar_create((rect_t){0, HEADER_H, sb_w, panel_top - HEADER_H},
                                       app->circuit_canvas);

    app->timing_canvas = timing_canvas_widget_create(
        (rect_t){sb_w, panel_top, sw - sb_w, pan_h});

    app->input_panel = input_panel_create(
        (rect_t){0, panel_top, sb_w, pan_h}, app->circuit);
    input_panel_set_run_cb(app->input_panel, on_run, app);

    /* Dividers: bounds computed in relayout(). */
    app->div_h = divider_widget_create((rect_t){0, 0, 1, 1}, DIVIDER_HORIZONTAL);
    divider_widget_set_change_cb(app->div_h, on_divider_h, app);
    app->div_v = divider_widget_create((rect_t){0, 0, 1, 1}, DIVIDER_VERTICAL);
    divider_widget_set_change_cb(app->div_v, on_divider_v, app);

    /* File menu (header) */
    app->file_menu = menu_create((rect_t){8, 4, 80, 22}, "File");
    menu_add_item(app->file_menu, "New",        "Ctrl+N");
    menu_add_item(app->file_menu, "Open...",    "Ctrl+O");
    menu_add_item(app->file_menu, "Save",       "Ctrl+S");
    menu_add_item(app->file_menu, "Save As...", "Ctrl+Shift+S");
    menu_set_on_select(app->file_menu, on_menu_select, app);

    /* Edit menu — R-5 undo/redo. Sits between File and View. */
    app->edit_menu = menu_create((rect_t){92, 4, 80, 22}, "Edit");
    menu_add_item(app->edit_menu, "Undo", "Ctrl+Z");
    menu_add_item(app->edit_menu, "Redo", "Ctrl+Y");
    menu_set_on_select(app->edit_menu, on_edit_menu_select, app);

    /* View menu — Phase 8 black-box toggle. */
    app->view_menu = menu_create((rect_t){176, 4, 80, 22}, "View");
    menu_add_item(app->view_menu, "Toggle black-box view", "Ctrl+B");
    menu_set_on_select(app->view_menu, on_view_menu_select, app);

    /* Help menu — R-14 + R-15. */
    app->help_menu = menu_create((rect_t){260, 4, 80, 22}, "Help");
    menu_add_item(app->help_menu, "Show keyboard reference", "F1");
    menu_add_item(app->help_menu, "About DCS",                "");
    menu_set_on_select(app->help_menu, on_help_menu_select, app);

    /* Help dialog — full-screen modal layer. Added LAST so it sits on top
       in dispatch and draw order. Starts hidden. */
    app->help_dialog = help_dialog_create((rect_t){0, 0, sw, sh});

    /* Status label */
    app->status_label = label_create((rect_t){0, sh - STATUS_H, sw, STATUS_H},
                                     "", 14, 0xC8C8C8FFu);
    app->status_bg = panel_create((rect_t){0, sh - STATUS_H, sw, STATUS_H});
    panel_set_background(app->status_bg, 0x282832FFu);
    app->header_bg = panel_create((rect_t){0, 0, sw, HEADER_H});
    panel_set_background(app->header_bg, 0xC8C8C8FFu);

    /* Compose: bgs first (under content), then content widgets, then dividers
       on TOP of content (so they win hit-tests inside their hover bands), then
       file menu LAST (its expanded-on-open bounds covers everything). */
    panel_add_child(app->root, &app->header_bg->base);
    panel_add_child(app->root, &app->status_bg->base);
    panel_add_child(app->root, &app->circuit_canvas->base);
    panel_add_child(app->root, &app->toolbar->base);
    panel_add_child(app->root, &app->timing_canvas->base);
    panel_add_child(app->root, &app->input_panel->base);
    panel_add_child(app->root, &app->div_h->base);
    panel_add_child(app->root, &app->div_v->base);
    panel_add_child(app->root, &app->status_label->base);
    panel_add_child(app->root, &app->file_menu->base);
    panel_add_child(app->root, &app->edit_menu->base);
    panel_add_child(app->root, &app->view_menu->base);
    panel_add_child(app->root, &app->help_menu->base);
    /* Modal layer LAST so it's the topmost child (depth-first dispatch
       tries last-child first, draws last). */
    panel_add_child(app->root, &app->help_dialog->base);

    relayout(app, sw, sh);   /* sets divider bounds + ranges */
}

static void on_frame_resize(int new_w, int new_h, void *user) {
    relayout((dcs_app_t *)user, new_w, new_h);
}

/* ── public API ──────────────────────────────────────────────────── */

void dcs_app_init(dcs_app_t *self, iplatform_t *p, igraph_t *g, const char *initial_path) {
    memset(self, 0, sizeof(*self));
    self->platform = p;
    self->graph    = g;
    self->circuit  = circuit_create();
    simulation_init(&self->sim, self->circuit);
    command_stack_init(&self->cmds);                /* R-5 */
    snprintf(self->file_path, sizeof(self->file_path),
             "%s", initial_path ? initial_path : "untitled.dcs");

    build_widgets(self);
    frame_init(&self->frame, g, p, &self->root->base);
    /* ESC must NOT quit — circuit_canvas uses ESC to cancel modes. */
    frame_quit(&self->frame)->esc_quits = 0;
    /* Save-on-close gate (R-11): prompt if dirty when user X-clicks /
       Alt-F4s / triggers any other quit path. */
    quit_manager_set_attempt_cb(frame_quit(&self->frame), on_attempt_quit, self);
    /* Re-layout widgets when the user maximizes / resizes / restores. */
    frame_set_resize_cb(&self->frame, on_frame_resize, self);

    /* If an initial path was given and the file exists, load it. */
    if (initial_path) {
        int len = 0;
        char *text = p->read_file(p->self, initial_path, &len);
        if (text) {
            load_circuit_from_text(self, initial_path, text);
            free(text);
            simulation_release(&self->sim);
            simulation_init(&self->sim, self->circuit);
        }
    }
    /* Seed the snapshot cache so the FIRST mutation can build an undo
       entry from a known "before". load_circuit_from_text already does
       this when it ran; this catches the "no initial path" case. */
    if (!self->last_snapshot) {
        self->last_snapshot = serialize_snapshot(self);
    }
    /* Push the initial title (untitled or loaded basename, no asterisk —
       dirty is zero from the memset above). */
    refresh_window_title(self);
}

void dcs_app_release(dcs_app_t *self) {
    frame_shutdown(&self->frame);   /* recursively destroys widget tree */
    simulation_release(&self->sim);
    if (self->circuit) circuit_destroy(self->circuit);
    self->circuit = NULL;
    command_stack_release(&self->cmds);            /* R-5 */
    free(self->last_snapshot);
    self->last_snapshot = NULL;
}

void dcs_app_run(dcs_app_t *self) {
    while (!quit_manager_should_quit(&self->frame.quit, self->graph)) {
        poll_global_shortcuts(self);
        frame_tick(&self->frame);
    }
}

void dcs_app_undo(dcs_app_t *self) {
    const char *desc = command_stack_undo(&self->cmds);
    if (!desc) { set_status(self, "Nothing to undo"); return; }
    /* install_circuit_text (called inside the command's undo) already
       refreshed last_snapshot to the restored state. */
    set_status(self, "Undid: %s", desc);
    dcs_app_set_dirty(self, 1);   /* even after undo, state differs from
                                     the last saved state on disk */
    /* simulation references the old circuit_t — reseat. */
    simulation_release(&self->sim);
    simulation_init(&self->sim, self->circuit);
    timing_canvas_widget_set_waves(self->timing_canvas, NULL);
}

void dcs_app_redo(dcs_app_t *self) {
    const char *desc = command_stack_redo(&self->cmds);
    if (!desc) { set_status(self, "Nothing to redo"); return; }
    set_status(self, "Redid: %s", desc);
    dcs_app_set_dirty(self, 1);
    simulation_release(&self->sim);
    simulation_init(&self->sim, self->circuit);
    timing_canvas_widget_set_waves(self->timing_canvas, NULL);
}
