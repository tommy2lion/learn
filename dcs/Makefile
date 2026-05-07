CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
# MSYS2 workaround: export so gcc subprocess inherits them
export TEMP   = /tmp
export TMP    = /tmp
export TMPDIR = /tmp

# raylib (MSYS2 MinGW64) — only graph_raylib.c includes raylib.h
RAYLIB_INC  = /c/msys64/mingw64/include
RAYLIB_LIB  = /c/msys64/mingw64/lib
RAYLIB_LDFL = -L$(RAYLIB_LIB) -lraylib -lopengl32 -lgdi32 -lwinmm -lm

# ── Phase 2.1 sources ──────────────────────────────────────────────
FW_PLATFORM_SRC = src/framework/platform/platform_windows.c \
                  src/framework/platform/platform_linux.c
FW_PLATFORM_HDR = src/framework/platform/iplatform.h \
                  src/framework/core/oo.h

# ── Phase 2.2 sources ──────────────────────────────────────────────
FW_GRAPH_SRC = src/framework/graphics/graph_raylib.c
FW_GRAPH_HDR = src/framework/graphics/igraph.h \
               src/framework/core/color.h \
               src/framework/core/rect.h

# ── Phase 2.1: iplatform unit tests ────────────────────────────────
TEST_IPLATFORM_SRC = test/test_iplatform.c
TEST_IPLATFORM_EXE = test/test_iplatform.exe

$(TEST_IPLATFORM_EXE): $(FW_PLATFORM_SRC) $(FW_PLATFORM_HDR) $(TEST_IPLATFORM_SRC)
	$(CC) $(CFLAGS) $(FW_PLATFORM_SRC) $(TEST_IPLATFORM_SRC) -o $@ -lcomdlg32

test_iplatform: $(TEST_IPLATFORM_EXE)
	./$(TEST_IPLATFORM_EXE)

# ── Phase 2.2: igraph offline (vtable / color / layout) tests ──────
TEST_IGRAPH_SRC = test/test_igraph.c
TEST_IGRAPH_EXE = test/test_igraph.exe

$(TEST_IGRAPH_EXE): $(FW_GRAPH_SRC) $(FW_GRAPH_HDR) $(TEST_IGRAPH_SRC)
	$(CC) $(CFLAGS) -I $(RAYLIB_INC) $(FW_GRAPH_SRC) $(TEST_IGRAPH_SRC) -o $@ $(RAYLIB_LDFL)

test_igraph: $(TEST_IGRAPH_EXE)
	./$(TEST_IGRAPH_EXE)

# ── future phases (2.3 widgets, ...) hook in here ──────────────────

.PHONY: test test_iplatform test_igraph clean

test: test_iplatform test_igraph

clean:
	rm -f test/*.exe test/*.o
