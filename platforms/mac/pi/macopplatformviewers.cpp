/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** $Id$
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "MacOpPlatformViewers.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/data_types/OpenURLSetting.h"

OP_STATUS OpPlatformViewers::Create(OpPlatformViewers** new_platformviewers)
{
	OP_ASSERT(new_platformviewers != NULL);
	*new_platformviewers = new MacOpPlatformViewers();
	if(*new_platformviewers == NULL)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS MacOpPlatformViewers::OpenInDefaultBrowser(const uni_char* url) 
{ 
	return OpStatus::ERR; 
}

OP_STATUS MacOpPlatformViewers::OpenInOpera(const uni_char* url)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}