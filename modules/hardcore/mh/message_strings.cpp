/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef MESSAGE_NAMES

const char* GetStringFromMessage(OpMessage msg)
{
	const char* name;

	switch ( msg )
	{
	case MSG_NO_MESSAGE: name = "MSG_NO_MESSAGE"; break;
#include "modules/hardcore/mh/generated_message_strings.inc"
	default: name = "MSG_UNKNOWN";
	}

	return name;
}

#endif // MESSAGE_NAMES
