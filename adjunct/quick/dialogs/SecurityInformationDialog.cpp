/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file SecurityInformationDialog.cpp
 *
 * Contains function definitions for the classes defined in the corresponding
 * header file.
 */

#include "core/pch.h"

#include "adjunct/quick/dialogs/SecurityInformationDialog.h"

#include "adjunct/desktop_util/prefs/WidgetPrefs.h"
#include "adjunct/quick/dialogs/CertificateManagerDialog.h"
#include "adjunct/quick/managers/DesktopSecurityManager.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpEdit.h"

#ifdef SECURITY_INFORMATION_PARSER
# include "modules/windowcommander/OpSecurityInfoParser.h"
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
# include "modules/windowcommander/OpTrustInfoResolver.h"
#endif // TRUST_INFO_RESOLVER

/***********************************************************************************
**
**	Class SecurityInformationDialog
**
***********************************************************************************/

SecurityInformationDialog::SecurityInformationDialog()
	: m_server_tree_model(NULL)
	, m_client_tree_model(NULL)
#ifdef SECURITY_INFORMATION_PARSER
	, m_parser(NULL)
#endif // SECURITY_INFORMATION_PARSER
	, m_trust_level(OpDocumentListener::TRUST_NOT_SET)
#ifdef SECURITY_INFORMATION_PARSER
	, m_info_container(NULL)
#endif // SECURITY_INFORMATION_PARSER
	, m_wic(NULL)
	, m_remembered_cert_pos(-1)
#ifdef TRUST_INFO_RESOLVER
	, m_trust_info_resolver(NULL)
#endif // TRUST_INFO_RESOLVER
	, m_url_info(NULL)
{
}

OP_STATUS SecurityInformationDialog::Init(DesktopWindow* parent_window, OpWindowCommander* wic, URLInformation* url_info)
{
	if (wic)
	{
		m_wic = wic;
		m_security_mode = m_wic->GetSecurityMode();
		m_trust_level = m_wic->GetTrustMode();
#ifdef SECURITY_INFORMATION_PARSER
		m_low_security_reason = wic->SecurityLowStatusReason();
		m_server_name.Set(wic->ServerUniName());
		wic->CreateSecurityInformationParser(&m_parser);
#endif // SECURITY_INFORMATION_PARSER
	}
	else if (url_info)
	{
		m_url_info = url_info;
#ifdef SECURITY_INFORMATION_PARSER
		m_security_mode = (OpDocumentListener::SecurityMode)m_url_info->GetSecurityMode();
		m_trust_level = (OpDocumentListener::TrustMode)m_url_info->GetTrustMode();
		m_low_security_reason = m_url_info->SecurityLowStatusReason();
		m_server_name.Set(m_url_info->ServerUniName());
		m_url_info->CreateSecurityInformationParser(&m_parser);
#endif // SECURITY_INFORMATION_PARSER
	}
	else
		return OpStatus::ERR;
#ifdef SECURITY_INFORMATION_PARSER
	if (m_parser)
	{
		m_server_tree_model     = m_parser->GetValidatedTreeModelPtr();
		if (!m_server_tree_model || m_server_tree_model->GetItemCount() <= 0)
		{
			m_server_tree_model = m_parser->GetServerTreeModelPtr();
		}
		m_client_tree_model     = m_parser->GetClientTreeModelPtr();
		m_info_container        = m_parser->GetSecurityInformationContainterPtr();
	}
#endif // SECURITY_INFORMATION_PARSER
	return Dialog::Init(parent_window);
}

SecurityInformationDialog::~SecurityInformationDialog()
{
#ifdef TRUST_INFO_RESOLVER
	OP_DELETE(m_trust_info_resolver);
#endif // TRUST_INFO_RESOLVER
#ifdef SECURITY_INFORMATION_PARSER
	OP_DELETE(m_parser);
#endif // SECURITY_INFORMATION_PARSER
}

BOOL SecurityInformationDialog::GetShowPage(INT32 page_number)
{
	if (page_number == 3)
	{
		return ( (!m_client_tree_model || (m_client_tree_model->GetItemCount() <= 0)) ) ? FALSE : TRUE;
	}
	else if (page_number == 2)
	{
		return ( (!m_server_tree_model || (m_server_tree_model->GetItemCount() <= 0)) ) ? FALSE : TRUE;
	}
	return TRUE;
}

void SecurityInformationDialog::OnInit()
{
	// Set flat mode for some edit fields (was: labels)
	OpEdit *field;
	if ((field = (OpEdit*)GetWidgetByName("Summary_subject_edit")))
		field->SetFlatMode();
	if ((field = (OpEdit*)GetWidgetByName("Summary_issuer_edit")))
		field->SetFlatMode();
	if ((field = (OpEdit*)GetWidgetByName("Summary_expires_edit")))
		field->SetFlatMode();
	if ((field = (OpEdit*)GetWidgetByName("Summary_protocol_edit")))
		field->SetFlatMode();

	// Set the title of the dialog according to the url, if found.
	OpString title_string;
	MakeTitleString(title_string);
	SetTitle(title_string.CStr());

	PopulateTrustPage();
	PopulateSummaryPage();
	PopulateCertificateDetailsPage();

	// Set correct tab as active.
	// --------------------------
	// If it has a security rating of some sort and is not phishing, choose the security tab
	// Otherwise go for the fraud tab

	if(m_security_mode != OpDocumentListener::NO_SECURITY &&
	   m_security_mode != OpDocumentListener::UNKNOWN_SECURITY &&
	   m_trust_level   != OpDocumentListener::PHISHING_DOMAIN)
	{
		SetCurrentPage(0);
	}
	else
	{
		SetCurrentPage(1);
	}

	OpLabel *page_header = (OpLabel *)GetWidgetByName("Summary_page_header");
	if (page_header)
		page_header->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
}

