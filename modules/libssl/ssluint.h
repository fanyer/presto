/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef _SSLUINT_H_
#define _SSLUINT_H_

#ifdef HAVE_INT64

typedef uint32 uint;
typedef uint32 uint24;
typedef UINT64 uint64;

#else

typedef uint32 uint;
typedef uint32 uint24;

#include "modules/libssl/base/uint64.h"

#endif

#define SSL_SIZE_UINT_8		1
#define SSL_SIZE_UINT_16	2
#define SSL_SIZE_UINT_24	3
#define SSL_SIZE_UINT_32	4
#define SSL_SIZE_UINT_64	8

#endif 
