// thread_pool.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <sys/types.h>
#define TASK_SIZE 4096

/** */
struct Task
{
    off_t inputSize;
    off_t outputSize;
    size_t id;
    unsigned char* input;
    unsigned char output[TASK_SIZE * 2];
};

/** */
typedef struct Task* Task;

/**
 * 
 * @param instance
 * @return
 */
off_t task_execute(Task instance);
