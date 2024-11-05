// thread_pool.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://en.wikipedia.org/wiki/Thread_pool

#include <pthread.h>
#include "mapped_file_collection.h"
#include "task_queue.h"

struct ThreadPool
{
    pthread_cond_t empty;
    struct TaskQueue tasks;
    struct TaskQueue completedTasks;
    pthread_t* threads;
};

typedef struct ThreadPool* ThreadPool;

bool thread_pool(
    ThreadPool instance, 
    MappedFileCollection mappedFiles,
    unsigned long workers);

void finalize_thread_pool(ThreadPool instance);
