/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @shuais
 */

#ifndef CRASH_LOGGING_DIALOG_H
#define CRASH_LOGGING_DIALOG_H

#include "adjunct/desktop_util/crash/logsender.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"

class OpProgressBar;
class OpWidget;

class CrashLoggingDialog
	: public Dialog
	, public LogSender::Listener
{
public:
	CrashLoggingDialog(OpStringC& file, BOOL& restart, BOOL& minimal);
	virtual ~CrashLoggingDialog(void){}

protected:
	virtual	void				OnInit();
	virtual void				OnButtonClicked(INT32 index);
	virtual void				OnClick(OpWidget *widget, UINT32 id);
	virtual void				OnClose(BOOL user_initiated);

	virtual BOOL				GetIsBlocking() {return TRUE;}
	virtual BOOL				GetShowProgressIndicator() const { return TRUE; }
	virtual const char*			GetWindowName()			{return "Crash Dialog";}
	virtual DialogImage			GetDialogImageByEnum()	{return IMAGE_NONE;}
	virtual BOOL				GetIsResizingButtons()  {return TRUE;}

	virtual INT32				GetButtonCount()		{return 2;}
	virtual void				GetButtonInfo(INT32 index,
										  OpInputAction*& action,
										  OpString& text,
										  BOOL& is_enabled,
										  BOOL& is_default,
										  OpString8& name);
	
	// OpInputContext
	virtual void			OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void			OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

	// LogSender::Listener
	virtual void			OnSendingStarted(LogSender* sender) {}
	virtual void			OnSendSucceeded(LogSender* sender);
	virtual void			OnSendFailed(LogSender* sender) { SetSendingStatus(FALSE); }


private:
	void						OnSent();
	OP_STATUS					OnSend();
	void						OnNotSend();
	void						SaveRecoveryStrategy();
	void						SetSendingStatus(BOOL); // TRUE: Sending  FALSE: Failed
	void						GetLastActiveTabL(OpString&);	

private:
	time_t m_crash_time;
	BOOL* p_restart;
	BOOL* p_minimal;
	OpString m_dmp_path;

	OpProgressBar* m_spinner;
	LogSender m_logsender;

	// support for multi-line ghost string, since Multiedit don't support it
	OpMultilineEdit* m_actions_field;
	OpString	m_ghost_string;
	WIDGET_COLOR m_original_color;
	void SetActionsGhostText();
	void ClearActionsGhostText();
};

#endif // CRASH_LOGGING_DIALOG_H
