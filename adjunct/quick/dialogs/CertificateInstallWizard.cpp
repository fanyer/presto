/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Marius Blomli
*/

/**
 * @file CertificateInstallWizard.cpp
 *
 * Dialog used as the UI for installing certificates, both CAs and client certificates
 */

#include "core/pch.h"

#include "adjunct/quick/dialogs/CertificateInstallWizard.h"

#include "adjunct/quick/dialogs/CertificateDetailsDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpMultiEdit.h"

/***********************************************************************************
**
**	CertificateInstallWizard
**
***********************************************************************************/

CertificateInstallWizard::CertificateInstallWizard(OpSSLListener::SSLCertificateContext* context, OpSSLListener::SSLCertificateOption options) :
	m_context((SSL_Certificate_DisplayContext*)context), // This is a hack to keep the dialogs sort of working while converting to use the wic interface alone
	m_type(SELECT_CERT),
	m_options(options),
	m_domain_name()
{
	if (m_context->GetShowDeleteInstallButton())
	{
		m_type = INSTALL_CERT;
	}
	if (m_context->GetServerName())
		m_domain_name.Set(m_context->GetServerName()->UniName());
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void CertificateInstallWizard::OnInit()
{
	// Title string
	OpString title;
	g_languageManager->GetString(m_context->GetTitle(), title);
    SetTitle(title.CStr());

	// Server name - now part of the message
	/*if (!m_domain_name.IsEmpty())
	{
		OpEdit* field = (OpEdit*)GetWidgetByName("Domain_edit");
		if (field)
		{
			field->SetEllipsis(ELLIPSIS_CENTER);
			field->SetFlatMode();
			field->SetText(m_domain_name.CStr());
		}
	}
	else // don't show the according labels
	{
		ShowWidget("Domain_label", FALSE);
		ShowWidget("Domain_edit", FALSE);
	}*/

	// Main text field
	OpString label, text;
	if (!m_domain_name.IsEmpty())
	{
		label.AppendFormat(UNI_L("<%s>\n\n"), m_domain_name.CStr());
	}
	g_languageManager->GetString(m_context->GetMessage(), text);
	label.Append(text);
	OpMultilineEdit* info_label = (OpMultilineEdit*)GetWidgetByName("Certificate_information_label");
	if(info_label)
	{
		info_label->SetLabelMode();
		info_label->SetText(label.CStr());
	}

	// Certificate listbox
	m_context->SetCurrentCertificateNumber(0);
	OpListBox* cert_listbox = (OpListBox*)GetWidgetByName("Certificate_selector_listbox");
	if(cert_listbox)
	{
		OpString subject_text, issuer_text, certificate_text;
	    BOOL warn, allow;
		m_context->GetCertificateTexts(subject_text, issuer_text, certificate_text, allow, warn, m_context->GetCurrentCertificateNumber());

		if(m_context->IsSingleOnlyCert())
		{
			OpString singlename;
			singlename.Set(subject_text.CStr(), subject_text.FindFirstOf(UNI_L("\r\n")));
			cert_listbox->AddItem(singlename.CStr());
		}
		else
		{
			int i = 0;
			OpString cert_short_name;
			while(m_context->GetCertificateShortName(cert_short_name, i))
			{
				cert_listbox->AddItem(cert_short_name.CStr());
				i++;
			}
		}
		cert_listbox->SelectItem(m_context->GetCurrentCertificateNumber(), TRUE);
	}

	// View button
	if (m_type == INSTALL_CERT) // Must be set as default if we're installing.
	{
		OpButton* view_button = (OpButton*)GetWidgetByName("View_selected_cert_button");
		if (view_button)
		{
			view_button->SetDefaultLook();
		}
	}

	// Comments field
    OpString comments;
	int i = 0;
	const OpStringC* comment = m_context->AccessCertificateComment(i);
	while (comment)
	{
		comments.AppendFormat(UNI_L("- %s\r\n"), comment->CStr());
		comment = m_context->AccessCertificateComment(++i);
	}
	OpMultilineEdit* extra_label = (OpMultilineEdit*)GetWidgetByName("Certificate_extra_label");
	if (extra_label)
	{
		if (comments.HasContent())
		{
			extra_label->SetText(comments.CStr());
			extra_label->SetReadOnly(TRUE);
			extra_label->SetVisibility(TRUE);
		}
		else
		{
			extra_label->SetVisibility(FALSE);
		}
	}
}

/***********************************************************************************
**
**	GetButtonCount
**
***********************************************************************************/

INT32 CertificateInstallWizard::GetButtonCount()
{
	return 3; // [install/ok] [cancel] [help]
}

/***********************************************************************************
**
**	GetDialogImageByEnum
**
***********************************************************************************/

Dialog::DialogImage CertificateInstallWizard::GetDialogImageByEnum()
{
	return Dialog::IMAGE_NONE;
}

/***********************************************************************************
**
**	GetButtonInfo
**
***********************************************************************************/

void CertificateInstallWizard::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;
	is_default = FALSE;
	switch (index)
	{
	case 0:
		{
			// mariusab 20070503: This mix of actions should be decoupled and
			// a separate dialog made for the two unrelated uses of this dialog.
			if (m_type == SELECT_CERT)
			{
				g_languageManager->GetString(Str::DI_IDOK, text);
				action = GetOkAction();
				is_default = TRUE;
			}
			else // (m_type == INSTALL_CERT)
			{
				g_languageManager->GetString(Str::DI_IDM_INSTALL_CERT, text);
				action = OP_NEW(OpInputAction, (OpInputAction::ACTION_INSTALL_CERTIFICATE));
			}
			name.Set(WIDGET_NAME_BUTTON_OK);
		}
		break;
	case 1:
		{
			action = GetCancelAction();
			text.Set(GetCancelText());
			name.Set(WIDGET_NAME_BUTTON_CANCEL);
		}
		break;
	case 2:
		{
			action = GetHelpAction();
			text.Set(GetHelpText());
			name.Set(WIDGET_NAME_BUTTON_HELP);
		}
		break;
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL CertificateInstallWizard::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_INSTALL_CERTIFICATE)
	{
		OpString message, header;
		g_languageManager->GetString(Str::S_WARN_BEFORE_INSTALLING_CERTIFICATE, message);
		g_languageManager->GetString(m_context->GetTitle(), header);
		if(SimpleDialog::ShowDialog(WINDOW_NAME_WARN_INSTALL_CERT, this, message.CStr(), header.CStr(), Dialog::TYPE_OK_CANCEL, Dialog::IMAGE_INFO, NULL, GetHelpAnchor()) == Dialog::DIALOG_RESULT_OK)
		{
			m_context->OnCertificateBrowsingDone(TRUE, OpSSLListener::SSL_CERT_OPTION_ACCEPT);
			CloseDialog(FALSE, FALSE, TRUE);
		}
		return TRUE;
	}
	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**	OnClick
**
***********************************************************************************/

void CertificateInstallWizard::OnClick(OpWidget *widget, UINT32 id)
{
	if(widget->IsNamed("View_selected_cert_button"))
	{
		CertificateDetailsDialog* dialog = OP_NEW(CertificateDetailsDialog, (m_context));
		if (dialog)
			dialog->Init(this);
	}
	Dialog::OnClick(widget, id);
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void CertificateInstallWizard::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget->IsNamed("Certificate_selector_listbox"))
	{
		INT32 pos = ((OpListBox*)widget)->GetSelectedItem();
		if(pos != -1)
		{
			m_context->SetCurrentCertificateNumber(pos);
		}
	}
	Dialog::OnChange(widget, changed_by_mouse);
}

UINT32 CertificateInstallWizard::OnOk()
{
	m_context->OnCertificateBrowsingDone(TRUE, OpSSLListener::SSL_CERT_OPTION_ACCEPT);
	return Dialog::OnOk();
}

void CertificateInstallWizard::OnCancel()
{
	m_context->OnCertificateBrowsingDone(FALSE, OpSSLListener::SSL_CERT_OPTION_REFUSE);
}
