#ifndef DCS_APP_COMMAND_H
#define DCS_APP_COMMAND_H

#include "../framework/core/oo.h"

/* Command pattern for undo/redo (R-5). Each user-driven mutation builds
   a command_t carrying enough state to (a) re-apply itself (execute,
   used by redo) and (b) reverse itself (undo). The command_stack owns
   the command's lifetime and frees it via vt->destroy when the entry
   is evicted from either stack.

   Construction convention: the mutation site performs the action first,
   then builds a command capturing the inverse. command_stack_push then
   stores it WITHOUT calling execute (the action already happened). Redo
   later re-executes via vt->execute. */

typedef struct tagt_command command_t;

class tagt_command_vt {
    /* Re-apply the change. Called by command_stack_redo. */
    void        (*execute) (command_t *self);
    /* Reverse the change. Called by command_stack_undo. */
    void        (*undo)    (command_t *self);
    /* Free any owned state and the command itself. */
    void        (*destroy) (command_t *self);
    /* Short human-readable description for the status bar
       ("Wired", "Deleted 3 nodes", etc.). May return NULL. */
    const char *(*describe)(const command_t *self);
};
typedef class tagt_command_vt command_vt_t;

class tagt_command {
    const command_vt_t *vt;
};

static inline void command_execute(command_t *c) {
    if (c && c->vt && c->vt->execute) c->vt->execute(c);
}
static inline void command_undo(command_t *c) {
    if (c && c->vt && c->vt->undo) c->vt->undo(c);
}
static inline void command_destroy(command_t *c) {
    if (c && c->vt && c->vt->destroy) c->vt->destroy(c);
}
static inline const char *command_describe(const command_t *c) {
    if (c && c->vt && c->vt->describe) {
        const char *s = c->vt->describe(c);
        return s ? s : "(unknown)";
    }
    return "(unknown)";
}

#endif /* DCS_APP_COMMAND_H */
