/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file libopeay_util.h
 *
 * Utility functions.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef LIBOPEAY_UTIL_H
#define LIBOPEAY_UTIL_H

#if defined(_DEBUG) || defined(SELFTEST)

const char* EVP_PKEY_type_name(int pkey_type);
const char* X509_V_ERR_name(int err);

#endif // defined(_DEBUG) || defined(SELFTEST)

#endif // LIBOPEAY_UTIL_H
