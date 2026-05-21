#ifndef DCS_APP_DCS_APP_H
#define DCS_APP_DCS_APP_H

#include "../framework/platform/iplatform.h"
#include "../framework/graphics/igraph.h"
#include "../framework/widgets/frame.h"
#include "../framework/widgets/panel.h"
#include "../framework/widgets/menu.h"
#include "../framework/widgets/splitter.h"
#include "../framework/widgets/label.h"
#include "circuit_canvas_widget.h"
#include "timing_canvas_widget.h"
#include "side_toolbar.h"
#include "input_panel.h"
#include "divider_widget.h"
#include "help_dialog.h"
#include "command_stack.h"
#include "../domain/circuit.h"
#include "../domain/simulation.h"

#define DCS_APP_FILE_PATH_LEN 256
#define DCS_APP_STATUS_LEN    160

class tagt_dcs_app {
    iplatform_t *platform;
    igraph_t    *graph;

    circuit_t   *circuit;            /* owned */
    simulation_t sim;                /* owned: simulation_init/release */

    /* widgets (composed under root) */
    panel_t                 *root;
    panel_t                 *header_bg;
    panel_t                 *status_bg;
    side_toolbar_t          *toolbar;
    circuit_canvas_widget_t *circuit_canvas;
    timing_canvas_widget_t  *timing_canvas;
    input_panel_t           *input_panel;
    menu_t                  *file_menu;
    menu_t                  *edit_menu;     /* R-5 — Undo / Redo                */
    menu_t                  *view_menu;     /* Phase 8 — black-box toggle, etc. */
    menu_t                  *help_menu;     /* R-14 — Show help (F1)            */
    help_dialog_t           *help_dialog;   /* R-14 — modal keyboard reference   */
    label_t                 *status_label;
    divider_widget_t        *div_h;        /* between canvas + bottom panel */
    divider_widget_t        *div_v;        /* between sidebar + canvas      */

    /* runtime layout values (mutated by divider drags) */
    int sidebar_w;
    int panel_h;

    /* file path bookkeeping */
    char  file_path[DCS_APP_FILE_PATH_LEN];
    int   path_is_explicit;

    /* unsaved-changes flag — set by canvas mutation callback, cleared by
       New/Open/Save. Reflected in the window title as a trailing "*".
       (R-10) */
    int   dirty;

    /* Undo / redo stack (R-5 part 2). The canvas mutation callback
       serialises the post-mutation state, pairs it with `last_snapshot`
       (which holds the previous state), builds a snapshot_cmd, and
       pushes it here. Undo/redo restore by re-parsing the captured
       strings via install_circuit_text(NULL-path mode). */
    command_stack_t cmds;
    /* Cache of the current state as a freshly-serialised string. Updated
       after every mutation, file load, file new, and undo/redo so the
       NEXT mutation can build a command with a correct "before". */
    char           *last_snapshot;

    /* status message + auto-clear */
    char   status_text[DCS_APP_STATUS_LEN];
    double status_until;

    /* Held by the frame; we run our own loop to insert global-shortcut polling */
    frame_t frame;
};
typedef class tagt_dcs_app dcs_app_t;

void dcs_app_init   (dcs_app_t *self, iplatform_t *p, igraph_t *g, const char *initial_path);
void dcs_app_release(dcs_app_t *self);
void dcs_app_run    (dcs_app_t *self);

/* Undo / redo entry points (R-5). dcs_app's poll_global_shortcuts
   (Stage 9c) binds Ctrl+Z to undo and Ctrl+Y to redo. No-op when the
   corresponding stack is empty. */
void dcs_app_undo   (dcs_app_t *self);
void dcs_app_redo   (dcs_app_t *self);

#endif /* DCS_APP_DCS_APP_H */
