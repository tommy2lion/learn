#include "help_dialog.h"
#include "../framework/core/color.h"
#include <stdlib.h>
#include <string.h>

/* ── content (DCS-specific; lives here, not in framework, until a second
       caller wants the modal mechanism) ───────────────────────────────── */

typedef struct tagt_help_entry {
    const char *shortcut;
    const char *action;
} help_entry_t;

typedef struct tagt_help_section {
    const char         *title;
    const help_entry_t *entries;
    int                 n;
} help_section_t;

static const help_entry_t SEC_FILES[] = {
    {"Ctrl+N",       "New circuit"},
    {"Ctrl+O",       "Open .dcs file"},
    {"Ctrl+S",       "Save"},
    {"Ctrl+Shift+S", "Save As..."},
};
static const help_entry_t SEC_VIEW[] = {
    {"Ctrl+B", "Toggle black-box view"},
    {"Ctrl++", "Zoom in"},
    {"Ctrl+-", "Zoom out"},
    {"F",      "Fit view"},
};
static const help_entry_t SEC_EDIT[] = {
    {"Ctrl+A",       "Select all nodes"},
    {"Del",          "Delete selection"},
    {"Arrows",       "Nudge selection by 1 px"},
    {"Ctrl+Z",       "Undo"},
    {"Ctrl+Y",       "Redo (also Ctrl+Shift+Z)"},
    {"ESC",          "Cancel current mode / clear selection"},
};
static const help_entry_t SEC_SIM[] = {
    {"R",       "Run one step"},
    {"Shift+R", "Reset and run"},
};
static const help_entry_t SEC_MOUSE[] = {
    {"Left click",  "Place / wire / select"},
    {"Left drag",   "Box-select (empty area) or move node"},
    {"Right click", "Delete node or wire / cancel"},
    {"Middle drag", "Pan view"},
    {"Wheel",       "Zoom in / out"},
};
static const help_entry_t SEC_HELP[] = {
    {"F1",  "Toggle this dialog"},
    {"ESC", "Close this dialog"},
};

static const help_section_t SECTIONS[] = {
    {"Files",      SEC_FILES, (int)(sizeof(SEC_FILES) / sizeof(SEC_FILES[0]))},
    {"View",       SEC_VIEW,  (int)(sizeof(SEC_VIEW)  / sizeof(SEC_VIEW[0]))},
    {"Edit",       SEC_EDIT,  (int)(sizeof(SEC_EDIT)  / sizeof(SEC_EDIT[0]))},
    {"Simulation", SEC_SIM,   (int)(sizeof(SEC_SIM)   / sizeof(SEC_SIM[0]))},
    {"Mouse",      SEC_MOUSE, (int)(sizeof(SEC_MOUSE) / sizeof(SEC_MOUSE[0]))},
    {"Help",       SEC_HELP,  (int)(sizeof(SEC_HELP)  / sizeof(SEC_HELP[0]))},
};
static const int N_SECTIONS = (int)(sizeof(SECTIONS) / sizeof(SECTIONS[0]));

/* ── layout constants ────────────────────────────────────────────────── */

#define BOX_W           560
#define BOX_H           660       /* sized for all 6 sections + footer */
#define BOX_PAD          24
#define TITLE_SIZE       20
#define HEADING_SIZE     16
#define BODY_SIZE        14
#define LINE_H           17
#define SECTION_GAP       6
#define SHORTCUT_COL_W  130
#define VIEWPORT_MARGIN  40       /* min gap between box and window edge */

#define OVERLAY_RGBA    0x000000A0u   /* 63% black */
#define BOX_BG_RGBA     0xFAFAFAFFu   /* near-white */
#define BOX_BORDER_RGBA COLOR_DARKGRAY
#define BOX_TITLE_RGBA  COLOR_BLACK
#define HEADING_RGBA    0x4080E0FFu   /* COLOR_BLUE-ish */
#define BODY_RGBA       COLOR_BLACK
#define HINT_RGBA       COLOR_DARKGRAY

/* ── helpers ─────────────────────────────────────────────────────────── */

static rect_t compute_box(rect_t bounds) {
    rect_t r;
    r.w = BOX_W;
    r.h = BOX_H;
    /* Shrink to fit the viewport when the window is smaller than the
       ideal box (e.g. low-res displays). Keeps at least VIEWPORT_MARGIN
       on each side. */
    if (r.w > bounds.w - VIEWPORT_MARGIN) r.w = bounds.w - VIEWPORT_MARGIN;
    if (r.h > bounds.h - VIEWPORT_MARGIN) r.h = bounds.h - VIEWPORT_MARGIN;
    if (r.w < 200) r.w = 200;
    if (r.h < 200) r.h = 200;
    r.x = bounds.x + (bounds.w - r.w) * 0.5f;
    r.y = bounds.y + (bounds.h - r.h) * 0.5f;
    return r;
}

