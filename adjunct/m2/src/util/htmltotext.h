/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef HTML_TO_TEXT_H
#define HTML_TO_TEXT_H

#include "adjunct/m2/src/include/defs.h"

namespace HTMLToText
{
	/** Convert the HTML part of the currently visible mail message to text
	  *
	  * NB TODO This function was moved here from m2glue.cpp to isolate the madness.
	  *    This function is nasty and shouldn't be here; core should have a function
	  *    available to convert HTML to text. See bug #328157. Avoid if possible.
	  *
	  * @param message_id ID of Message to convert, only used to check if this is current message
	  * @param plain_text Plain text output
	  */
	OP_STATUS GetCurrentHTMLMailAsText(message_gid_t message_id, OpString16& plain_text);
};

#endif // HTML_TO_TEXT_H
