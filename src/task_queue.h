// task_queue.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <sys/types.h>
#include <stdbool.h>

struct TaskQueueNode
{
    off_t offset;
    off_t length;
    char* buffer;
    struct TaskQueueNode* next;
};

struct TaskQueue
{
    struct TaskQueueNode* first;
    struct TaskQueueNode* last;
};

typedef struct TaskQueueNode* TaskQueueNode;
typedef struct TaskQueue* TaskQueue;

bool task_queue(TaskQueue instance);

bool task_queue_enqueue(
    TaskQueue instance, 
    off_t offset, 
    off_t length, 
    char* buffer);

bool task_queue_try_dequeue(TaskQueue instance, TaskQueueNode result);

void finalize_task_queue(TaskQueue instance);
