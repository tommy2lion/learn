CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99
RAYINC  = /c/msys64/mingw64/include
RAYLIB  = /c/msys64/mingw64/lib
RAYLDFL = -lraylib -lopengl32 -lgdi32 -lwinmm

# Modular edition sources
MOD_DIR  = modular
MOD_SRCS = $(MOD_DIR)/main.c         \
           $(MOD_DIR)/solver.c        \
           $(MOD_DIR)/parser.c        \
           $(MOD_DIR)/card.c          \
           $(MOD_DIR)/ui.c            \
           $(MOD_DIR)/game.c          \
           $(MOD_DIR)/screen_game.c   \
           $(MOD_DIR)/screen_custom.c

all: calc24 calc24_gui calc24_modular

# ── Terminal version ─────────────────────────────────────────────────────────
calc24: terminal/calc24.c
	$(CC) $(CFLAGS) -o calc24.exe terminal/calc24.c -lm

# ── Monolithic GUI (raylib) ──────────────────────────────────────────────────
calc24_gui: gui/calc24_gui.c
	$(CC) $(CFLAGS) -I$(RAYINC) -o calc24_gui.exe gui/calc24_gui.c \
	    -L$(RAYLIB) $(RAYLDFL)

# ── Modular GUI (raylib) ─────────────────────────────────────────────────────
calc24_modular: $(MOD_SRCS)
	$(CC) $(CFLAGS) -I$(RAYINC) -I$(MOD_DIR) -o calc24_modular.exe \
	    $(MOD_SRCS) -lm -L$(RAYLIB) $(RAYLDFL)

clean:
	rm -f calc24.exe calc24_gui.exe calc24_modular.exe

.PHONY: all clean
