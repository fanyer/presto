/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"
#include "adjunct/quick/dialogs/MidClickDialog.h"
#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/data_types/OpenURLSetting.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/doc/frm_doc.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#ifdef _X11_
# include "modules/prefs/prefsmanager/collections/pc_unix.h"
# include "platforms/unix/product/x11quick/panningmanager.h"
#endif

//#define OPEN_ON_MOUSE_UP

MidClickDialog* MidClickDialog::m_dialog = 0;

static BOOL m_open_on_release = FALSE;
MidClickManager::PanningPageListener MidClickManager::panning_listener;

# ifdef WIC_MIDCLICK_SUPPORT
/* static */ void
MidClickManager::OnMidClick(OpWindowCommander *commander, OpDocumentContext &context, OpPage* page, BOOL mousedown, ShiftKeyState modifiers, BOOL is_gadget)
{
	panning_listener.ListenPage(page);

	if (mousedown)
	{
		m_open_on_release = FALSE;

		if( modifiers == SHIFTKEY_SHIFT )
		{
			MidClickDialog::Create(g_application->GetActiveDesktopWindow());
		}
		else if( !modifiers )
		{
			BOOL stop = MidClickDialog::FirstTime(g_application->GetActiveDesktopWindow());
			if( stop )
			{
				return;
			}

			// 5=Do not open link. Test ExtendedCenterMouseButtonAction for what to do.
			if (g_pcui->GetIntegerPref(PrefsCollectionUI::CenterMouseButtonAction) != 5 && context.HasLink())
			{
				OpenCurrentClickedURL(commander, context, FALSE, is_gadget);
				m_open_on_release = FALSE;
				return;
			}

#if defined(_X11_)
			//
			// 0=Start panning
			// 1=Treat content on mouse selection as url and open it in the current page
			//
			int action = g_pcui->GetIntegerPref(PrefsCollectionUI::ExtendedCenterMouseButtonAction);
			if(action == 0)
			{
				PanningManager::Start(commander, page);
			}
			else if( action == 1 )
			{
				OpString text;
				g_desktop_clipboard_manager->GetText(text, true);
				if( text.Length() > 0 )
				{
					if (WindowCommanderProxy::GetPreviewMode(commander) ||
						WindowCommanderProxy::GetType(commander) == WIN_TYPE_BRAND_VIEW)
					{
						g_application->OpenURL( text, MAYBE, YES, MAYBE );
					}
					else
					{
						g_application->OpenURL( text, MAYBE, MAYBE, MAYBE );
					}
				}
			}
#elif defined(MSWIN)
			// FIXME julienp
extern void DocWin_HandleIntelliMouseWheelDown(OpWindowCommander* commander, OpView* view);
			DocWin_HandleIntelliMouseWheelDown(commander, context.GetView());
#endif // MSWIN

		}
	}
	else // mouseup
	{
		if( m_open_on_release )
		{
			m_open_on_release = FALSE;
			OpenCurrentClickedURL(commander, context, FALSE, is_gadget);
		}
	}
}

void MidClickManager::OpenCurrentClickedURL(OpWindowCommander *commander, OpDocumentContext &context, BOOL is_gadget, BOOL test_only)
{
	if (IsSentMail(context, test_only))
		return SentMail(context);

	// Object will be deleted later in callback handling
	MidClickUrlOpener* url_opener = OP_NEW(MidClickUrlOpener, ());
	if (url_opener)
	{
		if (OpStatus::IsError(url_opener->OpenUrl(commander, context, is_gadget, test_only)))
			OP_DELETE(url_opener);
	}
}

void MidClickManager::StopPanning()
{
#ifdef MSWIN
	extern void EndPanning();
	EndPanning();
#endif
}

bool MidClickManager::IsSentMail(OpDocumentContext& context, BOOL test_only)
{
	int action = g_pcui->GetIntegerPref(PrefsCollectionUI::CenterMouseButtonAction);
	return context.HasLink() && context.HasMailtoLink() &&  action >= 0 && action <= 4 && !test_only;
}

