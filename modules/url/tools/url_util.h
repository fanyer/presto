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

#ifndef _URL_UTIL_H_
#define _URL_UTIL_H_

struct URL_Custom_Header
{
	URL_Custom_Header()
	: bypass_security(FALSE){}
	OpString8 name;
	OpString8 value;

	/**
	 *  url normally filters out dangerous headers
	 * which should not be set by callers.
	 * If the caller needs t 	o bypass this filter,
	 * set bypass_security to TRUE.
	 *
	 * Must NOT be set by xmlhttprequest caller code.
	 */
	BOOL bypass_security;
};


/** @return a squashed set of OP_STATUS values, consisting of OK, ERR and any of valid1, valid2 ... (expand list as necessary).
 *     Other status values are mapped to OK if IsSuccess and ERR otherwise. */
OP_STATUS SquashStatus(OP_STATUS error, OP_STATUS valid1 = OpStatus::OK, OP_STATUS valid2 = OpStatus::OK, OP_STATUS valid3 = OpStatus::OK, OP_STATUS valid4 = OpStatus::OK, OP_STATUS valid5 = OpStatus::OK);

#endif // _URL_UTIL_H_
