# Step 2 Supplement — Refinement Implementation Plan

A staged, commit-by-commit plan for working through the refinements
in [`step2-supplement-refinement.md`](step2-supplement-refinement.md).
Each stage is small enough to fit comfortably in one AI session
without the conversation drifting or hitting context limits, and
ordered so dependencies are satisfied before they're needed.

> **How to read this document.** Each stage entry mirrors the
> implementation-plan style: **Goal · Touches · Steps · Tests ·
> Done when · Commit subject**. Code-level decisions (specific APIs,
> data layouts) get sketched here; full implementation details
> defer to the source / docs the stage edits.

---

## 0. Pre-flight (do once before Stage 1)

1. **Baseline.** `make test` → expect **561 / 561 passing**.
   `make gui` and `make cli` → both build clean (zero warnings).
2. **Re-read** the refinement doc, especially the R-* entries this
   plan targets. Cross-references are noted per stage.
3. **Branch policy.** Stay on `main`. One commit per stage. Skip
   the auto-bumped `package.json` / `package-lock.json` per project
   conventions.

---

## Cross-cutting invariants

These hold across every stage. Any commit that violates one is
wrong, regardless of test-suite green.

| Invariant | Why |
|---|---|
| All existing tests stay green; the suite grows monotonically. | Refinements are additive; nothing in the supplement's semantics is changing. |
| No `framework/*` file gains a `#include "../app/*"` or `"../domain/*"`. | Layering rule. |
| No `domain/*` file gains a `#include "../app/*"` or any raylib / Win32 dependency. | Layering rule. |
| New `iplatform_t` methods get **both** a Windows impl AND a Linux stub. | Backend swap-ability. |
| Manual GUI gates: never claim a stage complete on test-suite green alone. The user runs `dcs_gui.exe` and confirms. | Visual changes require visual verification. |

---

## What's IN scope for this plan

These nine refinement items can land **without** Step-3 prerequisites:

- **R-12** Ctrl+A select all
- **R-13** Del key deletes selection
- **R-10** Window title shows current file
- **R-11** Save-on-close prompt
- **R-14** Help menu + F1
- **R-2** Sidebar BLACK-BOX button prominence
- **R-16** Rich CLI `--help` for AI tools
- **R-15** Version system + tamper signature
- **R-1** Display name first-class
- **R-5** Undo / Redo (Command pattern)

That's nine entries across **roughly 10–13 commits** depending on
sub-staging.

## What's OUT of scope (Step-3 territory)

These need Step-3 work to land first; documented in
`step2-supplement-refinement.md` and won't be touched by this plan:

- **R-3** Pin stubs — bundle with Step 3 Phase 3.3 (variable pins).
- **R-4** Real D flip-flop — needs Step 3 Phase 3.4 (stateful primitives).
- **R-6** Vertical pin orientation — needs Step 3 Phase 3.1 (shape DSL).
- **R-7 full** Network type + case enumeration — gated on R-6.
- **R-9** Feedback circuits — needs sequential simulation (Step 3+).
- **R-17** HTTP endpoint — explicitly tagged "Step 3 or 4" by the user.

---

## Stage 1 — Easy keyboard shortcuts (R-12 + R-13)

**Goal.** Two essential shortcuts: `Ctrl+A` selects every node;
`Del` removes the current selection.

**Touches.**
- `src/app/circuit_canvas_widget.{h,c}` — new `select_all` helper +
  handle `IK_DELETE` in CMODE_IDLE.
- `src/app/dcs_app.c` — `poll_global_shortcuts` dispatches `Ctrl+A`.
- `test/test_circuit_canvas_supplement.c` — 2 integration cases.

**Steps.**
1. Add public `circuit_canvas_widget_select_all(self)` — iterates
   inputs, components, outputs; clears existing selection; adds each
   to the selection set (cap at `MAX_SELECTION`).
2. In `poll_global_shortcuts`: on `Ctrl+A`, call the helper.
3. In `ccw_handle_event` CMODE_IDLE branch, on `EV_KEY_PRESS` with
   `IK_DELETE`: call existing `remove_selection(cw)`. (The deletion
   machinery already exists from Phase 4; just need to wire the key.)
