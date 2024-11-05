// main.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man3/getopt.3.html
//  - https://www.man7.org/linux/man-pages/man3/perror.3.html
//  - https://www.man7.org/linux/man-pages/man3/sprintf.3p.html
//  - https://www.man7.org/linux/man-pages/man3/strtol.3.html

//  - https://www.man7.org/linux/man-pages/man3/pthread_create.3.html

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "encoder.h"
#include "mapped_file_collection.h"

static void main_print_usage(FILE* output, char* args[])
{
    fprintf(output, "Usage: %s [OPTION]... FILE...\n", args[0]);
}

static bool main_encode_sequential(MappedFileCollection mappedFiles)
{
    Encoder encoder = { 0 };

    for (int i = 0; i < mappedFiles->count; i++)
    {    
        if (!encoder_next_encode(&encoder, mappedFiles->items + i))
        {
            return false;
        }
    }

    encoder_end_encode(encoder);

    return true;
}

static void* main_produce()
{
    fprintf(stderr, "From producer, hello!\n");

    return NULL;
}

static void* main_consume()
{
    fprintf(stderr, "From consumer, goodbye!\n");

    return NULL;
}

static bool main_encode_parallel(
    MappedFileCollection mappedFiles, 
    unsigned long jobs)
{
    // (1) create a producer thread that reads the mapped files
    // (2) create consumer threads, one for each job

    int ex;
    pthread_t producer;

    if ((ex = pthread_create(&producer, NULL, main_produce, NULL)))
    {
        errno = ex;

        return false;
    }

    pthread_t* consumers = malloc(jobs * sizeof * consumers);

    for (unsigned long job = 0; job < jobs; job++)
    {
        if ((ex = pthread_create(consumers + job, NULL, main_consume, NULL)))
        {
            errno = ex;

            return false;
        }
    }

    for (unsigned long job = 0; job < jobs; job++)
    {
        if ((ex = pthread_join(consumers[job], NULL)))
        {
            errno = ex;

            return false;
        }
    }

    free(consumers);

    if ((ex = pthread_join(producer, NULL)))
    {
        errno = ex;

        return false;
    }

    return true;
}

int main(int count, char* args[])
{
    int option;
    unsigned long jobs = 1;

    while ((option = getopt(count, args, "hj:")) != -1)
    {
        switch (option)
        {
        case 'h':
            main_print_usage(stdout, args);

            return EXIT_SUCCESS;

        case 'j':
            errno = 0;
            jobs = strtoul(optarg, NULL, 10);

            if (errno || jobs < 1)
            {
                main_print_usage(stderr, args);

                return EXIT_FAILURE;
            }
            break;
            
        default: return EXIT_FAILURE;
        }
    }

    if (optind >= count)
    {
        main_print_usage(stderr, args);

        return EXIT_FAILURE;
    }

    struct MappedFileCollection mappedFiles;
    int fileCount = count - optind;
    int ex = mapped_file_collection(&mappedFiles, args + optind, fileCount);
    char* app = args[0];

    if (ex == -1)
    {
        perror(app);

        return EXIT_FAILURE;
    }

    if (ex < fileCount)
    {
        char* path = args[optind + ex];

        fprintf(stderr, "%s: %s: %s\n", app, path, strerror(errno));
        
        return EXIT_FAILURE;
    }

    bool result;

    if (jobs == 1)
    {
        result = main_encode_sequential(&mappedFiles);
    }
    else
    {
        result = main_encode_parallel(&mappedFiles, jobs);
    }

    finalize_mapped_file_collection(&mappedFiles);

    if (!result)
    {
        perror(app);

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