OP_STATUS SecurityInformationDialog::PopulateTrustPage()
{
	// Get the widget pointers.
	OpBrowserView* trust_browserview = (OpBrowserView*)GetWidgetByName("Trust_information_browserview");
	OpLabel* label = (OpLabel*)GetWidgetByName("Downloading_trust_label");
	OpMultilineEdit* text = (OpMultilineEdit*)GetWidgetByName("Downloading_trust_text");
	OpButton* start_button = (OpButton*)GetWidgetByName("Get_trust_page_button");
	OpProgressBar* progress = (OpProgressBar*)GetWidgetByName("Downloading_trust_progress");
	OpIcon* icon = (OpIcon*)GetWidgetByName("page2_icon");

	if (trust_browserview)
	{
		trust_browserview->GetWindowCommander()->SetScriptingDisabled(TRUE);
		trust_browserview->SetVisibility(FALSE);
	}
	if (label)
	{
		label->SetVisibility(TRUE);
	}
	if (text)
	{
		text->SetVisibility(TRUE);
		text->SetLabelMode();
	}

	BOOL enabled = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTrustRating);

	if (enabled || (m_trust_level != OpDocumentListener::TRUST_NOT_SET) )
	{
		DownloadTrustInformation();
		if (start_button)
		{
			start_button->SetVisibility(FALSE);
		}
		if (label)
		{
			OpString string;
			g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_IN_PROGRESS, string);
			label->SetText(string.CStr());
		}
		if (text)
		{
			OpString string;
			g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_CONNECTING_TO, string);
			text->SetText(string.CStr());
		}
	}
	else
	{
		if (start_button)
		{
			start_button->SetVisibility(TRUE);
			start_button->SetEnabled(TRUE);
		}
		if (progress)
		{
			progress->SetVisibility(FALSE);
		}
		if (label)
		{
			OpString string;
			g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_DISABLED, string);
			label->SetText(string.CStr());
			label->font_info.weight = 7;
		}
		// Add a unknown trust status icon
		if (icon) 
			icon->SetImage("Trust Unknown");	//not fatal
		
		if (text)
		{
			OpString string;
			g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_USE_BUTTON, string);
			text->SetText(string.CStr());
		}
	}
	// Init the on/off checkbox
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Trust_info_on_off_box"), PrefsCollectionNetwork::EnableTrustRating);
	return OpStatus::OK;
}

OP_STATUS SecurityInformationDialog::DownloadTrustInformation(BOOL force_resolved)
{
#ifdef TRUST_INFO_RESOLVER
	OpBrowserView* trust_browserview = (OpBrowserView*)GetWidgetByName("Trust_information_browserview");
	if (trust_browserview && trust_browserview->GetWindowCommander())
	{
		if (m_wic)
		{
			RETURN_IF_ERROR(m_wic->CreateTrustInformationResolver(&m_trust_info_resolver, this));
		}
		else if (m_url_info)
		{
			RETURN_IF_ERROR(m_url_info->CreateTrustInformationResolver(&m_trust_info_resolver, this));
		}
		else
			return OpStatus::ERR;

		return m_trust_info_resolver->DownloadTrustInformation(trust_browserview->GetWindowCommander());
	}
	else
#endif // TRUST_INFO_RESOLVER
		return OpStatus::ERR;
}

#ifdef TRUST_INFO_RESOLVER
void SecurityInformationDialog::OnTrustInfoResolved()
{
	OpBrowserView* trust_browserview = (OpBrowserView*)GetWidgetByName("Trust_information_browserview");
	OpLabel* label = (OpLabel*)GetWidgetByName("Downloading_trust_label");
	OpMultilineEdit* text = (OpMultilineEdit*)GetWidgetByName("Downloading_trust_text");
	OpProgressBar* progress = (OpProgressBar*)GetWidgetByName("Downloading_trust_progress");

	trust_browserview->AddListener(this);
	OpString string;
	g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_IN_PROGRESS, string);
	label->SetText(string.CStr());
	g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_CONNECTING_TO, string);
	text->SetText(string.CStr());
	progress->SetVisibility(TRUE);
}
#endif // TRUST_INFO_RESOLVER

OP_STATUS SecurityInformationDialog::PopulateCertificateDetailsPage()
{
	if (m_server_tree_model)
	{
		SetTreeViewModel("Cert_chain_treeview", m_server_tree_model);
	}
	if (m_client_tree_model && (m_client_tree_model->GetItemCount() > 0))
	{
		SetTreeViewModel("Client_cert_chain_treeview", m_client_tree_model);
	}
	OpMultilineEdit* certDetails = (OpMultilineEdit*)GetWidgetByName("Cert_details");
	if (certDetails)
	{
		certDetails->SetReadOnly(TRUE);
	}
	return OpStatus::OK;
}

