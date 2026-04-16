CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99
RAYINC  = /c/msys64/mingw64/include
RAYLIB  = /c/msys64/mingw64/lib
RAYLDFL = -lraylib -lopengl32 -lgdi32 -lwinmm

# Modular edition sources
MOD_SRCS = src/main.c      \
           src/solver.c    \
           src/parser.c    \
           src/card.c      \
           src/ui.c        \
           src/game.c      \
           src/screen_game.c   \
           src/screen_custom.c

all: calc24 calc24_gui calc24_modular

# ── Terminal version (original) ──────────────────────────────────────────────
calc24: calc24.c
	$(CC) $(CFLAGS) -o calc24.exe calc24.c -lm

# ── Monolithic GUI (original, kept for reference) ────────────────────────────
calc24_gui: calc24_gui.c
	$(CC) $(CFLAGS) -I$(RAYINC) -o calc24_gui.exe calc24_gui.c \
	    -L$(RAYLIB) $(RAYLDFL)

# ── Modular GUI ──────────────────────────────────────────────────────────────
calc24_modular: $(MOD_SRCS)
	$(CC) $(CFLAGS) -I$(RAYINC) -Isrc -o calc24_modular.exe \
	    $(MOD_SRCS) -lm -L$(RAYLIB) $(RAYLDFL)

clean:
	rm -f calc24.exe calc24_gui.exe calc24_modular.exe

.PHONY: all clean
