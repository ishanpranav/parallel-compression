// main.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man3/getopt.3.html

#include <stdio.h>
#include <unistd.h>
#include "euler.h"

void main_print_usage(String args[])
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

    for (int i = optind; i < count; i++)
    {
        printf("compressing '%s'...\n", args[i]);
    }

    return EXIT_SUCCESS;
}