OP_STATUS SecurityInformationDialog::PopulateSummaryPage()
{
	// Set the dialog text
	OpString dialog_text;
	MakeDialogText(dialog_text);
	SetWidgetText("Connection_description_label", dialog_text.CStr());

	// Set the secure/insecure heading.
	OpString secure_insecure_heading;

	if (g_desktop_security_manager->IsHighSecurity(m_security_mode))
		g_languageManager->GetString(Str::LocaleString(Str::D_NEW_SECURITY_INFORMATION_SUMMARY_PAGE_HEADER_SECURE), secure_insecure_heading);
	else
		g_languageManager->GetString(Str::LocaleString(Str::D_NEW_SECURITY_INFORMATION_SUMMARY_PAGE_HEADER_INSECURE), secure_insecure_heading);

	SetWidgetText("Summary_page_header", secure_insecure_heading.CStr());

	OpCheckBox* remove_remembered_checkbox = (OpCheckBox*)GetWidgetByName("Remove_remembered_checkbox");
	if (remove_remembered_checkbox)
	{
#ifdef SECURITY_INFORMATION_PARSER
		if (m_info_container && m_info_container->GetPermanentlyConfirmedCertificate())
		{
			// make sure the relevant certificate is found, so we don't run into problems reopening the sec info after un-remembering it.
			m_remembered_cert_pos = -1;
			OpString tmp_string;
			int i = 0;
			SSL_Certificate_DisplayContext remembered_context(IDM_TRUSTED_CERTIFICATE_BUTT);
			while (remembered_context.GetCertificateShortName(tmp_string, i) && m_remembered_cert_pos == -1)
			{
				// Cert short name in the form "ser.verna.me:port certname"
				int colon = tmp_string.FindFirstOf((uni_char)':');
				int space = tmp_string.FindFirstOf((uni_char)' ');
				if ( (colon < space) &&
					 (tmp_string.Length() > (space + 1)) )
				{
					OpString cert_name, server, port;
					server.Set(tmp_string.CStr(), colon);
					port.Set(tmp_string.SubString(colon + 1).CStr(), space - colon - 1);
					cert_name.Set(tmp_string.SubString(space + 1));
					if ((m_info_container) &&
						(m_info_container->GetSubjectCommonNamePtr()) &&
						(m_info_container->GetSubjectCommonNamePtr()->Compare(cert_name) == 0) &&
						(m_info_container->GetServerURLPtr()) &&
						(m_info_container->GetServerURLPtr()->Compare(server) == 0) &&
						(m_info_container->GetServerPortNumberPtr()) &&
						(m_info_container->GetServerPortNumberPtr()->Compare(port) == 0) )
					{
						m_remembered_cert_pos = i;
					}
				}
				i++;
			}

			if (m_remembered_cert_pos > -1)
			{
				remove_remembered_checkbox->SetVisibility(TRUE);
				remove_remembered_checkbox->SetValue(1);
			}
			else
			{
				remove_remembered_checkbox->SetVisibility(FALSE);
			}
		}
		else
#endif // SECURITY_INFORMATION_PARSER
		{
			remove_remembered_checkbox->SetVisibility(FALSE);
		}
	}

	// Set the content of the dialog.
#ifdef SECURITY_INFORMATION_PARSER
	if (m_info_container)
	{
		OpEdit* summary_subject_edit = (OpEdit*)GetWidgetByName("Summary_subject_edit");
		if (summary_subject_edit)
		{
			summary_subject_edit->SetText(m_info_container->GetSubjectSummaryString()->CStr());
			summary_subject_edit->SetEllipsis(ELLIPSIS_END);
		}

		OpEdit* summary_issuer_edit = (OpEdit*)GetWidgetByName("Summary_issuer_edit");
		if (summary_issuer_edit)
		{
			summary_issuer_edit->SetText(m_info_container->GetIssuerSummaryString()->CStr());
			summary_issuer_edit->SetEllipsis(ELLIPSIS_END);
		}

		OpEdit* summary_expires_edit = (OpEdit*)GetWidgetByName("Summary_expires_edit");
		if (summary_expires_edit)
		{
			summary_expires_edit->SetText(m_info_container->GetExpiresPtr()->CStr());
			summary_expires_edit->SetEllipsis(ELLIPSIS_END);
		}

		OpEdit* summary_protocol_edit = (OpEdit*)GetWidgetByName("Summary_protocol_edit");
		if (summary_protocol_edit)
		{
			summary_protocol_edit->SetText(m_info_container->GetProtocolPtr()->CStr());
			summary_protocol_edit->SetEllipsis(ELLIPSIS_END);
		}
	}
	else
#endif // SECURITY_INFORMATION_PARSER
	{
		HideCertificateDetails();
	}

	// Set the header in bold text
	OpWidget* page_header = GetWidgetByName("Summary_page_header");
	if (page_header)
		page_header->font_info.weight = 7;

	// Add padlock image to the security page
	OpIcon* icon = (OpIcon*)GetWidgetByName("page_icon");
	if (icon)
	{
		if (m_security_mode == OpDocumentListener::HIGH_SECURITY)
		{
			icon->SetImage("High Security Simple");	//not fatal
		}
		else if (g_desktop_security_manager->IsEV(m_security_mode))
		{
			icon->SetImage("Extended Security");	//not fatal
		}	
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	OnOK
**
***********************************************************************************/

UINT32 SecurityInformationDialog::OnOk()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Trust_info_on_off_box"), PrefsCollectionNetwork::EnableTrustRating);
#ifdef SECURITY_INFORMATION_PARSER
	if ( m_info_container && m_info_container->GetPermanentlyConfirmedCertificate() )
	{
		OpCheckBox* remove_remembered_checkbox = (OpCheckBox*)GetWidgetByName("Remove_remembered_checkbox");
		if (remove_remembered_checkbox &&
			remove_remembered_checkbox->IsVisible() &&
			(remove_remembered_checkbox->GetValue() == 0) )
		{
			RemoveFromRemembered();
		}
	}
#endif // SECURITY_INFORMATION_PARSER
	return 0;
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/


// Puts the relevant information from the tree model in the edit field when the user clicks on an item.
void SecurityInformationDialog::OnChange(OpWidget* widget, BOOL changed_by_mouse)
{
	OpTreeView* tw = (OpTreeView*)GetWidgetByName("Cert_chain_treeview");
	OpTreeView* tw_client = (OpTreeView*)GetWidgetByName("Client_cert_chain_treeview");

	if (widget == tw)
	{
		int pos = tw->GetSelectedItemModelPos();
		int count = tw->GetItemCount();
		if (pos >= 0 && pos < count)
		{
			uni_char* user_data = (uni_char*)m_server_tree_model->GetItemUserData(pos);
			OpMultilineEdit* editfield = (OpMultilineEdit*)GetWidgetByName("Cert_details");
			if (editfield)
			{
				editfield->SetText(user_data);
			}
			if (tw_client)
			{
				tw_client->SetSelectedItem(-1);
			}
		}
	}
	else if (widget == tw_client)
	{
		int pos = tw_client->GetSelectedItemModelPos();
		int count = tw_client->GetItemCount();
		if (pos >= 0 && pos < count)
		{
			uni_char* user_data = (uni_char*)m_client_tree_model->GetItemUserData(pos);
			OpMultilineEdit* editfield = (OpMultilineEdit*)	GetWidgetByName("Client_cert_details");
			if (editfield)
			{
				editfield->SetText(user_data);
			}
			if (tw)
			{
				tw->SetSelectedItem(-1);
			}
		}
	}
	Dialog::OnChange(widget, changed_by_mouse);

	// This eliminates irritating marking of edit fields in the dialog when switching tabs.
	if (widget->GetType() == WIDGET_TYPE_TABS)
	{
		OpEdit* issued = (OpEdit*)GetWidgetByName("Issued_label");
		if (issued)
		{
			issued->SelectNothing();
		}
		OpEdit* expires = (OpEdit*)GetWidgetByName("Expires_label");
		if (expires)
		{
			expires->SelectNothing();
		}
		OpEdit* crypto_spec = (OpEdit*)GetWidgetByName("Crypto_spec_label");
		if (crypto_spec)
		{
			crypto_spec->SelectNothing();
		}
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL SecurityInformationDialog::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_DOWNLOAD_TRUST_PAGE)
	{
		// Download trust info for the document
		if (m_wic)
		{
			m_wic->CheckDocumentTrust(TRUE, FALSE);
		}
		// Start the download.
		DownloadTrustInformation();
		// Disable the button.
		OpButton* start_button = (OpButton*)GetWidgetByName("Get_trust_page_button");
		if (start_button)
		{
			start_button->SetEnabled(FALSE);
		}
		return TRUE;
	}
	return Dialog::OnInputAction(action);
}


// Move to use this dialog text later.
void SecurityInformationDialog::MakeDialogText(OpString &string)
{
	if (m_trust_level == OpDocumentListener::PHISHING_DOMAIN)
	{
		MakeFraudWarningText(string);
	}
	else
	{
		if (HasSpecifiedSecurityIssue())
		{
			// security failed because of a problem that can be specified in the dialog
			MakeFailedSecurityText(string);
		}
		else if (g_desktop_security_manager->IsEV(m_security_mode))
		{
			MakeExtendedValidationText(string);
			AddRenegotiationWarningIfNeeded(string);
		}
		else if (m_security_mode == OpDocumentListener::HIGH_SECURITY)
		{
			MakeNormalSecurityText(string);
			AddRenegotiationWarningIfNeeded(string);
		}
#ifdef SECURITY_INFORMATION_PARSER
		else if (m_info_container)
		{
			// Secuirty level is set to 0, but it seems there is some sort of
			// security available. This means we can't be too specific about
			// how insecure the user actually is, and have to give some less
			// harsh message.
			MakeNoSecurityWithReservationsText(string);
		}
#endif // SECURITY_INFORMATION_PARSER
		else
		{
			MakeNoSecurityText(string);
		}
	}
	LabelifyWidget("Connection_description_label");
}

#ifdef SECURITY_INFORMATION_PARSER
BOOL SecurityInformationDialog::HasSpecifiedSecurityIssue()
{
		return (m_info_container &&
				( m_info_container->GetUserConfirmedCertificate() ||
				m_info_container->GetPermanentlyConfirmedCertificate() ||
				m_info_container->GetOCSPProblem() ||
				// In a transition period (2010), the lack of renegotiation support is not considered a security problem so the security level remains high
				(m_info_container->GetRenegotiationExtensionUnsupported()) && (!g_desktop_security_manager->IsHighSecurity(m_security_mode))));
}
#endif // SECURITY_INFORMATION_PARSER


OP_STATUS SecurityInformationDialog::AddRenegotiationWarningIfNeeded(OpString &string)
{
	if (m_info_container == NULL)
		return OpStatus::ERR;
	if (m_info_container->GetRenegotiationExtensionUnsupported())
	{
		OpString dbstring;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MSG_NO_RENEG_SUPPORT_WARN, dbstring));
		return string.AppendFormat(UNI_L("\n\n%s"), dbstring.CStr());
	}
	return OpStatus::OK;
}


