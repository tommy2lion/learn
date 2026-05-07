#ifndef DCS_FW_COLOR_H
#define DCS_FW_COLOR_H

#include <stdint.h>

/* Color encoding: uint32_t in 0xRRGGBBAA byte order.
   Use COLOR_RGBA(r,g,b,a) at construction sites. */

#define COLOR_RGBA(r, g, b, a) ( ((uint32_t)(r) << 24) \
                               | ((uint32_t)(g) << 16) \
                               | ((uint32_t)(b) <<  8) \
                               | ((uint32_t)(a)      ) )

#define COLOR_R(c)  (((c) >> 24) & 0xFFu)
#define COLOR_G(c)  (((c) >> 16) & 0xFFu)
#define COLOR_B(c)  (((c) >>  8) & 0xFFu)
#define COLOR_A(c)  ( (c)        & 0xFFu)

/* Common palette — fully opaque unless suffixed _A50 etc. */
#define COLOR_BLACK     0x000000FFu
#define COLOR_WHITE     0xFFFFFFFFu
#define COLOR_RED       0xC83C3CFFu
#define COLOR_GREEN     0x46966EFFu
#define COLOR_BLUE      0x4080E0FFu
#define COLOR_GRAY      0x808080FFu
#define COLOR_LIGHTGRAY 0xC8C8C8FFu
#define COLOR_DARKGRAY  0x505050FFu
#define COLOR_ORANGE    0xFF8C00FFu
#define COLOR_BG        0xF0F0F0FFu

#endif /* DCS_FW_COLOR_H */
