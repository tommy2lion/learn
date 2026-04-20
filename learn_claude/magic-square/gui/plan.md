# Plan: Mouse Picking for Rubik's Cube (Phase 7 Optional Step 13)

## Context
The current GUI uses left-mouse drag to orbit the camera and keyboard U/D/L/R/F/B for face turns. Mouse picking is the last unimplemented feature from design.md step 13. The goal is to let the user click a visible face to trigger a face turn — left click = CW, right click = CCW — without breaking the existing orbit-drag behavior.

## Scheme: Click-to-Turn

| Input | Action |
|-------|--------|
| Left click on face (drag < 5 px) | CW turn of that face |
| Right click on face | CCW turn of that face |
| Left drag (≥ 5 px) | Orbit camera (unchanged) |
| Scroll | Zoom (unchanged) |

Example: click the top face → U, right-click the top face → U', click front face → F.

---

## Implementation — single file: `magic-square/gui/magic_square_gui.c`

### 1. Add `CLICK_DRAG_THRESHOLD` macro (near line 34, with other `#define`s)
```c
#define CLICK_DRAG_THRESHOLD 5.0f
```

### 2. Add `MousePick` struct (after the `Anim` struct, ~line 235)
```c
typedef struct { float drag_dist; } MousePick;
```

### 3. Add `pick_face()` function (after `anim_update()`, ~line 260)
Ray-plane intersection against 6 axis-aligned face planes at ±1.46 (= 1.0 + FACE_OFF).
Uses **back-face culling** to guarantee only the visible face is returned.
Returns face index 0–5 or -1.

```c
static int pick_face(Ray ray)
{
    /* axis(0=x,1=y,2=z), plane coord, outward normal sign */
    static const struct { int axis; float K, ns; } PL[6] = {
        { 1, +1.46f, +1.0f },  /* U */
        { 1, -1.46f, -1.0f },  /* D */
        { 0, -1.46f, -1.0f },  /* L */
        { 0, +1.46f, +1.0f },  /* R */
        { 2, +1.46f, +1.0f },  /* F */
        { 2, -1.46f, -1.0f },  /* B */
    };
    float orig[3] = { ray.position.x,  ray.position.y,  ray.position.z  };
    float dir[3]  = { ray.direction.x, ray.direction.y, ray.direction.z };
    float best_t  = 1e30f;
    int   best    = -1;

    for (int f = 0; f < 6; f++) {
        int   a  = PL[f].axis;
        float K  = PL[f].K;
        float ns = PL[f].ns;
        if (dir[a] * ns >= 0.0f) continue;          /* back-face cull */
        float t = (K - orig[a]) / dir[a];
        if (t < 1e-4f) continue;
        float h[3] = { orig[0]+t*dir[0], orig[1]+t*dir[1], orig[2]+t*dir[2] };
        int t1 = (a+1)%3, t2 = (a+2)%3;
        if (h[t1] < -1.5f || h[t1] > 1.5f) continue;
        if (h[t2] < -1.5f || h[t2] > 1.5f) continue;
        if (t < best_t) { best_t = t; best = f; }
    }
    return best;
}
```

### 4. Declare `mpick` in `main()` (after `timer`, ~line 321)
```c
MousePick mpick = { 0 };
```

### 5. Mouse input block — insert after ENTER handler (after line 368), before the dequeue block
```c
/* ── mouse picking ──────────────────────────────────── */
/* Accumulate drag distance while left button held */
if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    mpick.drag_dist = 0.0f;
if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    Vector2 d = GetMouseDelta();
    mpick.drag_dist += sqrtf(d.x*d.x + d.y*d.y);
}
if (idle) {
    /* Left release with tiny movement = click → CW */
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)
        && mpick.drag_dist < CLICK_DRAG_THRESHOLD) {
        int f = pick_face(GetMouseRay(GetMousePosition(), camera));
        if (f >= 0) queue_push(&queue, f * 3);
    }
    /* Right click → CCW */
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        int f = pick_face(GetMouseRay(GetMousePosition(), camera));
        if (f >= 0) queue_push(&queue, f * 3 + 1);
    }
}
```

Move encoding: `f * 3` = CW, `f * 3 + 1` = CCW (matches cube.h M_U=0, M_Up=1, M_D=3, M_Dp=4 …).
Clicks simply `queue_push`; the existing dequeue block at line 370 fires immediately after in the same frame.

### 6. Update bottom legend (line 449–451)
Append `"  LClick: CW  RClick: CCW"` to the existing legend string.

---

## Why this works cleanly

- **Back-face culling** (`dir[a] * ns >= 0 → skip`) guarantees only front-facing (visible) planes are tested. At most 3 faces can pass at any camera angle.
- **`idle` guard** (`!anim.active && queue.count == 0`) is identical to the keyboard guard — clicks are ignored while an animation is in flight.
- **Drag threshold** prevents tiny cursor jitter during orbiting from being misread as a click. On a click frame the orbit delta is < 5 px, shifting azimuth/elevation by at most 0.025 rad (~1.4°) — imperceptible.
- **`orbit_update`** is unchanged; `GetMouseDelta()` returns the same value regardless of how many times it is called per frame.
- **Move queue integration** is identical to keyboard input — just `queue_push`. Timer, undo/redo history, and the dequeue-to-animation pipeline all work without modification.

---

## Verification

1. Build: `cd magic-square/gui && make magic_square_gui` — should compile clean.
2. Run `magic_square_gui.exe`:
   - Left-click the top face → U turn animates
   - Right-click the top face → U' turn animates
   - Left-click front face → F turn
   - Left-drag → orbit camera unchanged
   - Click on background (miss) → no turn
   - Click while animation running → ignored
   - Timer, undo (Z), redo (Y), SPACE, ENTER all unchanged
3. Commit as "Phase 7 (optional): mouse picking — click face to turn".
