CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
# MSYS2 workaround: export so gcc subprocess inherits them
export TEMP   = /tmp
export TMP    = /tmp
export TMPDIR = /tmp

SIM_SRC  = src/sim/sim.c
SIM_INC  = src/sim

TEST_SIM_SRC = test/test_sim.c
TEST_SIM_EXE = test/test_sim.exe

# ── Step 1.1: simulation engine tests ──────────────────────────────────────
$(TEST_SIM_EXE): $(SIM_SRC) src/sim/sim.h $(TEST_SIM_SRC)
	$(CC) $(CFLAGS) -I $(SIM_INC) $(SIM_SRC) $(TEST_SIM_SRC) -o $@

test_sim: $(TEST_SIM_EXE)
	./$(TEST_SIM_EXE)

# ── Step 1.2 (parser) — added in next step ─────────────────────────────────
# ── Step 1.3 (CLI)    — added in next step ─────────────────────────────────
# ── Step 1.4-6 (GUI)  — added later        ─────────────────────────────────

.PHONY: test test_sim clean

test: test_sim

clean:
	rm -f test/*.exe test/*.o
