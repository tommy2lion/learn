#include "quit_manager.h"

void quit_manager_init(quit_manager_t *self) {
    self->esc_quits    = 0;
    self->requested    = 0;
    self->on_attempt   = NULL;
    self->attempt_user = NULL;
}

void quit_manager_request(quit_manager_t *self) {
    self->requested = 1;
}

void quit_manager_set_attempt_cb(quit_manager_t *self,
                                 quit_attempt_fn_t cb, void *user) {
    self->on_attempt   = cb;
    self->attempt_user = user;
}

int quit_manager_should_quit(quit_manager_t *self, igraph_t *g) {
    int want = 0;
    if (self->requested)                                       want = 1;
    if (g->should_close(g->self))                              want = 1;
    if (self->esc_quits && g->key_pressed(g->self, IK_ESCAPE)) want = 1;
    if (!want) return 0;
    /* Attempt gate (R-11): callback may veto. Clear `requested` either way
       so a vetoed quit doesn't immediately re-trigger next tick from a
       leftover explicit request. The graphics layer's should_close is
       expected to be edge-triggered (raylib's WindowShouldClose returns
       true once per X-click then resets to false). */
    if (self->on_attempt && !self->on_attempt(self->attempt_user)) {
        self->requested = 0;
        return 0;
    }
    return 1;
}
