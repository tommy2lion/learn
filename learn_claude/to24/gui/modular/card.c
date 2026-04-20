#include "card.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

const char *card_face(int n) {
    static const char *tbl[] = {
        "", "A","2","3","4","5","6","7","8","9","10","J","Q","K"
    };
    static char buf[8];
    if (n >= 1 && n <= 13) return tbl[n];
    snprintf(buf, sizeof(buf), "%d", n);
    return buf;
}

void card_draw(int n, Rectangle rect,
               int used, int hovered, float flash_t, int index) {
    /* Apply hover lift when card is available */
    Rectangle drawn = rect;
    if (!used && hovered && flash_t <= 0.0f) drawn.y -= 6.0f;

    if (used) {
        DrawRectangleRounded(drawn, 0.12f, 10, (Color){55, 55, 70, 255});
        DrawRectangleRoundedLinesEx(drawn, 0.12f, 10, 2.0f, (Color){75, 75, 90, 255});
        return;
    }

    /* Background: lerp toward green during win flash */
    Color bg = COL_CARD;
    if (flash_t > 0.0f) {
        float t = (flash_t > 0.6f) ? 1.0f : flash_t / 0.6f;
        bg = ColorLerp(COL_CARD, (Color){180, 255, 190, 255}, t);
    }

    /* Drop shadow */
    DrawRectangleRounded(
        (Rectangle){drawn.x + 4.0f, drawn.y + 7.0f, drawn.width, drawn.height},
        0.12f, 10, (Color){0, 0, 0, 80}
    );
    /* Card face */
    DrawRectangleRounded(drawn, 0.12f, 10, bg);
    DrawRectangleRoundedLinesEx(drawn, 0.12f, 10, 2.0f, (Color){175, 175, 165, 255});

    const char *lbl  = card_face(n);
    size_t      llen = strlen(lbl);
    int fs_big = (llen >= 3) ? 34 : (llen == 2 ? 44 : 52);
    int fs_sm  = 17;
    Color ink  = (Color){30, 30, 50, 255};

    int bw = MeasureText(lbl, fs_big);

    /* Top-left corner label */
    DrawText(lbl, (int)drawn.x + 9, (int)drawn.y + 7, fs_sm, ink);
    /* Centre large label */
    DrawText(lbl,
             (int)(drawn.x + (drawn.width  - bw) / 2),
             (int)(drawn.y + (drawn.height - fs_big) / 2),
             fs_big, ink);
    /* Bottom-right corner label */
    int sw = MeasureText(lbl, fs_sm);
    DrawText(lbl,
             (int)(drawn.x + drawn.width  - sw - 9),
             (int)(drawn.y + drawn.height - fs_sm - 7),
             fs_sm, ink);

    /* Position badge [1]–[4] */
    char badge[16];
    snprintf(badge, sizeof(badge), "[%d]", index + 1);
    DrawText(badge,
             (int)(drawn.x + (drawn.width - MeasureText(badge, 12)) / 2),
             (int)(drawn.y  + drawn.height - 20),
             12, (Color){120, 115, 105, 255});
}
