#ifndef DCS_FW_OO_H
#define DCS_FW_OO_H

/* framework/core/oo.h
 *
 * Internal use only. Include this header FIRST in framework / application
 * translation units (before raylib.h, windows.h, etc.) so the macros take
 * effect on the right tokens. The guards below let other code that already
 * defines `class` (rare in C, common in C++) compile without conflict.
 *
 * NOTE: if any C++ code is ever pulled in, REMOVE these macros — `class`
 * is a reserved keyword in C++ and the macro will break compilation.
 */
#ifndef class
#define class      struct
#endif
#ifndef interface
#define interface  struct
#endif

#endif /* DCS_FW_OO_H */
