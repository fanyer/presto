/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/controller/WebHandlerPermissionController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickGroupBox.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickRadioButton.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/viewers/viewers.h"
#include "modules/windowcommander/OpWindowCommander.h"


OpVector<WebHandlerPermissionController> WebHandlerPermissionController::m_list;

// static
void WebHandlerPermissionController::Add(OpDocumentListener::WebHandlerCallback* cb, DesktopWindow* parent)
{
	WebHandlerPermissionController* controller = OP_NEW(WebHandlerPermissionController, (cb));
	if (!controller || OpStatus::IsError(m_list.Add(controller)))
	{
		OP_DELETE(controller);
		cb->OnCancel();
		return;
	}

	// On error, ShowDialog() automatically calls OnUiClosing() which triggers OnCancel()
	if (OpStatus::IsError(::ShowDialog(controller, g_global_ui_context, parent)))
		return;
}

// static
void WebHandlerPermissionController::Remove(OpDocumentListener::WebHandlerCallback* cb)
{
	for (UINT32 i=0; i<m_list.GetCount(); i++)
	{
		WebHandlerPermissionController* controller = m_list.Get(i);
		if (controller->m_webhandler_cb == cb)
		{
			controller->m_webhandler_cb = 0;
			controller->CancelDialog();
			break;
		}
	}
}



WebHandlerPermissionController::~WebHandlerPermissionController()
{
	m_list.RemoveByItem(this);
}


void WebHandlerPermissionController::InitL()
{
	// Use overlay dialog so that we can use other tabs while dialog is waiting for input
	QuickOverlayDialog* dialog = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Web Handler Permissions Dialog", dialog));
	dialog->SetDimsPage(true);

	// We can get more than one dialog at at time (deliberately evil sites) so if there
	// is more than one we add a number to help the user understand what is going on
	if (m_list.GetCount() > 1)
	{
		ANCHORD(OpString, title);
		g_languageManager->GetStringL(Str::D_WEB_HANDLER_CHOOSE_APPLICATION, title);
		LEAVE_IF_ERROR(title.AppendFormat(UNI_L(" (%d)"), m_list.GetCount()));
		LEAVE_IF_ERROR(dialog->SetTitle(title));
	}

	m_widgets = m_dialog->GetWidgetCollection();

	ANCHORD(OpString, s);
	ANCHORD(OpString, mime_or_protocol);

	QuickGroupBox* group = m_widgets->GetL<QuickGroupBox>("group_message");
	mime_or_protocol.SetFromUTF8L(m_webhandler_cb->GetMimeTypeOrProtocol());
	LEAVE_IF_ERROR(I18n::Format(s, Str::D_WEB_HANDLER_USAGE_CAPTION, mime_or_protocol));
	LEAVE_IF_ERROR(group->SetText(s));

	m_is_mailto = !mime_or_protocol.CompareI("mailto");

	QuickRadioButton* radiobutton = m_widgets->GetL<QuickRadioButton>("radiobutton_1");
	LEAVE_IF_ERROR(I18n::Format(s, Str::D_WEB_HANDLER_USAGE_CONFIRM, m_webhandler_cb->GetDescription(), m_webhandler_cb->GetTargetHostName()));
	LEAVE_IF_ERROR(radiobutton->SetText(s));

	LEAVE_IF_ERROR(GetBinder()->Connect("radiobutton_1", m_radio_value[0]));
	LEAVE_IF_ERROR(GetBinder()->Connect("radiobutton_2", m_radio_value[1]));
	LEAVE_IF_ERROR(GetBinder()->Connect("radiobutton_3", m_radio_value[2]));
	LEAVE_IF_ERROR(m_radio_value[0].Subscribe(MAKE_DELEGATE(*this, &WebHandlerPermissionController::OnRadioChanged)));
	LEAVE_IF_ERROR(m_radio_value[1].Subscribe(MAKE_DELEGATE(*this, &WebHandlerPermissionController::OnRadioChanged)));
	LEAVE_IF_ERROR(m_radio_value[2].Subscribe(MAKE_DELEGATE(*this, &WebHandlerPermissionController::OnRadioChanged)));

	// oh well, core does not set it, but should
	// If set externally we must check range here
	m_mode = WebHandler;

	m_radio_value[m_mode].Set(true);

	QuickCheckBox* checkbox = m_widgets->GetL<QuickCheckBox>("checkbox_default");
	if (m_webhandler_cb->ShowNeverAskMeAgain())
		LEAVE_IF_ERROR(GetBinder()->Connect(*checkbox, m_dont_ask_again));
	else
		checkbox->Hide();

	if (m_webhandler_cb->IsProtocolHandler())
	{
		ANCHORD(TrustedProtocolData, td);
		int index = g_pcdoc->GetTrustedProtocolInfo(mime_or_protocol, td);
		if (index >= 0)
			m_application_text.Set(td.filename);
	}
	else
	{
		Viewer* viewer = g_viewers->FindViewerByMimeType(mime_or_protocol);
		if (viewer)
			m_application_text.Set(viewer->GetApplicationToOpenWith());
	}

	// Monitor changes in text field so that we can disable out OK button when data is not valid
	LEAVE_IF_ERROR(GetBinder()->Connect("filechooser_application", m_application_text));
	LEAVE_IF_ERROR(m_application_text.Subscribe(MAKE_DELEGATE(*this, &WebHandlerPermissionController::OnTextChanged)));
}

