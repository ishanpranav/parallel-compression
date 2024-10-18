// main.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man3/getopt.3.html

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "euler.h"

void main_print_usage(String args[])
{
    printf("Usage: %s [OPTION]... FILE...\n", args[0]);
}

void main_encode(char destination[], char source[], size_t count)
{

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

    size_t length = 0;
    size_t capacity = 4;
    size_t applicationLength = strlen(args[0]);
    char* buffer = malloc(capacity);

    euler_assert(buffer);

    for (int i = optind; i < count; i++)
    {
        FILE* inputStream = fopen(args[i], "r");

        if (!inputStream)
        {
            size_t argLength = strlen(args[i]);
            char* errorMessage = malloc(applicationLength + argLength + 3);

            euler_assert(errorMessage);
            sprintf(errorMessage, "%s: %s", args[0], args[i]);
            perror(errorMessage);

            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
