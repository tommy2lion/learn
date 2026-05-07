#ifndef DCS_FW_QUIT_MANAGER_H
#define DCS_FW_QUIT_MANAGER_H

#include "oo.h"
#include "../graphics/igraph.h"

class tagt_quit_manager {
    int  esc_quits;     /* 1 = pressing ESC requests quit (default 0)        */
    int  requested;     /* set when programmatic quit is requested           */
};
typedef class tagt_quit_manager quit_manager_t;

void quit_manager_init   (quit_manager_t *self);
void quit_manager_request(quit_manager_t *self);
/* Polled by the frame loop each tick. Returns 1 if the loop should exit. */
int  quit_manager_should_quit(quit_manager_t *self, igraph_t *g);

#endif /* DCS_FW_QUIT_MANAGER_H */
