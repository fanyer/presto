/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef  _URL_ID_H_
#define _URL_ID_H_

#ifdef URL_USE_OLD_ID_TYPE
typedef int URL_ID;
#else
// Defines the various variants of the URL ID value, 32 and 64 bit
#ifdef SIXTY_FOUR_BIT
// 64 bit system
# ifdef URL_USE_64BIT_ID
// 64 bit ID
typedef UINT64 URL_ID;
# else
// 32 bit ID
typedef UINT32 URL_ID;
# endif
#else // SIXTY_FOUR_BIT
// 32 bit system
// 32 bit ID
typedef UINT32 URL_ID;
#endif // SIXTY_FOUR_BIT
#endif // URL_USE_OLD_ID_TYPE


#ifdef URL_USE_OLD_ID_TYPE
typedef unsigned int URL_CONTEXT_ID;
#else
// Defines the various variants of the URL Context ID value, 32 and 64 bit
#ifdef SIXTY_FOUR_BIT
// 64 bit system
# ifdef URL_USE_64BIT_CONTEXT_ID
// 64 bit ID
typedef UINT64 URL_CONTEXT_ID;
# else
// 32 bit ID
typedef UINT32 URL_CONTEXT_ID;
# endif
#else // SIXTY_FOUR_BIT
// 32 bit system
// 32 bit ID
typedef UINT32 URL_CONTEXT_ID;
#endif // SIXTY_FOUR_BIT
#endif // URL_USE_OLD_ID_TYPE

#endif // _URL_ID_H_
