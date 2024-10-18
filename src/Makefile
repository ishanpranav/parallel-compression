# Makefile
# Copyright (c) 2024 Ishan Pranav
# Licensed under the MIT license.

CC=clang
CFLAGS=-pedantic -std=c99 -Wall -Wextra

all: nyuenc

nyuenc: main.c
	$(CC) $(CFLAGS) main.c -o nyuenc

clean:
	rm -f *.o nyush