OP_STATUS SecurityInformationDialog::MakeExtendedValidationText(OpString &string)
{
	string.Empty();
	OpString dbstring;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_HIGH_ASSURANCE_HTTPS, dbstring));
	return string.AppendFormat(dbstring.CStr(), m_server_name.CStr(), m_server_name.CStr());
}

OP_STATUS SecurityInformationDialog::MakeNormalSecurityText(OpString &string)
{
	string.Empty();
	OpString dbstring;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_SAFE_HTTPS, dbstring));
	return string.AppendFormat(dbstring.CStr(), m_server_name.CStr());
}

OP_STATUS SecurityInformationDialog::MakeFailedSecurityText(OpString &string)
{
	string.Empty();
	OpString dbstring;
	g_languageManager->GetString(
#ifdef SECURITY_INFORMATION_PARSER
								 m_info_container->GetPermanentlyConfirmedCertificate() ?
								 Str::LocaleString(Str::D_SECURITY_INFORMATION_PERMANENTLY_ACCEPTED) :
#endif // SECURITY_INFORMATION_PARSER
								 Str::LocaleString(Str::D_NEW_SECURITY_INFORMATION_CERT_FAILURE), dbstring);

	string.AppendFormat(dbstring.CStr(), m_server_name.CStr());

	OP_MEMORY_VAR UINT nof_cert_errors = CountErrosAndLowSecReasons();
	// Describe each error in detail.
	UINT counter = 0;
#ifdef SECURITY_INFORMATION_PARSER
	if (m_info_container->GetConfiguredWarning())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_ASKED_WARNING, string);
	}

	if (m_info_container->GetServernameMismatch())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_SERVER_NAME_MISMATCH, string);
	}

	if (m_info_container->GetUnknownCA())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_UNKNOWN_CA, string);
	}

	if (m_info_container->GetCertExpired())
	{
		if (nof_cert_errors > 1) counter++;
		long secs_until;
		if ( (OpStatus::IsSuccess(SecondsUntilExpiry(*m_info_container->GetExpiresPtr(), secs_until))) && (secs_until < 0) )
		{
			AppendNumbered(counter, Str::D_SECURITY_INFORMATION_EXPIRED, string);
		}
		else
		{
			AppendNumbered(counter, Str::D_SECURITY_INFORMATION_INVALID, string);
		}
	}

	if (m_info_container->GetCertNotYetValid())
	{
		if (nof_cert_errors > 1) counter++;
		long secs_until;
		if ( (OpStatus::IsSuccess(SecondsUntilExpiry(*m_info_container->GetIssuedPtr(), secs_until))) && (secs_until > 0) )
		{
			AppendNumbered(counter, Str::D_SECURITY_INFORMATION_NOT_YET_VALID, string);
		}
		else
		{
			AppendNumbered(counter, Str::D_SECURITY_INFORMATION_CA_NOT_YET_VALID, string);
		}
	}

	if (m_info_container->GetAnonymousConnection())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_ANONYMOUS, string);
	}

	if (m_info_container->GetAuthenticationOnlyNoEncryption())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_UNENCRYPTED, string);
	}

	if (m_info_container->GetLowEncryptionLevel())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_LOW_ENCRYPTION, string);
	}

	if (m_info_container->GetOcspRequestFailed())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_OCSP_FAILED, string);
	}

	if (m_info_container->GetOcspResponseFailed())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_OCSP_FAILED, string);
	}
	
	if (m_info_container->GetRenegotiationExtensionUnsupported())
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::S_MSG_NO_RENEG_SUPPORT_WARN, string);
	}
