// main.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://man7.org/linux/man-pages/man2/close.2.html
//  - https://www.man7.org/linux/man-pages/man3/fstat.3p.html
//  - https://www.man7.org/linux/man-pages/man3/getopt.3.html
//  - https://www.man7.org/linux/man-pages/man2/mmap.2.html
//  - https://www.man7.org/linux/man-pages/man2/open.2.html
//  - https://www.man7.org/linux/man-pages/man3/perror.3.html
//  - https://www.man7.org/linux/man-pages/man3/sprintf.3p.html
//  - https://www.man7.org/linux/man-pages/man3/stat.3type.html

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "euler.h"
#include "mapped_file.h"

static void main_print_usage(String args[])
{
    printf("Usage: %s [OPTION]... FILE...\n", args[0]);
}

static void main_print_file_error(
    String application,
    size_t applicationLength,
    String arg)
{
    String errorMessage = malloc(applicationLength + strlen(arg) + 3);

    if (!errorMessage)
    {
        perror(application);

        return;
    }

    sprintf(errorMessage, "%s: %s", application, arg);
    perror(errorMessage);
}

static void main_encode(MappedFile file)
{
    printf("%s\n", file->buffer);
}

int main(int count, String args[])
{
    int option;

    while ((option = getopt(count, args, "h")) != -1)
    {
        switch (option)
        {
        case 'h':
            main_print_usage(args);

            return EXIT_SUCCESS;

        default: return EXIT_FAILURE;
        }
    }

    if (optind >= count)
    {
        main_print_usage(args);

        return EXIT_FAILURE;
    }

    MappedFile mappedFiles = malloc((count - optind) * sizeof * mappedFiles);

    if (!mappedFiles)
    {
        perror(args[0]);

        return EXIT_FAILURE;
    }

    size_t applicationLength = strlen(args[0]);

    for (int i = optind; i < count; i++)
    {
        int descriptor = open(args[i], O_RDONLY);

        if (descriptor == -1)
        {
            main_print_file_error(args[0], applicationLength, args[i]);

            return EXIT_FAILURE;
        }

        struct stat status;

        if (fstat(descriptor, &status) == -1)
        {
            main_print_file_error(args[0], applicationLength, args[i]);

            return EXIT_FAILURE;
        }

        unsigned char* buffer = mmap(
            NULL,
            status.st_size,
            PROT_READ,
            MAP_PRIVATE,
            descriptor,
            0);

        if (buffer == MAP_FAILED)
        {
            main_print_file_error(args[0], applicationLength, args[i]);

            return EXIT_FAILURE;
        }

        mappedFiles[i - optind].size = status.st_size;
        mappedFiles[i - optind].buffer = buffer;

        if (close(descriptor) == -1)
        {
            main_print_file_error(args[0], applicationLength, args[i]);

            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < count - optind; i++)
    {
        printf("[%d]:\n", i);
        main_encode(mappedFiles + i);
    }

    return EXIT_SUCCESS;
}
