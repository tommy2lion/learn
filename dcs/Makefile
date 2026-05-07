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

# ── Phase 2.4 domain sources ───────────────────────────────────────
DOMAIN_SRC = src/domain/gate_and.c \
             src/domain/gate_or.c \
             src/domain/gate_not.c \
             src/domain/circuit.c \
             src/domain/circuit_io.c \
             src/domain/waveform.c \
             src/domain/simulation.c
DOMAIN_HDR = src/domain/component.h \
             src/domain/circuit.h \
             src/domain/circuit_io.h \
             src/domain/waveform.h \
             src/domain/simulation.h

# ── Phase 2.3 sources ──────────────────────────────────────────────
FW_WIDGET_SRC = src/framework/core/focus_manager.c \
                src/framework/core/quit_manager.c \
                src/framework/widgets/frame.c \
                src/framework/widgets/panel.c \
                src/framework/widgets/button.c \
                src/framework/widgets/label.c \
                src/framework/widgets/splitter.c \
                src/framework/widgets/canvas_widget.c \
                src/framework/widgets/menu.c
FW_WIDGET_HDR = src/framework/core/focus_manager.h \
                src/framework/core/quit_manager.h \
                src/framework/core/message.h \
                src/framework/widgets/widget.h \
                src/framework/widgets/frame.h \
                src/framework/widgets/panel.h \
                src/framework/widgets/button.h \
                src/framework/widgets/label.h \
                src/framework/widgets/splitter.h \
                src/framework/widgets/canvas_widget.h \
                src/framework/widgets/menu.h

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

# ── Phase 2.3 (part 1): widget framework ──────────────────────────
# Offline structural test for widget framework — does not open a window.
TEST_WIDGETS_SRC = test/test_widgets.c
TEST_WIDGETS_EXE = test/test_widgets.exe

$(TEST_WIDGETS_EXE): $(FW_WIDGET_SRC) $(FW_WIDGET_HDR) $(FW_GRAPH_HDR) $(TEST_WIDGETS_SRC)
	$(CC) $(CFLAGS) -I $(RAYLIB_INC) $(FW_WIDGET_SRC) $(TEST_WIDGETS_SRC) -o $@

test_widgets: $(TEST_WIDGETS_EXE)
	./$(TEST_WIDGETS_EXE)

# Windowed end-to-end demos. Run manually: `make demos` then launch.
DEMO1_SRC = demo/framework_demo.c
DEMO1_EXE = demo/framework_demo.exe
DEMO2_SRC = demo/widget_showcase.c
DEMO2_EXE = demo/widget_showcase.exe

$(DEMO1_EXE): $(FW_PLATFORM_SRC) $(FW_GRAPH_SRC) $(FW_WIDGET_SRC) $(DEMO1_SRC) \
              $(FW_PLATFORM_HDR) $(FW_GRAPH_HDR) $(FW_WIDGET_HDR)
	$(CC) $(CFLAGS) -I $(RAYLIB_INC) \
		$(FW_PLATFORM_SRC) $(FW_GRAPH_SRC) $(FW_WIDGET_SRC) $(DEMO1_SRC) \
		-o $@ -lcomdlg32 $(RAYLIB_LDFL)

$(DEMO2_EXE): $(FW_PLATFORM_SRC) $(FW_GRAPH_SRC) $(FW_WIDGET_SRC) $(DEMO2_SRC) \
              $(FW_PLATFORM_HDR) $(FW_GRAPH_HDR) $(FW_WIDGET_HDR)
	$(CC) $(CFLAGS) -I $(RAYLIB_INC) \
		$(FW_PLATFORM_SRC) $(FW_GRAPH_SRC) $(FW_WIDGET_SRC) $(DEMO2_SRC) \
		-o $@ -lcomdlg32 $(RAYLIB_LDFL)

demo:  $(DEMO1_EXE)
demos: $(DEMO1_EXE) $(DEMO2_EXE)

# ── Phase 2.4: domain layer + CLI + ported tests ───────────────────
TEST_CIRCUIT_SRC    = test/test_circuit.c
TEST_CIRCUIT_EXE    = test/test_circuit.exe
TEST_CIRCUIT_IO_SRC = test/test_circuit_io.c
TEST_CIRCUIT_IO_EXE = test/test_circuit_io.exe
TEST_CLI_SH         = test/test_cli.sh

$(TEST_CIRCUIT_EXE): $(DOMAIN_SRC) $(DOMAIN_HDR) $(TEST_CIRCUIT_SRC)
	$(CC) $(CFLAGS) $(DOMAIN_SRC) $(TEST_CIRCUIT_SRC) -o $@

test_circuit: $(TEST_CIRCUIT_EXE)
	./$(TEST_CIRCUIT_EXE)

$(TEST_CIRCUIT_IO_EXE): $(DOMAIN_SRC) $(DOMAIN_HDR) $(TEST_CIRCUIT_IO_SRC)
	$(CC) $(CFLAGS) $(DOMAIN_SRC) $(TEST_CIRCUIT_IO_SRC) -o $@

test_circuit_io: $(TEST_CIRCUIT_IO_EXE)
	./$(TEST_CIRCUIT_IO_EXE)

# new dcs_cli built on the new domain layer (still uses iplatform's read_file)
CLI_SRC  = cli/main.c
CLI_EXE  = dcs_cli.exe

$(CLI_EXE): $(FW_PLATFORM_SRC) $(FW_PLATFORM_HDR) $(DOMAIN_SRC) $(DOMAIN_HDR) $(CLI_SRC)
	$(CC) $(CFLAGS) $(FW_PLATFORM_SRC) $(DOMAIN_SRC) $(CLI_SRC) -o $@ -lcomdlg32

cli: $(CLI_EXE)

test_cli: $(CLI_EXE) $(TEST_CLI_SH)
	sh $(TEST_CLI_SH)

# ── future phases (2.5 app widgets, ...) ───────────────────────────

.PHONY: test test_iplatform test_igraph test_widgets test_circuit test_circuit_io test_cli cli demo demos clean

test: test_iplatform test_igraph test_widgets test_circuit test_circuit_io test_cli

clean:
	rm -f test/*.exe test/*.o demo/*.exe $(CLI_EXE)
