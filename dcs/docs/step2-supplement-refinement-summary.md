# Step 2 Supplement — Refinement Summary

Retrospective on the refinement work tracked in
[`step2-supplement-refinement.md`](step2-supplement-refinement.md) and
sequenced by [`step2-supplement-refinement-plan.md`](step2-supplement-refinement-plan.md).
Covers every stage that landed, the side-bugs that surfaced along the
way, and the design decisions that emerged from actually running the
plan against the code.

---

## 0. Final outcomes

| Metric | After Phase 13 (supplement) | After refinement work |
|---|---|---|
| Test count | 561 | **683** (+122) |
| Test suites | 8 | 10 (+ `test_help_dialog`, `test_sha256`, `test_command_stack`) |
| Source files (`src/`) | 29 | 36 (+ `help_dialog.{h,c}`, `command.h`, `command_stack.{h,c}`, `sha256.{h,c}`, `version.h`, `build_info.h`) |
| R-* items resolved | 0 / 17 + 2 added | **11 done, 3 partial, 6 deferred to Step 3** |
| Commits since `a9e89a6` (plan) | — | **22** |

Refinement status table:

| Item | Status | Where |
|---|---|---|
| R-1 Display name first-class | 🟡 Part 1 (visibility) done; part 2 (in-GUI edit) gated on R-18 | `74162b3` |
| R-2 Sidebar BLACK-BOX prominence | ✅ | `cda9ce1` |
| R-3 Pin stubs | ⏸ Step 3 (Phase 3.3 variable pins) |
| R-4 Real D-FF | ⏸ Step 3 (Phase 3.4 sequential primitives) |
| R-5 Ctrl+Z / Ctrl+Y | ✅ | `b8d7166` + `0254b07` + `1415192` |
| R-6 Vertical pin orientation | ⏸ Step 3 (Phase 3.1 shape DSL) |
| R-7 Network classification + case enum | 🟡 auto-align landed earlier; case enum gated on R-6 |
| R-8 Shared V bus | ✅ (landed pre-plan) |
| R-9 Feedback circuits | ⏸ Step 3+ (sequential evaluator) |
| R-10 Window title shows file | ✅ | `14f128d` (+ `74162b3` display-name extension) |
| R-11 Save-on-close prompt | ✅ | `2793cc1` + `58395cd` + `aa24041` |
| R-12 Ctrl+A select all | ✅ | `0dbad16` |
| R-13 Del key deletes selection | ✅ | `0dbad16` + `a797909` (focus fix) |
| R-14 Help menu + F1 dialog | ✅ | `bddbe96` |
| R-15 Version system + tamper sig | ✅ | `4ca3bcc` + `640f244` + `9114d99` |
| R-16 Rich CLI `--help` | ✅ | `3410b43` |
| R-17 GUI HTTP endpoint | ⏸ Step 3 or 4 (user-tagged) |
| R-18 Focus dispatch decision | 🟡 interim global routing in place; model choice deferred |
| R-19 Arrow-key nudge | ✅ | `68444de` |

---

## 1. Stage-by-stage summary

### Stage 1 — Ctrl+A select-all + Del delete-selection (R-12, R-13)

**Built.** New public `circuit_canvas_widget_select_all(self)`. Both
shortcuts dispatched globally from `dcs_app::poll_global_shortcuts`.

**Difficulties.** Mid-stage user testing revealed Del wasn't actually
firing in the GUI even though tests passed. Investigation found
**`focus_manager_set` is never called anywhere in the codebase** —
so the framework's focused-widget key-event dispatch in `frame.c`
is dead code. Tests had been passing only because they call
`widget_handle_event(&cw->base, &ev)` directly, bypassing focus.

