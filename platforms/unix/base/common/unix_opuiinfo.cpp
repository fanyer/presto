/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Edward Welbourne (by copy-and-bodge from unix_opsysteminfo.cpp)
*/
#include "core/pch.h" // Coding standards requirement.

#include "unix_opuiinfo.h"
// NB: that #defines UnixOpUiInfo to UnixOpSystemInfo 

#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/hardcore/mh/messages.h"

void UnixOpUiInfo::GetFont(OP_SYSTEM_FONT font, FontAtt &retval, BOOL use_system_default)
{
	// hmm ... none of this is Unix-specific; why isn't it OpUiInfo's default implementation ?
	OP_STATUS rc;
	(void)rc;
	// FIXME-PREFSMAN3: Handle use_system_default
	g_pcfontscolors->GetFont(font, retval);
}