4. Tests: synthetic `Ctrl+A` selects all 3 nodes in a simple
   circuit; synthetic `Del` removes them.

**Done when.**
- Suite + 2 tests green.
- Manual: `Ctrl+A` highlights every node; `Del` deletes them.

**Commit subject.** `Add Ctrl+A select-all and Del delete-selection (R-12, R-13)`

---

## Stage 2 — Window title shows current file (R-10)

**Goal.** Window title displays the current file basename, with a
`*` marker when there are unsaved changes. Also lays the groundwork
(dirty flag) for Stage 3's save-on-close prompt.

**Touches.**
- `src/framework/platform/iplatform.h` — declare `set_window_title`.
- `src/framework/platform/platform_windows.c` — Win32 impl
  (`SetWindowTextA`).
- `src/framework/platform/platform_linux.c` — stub returning no-op.
- `src/framework/graphics/graph_raylib.c` — optional title update
  via raylib's `SetWindowTitle` for the gui process (chosen path
  depends on whether iplatform owns the window or graph_raylib does).
- `src/app/dcs_app.h` — add `int dirty;` field.
- `src/app/dcs_app.c` — refresh title on every relevant transition:
  - File new / open / save → reset dirty, refresh title.
  - Any mutation (canvas status callback hook is one option) →
    set dirty, refresh title.

**Steps.**
1. Add the iplatform method + both backend impls.
2. Add a `dcs_app_set_dirty(app, bool)` helper that updates dirty
   and refreshes the title. Title format:
   `"DCS — <basename>"` clean, `"DCS — <basename> *"` dirty.
3. Identify mutation entry points in `dcs_app` and the canvas
   widget; route them through `set_dirty(app, true)`. Simplest
   path: extend the existing `circuit_canvas_widget_set_status_cb`
   to also signal dirty (anything that prints a status counts as
   a mutation today). Or add a new `set_dirty_cb`.
4. Clear dirty on `action_save` / `action_save_as` success, and
   on `action_new` / `load_circuit_from_text` (those start a fresh
   file).
5. Tests: there's no good GUI test for window title (mock igraph
   doesn't track it). Cover the dirty-flag state machine in a
   unit test on `dcs_app_set_dirty` (or its equivalent).

**Done when.**
- Title shows `DCS — untitled.dcs` on fresh start.
- Opening `circuits/half_adder.dcs` changes to `DCS — half_adder.dcs`.
- Any mutation appends `*`.
- Save removes `*`.
- Suite green.

**Commit subject.** `Show current file in window title + track dirty state (R-10)`

---

## Stage 3 — Save-on-close prompt (R-11)

**Goal.** Quitting with unsaved changes prompts
`Save changes to <file>?  [Save] [Don't Save] [Cancel]`.

**Touches.**
- `src/framework/platform/iplatform.h` — declare
  `int show_message_box(self, title, message, button_labels[], n_buttons)`
  returning the button index (or -1 if cancelled).
- `src/framework/platform/platform_windows.c` — Win32 impl (use
  `MessageBoxA` with `MB_YESNOCANCEL` style; map result to index).
- `src/framework/platform/platform_linux.c` — stub or zenity.
- `src/framework/widgets/frame.c` (or wherever `quit_manager` is
  consulted) — when `on_attempt_quit` returns 0, prompt; route
  user's button choice back to "allow" / "deny" the quit.
- `src/app/dcs_app.c` — register the on-attempt-quit callback that
  reads the dirty flag (Stage 2) and shows the prompt.

**Steps.**
1. Add the iplatform method + both backends.
2. Wire the quit-attempt hook in `dcs_app_init`.
3. On `dirty == false`: return "allow quit" immediately.
   On `dirty == true`: show the message box, dispatch:
   - **Save** → call `action_save`; if it succeeds (dirty cleared),
     allow quit.
   - **Don't Save** → allow quit immediately.
   - **Cancel** → deny quit.
4. Test the state machine via a mock iplatform whose
   `show_message_box` returns a scripted button index.

**Done when.**
- Editing a file then Ctrl+Q / X-button → prompt appears.
- Each of the three responses behaves as specified.
- A clean file quits silently.

