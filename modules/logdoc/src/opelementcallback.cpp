/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef OPELEMENTCALLBACK_SUPPORT

#include "modules/logdoc/opelementcallback.h"
#include "modules/logdoc/src/xmlsupport.h"
#include "modules/logdoc/logdoc.h"

/* static */ OP_STATUS
OpElementCallback::MakeTokenHandler(XMLTokenHandler *&tokenhandler, LogicalDocument *logdoc, OpElementCallback *callback)
{
	tokenhandler = OP_NEW(LogdocXMLTokenHandler, (logdoc));
	if (!tokenhandler)
		return OpStatus::ERR_NO_MEMORY;
	((LogdocXMLTokenHandler *) tokenhandler)->SetElementCallback(callback);
	return OpStatus::OK;
}

/* virtual */
OpElementCallback::~OpElementCallback()
{
}

#endif // OPELEMENTCALLBACK_SUPPORT
