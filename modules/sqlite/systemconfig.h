/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

// This file redefines some system functions so that we don't have
// to alter the third party code in order to compile.

#undef assert

#ifdef OP_ASSERT
# define assert(a) OP_ASSERT(a)
#else
# define assert(a) ((void)0)
#endif

#define SQLITE_THREADSAFE 0
/* Explicitely disable all the code that leads with sqlite's extension loading mechanism, we don't
 * use it and the code uses LoadLibrary() to load dll:s, which we don't want as per DSK-311377 */
#define SQLITE_OMIT_LOAD_EXTENSION

#ifdef SQLITE_OPERA_PI
#define SQLITE_OS_OTHER 1
#endif

#if 1
#ifdef atoi
# undef atoi
#endif // atoi

#ifdef free
# undef free
#endif // free

#ifdef malloc
# undef malloc
#endif // malloc

#ifdef memcmp
# undef memcmp
#endif // memcmp

#ifdef memcpy
# undef memcpy
#endif // memcpy

#ifdef memset
# undef memset
#endif // memset

#ifdef strcmp
# undef strcmp
#endif // strcmp

#ifdef strncmp
# undef strncmp
#endif // strncmp

#ifdef strlen
# undef strlen
#endif // strlen

#ifdef memmove
# undef memmove
#endif // memmove

#ifdef realloc
# undef realloc
#endif // realloc

#ifdef localtime
# undef localtime
#endif // localtime

#ifdef strncpy
# undef strncpy
#endif // strncpy

#else

#ifdef atoi
# undef atoi
#endif // atoi
#define atoi op_atoi

#ifdef free
# undef free
#endif // free
#define free op_free

#ifdef malloc
# undef malloc
#endif // malloc
#define malloc op_malloc

#ifdef memcmp
# undef memcmp
#endif // memcmp
#define memcmp op_memcmp

#ifdef memcpy
# undef memcpy
#endif // memcpy
#define memcpy op_memcpy

#ifdef memset
# undef memset
#endif // memset
#define memset op_memset

#ifdef strcmp
# undef strcmp
#endif // strcmp
#define strcmp op_strcmp

#ifdef strncmp
# undef strncmp
#endif // strncmp
#define strncmp op_strncmp

#ifdef strlen
# undef strlen
#endif // strlen
#define strlen op_strlen

#ifdef memmove
# undef memmove
#endif // memmove
#define memmove op_memmove

#ifdef realloc
# undef realloc
#endif // realloc
#define realloc op_realloc

#ifdef localtime
# undef localtime
#endif // localtime
#define localtime op_localtime

#ifdef strncpy
# undef strncpy
#endif // strncpy
#define strncpy op_strncpy

#endif