#endif // SECURITY_INFORMATION_PARSER

	if (m_low_security_reason & SECURITY_LOW_REASON_WEAK_METHOD)
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_NEW_SECURITY_INFORMATION_LOW_SEC_REASON_WEAK_METHOD, string);
	}

	if ( (m_low_security_reason & SECURITY_LOW_REASON_WEAK_KEY) ||
		 (m_low_security_reason & SECURITY_LOW_REASON_SLIGHTLY_WEAK_KEY) )
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_NEW_SECURITY_INFORMATION_LOW_SEC_REASON_WEAK_KEY, string);
	}

	if (m_low_security_reason & SECURITY_LOW_REASON_WEAK_PROTOCOL)
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_NEW_SECURITY_INFORMATION_LOW_SEC_REASON_WEAK_PROTOCOL, string);
	}

	// This is covered by the security errors
	//if (m_low_security_reason & SECURITY_LOW_REASON_CERTIFICATE_WARNING)
	//{
	//	if (nof_cert_errors > 1) counter++;
	//	AppendNumbered(counter, Str::D_NEW_SECURITY_INFORMATION_LOW_SEC_REASON_CERTIFICATE_WARNINGS, string);
	//}

	// ?? Duplicate?
	if ( (m_low_security_reason & SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE) ||
		 (m_low_security_reason & SECURITY_LOW_REASON_OCSP_FAILED) )
	{
		if (nof_cert_errors > 1) counter++;
		AppendNumbered(counter, Str::D_NEW_SECURITY_INFORMATION_LOW_SEC_REASON_OCSP_FAILED, string);
	}

	if (nof_cert_errors == 0) // There were no cert errors specified
	{
		AppendNumbered(counter, Str::D_SECURITY_INFORMATION_ERROR_MISSING, string);
	}

	return OpStatus::OK;
}

