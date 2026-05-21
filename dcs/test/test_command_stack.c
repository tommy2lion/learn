/* Unit tests for command_stack (R-5, Stage 9a). Uses a tiny stub
   command whose execute adds `delta` to a shared counter and undo
   subtracts, so do/undo round-trips become arithmetic equalities the
   test can verify directly. */

#include "../src/app/command.h"
#include "../src/app/command_stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* ── stub command ─────────────────────────────────────────────────── */

typedef struct {
    command_t base;
    int      *counter;       /* shared with the test */
    int       delta;
    int      *destroyed;     /* incremented when destroy fires */
    char      desc[24];
} stub_cmd_t;

static void stub_execute(command_t *self) {
    stub_cmd_t *s = (stub_cmd_t *)self;
    *s->counter += s->delta;
}
static void stub_undo(command_t *self) {
    stub_cmd_t *s = (stub_cmd_t *)self;
    *s->counter -= s->delta;
}
static void stub_destroy(command_t *self) {
    stub_cmd_t *s = (stub_cmd_t *)self;
    (*s->destroyed)++;
    free(s);
}
static const char *stub_describe(const command_t *self) {
    return ((const stub_cmd_t *)self)->desc;
}

static const command_vt_t STUB_VT = {
    .execute  = stub_execute,
    .undo     = stub_undo,
    .destroy  = stub_destroy,
    .describe = stub_describe,
};

/* Builds a stub_cmd, "executes" it eagerly (per the push-after-doing
   convention), and returns the command_t* for the caller to push. */
static command_t *make_stub(int *counter, int delta, int *destroyed,
                            const char *desc) {
    stub_cmd_t *s = (stub_cmd_t *)calloc(1, sizeof(*s));
    s->base.vt   = &STUB_VT;
    s->counter   = counter;
    s->delta     = delta;
    s->destroyed = destroyed;
    snprintf(s->desc, sizeof(s->desc), "%s", desc);
    /* Apply the action up-front (matches the mutation-site contract). */
    *counter += delta;
    return &s->base;
}

