/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/data_types/OpenURLSetting.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpPage.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

// name is what is retured by GetWindowName()
OP_STATUS SimpleDialog::Init(const OpStringC8& specific_name, const OpStringC& title, const OpStringC& message, DesktopWindow* parent_window, DialogType dialog_type, DialogImage dialog_image, BOOL is_blocking, INT32* result, BOOL* do_not_show_again, const char* help_anchor, INT32 default_button_index, OpBrowserView *parent_browser_view)
{
	m_is_blocking = is_blocking;
	m_title = title.CStr();
	m_message = message.CStr();
	m_dialog_type = dialog_type;
	m_dialog_image = dialog_image;
	m_result = result;
	m_do_not_show_again = do_not_show_again;
	m_help_anchor = help_anchor;
	m_default_button_index = default_button_index;
	m_specific_dialog_name.Set(specific_name);

	if (m_result) *m_result = DIALOG_RESULT_CANCEL;

	m_inited = TRUE;
    return Dialog::Init(parent_window, 0, FALSE, parent_browser_view);	 // NO CODE AFTER THIS!!
}

/*static*/ OP_STATUS
SimpleDialog::ShowDialog(const OpStringC8& specific_name,DesktopWindow* parent_window, const uni_char* message, const uni_char* title, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, DialogListener * dialog_listener, BOOL* do_not_show_again, const char* help_anchor, INT32 default_button)
{
	SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
	if (dialog)
	{
		if (dialog_listener)
			dialog->SetDialogListener(dialog_listener);

		return dialog->Init(specific_name, title, message, parent_window ? parent_window : g_application->GetActiveDesktopWindow(), dialog_type, dialog_image, FALSE, NULL, do_not_show_again, help_anchor,default_button);
	}
	return OpStatus::ERR_NO_MEMORY;
}

/*static*/ OP_STATUS
SimpleDialog::ShowDialog(const OpStringC8& specific_name,DesktopWindow* parent_window, Str::LocaleString message_id, Str::LocaleString title_id, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, DialogListener * dialog_listener, BOOL* do_not_show_again, const char* help_anchor)
{
	OpString message;
	OpString title;

	g_languageManager->GetString(message_id, message);
	g_languageManager->GetString(title_id, title);

	return ShowDialog(specific_name, parent_window, message.CStr(), title.CStr(), dialog_type, dialog_image, dialog_listener, do_not_show_again, help_anchor);
}

/*static*/ OP_STATUS
SimpleDialog::ShowDialogFormatted(const OpStringC8& specific_name, DesktopWindow* parent_window, const uni_char *message, const uni_char* title, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, DialogListener * dialog_listener, BOOL* do_not_show_again, ... )
{
	va_list		ap;
	const int buf_len = 1024; /* XXX Should handle any length */

	OpString buf;
	if (!buf.Reserve(buf_len))
		return 0;

	va_start(ap, do_not_show_again);
	uni_vsnprintf(buf.CStr(), buf_len, message, ap);
	va_end(ap);

	return ShowDialog(specific_name, parent_window, buf.CStr(), title, dialog_type, dialog_image, dialog_listener, do_not_show_again);
}

// to be deprecated
INT32 SimpleDialog::ShowDialog(const OpStringC8& specific_name, DesktopWindow* parent_window, Str::LocaleString message_id, Str::LocaleString title_id, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, BOOL* do_not_show_again, const char* help_anchor)
{
	OpString message;
	OpString title;

	g_languageManager->GetString(message_id, message);
	g_languageManager->GetString(title_id, title);

	return ShowDialog(specific_name, parent_window, message.CStr(), title.CStr(), dialog_type, dialog_image, do_not_show_again, help_anchor);
}

// to be deprecated
INT32 SimpleDialog::ShowDialog(const OpStringC8& specific_name, DesktopWindow* parent_window, const uni_char* message, const uni_char* title, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, BOOL* do_not_show_again, const char* help_anchor,INT32 default_button)
{
	BOOL is_blocking = dialog_type != Dialog::TYPE_OK;

	INT32 result = 0;

	SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
	if (dialog)
		dialog->Init(specific_name, title, message, parent_window ? parent_window : g_application->GetActiveDesktopWindow(), dialog_type, dialog_image, is_blocking, is_blocking ? &result : NULL, do_not_show_again, help_anchor,default_button);

	return result;
}

// to be deprecated
INT32 SimpleDialog::ShowDialogFormatted(const OpStringC8& specific_name, DesktopWindow* parent_window, const uni_char *message, const uni_char* title, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, BOOL* do_not_show_again, ... )
{
	va_list		ap;
	const int buf_len = 1024; /* XXX Should handle any length */

	OpString buf;
	if (!buf.Reserve(buf_len))
		return 0;

	va_start(ap, do_not_show_again);
	uni_vsnprintf(buf.CStr(), buf_len, message, ap);
	va_end(ap);

	return ShowDialog(specific_name, parent_window, buf.CStr(), title, dialog_type, dialog_image, do_not_show_again);
}

