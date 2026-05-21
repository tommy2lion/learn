#ifndef DCS_FW_QUIT_MANAGER_H
#define DCS_FW_QUIT_MANAGER_H

#include "oo.h"
#include "../graphics/igraph.h"

/* Returns 1 to allow the quit, 0 to veto it. When the callback vetoes,
   quit_manager_should_quit clears `requested` and returns 0, so the next
   tick starts fresh (window X-button, ESC, or explicit quit_manager_request
   can fire again later). Used by dcs_app to show the save-on-close
   prompt — R-11. */
typedef int (*quit_attempt_fn_t)(void *user);

class tagt_quit_manager {
    int  esc_quits;     /* 1 = pressing ESC requests quit (default 0)        */
    int  requested;     /* set when programmatic quit is requested           */

    /* Optional attempt-quit gate. When set, called from should_quit just
       before returning 1 — vetoing it cancels the quit. (R-11) */
    quit_attempt_fn_t  on_attempt;
    void              *attempt_user;
};
typedef class tagt_quit_manager quit_manager_t;

void quit_manager_init      (quit_manager_t *self);
void quit_manager_request   (quit_manager_t *self);
void quit_manager_set_attempt_cb(quit_manager_t *self,
                                 quit_attempt_fn_t cb, void *user);
/* Polled by the frame loop each tick. Returns 1 if the loop should exit. */
int  quit_manager_should_quit(quit_manager_t *self, igraph_t *g);

#endif /* DCS_FW_QUIT_MANAGER_H */
