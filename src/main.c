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
#include "string_builder.h"

static void main_print_usage(String args[])
{
    printf("Usage: %s [OPTION]... FILE...\n", args[0]);
}

static void main_throw_file_error(StringBuilder application, String path)
{
    String errorMessage = malloc(application->length + strlen(path) + 3);

    if (!errorMessage)
    {
        perror(application->buffer);

        return;
    }

    sprintf(errorMessage, "%s: %s", application->buffer, path);
    perror(errorMessage);
    exit(EXIT_FAILURE);
}

static void main_encode(MappedFile file)
{
    printf("%s\n", file->buffer);
}

static void main_map_files(
    struct MappedFile results[],
    String paths[], 
    int count, 
    StringBuilder application)
{
    for (int i = 0; i < count; i++)
    {
        int descriptor = open(paths[i], O_RDONLY);

        if (descriptor == -1)
        {
            main_throw_file_error(application, paths[i]);
        }

        struct stat status;

        if (fstat(descriptor, &status) == -1)
        {
            main_throw_file_error(application, paths[i]);
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
            main_throw_file_error(application, paths[i]);
        }

        results[i].size = status.st_size;
        results[i].buffer = buffer;

        if (close(descriptor) == -1)
        {
            main_throw_file_error(application, paths[i]);
        }
    }
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

    struct StringBuilder application =
    {
        .length = strlen(args[0]),
        .buffer = args[0]
    };
    
    main_map_files(mappedFiles, args + optind, count - optind, &application);
    
    for (int i = 0; i < count - optind; i++)
    {
        printf("[%d]:\n", i);
        main_encode(mappedFiles + i);
    }

    return EXIT_SUCCESS;
}
