CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99
RAYINC  = /c/msys64/mingw64/include
RAYLIB  = /c/msys64/mingw64/lib
RAYLDFL = -lraylib -lopengl32 -lgdi32 -lwinmm

all: calc24 calc24_gui

# ── Terminal version ─────────────────────────────────────────────────────────
calc24: calc24.c
	$(CC) $(CFLAGS) -o calc24.exe calc24.c -lm

# ── Monolithic GUI (raylib) ──────────────────────────────────────────────────
calc24_gui: calc24_gui.c
	$(CC) $(CFLAGS) -I$(RAYINC) -o calc24_gui.exe calc24_gui.c \
	    -L$(RAYLIB) $(RAYLDFL)

clean:
	rm -f calc24.exe calc24_gui.exe

.PHONY: all clean
