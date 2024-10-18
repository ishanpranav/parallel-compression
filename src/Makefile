# Makefile
# Copyright (c) 2024 Ishan Pranav
# Licensed under the MIT license.

# References:
#  - https://www.man7.org/linux/man-pages/man3/getopt.3.html

# getopt in <main.c>: _POSIX_C_SOURCE >= 2

CC=clang
CFLAGS=-D_POSIX_C_SOURCE=2 -pedantic -std=c99 -Wall -Wextra

all: nyuenc

nyuenc: main.c euler.h mapped_file.h
	$(CC) $(CFLAGS) main.c -o nyuenc

clean:
	rm -f *.o nyush
