/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "adjunct/quick/controller/SimpleDialogController.h"

#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/selftest/optestsuite.h"

SimpleDialogController::SimpleDialogController(DialogType dialog_type, DialogImage dialog_image, OpStringC8 specific_name):
								m_ok_button(NULL),
								m_cancel_button(NULL),
								m_help_button(NULL),
								m_no_button(NULL),
								m_dialog_image(dialog_image),
								m_dialog_type(dialog_type),
								m_result(NULL),
								m_help_anchor(NULL),
								m_specific_dialog_name(specific_name),
								m_str_message(Str::NOT_A_STRING),
								m_str_title(Str::NOT_A_STRING),
								m_do_not_show_again(NULL),
								m_dont_show_again_accessor(NULL),
								m_hook_called(false)
{
}

SimpleDialogController::SimpleDialogController(DialogType dialog_type, DialogImage dialog_image, OpStringC8 specific_name, Str::LocaleString message, Str::LocaleString title): 
								m_ok_button(NULL),
								m_cancel_button(NULL),
								m_help_button(NULL),
								m_no_button(NULL),
								m_dialog_image(dialog_image),
								m_dialog_type(dialog_type),
								m_result(NULL),
								m_help_anchor(NULL),
								m_specific_dialog_name(specific_name),
								m_str_message(message),
								m_str_title(title),
								m_do_not_show_again(NULL),
								m_dont_show_again_accessor(NULL),
								m_hook_called(false)
{
}

SimpleDialogController::SimpleDialogController(DialogType dialog_type, DialogImage dialog_image, OpStringC8 specific_name, OpStringC message, OpStringC title):
								m_ok_button(NULL),
								m_cancel_button(NULL),
								m_help_button(NULL),
								m_no_button(NULL),
								m_dialog_image(dialog_image),
								m_dialog_type(dialog_type),
								m_result(NULL),
								m_help_anchor(NULL),
								m_specific_dialog_name(specific_name),
								m_str_message(Str::NOT_A_STRING),
								m_str_title(Str::NOT_A_STRING),
								m_title(title),
								m_message(message),
								m_do_not_show_again(NULL),
								m_dont_show_again_accessor(NULL),
								m_hook_called(false)
{
}

SimpleDialogController::~SimpleDialogController()
{
	OP_DELETE(m_dont_show_again_accessor);
}

void SimpleDialogController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Simple Dialog"));

#ifdef SELFTEST
	// This controller is used to ask about the result of a manual selftest.
	// Make it modeless so that it doesn't prevent the window under test from
	// receiving input.
	if (g_selftest.suite->DoRun())
		m_dialog->SetStyle(OpWindow::STYLE_MODELESS_DIALOG);
#endif // SELFTEST

	LEAVE_IF_ERROR(m_dialog->SetName(m_specific_dialog_name));

	if (m_message.IsEmpty() && m_str_message != Str::NOT_A_STRING)
	{
		ANCHORD(OpString, message);
		g_languageManager->GetStringL(m_str_message, message);
		LEAVE_IF_ERROR(SetWidgetText<QuickMultilineLabel>("info_label", message));
	}
	else
		LEAVE_IF_ERROR(SetWidgetText<QuickMultilineLabel>("info_label", m_message));

	if (m_title.IsEmpty() && m_str_title != Str::NOT_A_STRING)
	{
		ANCHORD(OpString, title);
		g_languageManager->GetStringL(m_str_title, title);
		LEAVE_IF_ERROR(m_dialog->SetTitle(title));
	}
	else
		LEAVE_IF_ERROR(m_dialog->SetTitle(m_title));
	
	m_ok_button = m_dialog->GetWidgetCollection()->GetL<QuickButton>(WIDGET_NAME_BUTTON_OK);
	m_cancel_button = m_dialog->GetWidgetCollection()->GetL<QuickButton>(WIDGET_NAME_BUTTON_CANCEL);
	m_help_button = m_dialog->GetWidgetCollection()->GetL<QuickButton>(WIDGET_NAME_BUTTON_HELP);
	m_no_button = m_dialog->GetWidgetCollection()->GetL<QuickButton>(WIDGET_NAME_BUTTON_NO);
	QuickCheckBox* do_not_show_again_checkbox = m_dialog->GetWidgetCollection()->GetL<QuickCheckBox>(WIDGET_NAME_CHECKBOX_DEFAULT);

	if (!m_dont_show_again_accessor && !m_do_not_show_again)
	{
		do_not_show_again_checkbox->Hide();
	}
	else
	{
		if (m_dont_show_again_accessor)
		{
			//binder takes ownership in case successful connection, it also deletes argument on failure 
			OP_STATUS stat = GetBinder()->Connect(*do_not_show_again_checkbox, m_dont_show_again_accessor, m_selected, m_deselected);
			m_dont_show_again_accessor = NULL;
			LEAVE_IF_ERROR(stat);
		}
		else 
		{
			LEAVE_IF_ERROR(GetBinder()->Connect(*do_not_show_again_checkbox, m_again_property));
			m_again_property.Set(*m_do_not_show_again);
		}
	}
	switch(m_dialog_type)
	{
		case TYPE_OK:
		{
			LEAVE_IF_ERROR(m_ok_button->SetText(Str::DI_ID_OK));
			m_cancel_button->Hide();
			break;
		}

		case TYPE_CLOSE:
		{
			LEAVE_IF_ERROR(m_ok_button->SetText(Str::DI_IDBTN_CLOSE));
			m_cancel_button->Hide();
			break;
		}

		case TYPE_OK_CANCEL:
		{
			LEAVE_IF_ERROR(m_ok_button->SetText(Str::DI_ID_OK));
			break;
		}

		case TYPE_YES_NO:
		{
			LEAVE_IF_ERROR(m_cancel_button->SetText(Str::DI_IDNO));
			break;
		}
	}

	if (m_dialog_type != TYPE_YES_NO_CANCEL)
	{
		// "No" button used only for "yes no cancel" dialogs
		m_no_button->Hide();
	}

	if (!m_help_anchor)
		m_help_button->Hide();
	else
		m_help_button->GetAction()->SetActionDataString(m_help_anchor);

	LEAVE_IF_ERROR(SetIcon(m_dialog_image));

	if(m_result)
	{
		m_dialog->SetStyle(OpWindow::STYLE_BLOCKING_DIALOG);
	}
}

