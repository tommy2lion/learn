SHELL   = /bin/bash
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99
RAYINC  = /c/msys64/mingw64/include
RAYLIB  = /c/msys64/mingw64/lib
RAYLDFL = -lraylib -lopengl32 -lgdi32 -lwinmm

# TMPDIR fix needed on some Windows/MSYS2 setups
export TMPDIR := /tmp

all: sudoku

sudoku: sudoku.c
	TMPDIR=/tmp TEMP=/tmp TMP=/tmp \
	$(CC) $(CFLAGS) -I$(RAYINC) -o sudoku.exe sudoku.c \
	    -L$(RAYLIB) $(RAYLDFL)

clean:
	rm -f sudoku.exe

.PHONY: all clean
