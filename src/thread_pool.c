// thread_pool.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://en.wikipedia.org/wiki/Thread_pool
//  - https://www.man7.org/linux/man-pages/man3/pthread_cond_init.3.html
//  - https://www.man7.org/linux/man-pages/man3/pthread_mutex_lock.3.html

#include "thread_pool.h"

bool thread_pool(ThreadPool instance)
{
    if (!task_queue(&instance->tasks))
    {
        return false;
    }

    if (!task_queue(&instance->completedTasks))
    {
        return false;
    }

    pthread_cond_init(&instance->empty, NULL);

    return true;
}

void finalize_thread_pool(ThreadPool instance)
{
    finalize_task_queue(&instance->tasks);
    finalize_task_queue(&instance->completedTasks);
    pthread_cond_destroy(&instance->empty);
}
