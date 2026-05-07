#ifndef DCS_APP_INPUT_PANEL_H
#define DCS_APP_INPUT_PANEL_H

#include "../framework/widgets/widget.h"
#include "../domain/circuit.h"
#include "../domain/simulation.h"

#define INPUT_PANEL_MAX_TOGGLES 16
#define INPUT_PANEL_MAX_VISIBLE  5
#define INPUT_PANEL_MAX_STEPS   64

typedef struct tagt_input_toggle {
    char     name[DOMAIN_NAME_LEN];
    signal_t value;
} input_toggle_t;

typedef void (*ip_run_fn_t)  (int sweep_mode, void *user);
typedef void (*ip_status_fn_t)(const char *msg, void *user);

class tagt_input_panel {
    widget_t          base;
    const circuit_t  *circuit;          /* not owned */
    int               steps;            /* current step count */
    input_toggle_t    toggles[INPUT_PANEL_MAX_TOGGLES];
    int               toggle_count;
    ip_run_fn_t       on_run;           /* receives sweep_mode flag */
    void             *run_user;
};
typedef class tagt_input_panel input_panel_t;

input_panel_t *input_panel_create(rect_t bounds, const circuit_t *c);
void           input_panel_set_circuit(input_panel_t *self, const circuit_t *c);
void           input_panel_set_run_cb(input_panel_t *self, ip_run_fn_t cb, void *user);
/* External read access: simulation reads these to feed inputs. */
signal_t       input_panel_value (const input_panel_t *self, const char *name);
int            input_panel_steps (const input_panel_t *self);

#endif /* DCS_APP_INPUT_PANEL_H */
