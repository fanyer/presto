/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl@opera.com)
 */

#ifndef PLUGIN_CRASH_CONTROLLER_H
#define PLUGIN_CRASH_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"
#include "adjunct/desktop_util/adt/opproperty.h"

class PluginViewer;
class LogSender;

class PluginCrashController : public PopupDialogContext
{
public:
	/** Constructor
	 * @param anchor_widget Widget that controller should use for placement
	 * @param path Path of the crashed plugin
	 * @param logsender Log sender used to send the crashlog
	 */
	PluginCrashController(OpWidget* anchor_widget, const uni_char* url, LogSender& logsender);

	// From PopupDialogContext
	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);

private:
	virtual void InitL();

	PluginViewer* m_plugin;
	OpMessageAddress m_address;
	OpProperty<OpString> m_url;
	OpProperty<OpString> m_comments;
	LogSender& m_logsender;
};

#endif // PLUGIN_CRASH_CONTROLLER_H
