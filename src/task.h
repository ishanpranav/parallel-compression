// task.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <sys/types.h>
#include <stdbool.h>
#define TASK_SIZE 4096

struct Task
{
    unsigned char output[TASK_SIZE * sizeof(Encoder)];
    off_t inputSize;
    off_t outputSize;
    size_t id;
    unsigned char* input;
};

typedef struct Task* Task;
