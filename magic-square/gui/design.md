# raylib Rubik's Cube GUI Design Document

## Overview

This document describes a step‑by‑step development plan for building an interactive 3D Rubik's Cube GUI using the raylib library. Each phase is designed to be small enough to fit in a single tool call (no more than 32k tokens), and each step produces a runnable program so you can verify as you go.

---

## Phase 1 — Project Skeleton

1. Create `magic_square/gui/` with an empty `magic_square_gui.c` and a `Makefile` mirroring `c/Makefile`. Compile the empty project to prove raylib links correctly.
2. Add a minimal `main()` that:
   - Opens a raylib window
   - Sets up a 3D camera with `BeginMode3D`
   - Draws a single colored cube
   - Run to verify: window opens, cube is visible, ESC closes

---

## Phase 2 — Cube Data Model (no rendering changes yet)

3. Define the cube state. Two common choices:

   - **Sticker model**: 6 faces × 9 stickers = 54 color enums. Simple, easy to render. Good for game logic.
   - **Cubie model**: 26 small cubies (8 corners + 12 edges + 6 centers), each with position + orientation. More physically accurate.

   **Recommend the sticker model** for simplicity.

4. Implement the 18 face‑turn operations (U, U', U2, D, D', D2, L, L', L2, R, R', R2, F, F', F2, B, B', B2) as permutations on the 54‑sticker array.  
   Unit‑test by verifying `U U U U` == identity.

5. Implement `scramble()` (apply N random moves) and `is_solved()`.

---

## Phase 3 — Static 3D Rendering

6. Render the 27 cubies as `DrawCube` / `DrawCubeWires` at their grid positions. Hide internal faces.
7. Color each visible sticker from the sticker array. The tricky part: map `(face, row, col)` → world‑space quad.  
   Draw the sticker as a slightly‑inset colored quad on the cubie's face so black gaps show between stickers (the classic Rubik's look).
8. Add an orbit camera controlled by:
   - Mouse drag to update camera position (spherical coordinates)
   - Scroll wheel to zoom

---

## Phase 4 — Face Rotation Animation

9. Add animation state: `{ axis, layer, angle, target_angle, active }`.  
   While active, rotate the 9 cubies belonging to the layer by `angle` around the axis each frame using `rlPushMatrix` / `rlRotatef` / `rlPopMatrix`.
10. When `angle` reaches `target_angle`, commit the move to the sticker array (the permutation from step 4) and clear `active`.  
    This separates "visual rotation" from "logical state".
11. Queue moves so a held‑down key doesn't stack glitchy half‑rotations — only dequeue when `!active`.

---

## Phase 5 — Input

12. Keyboard bindings:
    - `U D L R F B` for clockwise turns
    - `Shift+letter` for counter‑clockwise
    - Number keys for double turns (e.g. `2` for U2)
    - `SPACE` = scramble
    - `ENTER` = reset
    - `Z / Y` = undo / redo
13. (Optional) Mouse picking: ray‑cast from cursor into the scene, detect which sticker was clicked, detect drag direction on that face to infer the intended turn. This is the hardest part — defer until everything else works.

---

## Phase 6 — HUD & Polish

14. 2D overlay (after `EndMode3D` then draw text):
    - Move counter
    - Timer
    - "SOLVED!" banner when `is_solved()` returns true
    - Control legend
15. (Optional) Solver hint using a simple layer‑by‑layer algorithm, or Kociemba via an external library.

---

## Phase 7 — Build & Docs

16. Update Makefile to build `magic_square_gui.exe`.  
    Update `README.md` with controls and screenshots.

---

## Recommended Implementation Order (for incremental code generation)

- **Round 1**: Steps 1–2  
- **Round 2**: Steps 3–5  
- **Round 3**: Steps 6–8  
- **Round 4**: Steps 9–11  
- **Round 5**: Step 12  
- **Round 6**: Steps 14 & 16  
- **Round 7** (optional): Step 13 (mouse picking)

---

## Notes

- All code is written in C using the raylib library.
- Each phase should produce a compilable and runnable program for incremental verification.
- Separating animation from logical state ensures reliable turns and good visual feedback.