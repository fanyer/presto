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
 * @file CertificateWarningDialog.cpp
 *
 * Dialog used to warn about certificate problems.
 * Replaces the old CertificateInstallDialog for normal warnings.
 */

#include "core/pch.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/CertificateDetailsDialog.h"
#include "adjunct/quick/dialogs/CertificateWarningDialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpMultiEdit.h"
#ifdef SECURITY_INFORMATION_PARSER
# include "modules/windowcommander/OpSecurityInfoParser.h"
#endif // SECURITY_INFORMATION_PARSER

// When we only use the OpSSLListener SSLCertificateContext/SSLCertificateOption/SSLCertificateReason
// these includes should go away
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/libssl/sslcctx.h"

/***********************************************************************************
**
**	CertificateWarningDialog
**
***********************************************************************************/

CertificateWarningDialog::CertificateWarningDialog(OpSSLListener::SSLCertificateContext* context, OpSSLListener::SSLCertificateOption options) :
	m_context(context),
	m_options(options),
	m_warning_chain(),
	m_string_list()
{
}

/***********************************************************************************
**
**	~CertificateWarningDialog
**
***********************************************************************************/

CertificateWarningDialog::~CertificateWarningDialog()
{
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void CertificateWarningDialog::OnInit()
{
	ParseSecurityInformation();
	SetTitle(m_dialog_title.CStr());
	PopulateWarningPage();
	PopulateSecurityPage();
	PopulateDetailsPage();

}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 CertificateWarningDialog::OnOk()
{
	OpSSLListener::SSLCertificateOption options = OpSSLListener::SSL_CERT_OPTION_ACCEPT;
	OpCheckBox* checkbox = (OpCheckBox*)GetWidgetByName("Warning_disable_checkbox");
	if ( (checkbox) && (checkbox->GetValue() != 0) )
	{
		// The user has ticked the box not to be warned again about this server.
		options = (OpSSLListener::SSLCertificateOption)(options | OpSSLListener::SSL_CERT_OPTION_REMEMBER);
	}
	m_context->OnCertificateBrowsingDone(TRUE, options);

	return 0;
}

/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/

void CertificateWarningDialog::OnCancel()
{
	OpSSLListener::SSLCertificateOption options = OpSSLListener::SSL_CERT_OPTION_REFUSE;
	OpCheckBox* checkbox = (OpCheckBox*)GetWidgetByName("Warning_disable_checkbox");
	if ( (checkbox) && (checkbox->GetValue() != 0) )
	{
		// The user has ticked the box not to be warned again about this server.
		options = (OpSSLListener::SSLCertificateOption)(options | OpSSLListener::SSL_CERT_OPTION_REMEMBER);
	}
	m_context->OnCertificateBrowsingDone(FALSE, options);
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void CertificateWarningDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpTreeView* details_tree_view = (OpTreeView*)GetWidgetByName("Warning_chain_treeview");
	if (widget == details_tree_view)
	{
		int pos = details_tree_view->GetSelectedItemModelPos();
		if (pos >= 0 && pos < details_tree_view->GetItemCount())
		{
			OpMultilineEdit* details_multi_edit = (OpMultilineEdit*)GetWidgetByName("Warning_details");
			if (details_multi_edit)
			{
				OpString* user_data = (OpString*)m_warning_chain.GetItemUserData(pos);
				if (user_data)
					details_multi_edit->SetText(user_data->CStr());
				else
					details_multi_edit->SetText(UNI_L(""));

			}
		}
	}
	Dialog::OnChange(widget, changed_by_mouse);
}

INT32 CertificateWarningDialog::GetButtonCount()
{
	// accept / cancel / help
	return 3;
}

/***********************************************************************************
**
**	GetButtonInfo
**
***********************************************************************************/
void CertificateWarningDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;
	is_default = FALSE;
	// TODO: maybe java issues need different texts?
	switch (index)
	{
	case 0:
		{
			g_languageManager->GetString(Str::D_CERT_WARNING_OK, text);
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OK));
			name.Set(WIDGET_NAME_BUTTON_OK);
			break;
		}
	case 1:
		{
			g_languageManager->GetString(Str::D_CERT_WARNING_CANCEL, text);
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CANCEL));
			name.Set(WIDGET_NAME_BUTTON_CANCEL);
			break;
		}
	case 2:
		{
			text.Set(GetHelpText());
			action = GetHelpAction();
			name.Set(WIDGET_NAME_BUTTON_HELP);
			break;
		}
	}
}

/***********************************************************************************
**
**	GetDialogImageByEnum
**
***********************************************************************************/

