/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef PRINT_PROGRESS_DIALOG_H
#define PRINT_PROGRESS_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"

# if defined _PRINT_SUPPORT_ && defined GENERIC_PRINTING && defined QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG

class PrintProgressDialog : public Dialog
{
public:
	PrintProgressDialog(OpBrowserView* browser_view) : m_label(0), m_browser_view(browser_view) {}
	
	void OnInitVisibility()
	{
		m_label = (OpLabel*) GetWidgetByName("Message_label");
		SetPage(1);
	}

	void GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
	{
		is_enabled = TRUE;
		
		if (index == 0)
		{
			is_default = TRUE;
			action = GetCancelAction();
			text.Set(GetCancelText());
			name.Set(WIDGET_NAME_BUTTON_CANCEL);
		}
	}

	void SetPage( INT32 page )
	{
		if (m_label)
		{
			OpString message;
			TRAPD(rc,g_languageManager->GetStringL(Str::SI_PRINTING_PAGE_TEXT,message));
			message.AppendFormat(UNI_L(" %d"), page);
			m_label->SetLabel( message.CStr(), TRUE );
			m_label->Sync();
		}
	}
	
	void OnCancel()
	{
		m_browser_view->PrintProgressDialogDone(); // Dialog destroyed and no longer valid
		Dialog::OnCancel();
	}

	virtual BOOL			GetModality()			{ return TRUE; }
	virtual BOOL			GetIsBlocking() 		{ return FALSE; }
	virtual Type			GetType()				{ return DIALOG_TYPE_PRINT_PROGRESS; }
	virtual const char*		GetWindowName()			{ return "Print Progress Dialog"; }
	virtual BOOL			HasCenteredButtons() 	{ return TRUE; }
	INT32					GetButtonCount()		{ return 1; };

private:
	OpLabel* m_label;
	OpBrowserView* m_browser_view;
};

#endif // _PRINT_SUPPORT_ && GENERIC_PRINTING && QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG

#endif // PRINT_PROGRESS_DIALOG_H
