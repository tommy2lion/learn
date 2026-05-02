# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A multi-game learning project with puzzle games implemented across several platforms (terminal, GUI, web). Each game demonstrates different C architectural patterns.

| Project | Directory | Stack |
|---------|-----------|-------|
| Calc 24 (terminal) | `to24/terminal/` | C99, gcc |
| Calc 24 (monolithic GUI) | `to24/gui/monolithic/` | C99, raylib |
| Calc 24 (modular GUI) | `to24/gui/modular/` | C99, raylib |
| Calc 24 (web) | `to24-webpage/` | HTML/CSS/JS (no build) |
| Magic Square / Rubik's Cube GUI | `magic-square/gui/` | C99, raylib, 3D |
| Sudoku | `sudoku/` | C99, raylib |

## Build Commands

All C projects use `make` from their respective directories. Environment: Windows + MSYS2 MinGW64.

```sh
# Calc 24 — builds all three targets
cd to24 && make
# Outputs: calc24.exe, calc24_gui.exe, calc24_modular.exe

# Rubik's Cube GUI + tests
cd magic-square/gui && make
make test   # runs cube_test.exe (unit tests for cube logic)

# Sudoku
cd sudoku && make
```

Compiler flags: `-Wall -Wextra -O2 -std=c99 -lm -lraylib -lopengl32 -lgdi32 -lwinmm`  
raylib headers/libs: `/c/msys64/mingw64/include` and `/c/msys64/mingw64/lib`

## Architecture: Calc 24 Modular GUI

The modular version (`to24/gui/modular/`) is the most architecturally significant project. Its 8-module layout is a template for how new games should be structured:

- **solver** — pure algorithm, no raylib; brute-forces all 24-reachable expressions
- **parser** — expression tokenizer/evaluator; tracks consumption of card values
- **game** — state machine (`GS_PLAYING`, `GS_FLASH_CORRECT`, `GS_CUSTOM_INPUT`), scoring, round transitions; no raylib
- **card** — card display and face mapping (A=1, J=11, Q=12, K=13)
- **ui** — stateless widget structs (`UiButton`, `UiTextBox`, `UiModal`) + draw functions
- **screen_game** / **screen_custom** — layout and event routing per screen
- **main.c** — coordinator only; initializes raylib and runs the event loop

Key constraint: `game`, `solver`, and `parser` must not import raylib — this keeps them independently testable and reusable.

## Architecture: Rubik's Cube GUI

Data model: a `Cube` struct holding 54 stickers (`unsigned char s[54]`) — 6 faces × 9 stickers each.  
Moves are integers `[0, 18)` encoding U/D/L/R/F/B × CW/CCW/180°.  
Rendering uses a 3D orbit camera (mouse drag to rotate, scroll to zoom).  
Unit tests for cube logic are in `magic-square/gui/cube_test.c` and run via `make test`.

## C Coding Conventions

- All internal helpers are `static`; only the public API is declared in `.h` files.
- Context/state is passed as a pointer to a struct (e.g., `GameCtx *g`, `Cube *c`) — no global mutable state in modular code.
- UI widgets are plain structs + stateless draw functions; they hold no callbacks or pointers to game state.
- Windows-specific: Makefiles set `TEMP`/`TMP`/`TMPDIR` explicitly to work around MSYS2 issues.
