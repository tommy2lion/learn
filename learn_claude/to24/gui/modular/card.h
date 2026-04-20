#ifndef CALC24_CARD_H
#define CALC24_CARD_H

#include <raylib.h>

/*
 * card — card data and rendering
 *
 * card_face()  maps a number to its display label:
 *   1 → "A",  11 → "J",  12 → "Q",  13 → "K"
 *   2–10 → numeric string,  >13 → numeric string (custom numbers)
 *
 * card_draw()  renders a single card into the given rectangle.
 *   used    — card already placed in expression (greyed out)
 *   hovered — mouse is over it (lift animation)
 *   flash_t — seconds remaining in win-flash (0 = none)
 *   index   — 0-based position shown as [1]–[4] badge
 */

const char *card_face(int n);

void card_draw(int n, Rectangle rect,
               int used, int hovered, float flash_t, int index);

#endif /* CALC24_CARD_H */
