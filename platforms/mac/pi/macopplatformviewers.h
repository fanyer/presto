/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** $Id$
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MACOPPLATFORMVIEWERS_H
#define MACOPPLATFORMVIEWERS_H

#include "modules/pi/system/OpPlatformViewers.h"

class MacOpPlatformViewers: public OpPlatformViewers
{
	static OP_STATUS Create(OpPlatformViewers** new_platformviewers);

	virtual OP_STATUS OpenInDefaultBrowser(const uni_char* url);
	
	virtual OP_STATUS OpenInOpera(const uni_char* url);
};

#endif // MACOPPLATFORMVIEWERS_H