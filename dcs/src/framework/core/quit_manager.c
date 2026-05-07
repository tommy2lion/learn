#include "quit_manager.h"

void quit_manager_init(quit_manager_t *self) {
    self->esc_quits = 0;
    self->requested = 0;
}

void quit_manager_request(quit_manager_t *self) {
    self->requested = 1;
}

int quit_manager_should_quit(quit_manager_t *self, igraph_t *g) {
    if (self->requested)                       return 1;
    if (g->should_close(g->self))              return 1;   /* X button / Alt+F4 */
    if (self->esc_quits && g->key_pressed(g->self, IK_ESCAPE)) return 1;
    return 0;
}
