/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/**
 * @file core_includes.h
 *
 * This file is included in all opera_*.cpp wrapper files in libopeay.
 *
 * This file must be included exactly once.
 * This file must not be included from other modules.
 *
 */

#ifdef CORE_INCLUDES_H
#error This file must be included exactly once, from an opera_*.cpp wrapper file.
#endif
#define CORE_INCLUDES_H

/* Add c++ header files that can not be included from the C source files here: */
#include "modules/util/datefun.h"

#define LIBOPEAY_X509_EX_DATA

#ifdef MSWIN
// avoid collisions with the debug free
#undef free
#endif // MSWIN

// op_functions.h must be included in the resulting compilation unit just
// before an *.c OpenSSL source file. Therefore its include statement must
// be the last line of this file.
#include "modules/libopeay/addon/op_functions.h"