int main(void) {
    printf("=== command_stack tests ===\n");

    /* ── init: both stacks empty ─────────────────────────────────── */
    {
        command_stack_t s;
        command_stack_init(&s);
        check("init: undo_count == 0",      command_stack_undo_count(&s) == 0);
        check("init: redo_count == 0",      command_stack_redo_count(&s) == 0);
        check("init: can_undo == 0",        command_stack_can_undo(&s) == 0);
        check("init: can_redo == 0",        command_stack_can_redo(&s) == 0);
        check("undo of empty: returns NULL",command_stack_undo(&s) == NULL);
        check("redo of empty: returns NULL",command_stack_redo(&s) == NULL);
        command_stack_release(&s);
    }

    /* ── push, undo, redo round-trip ─────────────────────────────── */
    {
        int counter = 0, destroyed = 0;
        command_stack_t s;
        command_stack_init(&s);

        command_stack_push(&s, make_stub(&counter, 7, &destroyed, "plus7"));
        check("push: counter == 7 (action pre-applied)", counter == 7);
        check("push: undo_count == 1",     command_stack_undo_count(&s) == 1);
        check("push: redo_count == 0",     command_stack_redo_count(&s) == 0);

        const char *u = command_stack_undo(&s);
        check("undo: returns description", u && strcmp(u, "plus7") == 0);
        check("undo: counter == 0",        counter == 0);
        check("undo: moved to redo",       command_stack_undo_count(&s) == 0
                                        && command_stack_redo_count(&s) == 1);

        const char *r = command_stack_redo(&s);
        check("redo: returns description", r && strcmp(r, "plus7") == 0);
        check("redo: counter == 7",        counter == 7);
        check("redo: back to undo",        command_stack_undo_count(&s) == 1
                                        && command_stack_redo_count(&s) == 0);

        command_stack_release(&s);
        check("release: stub destroyed",   destroyed == 1);
    }

    /* ── push clears redo stack ──────────────────────────────────── */
    {
        int counter = 0, destroyed = 0;
        command_stack_t s;
        command_stack_init(&s);

        command_stack_push(&s, make_stub(&counter, 1, &destroyed, "A"));
        command_stack_undo(&s);
        check("setup: 0 in undo, 1 in redo",
              command_stack_undo_count(&s) == 0 &&
              command_stack_redo_count(&s) == 1);
        command_stack_push(&s, make_stub(&counter, 5, &destroyed, "B"));
        check("push after undo: redo cleared",
              command_stack_redo_count(&s) == 0);
        check("push after undo: redo'd stub was destroyed",
              destroyed == 1);
        command_stack_release(&s);
        check("release: 'B' also destroyed", destroyed == 2);
    }

    /* ── multi-push, multi-undo preserves LIFO order ─────────────── */
    {
        int counter = 0, destroyed = 0;
        command_stack_t s;
        command_stack_init(&s);
        command_stack_push(&s, make_stub(&counter, 1, &destroyed, "1"));
        command_stack_push(&s, make_stub(&counter, 2, &destroyed, "2"));
        command_stack_push(&s, make_stub(&counter, 4, &destroyed, "4"));
        check("3 pushes: counter == 7", counter == 7);
        const char *u1 = command_stack_undo(&s);
        check("undo 1: most recent first", strcmp(u1, "4") == 0);
        check("undo 1: counter == 3",      counter == 3);
        const char *u2 = command_stack_undo(&s);
        check("undo 2: LIFO order",        strcmp(u2, "2") == 0);
        check("undo 2: counter == 1",      counter == 1);
        const char *u3 = command_stack_undo(&s);
        check("undo 3: last in",           strcmp(u3, "1") == 0);
        check("undo 3: counter == 0",      counter == 0);
        check("nothing left to undo",      command_stack_undo(&s) == NULL);
        command_stack_release(&s);
    }

    /* ── cap eviction: oldest is dropped + destroyed ─────────────── */
    {
        int counter = 0, destroyed = 0;
        command_stack_t s;
        command_stack_init(&s);
        /* Fill to cap. */
        for (int i = 0; i < COMMAND_STACK_CAP; i++) {
            char nm[16]; snprintf(nm, sizeof(nm), "c%d", i);
            command_stack_push(&s, make_stub(&counter, 1, &destroyed, nm));
        }
        check("filled to cap",     command_stack_undo_count(&s) == COMMAND_STACK_CAP);
        check("no destroys yet",   destroyed == 0);

        /* One more push → oldest evicted + destroyed. */
        command_stack_push(&s, make_stub(&counter, 1, &destroyed, "overflow"));
        check("after overflow: still at cap",
              command_stack_undo_count(&s) == COMMAND_STACK_CAP);
        check("after overflow: one destroy", destroyed == 1);

        /* The remaining undo stack must NOT contain "c0" (the evicted
           one). Walk it: the bottom should now be "c1". */
        /* We can't peek directly — undo the entire stack and verify the
           last-undone is "c1" (the original c0 is gone). */
        while (command_stack_can_undo(&s)) (void)command_stack_undo(&s);
        /* Redo everything back, then undo to bottom to check description. */
        while (command_stack_can_redo(&s)) (void)command_stack_redo(&s);
        const char *last = NULL;
        while (command_stack_can_undo(&s)) last = command_stack_undo(&s);
        check("evicted bottom is now 'c1', not 'c0'",
              last && strcmp(last, "c1") == 0);

        command_stack_release(&s);
        check("release: every remaining stub destroyed",
              destroyed == COMMAND_STACK_CAP + 1);
    }

    /* ── NULL safety ─────────────────────────────────────────────── */
    {
        command_stack_t s;
        command_stack_init(&s);
        command_stack_push(&s, NULL);   /* must not crash */
        check("push(NULL) is a no-op",     command_stack_undo_count(&s) == 0);
        command_stack_release(&s);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
