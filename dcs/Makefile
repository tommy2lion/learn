CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
# MSYS2 workaround: export so gcc subprocess inherits them
export TEMP   = /tmp
export TMP    = /tmp
export TMPDIR = /tmp

SIM_SRC     = src/sim/sim.c
SIM_INC     = src/sim
PARSER_SRC  = src/parser/parser.c
PARSER_INC  = src/parser
CLI_SRC     = cli/main.c
CLI_EXE     = dcs_cli.exe
GUI_SRC     = src/gui/main.c src/gui/canvas.c src/gui/editor.c src/gui/waveform.c
GUI_INC     = src/gui
GUI_EXE     = dcs_gui.exe

# raylib (MSYS2 MinGW64)
RAYLIB_INC  = /c/msys64/mingw64/include
RAYLIB_LIB  = /c/msys64/mingw64/lib
RAYLIB_LDFL = -L$(RAYLIB_LIB) -lraylib -lopengl32 -lgdi32 -lwinmm -lm

TEST_SIM_SRC    = test/test_sim.c
TEST_SIM_EXE    = test/test_sim.exe
TEST_PARSER_SRC = test/test_parser.c
TEST_PARSER_EXE = test/test_parser.exe
TEST_CLI_SH     = test/test_cli.sh

# ── Step 1.1: simulation engine tests ──────────────────────────────────────
$(TEST_SIM_EXE): $(SIM_SRC) src/sim/sim.h $(TEST_SIM_SRC)
	$(CC) $(CFLAGS) -I $(SIM_INC) $(SIM_SRC) $(TEST_SIM_SRC) -o $@

test_sim: $(TEST_SIM_EXE)
	./$(TEST_SIM_EXE)

# ── Step 1.2: parser and serializer tests ──────────────────────────────────
$(TEST_PARSER_EXE): $(SIM_SRC) $(PARSER_SRC) src/sim/sim.h src/parser/parser.h $(TEST_PARSER_SRC)
	$(CC) $(CFLAGS) -I $(SIM_INC) -I $(PARSER_INC) $(SIM_SRC) $(PARSER_SRC) $(TEST_PARSER_SRC) -o $@

test_parser: $(TEST_PARSER_EXE)
	./$(TEST_PARSER_EXE)

# ── Step 1.3: CLI executable + integration tests ──────────────────────────
$(CLI_EXE): $(SIM_SRC) $(PARSER_SRC) src/sim/sim.h src/parser/parser.h $(CLI_SRC)
	$(CC) $(CFLAGS) -I $(SIM_INC) -I $(PARSER_INC) $(SIM_SRC) $(PARSER_SRC) $(CLI_SRC) -o $@

cli: $(CLI_EXE)

test_cli: $(CLI_EXE) $(TEST_CLI_SH)
	sh $(TEST_CLI_SH)

# ── Step 1.4: GUI viewer (raylib) ──────────────────────────────────────────
$(GUI_EXE): $(SIM_SRC) $(PARSER_SRC) $(GUI_SRC) \
            src/sim/sim.h src/parser/parser.h \
            src/gui/canvas.h src/gui/editor.h src/gui/waveform.h
	$(CC) $(CFLAGS) -I $(SIM_INC) -I $(PARSER_INC) -I $(GUI_INC) -I $(RAYLIB_INC) \
		$(SIM_SRC) $(PARSER_SRC) $(GUI_SRC) -o $@ $(RAYLIB_LDFL)

gui: $(GUI_EXE)

# ── Step 1.5-6 — added later ───────────────────────────────────────────────

.PHONY: cli gui test test_sim test_parser test_cli clean

test: test_sim test_parser test_cli

clean:
	rm -f test/*.exe test/*.o $(CLI_EXE) $(GUI_EXE)
