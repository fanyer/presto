/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DAPI_VCARD_SUPPORT

#include "modules/device_api/src/VCardGlobals.h"

VCardGlobals* VCardGlobals::MakeL()
{
	VCardGlobals* retval = OP_NEW_L(VCardGlobals, ());
	retval->address_type_names[0] = UNI_L("dom");
	retval->address_type_names[1] = UNI_L("intl");
	retval->address_type_names[2] = UNI_L("postal");
	retval->address_type_names[3] = UNI_L("parcel");
	retval->address_type_names[4] = UNI_L("home");
	retval->address_type_names[5] = UNI_L("work");
	retval->address_type_names[6] = UNI_L("pref");


	retval->phone_type_names[0] = UNI_L("home");
	retval->phone_type_names[1] = UNI_L("work");
	retval->phone_type_names[2] = UNI_L("cell");
	retval->phone_type_names[3] = UNI_L("pref");
	retval->phone_type_names[4] = UNI_L("voice");
	retval->phone_type_names[5] = UNI_L("fax");
	retval->phone_type_names[6] = UNI_L("video");
	retval->phone_type_names[7] = UNI_L("pager");
	retval->phone_type_names[8] = UNI_L("bbs");
	retval->phone_type_names[9] = UNI_L("modem");
	retval->phone_type_names[10] = UNI_L("car");
	retval->phone_type_names[11] = UNI_L("isdn");
	retval->phone_type_names[12] = UNI_L("pcs");
	retval->phone_type_names[13] = UNI_L("msg");

	retval->email_type_names[0] = UNI_L("internet");
	retval->email_type_names[1] = UNI_L("x400");
 	retval->email_type_names[2] = UNI_L("pref");
	return retval;
}

#endif // DAPI_VCARD_SUPPORT