OP_STATUS SecurityInformationDialog::MakeNoSecurityWithReservationsText(OpString &string)
{
	string.Empty();
	OpString dbstring;
	g_languageManager->GetString(Str::D_SECURITY_INFORMATION_MAYBE_SECURE, dbstring);
	return string.AppendFormat(dbstring.CStr(), m_server_name.CStr());
}

OP_STATUS SecurityInformationDialog::MakeNoSecurityText(OpString &string)
{
	string.Empty();
	OpString dbstring;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_INSECURE_CONNECTION, dbstring));
	return string.AppendFormat(dbstring.CStr(), m_server_name.CStr());
}

OP_STATUS SecurityInformationDialog::MakeFraudWarningText(OpString &string)
{
	string.Empty();
	return g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_FRAUD_DESCRIPTION, string);
}

void SecurityInformationDialog::MakeTitleString(OpString &string)
{
	if (m_server_name.HasContent())
	{
		OpString loc_str;
		g_languageManager->GetString(Str::D_SECURITY_INFORMATION_FOR_KNOWN_SERVER, loc_str);
		string.Empty();
		string.AppendFormat(loc_str.CStr(), m_server_name.CStr());
	}
	else
	{
		g_languageManager->GetString(Str::D_SECURITY_INFORMATION_FOR_UNKNOWN_SERVER, string);
	}
}

OP_STATUS SecurityInformationDialog::HideCertificateDetails()
{
	OP_STATUS status = OpStatus::OK;
	if (OpStatus::IsError(HideWidget("Summary_cert_heading")))
	{
		status = OpStatus::ERR;
	}
	if (OpStatus::IsError(HideWidget("Summary_cert_holder")))
	{
		status = OpStatus::ERR;
	}
	if (OpStatus::IsError(HideWidget("Summary_subject_edit")))
	{
		status = OpStatus::ERR;
	}
	if (OpStatus::IsError(HideWidget("Summary_cert_issuer")))
	{
		status = OpStatus::ERR;
	}
	if (OpStatus::IsError(HideWidget("Summary_issuer_edit")))
	{
		status = OpStatus::ERR;
	}
	if (OpStatus::IsError(HideWidget("Summary_cert_expires")))
	{
		status = OpStatus::ERR;
	}
	if (OpStatus::IsError(HideWidget("Summary_expires_edit")))
	{
		status = OpStatus::ERR;
	}
	if (OpStatus::IsError(HideWidget("Summary_protocol_heading")))
	{
		status = OpStatus::ERR;
	}
	if (OpStatus::IsError(HideWidget("Summary_protocol_edit")))
	{
		status = OpStatus::ERR;
	}
	return status;
}

OP_STATUS SecurityInformationDialog::SetTreeViewModel(const char* name, SimpleTreeModel* tree_model)
{
	OpWidget* widget = GetWidgetByName(name);
	if (!widget || widget->GetType() != WIDGET_TYPE_TREEVIEW || !tree_model)
	{
		return OpStatus::ERR;
	}
	else
	{
		((OpTreeView*)widget)->SetUserSortable(FALSE);
		((OpTreeView*)widget)->SetShowThreadImage(TRUE);
		((OpTreeView*)widget)->SetShowColumnHeaders(FALSE);
		((OpTreeView*)widget)->SetTreeModel(tree_model);
		return OpStatus::OK;
	}
}