void MidClickManager::SentMail(OpDocumentContext& context)
{
	OpString8 raw_mailto;
	int action = g_pcui->GetIntegerPref(PrefsCollectionUI::CenterMouseButtonAction);
	MailTo mailto;
	RETURN_VOID_IF_ERROR(context.GetLinkData(OpDocumentContext::AddressForUIEscaped, &raw_mailto));
	MailCompose::ComposeMail(mailto, FALSE, action==2 || action ==4, action ==3 || action == 4);
}

OP_STATUS MidClickUrlOpener::OpenUrl(OpWindowCommander * commander, OpDocumentContext& context, BOOL is_gadget, BOOL test_only)
{
	int action = g_pcui->GetIntegerPref(PrefsCollectionUI::CenterMouseButtonAction);
	if (!context.HasLink() || action < 0 || action > 4 || test_only)
		return OpStatus::ERR;

	RETURN_IF_ERROR(context.GetLinkData(OpDocumentContext::AddressNotForUI, &m_setting.m_address));

	if (commander)
	{
		m_setting.m_has_src_settings = TRUE;
		m_setting.m_src_css_mode     = commander->GetCssMode();
		m_setting.m_src_scale        = commander->GetScale();
		m_setting.m_src_encoding     = commander->GetEncoding();
		m_setting.m_src_image_mode   = commander->GetImageMode();
		m_setting.m_src_commander	= commander;
	}

#ifdef GADGET_SUPPORT
	if (is_gadget && action != 0 &&
		uni_strncmp(UNI_L("file:"), m_setting.m_address.CStr(), 5) == 0)
	{
		//  If this command came from a widget, and the command
		//  is to open a new page or window, and it's opening
		//  a "file:", then we ignore this command.
		return OpStatus::ERR;
	}
#endif // GADGET_SUPPORT

	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled))
		m_setting.m_ref_url = WindowCommanderProxy::GetActiveFrameDocumentURL(commander);

	//
	// Open the link under the mouse (if any) as specified by
	// CenterMouseButtonAction.
	//
	// Behavior in 7.11 final
	// 0=Open link
	// 1=Open link in new page
	// 2=Open link in new page in background
	// 3=Open link in new window
	// 4=Open link in background window
	//
	m_setting.m_new_window    = action == 3 || action == 4 ? YES : NO;
	m_setting.m_new_page      = action == 1 || action == 2 ? YES : NO;
	m_setting.m_in_background = action == 2 || action == 4 ? YES : NO;
	m_setting.m_is_privacy_mode = commander && commander->GetPrivacyMode();

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_OPEN_URL_AFTER_MIDDLE_CLICK, (MH_PARAM_1)this));
	g_main_message_handler->PostMessage(MSG_OPEN_URL_AFTER_MIDDLE_CLICK, (MH_PARAM_1)this, 0);

	return OpStatus::OK;
}

void MidClickUrlOpener::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_OPEN_URL_AFTER_MIDDLE_CLICK)
	{
		OP_ASSERT(par1 == (MH_PARAM_1) this);
		g_main_message_handler->UnsetCallBack(this, MSG_OPEN_URL_AFTER_MIDDLE_CLICK);
		g_application->OpenURL(m_setting);
		OP_DELETE(this);
	}
}

# endif // WIC_MIDCLICK_SUPPORT

MidClickDialog::~MidClickDialog()
{
	m_dialog = 0;
}

void MidClickDialog::OnInit()
{
#if defined(_UNIX_DESKTOP_)
	BOOL panning = g_pcui->GetIntegerPref(PrefsCollectionUI::ExtendedCenterMouseButtonAction) == 0;

	SetWidgetValue("radio_clipboard", !panning );
	SetWidgetValue("radio_panning", panning );
	SetWidgetValue("check_horizontal", g_pcunix->GetIntegerPref(PrefsCollectionUnix::PanningMode)==1);

	SetWidgetEnabled("check_horizontal", GetWidgetValue("radio_clipboard") ? FALSE : TRUE);
#endif

	int value =	g_pcui->GetIntegerPref(PrefsCollectionUI::CenterMouseButtonAction);

	SetWidgetValue("radio_no_open", value < 0 ||value > 4 );
	SetWidgetValue("radio_open", value == 0 );
	SetWidgetValue("radio_new_page", value == 1 );
	SetWidgetValue("radio_background_page", value == 2 );
	SetWidgetValue("radio_new_window", value == 3 );
	SetWidgetValue("radio_background_window", value == 4 );
}

void MidClickDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("radio_clipboard"))
	{
		SetWidgetEnabled("check_horizontal", GetWidgetValue("radio_clipboard") ? FALSE : TRUE);
	}
}


void MidClickDialog::OnInitVisibility()
{
#if defined(MSWIN) || defined(_MACINTOSH_)
	ShowWidget("label_action", FALSE);
	ShowWidget("radio_clipboard", FALSE);
	ShowWidget("radio_panning", FALSE);
	ShowWidget("check_horizontal", FALSE);
#endif
}


UINT32 MidClickDialog::OnOk()
{
	OP_STATUS err;
#if defined(_UNIX_DESKTOP_)
	TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ExtendedCenterMouseButtonAction, GetWidgetValue("radio_panning") ? 0 : 1));
	TRAP(err, g_pcunix->WriteIntegerL(PrefsCollectionUnix::PanningMode, GetWidgetValue("check_horizontal") ? 1 : 0));
#endif

	OP_MEMORY_VAR int value = 0;
	if( GetWidgetValue("radio_no_open") )
		value = 5;
	else if( GetWidgetValue("radio_open") )
		value = 0;
	else if( GetWidgetValue("radio_new_page") )
		value = 1;
	else if( GetWidgetValue("radio_background_page") )
		value = 2;
	else if( GetWidgetValue("radio_new_window") )
		value = 3;
	else if( GetWidgetValue("radio_background_window") )
		value = 4;

	TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::CenterMouseButtonAction, value));

	return 1;
}


void MidClickDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;

	switch (index)
	{
		case 0:
			action = GetOkAction();
			text.Set(GetOkText());
			is_default = TRUE;
			name.Set(WIDGET_NAME_BUTTON_OK);
			break;
		case 1:
			action = GetCancelAction();
			text.Set(GetCancelText());
			name.Set(WIDGET_NAME_BUTTON_CANCEL);
			break;
		case 2:
		{
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_APPLY));
			g_languageManager->GetString(Str::DI_IDM_APPLY,text);
			name.Set(WIDGET_NAME_BUTTON_APPLY);
			break;
		}
	}
}


BOOL MidClickDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_APPLY:
		{
			OnOk();
			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}


// static
void MidClickDialog::Create(DesktopWindow* parent_window)
{
	if( KioskManager::GetInstance()->GetNoChangeButtons() )
	{
		return;
	}

	if( !m_dialog && !g_application->IsEmBrowser())
	{
		m_dialog = OP_NEW(MidClickDialog, ());
		if (m_dialog)
		{
			if (OpStatus::IsError(m_dialog->Init(parent_window)))
				m_dialog = NULL;
		}
	}
}


// static
BOOL MidClickDialog::FirstTime(DesktopWindow* parent_window )
{
	if (g_application->IsEmBrowser())
	{
		return TRUE;
	}

	if( KioskManager::GetInstance()->GetNoChangeButtons() )
	{
		return FALSE; // Never show dialog, but allow action if set
	}


// Commented out the following since the dialog can be more annoying than useful
#if 0
	if(!g_pcui->GetIntegerPref(PrefsCollectionUI::HasShownCenterClickInfo))
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::HasShownCenterClickInfo, true));
		prefsManager->CommitL();

		// Using mouse in dialogs can reset the URL
		URL url = windowManager->GetCurrentClickedURL();

		OpString title, msg;
		g_languageManager->GetString(Str::D_MIDCLICKDIALOG_CENTER_MOUSE_CONFIG, title);
		g_languageManager->GetString(Str::DI_MIDCLICK_FIRST_TIME_HELP,msg);
		int result = g_application->ShowDialog(parent_window, msg.CStr(), title.CStr(), Dialog::TYPE_YES_NO, Dialog::IMAGE_INFO);
		if( result == DIALOG_RESULT_YES )
		{
			Create( parent_window, TRUE );
		}

		windowManager->SetCurrentClickedURL(url);
	}
#endif // 0

	return FALSE;
}
