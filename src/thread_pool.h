// thread_pool.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://en.wikipedia.org/wiki/Thread_pool

#include <pthread.h>
#include "task_queue.h"

struct ThreadPool
{
    pthread_mutex_t mutex;
    pthread_cond_t empty;
    struct TaskQueue tasks;
    struct TaskQueue completedTasks;
};

typedef struct ThreadPool* ThreadPool;

bool thread_pool(ThreadPool instance);
void finalize_thread_pool(ThreadPool instance);
