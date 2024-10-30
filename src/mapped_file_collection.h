// mapped_file_collection.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <stdbool.h>
#include "mapped_file.h"

/**  */
struct MappedFileCollection
{
    int count;
    struct MappedFile* items;
};

/**  */
typedef struct MappedFileCollection* MappedFileCollection;

/**
 * 
 * @param instance 
 * @param paths
 * @param count
 * @return 
 */
int mapped_file_collection(
    MappedFileCollection instance, 
    char* paths[], 
    int count);

/**
 * 
 * @param instance
 */
void finalize_mapped_file_collection(MappedFileCollection instance);
