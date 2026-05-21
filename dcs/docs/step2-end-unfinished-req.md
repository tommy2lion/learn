# Step 2 — Unfinished Requirements (Carry-over to Step 3)

A comprehensive enumeration of every requirement raised in any Step 2
document that is **not yet implemented or only partially implemented**
in the current codebase. This list feeds the Step 3 plan.

**Audit scope (every Step 2 doc):**

- `docs/step2-refactor-plan.md`
- `docs/step2-refactor-design.md`
- `docs/step2-review-of-refactor-design.md`
- `docs/step2-code-review-req.md`
- `docs/step2-code-review.md`
- `docs/step2-supplement-req.md`
- `docs/step2-supplement-design.md`
- `docs/step2-supplement-implementation-plan.md`
- `docs/step2-supplement-implementation-summary.md`
- `docs/step2-supplement-refinement.md`
- `docs/step2-supplement-refinement-plan.md`
- `docs/step2-supplement-refinement-summary.md`

**Verification methodology.** Where an item's status was uncertain, I
cross-checked against current source (mostly `src/`). For each
unfinished item I cite the originating doc and note the gate (if any)
that blocks it.

**Legend.**
- ⏸ Deferred — explicitly named for later
- 🟡 Partial — some progress; remainder gated
- ❓ Status uncertain — needs re-audit during Step 3 planning

---

## 1. Code quality & safety — `step2-code-review.md`

The code-review report listed multiple findings. **Many were silently
resolved during the supplement and refinement work** without being
formally checked off in `step2-code-review.md` itself. The accurate
picture (verified against current source):

### 1.1 Resolved (no longer in the unfinished list)

| Finding | Resolution | Source verification |
|---|---|---|
| Realloc-leak pattern in `panel.c`, `circuit.c` | All `realloc` sites use the `tmp = realloc; if (!tmp) return -1` pattern with no leak. | `src/framework/widgets/panel.c:67-73`, `src/domain/circuit.c:42-55` |
| Stale-pointer cast in `input_panel` (direct field write from dcs_app) | `input_panel_set_circuit()` setter exists; dcs_app uses it (2 call sites). | `src/app/input_panel.h:32`, `src/app/dcs_app.c:233,332` |
| `simulation_run` ignores `waveform_add_track` return | Return value is checked; rolls back with `waveform_release` + re-init on failure. | `src/domain/simulation.c:21-36` |
| `auto_layout` malloc returns unchecked | All three allocs (`depths`, `col_total`, `col_idx`) check return; OOM path frees + returns. | `src/app/circuit_canvas_widget.c:99,112` |
| Bit-packing overflow in `wire_at` (`(i << 8) \| p` truncating >256 comps) | Refactored to use an out-parameter `int *pin_out` instead of packed return. | `src/app/circuit_canvas_widget.c:715` |
| Unbounded loop in `next_name` | Capped at 10 000 retries with fallback to a counter-suffix name; commented. | `src/app/circuit_canvas_widget.c:600-606` |
| `menu_add_item` silently drops past `MENU_MAX_ITEMS` | Returns `int` (index or -1); caller can detect rejection. | `src/framework/widgets/menu.c:151-152`, `menu.h:43-44` |
| `tagt_` violations — `InputSpec`, `stim_ctx_t`, `stb_item_t` (3 of 4) | All now use the `tagt_<noun>` prefix. | `cli/main.c:20`, `src/app/dcs_app.c:498`, `src/app/side_toolbar.c:17` |

### 1.2 Still unfinished

