/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2005 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#if defined(USE_ZLIB) && !defined(USE_SYSTEM_ZLIB)
#include "modules/zlib/zlib.h"
#include "modules/zlib/zutil.h"

# ifndef HAVE_MEMCMP
int op_memcmp(const void* a, const void* b, size_t n);
# endif
# ifndef HAVE_MEMCPY
void* op_memcpy(void *dest, const void *src, size_t n);
# endif
# ifndef HAVE_MEMSET
void* op_memset(void *s, int c, size_t n);
# endif

void *_zalloc (unsigned int items)
{
    void *res = op_malloc(items);
    if( res )
	zmemzero( res, items );
    return res;
}

void  _zfree (void *ptr)
{
    if(ptr) op_free(ptr);
}

void zmemzero( void *ptr, unsigned int len )
{
    op_memset( ptr, 0, len );
}

int zmemcmp( const void *ptr, const void *ptr2, unsigned int len )
{
    return op_memcmp( ptr, ptr2, len );
}

void zmemcpy( void *ptr, const void *ptr2, unsigned int len )
{
    op_memcpy( ptr, ptr2, len );
}
#endif // defined(USE_ZLIB) && !defined(USE_SYSTEM_ZLIB)
