// encoder.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man3/fwrite.3p.html

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "encoder.h"

bool encoder_flush(Encoder value)
{
    return fwrite(&value, sizeof value, 1, stdout);
}

bool encoder_next_encode(Encoder* instance, MappedFile input)
{
    Encoder clone = *instance;

    for (off_t i = 0; i < input.size; i++)
    {
        unsigned char current = input.buffer[i];

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

off_t encoder_encode(
    unsigned char output[], 
    Encoder* instance,
    unsigned char input[],
    off_t inputSize)
{
    Encoder clone = *instance;
    off_t outputSize = 0;

    for (off_t i = 0; i < inputSize; i++)
    {
        unsigned char current = input[i];

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
        
        memcpy(output + outputSize, &clone, sizeof clone);
        
        outputSize += sizeof clone;
        clone.count = 1;
        clone.previous = current;
    }

    if (clone.count)
    {
        memcpy(output + outputSize, &clone, sizeof clone);

        outputSize += sizeof clone;
    }

    *instance = clone;

    return outputSize;
}

bool encoder_end_encode(Encoder instance)
{
    return !instance.count || encoder_flush(instance);
}