| # | Finding | Origin | Gate |
|---|---|---|---|
| **U-1** | `input_toggle_t` (in `input_panel.h:15`) — no `tagt_` prefix; only remaining naming-convention violation from §1 of code review | `step2-code-review.md` §1 | Trivial fix; no gate |
| **U-2** | `circuit_io_serialize` cap is a fixed estimate (`4096 + N*256 + (in+out)*96`); long names + many components could overflow with no growth path. | `step2-code-review.md` §2 medium | Replace fixed estimate with a growable buffer (realloc on snprintf truncation) |
| **U-3** | `waveform_set_value` / `waveform_get_track` have a `t->values == NULL when step_count == 0` fragile path | `step2-code-review.md` §2 medium | Bounds-check still passes; minor robustness work |
| **U-4** | `circuit_canvas_widget` carries its own camera + scissor logic instead of extending `framework/widgets/canvas_widget`. Two camera implementations live in parallel. | `step2-code-review.md` §4 | Pragmatic Step-2 decision; consolidation possible in Step 3 polish |
| **U-5** | **App-layer test coverage** — code review noted "App layer has zero tests" (§5). Resolution: `test_circuit_canvas_supplement` (130 cases) + `test_help_dialog` (16) cover canvas widget + help dialog. **Still no tests** for `dcs_app`, `side_toolbar`, `input_panel`, `divider_widget`, `timing_canvas_widget`. | `step2-code-review.md` §5 | Step 3 Phase 3.0 (hardening) |

---

## 2. Refactor design — `step2-refactor-design.md` + `step2-review-of-refactor-design.md`

### 2.1 "Phase 2.7 polish" items (explicit deferrals from the design doc)

| # | Item | Where the doc placed it | Current state | Gate |
|---|---|---|---|---|
| **U-6** | **Partial refresh / dirty-region invalidation.** Widget gains a `dirty` flag (reserved); framework redraws only invalidated regions to a backbuffer; flush per tick. | `step2-refactor-design.md` §A.4 + §8 (Phase 2.7) | `widget_t::dirty` field exists, never read. Full-frame redraw every tick. | Step 3 Phase 3.0 |
| **U-7** | **Linux platform impl.** `platform_linux.c` with zenity-backed file dialogs + a stub fallback; `confirm_yes_no_cancel` via zenity --question. | `step2-refactor-design.md` §A.2 (Phase 2.7) | Stubs return `0` / `DLG_ERROR`. Build links but no Linux-native dialogs. | Step 3 Phase 3.0 (or whenever first Linux user appears) |
| **U-8** | **Object factories** — `framework_factory_t`, `app_factory_t`. Centralised allocation, testable via mock factories, decouples consumers from struct layout. | `step2-refactor-design.md` §A.6, `step2-refactor-plan.md` req #13 | Direct constructors everywhere (`button_create`, `menu_create`, `circuit_canvas_widget_create`, …). No factory exists. | Step 3 Phase 3.0 (testability prerequisite) |
| **U-9** | **State pattern for editor modes.** Each mode (idle / placing / wiring / dragging / marquee / resizing / wire-edit) as its own class with `enter / handle_event / exit`. | `step2-refactor-design.md` §B.2 | Plain enum (`canvas_mode_t`) + `switch` in `ccw_handle_event`. Comment in code acknowledges it as future-polish. | Step 3 Phase 3.0 |
| **U-10** | **`input_box` widget.** Generic single-line text input — the framework's missing widget. Needed for in-GUI rename / display-name edit / chipset parameter entry. | `step2-refactor-design.md` §A.4 + `step2-supplement-refinement.md` R-1 part 2 | Never built. R-1 part 2 (display-name edit) is gated on this. | Step 3 Phase 3.0 (prerequisite for R-1 b, R-7 chipset params, custom-stimulus inline edit) |
| **U-11** | **Splitter widget integration.** `splitter_t` exists in `framework/widgets/` but the DCS layout uses a bespoke `divider_widget` (with its own drag callback shape). Consolidate. | `step2-refactor-design.md` §A.4 | Both widgets coexist; `divider_widget` is the one actually used. | Step 3 Phase 3.0 |
| **U-12** | **Focus indicators.** Visible focus ring around the currently-focused widget; Tab traversal. | `step2-refactor-design.md` §A.5 | Framework `focus_manager` exists but nothing ever calls `focus_manager_set`. Dead code. | Step 3 Phase 3.0 — see U-13 for the model decision |

