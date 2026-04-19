# zeroF / test

A collection of small puzzle games implemented in multiple languages and environments.

## Projects

### Calc 24

Classic "Make 24" card game — combine four numbers with arithmetic operators to reach 24.

| # | Location | Tech | Notes |
|---|----------|------|-------|
| 1 | `webpage/index.html` | HTML / CSS / JS | Runs in any browser, no build step |
| 2 | `c/terminal/calc24.c` | C99, gcc | ASCII card art, stdin/stdout |
| 3 | `c/gui/calc24_gui.c` | C99, raylib | Monolithic graphical GUI |
| 4 | `c/gui/modular/` | C99, raylib | Refactored 8-module architecture |

**Rules:** Ace = 1, J = 11, Q = 12, K = 13. Use +, −, ×, ÷ and parentheses to reach exactly **24**.

### Magic Square

3×3 puzzle — place numbers 1–9 so every row, column, and diagonal sums to 15.

| # | Location | Tech |
|---|----------|------|
| 1 | `magic square/terminal/magic_square.c` | C99, gcc |

## Quick Start

### Web (Calc 24)
Open `webpage/index.html` in any browser.

### C builds (requires [MSYS2](https://www.msys2.org/) + raylib)

```bash
# Install raylib once
pacman -S mingw-w64-x86_64-raylib

# Build all Calc 24 targets
cd c
make

./calc24.exe           # terminal
./calc24_gui.exe       # monolithic GUI
./calc24_modular.exe   # modular GUI

# Build Magic Square
cd "magic square/terminal"
gcc magic_square.c -o magic_square
./magic_square
```

## Repository Layout

```
test/
├── webpage/                  # Web version of Calc 24
├── c/                        # C implementations of Calc 24
│   ├── Makefile
│   ├── terminal/             # stdin/stdout version
│   ├── gui/
│   │   ├── calc24_gui.c      # monolithic GUI
│   │   └── modular/          # 8-module refactor
│   └── design/               # architecture docs
└── magic square/
    └── terminal/             # CLI magic square game
```
