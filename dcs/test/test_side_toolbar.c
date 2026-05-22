/* App-layer scaffold tests for side_toolbar (U-5 / Stage 3).
   Place-button hit-tests, click-arms-place_kind, black-box toggle.
   Uses a real circuit_canvas_widget_t — pairs well with the toolbar
   since they're tightly coupled by design. */

#include "../src/app/side_toolbar.h"
#include "../src/app/circuit_canvas_widget.h"
#include "../src/framework/widgets/widget.h"
#include "../src/framework/core/message.h"
#include "../src/domain/circuit.h"
#include "../src/domain/component.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;
static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

/* Per side_toolbar.c (kept in sync — these are the hardcoded rects):
   HPAD = 12, BTN_H = 40, VIEW_BTN_H = 38, VIEW_BTN_PAD = 14.
   ITEMS y-offsets (U-45): AND 60, OR 105, NOT 150, NAND 195, NOR 240,
   XOR 285, XNOR 330, +INPUT 400, +OUTPUT 445. */
static vec2_t item_centre(rect_t toolbar_b, int item_y) {
    return (vec2_t){
        toolbar_b.x + toolbar_b.w * 0.5f,
        toolbar_b.y + item_y + 20    /* halfway down the 40-px button */
    };
}
static vec2_t view_btn_centre(rect_t toolbar_b) {
    return (vec2_t){
        toolbar_b.x + toolbar_b.w * 0.5f,
        toolbar_b.y + toolbar_b.h - 14 - 38 * 0.5f
    };
}

static event_t left_press(vec2_t pos) {
    event_t e = {0};
    e.kind = EV_MOUSE_PRESS;
    e.mouse.btn = IM_LEFT;
    e.mouse.pos = pos;
    return e;
}

