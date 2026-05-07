# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Context

`dcs` (Digital Circuit Simulation) is a project for building a digital circuit simulation tool — visual GUI editor, timing diagram viewer, and headless CLI mode — based on a hierarchical component model that scales from AND/OR/NOT gates up to a CPU.

The project is undergoing a layered rewrite (Step 2). Two trees live side-by-side under `dcs/`:

- **`prototype_version/`** — the frozen Step-1 prototype. Self-contained: `cd prototype_version && make test` builds and runs all 98 unit/integration tests. The original design docs live there too: `prototype_version/architecture.md` and `prototype_version/step1-design.md`.
- **`src/`, `test/`, `Makefile`** — the new layered codebase being built up phase-by-phase. The plan is in `step2-refactor-plan.md` (requirements), `step2-refactor-design.md` (design proposal v2), and `step2-review-of-refactor-design.md` (review notes).

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
