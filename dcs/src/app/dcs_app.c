#include "dcs_app.h"
#include "../framework/core/color.h"
#include "../domain/circuit_io.h"
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

/* ── file actions ────────────────────────────────────────────────── */

static void load_circuit_from_text(dcs_app_t *app, const char *path, const char *text) {
    char err[256] = {0};
    circuit_t *c = circuit_io_parse(text, err, sizeof(err));
    if (!c) { set_status(app, "Parse error: %s", err); return; }
    if (app->circuit) circuit_destroy(app->circuit);
    app->circuit = c;
    circuit_canvas_widget_set_circuit(app->circuit_canvas, c);
    /* input_panel and timing_canvas keep raw pointers; just update them */
    /* (input_panel_t.circuit is the only field, which we already set in create —
       but here it points to the OLD circuit. Reseat by recreating? Simpler:
       expose a setter on input_panel.) */
    /* For now, the input_panel was created with the original circuit pointer.
       We need a quick setter — just patch the field directly via casting since
       we haven't exposed one. */
    /* (input_panel.h doesn't expose set_circuit; do it by struct knowledge.) */
    {
        /* Direct field access — header-internal coupling is acceptable here. */
        const circuit_t **slot = (const circuit_t **)&((input_panel_t *)app->input_panel)->circuit;
        *slot = c;
    }
    snprintf(app->file_path, sizeof(app->file_path), "%s", path);
    app->path_is_explicit = 1;
    set_status(app, "Opened %s", path);
}

static void action_new(dcs_app_t *app) {
    if (app->circuit) { circuit_destroy(app->circuit); app->circuit = NULL; }
    app->circuit = circuit_create();
    circuit_canvas_widget_set_circuit(app->circuit_canvas, app->circuit);
    {
        const circuit_t **slot = (const circuit_t **)&((input_panel_t *)app->input_panel)->circuit;
        *slot = app->circuit;
    }
    snprintf(app->file_path, sizeof(app->file_path), "untitled.dcs");
    app->path_is_explicit = 0;
    /* clear simulation results */
    simulation_release(&app->sim);
    simulation_init(&app->sim, app->circuit);
    timing_canvas_widget_set_waves(app->timing_canvas, NULL);
    set_status(app, "New circuit");
}

static void action_open(dcs_app_t *app) {
    char path[DCS_APP_FILE_PATH_LEN] = {0};
    if (!app->platform->open_file(app->platform->self, "Open .dcs", path, sizeof(path))) return;
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
    if (!app->platform->save_file(app->platform->self, "Save .dcs", path, sizeof(path))) return;
    char *text = circuit_io_serialize(app->circuit);
    if (!text) { set_status(app, "Serialize failed"); return; }
    int n = (int)strlen(text);
    int rc = app->platform->write_file(app->platform->self, path, text, n);
    free(text);
    if (rc != 0) { set_status(app, "Save failed"); return; }
    snprintf(app->file_path, sizeof(app->file_path), "%s", path);
    app->path_is_explicit = 1;
    set_status(app, "Saved to %s", path);
}

static void action_save(dcs_app_t *app) {
    if (!app->path_is_explicit) { action_save_as(app); return; }
    char *text = circuit_io_serialize(app->circuit);
    if (!text) { set_status(app, "Serialize failed"); return; }
    int n = (int)strlen(text);
    int rc = app->platform->write_file(app->platform->self, app->file_path, text, n);
    free(text);
    if (rc == 0) set_status(app, "Saved to %s", app->file_path);
    else         set_status(app, "Save failed");
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

/* ── run / sweep ─────────────────────────────────────────────────── */

typedef struct { dcs_app_t *app; int sweep; } stim_ctx_t;

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
    if (ctrl && g->key_pressed(g->self, IK_N)) action_new(app);
    if (ctrl && g->key_pressed(g->self, IK_O)) action_open(app);
    if (ctrl && g->key_pressed(g->self, IK_S)) {
        if (shift) action_save_as(app);
        else       action_save   (app);
    }
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
    circuit_canvas_widget_set_status_cb(app->circuit_canvas, on_canvas_status, app);

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
    snprintf(self->file_path, sizeof(self->file_path),
             "%s", initial_path ? initial_path : "untitled.dcs");

    build_widgets(self);
    frame_init(&self->frame, g, p, &self->root->base);
    /* ESC must NOT quit — circuit_canvas uses ESC to cancel modes. */
    frame_quit(&self->frame)->esc_quits = 0;
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
}

void dcs_app_release(dcs_app_t *self) {
    frame_shutdown(&self->frame);   /* recursively destroys widget tree */
    simulation_release(&self->sim);
    if (self->circuit) circuit_destroy(self->circuit);
    self->circuit = NULL;
}

void dcs_app_run(dcs_app_t *self) {
    while (!quit_manager_should_quit(&self->frame.quit, self->graph)) {
        poll_global_shortcuts(self);
        frame_tick(&self->frame);
    }
}
