/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file openssl_util.cpp
 *
 * Utility functions, used by OpenSSL-specific code.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */


#include "core/pch.h"

#ifdef LIBOPEAY_ENABLE_PARTLY_OPENSSL_SUPPORT

#include "modules/libcrypto/src/openssl_impl/openssl_util.h"


OP_STATUS openssl_success_if(bool success_condition)
{
	if (success_condition)
	{
		// Successful case.
		// There shouldn't be any errors.
		OP_ASSERT(ERR_peek_error() == 0);
		// Forced clearing of the error queue here, if it's still not empty.
		// Rationale: it will help to localize errors.
		ERR_clear_error();
		return OpStatus::OK;
	}

	// Default error.
	OP_STATUS status = OpStatus::ERR;

	// Process error queue.
	for (;;)
	{
		unsigned long err = ERR_get_error();
		if (err == 0)
			break;

		// Handle malloc failure specially.
		if (ERR_GET_REASON(err) == ERR_R_MALLOC_FAILURE)
		{
			status = OpStatus::ERR_NO_MEMORY;
			// Clear eventual errors left in the queue.
			ERR_clear_error();
			break;
		}
	}

	OP_ASSERT(ERR_peek_error() == 0);
	return status;
}

#endif // LIBOPEAY_ENABLE_PARTLY_OPENSSL_SUPPORT