### 2.2 Review-driven additions (`step2-review-of-refactor-design.md`)

Most of the 11 review points were integrated back into the design and
shipped during Phase 2.x. Items NOT yet implemented:

| # | Item | Status | Gate |
|---|---|---|---|
| **U-13** | **Framework focus dispatch is dead code (R-18).** Two possible end-states: (a) retire focus, keep all keys global (~30-line deletion); (b) wire focus up at click/Tab boundaries (~50-100 lines + Tab traversal). | Interim global routing in place (`a797909`, `cea5d4b`). | Step 3 Phase 3.0 (becomes mandatory when first text-input widget lands — U-10) |

---

## 3. Refinement R-* items — `step2-supplement-refinement.md`

Verbatim from the doc's status badges (cross-checked against the
refinement-summary):

| # | Item | Status | Gate |
|---|---|---|---|
| **U-14** | **R-1 part 2** — In-GUI display-name editing | 🟡 Part 1 (visibility) shipped; part 2 deferred | U-10 (`input_box`) + U-13 (focus model) |
| **U-15** | **R-3** — Pin stubs poke outside the gate body so wires terminate ~6 px before the outline | ⏸ | Step 3 Phase 3.3 (variable pin counts — pin geometry refactor happens there) |
| **U-16** | **R-4** — `circuits/dff_stub.dcs` is a fake D-FF; build a real one | ⏸ | Step 3 Phase 3.4 (sequential primitives) |
| **U-17** | **R-6** — Vertical pin orientation (inputs from top/bottom instead of left/right) | ⏸ | Step 3 Phase 3.1 (shape DSL — per-component pin layout) |
| **U-18** | **R-7 full** — Network type classification + per-pin orientation case enumeration | 🟡 Auto-align partial shipped; case enum gated on U-17 | Inherits U-17's gate |
| **U-19** | **R-9** — Feedback circuits (output → earlier-stage input). Today the parser rejects them. | ⏸ | Sequential evaluator (Step 3 Phase 3.4 follow-up) |
| **U-20** | **R-17** — GUI HTTP endpoint so an AI can POST a generated `.dcs` and have the GUI auto-open it | ⏸ | User-tagged "Step 3 or 4" — needs HTTP/IPC layer |

---

## 4. Forward-looking requirements 7-1 → 7-8 — `step2-code-review-req.md` §7

These were enumerated as Step-2-era requirements that the code review
assessed for architecture readiness; all are explicitly Step-3 work:

| # | Item | Scope | Step 3 phase |
|---|---|---|---|
| **U-21** | **7-1 ANSI/IEEE standard symbol support** — shape DSL, per-component shape field on `component_vt_t`, shape interpreter in the renderer | Medium effort | Phase 3.1 |
| **U-22** | **7-2 Simulation package** — extends 7-1 to user-defined components (registry, shape per kind) | Medium effort | Phase 3.2 (rolled into 3.1) |
| **U-23** | **7-3 Chipsets & physical-package view** — chipset definition file, instantiation in main circuits, recursive evaluation, optional pinout diagram | **Large effort** | Phase 3.6 (largest Step-3 block) |
| **U-24** | **7-4 CLK clock generator primitive** — stateful component (current ones are pure); establishes the per-instance state + `step()` pattern | Medium effort | Phase 3.4 |
| **U-25** | **7-5 Real circuit elements (R, L, C, LEDs)** — non-Boolean analog signals; `signal_t` semantics broaden to per-pin physical type | **Large effort** (likely design-only in Step 3) | Phase 3.8 |
| **U-26** | **7-6 Toolbar icons (miniature ANSI/IEEE shapes)** — falls out from U-21's shape system | Trivial once U-21 lands | Phase 3.2 |
| **U-27** | **7-7 Custom input sequences** — bit-string stimulus per input in the input panel; depends on U-10 (`input_box`) for inline editing | Small (UI + stim callback) | Phase 3.5 |
| **U-28** | **7-8 File-type planning** — `.dcsc` (component definition) / `.dcss` (simulation) / `.dcsw` (waveform) vs staying on `.dcs` with annotation blocks. Open decision. | Discussion + minor parser/serializer work | Phase 3.7 (resolves and applies retroactively) |

