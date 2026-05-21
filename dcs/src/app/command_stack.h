#ifndef DCS_APP_COMMAND_STACK_H
#define DCS_APP_COMMAND_STACK_H

#include "../framework/core/oo.h"
#include "command.h"

/* Bounded undo + redo stacks (R-5). Cap is the maximum number of
   commands retained in EACH stack — push beyond the cap evicts the
   OLDEST undo entry (calling its destroy). The redo stack is cleared
   on any new push, per standard editor semantics ("once you make a
   new change, the path forward is lost"). */

#define COMMAND_STACK_CAP 100

class tagt_command_stack {
    command_t *undo[COMMAND_STACK_CAP];
    int        undo_count;
    command_t *redo[COMMAND_STACK_CAP];
    int        redo_count;
};
typedef class tagt_command_stack command_stack_t;

void  command_stack_init   (command_stack_t *self);
/* Destroys every retained command. */
void  command_stack_release(command_stack_t *self);

/* Push a freshly-executed command. Clears the redo stack. If the undo
   stack is full, evicts and destroys its oldest entry. Takes ownership;
   passing NULL is a no-op. */
void  command_stack_push   (command_stack_t *self, command_t *cmd);

/* Undo the most recent command. Returns the command's description (or
   NULL if there was nothing to undo). The command moves to the redo
   stack. The returned pointer is owned by the command and stays valid
   until the command is destroyed (i.e. evicted or stack released). */
const char *command_stack_undo(command_stack_t *self);

/* Redo the most recently undone command. Same return shape. */
const char *command_stack_redo(command_stack_t *self);

int command_stack_can_undo (const command_stack_t *self);
int command_stack_can_redo (const command_stack_t *self);
int command_stack_undo_count(const command_stack_t *self);
int command_stack_redo_count(const command_stack_t *self);

#endif /* DCS_APP_COMMAND_STACK_H */
