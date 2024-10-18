// string_builder.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

/**  */
struct StringBuilder
{
    size_t length;
    char* buffer;
};

/**  */
typedef struct StringBuilder* StringBuilder;