void SimpleDialogController::SetDoNotShowAgain(bool& do_not_show_again)
{
	m_do_not_show_again = &do_not_show_again;
}

OP_STATUS SimpleDialogController::SetIcon(DialogImage image)
{
	QuickIcon* icon = m_dialog->GetWidgetCollection()->Get<QuickIcon>("dialog_icon");
	RETURN_VALUE_IF_NULL(icon,OpStatus::ERR);
	m_dialog_image = image;

	const char* image_name = GetDialogImage();
	if (image_name != NULL)
	{
		icon->SetImage(image_name);
		icon->Show();
	}
	else
	{
		icon->Hide();
	}

	return OpStatus::OK;
}

const char* SimpleDialogController::GetDialogImage()
{
	switch (m_dialog_image)
	{
		case IMAGE_WARNING: return "Dialog Warning"; break;
		case IMAGE_QUESTION: return "Dialog Question"; break;
		case IMAGE_ERROR: return "Dialog Error"; break;
		case IMAGE_INFO: return "Dialog Info"; break;
	}

	return NULL;
}

BOOL SimpleDialogController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
		case OpInputAction::ACTION_NO:
		case OpInputAction::ACTION_CANCEL:
			return TRUE;
	}

	return FALSE;

}

OP_STATUS SimpleDialogController::HandleAction(OpInputAction* action)
{
	if (m_dialog_type!= TYPE_OK && m_dialog_type != TYPE_CLOSE)
	{
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_NO:
				OnNo();
				break;

			case OpInputAction::ACTION_OK:
				OnOk();
				break;

			case OpInputAction::ACTION_CANCEL:
				OnCancel();
				break;
		}
	}
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_NO:
		case OpInputAction::ACTION_OK:
		case OpInputAction::ACTION_CANCEL:
			m_hook_called = true;
			if (m_dialog && m_dialog->GetDesktopWindow() && !m_dialog->GetDesktopWindow()->IsClosing())
				m_dialog->GetDesktopWindow()->Close();
			return TRUE;
	}

	return DialogContext::HandleAction(action);
}

void SimpleDialogController::OnOk()
{
	if (m_result)
		*m_result = DIALOG_RESULT_OK;
	if (m_do_not_show_again)
		*m_do_not_show_again = m_again_property.Get();
}

void SimpleDialogController::OnNo()
{ 
	if (m_result) 
		*m_result = DIALOG_RESULT_NO;
	if (m_do_not_show_again)
		*m_do_not_show_again = m_again_property.Get();
}

void SimpleDialogController::OnCancel()
{
	if (m_result)
		*m_result = DIALOG_RESULT_CANCEL;
}

void SimpleDialogController::OnUiClosing()
{
	if (!m_hook_called && m_dialog_type!= TYPE_OK && m_dialog_type != TYPE_CLOSE)
		OnCancel();

	DialogContext::OnUiClosing();
}

OP_STATUS SimpleDialogController::SetMessage(const OpStringC& message) 
{
	return SetWidgetText<QuickMultilineLabel>("info_label",message);
}
