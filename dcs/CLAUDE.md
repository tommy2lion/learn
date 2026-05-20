# CLAUDE.md

Guidance for Claude Code (claude.ai/code) when working in this repository.
**Read this whole file before touching code.** It captures conventions and
principles that aren't re-derivable from the source alone.

---

## 1. Project context

`dcs` (Digital Circuit Simulation) — a hierarchical digital-circuit
simulation tool with a GUI editor, timing-diagram viewer, and headless
CLI. Scales from AND/OR/NOT gates up to (eventually) a CPU.

Two trees live side-by-side under `dcs/`:

- **`prototype_version/`** — frozen Step-1 prototype, commit `4a24207`.
  Self-contained: `cd prototype_version && make test` runs 98 tests.
  **Never modify** — it's the reference baseline.
- **`src/`, `test/`, `Makefile`** — the layered Step-2 codebase being
  built up phase-by-phase. All current work lands here.

Repo root is one level up at `learn/`, not inside `dcs/`. The sibling
`learn_claude/` directory holds raylib games whose modular-C style is the
ancestor of these conventions.

### 1.1 Design docs in `docs/`

Read in this order — each builds on the previous. Skim early sections of
each before doing substantial work in the affected area.

| Doc | Role |
|---|---|
| `prototype_version/architecture.md` | Original natural-language architecture |
| `prototype_version/step1-design.md` | Step-1 prototype design |
| `docs/step2-refactor-plan.md` | Step-2 requirements (14 points) |
| `docs/step2-refactor-design.md` | Step-2 layered design (definitive) |
| `docs/step2-review-of-refactor-design.md` | Review notes integrated back into design |
| `docs/step2-code-review-req.md` / `step2-code-review.md` | Post-impl review + verdicts |
| `docs/step2-supplement-req.md` / `step2-supplement-design.md` / `step2-supplement-implementation-plan.md` | Wire-geometry + display-mode supplement |
| `docs/step2-supplement-refinement.md` | Deferred polish observed during manual testing |
| `docs/step3-plan.md` | Forward plan (chipsets, shapes, real-element design) |

---

## 2. Build & environment

Windows + MSYS2 MinGW64. Build from `dcs/`:

```
make test                     # full suite (8 subsuites; must stay green)
make gui                      # dcs_gui.exe
make cli                      # dcs_cli.exe
make test_wire_geometry       # one specific subsuite
make clean
```

Compiler: `gcc -Wall -Wextra -O2 -std=c99`.
Link flags pattern: `-lm -lraylib -lopengl32 -lgdi32 -lwinmm`.
raylib headers/libs at `/c/msys64/mingw64/include` and `/c/msys64/mingw64/lib`.
Makefiles set `TEMP`/`TMP`/`TMPDIR=/tmp` to work around MSYS2 quirks.

**Manual GUI gates.** The user runs `dcs_gui.exe` themselves and reports
back. Claude cannot see the screen. When a phase requires visual
verification, surface the exact thing to check and **wait for the user
to confirm** before claiming the phase complete.

---

## 3. Layered architecture

Three layers + entry-point glue. No cycles.

```
domain/      pure logic (circuits, components, simulation, .dcs I/O,
             wire_geometry). No UI / raylib / Win32. Headlessly testable.
framework/   reusable widget framework — depends only on iplatform and
             igraph interfaces. Hot-swappable backend.
  core/      oo macros, rect, color, message, focus/quit managers
  platform/  iplatform interface + platform_windows impl
  graphics/  igraph interface + graph_raylib impl
  widgets/   widget base, panel, button, label, menu, splitter, frame,
             canvas_widget
app/         DCS-specific composition + subclasses. The only layer that
             knows about both framework and domain.
gui/main.c   wires platform + graph + dcs_app, runs frame
cli/main.c   wires platform + domain, runs the headless simulator
```

### 3.1 Layering invariants (NEVER violate)

1. **`domain/*` never includes** `framework/widgets/`, `framework/graphics/`,
   `framework/platform/`, or `app/*`. (Exception: `framework/core/oo.h`
   and `framework/core/rect.h` are header-only shared types — domain
   may include them.)
2. **`framework/*` never includes `domain/*` or `app/*`.**
3. **Only `framework/graphics/graph_raylib.c`** includes `raylib.h`.
4. **Only `framework/platform/platform_windows.c`** includes `windows.h` /
   `commdlg.h`. The future `platform_linux.c` is the only file that
   includes `gtk.h` / GTK-related headers.