**Design change.** Routed Del globally too (matching Ctrl+A's pattern),
and removed the now-known-dead `IK_DELETE` branch from
`ccw_handle_event`. Single source of truth.

**Side fix.** ESC was also broken for the same reason — fixed in
`cea5d4b` with a new `circuit_canvas_widget_cancel_mode`. Raylib's
default `WindowShouldClose() == ESC` behaviour was *also* turning
ESC into "quit the program" before the canvas could see it — fixed
in `f11f5cb` with `SetExitKey(KEY_NULL)`.

**Followups.** R-18 entry added to the refinement doc to record the
framework-focus dead-code finding for proper resolution in Step 3.

---

### Stage 1.5 — Arrow-key 1-px nudge (R-19, added during the plan)

**Built.** `circuit_canvas_widget_nudge_selection(self, dx, dy)`,
edge-triggered global handler (one tap = one pixel).

**Difficulties.** None. Trivial layering on top of Stage 1's pattern.

**Design change.** Plan slotted this after Stage 1 because it's the
same global-shortcut shape and complements R-7's coarser 8 px
auto-align.

---

### Stage 2 — Window title + dirty flag (R-10)

**Built.** New `igraph_t::set_window_title` seam (raylib's
`SetWindowTitle` on Windows; cross-platform via raylib). New canvas
widget `on_mutated` callback fires from every structural / geometric
mutation entry point (12 call sites). `dcs_app` gains `int dirty`,
`dcs_app_set_dirty()` funnel, and `refresh_window_title()`.

**Difficulties.** Initial implementation only refreshed the title
when the dirty *flag* changed. But opening a file changes the
basename too while dirty stays 0 — the title kept showing the
previous "DCS — untitled". Resolution: always refresh on `set_dirty`,
not just on flag changes. SetWindowTitle is cheap.

**Design change.** The mutation callback funnel is broader than the
plan originally sketched: every place / connect / disconnect / delete
/ drag-end / wire-edit-release / right-click-wire-delete / nudge
site calls `notify_mutated(cw)`. View-only operations (set_highlight,
cancel_mode, set_display_mode, set_display_name) stay silent — those
are state transitions, not user mutations.

---

### Stage 3 — Save-on-close prompt (R-11)

**Built.** New `iplatform_t::confirm_yes_no_cancel(self, owner, title,
message)` returning a `dialog_result_t` enum. Windows impl uses
`MessageBoxA` with `MB_YESNOCANCEL`; Linux stub. New
`quit_manager_t::on_attempt` callback gates the quit decision. dcs_app
wires the dirty-flag check into the prompt.

**Difficulties.** Two big ones, both about MessageBox centering.

1. **First fix attempt (`58395cd`)** passed the raylib HWND as
   the owner but the dialog still screen-centred. Confirmed via a
   temporary diagnostic `fprintf(stderr, "owner=%p IsWindow=%d ...")`
   that the HWND *was* valid and recognised. Root cause:
   **MessageBoxA does not reliably centre on the owner HWND** on
   any Windows version we care about — it's a long-standing Win32
   gotcha.

2. **Second fix (`aa24041`)** uses the classic Win32 CBT-hook
   workaround: `SetWindowsHookExA(WH_CBT, …, GetCurrentThreadId())`
   before the call, catch `HCBT_ACTIVATE`, reposition with
   `SetWindowPos`, `UnhookWindowsHookEx` afterwards. ~25 lines, no
   manifest needed (vs `TaskDialog` which requires a v6
   common-controls manifest).

**Design change.** Threaded the owner HWND through the file dialogs
too (`open_file`, `save_file`) so Save As / Open also centre on the
GUI window. Common `native_owner(app)` helper in dcs_app, used by
all three OS-dialog call sites.

---

### Stage 4 — Help menu + F1 reference dialog (R-14)

**Built.** New `src/app/help_dialog.{h,c}` — modal widget with
full-screen bounds that captures all mouse events when visible.
Centred 560×660 box renders shortcuts grouped into six sections
(Files / View / Edit / Simulation / Mouse / Help). F1 toggles,
ESC / click-outside dismisses. Added LAST to the root panel so
it's topmost in depth-first dispatch + draw order. New IK_F1 in
igraph.

**Difficulties.** User report mid-stage: dialog box was 100 px too
short, last section overflowed below the box border. Fixed by:
- bumping `BOX_H` from 560 to 660
- tightening `LINE_H` (18 → 17) and `SECTION_GAP` (10 → 6)
- adding `push_scissor` / `pop_scissor` around content rendering
  so future overflow stays inside the border defensively
- shrinking the box to fit small viewports (≥ 40 px margin from
  edges, min 200×200)

**Design change.** When the dialog is visible, `poll_global_shortcuts`
swallows every shortcut except F1 / ESC. Otherwise Ctrl+S etc. would
still fire behind the modal — confusing.

---

### Stage 5 — Sidebar `[ ] BLACK-BOX` prominence (R-2)

**Built.** Horizontal separator line above the toggle. Distinct
inactive bg colour (darker than place buttons). Larger label font.
Taller button. Thicker border. White border when active.

**Difficulties.** None — pure visual tweaks.

**Design change.** Swapped the U+25A0 ■ glyph for ASCII `[X]`
defensively — Phase 11 already established that raylib's default
bitmap font misses higher Unicode codepoints (renders as `?`).
User hadn't reported the issue yet but it was a latent bug.

---

### Stage 6 — Rich CLI `--help-format` (R-16)

**Built.** `print_usage(FILE *)` now takes a stream so the same body
serves both the `--help` path (stdout, exit 0) and the error path
(stderr, exit 1). New `print_format_help(FILE *)` prints a 165-line
self-contained reference covering file structure, gate kinds, all
annotations (`@layout`, `@wires`, `@display_mode`, `@display_name`,
`@pin_style`), constraints, and four worked examples (AND, half-adder,
2:1 mux, DFF stub with full annotations).

**Difficulties.** One test failure: `out_long=$($EXE --help)` vs
`out_short=$($EXE -h)` compared unequal even though their stdout
content was identical. The existing `check()` helper in `test_cli.sh`
strips CRs from `actual` but not from `expected`. Fixed by stripping
both sides explicitly.

**Design change.** Help flags handled BEFORE the positional-arg
check so they work with zero args.

---

### Stage 7 — Version system + tamper signature (R-15, sub-staged)

**Built across three commits.**

- **7a (`4ca3bcc`)** — `src/version.h` with `DCS_VERSION_*` macros;
  CLI `--version` prints `DCS 1.0.0`.
- **7b (`640f244`)** — Makefile generates `src/build_info.c` on every
  build using `date +%Y%m%d` and `git rev-parse --short=8 HEAD`.
  Cmp-then-mv pattern: mtime only updates when content actually
  changed, so same-day same-commit rebuilds don't relink. CLI
  `--version` now prints `DCS 1.0.0:20260521:640f244c`.
- **7c (`9114d99`)** — vendored public-domain SHA-256 at
  `src/sha256.{h,c}` (~120 lines, FIPS 180-4 vectors as tests). Make
  rule computes `SHA-256(version|date|commit|salt)` via `sha256sum`
  and embeds it. GUI startup verifies and prints a stderr warning
  on mismatch (CLAUDE.md §9.4 "log loudly"). About menu item shows
  the full four-field version stamp.

**Difficulties.**

1. **Test math bug.** SHA-256's NIST 1-million-'a' test originally
   ran `1000 × 1024 = 1,024,000` bytes instead of 1,000,000. Caught
   the off-by-24K immediately because the hash differed from the
   well-known vector.

2. **Salt env var via `DCS_BUILD_SALT=x make ...` doesn't work on
   MSYS2 make.** Tested with a minimal repro Makefile — neither
   inline `VAR=value` prefix nor `export` reached the recipe shell.
   Only Make's `command-line variable` syntax (`make
   DCS_BUILD_SALT=x cli`) was honoured. Documented this quirk in
   both `.env.example` and the Makefile comment, then added a
   `.env`-file reader as the primary override path so casual users
   don't have to know the gotcha.

3. **Initial centring guess wrong** (Stage 3-style). The first
   `win_confirm_yes_no_cancel` impl OR-ed `MB_TASKMODAL` thinking
   it'd help; same Win32 quirk as Stage 3 (see above). Fixed in
   `aa24041` along with the Stage-3 dialog.

**Design change.** Added `.env` / `.env.example` template files for
build-time secrets (gitignored / committed respectively), and a
top-level `build.sh` wrapper (release / debug / clean / test). The
build script tracks last-build mode in `.build_mode` so same-mode
repeats stay incremental but mode switches force `-B` (CFLAGS isn't
in make's dep graph, so an incremental build would silently keep
the old mode's objects).

---

### Stage 8a — Display name vs basename in title + status (R-1, part 1)

**Built.** `has_custom_display_name(app, out, len)` returns 1 + the
name when `display_name` is set AND differs from the file basename.
`refresh_window_title` formats `DCS — basename (display_name)` when
the names differ. `set_file_status(app, verb, path)` formats `"Opened
X (display: Y)"` similarly. Reused by load + save + save-as.

**Difficulties.** User report: window title showed "DCS — untitled"
on file open even after the fix should have taken effect. Root cause
(same as Stage 2): `dcs_app_set_dirty` only refreshed the title on a
dirty *flag* flip, not when basename / display_name changed while
dirty stayed 0 (file load: dirty was 0, stays 0, no refresh fires).
Removed the change-detector; title now always refreshes via
`set_dirty`.

**Design change.** Stage 8b (in-GUI display-name editor) is the first
stage that needs *focus-scoped* keyboard input — a text field where
typing Del means "delete a character" not "delete canvas selection".
That requires resolving R-18 (focus model decision) first. Deferred
8b to Step 3.

---

### Stage 9 — Undo / Redo via Command pattern (R-5, sub-staged)

**Built across three commits.**

- **9a (`b8d7166`)** — `src/app/command.h` (interface: `execute`,
  `undo`, `destroy`, `describe`), `command_stack.{h,c}` (bounded
  LIFO, cap 100, evict oldest on overflow, push clears redo).
  35 unit tests with a stub counter command.
- **9b (`0254b07`)** — snapshot-based command (single concrete
  command type) instead of per-mutation command types. Mutation
  callback serialises the canvas state and pairs it with the
  previously-cached state. Refactored `load_circuit_from_text` →
  `install_circuit_text(app, text, path_or_NULL)` so both file
  loads AND snapshot restores share the same install path. Public
  `dcs_app_undo()` / `dcs_app_redo()` APIs.
- **9c (`1415192`)** — Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z bindings,
  Edit menu (between File and View), help_dialog content updated.
  IK_Y + IK_Z keycodes added.

**Difficulties / design decisions.**

The biggest single design call was **snapshot-based vs
per-mutation commands**. Per-mutation would mean ~7 distinct
command types (`move_node_cmd_t`, `connect_wire_cmd_t`,
`delete_component_cmd_t`, etc.), each capturing precise inverse
state. Delete commands especially are intricate — they have to
snapshot the deleted component's entire neighbourhood (all
`in_wires` of OTHER components that referenced this one's output,
all `output_names` that matched it, etc.).

