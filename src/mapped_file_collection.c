// mapped_file_collection.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "mapped_file_collection.h"

static void mapped_file_collection_unmap(struct MappedFile* items, size_t count)
{
    for (size_t i = 0; i < count; i++) 
    {
        munmap(items[i].buffer, items[i].size);
    }

    free(items);
}

int mapped_file_collection(
    MappedFileCollection instance, 
    String paths[], 
    size_t count)
{
    MappedFile items = malloc(count * sizeof * items);

    if (!items)
    {
        return -1;
    }

    for (size_t i = 0; i < count; i++)
    {
        int descriptor = open(paths[i], O_RDONLY);

        if (descriptor == -1)
        {
            mapped_file_collection_unmap(items, i);

            return i;
        }

        struct stat status;

        if (fstat(descriptor, &status) == -1)
        {
            mapped_file_collection_unmap(items, i);

            return i;
        }

        unsigned char* buffer = mmap(
            NULL,
            status.st_size,
            PROT_READ,
            MAP_PRIVATE,
            descriptor,
            0);

        if (buffer == MAP_FAILED)
        {
            mapped_file_collection_unmap(items, i);

            return i;
        }

        items[i].size = status.st_size;
        items[i].buffer = buffer;

        if (close(descriptor) == -1)
        {
            mapped_file_collection_unmap(items, i);

            return i;
        }
    }

    instance->count = count;
    instance->items = items;

    return count;
}

void finalize_mapped_file_collection(MappedFileCollection instance)
{
    mapped_file_collection_unmap(instance->items, instance->count);

    instance->count = 0;
}