5. Entry points (`gui/main.c`, `cli/main.c`) may include anything.
6. The framework must compile in isolation given stub `iplatform_t*`
   and `igraph_t*`.

The code-review (`docs/step2-code-review.md` §3) verified all six hold
as of commit `9f04b0a`. Every new change must preserve them.

### 3.2 Concrete examples

- **Domain → framework**: `domain/wire_geometry.h` includes
  `framework/core/oo.h` (for `class` / `interface` macros) and
  `framework/core/rect.h` (for `vec2_t`). That's the **only** kind of
  framework include allowed from domain — interface-free utility headers.
- **Adding a Cairo graphics backend**: drop `framework/graphics/graph_cairo.c`
  next to `graph_raylib.c`; swap one source in the Makefile. Zero other
  code changes.
- **Adding a Linux platform**: drop `framework/platform/platform_linux.c`;
  swap one source. (Skeleton already exists for Step-2.7.)

---

## 4. Coding conventions

### 4.1 OO macros (`framework/core/oo.h`)

```c
#define class      struct       /* a concrete data type with state + methods */
#define interface  struct       /* a vtable struct (function pointers + void *self) */
```

Both expand to `struct`. The distinction is **semantic** — `class` is
"this owns state"; `interface` is "this abstracts a contract".

Include `oo.h` **first** in framework / application TUs (before
`raylib.h`, `windows.h`, etc.) so the macros take effect on the right
tokens. The header has `#ifndef class` guards. **Remove the macros if
any C++ code is ever introduced** — `class` is a C++ reserved keyword.

### 4.2 Naming (strict)

| Kind | Style | Example |
|---|---|---|
| Struct tag | `tagt_<noun>` | `tagt_circuit` |
| Type alias | `<noun>_t` | `circuit_t` |
| Functions | `<class>_<verb>[_<obj>]` | `circuit_add_component`, `frame_dispatch_event` |
| Locals & fields | `snake_case` | `int input_count;` |
| Macros / constants | `UPPER_SNAKE_CASE` | `DOMAIN_NAME_LEN` |
| Files | `<class>.h` / `<class>.c`, one class per pair | `button.h`, `button.c` |
| Globals (rare) | bare lowercase, no `tagt_` | `static mem_manager_t mem_manager;` |

**Never** use Hungarian notation (`iCount`, `pszName`, `m_x`, etc.).
**Never** use CamelCase for variables, functions, or types.
The **first parameter** of any method is `self` of the class type:
`void widget_draw(widget_t *self, igraph_t *g);`

Uppercase is reserved for **macros only**. Even acronyms in identifiers
go lowercase: `gdp`, not `GDP`.

### 4.3 Header hygiene

- Header guard: `DCS_<MODULE>_<FILE>_H` (e.g. `DCS_DOMAIN_CIRCUIT_H`).
- A class `.h` declares **only** the public API. All internal helpers
  live in the `.c` as `static`.
- An `.h` includes only what its **declarations** need; implementation
  details (e.g. `raylib.h`, `windows.h`) belong in the `.c`.
- Forward-declare cross-module types whenever the full type isn't
  required by the header.

### 4.4 One class per file

Each major type gets its own `.h` / `.c` pair. Small helpers (a few
lines) may live inline in their consumer's `.c` as file-scope statics
— but anything substantial (>~50 lines or used by more than one TU)
graduates to its own file.

---

## 5. Design patterns in use

The codebase deliberately applies a small set of patterns. Use these
rather than inventing new ones when adding similar capabilities.

| Pattern | Where | Why |
|---|---|---|
| **vtable polymorphism** | `component_vt_t`, `widget_vt_t`, `iplatform_t`, `igraph_t` | C's equivalent to virtual methods. Always: const vtable + `void *self`. |
| **Factory** | `framework_factory_t`, `app_factory_t` (per refactor-design §A.6) | Centralized allocation; testable via mock factories; decouples consumers from struct layout. |
| **State pattern** | Editor modes (planned `app/modes/*`; currently a plain enum + switch) | Each mode is a class with `enter / handle_event / exit`. Modes never reach into the widget tree directly — they only manipulate `editor_state_t` (shared context). |
| **Observer / callback** | `menu_set_on_select(cb, user)`, `button.on_click`, `circuit_canvas_widget_set_status_cb`, `divider_widget_set_change_cb`, `frame_set_resize_cb` | Lower-layer notifies upper-layer via registered callback. **Always pass `void *user` data** alongside the function pointer — no globals. |
| **Composition (single inheritance)** | `class tagt_button { widget_t base; ... };` | The base struct is **first** so a `widget_t *` can be cast to the concrete type (and vice versa) safely. |

