/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Manuela Hutter
*/

#ifndef __CONFIRM_EXIT_DIALOG_H__
#define __CONFIRM_EXIT_DIALOG_H__

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "modules/locale/locale-enum.h"
#include "modules/windowcommander/OpWindowCommander.h"

/***********************************************************************************
**
**	ConfirmExitDialog
**
***********************************************************************************/

class ConfirmExitDialog : public Dialog
{
public:
	ConfirmExitDialog(PrefsCollectionUI::integerpref pref);
	virtual ~ConfirmExitDialog() {}

	virtual OP_STATUS			Init(Str::LocaleString title, Str::LocaleString message, DesktopWindow* parent_window);

	///--------------------- Dialog ---------------------///
	virtual Type				GetType()				{return DIALOG_TYPE_CONFIRM_EXIT;}
	virtual const char*			GetWindowName()			{return "Confirm Exit Dialog";}
	virtual BOOL				GetProtectAgainstDoubleClick()	{return FALSE;}

	virtual DialogType			GetDialogType();
	virtual DialogImage			GetDialogImageByEnum()	{return IMAGE_QUESTION;}
	virtual BOOL				GetDoNotShowAgain()		{return TRUE;}

	/*
	 * Turning around the order of the actions. By default, the Cancel button has the YNCancel action
	 * on it in dialogs of DialogType::TYPE_YES_NO_CANCEL, and the 'No'-button has the actual Cancel
	 * action. Hitting ESC would though still trigger OnCancel, and therefore behave like pressing the
	 * 'No'-Button (see bug DSK-270495).
	 * Therefore we're turning around the actions, to the 'No'-Button triggers OnYNCancel, and the
	 * Cancel-Button triggers OnCancel().
	 */
	virtual OpInputAction*	GetYNCCancelAction() { return Dialog::GetCancelAction(); }
	virtual OpInputAction*	GetCancelAction() { return Dialog::GetYNCCancelAction(); }

	virtual Str::LocaleString	GetOkTextID()				{ return Str::D_SYNC_SHUTDOWN_CANCEL; }
	virtual Str::LocaleString	GetCancelTextID();
	virtual Str::LocaleString	GetDoNotShowAgainTextID()	{ return Str::D_REMEMBER_EXIT_CHOICE; }

	virtual void				OnActivate(BOOL activate, BOOL first_time);
	virtual void				OnInit();	
	virtual UINT32				OnOk();
	virtual void				OnYNCCancel();
	virtual void				OnCancel();

	///--------------------- OpTimerListener ---------------------///
	virtual void		OnTimeOut(OpTimer* timer);

private:
	void UpdateTime(INT32 seconds);

private:
	static const INT32				s_timer_secs = 20; // amount of seconds to count down before exiting automatically

	PrefsCollectionUI::integerpref	m_pref;			// the pref writen if "remember this choice" is checked
	OpTimer							m_timer;		// when this timer times out, the dialog is auto-closed
	INT32							m_secs_counter;	// current seconds left

	OpString						m_title;
	OpString						m_message;
};

#endif // __CONFIRM_EXIT_DIALOG_H__
