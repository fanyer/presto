/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file openssl_util.h
 *
 * Utility functions, used by OpenSSL-specific code.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OPENSSL_UTIL_H
#define OPENSSL_UTIL_H

#ifdef LIBOPEAY_ENABLE_PARTLY_OPENSSL_SUPPORT

#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/err.h"


/** Convert OpenSSL error queue into OP_STATUS.
 *
 * "Positive" version.
 *
 * This function should be called immediately after every call to OpenSSL
 * which can push errors into OpenSSL error queue, unless you handle
 * the error queue another way.
 *
 * If success_condition is true - it ensures that the error queue is clean
 * and returns OpStatus::OK.
 *
 * If success_condition is false - it processes the error queue, pops up all errors
 * and figures out the corresponding OP_STATUS code.
 *
 * In any case the queue will be clean after returning from this function.
 *
 * @param [in] success_condition - Condition, considered successful
 *                                 for just-made call to OpenSSL.
 *
 * @return OP_STATUS code.
 */
OP_STATUS openssl_success_if(bool success_condition);

/** Convert OpenSSL error queue into OP_STATUS.
 *
 * "Negative" version.
 *
 * This function does the opposite of openssl_success_if():
 * it treats the argument as an error condition
 * instead of success_condition.
 *
 */
inline OP_STATUS openssl_error_if(bool error_condition)
{
	return openssl_success_if(!error_condition);
}

/** OpenSSL-specific version of RETURN_IF_ERROR, positive. */
#define OPENSSL_VERIFY_OR_RETURN(success_condition) \
	RETURN_IF_ERROR(openssl_success_if(!!(success_condition)))

/** OpenSSL-specific version of RETURN_IF_ERROR, negative. */
#define OPENSSL_RETURN_IF(error_condition) \
	RETURN_IF_ERROR(openssl_error_if(!!(error_condition)))

/** OpenSSL-specific version of LEAVE_IF_ERROR, positive. */
#define OPENSSL_VERIFY_OR_LEAVE(success_condition) \
	LEAVE_IF_ERROR(openssl_success_if(!!(success_condition)))

/** OpenSSL-specific version of LEAVE_IF_ERROR, negative. */
#define OPENSSL_LEAVE_IF(error_condition) \
	LEAVE_IF_ERROR(openssl_error_if(!!(error_condition)))

/** OpenSSL-specific macro break-on-error, negative. */
#define OPENSSL_BREAK_IF(error_condition) \
	{ ERR_clear_error(); if (error_condition) break; }

/** OpenSSL-specific macro break-on-error, positive. */
#define OPENSSL_VERIFY_OR_BREAK(success_condition) \
	OPENSSL_BREAK_IF( !(success_condition) )

/** OpenSSL-specific macro break-on-error with OP_STATUS saving, negative. */
#define OPENSSL_BREAK2_IF(error_condition, opstatus_variable) \
	{ \
		const bool OPENSSL_BREAK2_IF_TMP_VAR = !!(error_condition); \
		opstatus_variable = openssl_error_if(OPENSSL_BREAK2_IF_TMP_VAR); \
		if (OPENSSL_BREAK2_IF_TMP_VAR) \
			break; \
	}

/** OpenSSL-specific macro break-on-error with OP_STATUS saving, positive. */
#define OPENSSL_VERIFY_OR_BREAK2(success_condition, opstatus_variable) \
	OPENSSL_BREAK2_IF( !(success_condition), opstatus_variable )

#endif // LIBOPEAY_ENABLE_PARTLY_OPENSSL_SUPPORT
#endif // OPENSSL_UTIL_H
