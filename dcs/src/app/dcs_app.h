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
    side_toolbar_t          *toolbar;
    circuit_canvas_widget_t *circuit_canvas;
    timing_canvas_widget_t  *timing_canvas;
    splitter_t              *vsplit;       /* canvas above, timing below */
    input_panel_t           *input_panel;
    menu_t                  *file_menu;
    label_t                 *status_label;

    /* file path bookkeeping */
    char  file_path[DCS_APP_FILE_PATH_LEN];
    int   path_is_explicit;

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

#endif /* DCS_APP_DCS_APP_H */
