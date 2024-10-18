// exception.h
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// https://github.com/ishanpranav/codebook/blob/master/lib/exception.h

#ifndef EXCEPTION_f152f59488c248f48b77fc64f8dd0fc6
#define EXCEPTION_f152f59488c248f48b77fc64f8dd0fc6

/** Specifies an error that occurs at runtime. */
enum Exception
{
    /** There is not enough memory to continue the program. */
    EXCEPTION_OUT_OF_MEMORY = -1
};

/** Specifies an error that occurs at runtime. */
typedef enum Exception Exception;

#endif
