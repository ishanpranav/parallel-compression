// task_queue.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <stdlib.h>
#include "task_queue.h"

bool task_queue(TaskQueue instance)
{
    instance->first = NULL;
    instance->last = NULL;

    return true;
}

bool task_queue_enqueue(
    TaskQueue instance, 
    off_t offset, 
    off_t size,
    unsigned char buffer[])
{
    TaskQueueNode added = malloc(sizeof * added);

    if (!added)
    {
        return false;
    }

    added->offset = offset;
    added->size = size;
    added->buffer = buffer;
    added->next = NULL;
    
    if (instance->last == NULL) 
    {
        instance->first = added;
        instance->last = added;
    
        return true;
    }

    instance->last->next = added;
    instance->last = added;

    return true;
}

bool task_queue_try_dequeue(TaskQueue instance, TaskQueueNode result)
{
    if (!instance->first) 
    {
        return false;
    }

    TaskQueueNode removed = instance->first;

    if (result) 
    {
        *result = *removed;
    }

    if (instance->first == instance->last)
    {
        instance->last = NULL;
    }

    instance->first = instance->first->next;

    free(removed);

    return true;
}

void finalize_task_queue(TaskQueue instance)
{
    while (task_queue_try_dequeue(instance, NULL)) { }
}
