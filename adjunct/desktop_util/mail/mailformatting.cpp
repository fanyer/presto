/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/mail/mailformatting.h"


/***********************************************************************************
 ** Get the email address from an List-ID (mailing list ID)
 **
 ** MailFormatting::FormatListIdToEmail
 ** @param list_id M2 List ID
 ** @param email Email address (output)
 **
 ***********************************************************************************/
OP_STATUS MailFormatting::FormatListIdToEmail(const OpStringC& list_id, OpString& email)
{
	RETURN_IF_ERROR(email.Set(list_id));

	int pos = email.Find(UNI_L("@"));
	if (pos != KNotFound)
	{
		return OpStatus::OK;
	}
	pos = email.Find(UNI_L("."));
	if (pos == KNotFound)
	{
		return OpStatus::OK;
	}

	email.CStr()[pos] = '@';

	return OpStatus::OK;
}


#endif //M2_SUPPORT
