// error.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

#include <assert.h>
#include <errno.h>
#define error_ok(condition) do                                                 \
{                                                                              \
    int ex = (condition);                                                      \
                                                                               \
    assert(!ex);                                                               \
                                                                               \
    if (ex)                                                                    \
    {                                                                          \
        errno = ex;                                                            \
                                                                               \
        return false;                                                          \
    }                                                                          \
} while (false)
#define error_thread_ok(condition) do                                          \
{                                                                              \
    int ex = (condition);                                                      \
                                                                               \
    assert(!ex);                                                               \
                                                                               \
    if (ex)                                                                    \
    {                                                                          \
        *result = ex;                                                          \
                                                                               \
        return result;                                                         \
    }                                                                          \
} while (false)