int main(void) {
    printf("=== side_toolbar tests ===\n");

    rect_t TB = {0, 0, 140, 600};   /* matches the typical sidebar layout */

    /* ── create + target wiring ─────────────────────────────── */
    {
        circuit_t *c = circuit_create();
        circuit_canvas_widget_t *cw = circuit_canvas_widget_create((rect_t){200, 0, 600, 400}, c);
        side_toolbar_t *tb = side_toolbar_create(TB, cw);
        check("create: non-NULL",            tb != NULL);
        check("create: target stored",       tb->target == cw);
        check("create: bounds preserved",    tb->base.bounds.w == 140);
        widget_destroy(&tb->base);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── click AND arms place_kind = PLACE_AND ───────────────── */
    {
        circuit_t *c = circuit_create();
        circuit_canvas_widget_t *cw = circuit_canvas_widget_create((rect_t){200, 0, 600, 400}, c);
        side_toolbar_t *tb = side_toolbar_create(TB, cw);
        event_t e = left_press(item_centre(TB, 60));
        widget_handle_event(&tb->base, &e);
        check("click AND: mode == CMODE_PLACING",  cw->mode == CMODE_PLACING);
        check("click AND: place_kind == PLACE_AND", cw->place_kind == PLACE_AND);
        widget_destroy(&tb->base);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── click OR arms PLACE_OR ─────────────────────────────── */
    {
        circuit_t *c = circuit_create();
        circuit_canvas_widget_t *cw = circuit_canvas_widget_create((rect_t){200, 0, 600, 400}, c);
        side_toolbar_t *tb = side_toolbar_create(TB, cw);
        event_t e = left_press(item_centre(TB, 110));
        widget_handle_event(&tb->base, &e);
        check("click OR: place_kind == PLACE_OR",  cw->place_kind == PLACE_OR);
        widget_destroy(&tb->base);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── click NOT arms PLACE_NOT ───────────────────────────── */
    {
        circuit_t *c = circuit_create();
        circuit_canvas_widget_t *cw = circuit_canvas_widget_create((rect_t){200, 0, 600, 400}, c);
        side_toolbar_t *tb = side_toolbar_create(TB, cw);
        event_t e = left_press(item_centre(TB, 160));
        widget_handle_event(&tb->base, &e);
        check("click NOT: place_kind == PLACE_NOT", cw->place_kind == PLACE_NOT);
        widget_destroy(&tb->base);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── click +INPUT arms PLACE_INPUT, +OUTPUT arms PLACE_OUTPUT ─ */
    {
        circuit_t *c = circuit_create();
        circuit_canvas_widget_t *cw = circuit_canvas_widget_create((rect_t){200, 0, 600, 400}, c);
        side_toolbar_t *tb = side_toolbar_create(TB, cw);
        event_t e = left_press(item_centre(TB, 400));
        widget_handle_event(&tb->base, &e);
        check("click +INPUT:  place_kind == PLACE_INPUT",  cw->place_kind == PLACE_INPUT);
        e = left_press(item_centre(TB, 445));
        widget_handle_event(&tb->base, &e);
        check("click +OUTPUT: place_kind == PLACE_OUTPUT", cw->place_kind == PLACE_OUTPUT);
        widget_destroy(&tb->base);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── click NAND / NOR / XOR / XNOR arms the matching kind (U-45) ─ */
    {
        circuit_t *c = circuit_create();
        circuit_canvas_widget_t *cw = circuit_canvas_widget_create((rect_t){200, 0, 600, 400}, c);
        side_toolbar_t *tb = side_toolbar_create(TB, cw);
        event_t e;
        e = left_press(item_centre(TB, 195)); widget_handle_event(&tb->base, &e);
        check("click NAND: place_kind == PLACE_NAND", cw->place_kind == PLACE_NAND);
        e = left_press(item_centre(TB, 240)); widget_handle_event(&tb->base, &e);
        check("click NOR:  place_kind == PLACE_NOR",  cw->place_kind == PLACE_NOR);
        e = left_press(item_centre(TB, 285)); widget_handle_event(&tb->base, &e);
        check("click XOR:  place_kind == PLACE_XOR",  cw->place_kind == PLACE_XOR);
        e = left_press(item_centre(TB, 330)); widget_handle_event(&tb->base, &e);
        check("click XNOR: place_kind == PLACE_XNOR", cw->place_kind == PLACE_XNOR);
        widget_destroy(&tb->base);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── clicking the ACTIVE button cancels placement ───────── */
    {
        circuit_t *c = circuit_create();
        circuit_canvas_widget_t *cw = circuit_canvas_widget_create((rect_t){200, 0, 600, 400}, c);
        side_toolbar_t *tb = side_toolbar_create(TB, cw);
        event_t e = left_press(item_centre(TB, 60));
        widget_handle_event(&tb->base, &e);
        check("setup: PLACE_AND armed", cw->place_kind == PLACE_AND);
        /* Click AND again — should cancel. */
        widget_handle_event(&tb->base, &e);
        check("second click on AND: place_kind == PLACE_NONE",
              cw->place_kind == PLACE_NONE);
        check("second click on AND: mode == CMODE_IDLE",
              cw->mode == CMODE_IDLE);
        widget_destroy(&tb->base);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    /* ── click black-box toggle switches display_mode ──────── */
    {
        circuit_t *c = circuit_create();
        circuit_canvas_widget_t *cw = circuit_canvas_widget_create((rect_t){200, 0, 600, 400}, c);
        side_toolbar_t *tb = side_toolbar_create(TB, cw);
        display_mode_t before = circuit_canvas_widget_display_mode(cw);
        event_t e = left_press(view_btn_centre(TB));
        widget_handle_event(&tb->base, &e);
        display_mode_t after = circuit_canvas_widget_display_mode(cw);
        check("toggle button: display_mode flipped", before != after);
        /* Flip back. */
        widget_handle_event(&tb->base, &e);
        check("toggle button (again): back to original",
              circuit_canvas_widget_display_mode(cw) == before);
        widget_destroy(&tb->base);
        widget_destroy(&cw->base);
        circuit_destroy(c);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
