/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef OP_PLUGIN_CRASHED_BAR_H
#define OP_PLUGIN_CRASHED_BAR_H

#include "adjunct/desktop_util/crash/logsender.h"
#include "adjunct/quick/widgets/OpInfobar.h"

class OpProgressBar;
class OpLabel;

/** @brief Toolbar appearing when a plugin has crashed
 * Toolbar appears when a plugin has crashed that affects the page visible in
 * the window the toolbar appears in.
 */
class OpPluginCrashedBar : public OpInfobar, public LogSender::Listener
{
public:
	static OP_STATUS Construct(OpPluginCrashedBar** bar) { return QuickOpWidgetBase::Construct(bar); }
	OpPluginCrashedBar();

	OP_STATUS Init() { return OpInfobar::Init("Plugin Crashed Toolbar"); }

	OP_STATUS ShowCrash(const OpStringC& path, const OpMessageAddress& address);

	// From OpInfoBar
	virtual BOOL OnInputAction(OpInputAction* action);
	virtual const char* GetInputContextName() { return "Plugin Crashed Toolbar"; }
	virtual BOOL Hide();

	// From LogSender::Listener
	virtual void OnSendingStarted(LogSender* sender);
	virtual void OnSendSucceeded(LogSender* sender);
	virtual void OnSendFailed(LogSender* sender);

private:
	OP_STATUS FindFile(time_t crash_time);
	void OnSendingStarted(const OpMessageAddress& address);
	void OnSendSucceeded(const OpMessageAddress& address);
	void OnSendFailed(const OpMessageAddress& address);

	static const time_t MaxCrashTimePassed = 2; // seconds

	bool m_found_log_file;
	bool m_sending_report;
	bool m_sending_failed;
	OpProgressBar* m_spinner;
	OpLabel* m_progress_label;
	OpWidget* m_send_button;
	OpAutoPtr<LogSender> m_logsender;
	OpMessageAddress m_address;

	static OtlList<OpPluginCrashedBar*> s_visible_bars;
};

#endif // OP_PLUGIN_CRASHED_BAR_H
