/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_MEMORY_DEBUG_H
#define WINDOWS_MEMORY_DEBUG_H

#ifdef _DEBUG
# define _CRTDBG_MAP_ALLOC
# include <stdlib.h>
# include <crtdbg.h>
# define DEBUG_NEW				new(_NORMAL_BLOCK, __FILE__, __LINE__)
# ifdef OP_NEW
#  undef OP_NEW
# endif // OP_NEW
# define OP_NEW(obj, args)		::new(_NORMAL_BLOCK, __FILE__, __LINE__) obj##args
# ifdef OP_NEWA
#  undef OP_NEWA
# endif // OP_NEWA
# define OP_NEWA(obj, count)	::new(_NORMAL_BLOCK, __FILE__, __LINE__) obj[count]
# ifdef OP_NEW_L
#  undef OP_NEW_L
# endif // OP_NEW_L
# define OP_NEW_L(obj, args)	::new(_NORMAL_BLOCK, __FILE__, __LINE__) obj##args
# ifdef OP_NEWA_L
#  undef OP_NEWA_L
# endif // OP_NEWA_L
# define OP_NEWA_L(obj, count)	::new(_NORMAL_BLOCK, __FILE__, __LINE__) obj[count]

#endif // _DEBUG
#endif // !WINDOWS_MEMORY_DEBUG_H