**Commit subject.** `Prompt to save on close when there are unsaved changes (R-11)`

---

## Stage 4 — Help menu + F1 reference dialog (R-14)

**Goal.** A **Help** menu next to View, and `F1` opens a modal
reference dialog listing every keyboard / mouse shortcut.

**Touches.**
- New `src/app/help_dialog.{h,c}` — minimal modal widget that
  captures all events (consumes click outside / ESC to close).
- `src/app/dcs_app.{h,c}` — add a `help_menu`, wire `F1` shortcut,
  show/hide the help_dialog widget.
- `Makefile` — add `help_dialog.c` to APP_SRC.

**Steps.**
1. Decide modal mechanism: easiest is a `panel_t` with a high
   z-order (drawn last) + a `widget` whose `handle_event` returns
   1 for everything when visible. Or repurpose the menu widget.
   Pick the simplest path; document the choice in the commit.
2. Author the help text. Content lives in a single static string
   inside `help_dialog.c`. Sections: Files, View, Edit (incl. the
   Ctrl+A and Del from Stage 1), Simulation, Mouse. See R-14 for
   the full text.
3. View menu gains "Show help" item (or new Help menu entry).
   Shortcut binding: `F1` in `poll_global_shortcuts`. ESC closes
   the dialog.
4. Test that the dialog can be opened / closed via the public API
   (no need to GUI-test the rendering).

**Done when.**
- F1 opens the dialog; ESC closes it; click outside closes it.
- Suite green + 2 tests for the state machine.

**Commit subject.** `Add Help menu + F1 reference dialog (R-14)`

---

## Stage 5 — Sidebar BLACK-BOX button prominence (R-2)

**Goal.** The bottom-of-sidebar toggle button is more visually
discoverable. Smallest stage in the plan.

**Touches.**
- `src/app/side_toolbar.c` — adjust colors / icon / size.

**Steps.**
1. Lighter background contrast in inactive state.
2. Optional: a tiny eye-icon glyph (`👁` style) using a couple of
   raylib draw_line calls. Or keep text and just bump font weight
   / color.
3. Manual visual test only — no automated check.

**Done when.**
- The button is visibly more discoverable than today.
- Suite green (no logic changes).

**Commit subject.** `Bump prominence of sidebar BLACK-BOX toggle (R-2)`

---

## Stage 6 — Rich CLI `--help` for AI tools (R-16)

**Goal.** `dcs_cli.exe --help` (and a longer `--help-format`)
contains the full `.dcs` file format spec so AI tools can generate
valid circuits without external context.

**Touches.**
- `cli/main.c` — expand help string, add `--help-format` flag.
- `circuits/*.dcs` — already serves as worked examples; reference
  them in the help text by relative path.

**Steps.**
1. Brief `--help` stays as today (one-line synopsis + flag list).
2. New `--help-format` prints:
   - `.dcs` file grammar (BNF-ish or by example).
   - Available gate kinds (`and`, `or`, `not`) + their pin counts.
   - All annotations (`# @layout`, `# @wires`, `# @display_mode`,
     `# @display_name`, `# @pin_style`).
   - 3–5 worked examples: AND gate, half adder, multiplexer-style
     circuit, a fan-out demo. Full text the AI can pattern-match.
   - Current constraints (`DOMAIN_MAX_PINS_IN = 2`, no feedback,
     wire-name uniqueness).
3. No new tests — existing CLI tests cover behaviour; this just
   changes the help string.

**Done when.**
- `./dcs_cli.exe --help-format` prints a multi-page reference.
- Existing CLI tests stay green.

**Commit subject.** `CLI: rich --help-format for AI-assisted .dcs generation (R-16)`

---

## Stage 7 — Version system (R-15)

**Goal.** Structured version reporting in GUI About + CLI
`--version`. Format: `DCS 1.0.0:20260520:71ed6c30:<sig>`.

**Substaging.** This stage splits into three commits to stay
small. Each one is self-contained.

### Stage 7a — Version header + CLI flag

**Touches.**
- `src/version.h` (new) — `DCS_VERSION_MAJOR/MINOR/PATCH/STR`.
- `cli/main.c` — `--version` prints the static version.

