# Makefile
# Copyright (c) 2024 Ishan Pranav
# Licensed under the MIT license.

# References:
#  - https://www.man7.org/linux/man-pages/man3/getopt.3.html

# getopt in <main.c>: _POSIX_C_SOURCE >= 2

CC=clang
CFLAGS=-D_POSIX_C_SOURCE=2 -lpthread -O3 -pedantic -std=c99 -Wall -Wextra

all: nyuenc

nyuenc: main.c encoder mapped_file_collection task_queue
	$(CC) $(CFLAGS) *.o main.c -o nyuenc

encoder: encoder.c encoder.h
	$(CC) $(CFLAGS) -c encoder.c

mapped_file_collection: mapped_file_collection.c mapped_file_collection.h \
	mapped_file.h
	$(CC) $(CFLAGS) -c mapped_file_collection.c

task_queue: task_queue.c task_queue.h
	$(CC) $(CFLAGS) -c task_queue.c
	
# thread_pool: thread_pool.c thread_pool.h
# 	$(CC) $(CFLAGS) -c thread_pool.c
	
clean:
	rm -f *.o nyuenc a.out
