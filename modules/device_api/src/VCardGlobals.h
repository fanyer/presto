/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICEAPI_VCARDGLOBALS_H
#define DEVICEAPI_VCARDGLOBALS_H

#ifdef DAPI_VCARD_SUPPORT

class OpVCardEntry;
/** This class groups some strings needed in vCard implementation.
  */
class VCardGlobals
{
public:
	/** Constructs and returns VCardGlobals
	 * or leaves if any error occurs.
	 */
	static VCardGlobals* MakeL();
private:
	friend class OpVCardEntry;
	enum {
		  ADDRESS_TYPES_COUNT = 7
		, PHONE_TYPES_COUNT = 14
		, EMAIL_TYPES_COUNT = 3
	};
	const uni_char* address_type_names[ADDRESS_TYPES_COUNT];
	const uni_char* phone_type_names[PHONE_TYPES_COUNT];
	const uni_char* email_type_names[EMAIL_TYPES_COUNT];
};

#endif // DAPI_VCARD_SUPPORT

#endif // DEVICEAPI_VCARDGLOBALS_H
