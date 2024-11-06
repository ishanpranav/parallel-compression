// mapped_file.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#ifndef MAPPED_FILE_22de81f5f5ab41b49e3f0398b0d9e3d6
#define MAPPED_FILE_22de81f5f5ab41b49e3f0398b0d9e3d6
#include <sys/types.h>

// References:
//  - https://www.man7.org/linux/man-pages/man3/off_t.3type.html

/** */
struct MappedFile
{
    off_t size;
    unsigned char* buffer;
};

/** */
typedef struct MappedFile MappedFile;

#endif