OP_STATUS SecurityInformationDialog::LabelifyWidget(const char* name)
{
	OpWidget* widget = GetWidgetByName(name);
	if (widget && widget->GetType() == WIDGET_TYPE_MULTILINE_EDIT)
	{
#ifdef WIDGETS_CAP_MULTILINEEDIT_AGGRESSIVE_WRAP
		((OpMultilineEdit*)widget)->SetAggressiveWrapping(TRUE); // Aggressive wrap for long server domains
#endif
		if (((OpMultilineEdit*)widget)->GetPreferedHeight() <= widget->GetHeight())
		{
			((OpMultilineEdit*)widget)->SetLabelMode();
		}
		else
		{
			((OpMultilineEdit*)widget)->SetReadOnly(TRUE);
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS SecurityInformationDialog::HideWidget(const char* name)
{
	OpWidget* widget = GetWidgetByName(name);
	if (widget)
	{
		widget->SetVisibility(FALSE);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS SecurityInformationDialog::AppendNumbered(UINT number, Str::LocaleString string_id, OpString &target_string)
{
	target_string.Append(UNI_L("\n"));
	if (number > 0)
	{
		RETURN_IF_ERROR(target_string.AppendFormat(UNI_L("(%d) "), number));
	}
	else
	{
		RETURN_IF_ERROR(target_string.Append("   \n "));
	}
	OpString issue_str;
	RETURN_IF_ERROR(g_languageManager->GetString(string_id, issue_str));
	return target_string.Append(issue_str);
}

OP_STATUS SecurityInformationDialog::SecondsUntilExpiry(const OpStringC &expiry_time, long &secs_until)
{
	tm timestruct;
	RETURN_IF_ERROR(GetTMStructTime(expiry_time, timestruct));
	long time_of_expiry = (long)op_mktime(&timestruct);
	long current_time   = (long)(g_op_time_info->GetTimeUTC() / 1000);
	secs_until = time_of_expiry - current_time;
	return OpStatus::OK;
}

OP_STATUS SecurityInformationDialog::GetTMStructTime(const OpStringC &time, tm &timestruct)
{
	return OpStatus::ERR;
/*
	// TODO: This no longer works, libssl localizes the string, we need to get the string in a workable format.
	OpString time_string, date_substring, time_substring, day, month, year, hour, minute, second; // Substrings
	int date_len, time_len, day_len, month_len, hour_len, minute_len; // Length of the substrings

	uni_char space  = ' ';
	uni_char period = '.';
	uni_char colon  = ':';

	time_string.Set(time);
	date_len = time_string.FindFirstOf(space);
	time_len = time_string.FindLastOf(space) - date_len - 1;
	date_substring.Set( time_string.SubString(0,            date_len), date_len );
	time_substring.Set( time_string.SubString(date_len + 1, time_len), time_len );

	day_len   = date_substring.FindFirstOf(period);
	month_len = date_substring.FindLastOf(period) - day_len - 1;
	day.Set(   date_substring.SubString(0,                       day_len)  , day_len   );
	month.Set( date_substring.SubString(day_len + 1,             month_len), month_len );
	year.Set(  date_substring.SubString(day_len + month_len + 2, KAll)      );

	hour_len   = time_substring.FindFirstOf(colon);
	minute_len = time_substring.FindLastOf(colon) - hour_len - 1;
	hour.Set(   time_substring.SubString(0,                       hour_len), hour_len );
	minute.Set( time_substring.SubString(hour_len + 1,            hour_len), minute_len );
	second.Set( time_substring.SubString(hour_len + minute_len + 2, KAll)     );

	timestruct.tm_year = uni_atoi(year) - 1900;
	timestruct.tm_mon  = uni_atoi(month);
	timestruct.tm_mday = uni_atoi(day);
	timestruct.tm_hour = uni_atoi(hour);
	timestruct.tm_min  = uni_atoi(minute);
	timestruct.tm_sec  = uni_atoi(second);
	timestruct.tm_isdst = (int)(g_op_system_info->DaylightSavingsTimeAdjustmentMS(g_op_system_info->GetTimeUTC()) / 3600000);

	return OpStatus::OK;
*/
}

UINT SecurityInformationDialog::CountErrosAndLowSecReasons()
{
	UINT nof_cert_errors = 0;
#ifdef SECURITY_INFORMATION_PARSER
	if (m_info_container)
	{
		// Count the number of certificate errors.
		if (m_info_container->GetConfiguredWarning())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetServernameMismatch())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetUnknownCA())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetCertExpired())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetCertNotYetValid())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetAnonymousConnection())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetAuthenticationOnlyNoEncryption())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetLowEncryptionLevel())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetOcspRequestFailed())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetOcspResponseFailed())
		{
			nof_cert_errors++;
		}
		if (m_info_container->GetRenegotiationExtensionUnsupported())
		{
			nof_cert_errors++;
		}			
		// Count the number of low security reasons.
		if (m_low_security_reason & SECURITY_LOW_REASON_WEAK_METHOD)
		{
			nof_cert_errors++;
		}
		if ( (m_low_security_reason & SECURITY_LOW_REASON_WEAK_KEY) ||
			 (m_low_security_reason & SECURITY_LOW_REASON_SLIGHTLY_WEAK_KEY) )
		{
			nof_cert_errors++;
		}
		if (m_low_security_reason & SECURITY_LOW_REASON_WEAK_PROTOCOL)
		{
			nof_cert_errors++;
		}
		// Covered by other certificate warnings.
		//if (m_low_security_reason & SECURITY_LOW_REASON_CERTIFICATE_WARNING)
		//{
		//	nof_cert_errors++;
		//}
		if ( (m_low_security_reason & SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE) ||
			 (m_low_security_reason & SECURITY_LOW_REASON_OCSP_FAILED) )
		{
			nof_cert_errors++;
		}
	}
#endif // SECURITY_INFORMATION_PARSER
	return nof_cert_errors;
}

