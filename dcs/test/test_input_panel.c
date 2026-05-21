/* App-layer scaffold tests for input_panel (U-5 / Stage 3).
   Toggle state round-trip, set_circuit reseat, public-getter
   contracts. Synthesised mouse clicks via widget_handle_event;
   no igraph needed for the event-handling tests. */

#include "../src/app/input_panel.h"
#include "../src/framework/widgets/widget.h"
#include "../src/framework/core/message.h"
#include "../src/domain/circuit.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;
static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* Two-input circuit fixture. */
static circuit_t *make_two_input_circuit(void) {
    circuit_t *c = circuit_create();
    circuit_add_input(c, "a");
    circuit_add_input(c, "b");
    return c;
}

int main(void) {
    printf("=== input_panel tests ===\n");

    /* ── create + defaults ────────────────────────────────────── */
    {
        circuit_t *c = make_two_input_circuit();
        input_panel_t *p = input_panel_create((rect_t){0, 0, 200, 300}, c);
        check("create: non-NULL",         p != NULL);
        check("create: circuit pointer",  p->circuit == c);
        check("create: 8 default steps",  input_panel_steps(p) == 8);
        check("create: 0 toggles",        p->toggle_count == 0);
        widget_destroy(&p->base);
        circuit_destroy(c);
    }

    /* ── input_panel_value defaults to SIG_LOW for unknown ──── */
    {
        circuit_t *c = make_two_input_circuit();
        input_panel_t *p = input_panel_create((rect_t){0, 0, 200, 300}, c);
        check("value(unknown): SIG_LOW",
              input_panel_value(p, "nonexistent") == SIG_LOW);
        check("value(a)       SIG_LOW (no toggle yet)",
              input_panel_value(p, "a") == SIG_LOW);
        widget_destroy(&p->base);
        circuit_destroy(c);
    }

    /* ── synthetic click on a toggle row flips that input ──── */
    {
        circuit_t *c = make_two_input_circuit();
        input_panel_t *p = input_panel_create((rect_t){0, 0, 200, 300}, c);
        /* Toggle row 0 sits at (panel.x + 8, panel.y + 22, w - 16, 24).
           Aim for the centre. */
        event_t ev = {0};
        ev.kind = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){100, 22 + 12};   /* row 0 vertically */
        widget_handle_event(&p->base, &ev);
        check("click row 0: 'a' is now HIGH",
              input_panel_value(p, "a") == SIG_HIGH);
        check("click row 0: 'b' unchanged",
              input_panel_value(p, "b") == SIG_LOW);
        /* Click again — flip back to LOW. */
        widget_handle_event(&p->base, &ev);
        check("click row 0 again: 'a' back to LOW",
              input_panel_value(p, "a") == SIG_LOW);
        widget_destroy(&p->base);
        circuit_destroy(c);
    }

    /* ── set_circuit reseats the pointer ────────────────────── */
    {
        circuit_t *c1 = make_two_input_circuit();
        circuit_t *c2 = circuit_create();
        circuit_add_input(c2, "x");
        input_panel_t *p = input_panel_create((rect_t){0, 0, 200, 300}, c1);
        check("setup: panel sees c1", p->circuit == c1);
        input_panel_set_circuit(p, c2);
        check("set_circuit: panel sees c2", p->circuit == c2);
        widget_destroy(&p->base);
        circuit_destroy(c1);
        circuit_destroy(c2);
    }

    /* ── steps +/- buttons increment / decrement ────────────── */
    {
        circuit_t *c = make_two_input_circuit();
        input_panel_t *p = input_panel_create((rect_t){0, 0, 200, 300}, c);
        check("setup: 8 steps", input_panel_steps(p) == 8);
        /* Steps "+" rect at (x + w - 30, y + h - 78, 22, 22). Centre. */
        event_t ev = {0};
        ev.kind = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){200 - 30 + 11, 300 - 78 + 11};
        widget_handle_event(&p->base, &ev);
        check("steps +: now 9", input_panel_steps(p) == 9);
        /* Steps "-" rect at (x + 8, y + h - 78). */
        ev.mouse.pos = (vec2_t){8 + 11, 300 - 78 + 11};
        widget_handle_event(&p->base, &ev);
        check("steps -: back to 8", input_panel_steps(p) == 8);
        widget_destroy(&p->base);
        circuit_destroy(c);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
