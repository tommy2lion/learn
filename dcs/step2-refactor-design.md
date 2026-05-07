# DCS Step 2 — Refactoring Design Proposal

This document is a **design-only** proposal. It does not change code yet. It responds to the 14 points in `step2-refactor-plan.md` by describing two layers — a reusable lightweight framework, and the DCS application built on top — together with a phased migration plan.

The Step-1 implementation (`src/sim`, `src/parser`, `src/gui/*`, `cli/main.c`) already established what DCS needs to do. Step 2 is a clean rewrite using the layered, object-oriented C style described below.

---

## 0 — Coding conventions

### 0.1 `class` and `interface` macros

Both expand to `struct`. The distinction is purely semantic:
- **`class`** — a concrete data type with state and methods that act on it.
- **`interface`** — a vtable struct (function pointers only, plus a `void *self`) that abstracts a contract.

```c
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
```

### 0.2 Type tags and typedefs

Every class has a tag prefixed with `tagt_` (kept exactly as in the user's example), plus a short alias used at call sites:

```c
class tagt_mem_manager {
    uint8_t buffer[SMALL_BUFFER_SIZE];
};
typedef class tagt_mem_manager mem_manager_t;

void mem_manager_init      (mem_manager_t *self);
void mem_manager_init_block(mem_manager_t *self, void *p, size_t n);
```

Globals use the bare lowercase name with no `tagt_`:

```c
static mem_manager_t mem_manager;
```

### 0.3 Naming rules

| Kind | Style | Example |
|---|---|---|
| Types (struct tag) | `tagt_<noun>` | `tagt_circuit` |
| Type aliases | `<noun>_t` | `circuit_t` |
| Functions | `<class>_<verb>[_<obj>]` | `circuit_add_component`, `frame_dispatch_event` |
| Locals & fields | lowercase `snake_case` | `int input_count;` |
| Macros / constants | `UPPER_SNAKE_CASE` | `EDITOR_PANEL_H_DEFAULT` |
| Files | `<class>.h` / `<class>.c`, one class per pair | `button.h`, `button.c` |

No Hungarian notation, ever (no `iCount`, `pszName`, `m_x`, etc.). The first parameter of any method is `self` of the class type.

### 0.4 Header hygiene

- Every public header has an include guard `DCS_<MODULE>_<FILE>_H`.
- A class's `.h` declares only the public API. Internal helpers are `static` in the `.c`.
- The `.h` includes only what its declarations need; impl details (e.g. raylib, windows.h) belong in `.c` files.
- Forward-declare across modules whenever the full type isn't required by the header.

---

## Part A — Framework Design

A small reusable widget framework, loosely modelled on MFC's "doc/view" idea but with no preconceptions about look-and-feel. It is independent of DCS and could host any other 2D application.

### A.1 Module layout

```
src/framework/
├── core/
│   ├── oo.h               class/interface macros
│   ├── color.h            uint32_t RGBA helpers
│   ├── rect.h             rect_t, point_t, vec2_t
│   ├── message.h          event_t enum + payload union
│   ├── object_factory.h   framework_factory_t (interface)
│   ├── focus_manager.h
│   ├── focus_manager.c
│   ├── quit_manager.h
│   └── quit_manager.c
├── platform/
│   ├── iplatform.h
│   ├── platform_windows.c
│   └── platform_linux.c   (stub for Phase 2.1; fleshed out in 2.7)
├── graphics/
│   ├── igraph.h
│   └── graph_raylib.c     (only TU that includes raylib.h)
└── widgets/
    ├── widget.h           interface tagt_widget
    ├── frame.h, frame.c
    ├── panel.h, panel.c           (vertical/horizontal stacker)
    ├── canvas_widget.h, canvas_widget.c   (pannable/zoomable area)
    ├── button.h, button.c
    ├── label.h, label.c
    ├── menu.h, menu.c
    ├── input_box.h, input_box.c
    └── splitter.h, splitter.c     (draggable divider)
```

The framework compiles as a static archive (`libdcs_framework.a`). Application code links against it but **never** includes `raylib.h` or `windows.h` directly.

### A.2 Platform interface — `iplatform`

Wraps everything OS-specific. Initial method set (kept small to start; grown as needed):

```c
/* framework/platform/iplatform.h */

interface tagt_iplatform {
    void *self;

    /* file dialogs */
    int   (*open_file)  (void *self, const char *title, char *out, int max);
    int   (*save_file)  (void *self, const char *title, char *out, int max);

    /* file I/O — synchronous, small-file friendly (whole-file in/out).
       Streaming/chunked APIs can be added later when waveform export
       or chipset libraries grow. Caller frees the read_file buffer. */
    char *(*read_file)  (void *self, const char *path, int *len_out);
    int   (*write_file) (void *self, const char *path, const char *buf, int len);

    /* time */
    uint64_t (*time_ms) (void *self);

    /* clipboard (optional, may be a no-op) */
    int   (*set_clipboard)(void *self, const char *text);
    int   (*get_clipboard)(void *self, char *out, int max);
};
typedef interface tagt_iplatform iplatform_t;
```

`platform_windows.c` provides the concrete vtable; lifts the Win32 `GetOpenFileNameA` / `GetSaveFileNameA` code from today's `dialog.c`, plus implements file I/O via stdio and time via `GetTickCount64`.

`platform_linux.c` (stub initially): file I/O via stdio works directly, time via `clock_gettime(CLOCK_MONOTONIC)`, dialogs return `0` until Phase 2.7 when GTK or `zenity` shells are added.

Notes on the eventual `zenity` impl:
- `zenity` is a GTK utility and isn't always installed (e.g., minimal headless distros). The impl probes for it at startup and degrades gracefully if missing.
- It's invoked synchronously via `popen`; that's fine for DCS's interactive open/save use case.
- **Fallback** when `zenity` is missing: `open_file` / `save_file` return `0`. The caller can then use the file path supplied on the command line (the way `./dcs_gui.exe foo.dcs` already works), or print an instructional message asking the user to specify a path.

The application acquires the platform once, at startup:

```c
iplatform_t *platform = platform_create();   /* selects impl at link time */
char path[512];
if (platform->open_file(platform->self, "Open .dcs", path, sizeof(path))) {
    int len;
    char *buf = platform->read_file(platform->self, path, &len);
    /* ... */
}
```

### A.3 Graphics interface — `igraph`

Wraps every drawing and input-querying primitive raylib offers. **`graph_raylib.c` is the only translation unit that includes `raylib.h`.** Future backends (Cairo, nanoVG, software) implement the same interface.

```c
/* framework/graphics/igraph.h */

typedef enum {
    CURSOR_DEFAULT,
    CURSOR_HAND,
    CURSOR_NS_RESIZE,
    CURSOR_EW_RESIZE,
    CURSOR_TEXT,
} cursor_kind_t;

interface tagt_igraph {
    void *self;

    /* lifecycle */
    int   (*init)         (void *self, int w, int h, const char *title);
    void  (*shutdown)     (void *self);
    int   (*should_close) (void *self);
    void  (*begin_frame)  (void *self);
    void  (*end_frame)    (void *self);
    void  (*screen_size)  (void *self, int *w, int *h);

    /* drawing primitives — color is uint32_t packed RGBA */
    void  (*draw_rect)         (void *self, rect_t r, uint32_t color);
    void  (*draw_rect_lines)   (void *self, rect_t r, float thick, uint32_t color);
    void  (*draw_line)         (void *self, vec2_t a, vec2_t b, float thick, uint32_t color);
    void  (*draw_circle)       (void *self, vec2_t c, float r, uint32_t color);
    void  (*draw_circle_lines) (void *self, vec2_t c, float r, float thick, uint32_t color);
    void  (*draw_text)         (void *self, const char *s, vec2_t pos, float size, uint32_t color);
    float (*measure_text)      (void *self, const char *s, float size);

    /* input querying */
    vec2_t (*mouse_position)   (void *self);
    vec2_t (*mouse_delta)      (void *self);
    int    (*mouse_down)       (void *self, int btn);
    int    (*mouse_pressed)    (void *self, int btn);
    int    (*mouse_released)   (void *self, int btn);
    float  (*mouse_wheel)      (void *self);
    int    (*key_down)         (void *self, int key);
    int    (*key_pressed)      (void *self, int key);

    /* 2D camera (pan/zoom) */
    void  (*push_camera2d) (void *self, vec2_t target, vec2_t offset, float zoom);
    void  (*pop_camera2d)  (void *self);
    vec2_t(*screen_to_world)(void *self, vec2_t screen);
    vec2_t(*world_to_screen)(void *self, vec2_t world);

    /* clipping */
    void  (*push_scissor) (void *self, rect_t r);
    void  (*pop_scissor)  (void *self);

    /* cursor */
    void  (*set_cursor)   (void *self, cursor_kind_t kind);
};
typedef interface tagt_igraph igraph_t;
```

The header `core/color.h` provides convenience constants:
```c
#define COLOR_BLACK    0x000000FFu
#define COLOR_WHITE    0xFFFFFFFFu
#define COLOR_BLUE     0x4080E0FFu
/* ... */
#define COLOR_RGBA(r,g,b,a)  (((r)<<24) | ((g)<<16) | ((b)<<8) | (a))
```

### A.4 Widget framework

#### Widget interface

```c
/* framework/widgets/widget.h */

typedef struct tagt_widget widget_t;

interface tagt_widget_vtable {
    void (*draw)         (widget_t *self, igraph_t *g);
    int  (*handle_event) (widget_t *self, const event_t *ev);  /* 1=consumed */
    void (*on_focus)     (widget_t *self);
    void (*on_blur)      (widget_t *self);
    void (*destroy)      (widget_t *self);
};

class tagt_widget {
    const interface tagt_widget_vtable *vt;
    rect_t   bounds;
    widget_t *parent;
    int      visible;
    int      dirty;        /* reserved for Phase 2.7 partial refresh — UNUSED before then */
    void    *user;
};
```

Concrete widgets place their state struct after the embedded `widget_t` (single-inheritance via composition):

```c
/* widgets/button.c */
class tagt_button {
    widget_t base;             /* must be first */
    char     label[64];
    void   (*on_click)(void *user);
    void    *click_user;
};
```

#### Frame

```c
/* widgets/frame.h */
class tagt_frame {
    igraph_t        *graph;
    iplatform_t     *platform;
    widget_t        *root;            /* usually a panel */
    focus_manager_t *focus;
    quit_manager_t  *quit;
    int              should_quit;
};
typedef class tagt_frame frame_t;

void frame_init   (frame_t *self, igraph_t *g, iplatform_t *p, widget_t *root);
void frame_run    (frame_t *self);    /* blocking main loop */
void frame_shutdown(frame_t *self);
```

`frame_run`'s body each iteration:
```
1. graph->begin_frame(...)
2. poll input → build event list → quit_manager intercepts → focus/hit-test dispatch
3. recursive draw of widget tree
4. graph->end_frame(...)
```

#### Container widgets

- **`panel_t`** — vertical or horizontal stacker, with optional gaps and per-child explicit positions. No flex; just predictable absolute layout.
- **`splitter_t`** — two children with a draggable divider between them; replaces today's panel-resize logic.
- **`canvas_widget_t`** — base for any 2D pannable/zoomable area. Manages its own camera (target/offset/zoom). Subclasses override a `draw_world(self, igraph)` method called inside the camera transform.

#### Leaf widgets

- **`button_t`** — text label, hover & active states, `on_click` callback.
- **`label_t`** — static text.
- **`menu_t`** — dropdown menu with items, shortcut hint text, `on_select(idx)`.
- **`input_box_t`** — single-line text input.

#### Partial refresh — designed for, deferred

Each widget has a `dirty` flag; `frame_t` collects dirty rects and could redraw only those regions in a future phase. For the initial rewrite, every frame redraws the whole window — simple and good enough at 60 Hz. **Until Phase 2.7 actually wires up dirty-rect collection and a backbuffer, the `dirty` field is unused — widgets must not rely on it.** The field is declared up-front only so adding partial refresh later doesn't require touching every widget definition.

### A.5 Focus & input dispatch

#### Event types

```c
/* framework/core/message.h */
typedef enum {
    EV_MOUSE_MOVE,                        /* fired every frame the cursor moves        */
    EV_MOUSE_DOWN,                        /* held: fired every frame a button is held  */
    EV_MOUSE_UP,                          /* held: fired every frame a button is up    */
    EV_MOUSE_PRESS,                       /* edge: fired once on up→down transition    */
    EV_MOUSE_WHEEL,                       /* fired when the wheel scrolls              */
    EV_KEY_DOWN,                          /* held: fired every frame a key is held     */
    EV_KEY_UP,                            /* held: fired every frame a key is up (rare)*/
    EV_KEY_PRESS,                         /* edge: fired once on up→down transition    */
    EV_RESIZE,
    EV_FOCUS_IN,   EV_FOCUS_OUT,
} event_kind_t;

class tagt_event {
    event_kind_t kind;
    union {
        struct { vec2_t pos; int btn; int mods; }   mouse;
        struct { float dy; }                        wheel;
        struct { int key; int mods; }               key;
        struct { int w; int h; }                    resize;
    };
};
typedef class tagt_event event_t;
```

#### Routing

- `quit_manager_t` runs first. If it consumes the event (e.g., ESC, window-close request), dispatch stops.
- Mouse events go to the deepest widget under the cursor (hit-test descent).
- Keyboard events go to the focused widget.
- A click on a widget transfers focus to it (unless the widget refuses focus).
- **Edge events (`*_PRESS`) are derived inside `frame_dispatch`** by comparing this frame's `igraph` input snapshot to the previous frame's. The igraph backend itself only reports per-frame "is this key/button currently down" state; the framework layer turns that into edge-triggered events so widgets get clean "fired exactly once" semantics.

#### Managers

```c
class tagt_focus_manager {
    widget_t *current;
    /* Tab cycles focus order across registered widgets. */
};

class tagt_quit_manager {
    int  (*on_attempt_quit)(void *user);   /* return 1 to allow */
    void *user;
};
```

`quit_manager` is what handles ESC, the `[X]` button, etc. The application registers callbacks for "are unsaved changes safe to discard?" as it grows.

### A.6 Object factory

```c
/* framework/core/object_factory.h */

interface tagt_framework_factory {
    void *self;
    iplatform_t *platform;
    igraph_t    *graph;

    button_t        *(*create_button)        (void *self, const char *label);
    label_t         *(*create_label)         (void *self, const char *text);
    panel_t         *(*create_panel)         (void *self);
    menu_t          *(*create_menu)          (void *self);
    input_box_t     *(*create_input_box)     (void *self);
    canvas_widget_t *(*create_canvas_widget) (void *self);
    splitter_t      *(*create_splitter)      (void *self, int horizontal);
    frame_t         *(*create_frame)         (void *self, widget_t *root);

    void (*destroy_widget)(void *self, widget_t *w);
};
typedef interface tagt_framework_factory framework_factory_t;
```

Why factories?
- Centralized allocation strategy (could swap to a pool allocator).
- Tests can supply mock factories.
- Decouples consumers from concrete struct layouts (some widgets may have many fields the caller shouldn't see).

#### Lifecycle and ownership

The framework owns the widget tree. Specifically:

- The **factory** allocates a widget. While it is "orphaned" (created but not yet attached to a parent), the caller is responsible for it and must use the factory's `destroy_widget` to release it.
- Once a widget is **attached to a parent** (e.g., added to a panel, or set as the frame's root), the parent owns its lifetime. Calling `destroy_widget` directly on an attached widget is incorrect.
- `dcs_app_t` and other application objects hold **weak references** to widgets they need to talk to (e.g., `circuit_canvas`, `input_panel`). They never `destroy_widget` those references — the frame does it.
- **Destruction is recursive** from the top: `frame_shutdown` walks the widget tree post-order, calling each widget's `destroy` vtable method, which in turn frees the widget's own resources before the framework frees the widget struct itself.

Mnemonic: *"Factory births orphans; parents adopt them; frame buries them."*

### A.7 Build / link order

```
[domain/*]            ← pure logic, no deps
[framework/core,
 framework/platform/iplatform,
 framework/graphics/igraph]
[framework/widgets]   ← depends on core + interfaces
[framework/platform/platform_windows]   ← only on Windows builds
[framework/graphics/graph_raylib]       ← only when raylib backend chosen
[app/*]               ← depends on framework + domain
[gui/main.c]          ← wires everything via factories
```

The framework's public headers form a tree that doesn't transitively include raylib or windows.h. The Windows/raylib impls are translation units linked in only by the application binary; replacing the graphics or platform backend recompiles those impls but no other code.

---

## Part B — Application Design (DCS)

### B.1 Domain layer (no UI dependencies)

```
src/domain/
├── component.h        interface tagt_component
├── component.c
├── gate_and.c, gate_or.c, gate_not.c   primitive components
├── chipset.h, chipset.c                composite (deferred to its phase)
├── pin.h
├── wire.h
├── circuit.h, circuit.c
├── simulation.h, simulation.c
└── circuit_io.h, circuit_io.c          .dcs read/write (extended format)
```

#### `component_t` — interface

```c
/* domain/component.h */
typedef enum { COMP_AND, COMP_OR, COMP_NOT, COMP_CHIPSET } component_kind_t;

interface tagt_component_vtable {
    component_kind_t (*kind)      (void *self);
    int              (*pin_count_in) (void *self);
    int              (*pin_count_out)(void *self);
    void             (*evaluate)  (void *self, const signal_t *in, signal_t *out);
    void             (*destroy)   (void *self);
};

class tagt_component {
    const interface tagt_component_vtable *vt;
    char    name[SIM_NAME_LEN];
    vec2_t  position;       /* persisted layout (pixel coords) */
};
```

Every primitive (`gate_and_t`, `gate_or_t`, `gate_not_t`) embeds `component_t` first and supplies a vtable. `chipset_t` (Phase 2.6+) does the same but holds an internal `circuit_t`.

#### `circuit_t`

```c
class tagt_circuit {
    component_t **components;   /* owned; polymorphic */
    int           component_count;
    wire_t       *wires;
    int           wire_count;
    pin_ref_t    *external_inputs, *external_outputs;
    int           input_count, output_count;
};
```

Holds the structural truth + every component's `position` (pixel layout). Inputs and outputs are external pin references (not separate components).

#### `waveform_t`

Multi-channel time-series data: each track holds N step values for one named signal (an external input or output of the circuit). Used as `simulation_t`'s output and as the `timing_canvas_widget_t`'s input.

```c
class tagt_waveform_track {
    char      name[SIM_NAME_LEN];
    signal_t *values;          /* length = step_count, owned */
    int       step_count;
};
typedef class tagt_waveform_track waveform_track_t;

class tagt_waveform {
    waveform_track_t *tracks;  /* one per recorded signal — inputs first, then outputs */
    int               track_count;
    int               step_count;
};
typedef class tagt_waveform waveform_t;
```

#### `simulation_t`

Replaces today's `circuit_run`. Drives a `circuit_t` for N steps, given an input-stimulus function (static toggles or sweep), and records traces.

```c
class tagt_simulation {
    circuit_t     *circuit;
    waveform_t     waves;     /* recorded inputs & outputs over N steps */
};

void simulation_run  (simulation_t *self, const stimulus_t *stim, int steps);
```

#### `circuit_io`

`circuit_io_load(path) → circuit_t *` and `circuit_io_save(circuit, path)`. Extended `.dcs` format described in B.4. Replaces `parser.c`.

### B.2 Application widgets

```
src/app/
├── circuit_canvas_widget.h, .c   subclass of canvas_widget_t
├── timing_canvas_widget.h, .c    subclass of canvas_widget_t
├── side_toolbar.h, .c            panel of gate-palette buttons
├── file_menu.h, .c               File ▾ dropdown
├── input_panel.h, .c             toggles + Steps + RUN + SWEEP
├── editor_state.h, .c            shared interaction state object
├── modes/                        State pattern for editor modes
│   ├── mode_idle.c
│   ├── mode_placing.c
│   ├── mode_wiring.c
│   ├── mode_dragging.c
│   ├── mode_marquee.c
│   └── mode_resizing.c
├── dcs_app.h, .c                 top-level app object
└── app_factory.h, .c
```

`circuit_canvas_widget` and `timing_canvas_widget` extend the framework's `canvas_widget_t`, overriding `draw_world` to paint their respective contents. They consume domain types directly (`circuit_t`, `waveform_t`).

Today's `editor.c` god-object is split:
- Visual layout → distinct widgets.
- Editor mode logic → State pattern (each mode is a small class implementing `enter / handle_event / exit`).
- Selection, hover, drag bookkeeping → `editor_state_t` (a plain data object passed to mode classes).
- File commands → `file_menu_t` calling `dcs_app_t` methods.

`editor_state_t` is the **context** that mode classes read and write. It carries the references and the transient interaction data that all modes share:

```c
class tagt_editor_state {
    /* Stable refs (set once at startup) */
    circuit_canvas_widget_t *canvas;     /* mouse-pos-in-world via canvas->camera */
    circuit_t               *circuit;    /* the circuit being edited              */

    /* Transient interaction data (refreshed each frame) */
    vec2_t  mouse_world;                 /* cached for the active mode             */
    int     hover_node;                  /* -1 if none                             */
    int     hover_wire;                  /* -1 if none                             */
    int     wire_src_node;               /* MODE_WIRING                            */
    int     drag_node;                   /* MODE_DRAGGING                          */
    vec2_t  drag_offset;                 /* MODE_DRAGGING                          */
    vec2_t  marquee_start;               /* MODE_MARQUEE                           */
};
typedef class tagt_editor_state editor_state_t;
```

Each mode receives `editor_state_t *` in its `enter / handle_event / exit` calls. **A mode never reaches into the widget tree directly** — it only manipulates `editor_state` and (through it) the `circuit_t`. This keeps modes testable in isolation: a unit test can construct an `editor_state_t` with a stub canvas + a real `circuit_t`, drive the mode's `handle_event` with synthetic events, and assert on the resulting circuit/state — no GUI required.

### B.3 App object & factory

```c
class tagt_dcs_app {
    framework_factory_t *fw_factory;
    iplatform_t         *platform;
    igraph_t            *graph;

    frame_t              frame;
    circuit_t           *circuit;          /* current */
    simulation_t        *sim;

    /* widget refs */
    circuit_canvas_widget_t *circuit_canvas;
    timing_canvas_widget_t  *timing_canvas;
    side_toolbar_t          *toolbar;
    file_menu_t             *file_menu;
    input_panel_t           *input_panel;
    splitter_t              *vertical_split;

    char                 file_path[256];
    int                  path_is_explicit;
};

void dcs_app_new      (dcs_app_t *self);
void dcs_app_open     (dcs_app_t *self);   /* triggers iplatform open dialog */
int  dcs_app_save     (dcs_app_t *self);
int  dcs_app_save_as  (dcs_app_t *self);
void dcs_app_run      (dcs_app_t *self);
void dcs_app_sweep    (dcs_app_t *self);
```

`app_factory_t` constructs all of the above and wires them together at startup.

### B.4 Saving circuit state with layout

The `.dcs` format stays backward-compatible. Layout is appended as an annotation block:

```
inputs: a, b
outputs: sum, carry

carry   = and(a, b)
a_or_b  = or(a, b)
n_carry = not(carry)
sum     = and(a_or_b, n_carry)

# @layout
# @  carry          = 280, 320
# @  a_or_b         = 280, 440
# @  n_carry        = 460, 320
# @  sum            = 640, 380
# @  __input:a      = 100, 320
# @  __input:b      = 100, 440
# @  __output:sum   = 820, 320
# @  __output:carry = 820, 440
```

Rules:
- The marker `# @layout` opens the block; subsequent lines starting with `# @  ` carry `<name> = <x>, <y>` entries.
- External I/O entries are prefixed `__input:` / `__output:` to disambiguate from gate names.
- Old `.dcs` files (no layout block) load fine; their components get auto-placed by the existing topological layout (preserving today's behavior).
- The serializer always writes the block when positions are known.
- Unknown `# @<word>` annotations are ignored (forward-compatible).

### B.5 Extensibility for chipsets

Per requirement #10, the type system anticipates composite components even though their parser/serializer support is deferred:

- `chipset_t` extends `component_t` and contains an internal `circuit_t *body`.
- A chipset's `evaluate` walks the body each simulation step.
- File format: a chipset definition is a separate `.dcs` file. A circuit instantiates a chipset by referencing its name (e.g., `adder1 = full_adder(a, b, cin)`), looked up in a registry at load time.
- Registry: `component_registry_t` maps names → constructor functions, populated by both built-in primitives and user-defined chipsets discovered on a search path.

---

## C — Phased migration

The rewrite is large. Each phase below is a complete commit set that builds and passes its own tests before the next phase starts.

| Phase | Scope | Deliverables / verification |
|---|---|---|
| **2.0** | Snapshot the prototype | Create `dcs/prototype_version/` and move the entire current Step-1 implementation into it: `src/`, `cli/`, `test/`, `circuits/`, `Makefile`, `architecture.md`, `step1-design.md`, plus the `.gitignore` adjustments to keep build outputs out of the snapshot. Verify it builds standalone with `make` from inside `prototype_version/` and all 98 tests still pass. Commit as a clean "before-refactor" baseline. |
| **2.1** | Conventions + `iplatform` | `oo.h`, `iplatform.h`, `platform_windows.c` (lifts current `dialog.c` + adds `read_file`/`write_file`/`time_ms`); GUI still uses today's editor but file I/O routes through `iplatform`. Existing 98 tests pass. |
| **2.2** | `igraph` + `graph_raylib` | Define interface, implement against raylib. Migrate today's `canvas.c`/`waveform.c` drawing calls behind the interface. No widget framework yet — intermediate step. Tests still pass. |
| **2.3** | Widget framework | `frame`, `widget`, `panel`, `splitter`, `canvas_widget`, `button`, `label`, `menu`, `input_box`. Build a one-button demo `framework_demo.exe` to validate. |
| **2.4** | Domain rewrite | New `component_t` hierarchy, `circuit_t`, `simulation_t`, `circuit_io`. Migrate the 33 sim + 54 parser unit tests onto the new types (CLI also moves; the 11 CLI integration tests pass unchanged externally). |
| **2.5** | App widgets | `circuit_canvas`, `timing_canvas`, `side_toolbar`, `file_menu`, `input_panel`, mode state classes. `dcs_app` wires them via `app_factory`. Old `src/gui/*` deleted. |
| **2.6** | Layout block in `.dcs` | Round-trip tests for the new format; load existing fixtures (no layout) → auto-layout → save (with layout) → re-load → identical state. |
| **2.7** | Polish | Partial refresh (dirty rects + backbuffer, only if profiling shows it matters), focus indicators, Linux platform impl (POSIX I/O + zenity dialogs), undo/redo via Command pattern. |

**Why Phase 2.0:** The Step-1 implementation is preserved in full as `prototype_version/`. This gives an unambiguous reference to compare behavior against during migration, lets us run regression tests via the original binaries if the new ones diverge, and means Phase 2.1 starts from a clean empty `dcs/` (besides the snapshot) without needing a complex per-file move-versus-rewrite policy.

**On test continuity:** maintaining the 98-green-test bar across phases means some phases must keep the old code in place behind a thin **adapter** while the new code is built up. For example, Phase 2.4 (domain rewrite) may temporarily expose the old `circuit_run` semantics through a shim that internally calls the new `simulation_run`, so existing CLI integration tests pass unchanged while the test suite is being ported. Adapters are explicitly removed in the same commit that migrates their last test consumer, so they never linger.

After Phase 2.7, the layered structure is complete:

```
[gui/main.c] → [app] → [framework + domain]
                          │
                  [iplatform] [igraph]
                  (windows)   (raylib)
```

Swapping platforms or graphics backends touches only the corresponding impl file.

---

## D — Design patterns used

| Pattern | Where | Why |
|---|---|---|
| **Strategy** | `iplatform`, `igraph`, `component` | Concrete impls swap without touching consumers. |
| **Composite** | Widget tree (`frame` → panels → leaves) | Uniform `draw` / `handle_event` traversal. |
| **Factory** | `framework_factory`, `app_factory` | Centralized construction, mockable for tests. |
| **State** | Editor modes (idle / placing / wiring / dragging / marquee / resizing) | Replaces today's giant switch with small classes; transitions explicit. |
| **Observer** *(optional)* | `circuit_t` change notifications → canvas redraw | Useful once partial refresh is wired. |
| **Command** *(optional, deferred)* | Undo/redo of edits | Logged operations enable history, scripting, automated tests. |

The overarching rule: **depend on interfaces, not concrete classes.** App code never names `platform_windows_t` or `graph_raylib_t` — only `iplatform_t *` and `igraph_t *`.

---

## E — Open design questions (refine during review)

These are intentionally left open. Decisions can be made as the doc is reviewed; defaults below will be used unless changed.

1. **Linux platform impl in Phase 2.7:** real GTK/zenity dialogs **or** stub-only. *Default proposed: zenity (a single shell-out, no extra link deps; falls back gracefully when zenity is missing — see A.2).*
2. **Partial refresh strategy:** dirty rects with a backbuffer texture **or** stay full-frame and rely on 60 Hz being cheap. *Default proposed: full-frame; reconsider only if profiling shows a problem.*
3. **Color palette:** hard-coded constants in `core/color.h` **or** themable via a `theme_t`. *Default proposed: hard-coded for now; trivially refactorable later.*
4. **Test strategy during migration:** keep current 98 tests as-is and port piecemeal **or** write a parallel test suite for the new domain types up front. *Default proposed: piecemeal — each phase migrates its tests as part of its commit set; total count of green tests should never drop below today's 98.*

> **Settled during review:** the `tagt_` struct-tag convention with `<noun>_t` aliases at call sites was reviewed and accepted as-is.

Once these are settled, Phase 2.0 can begin.