/*virtual*/
const char*	SimpleDialog::GetSpecificWindowName()
{
	if (m_specific_dialog_name.HasContent())
	{
		return m_specific_dialog_name.CStr();
	}
	else
	{
		OP_ASSERT(!"Should add a dialog title ");
		return "Simple Dialog";
	}
}


/*virtual*/ Str::LocaleString
SimpleDialog::GetOkTextID()
{
	if (m_ok_text_id == 0)
	{
		return Dialog::GetOkTextID();
	}
	else
	{
		return Str::LocaleString(m_ok_text_id);
	}
}

OP_STATUS
SimpleDialog::SetOkTextID(Str::LocaleString ok_text_id)
{
	OP_ASSERT(!m_inited || !"Can't call this function on an initialized dialog");

	m_ok_text_id = ok_text_id;
	return OpStatus::OK;
}

/*virtual*/ Str::LocaleString
SimpleDialog::GetCancelTextID()
{
	if (m_cancel_text_id == 0)
	{
		return Dialog::GetCancelTextID();
	}
	else
	{
		return Str::LocaleString(m_cancel_text_id);
	}
}

OP_STATUS
SimpleDialog::SetCancelTextID(Str::LocaleString cancel_text_id)
{
	OP_ASSERT(!m_inited || !"Can't call this function on an initialized dialog");

	m_cancel_text_id = cancel_text_id;
	return OpStatus::OK;
}


/*virtual*/ BOOL
SimpleDialog::GetProtectAgainstDoubleClick()
{
	switch(m_protection_type)
	{
	case ForceOn:
		return TRUE;
	case ForceOff:
		return FALSE;
	default:
		return Dialog::GetProtectAgainstDoubleClick();
	}
}

/*virtual*/ void
SimpleDialog::SetProtectAgainstDoubleClick(SimpleDialog::DoubleClickProtectionType type)
{
	m_protection_type = type;
}

void SimpleDialog::OnReset()
{
	OpMultilineEdit* edit = (OpMultilineEdit*) GetWidgetByName("Simple_message");

	if (edit)
	{
		const OpRect original_rect = edit->GetRect();
		INT32 left, top, right, bottom;
		edit->GetPadding(&left, &top, &right, &bottom);
		int width = MAX(original_rect.width, edit->GetMaxBlockWidth() + left + right);
		int height = MAX(original_rect.height, edit->GetContentHeight() + top + bottom);

		DesktopWindow *parent_desktop_win = GetParentDesktopWindow();
		if (parent_desktop_win && GetOverlayed())
		{
			// Limit the size of the editfield to a bit less than the parent window. Otherwise we might get
			// a dialog that is so large that the user can't see the dialog title or buttons.
			INT32 max_w = parent_desktop_win->GetInnerWidth() - parent_desktop_win->GetInnerWidth() / 5;
			INT32 max_h = parent_desktop_win->GetInnerHeight() - parent_desktop_win->GetInnerHeight() / 5;
			width = MIN(width, max_w);
			height = MIN(height, max_h);
		}
		edit->SetSize(width, height);
	}
}

void SimpleDialog::OnInit()
{
	SetTitle(m_title);

	OpMultilineEdit* edit = (OpMultilineEdit*) GetWidgetByName("Simple_message");

	if (edit)
	{
		edit->SetFlatMode(); // useful to be able to select text from error boxes etc.
		edit->SetScrollbarStatus(SCROLLBAR_STATUS_AUTO, SCROLLBAR_STATUS_AUTO);
		edit->SetText(m_message);
	}

	// Set default button if specified
	if(m_default_button_index != 0)
	{
		SetDefaultPushButton(m_default_button_index);
	}
}

UINT32 SimpleDialog::OnOk()
{
	if (m_result)
	{
		*m_result = DIALOG_RESULT_OK;
	}
	return 0;
}

void SimpleDialog::OnClose(BOOL user_initiated)
{
	if (m_do_not_show_again)
	{
		*m_do_not_show_again = IsDoNotShowDialogAgainChecked();
	}

	Dialog::OnClose(user_initiated);
}

BOOL SimpleDialog::OnInputAction(OpInputAction* action)
{
	if( m_dialog_type == TYPE_YES_NO_CANCEL )
	{
		// The YNC box has no help button and we use the Help button as cancel
		// Mapping: OK -> Yes, Cancel -> No (note use of special result code below),
		// Help -> Cancel

		BOOL s = Dialog::OnInputAction(action);
		if( action->GetAction() == OpInputAction::ACTION_OK )
		{
			if (m_result) *m_result = DIALOG_RESULT_YES;
		}
		else if( action->GetAction() == OpInputAction::ACTION_CANCEL )
		{
			if (m_result) *m_result = DIALOG_YNC_RESULT_NO;
		}
		return s;
	}
	else
	{
		return Dialog::OnInputAction(action);
	}
}

