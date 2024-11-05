// encoder.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://en.wikipedia.org/wiki/Run-length_encoding

#include <stdbool.h>
#include "mapped_file.h"

/** */
struct Encoder
{
    unsigned char previous;
    unsigned char count;
};

/** */
typedef struct Encoder Encoder;

/**
 * 
 * @param value
 * @param input
 * @return 
 */
bool encoder_next_encode(Encoder* value, MappedFile input);

/**
 * 
 * @param value
 * @return 
 */
bool encoder_end_encode(Encoder value);

/**
 * 
 * @param output
 * @param instance
 * @param input
 * @param inputSize
 * @return 
 */
off_t encoder_encode(
    unsigned char output[], 
    Encoder* instance,
    unsigned char input[],
    off_t inputSize);
