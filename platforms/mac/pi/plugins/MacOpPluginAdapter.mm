/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "platforms/mac/pi/plugins/MacOpPluginAdapter.h"
#include "platforms/mac/model/BottomLineInput.h"

//////////////////////////////////////////////////////////////////////

void MacOpPluginAdapter::StartIME()
{
	[[BottomLineInput bottomLineInput] shouldShow];
}

///////////////////////////////////////////////////

bool MacOpPluginAdapter::IsIMEActive()
{ 
	return [[BottomLineInput bottomLineInput] isVisible];
}

///////////////////////////////////////////////////

void MacOpPluginAdapter::NotifyFocusEvent(bool focus, FOCUS_REASON reason)
{
#ifdef NS4P_COMPONENT_PLUGINS
	// Each time a plugin is focused save the destination address so the
	// composed text ends up in the right place
	if (focus && m_plugin_channel)
		[[BottomLineInput bottomLineInput] setDestMessageAddress:m_plugin_channel->GetDestination()];
#endif // NS4P_COMPONENT_PLUGINS
}

///////////////////////////////////////////////////

#endif // _PLUGIN_SUPPORT_

