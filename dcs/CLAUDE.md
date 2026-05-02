# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Context

`dcs` (Digital Circuit Simulation) is a project for building a digital circuit simulation tool — visual GUI editor, timing diagram viewer, and headless CLI mode — based on a hierarchical component model that scales from AND/OR/NOT gates up to a CPU. See `architecture.md` for the full design and `step1-design.md` for the Phase 1 implementation plan.

It lives at `learn/dcs/` inside the `learn` monorepo — the git repository root is one level up at `learn/`, not inside `dcs/`.

The sibling project `learn_claude/` contains finished C/raylib games (Calc 24, Rubik's Cube, Sudoku) whose build setup and architecture conventions are the reference for new projects in this repo.

## Environment (C + raylib, if applicable)

Windows + MSYS2 MinGW64. C projects build with `make` from their project directory.

Compiler flags pattern: `-Wall -Wextra -O2 -std=c99 -lm -lraylib -lopengl32 -lgdi32 -lwinmm`  
raylib headers/libs: `/c/msys64/mingw64/include` and `/c/msys64/mingw64/lib`  
Makefiles set `TEMP`/`TMP`/`TMPDIR` explicitly to work around MSYS2 issues.

## Architecture Conventions (reference from sibling project)

The modular C pattern used in `learn_claude/to24/gui/modular/` is the established template:

- Logic modules must not import raylib — keeps them independently testable.
- All internal helpers are `static`; only public API goes in `.h` files.
- State is passed as a pointer to a struct — no global mutable state.
- UI widgets are plain structs + stateless draw functions with no callbacks or game-state pointers.
- `main.c` is coordinator only: initializes raylib and runs the event loop.
