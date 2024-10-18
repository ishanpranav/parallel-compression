// encoder.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man2/write.2.html

#include <limits.h>
#include <string.h>
#include <unistd.h>
#include "encoder.h"

static bool encoder_flush(Encoder instance)
{
    return write(STDOUT_FILENO, &instance, sizeof instance) != -1;
}

bool encoder_next_encode(Encoder* instance, MappedFile input)
{
    Encoder clone = *instance;

    for (off_t i = 0; i < input->size; i++)
    {
        unsigned char current = input->buffer[i];

        if (!clone.count)
        {
            clone.previous = current;
            clone.count = 1;

            continue;
        }

        if (current == clone.previous && clone.count < UCHAR_MAX)
        {
            clone.count++;

            continue;
        }
        
        if (!encoder_flush(clone))
        {
            *instance = clone;

            return false;
        }

        clone.count = 1;
        clone.previous = current;
    }

    *instance = clone;

    return true;
}

bool encoder_end_encode(Encoder instance)
{
    return !instance.count || encoder_flush(instance);
}
