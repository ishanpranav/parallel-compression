// mapped_file.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <sys/types.h>

// References:
//  - https://www.man7.org/linux/man-pages/man3/off_t.3type.html

/** */
struct MappedFile
{
    off_t size;
    char* buffer;
};

/** */
typedef struct MappedFile* MappedFile;