OP_STATUS SkinVersionError::Init(DesktopWindow* parent_window)
{
	OpString title;
	OpString message;
	g_languageManager->GetString(Str::D_SKIN_FAILURE_TITLE, title);
	g_languageManager->GetString(Str::S_SKIN_FAILURE_MESSAGE, message);
	return SimpleDialog::Init(WINDOW_NAME_SKIN_VERSION, title, message, parent_window, TYPE_OK);
}

OP_STATUS SetupVersionError::Init(DesktopWindow* parent_window)
{
	OpString title;
	OpString message;
	g_languageManager->GetString(Str::D_SETUP_FAILURE_TITLE, title);
	g_languageManager->GetString(Str::D_SETUP_FAILURE_MESSAGE, message);
	return SimpleDialog::Init(WINDOW_NAME_SETUP_VERSION, title, message, parent_window,TYPE_OK);
}

AskAboutFormRedirectDialog::~AskAboutFormRedirectDialog()
{
	if (callback)
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_CANCEL);
}

OP_STATUS AskAboutFormRedirectDialog::Init(DesktopWindow* parent_window, const uni_char * source_url, const uni_char * target_url)
{
	OpString title, message, format, caption;
	RETURN_IF_ERROR(format.Set("%s\n\n%s\n\n%s"));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_MSG_ASK_POST_DIRECT, message));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_WARN_FORM_REDIRECT, title));
	RETURN_IF_ERROR(caption.AppendFormat(format.CStr(), source_url, target_url, message.CStr()));
	if(message.IsEmpty())
		return OpStatus::ERR;

	return SimpleDialog::Init(WINDOW_NAME_ASK_ABOUT_FORM_REDIRECT, title, caption, parent_window, TYPE_YES_NO_CANCEL, IMAGE_WARNING, FALSE);
}

UINT32 AskAboutFormRedirectDialog::OnOk()
{
	if (callback)
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_YES);
	callback = NULL;
	return 0;
}

void AskAboutFormRedirectDialog::OnCancel()
{
	if (callback)
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_NO);
	callback = NULL;
}

FormRequestTimeoutDialog::~FormRequestTimeoutDialog()
{
	if (callback)
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_CANCEL);
}

OP_STATUS FormRequestTimeoutDialog::Init(DesktopWindow* parent_window, const uni_char * url)
{
	OpString title, message, format, caption;
	RETURN_IF_ERROR(format.Set("%s\n\n%s"));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_ERR_HTTP_100CONTINUE_ERROR, message));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_WARN_FORM_TIMEOUT, title));
	RETURN_IF_ERROR(caption.AppendFormat(format.CStr(), url, message.CStr()));
	if(message.IsEmpty())
		return OpStatus::ERR;

	return SimpleDialog::Init(WINDOW_NAME_FORM_REQUEST_TIMEOUT, title, caption, parent_window, TYPE_OK, IMAGE_WARNING, FALSE);
}

UINT32 FormRequestTimeoutDialog::OnOk()
{
	if (callback)
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_YES);
	callback = NULL;
	return 0;
}

void FormRequestTimeoutDialog::OnCancel()
{
	if (callback)
		callback->OnDialogReply(OpDocumentListener::DialogCallback::REPLY_NO);
	callback = NULL;
}

#ifdef QUICK_USE_DEFAULT_BROWSER_DIALOG
OP_STATUS DefaultBrowserDialog::Init(DesktopWindow * parent_window)
{
	OpString title, caption;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDM_GRP_DFLTBROWSER, title));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDM_LBL_DFLTBROWSER, caption));

	return SimpleDialog::Init(WINDOW_NAME_DEFAULT_BROWSER, title, caption, parent_window, TYPE_YES_NO, IMAGE_QUESTION, FALSE, NULL, &m_do_not_show_again);
}

UINT32 DefaultBrowserDialog::OnOk()
{
	g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START_CONTINUE, 0, 0);
	(static_cast<DesktopOpSystemInfo*>(g_op_system_info))->SetAsDefaultBrowser();
	TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowDefaultBrowserDialog, !m_do_not_show_again));
	return 0;
}

void DefaultBrowserDialog::OnCancel()
{
	g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START_CONTINUE, 0, 0);
	TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowDefaultBrowserDialog, !m_do_not_show_again));
}

# endif // QUICK_USE_DEFAULT_BROWSER_DIALOG

