/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined UNICODE_SELFTEST_UTILITIES_H && defined SELFTEST
#define UNICODE_SELFTEST_UTILITIES_H

#include "modules/unicode/unicode.h"

#ifdef USE_UNICODE_LINEBREAK
/** Map linebreakclass to name */
const char *lbname(LinebreakClass cl);
#endif

#endif
