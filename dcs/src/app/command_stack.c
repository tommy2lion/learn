#include "command_stack.h"
#include <string.h>

void command_stack_init(command_stack_t *self) {
    memset(self, 0, sizeof(*self));
}

static void clear_redo(command_stack_t *self) {
    for (int i = 0; i < self->redo_count; i++) command_destroy(self->redo[i]);
    self->redo_count = 0;
}

void command_stack_release(command_stack_t *self) {
    for (int i = 0; i < self->undo_count; i++) command_destroy(self->undo[i]);
    self->undo_count = 0;
    clear_redo(self);
}

void command_stack_push(command_stack_t *self, command_t *cmd) {
    if (!cmd) return;
    clear_redo(self);
    if (self->undo_count >= COMMAND_STACK_CAP) {
        /* Cap reached — evict the OLDEST entry (index 0). Shift the
           rest down. Destroy the evicted command. */
        command_destroy(self->undo[0]);
        for (int i = 1; i < self->undo_count; i++)
            self->undo[i - 1] = self->undo[i];
        self->undo_count--;
    }
    self->undo[self->undo_count++] = cmd;
}

const char *command_stack_undo(command_stack_t *self) {
    if (self->undo_count <= 0) return NULL;
    command_t *cmd = self->undo[--self->undo_count];
    command_undo(cmd);
    /* Move to redo stack. The redo stack is the inverse of the undo
       stack; it never gets evicted from the bottom because redo's size
       is bounded by undo's (one undo = one redo). */
    self->redo[self->redo_count++] = cmd;
    return command_describe(cmd);
}

const char *command_stack_redo(command_stack_t *self) {
    if (self->redo_count <= 0) return NULL;
    command_t *cmd = self->redo[--self->redo_count];
    command_execute(cmd);
    self->undo[self->undo_count++] = cmd;
    return command_describe(cmd);
}

int command_stack_can_undo  (const command_stack_t *self) { return self->undo_count > 0; }
int command_stack_can_redo  (const command_stack_t *self) { return self->redo_count > 0; }
int command_stack_undo_count(const command_stack_t *self) { return self->undo_count; }
int command_stack_redo_count(const command_stack_t *self) { return self->redo_count; }
