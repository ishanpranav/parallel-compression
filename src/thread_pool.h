// thread_pool.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <pthread.h>
#include "mapped_file_collection.h"
#include "task.h"

/** */
struct ThreadPool
{
    size_t index;
    size_t count;
    size_t resultId;
    size_t flushId;
    size_t resultsCount;
    pthread_mutex_t mutex;
    pthread_cond_t producer;
    pthread_cond_t consumer;
    struct Task* items;
};

/** */
typedef struct ThreadPool* ThreadPool;

/**
 * 
 * 
 * @param instance
 * @param mappedFiles
 * @return 
 */
bool thread_pool(ThreadPool instance, MappedFileCollection mappedFiles);

/**
 * 
 * @param instance
 * @param result
 * @return
 */
bool thread_pool_dequeue(ThreadPool instance, Task* result);

/**
 * 
 * @param instance
 */
void finalize_thread_pool(ThreadPool instance);