Dialog::DialogImage CertificateWarningDialog::GetDialogImageByEnum()
{
	return Dialog::IMAGE_WARNING;
}

/***********************************************************************************
**
**	ParseSecurityInformation
**
***********************************************************************************/

OP_STATUS CertificateWarningDialog::ParseSecurityInformation()
{
	unsigned int cert_num = 0;
	unsigned int i;
#if 0
	cert_num = m_context->GetNumberOfCertificates();

	OpSSLListener::SSLCertificate * current_certificate = NULL;
	// Loop over the certificates in the context
	for (i = 0; i < cert_num; i++)
	{
		current_certificate = m_context->GetCertificate(i);
		INT32 pos = m_warning_chain.AddItem(current_certificate->GetShortName());
#else
	SSL_Certificate_DisplayContext * display_context = static_cast<SSL_Certificate_DisplayContext*>(m_context);
	OpString tmp_title;
	OpString_list title_list;
	while(display_context->GetCertificateShortName(tmp_title, cert_num)) // Need to call this to make sure that the information is populated.
	{
		title_list.Resize(cert_num + 1);
		title_list[cert_num].Set(tmp_title);
		cert_num++;
	}
	// Loop over the certificates in the context
	for (i = 0; i < cert_num; i++)
	{
		// Extract certificate info
		BOOL dummy_warn, dummy_deny;
		OpString dummy_field;
		display_context->SetCurrentCertificateNumber(i);
		URL sec_info = display_context->AccessCurrentCertificateInfo(dummy_warn, dummy_deny, dummy_field);
#endif
		// Parse the info
#ifdef SECURITY_INFORMATION_PARSER
		INT32 pos = m_warning_chain.AddItem(title_list[i].CStr());
		OpSecurityInformationParser * osip;
		RETURN_IF_ERROR(m_context->CreateSecurityInformationParser(&osip));

		SimpleTreeModel* cert_tree_mod = osip->GetServerTreeModelPtr();
		if (cert_tree_mod)
		{
			INT32 item_count = cert_tree_mod->GetItemCount();
			INT32* map = OP_NEWA(INT32, item_count); // Map between the position in the treeview from the parser and the member treeview in this class
			// Traverse the content of the generated tree model and insert the content into this class'
			// treeview.
			for (INT32 j = 0; j < item_count; j++)
			{
				// Extract parent index
				INT32 parent = cert_tree_mod->GetItemParent(j);
				if (parent == -1)
					parent = pos; // Items without a parent now has the top node of the cert as parent
				else if ( (parent >= 0) && (parent < item_count) )
					parent = map[parent]; // The parents position in the new list.
				// Extract item data
				OpString* data_string = NULL;
				uni_char* data = (uni_char*)cert_tree_mod->GetItemUserData(j);
				if (data)
				{
					data_string = OP_NEW(OpString, ());
					data_string->Set(data);
					m_string_list.Add(data_string);
				}
				// Extract item text
				OpString text;
				text.Set(cert_tree_mod->GetItemString(j));
				// Add item in the new treemodel
				INT32 new_pos = m_warning_chain.AddItem(text.CStr(), NULL, 0, parent, data_string);
				map[j] = new_pos;
			}
			OP_DELETEA(map);
		}
		// Extract the fields to display in the security tab, only needed for the first cert.
		if (i == 0)
		{
			OpSecurityInformationContainer* container = osip->GetSecurityInformationContainterPtr();
			if (container)
			{
				m_subject_name.Set(container->GetSubjectSummaryString()->CStr());
				m_issuer_name.Set(container->GetIssuerSummaryString()->CStr());
				m_cert_valid.Set(container->GetExpiresPtr()->CStr()); // TODO: Maybe strip this date down a bit, but how?
			}
		}
		OP_DELETE(osip);
#endif // SECURITY_INFORMATION_PARSER
	}
#if 0
	// Get the error messages
	int comment_num = m_context->GetNumberOfComments();
	for (i = 0; i < comment_num; i++)
	{
		if (i > 0)
			m_cert_comments.Append(UNI_L("\n"));
		if (comment_num > 1)
			m_cert_comments.AppendFormat(UNI_L("(%d) "), (i + 1));
		m_cert_comments.Append(m_context->GetCertificateComment(i));
	}
	// Get the host name
	m_context->GetServerName(&m_domain_name);

	// Get encryption protocol
	if (current_certificate)
		m_encryption.Set(current_certificate->GetInfo());
#else

	// Get encryption protocol
	m_encryption.Set(display_context->GetCipherName()); // TODO: This is not the correct protocol name
	// Get the error messages
	i = 0; // Iterator over the comments
	const OpStringC* comment = display_context->AccessCertificateComment(i);
	OpString_list comment_list;
	while (comment)
	{
		comment_list.Resize(i + 1);
		comment_list[i].Set(comment->CStr());
		comment = display_context->AccessCertificateComment(++i);
	}
	for (unsigned long j = 0; j < comment_list.Count(); j++)
	{
		if (j > 0)
			m_cert_comments.Append(UNI_L("\n"));
		if (comment_list.Count() > 1)
			m_cert_comments.AppendFormat(UNI_L("(%d) "), (j + 1));
		m_cert_comments.Append(comment_list[j]);
	}
	// Get the simple warning with the document url information, if any
	if (display_context->GetPath().HasContent())
	{
		OpString simple_warning;
		g_languageManager->GetString(display_context->GetMessage(), simple_warning);
		m_warning_tab_text.AppendFormat(UNI_L("<%s>\n\n%s"), display_context->GetPath().CStr(), simple_warning.CStr());
	}
	else
		g_languageManager->GetString(display_context->GetMessage(), m_warning_tab_text);
	// Get the host name
	if (display_context->GetServerName())
	{
		m_domain_name.Set(display_context->GetServerName()->UniName());
	}
	// Get the dialog title
	g_languageManager->GetString(display_context->GetTitle(), m_dialog_title);
#endif
	return OpStatus::OK;
}

/***********************************************************************************
**
**	PopulateSecurityPage
**
***********************************************************************************/

OP_STATUS CertificateWarningDialog::PopulateSecurityPage()
{
	OpMultilineEdit* top_label = (OpMultilineEdit*)GetWidgetByName("Warning_security_description");
	if (top_label)
	{
		top_label->SetText(m_cert_comments.CStr()); // Make sure the text fits into the field. If it doesen't, make the field scrollable.

		if (top_label->GetPreferedHeight() <= top_label->GetHeight())
			top_label->SetLabelMode();
		else
			top_label->SetReadOnly(TRUE);
	}
	OpLabel* name = (OpLabel*)GetWidgetByName("Warning_name");
	if (name)
	{
		name->SetText(m_subject_name.CStr());
		name->SetEllipsis(ELLIPSIS_END);
	}
	OpLabel* issuer = (OpLabel*)GetWidgetByName("Warning_issuer");
	if (issuer)
	{
		issuer->SetText(m_issuer_name.CStr());
		issuer->SetEllipsis(ELLIPSIS_END);
	}
	OpLabel* valid = (OpLabel*)GetWidgetByName("Warning_valid");
	if (valid)
	{
		valid->SetText(m_cert_valid.CStr());
		valid->SetEllipsis(ELLIPSIS_END);
	}
	OpLabel* encryption = (OpLabel*)GetWidgetByName("Warning_encryption");
	if (encryption)
	{
		encryption->SetText(m_encryption.CStr());
		encryption->SetEllipsis(ELLIPSIS_END);
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	PopulateWarningPage
**
***********************************************************************************/

OP_STATUS CertificateWarningDialog::PopulateWarningPage()
{
	OpMultilineEdit* info_label = (OpMultilineEdit*)GetWidgetByName("Cert_info_label");
	if (info_label)
	{
		if (info_label->GetPreferedHeight() <= info_label->GetHeight()) // Make sure the text fits into the field. If it doesen't, make the field scrollable.
			info_label->SetLabelMode();
		else
			info_label->SetReadOnly(TRUE);
		info_label->SetText(m_warning_tab_text.CStr());
	}
	OpLabel* domain_edit = (OpLabel*)GetWidgetByName("Domain_edit");
	if (domain_edit)
	{
		domain_edit->SetText(m_domain_name.CStr());
		domain_edit->SetEllipsis(ELLIPSIS_END);
		//domain_edit->SetFlatMode();
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	PopulateDetailsPage
**
***********************************************************************************/

OP_STATUS CertificateWarningDialog::PopulateDetailsPage()
{
	OpTreeView* details_tree_view = (OpTreeView*)GetWidgetByName("Warning_chain_treeview");
	if (details_tree_view)
	{
		details_tree_view->SetUserSortable(FALSE);
		details_tree_view->SetShowThreadImage(TRUE);
		details_tree_view->SetShowColumnHeaders(FALSE);
		details_tree_view->SetTreeModel(&m_warning_chain);
	}
	OpMultilineEdit* details_multi_edit = (OpMultilineEdit*)GetWidgetByName("Warning_details");
	if (details_multi_edit)
	{
		details_multi_edit->SetReadOnly(TRUE);
	}
	return OpStatus::OK;
}
