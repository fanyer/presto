/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef OPTREECALLBACK_SUPPORT

#include "modules/logdoc/optreecallback.h"
#include "modules/logdoc/src/xmlsupport.h"
#include "modules/logdoc/logdoc.h"

/* static */ OP_STATUS
OpTreeCallback::MakeTokenHandler(XMLTokenHandler *&tokenhandler, LogicalDocument *logdoc, OpTreeCallback *callback, const uni_char *fragment)
{
	tokenhandler = OP_NEW(LogdocXMLTokenHandler, (logdoc));
	if (!tokenhandler || OpStatus::IsMemoryError(((LogdocXMLTokenHandler *) tokenhandler)->SetTreeCallback(callback, fragment)))
	{
		OP_DELETE(tokenhandler);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/* virtual */
OpTreeCallback::~OpTreeCallback()
{
}

#endif // OPTREECALLBACK_SUPPORT
