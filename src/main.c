// main.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man3/getopt.3.html
//  - https://www.man7.org/linux/man-pages/man3/perror.3.html
//  - https://www.man7.org/linux/man-pages/man3/sprintf.3p.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "encoder.h"
#include "euler.h"
#include "mapped_file_collection.h"

static void main_print_usage(String args[])
{
    printf("Usage: %s [OPTION]... FILE...\n", args[0]);
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

    struct MappedFileCollection mappedFiles;
    int fileCount = count - optind;
    int ex = mapped_file_collection(&mappedFiles, args + optind, fileCount);

    if (ex == -1)
    {
        perror(args[0]);

        return EXIT_FAILURE;
    }

    if (ex < fileCount)
    {
        String path = args[optind + ex];
        String errorMessage = malloc(strlen(args[0]) + strlen(path) + 3);

        if (!errorMessage)
        {
            perror(args[0]);

            return EXIT_FAILURE;
        }

        sprintf(errorMessage, "%s: %s", args[0], path);
        perror(errorMessage);
        free(errorMessage);
        
        return EXIT_FAILURE;
    }

    struct Encoder runLengthEncoder = { 0 };

    for (int i = 0; i < fileCount; i++)
    {    
        if (!encoder_next_encode(&runLengthEncoder, mappedFiles.items + i))
        {
            perror(args[0]);
            finalize_mapped_file_collection(&mappedFiles);

            return EXIT_FAILURE;
        }
    }

    encoder_end_encode(&runLengthEncoder);
    finalize_mapped_file_collection(&mappedFiles);

    return EXIT_SUCCESS;
}