---

## 5. Implementation summary findings — `step2-supplement-implementation-summary.md`

Items the implementation summary called out as deferred or known
limitations:

| # | Item | Where in the summary | Gate |
|---|---|---|---|
| **U-29** | **Two-pass renderer fallback** — when a wire has no routed geometry (newly-placed without going through Phase-4 mutation hooks), the renderer draws a diagonal direct line. Acceptable today; could be tightened by triggering reseat on every detection. | Phase 3 design-change note | Step 3 Phase 3.0 polish (probably auto-fixed by U-30 below) |
| **U-30** | **Phase-12 manual bend-drag vs Phase-13 Steiner trunk trade-off.** Multi-consumer nets routed via Steiner trunk-bus have **non-draggable trunk segments** (only V-bus and consumer stubs can be moved as units). | Phase 13 + R-8 commits; `step2-supplement-refinement-summary.md` §1 Stage 9 (referenced) | Step 3 — once shape DSL is in, route segments can become richer/draggable |
| **U-31** | **Auto-align threshold (8 px) hard-coded.** Reasonable today but might want a setting later. | Phase 13-E + R-7 partial commit | Trivial; defer until someone complains |
| **U-32** | **`DOMAIN_MAX_PINS_IN = 2` hard cap.** All gates are 2-input today; chipsets and multi-input gates need a refactor (dynamic alloc per-component or bump to 16). | `step2-code-review.md` §6 + `step2-supplement-refinement-summary.md` | Step 3 Phase 3.3 — prerequisite for U-15, U-23, U-24 |

---

## 6. Refinement summary callouts — `step2-supplement-refinement-summary.md`

Items the refinement summary listed as carry-overs into Step 3:

| # | Item | Source | Gate |
|---|---|---|---|
| **U-33** | **Per-mutation undo command types** (richer undo descriptions than the generic "edit" label). Snapshot-based commands shipped in Stage 9b cover correctness; granular labels would let the status bar say "Undid: wire connection" or "Undid: 3-node delete". | `step2-supplement-refinement-summary.md` §3.4 | Step 3 polish; the command_stack interface is already type-agnostic |
| **U-34** | **Ctrl+C copy selection as image / OLE object** (discussion-only candidate). Three feasibility paths sketched: CF_DIB bitmap (low effort), SVG/EMF vector (medium), OLE server (high). PrintScreen fallback noted. | `step3-plan.md` §6.1 | Step 3 — same priority as U-20 (HTTP endpoint), both are "external-workflow integration" |

---

## 7. Items NOT yet recorded elsewhere

Items I noticed during the audit that aren't explicitly tracked in any
existing doc:

| # | Item | Why it matters |
|---|---|---|
| **U-35** | **No CHANGELOG / RELEASES doc.** The version stamp (R-15) is in place but there's no human-readable record of what each version contains. Useful before any v1.0 release announcement. | Pre-release polish |
| **U-36** | **No installation / quickstart guide.** README is a document trail (design history) — there's no "how do I install + run this" for an external user. | Pre-release |
| **U-37** | **Demo / example circuit gallery.** `circuits/` has scattered fixtures (half_adder, dff_stub, my_test, demo1-and3) but no curated "open this to see how DCS works" tour. | Pre-release; helpful for teachers / new users |
| **U-38** | **No automated CI / GitHub Actions pipeline.** Build + test runs locally only. | Pre-release polish |

### Late additions (2026-05-21, user-reported)