**Done when.** `./dcs_cli.exe --version` prints `DCS 1.0.0`.

**Commit subject.** `Add src/version.h + CLI --version flag (R-15, part 1)`

### Stage 7b — Generated `build_info.c`

**Touches.**
- `Makefile` — rule that regenerates `src/build_info.c` before
  every compile. Captures `date +%Y%m%d` and
  `git rev-parse --short=8 HEAD` into `const char *` globals.
- `src/build_info.h` (new) — declares the externs.
- `cli/main.c` — `--version` now prints
  `DCS 1.0.0:20260520:71ed6c30`.

**Done when.** Version output includes today's date + the actual
current short git commit.

**Commit subject.** `Generate src/build_info.c at make time (R-15, part 2)`

### Stage 7c — Tamper signature + GUI About

**Touches.**
- `Makefile` — extend the build_info rule to compute
  `SHA-256(version || time || commit || $DCS_BUILD_SALT)` and
  embed as `dcs_build_sig`. Document the limitation in a code
  comment (salt-in-binary is casual-tampering protection only).
- `src/main.c` (gui) — recompute the signature at startup, compare,
  log a warning to stderr on mismatch.
- `src/app/dcs_app.c` — add About menu item showing the full
  version string in a modal (or reuse Stage 4's help_dialog).

**Done when.** GUI Help → About shows the full string with all
four parts. Modified binaries (replace the version string in a
hex editor) log a warning at startup.

**Commit subject.** `Tamper signature + GUI About dialog (R-15, part 3)`

---

## Stage 8 — Display name as first-class property (R-1)

**Goal.** Make the external-view display name a user-editable
property visible in the status bar, not just a quiet override of
the file basename.

**Substaging.** Two commits.

### Stage 8a — Status-bar shows display-name distinction

**Touches.**
- `src/app/dcs_app.c` — status bar text formatting. When the
  display_name differs from the basename: show
  `Editing half_adder.dcs · display name: HalfAdder`.

**Done when.** Opening a file with `# @display_name = X` shows the
distinction in the status bar.

**Commit subject.** `Status bar: show display-name vs basename distinction (R-1, part 1)`

### Stage 8b — In-GUI display-name editing

**Touches.**
- View menu — new item "Set display name…".
- A small input-dialog widget (or simple modal text field).
- Saves to `circuit_canvas_widget_external_meta(cw)->display_name`,
  marks dirty (Stage 2).

**Done when.** User can edit the display name without leaving the
GUI. Saving the file persists it via the existing Phase-10
`# @display_name` annotation.

**Commit subject.** `Editable display name in View menu (R-1, part 2)`

---

## Stage 9 — Undo / Redo (R-5)

**Goal.** `Ctrl+Z` undoes the last edit; `Ctrl+Y` redoes. Touches
every mutation site in the canvas widget — the largest single
stage in this plan. Substaged carefully.

### Stage 9a — Command interface + stack

**Touches.**
- `src/app/command.h` (new) — `command_t` interface (do / undo /
  destroy vtable).
- `src/app/command_stack.{h,c}` (new) — bounded LIFO of `command_t*`
  with a separate redo stack. Cap at 100 entries; oldest fall off
  the bottom.
- `test/test_command_stack.c` (new) — push / undo / redo / cap
  behaviour with stub commands.
- `Makefile` — register the new test target.

**Done when.** Stack passes its own unit tests (5–10 cases).

**Commit subject.** `Command pattern infrastructure for undo/redo (R-5, part 1)`

### Stage 9b — Route mutations through commands

**Touches.**
- `src/app/circuit_canvas_widget.c` — convert each mutation site
  to build a `command_t` and push it on the stack instead of
  directly mutating:
  - `connect_wire` → `connect_wire_cmd_t`
  - `disconnect_input` → `disconnect_input_cmd_t`
  - `remove_component_at` / `remove_input_at` / `remove_output_at`
    → respective delete commands (recording the deleted node so
    undo can restore it)
  - drag-end (component move) → `move_node_cmd_t`
  - wire-edit shift → `wire_shift_cmd_t`
- Each command stores the **inverse** (old positions, removed
  pieces, etc.) in its struct so undo can reverse it.
- `test/test_circuit_canvas_supplement.c` — for each mutation,
  add an undo round-trip test (mutate, undo, verify state is
  identical to pre-mutation).

**Done when.** Every mutation has a do/undo pair. Round-trip
tests pass.

**Commit subject.** `Route all canvas mutations through commands (R-5, part 2)`

### Stage 9c — Ctrl+Z / Ctrl+Y shortcuts + UX polish

**Touches.**
- `src/app/dcs_app.c` `poll_global_shortcuts` — `Ctrl+Z` calls
  `command_stack_undo`; `Ctrl+Y` (and `Ctrl+Shift+Z` for muscle
  memory) calls `command_stack_redo`.
- Add Edit menu (next to View) with Undo / Redo items.
- Status bar message: "Undo: <command description>".

**Done when.**
- Ctrl+Z undoes the last action; Ctrl+Y redoes it.
- Edit menu shows Undo / Redo items with shortcuts.
- Suite green.

**Commit subject.** `Ctrl+Z / Ctrl+Y bindings + Edit menu (R-5, part 3)`

---

## Definition of done (whole plan)

The plan is complete when **all** of the following hold:

1. Stages 1–9 are committed (12–13 commits total counting the
   sub-stages).
2. Every refinement in scope (R-1, R-2, R-5, R-10, R-11, R-12,
   R-13, R-14, R-15, R-16) is marked ✅ in
   `step2-supplement-refinement.md`.
3. The full test suite is green (target growth: 561 → ~600+).
4. Both binaries (`dcs_cli.exe`, `dcs_gui.exe`) build with zero
   warnings.
5. Manual visual gates passed:
   - Each shortcut works (Ctrl+A, Del, Ctrl+Z, Ctrl+Y, F1).
   - Title bar reflects current file + dirty state.
   - Quitting with unsaved changes prompts correctly.
   - Help dialog displays current shortcut set.
   - About dialog shows the full version string.

After this plan, the codebase is positioned for Step 3:

- Sequential logic (Phase 3.4, CLK + D-FF) unblocks R-4 (real DFF)
  and ties into R-9 (feedback).
- Shape DSL (Phase 3.1) unblocks R-6 (pin orientation) and R-7 full.
- Variable pin counts (Phase 3.3) unblocks R-3 (pin stubs) and
  multi-input gates (NAND, XOR, etc).
- HTTP / IPC layer (Step 3 or 4) unblocks R-17.

---

## Risk register

| Stage | Highest-risk failure mode | Mitigation |
|---|---|---|
| 2 | Dirty flag drifts (some mutations don't set it) | Single dirty-callback funnel routed through ALL canvas mutation hooks. Spot-check each mutation by triggering it and checking title. |
| 3 | Message box blocks the main thread | Synchronous modal is acceptable for save-on-close (rare). Document so future async work doesn't regress. |
| 7c | Salt extracted from binary defeats the tamper check | Documented as a known limitation in the code comment AND in R-15. Real protection needs code signing. |
| 9b | Command undo restores wrong state due to side effects | Each command captures **complete** inverse state in its struct (not just diffs). Round-trip tests for every mutation type. |
| 9b | Mutations done outside the canvas widget (e.g., via a future API) bypass the command stack | All mutations must be routed through the stack. Document in CLAUDE.md "Things to avoid". Use the `assert_geometry_consistent` pattern: maybe an `assert_command_stack_consistent` runs in debug builds. |

---

## Notes for the implementer

- Commit one stage at a time, with the suggested subject line as
  the first line. Body bullets describe what changed (files,
  lines).
- Skip the auto-bumped `package.json` / `package-lock.json` from
  each commit per existing project conventions.
- For visual changes, the user runs `dcs_gui.exe` and confirms.
  Don't claim a stage complete until that confirmation lands.
- If a stage grows beyond ~250 lines of diff, look for a sub-split.
  Smaller commits are easier for both AI and human review.
- Refer back to `step2-supplement-refinement.md` for the
  motivation behind each R-* item before starting its stage. Don't
  re-derive; re-read.
