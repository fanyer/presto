//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef MAILFORMATTING_H
#define MAILFORMATTING_H

#ifdef M2_SUPPORT

namespace MailFormatting
{
	/** Get the email address from an List-ID (mailing list ID)
	  * @param list_id mailing list List-ID
	  * @param email Email address (output)
	  */
	OP_STATUS FormatListIdToEmail(const OpStringC& list_id, OpString& email);
};

#endif // M2_SUPPORT

#endif // MAILFORMATTING_H
