// main.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man3/fwrite.3p.html
//  - https://www.man7.org/linux/man-pages/man3/getopt.3.html
//  - https://www.man7.org/linux/man-pages/man3/perror.3.html
//  - https://www.man7.org/linux/man-pages/man3/sprintf.3p.html
//  - https://www.man7.org/linux/man-pages/man3/strtol.3.html

//  - https://www.man7.org/linux/man-pages/man3/pthread_cond_signal.3p.html
//  - https://www.man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "encoder.h"
#include "error.h"
#include "thread_pool.h"

static void main_print_usage(FILE* output, char* args[])
{
    fprintf(output, "Usage: %s [OPTION]... FILE...\n", args[0]);
}

static bool main_encode_sequential(MappedFileCollection mappedFiles)
{
    Encoder encoder = { 0 };

    for (int i = 0; i < mappedFiles->count; i++)
    {
        if (!encoder_next_encode(&encoder, mappedFiles->items[i]))
        {
            return false;
        }
    }

    return encoder_end_encode(encoder);
}

static void* main_consume(void* arg)
{
    ThreadPool pool = (ThreadPool)arg;
    Task current;
    int* result = calloc(1, sizeof * result);

    assert(result);

    if (!result)
    {
        return NULL;
    }

    while (thread_pool_dequeue(pool, &current))
    {
        off_t outputSize = task_execute(current);

        error_thread_ok(pthread_mutex_lock(&pool->mutex));

        current->outputSize = outputSize;
        pool->resultsCount++;

        if (pool->resultId == current->id)
        {
            pool->resultId++;

            error_thread_ok(pthread_cond_signal(&pool->consumer));
        }

        error_thread_ok(pthread_mutex_unlock(&pool->mutex));
    }

    return result;
}

static bool main_next_flush(ThreadPool pool)
{
    if (pool->flushId == 0 && pool->resultId > 0)
    {
        Task current = pool->items;

        if (current->outputSize >= 2)
        {
            unsigned char* output = current->output;
            size_t size = current->outputSize - 2;

            bool ok = fwrite(output, sizeof * output, size, stdout) == size;

            assert(ok);

            if (!ok)
            {
                return false;
            }
        }

        pool->flushId++;
    }

    for (; pool->flushId < pool->resultId; pool->flushId++)
    {
        Task current = pool->items + pool->flushId;
        Task previous = current - 1;
        size_t size = current->outputSize;
        off_t previousSize = previous->outputSize;

        if (size < 2 || previousSize < 2)
        {
            continue;
        }

        unsigned char* output = current->output;
        unsigned char symbol = output[0];
        unsigned int count = output[1];
        unsigned int previousCount = previous->output[previousSize - 1];
        unsigned int previousSymbol = previous->output[previousSize - 2];

        if (symbol == previousSymbol && count + previousCount <= UCHAR_MAX)
        {
            output[1] += previousCount;
        }
        else
        {
            Encoder encoder =
            {
                .previous = previousSymbol,
                .count = previousCount
            };

            if (!encoder_flush(encoder))
            {
                return false;
            }
        }

        if (size <= 2)
        {
            continue;
        }

        size -= 2;

        bool ok = fwrite(output, sizeof * output, size, stdout) == size;

        assert(ok);

        if (!ok)
        {
            return false;
        }
    }

    return true;
}

static bool main_end_flush(ThreadPool pool)
{
    if (!pool->flushId)
    {
        return true;
    }

    Task previous = pool->items + pool->flushId - 1;
    unsigned char* output = previous->output;
    off_t size = previous->outputSize;

    bool result = fwrite(output + size - 2, sizeof * output, 2, stdout) == 2;

    assert(result);

    return result;
}

static bool main_produce(
    ThreadPool pool,
    MappedFileCollection mappedFiles,
    unsigned long jobs)
{
    error_ok(pthread_mutex_lock(&pool->mutex));

    size_t id = 0;

    for (int i = 0; i < mappedFiles->count; i++)
    {
        MappedFile mappedFile = mappedFiles->items[i];
        off_t offsets = mappedFile.size / TASK_SIZE;
        off_t remainder = mappedFile.size % TASK_SIZE;
        struct Task* items = pool->items;

        for (off_t offset = 0; offset < offsets; offset++)
        {
            items[id].id = id;
            items[id].input = mappedFile.buffer + offset * TASK_SIZE;
            items[id].inputSize = TASK_SIZE;
            id++;
        }

        if (remainder)
        {
            items[id].id = id;
            items[id].input = mappedFile.buffer + offsets * TASK_SIZE;
            items[id].inputSize = remainder;
            id++;
        }
    }

    pool->count = id;

    for (unsigned long job = 0; job < jobs; job++)
    {
        error_ok(pthread_cond_signal(&pool->producer));
    }

    error_ok(pthread_mutex_unlock(&pool->mutex));

    for (;;)
    {
        error_ok(pthread_mutex_lock(&pool->mutex));
        error_ok(pthread_cond_wait(&pool->consumer, &pool->mutex));
        
        if (!main_next_flush(pool))
        {
            return false;
        }

        if (pool->resultsCount >= pool->count)
        {
            if (!main_end_flush(pool))
            {
                return false;
            }

            error_ok(pthread_mutex_unlock(&pool->mutex));

            return true;
        }

        error_ok(pthread_mutex_unlock(&pool->mutex));
    }
}

static bool main_encode_parallel(
    MappedFileCollection mappedFiles,
    unsigned long jobs)
{
    struct ThreadPool pool;

    if (!thread_pool(&pool, mappedFiles))
    {
        return false;
    }

    int ex = 0;
    pthread_t* consumers = malloc(jobs * sizeof * consumers);

    assert(consumers);

    if (!consumers)
    {
        goto encode_parallel_thread_pool;
    }

    for (unsigned long job = 0; job < jobs; job++)
    {
        ex = pthread_create(consumers + job, NULL, main_consume, &pool);

        assert(!ex);

        if (ex)
        {
            errno = ex;

            goto encode_parallel_consumers;
        }
    }

    if (!main_produce(&pool, mappedFiles, jobs))
    {
        goto encode_parallel_consumers;
    }

    for (unsigned long job = 0; job < jobs; job++)
    {
        void* result;

        ex = pthread_join(consumers[job], &result);

        assert(!ex && result && !*(int*)result);

        if (ex)
        {
            errno = ex;

            goto encode_parallel_consumers;
        }

        if (!result)
        {
            goto encode_parallel_consumers;
        }

        if (*(int*)result)
        {
            errno = *(int*)result;

            free(result);
            
            goto encode_parallel_consumers;
        }
        
        free(result);
    }

encode_parallel_consumers:
    free(consumers);
encode_parallel_thread_pool:
    finalize_thread_pool(&pool);

    return !ex;
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