| # | Item | Where | Why it matters |
|---|---|---|---|
| **U-39** | **External-IO pin cap = 16 is too low.** `DOMAIN_MAX_IO` in `src/domain/component.h:9` caps a circuit at 16 external inputs AND 16 external outputs. Realistic future use (chipset definitions, eventual CPU-scale designs) needs orders of magnitude more — user proposed bumping the cap to **~256**. Note: this is distinct from **U-32** (`DOMAIN_MAX_PINS_IN = 2`, the per-gate input cap). | `src/domain/component.h:9` — `#define DOMAIN_MAX_IO 16` | Foundational; same dynamic-array pattern as U-32 (Phase 3.3 variable pins). Could bundle both into one allocation refactor. |
| **U-40** | **Wire auto-router does NOT avoid component bodies.** A user-built XOR (constructed from `not_a` / `not_b` / `a_and_not_b` / `not_a_and_b` / OR primitives — DCS has no XOR primitive yet) shows the auto-routed wire from `not_a`'s output crossing diagonally THROUGH the `not_b` gate body on its way to `not_a_and_b`'s input. Symmetric issue on the other half. See `issues/202605211856-issue-xor-gate-layout-wireline-cross-not-gate.png`. Root cause: `auto_route_wire` only knows about the producer/consumer pin positions; `auto_layout` doesn't consider wire-crossing minimisation when assigning component positions. Two router improvements possible: (a) component-avoidance in `auto_route_wire`, (b) crossing-aware placement in `auto_layout`. | `src/domain/wire_geometry.c::auto_route_wire`; `src/app/circuit_canvas_widget.c::auto_layout` | Visible UX glitch on the first composite circuit a teacher might build. Worth fixing before any v1 demo. Likely (a) is enough; (b) is a deeper algorithmic upgrade. |

---

## 8. Recommended Step 3 integration

A first-pass mapping of these 38 items to the proposed Step 3 phases
(see `step3-plan.md`):

| Step 3 phase | Items folded in |
|---|---|
| **3.0 Hardening + polish** | U-1, U-2, U-3, U-5, U-6, U-7, U-8, U-9, U-10, U-11, U-12, U-13, U-29, U-31, U-35, U-36, U-37, U-38, U-40 |
| **3.1 Shape DSL + renderer (U-21)** | U-17, U-26, U-30 |
| **3.2 Toolbar icons (U-26)** | (folded into 3.1) |
| **3.3 Variable pin count + IO cap bump** | U-15, U-32, U-39 |
| **3.4 CLK primitive (U-24)** | U-16, U-19 |
| **3.5 Custom input sequences (U-27)** | (depends on U-10 from 3.0) |
| **3.6 Chipsets (U-23)** | U-18 (full R-7 case enum needs chipset variety) |
| **3.7 File-type plan (U-28)** | (formalises decisions used by 3.6) |
| **3.8 Real circuit elements (U-25)** | Design-only |
| **3.9 (new) External integration** | U-20 (HTTP), U-33 (richer undo descriptions), U-34 (clipboard image) |

**3.0 is intentionally the largest bucket** — it sweeps up all the
code-quality + framework-polish work that didn't fit Step 2's
feature-driven phases. Better to land 3.0 in small commits across a
few sessions than batch it with feature work.

---

## 9. Numbers

- **Total unfinished items recorded:** **40**
- **Strictly code-quality (no new feature):** 5 (U-1 through U-5)
- **Framework polish ("Phase 2.7" originally):** 7 (U-6 through U-12)
- **Refinement R-* still open:** 7 (U-14 through U-20)
- **Forward 7-1..7-8 requirements:** 8 (U-21 through U-28)
- **Discovered during this audit:** 4 (U-35 through U-38)
- **Late additions (user-reported 2026-05-21):** 2 (U-39, U-40)
- **Discussion-only / candidate ideas:** 2 (U-33, U-34)
- **Already explicitly Step-3 work in `step3-plan.md`:** 8 (U-21..U-28)

The audit caught **multiple already-resolved items** that the original
docs still flag as open — listed in §1.1. Step 3 planning can skip
those without re-checking.

---

## Note

After this revision, **this document's contents have been merged into
`docs/step3-plan.md`** as the single working source. This file remains
as a historical artefact of the carry-over audit. Future planning work
should consult `step3-plan.md` directly.
