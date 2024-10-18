// mapped_file_collection.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <stdbool.h>
#include "euler.h"
#include "mapped_file.h"

/**  */
struct MappedFileCollection
{
    size_t count;
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
    String paths[], 
    size_t count);

/**
 * 
 * @param instance
 */
void finalize_mapped_file_collection(MappedFileCollection instance);
