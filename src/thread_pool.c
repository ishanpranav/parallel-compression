// thread_pool.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

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

    return true;
}

void finalize_thread_pool(ThreadPool instance)
{
    finalize_task_queue(&instance->tasks);
    finalize_task_queue(&instance->completedTasks);
}
