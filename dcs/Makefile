CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
# MSYS2 workaround: export so gcc subprocess inherits them
export TEMP   = /tmp
export TMP    = /tmp
export TMPDIR = /tmp

# ── Phase 2.1 sources ──────────────────────────────────────────────
FW_PLATFORM_SRC = src/framework/platform/platform_windows.c \
                  src/framework/platform/platform_linux.c
FW_PLATFORM_HDR = src/framework/platform/iplatform.h \
                  src/framework/core/oo.h

# ── Phase 2.1: iplatform unit tests ────────────────────────────────
TEST_IPLATFORM_SRC = test/test_iplatform.c
TEST_IPLATFORM_EXE = test/test_iplatform.exe

$(TEST_IPLATFORM_EXE): $(FW_PLATFORM_SRC) $(FW_PLATFORM_HDR) $(TEST_IPLATFORM_SRC)
	$(CC) $(CFLAGS) $(FW_PLATFORM_SRC) $(TEST_IPLATFORM_SRC) -o $@ -lcomdlg32

test_iplatform: $(TEST_IPLATFORM_EXE)
	./$(TEST_IPLATFORM_EXE)

# ── future phases (2.2 igraph, 2.3 widgets, ...) hook in here ──────

.PHONY: test test_iplatform clean

test: test_iplatform

clean:
	rm -f test/*.exe test/*.o
