#include "ui.h"
#include <string.h>
#include <stdio.h>

/* ── Button ───────────────────────────────────────────────────────────────── */

void ui_button_draw(const UiButton *b, int hovered) {
    Color bg = hovered ? Fade(b->bg, 0.70f) : b->bg;
    DrawRectangleRounded(b->rect, 0.25f, 8, bg);
    DrawRectangleRoundedLinesEx(b->rect, 0.25f, 8, 1.5f, Fade(WHITE, 0.15f));
    int fw = MeasureText(b->label, 20);
    DrawText(b->label,
             (int)(b->rect.x + (b->rect.width  - fw) / 2),
             (int)(b->rect.y + (b->rect.height - 20) / 2),
             20, b->fg);
}

int ui_button_hovered(const UiButton *b) {
    return CheckCollisionPointRec(GetMousePosition(), b->rect);
}

int ui_button_clicked(const UiButton *b) {
    return ui_button_hovered(b) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/* ── TextBox ──────────────────────────────────────────────────────────────── */

void ui_textbox_clear(UiTextBox *tb) {
    tb->buf[0] = '\0';
}

void ui_textbox_append(UiTextBox *tb, char ch) {
    int len = (int)strlen(tb->buf);
    if (len < UI_TEXTBOX_MAXLEN) {
        tb->buf[len]     = ch;
        tb->buf[len + 1] = '\0';
    }
}

void ui_textbox_backspace(UiTextBox *tb) {
    int len = (int)strlen(tb->buf);
    if (len > 0) tb->buf[len - 1] = '\0';
}

void ui_textbox_draw(const UiTextBox *tb, Rectangle rect,
                     int focused, float anim) {
    Color border = focused ? COL_CYAN : COL_BORDER;
    Color bg     = focused ? (Color){20, 60, 110, 255} : COL_PANEL;

    DrawRectangleRounded(rect, 0.15f, 8, bg);
    DrawRectangleRoundedLinesEx(rect, 0.15f, 8, 2.0f, border);

    /* Value + blinking cursor on focused box */
    char disp[UI_TEXTBOX_MAXLEN + 4];
    if (focused)
        snprintf(disp, sizeof(disp), "%s%s",
                 tb->buf, ((int)(anim * 2) % 2) ? "|" : "");
    else
        snprintf(disp, sizeof(disp), "%s", tb->buf[0] ? tb->buf : "_");

    int tw = MeasureText(disp, 24);
    DrawText(disp,
             (int)(rect.x + (rect.width  - tw) / 2),
             (int)(rect.y + (rect.height - 24) / 2),
             24, focused ? COL_GOLD : COL_WHITE);
}

/* ── Misc ─────────────────────────────────────────────────────────────────── */

void ui_draw_overlay(void) {
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, COL_OVERLAY);
}

void ui_draw_text_centred(const char *text, int cx, int y, int fs, Color col) {
    DrawText(text, cx - MeasureText(text, fs) / 2, y, fs, col);
}