static int point_in_rect(vec2_t p, rect_t r) {
    return p.x >= r.x && p.x < r.x + r.w
        && p.y >= r.y && p.y < r.y + r.h;
}

/* ── vtable ──────────────────────────────────────────────────────────── */

static void hd_draw(widget_t *self, igraph_t *g) {
    help_dialog_t *d = (help_dialog_t *)self;
    /* Translucent overlay over the whole window. */
    g->draw_rect(g->self, d->base.bounds, OVERLAY_RGBA);

    /* Centred box. Recompute every draw so window resize moves it. */
    d->box = compute_box(d->base.bounds);
    g->draw_rect      (g->self, d->box, BOX_BG_RGBA);
    g->draw_rect_lines(g->self, d->box, 2.0f, BOX_BORDER_RGBA);

    /* Clip all subsequent text/lines to the box interior so the dialog
       never leaks content outside its borders even if the section list
       grows or the viewport is unusually small. */
    rect_t inner = {
        d->box.x + 2, d->box.y + 2,
        d->box.w - 4, d->box.h - 4
    };
    g->push_scissor(g->self, inner);

    /* Title. */
    float x = d->box.x + BOX_PAD;
    float y = d->box.y + BOX_PAD;
    g->draw_text(g->self, "DCS Keyboard & Mouse Reference",
                 (vec2_t){x, y}, TITLE_SIZE, BOX_TITLE_RGBA);
    y += TITLE_SIZE + 10;
    /* Underline under title. */
    g->draw_line(g->self,
                 (vec2_t){x, y - 2},
                 (vec2_t){d->box.x + d->box.w - BOX_PAD, y - 2},
                 1.0f, HINT_RGBA);
    y += 6;

    for (int s = 0; s < N_SECTIONS; s++) {
        g->draw_text(g->self, SECTIONS[s].title,
                     (vec2_t){x, y}, HEADING_SIZE, HEADING_RGBA);
        y += HEADING_SIZE + 4;
        for (int i = 0; i < SECTIONS[s].n; i++) {
            const help_entry_t *e = &SECTIONS[s].entries[i];
            g->draw_text(g->self, e->shortcut,
                         (vec2_t){x + 8, y}, BODY_SIZE, BODY_RGBA);
            g->draw_text(g->self, e->action,
                         (vec2_t){x + 8 + SHORTCUT_COL_W, y},
                         BODY_SIZE, BODY_RGBA);
            y += LINE_H;
        }
        y += SECTION_GAP;
    }

    /* Footer hint at the bottom of the box. */
    g->draw_text(g->self, "Click outside or press F1 / ESC to close",
                 (vec2_t){x, d->box.y + d->box.h - BOX_PAD - BODY_SIZE},
                 BODY_SIZE, HINT_RGBA);

    g->pop_scissor(g->self);
}

static int hd_handle_event(widget_t *self, const event_t *ev) {
    help_dialog_t *d = (help_dialog_t *)self;
    /* Hidden: don't consume anything (frame.c's dispatch already skipped
       us via the visible check, but be defensive). */
    if (!d->base.visible) return 0;
    /* Mouse press outside the box dismisses the dialog. Anywhere else
       inside the bounds is consumed silently (the modal layer eats it
       so the canvas underneath stays inert). */
    if (ev->kind == EV_MOUSE_PRESS) {
        if (!point_in_rect(ev->mouse.pos, d->box)) {
            d->base.visible = 0;
        }
        return 1;
    }
    /* Move / release / wheel: consume but no state change. */
    return 1;
}

static void hd_destroy(widget_t *self) { free(self); }

static const widget_vt_t HELP_DIALOG_VT = {
    .draw         = hd_draw,
    .handle_event = hd_handle_event,
    .destroy      = hd_destroy,
};

/* ── public ──────────────────────────────────────────────────────────── */

help_dialog_t *help_dialog_create(rect_t bounds) {
    help_dialog_t *d = (help_dialog_t *)calloc(1, sizeof(help_dialog_t));
    if (!d) return NULL;
    d->base.vt      = &HELP_DIALOG_VT;
    d->base.bounds  = bounds;
    d->base.visible = 0;       /* starts hidden — opened by F1 / Help menu */
    d->box          = compute_box(bounds);
    return d;
}

void help_dialog_show(help_dialog_t *self) {
    if (self) self->base.visible = 1;
}

void help_dialog_hide(help_dialog_t *self) {
    if (self) self->base.visible = 0;
}

int help_dialog_is_visible(const help_dialog_t *self) {
    return self ? self->base.visible : 0;
}

void help_dialog_set_bounds(help_dialog_t *self, rect_t bounds) {
    if (!self) return;
    self->base.bounds = bounds;
    self->box         = compute_box(bounds);
}