Snapshot-based ended up cleaner:

- Each command holds two serialised strings (before / after,
  via `circuit_io_serialize_ex`) plus a label. Single command
  type covers every mutation past and future.
- Round-trip correctness is *free* — the existing serialize /
  parse path is the most-tested code in the codebase.
- Memory is fine: ~few KB per entry × 100 cap = a few hundred
  KB worst case.
- Trade-off: generic "edit" description instead of granular
  per-action labels. Acceptable for v1; the status text already
  tells the user what just happened before the undo.

**Cache-management subtlety.** The canvas mutation callback fires
AFTER the mutation has happened, so capturing "before" requires
caching the previously-serialised state. dcs_app maintains
`char *last_snapshot` for this. After each mutation:
serialize-now → build cmd(last_snapshot, current) → push → cache
current. File-new and file-open clear the stack and refresh the
cache (the previous document's history is meaningless after a
fresh document loads). Save / save-as preserve the stack
(persistence doesn't change in-memory state).

**Undo simulation re-init.** After restoring a snapshot, the
simulation_t still holds pointers to the old (destroyed) circuit.
dcs_app_undo / dcs_app_redo bundle a `simulation_release` +
`simulation_init` + `timing_canvas_widget_set_waves(NULL)` so the
viewer state is consistent.

---

## 2. Side-bugs surfaced during refinement (not in original plan)

### 2.1 Framework focus dispatch is dead code (R-18)

Found while debugging Stage 1's Del key. `focus_manager_set` is
never called anywhere; `focus_manager_get` always returns NULL;
the focused-widget keyboard dispatch in `frame.c:119-129` never
fires. Every `EV_KEY_PRESS` handler inside a widget was unreachable
in the real GUI.

Interim fix (in `a797909` / `cea5d4b`): route every canvas keyboard
behaviour globally via `poll_global_shortcuts`. Three new public
widget functions: `circuit_canvas_widget_select_all`,
`circuit_canvas_widget_delete_selection`,
`circuit_canvas_widget_cancel_mode`. The framework's focus system
remains in place but unused.

Permanent resolution (R-18) deferred to Step 3 — needs a model
decision (retire focus vs. wire focus up properly) which becomes
mandatory when the first text-input widget lands.

### 2.2 raylib ESC = quit (`f11f5cb`)

`WindowShouldClose()` returns true on ESC by default, before our
quit_manager can consult its callbacks. Added `SetExitKey(KEY_NULL)`
in `rl_init` to disable.

### 2.3 my_test.dcs placement bug (`1b62472`)

User report: opening `circuits/my_test.dcs` and trying to place an
AND gate did nothing. The file has 3 inputs + 5 gates = 8 wires
which exactly hit `INIT_CAP = 8`. The canvas widget's `place_at`
had a "manual push" fast path that bypassed `circuit_add_component`
(because freshly-placed gates have no input wires to validate) AND
bypassed `push_wire`'s grow logic — silent fail when
`wire_count == wire_cap`.

Fix: new public `circuit_add_orphan_component(c, comp)` in
`domain/circuit.c` that creates the output wire via `push_wire`
(which DOES grow) and pushes the component (also growing the array
on overflow). Atomic on alloc failure. Regression test added.

### 2.4 Dialog centring on Windows (Stage 3 / Stage 7c)

`MessageBoxA` does not reliably centre on the owner HWND. Twice
"fixed" with naive HWND passing; the *real* fix is a thread-local
CBT hook (`SetWindowsHookExA(WH_CBT, …)`) that catches
`HCBT_ACTIVATE` and repositions via `SetWindowPos`. Same Win32
gotcha that the wider Windows-development community has been working
around for years.

---

## 3. Design themes that emerged

### 3.1 "Global shortcut" pattern hardened

The original codebase had some shortcuts in
`poll_global_shortcuts` (Ctrl+N/O/S/B, Ctrl+= / Ctrl+-, R, F) and
some in widget event handlers. The R-12/R-13/R-18/R-19/R-5 work
made `poll_global_shortcuts` the single source of truth for ALL
keyboard input. Widget keyboard handlers are now dead code by
convention until R-18 is resolved.

This wasn't intentional design — it was forced by the focus
discovery. But it's worked out cleanly: every shortcut is in one
place, easy to audit, easy to extend.

### 3.2 Mutation funnel via `on_mutated` callback

Stage 2's dirty-flag tracking required identifying every state-
changing entry point in the canvas widget. The resulting
`notify_mutated(cw)` calls (12 sites) are now ALSO the funnel
through which:

- the dirty flag gets set (Stage 2)
- the undo snapshot gets captured (Stage 9b)

So one piece of plumbing serves two consumers. If a future
feature needs to react to mutations (an autosave timer, a
"changes since last save" log, etc.) the hook is ready.

### 3.3 Layering held up

Every refinement that needed a new OS primitive (window title,
message box, file dialog owner, native window handle) was added
through the interface layer:

- `iplatform_t` grew `confirm_yes_no_cancel` (R-11),
  reshaped `open_file` / `save_file` to accept an owner (Stage 3
  follow-up).
- `igraph_t` grew `set_window_title` (R-10),
  `get_native_window_handle` (R-11 follow-up).

No app or framework code took a direct raylib or Win32 dependency.
The platform_linux stub had to grow alongside (returning DLG_ERROR
or 0) but is otherwise still a placeholder.

### 3.4 Snapshot-vs-granular is a real choice

Stage 9b's snapshot-based undo is unusual — most editors do
per-mutation commands. The DCS-specific factors that made
snapshotting the right call:

- Small documents: typical circuits are <100 components, serialize
  to a few KB.
- Already round-trip-tested: the .dcs format had ~134 tests of
  serialize/parse correctness.
- Many distinct mutation types: ~7-8 entry points, each with
  intricate inverse state (especially deletes).

For larger documents (Step 3's chipsets, hierarchical circuits)
or finer-grained user feedback ("Undo wire connection"
vs. "Undo edit") this choice may need revisiting. The
`command_stack_t` interface itself is type-agnostic — adding
per-mutation command types alongside snapshot commands later is
straightforward.

---

## 4. What's left

Six items are gated on Step 3 architectural work and can't land
as Step 2 polish:

| Item | Gate |
|---|---|
| R-3 Pin stubs | Step 3 Phase 3.3 (variable pin counts) |
| R-4 Real D-FF | Step 3 Phase 3.4 (sequential primitives) |
| R-6 Vertical pin orientation | Step 3 Phase 3.1 (shape DSL) |
| R-7 full case enumeration | R-6 |
| R-9 Feedback circuits | Sequential evaluator |
| R-17 GUI HTTP endpoint | User-tagged "Step 3 or 4" |

Two more items could be done now but are better folded into Step 3:

| Item | Reason |
|---|---|
| R-18 Focus model decision | First text-input widget will land in Step 3 anyway |
| R-1 part 2 (in-GUI display-name editor) | Gated on R-18 |

[`step3-plan.md`](step3-plan.md) records the candidate
"Ctrl+C → copy as image" idea added during this refinement work
as section 6.1.

---

## 5. Numbers

| | Pre-refinement (after Phase 13) | Post-refinement |
|---|---|---|
| Total tests | 561 | 683 |
| `test_circuit` | 43 | 56 (+ orphan-component regression) |
| `test_iplatform` | 8 | 9 (+ confirm_yes_no_cancel vt slot) |
| `test_igraph` | 39 | 41 (+ set_window_title + get_native_window_handle vt slots) |
| `test_widgets` | 32 | 38 (+ quit_manager attempt-cb tests) |
| `test_cli` (sh) | 8 | 17 |
| `test_circuit_canvas_supplement` | 104 | 130 |
| New: `test_help_dialog` | — | 16 |
| New: `test_sha256` | — | 7 |
| New: `test_command_stack` | — | 35 |

| Refinement-introduced source files | LoC (approx) |
|---|---|
| `src/version.h` + `src/build_info.h` | 50 |
| `src/sha256.{h,c}` | 130 |
| `src/app/help_dialog.{h,c}` | 250 |
| `src/app/command.h` + `src/app/command_stack.{h,c}` | 200 |
| `build.sh` + `.env.example` | 80 |

22 commits, ~2000 lines of net diff, no layering violations introduced.
