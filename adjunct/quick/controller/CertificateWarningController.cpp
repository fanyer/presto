/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */
#include "core/pch.h"

#include "adjunct/quick/controller/CertificateWarningController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/libssl/sslcctx.h"
#include "modules/windowcommander/OpSecurityInfoParser.h"

CertificateWarningController::CertificateWarningController(
		OpSSLListener::SSLCertificateContext* context, OpSSLListener::SSLCertificateOption options,
		bool overlaid)
	: m_context(context)
	, m_options(options)
	, m_overlaid(overlaid)
{
}

void CertificateWarningController::InitL()
{
	if (m_overlaid)
	{
		QuickOverlayDialog* dialog = OP_NEW(QuickOverlayDialog, ());
		LEAVE_IF_NULL(dialog);
		LEAVE_IF_ERROR(SetDialog("Certificate Warning Dialog", dialog));
		dialog->SetDimsPage(true);
	}
	else
	{
		LEAVE_IF_ERROR(SetDialog("Certificate Warning Dialog"));
		m_dialog->SetStyle(OpWindow::STYLE_MODELESS_DIALOG);
	}

	//Binder for Treeview
	LEAVE_IF_ERROR(GetBinder()->Connect("Warning_chain_treeview", m_treeview_selected));
	LEAVE_IF_ERROR(GetBinder()->Connect("Warning_disable_checkbox", m_warning_disable));
	LEAVE_IF_ERROR(GetBinder()->Connect("Warning_details", m_warning_details));

	LEAVE_IF_ERROR(m_treeview_selected.Subscribe(MAKE_DELEGATE(*this, &CertificateWarningController::OnChangeItem)));

	LEAVE_IF_ERROR(ParseSecurityInformation());
	LEAVE_IF_ERROR(PopulateWarningPage());
	LEAVE_IF_ERROR(PopulateSecurityPage());
	LEAVE_IF_ERROR(PopulateDetailsPage());
}

OP_STATUS CertificateWarningController::ParseSecurityInformation()
{
	unsigned int cert_num = 0;
	unsigned int i;

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

		// Parse the info
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
	}

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
	OpString title;
	RETURN_IF_ERROR(g_languageManager->GetString(display_context->GetTitle(), title));
	RETURN_IF_ERROR(m_dialog->SetTitle(title));

	return OpStatus::OK;
}

OP_STATUS CertificateWarningController::PopulateSecurityPage()
{
	RETURN_IF_ERROR(SetWidgetText<QuickMultilineLabel>("Warning_security_description", m_cert_comments));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Warning_name", m_subject_name));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Warning_issuer", m_issuer_name));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Warning_valid", m_cert_valid));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Warning_encryption", m_encryption));
	return OpStatus::OK;
}

OP_STATUS CertificateWarningController::PopulateWarningPage()
{
	RETURN_IF_ERROR(SetWidgetText<QuickMultilineLabel>("Cert_info_label", m_warning_tab_text));
	RETURN_IF_ERROR(SetWidgetText<QuickLabel>("Domain_edit", m_domain_name));
	return OpStatus::OK;
}

OP_STATUS CertificateWarningController::PopulateDetailsPage()
{
	QuickTreeView* widget = m_dialog->GetWidgetCollection()->Get<QuickTreeView>("Warning_chain_treeview");
	RETURN_VALUE_IF_NULL(widget, OpStatus::ERR_NULL_POINTER);

	OpTreeView* details_tree_view = widget->GetOpWidget();
	details_tree_view->SetUserSortable(FALSE);
	details_tree_view->SetShowThreadImage(TRUE);
	details_tree_view->SetShowColumnHeaders(FALSE);
	details_tree_view->SetTreeModel(&m_warning_chain);

	return OpStatus::OK;
}

void CertificateWarningController::OnOk()
{
	OpSSLListener::SSLCertificateOption options = OpSSLListener::SSL_CERT_OPTION_ACCEPT;
	if (m_warning_disable.Get())
	{
		// The user has ticked the box not to be warned again about this server.
		options = OpSSLListener::SSLCertificateOption(options | OpSSLListener::SSL_CERT_OPTION_REMEMBER);
	}
	m_context->OnCertificateBrowsingDone(TRUE, options);
}

void CertificateWarningController::OnCancel()
{
	OpSSLListener::SSLCertificateOption options = OpSSLListener::SSL_CERT_OPTION_REFUSE;
	if (m_warning_disable.Get())
  	{
	   	// The user has ticked the box not to be warned again about this server.
		options = OpSSLListener::SSLCertificateOption(options | OpSSLListener::SSL_CERT_OPTION_REMEMBER);
	}
	m_context->OnCertificateBrowsingDone(FALSE, options);	
}

void CertificateWarningController::OnChangeItem(INT32 new_index)
{
	OpString* content = (OpString*)m_warning_chain.GetItemUserData(new_index);
	m_warning_details.Set(content ? *content : OpStringC(NULL));
}
