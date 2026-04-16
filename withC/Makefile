CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99

all: calc24

calc24: calc24.c
	$(CC) $(CFLAGS) -o calc24.exe calc24.c -lm

clean:
	rm -f calc24.exe

.PHONY: all clean