### 5.1 Specifically for dependency inversion

When the **lower layer** needs to notify the **upper layer**, the
lower layer offers a **callback-registration interface**, and the upper
layer registers its handler. The lower layer does not know the type of
its caller. Examples already in the codebase:

```c
/* lower-layer offers: */
void menu_set_on_select(menu_t *self, menu_on_select_t cb, void *user);
void button_set_on_click(button_t *self, void (*cb)(void *), void *user);
void divider_widget_set_change_cb(divider_widget_t *self,
                                  divider_change_fn cb, void *user);
```

This is the C-language realisation of the Hollywood principle ("don't
call us; we'll call you"). When you find yourself wanting to make a
lower-layer file `#include` an upper-layer file, **add a callback
slot to the lower-layer struct instead**.

### 5.2 Don't over-design

Patterns are tools, not goals. **Avoid** introducing an interface class
when a component manages its dependencies internally and has no
alternative implementation in sight. Specifically:

- A class with **one** implementation and no test-mock need does **not**
  need a separate vtable. Direct function calls are fine.
- A component that owns its helper structs internally does **not** need
  to expose a factory for them.
- If a public API is only ever called from one place, consider whether
  it needs to be public at all — a `static` helper is usually right.

Decision rule: introduce an interface only when **at least one** of
(a) the implementation is platform/library-specific (`iplatform`,
`igraph`), (b) tests need to mock it, or (c) two real implementations
exist or are imminent. Otherwise: direct types and direct calls.

---

## 6. Responsibility allocation & separation of concerns

The three layers each own one concern.

| Layer | Owns | Forbidden |
|---|---|---|
| `domain/` | Simulation correctness, file format, geometric data | UI primitives, OS APIs, raylib types |
| `framework/` | Generic widget mechanics, event dispatch, focus/quit, drawing abstraction | Knowing what a "circuit" is |
| `app/` | DCS-specific widgets (`circuit_canvas_widget`, `input_panel`), composition (`dcs_app`), mode logic | Direct raylib / Win32 calls; reinventing framework widgets |

### 6.1 Within a layer, classes own one responsibility

- `circuit_t` owns structural truth (components + wires + I/O lists +
  per-component pixel positions). It does **not** simulate (that's
  `simulation_t`), serialize (that's `circuit_io`), or render.
- `wire_geometry_t` owns the visual segment list for each net. It does
  **not** know about pin positions, components, or rendering — its
  callers compute those and call `auto_route_wire` / `set_segments`.
- `simulation_t` owns the time-evolution + waveform recording. It does
  **not** drive the GUI or mutate the circuit's structure.
- `circuit_canvas_widget_t` owns canvas-level interaction (modes,
  selection, drag, marquee). It does **not** know about file I/O —
  that's `dcs_app`'s job.

If you find yourself adding a second responsibility to an existing
class, split it instead. The Step-1 god-object `editor.c` is the
cautionary tale.

### 6.2 The app layer is the **only** integrator

App-layer code is the only place that may `#include` both a domain
header and a framework header in the same TU. The framework doesn't
know circuits exist; the domain doesn't know the GUI exists. They
meet only at `app/`.

---

## 7. Platform independence

### 7.1 Application layer is platform-agnostic

`app/*` code uses only `iplatform_t*` and `igraph_t*` — never raylib
or Win32 types. Adding a Linux build means writing one new file
(`platform_linux.c`) and re-linking; zero `app/` changes.

### 7.2 The two interface seams

- **`iplatform_t`** (`framework/platform/iplatform.h`) — wraps every
  OS-specific call: file dialogs, file I/O, time, clipboard.
- **`igraph_t`** (`framework/graphics/igraph.h`) — wraps every drawing
  and input-polling primitive. The **only** place that includes
  `raylib.h` is `graph_raylib.c`.

When you need a new OS or graphics capability:

1. Add the method to the interface struct in `iplatform.h` / `igraph.h`.
2. Implement it in **every** existing backend (`platform_windows.c`
   today; `platform_linux.c` stub-or-real).
3. Use it from app/framework code via the interface pointer.

**Don't** call raylib or Win32 directly from app or framework code,
even "just this once".

---

## 8. Dependency design

### 8.1 Depend on interfaces, not implementations

Wherever a class needs a graphics or platform capability, it stores
an `igraph_t *` or `iplatform_t *`, not a `graph_raylib_t *` or
`platform_windows_t *`. The concrete impl is picked at **link time**
only — by `gui/main.c` calling `platform_create()` and `graph_create()`.

For new collaborator slots in your own classes, follow the same rule
when more than one implementation is plausible (test mocks count as
implementations). When only one impl will ever exist, see §5.2.

### 8.2 Callback registration over direct knowledge (see §5.1)

When the lower layer must notify the upper, the lower exposes
`set_*_cb(self, fn, user)`. Never the reverse — the lower never
includes an upper header to call a known function.

### 8.3 Concrete impls are chosen at link time

`gui/main.c` calls `platform_create()` (in `platform_windows.c`) and
`graph_create()` (in `graph_raylib.c`). Switching backends = switching
which `.c` is in the Makefile. No `#ifdef`-laden code paths.

---

## 9. Safety & protective programming

The Step-2 code-review (`docs/step2-code-review.md` §2) found
several patterns to enforce going forward.

### 9.1 Realloc-and-NULL-check

Never assign `realloc(...)` directly back to the destination pointer
on failure — that leaks the old block and corrupts state. Standard
pattern:

```c
T *tmp = (T *)realloc(self->buf, sizeof(*tmp) * new_cap);
if (!tmp) return -1;            /* old buf still intact */
self->buf = tmp;
self->cap = new_cap;
```

Apply the same to `malloc` / `calloc` — check the return, fail
gracefully (or log + abort if there's no graceful path).

### 9.2 Validate at boundaries; trust internals

`set_segments` / `append_segments` validate every segment's H/V
invariant **once at the API boundary**, then trust the data. Same
shape: validate input names (non-NULL, non-empty, fits in buffer)
at the entry point. Don't sprinkle defensive checks through every
internal helper — that's noise without benefit.

### 9.3 Atomicity on failure

Mutating APIs should leave state **unchanged** when input is invalid
or allocation fails. Example: `wire_geometry_set_segments` validates
all segments first, then commits. Either succeed wholly or change
nothing.

### 9.4 Log even for "impossible" exceptions

When an error path is reachable but believed unlikely, **log loudly**
even if you choose not to handle it — these exceptions may never occur,
so silent failure makes them invisible. Concretely:

- Failing `realloc` → fail the operation AND log to stderr (or via
  `status` callback) with module + cause.
- Failing `waveform_add_track` mid-run → bail and report, don't
  silently continue with a partial waveform.
- An assertion that "can't happen" in normal flow → if disabling it
  in release would mask a real bug, keep it as `abort()` with a
  diagnostic message (see `assert_geometry_consistent` for the
  pattern).

The principle: **the loudest failure mode that can't be ignored is
best**. Silent corruption is the worst.

### 9.5 Defensive idioms already in the codebase

- `producer_for_wire(c, name).kind == NODE_NONE` checks before
  dereferencing.
- Stale-producer skip in the wire renderer (so deleted components
  don't leave ghost segments — see `draw_world` Pass 1).
- `assert_geometry_consistent(cw)` under `#ifndef NDEBUG` — fires
  after every mutation hook so drift surfaces immediately.
- `circuit_io_serialize_ex` emits **only** non-default fields, so a
  legacy `.dcs` round-trips byte-identical and unknown fields don't
  accumulate.

When adding a new mutation site, add a parallel `assert_*_consistent`
check (debug-only) so future drift is caught at the source.

### 9.6 Bounded loops, capped lookups

Auto-name generators, retry loops, recursive descents — all need an
explicit cap. Example: `next_name` retries up to 10000 times before
returning a fallback. Same for chipset recursion (when added) — check
for cycles, depth-cap.

---

## 10. Testability

### 10.1 Required test coverage

Every commit must keep the full suite green. As of Phase 11 the
suite has **454 tests** across:

| Layer | Suite | Notes |
|---|---|---|
| `iplatform` | `test_iplatform.c` | Vtable shape + concrete-impl smoke |
| `igraph` | `test_igraph.c` | Offline vtable + color + layout tests |
| Widget framework | `test_widgets.c` | Hit-test, focus, button click — no window opened |
| Circuit / components | `test_circuit.c` | Pure domain |
| `circuit_io` | `test_circuit_io.c` | Parse / serialize / round-trip incl. `@layout`, `@wires`, metadata |
| `wire_geometry` | `test_wire_geometry.c` | Pure data-structure tests |
| Canvas widget integration | `test_circuit_canvas_supplement.c` | Mock-igraph driven; verifies render dispatch + mutation hooks |
| CLI | `test_cli.sh` | End-to-end CLI smoke |

The **app layer was historically untested** (code-review §5). The
supplement work added 58 cases via `test_circuit_canvas_supplement.c`
using a mock `igraph_t`. Continue this pattern for any new app-layer
behaviour.

### 10.2 Design for testability

- **Pure functions** over passed-in state (no globals) — always.
- **vtable-based interfaces** (`igraph_t`, `iplatform_t`,
  `widget_vt_t`, `component_vt_t`) — mockable by writing a tiny
  alternative vtable. See `mock_igraph_init` in
  `test_circuit_canvas_supplement.c` for the canonical mock pattern.
- **Headless construction** — `circuit_canvas_widget_create()` works
  without opening a window because rendering goes through the
  injected `igraph_t*`, which is supplied later at draw time.
- **Public test hooks** when needed — `circuit_canvas_widget_reseat_wires`
  exists partly so tests can drive the mutation path without
  synthesizing events. Adding such helpers is OK if the alternative
  is event-synthesis brittleness.

### 10.3 What to test

For every new feature:

- **Domain logic** (data structures, parsers, serializers) → unit tests.
- **App behaviour** (widget state machines, mutation hooks,
  render dispatch) → integration tests with a mock igraph.
- **Visual changes** → manual GUI test gate; the user runs and confirms.
- **Public APIs** → at least one round-trip test (set → get returns
  what was set; serialize → parse round-trips).

---

## 11. Concurrency (forward-looking)

The codebase is **single-threaded today**. These principles apply
**when** multi-threading is introduced — most likely for the large-scale
circuit-simulation use case (Step 3+).

### 11.1 If multi-threading is enabled, consider thread safety

- Any struct touched by more than one thread needs an explicit
  synchronization strategy (mutex / atomic / message-passing / lock-free
  ring buffer). The choice and rationale go in the struct's header
  comment.
- Single-writer / multi-reader is often enough. Don't reach for full
  locking when only one thread mutates and others observe a snapshot.
- The widget framework + event loop should stay on the main thread.
  Domain-side computations may run on worker threads with results
  marshalled back via a thread-safe queue.

### 11.2 If processes / threads share data, consider access conflicts

- Memory shared across processes (shared-memory file mappings) needs
  a process-safe sync primitive (named mutex / spinlock in shared mem).
- Pipes / sockets / file-based message queues are usually simpler than
  raw shared memory for the volume of data DCS deals with.
- Document ownership: which process / thread writes which fields, who
  reads. Make it explicit at the struct definition.

### 11.3 Multi-core via multi-process

For large-scale circuit simulation (millions of gates, long step counts):
**multi-process parallelism is the planned vehicle for using multi-core
CPUs**. Sketch:

- Split the circuit graph into N independent partitions; assign each to
  a worker process.
- Workers communicate via a small message-passing layer (named pipes
  on Windows, Unix-domain sockets on Linux — both wrappable behind a
  future `imessage_t` interface).
- Coordinator process owns the GUI / file I/O; workers are headless
  domain-only.
- Result aggregation: workers write step-N partition outputs into a
  shared waveform buffer (one slot per worker, no contention) or stream
  them back to the coordinator.

This is **not built yet**. Mention it in code only when the choice
matters now (e.g., avoid baking in hidden-global state in `domain/`
that would block partitioning later).

### 11.4 Concrete defensive choices today

- `domain/` types take no globals — circuit, simulation, waveform all
  carry their own state via a `self` pointer. Already multi-process-
  friendly.
- `simulation_run` is a pure transform: input `circuit_t` + stimulus
  function → output `waveform_t`. Easy to parallelise per-partition
  when the time comes.
- Static / global mutable state in framework or app is **prohibited**
  (except trivial caches like raylib's internal state behind `igraph`,
  which the impl manages).

---

## 12. Conventions for working on this code

### 12.1 Commit discipline

- **One phase / one feature = one commit.** Don't bundle.
- Commit message: subject + body explaining **why**, not just **what**.
  Lead with the user-visible effect.
- Include `Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>`.
- Never amend without explicit user request. Use a new commit.
- Use `git mv` for renames (preserves history).
- Skip `package.json` / `package-lock.json` auto-bumps in commits.

### 12.2 Don't push without permission

Default is **commit, don't push**. The user explicitly says "push"
when ready. The accumulated unpushed commits form a reviewable batch.

### 12.3 Frozen baselines

- `prototype_version/` is the Step-1 baseline at commit `4a24207`.
  **Never modify.** It's there as the buildable reference.
- The `# @layout` / `# @wires` / `# @display_mode` / `# @display_name`
  / `# @pin_style` annotation grammars are now part of the file
  format contract. Don't redesign them; **extend** them.

### 12.4 Forward-compatible parsing

The `.dcs` file format is annotation-extensible. The parser:

- Ignores unknown `# @<name>` annotations silently.
- Ignores unknown style words, unknown pin references, malformed
  sub-lines within a known block.
- Accepts both UTF-8 (`→`) and ASCII (`->`) where both are common.

When adding a new annotation: pick a distinct keyword, parse defensively,
emit only when non-default. See `circuit_io.c`'s `parse_pin_style_line`
for the pattern.

### 12.5 Backward-compatible serialization

`circuit_io_serialize(c)` and `circuit_io_serialize_ex(c, NULL, NULL)`
must produce **byte-identical** output (verified by
`test_legacy_serialize_byte_identical`). Any new optional emission goes
through a `has_any_*` predicate that returns 0 when fields are at
defaults, so legacy round-trips stay clean.

### 12.6 Things to avoid

- Bare `struct` keyword in new code — use `class` / `interface`.
- `#include "raylib.h"` outside `graph_raylib.c`.
- `#include <windows.h>` outside `platform_windows.c`.
- Hungarian notation (`pBuf`, `iCount`, `m_field`).
- Globals — pass state via `self`. Period.
- Direct field casts to bypass encapsulation (`*(const T**)&w->field = x;`).
  Add a setter to the public API instead.
- Silent error swallow. If you must continue past an error, log it.
- New `#ifdef WIN32` / `#ifdef LINUX` branches in app or framework
  code. Push the platform-specific bit into `platform_*.c`.
- Re-deriving the wheel: use existing widgets / patterns / helpers.
  The framework intentionally has `frame`, `panel`, `button`, `label`,
  `menu`, `splitter`, `canvas_widget`, `divider_widget` — use them.

### 12.7 When in doubt

- Read the relevant section of `docs/step2-refactor-design.md` first.
- Then the corresponding section of `docs/step2-code-review.md` for
  the verdicts and known issues.
- Then the source. Implementation is the ground truth for what
  actually exists today.

---

## 13. Quick reference: shapes of common types

```c
/* Concrete class with vtable polymorphism. */
class tagt_widget {
    const widget_vt_t *vt;          /* MUST be first if used via base ptr */
    rect_t bounds;
    widget_t *parent;
    int visible;
    /* ... */
};

/* Interface — pure vtable + self ptr. */
interface tagt_igraph {
    void *self;
    void (*draw_rect)(void *self, rect_t r, uint32_t color);
    /* ... */
};

/* Single-inheritance via composition: base first. */
class tagt_button {
    widget_t base;                  /* base struct first */
    char label[64];
    void (*on_click)(void *user);   /* callback + user data */
    void *click_user;
};

/* Domain class — pure data + methods, no UI deps. */
class tagt_circuit {
    component_t **components;
    int component_count;
    /* ... */
};

/* Standard method shape: self is the first parameter. */
int  circuit_add_component (circuit_t *self, component_t *c,
                            const char *in1, const char *in2);
void circuit_destroy       (circuit_t *self);
```

Every new type should fit one of these moulds. If it doesn't, you're
probably about to violate something in this document.
