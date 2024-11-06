// task.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <sys/types.h>
#include <stdbool.h>
#define TASK_SIZE 4096

struct Task
{
    off_t inputSize;
    off_t outputSize;
    unsigned char* input;
    unsigned char output[TASK_SIZE * sizeof(Encoder)];
};

typedef struct Task* Task;
