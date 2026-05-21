/* Unit tests for the R-14 help dialog widget. Exercises the show / hide
   state machine, click-outside dismissal, click-inside no-op, and that
   handle_event consumes all events when visible (modal semantics). */

#include "../src/app/help_dialog.h"
#include "../src/framework/widgets/widget.h"
#include "../src/framework/core/message.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

int main(void) {
    printf("=== help_dialog widget tests ===\n");

    /* ── create starts hidden ────────────────────────────────── */
    {
        help_dialog_t *d = help_dialog_create((rect_t){0, 0, 1280, 800});
        check("create: non-NULL",         d != NULL);
        check("create: starts hidden",    help_dialog_is_visible(d) == 0);
        check("create: visible flag 0",   d->base.visible == 0);
        widget_destroy(&d->base);
    }

    /* ── show / hide / is_visible round-trip ─────────────────── */
    {
        help_dialog_t *d = help_dialog_create((rect_t){0, 0, 1280, 800});
        help_dialog_show(d);
        check("show: is_visible == 1",   help_dialog_is_visible(d) == 1);
        check("show: base.visible == 1", d->base.visible == 1);
        help_dialog_hide(d);
        check("hide: is_visible == 0",   help_dialog_is_visible(d) == 0);
        widget_destroy(&d->base);
    }

    /* ── NULL safety ─────────────────────────────────────────── */
    {
        help_dialog_show(NULL);            /* must not crash */
        help_dialog_hide(NULL);
        help_dialog_set_bounds(NULL, (rect_t){0, 0, 100, 100});
        check("is_visible(NULL) == 0", help_dialog_is_visible(NULL) == 0);
    }

    /* ── handle_event: hidden → returns 0 (passes through) ──── */
    {
        help_dialog_t *d = help_dialog_create((rect_t){0, 0, 1280, 800});
        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){10, 10};
        check("hidden: handle_event returns 0",
              widget_handle_event(&d->base, &ev) == 0);
        widget_destroy(&d->base);
    }

    /* ── handle_event: visible + click outside box dismisses ── */
    {
        help_dialog_t *d = help_dialog_create((rect_t){0, 0, 1280, 800});
        help_dialog_show(d);
        /* Trigger a draw to populate d->box (box is computed lazily and
           cached during draw — but compute_box also runs in create/
           set_bounds so the cached value is already correct here. */
        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){5, 5};   /* top-left, far outside any centred box */
        int consumed = widget_handle_event(&d->base, &ev);
        check("visible: click outside is consumed", consumed == 1);
        check("visible: click outside hides dialog",
              help_dialog_is_visible(d) == 0);
        widget_destroy(&d->base);
    }

    /* ── handle_event: visible + click inside box: consumed, NOT dismissed ── */
    {
        help_dialog_t *d = help_dialog_create((rect_t){0, 0, 1280, 800});
        help_dialog_show(d);
        /* Centre of the screen is unambiguously inside the centred box. */
        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind      = EV_MOUSE_PRESS;
        ev.mouse.btn = IM_LEFT;
        ev.mouse.pos = (vec2_t){1280 * 0.5f, 800 * 0.5f};
        int consumed = widget_handle_event(&d->base, &ev);
        check("visible: click inside is consumed",      consumed == 1);
        check("visible: click inside KEEPS dialog open",
              help_dialog_is_visible(d) == 1);
        widget_destroy(&d->base);
    }

    /* ── handle_event: visible + non-press events consumed but no state change ── */
    {
        help_dialog_t *d = help_dialog_create((rect_t){0, 0, 1280, 800});
        help_dialog_show(d);
        event_t mv;
        memset(&mv, 0, sizeof(mv));
        mv.kind      = EV_MOUSE_MOVE;
        mv.mouse.pos = (vec2_t){5, 5};
        check("visible: mouse move consumed",
              widget_handle_event(&d->base, &mv) == 1);
        check("visible: mouse move doesn't dismiss",
              help_dialog_is_visible(d) == 1);
        widget_destroy(&d->base);
    }

    /* ── set_bounds updates box centring (window resize path) ── */
    {
        help_dialog_t *d = help_dialog_create((rect_t){0, 0, 1000, 800});
        float cx_before = d->box.x + d->box.w * 0.5f;
        check("centre before resize ~= 500",
              cx_before > 499.0f && cx_before < 501.0f);
        help_dialog_set_bounds(d, (rect_t){0, 0, 2000, 800});
        float cx_after = d->box.x + d->box.w * 0.5f;
        check("centre after resize ~= 1000",
              cx_after > 999.0f && cx_after < 1001.0f);
        widget_destroy(&d->base);
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