void WebHandlerPermissionController::OnCancel()
{
	if (m_webhandler_cb)
		m_webhandler_cb->OnCancel();
}


void WebHandlerPermissionController::OnOk()
{
	if (!m_webhandler_cb || m_mode == NotDefined)
		return;

	const BOOL dont_ask_again = m_dont_ask_again.Get() ? TRUE : FALSE;

	switch (m_mode)
	{
		case WebHandler:
			m_webhandler_cb->OnOK(dont_ask_again, 0);
		break;
		case DefaultApplication:
			if (m_is_mailto)
			{
				OP_STATUS rc;
				TRAP_AND_RETURN_VOID_IF_ERROR(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, MAILHANDLER_SYSTEM));
			}
			m_webhandler_cb->OnOK(dont_ask_again, UNI_L(""));
		break;
		case CustomApplication:
		{
			if (m_application.IsEmpty())
				m_webhandler_cb->OnCancel();
			else
			{
				if (m_is_mailto)
				{
					OP_STATUS rc;
					TRAP_AND_RETURN_VOID_IF_ERROR(rc, g_pcapp->WriteStringL(PrefsCollectionApp::ExternalMailClient, m_application));
					TRAP_AND_RETURN_VOID_IF_ERROR(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, MAILHANDLER_APPLICATION));
				}
				m_webhandler_cb->OnOK(dont_ask_again, m_application);
			}
		}
	}
}


BOOL WebHandlerPermissionController::DisablesAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OK)
	{
		if (m_mode == CustomApplication)
			return m_application.IsEmpty();
	}

	return OkCancelDialogContext::DisablesAction(action);
}


OP_STATUS WebHandlerPermissionController::HandleAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OK)
	{
		if (m_mode == CustomApplication && m_application.IsEmpty())
			return OpStatus::ERR;
	}
	return OkCancelDialogContext::HandleAction(action);
}


OP_STATUS WebHandlerPermissionController::GetApplication(OpString& application, BOOL validate)
{
	RETURN_IF_ERROR(application.Set(m_application_text.Get()));

	application.Strip();
	if (application.IsEmpty())
		return OpStatus::ERR;

	if (validate)
	{
		// Perhaps test for existing file here. We have to take quotations, full paths
		// and executablity into consideration in that case
	}

	return OpStatus::OK;
}


void WebHandlerPermissionController::OnTextChanged(const OpStringC& text)
{
	if (m_mode == CustomApplication)
	{
		BOOL old_content = m_application.HasContent();
		OpStatus::Ignore(GetApplication(m_application, TRUE));
		if (old_content != m_application.HasContent())
			g_input_manager->UpdateAllInputStates();
	}
}


void WebHandlerPermissionController::OnRadioChanged(bool new_value)
{
	m_mode = NotDefined;
	for (int i=0; i<MaxModeCount; i++)
	{
		if (m_radio_value[i].Get())
		{
			m_mode = static_cast<Mode>(i);
			break;
		}
	}
	if (m_mode != NotDefined)
		g_input_manager->UpdateAllInputStates();
}

