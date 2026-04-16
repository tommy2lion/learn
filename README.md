# Calc 24

Four implementations of the classic "Make 24" card game, where you use any arithmetic operators to combine four numbers and reach 24.

## Implementations

| # | Location | Tech | Description |
|---|----------|------|-------------|
| 1 | `webpage/index.html` | HTML / CSS / JS | Browser game; no build step |
| 2 | `c/terminal/calc24.c` | C99, gcc | ASCII card art, stdin/stdout |
| 3 | `c/gui/calc24_gui.c` | C99, raylib | Monolithic graphical GUI |
| 4 | `c/gui/modular/` | C99, raylib | Refactored 8-module architecture |

## Quick Start

### Web
Open `webpage/index.html` in any browser.

### C builds (requires [MSYS2](https://www.msys2.org/) + raylib)

```bash
# Install raylib once
pacman -S mingw-w64-x86_64-raylib

# Build all targets from the c/ directory
cd c
make

# Run individually
./calc24.exe           # terminal
./calc24_gui.exe       # monolithic GUI
./calc24_modular.exe   # modular GUI
```

## Modular Architecture

The modular edition (`c/gui/modular/`) demonstrates clean separation of concerns:

```
solver / parser / game  ──  pure logic, no raylib, fully testable
card / ui               ──  rendering primitives
screen_game / screen_custom  ──  layout + event routing
main                    ──  coordinator only (~55 lines)
```

Design patterns used: Module, State Machine, Component, Strategy, Data-Driven.  
See `c/design/architecture.md` for the full design document.

## Rules

- Four cards are dealt (Ace = 1, J = 11, Q = 12, K = 13).
- Use +, −, ×, ÷ and parentheses to reach exactly **24**.
- Click a card to insert its value, then click an operator.
- Press **CHECK** to evaluate, **HINT** for a solution, **NEW** to skip, **CUSTOM** to set your own numbers.
