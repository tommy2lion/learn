# MinGW gcc under MSYS2 make can lose the TEMP variable; set it explicitly.
export TEMP := C:/Users/tommy/AppData/Local/Temp
export TMP  := C:/Users/tommy/AppData/Local/Temp

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99
RAYINC  = /c/msys64/mingw64/include
RAYLIB  = /c/msys64/mingw64/lib
RAYLDFL = -lraylib -lopengl32 -lgdi32 -lwinmm

all: magic_square_gui

magic_square_gui: magic_square_gui.c
	$(CC) $(CFLAGS) -I$(RAYINC) -o magic_square_gui.exe magic_square_gui.c \
	    -L$(RAYLIB) $(RAYLDFL)

clean:
	rm -f magic_square_gui.exe

.PHONY: all clean
