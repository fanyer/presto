/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef UNIX_DEFAULTSTYLE_H
#define UNIX_DEFAULTSTYLE_H

#include "modules/display/FontAtt.h"
#include "modules/pi/ui/OpUiInfo.h"

namespace UnixDefaultStyle
{
	/** See documentation for GetSystemFont() in OpSystemInfo (core-1) or
		OpUiInfo (core-2). */
	void GetSystemFont(OP_SYSTEM_FONT type, FontAtt &font);
}

#endif // UNIX_DEFAULTSTYLE_H
