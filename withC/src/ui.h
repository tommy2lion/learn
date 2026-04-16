#ifndef UI_H
#define UI_H

#include <raylib.h>

/* ── Window dimensions ──────────────────────────────────────────────────── */
#define SCREEN_W  900
#define SCREEN_H  640

/* ── Colour palette ─────────────────────────────────────────────────────── */
#define COL_BG      CLITERAL(Color){ 26,  26,  46, 255}
#define COL_PANEL   CLITERAL(Color){ 15,  52,  96, 255}
#define COL_PANEL2  CLITERAL(Color){ 20,  30,  60, 255}
#define COL_BORDER  CLITERAL(Color){ 22,  33,  62, 255}
#define COL_GOLD    CLITERAL(Color){245, 197,  24, 255}
#define COL_GREEN   CLITERAL(Color){ 46, 204, 113, 255}
#define COL_RED     CLITERAL(Color){231,  76,  60, 255}
#define COL_PURPLE  CLITERAL(Color){155,  89, 182, 255}
#define COL_CYAN    CLITERAL(Color){ 52, 152, 219, 255}
#define COL_GRAY    CLITERAL(Color){ 85,  85,  85, 255}
#define COL_WHITE   WHITE
#define COL_DARK    CLITERAL(Color){ 26,  26,  46, 255}
#define COL_CARD    CLITERAL(Color){240, 240, 235, 255}
#define COL_OVERLAY CLITERAL(Color){  0,   0,   0, 160}

/* ── Button ─────────────────────────────────────────────────────────────── */
typedef struct {
    Rectangle   rect;
    const char *label;
    Color       bg;
    Color       fg;
} UiButton;

void ui_button_draw   (const UiButton *b, int hovered);
int  ui_button_hovered(const UiButton *b);
int  ui_button_clicked(const UiButton *b);  /* true if pressed this frame */

/* ── Single-line numeric text box ───────────────────────────────────────── */
#define UI_TEXTBOX_MAXLEN 8

typedef struct {
    char buf[UI_TEXTBOX_MAXLEN + 1];
} UiTextBox;

void ui_textbox_clear    (UiTextBox *tb);
void ui_textbox_append   (UiTextBox *tb, char ch);   /* no-op if full  */
void ui_textbox_backspace(UiTextBox *tb);
void ui_textbox_draw     (const UiTextBox *tb, Rectangle rect,
                          int focused, float anim);

/* ── Misc draw helpers ───────────────────────────────────────────────────── */
void ui_draw_overlay      (void);                        /* dim backdrop   */
void ui_draw_text_centred (const char *text, int cx,
                           int y, int fs, Color col);    /* centre-aligned */

#endif /* UI_H */