void SecurityInformationDialog::TrustPageDownloadSuccess()
{
	// Set correct layout.
	OpBrowserView* trust_browserview = (OpBrowserView*)GetWidgetByName("Trust_information_browserview");
	if (trust_browserview)
	{
		trust_browserview->GetWindowCommander()->SetScriptingDisabled(TRUE);
		trust_browserview->SetVisibility(TRUE);
	}
	OpLabel* label = (OpLabel*)GetWidgetByName("Downloading_trust_label");
	if (label)
	{
		label->SetVisibility(FALSE);
	}

	OpWidget* icon = GetWidgetByName("page2_icon");
	if(icon)
	{
		icon->SetVisibility(FALSE);
	}

	OpMultilineEdit* text = (OpMultilineEdit*)GetWidgetByName("Downloading_trust_text");
	if (text)
	{
		text->SetVisibility(FALSE);
	}
	OpProgressBar* progressbar = (OpProgressBar*)GetWidgetByName("Downloading_trust_progress");
	if (progressbar)
	{
		progressbar->SetVisibility(FALSE);
	}
	OpButton* start_button = (OpButton*)GetWidgetByName("Get_trust_page_button");
	if (start_button)
	{
		start_button->SetEnabled(TRUE);
	}
}

void SecurityInformationDialog::TrustPageDownloadError()
{
	// Stop loading if still doing that. Unlist as listener.
	OpBrowserView* trust_browserview = (OpBrowserView*)GetWidgetByName("Trust_information_browserview");
	if (trust_browserview)
	{
		OpWindowCommander* commander = trust_browserview->GetWindowCommander();
		if (commander)
		{
			commander->Stop();
		}
		trust_browserview->RemoveListener(this);
	}
	// Set correct layout.
	OpLabel* label = (OpLabel*)GetWidgetByName("Downloading_trust_label");
	if (label)
	{
		OpString string;
		g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_FAILED, string);
		label->SetText(string.CStr());
		label->SetVisibility(TRUE);
	}
	OpMultilineEdit* text = (OpMultilineEdit*)GetWidgetByName("Downloading_trust_text");
	if (text)
	{
		OpString problem;
		g_languageManager->GetString(Str::D_NEW_SECURITY_INFORMATION_TRUST_PAGE_DOWNLOAD_FAILED_DESC, problem);
		text->SetText(problem.CStr());
		text->SetVisibility(TRUE);
	}
	OpProgressBar* progressbar = (OpProgressBar*)GetWidgetByName("Downloading_trust_progress");
	if (progressbar)
	{
		progressbar->SetVisibility(FALSE);
	}
	OpButton* start_button = (OpButton*)GetWidgetByName("Get_trust_page_button");
	if (start_button)
	{
		start_button->SetVisibility(FALSE);
	}
}

OP_STATUS SecurityInformationDialog::RemoveFromRemembered()
{
	// Sanity check
	if (m_remembered_cert_pos < 0)
		return OpStatus::ERR;

	// make context for remembered certificates
	SSL_Options* options = g_ssl_api->CreateSecurityManager(TRUE);
	if (!options)
		return OpStatus::ERR;
	SSL_Certificate_DisplayContext remembered_context(IDM_TRUSTED_CERTIFICATE_BUTT);
	OpString tmp_string;
	remembered_context.SetExternalOptionsManager(options);
	for (int i = 0; remembered_context.GetCertificateShortName(tmp_string, i); i++) {}

	// delete it
	remembered_context.SetCurrentCertificateNumber(m_remembered_cert_pos);
	if (SSL_CERT_DELETE_CURRENT_ITEM == remembered_context.PerformAction(SSL_CERT_BUTTON_INSTALL_DELETE))
	{
		remembered_context.PerformAction(SSL_CERT_BUTTON_OK);
		g_ssl_api->CommitOptionsManager(options);
		return OpStatus::OK;
	}

	// If removing a remembered refused certificate, we should probably also reload the page when closign the dialog after unticking the box.
	return OpStatus::ERR;
}

BOOL SecurityInformationDialog::OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context)
{
	// We are disabling context menu for straight DOCUMENT,
	// if other types are ored in, then we let OpBrowserView handle it
	if (context.HasLink() || context.HasTextSelection())
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

void SecurityInformationDialog::OnPageLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info)
{
#ifdef TRUST_INFO_RESOLVER
	OpProgressBar* progressbar = (OpProgressBar*)GetWidgetByName("Downloading_trust_progress");
	if ( progressbar && commander->HttpResponseIs200() != MAYBE)
	{
		if ( commander->HttpResponseIs200() == NO )
		{
			TrustPageDownloadError();
		}
		progressbar->SetProgress(info->loadedBytes, info->totalBytes);
	}
#endif // TRUST_INFO_RESOLVER
}

void SecurityInformationDialog::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user)
{
#ifdef TRUST_INFO_RESOLVER
	if (commander->HttpResponseIs200() == YES)
	{
		TrustPageDownloadSuccess();
	}
	else
	{
		TrustPageDownloadError();
	}
#endif // TRUST_INFO_RESOLVER
}

#ifdef TRUST_INFO_RESOLVER
void SecurityInformationDialog::OnTrustInfoResolveError()
{
	TrustPageDownloadError();
}
#endif // TRUST_INFO_RESOLVER
