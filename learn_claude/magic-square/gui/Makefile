# MinGW gcc under MSYS2 make can lose the TEMP variable; set it explicitly.
export TEMP := C:/Users/tommy/AppData/Local/Temp
export TMP  := C:/Users/tommy/AppData/Local/Temp

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99
RAYINC  = /c/msys64/mingw64/include
RAYLIB  = /c/msys64/mingw64/lib
RAYLDFL = -lraylib -lopengl32 -lgdi32 -lwinmm

all: magic_square_gui cube_test

magic_square_gui: magic_square_gui.c cube.c cube.h
	$(CC) $(CFLAGS) -I$(RAYINC) -o magic_square_gui.exe magic_square_gui.c cube.c \
	    -L$(RAYLIB) $(RAYLDFL) -lm

# Cube data model + unit tests (no raylib needed).
cube_test: cube.c cube_test.c cube.h
	$(CC) $(CFLAGS) cube.c cube_test.c -o cube_test.exe

test: cube_test
	./cube_test.exe

clean:
	rm -f magic_square_gui.exe cube_test.exe cube_test *.o

.PHONY: all clean test
