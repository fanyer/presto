/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/dialogs/CertificateManagerDialog.h"
#include "adjunct/quick/dialogs/CertificateDetailsDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/libssl/sslv3.h"
#include "modules/libssl/certs/certexp_base.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "modules/pi/OpLocale.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
 *
 * DesktopFileChooserListeners used by CertificateManagerDialog
 *
 * *********************************************************************************/
class ImportCertificateChooserListener : public DesktopFileChooserListener
{
		OpWindow * 						window;
		SSL_Certificate_DisplayContext*	cert_contex;
		CertificateManagerDialog *		dialog;
		CertImportExportCallback::Mode	m_mode;
	public:
		DesktopFileChooserRequest			request;

		DesktopFileChooserRequest &			GetRequest() { return request; }

		ImportCertificateChooserListener(OpWindow *							window,
										 SSL_Certificate_DisplayContext *	cert_contex,
										 CertificateManagerDialog*			dialog,
										 CertImportExportCallback::Mode		mode
										)
			:	window(window),
				cert_contex(cert_contex),
				dialog(dialog),
				m_mode(mode)
			 {}

		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
			{
				if (result.files.GetCount())
				{
					if (dialog->GetCallback())
					{
						if (dialog->GetCallback()->IsInProgress())
						{
							return;
						}
						else
							OP_DELETE(dialog->GetCallback());
					}
					dialog->SetCallback(OP_NEW(CertImportExportCallback, (m_mode, window, dialog, cert_contex, result.files.Get(0)->CStr(), NULL)));
					if (dialog->GetCallback() && dialog->GetCallback()->IsInProgress())
					{
						dialog->EnableDisableImportExportButtons(FALSE);
					}
				}
			}
};

class ExportCertificateChooserListener : public DesktopFileChooserListener
{
		OpWindow * 						window;
		SSL_Certificate_DisplayContext*	cert_contex;
		OpTreeView*						certs_treeview;
		CertificateManagerDialog *		dialog;
		CertImportExportCallback::Mode	mode;
	public:
		DesktopFileChooserRequest			request;

		DesktopFileChooserRequest &			GetRequest() { return request; }

		ExportCertificateChooserListener(OpWindow *							window,
										 SSL_Certificate_DisplayContext *	cert_contex,
										 OpTreeView *						certs_treeview
										,CertificateManagerDialog *			dialog,
										 CertImportExportCallback::Mode		mode
										)
			:	window(window), cert_contex(cert_contex), certs_treeview(certs_treeview)
				, dialog(dialog), mode(mode)
				{}

		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
			{
				if (result.files.GetCount())
				{
					OpString extension;
					extension.Set(result.files.Get(0)->CStr());

					// It is very, very silly that the interface relies on the extension of the file
					// exported, this should be known by the type of certificate exported and that
					// extension should have been chosen automatically in the filechooser dialog
					// aswell, but this is how the current api is, so at least the extension parameter
					// should be an empty string when there really wasn't an extension...
					int lastdot = extension.FindLastOf('.');
					if(lastdot != KNotFound)
						extension.Set(extension.SubString(lastdot+1));
					else
						extension.Empty();

					int pos = certs_treeview->GetSelectedItemModelPos();
					cert_contex->SetCurrentCertificateNumber(pos);
					if (dialog->GetCallback())
					{
						if (dialog->GetCallback()->IsInProgress())
						{
							return;
						}
						else
							OP_DELETE(dialog->GetCallback());
					}
					dialog->SetCallback(OP_NEW(CertImportExportCallback, (mode, window, dialog, cert_contex, result.files.Get(0)->CStr(), extension.CStr())));
					if (dialog->GetCallback() && dialog->GetCallback()->IsInProgress())
					{
						dialog->EnableDisableImportExportButtons(FALSE);
					}
				}
			}
};

/***********************************************************************************
**
**	CertificateManagerDialog
**
***********************************************************************************/

CertificateManagerDialog::CertificateManagerDialog() :
	m_site_cert_context(NULL),
	m_personal_cert_context(NULL),
	m_remembered_cert_context(NULL),
	m_remembered_refused_cert_context(NULL),
	m_intermediate_cert_context(NULL),
	m_personal_certs_model(2),
	m_authorities_certs_model(1),
	m_remembered_certs_model(3),
	m_remembered_refused_certs_model(3),
	m_intermediate_certs_model(1),
	m_personal_certs_treeview(NULL),
	m_authorities_certs_treeview(NULL),
	m_remembered_certs_treeview(NULL),
	m_remembered_refused_certs_treeview(NULL),
	m_intermediate_certs_treeview(NULL),
	m_importing_exporting_enabled(FALSE),
	m_callback(NULL),
	m_chooser(0)
{
	if (0 != (m_ssloptions = g_ssl_api->CreateSecurityManager(TRUE)))
	{
		if (0 != (m_personal_cert_context = OP_NEW(SSL_Certificate_DisplayContext, (IDM_PERSONAL_CERTIFICATES_BUTT))))
			m_personal_cert_context->SetExternalOptionsManager(m_ssloptions);
		if (0 != (m_site_cert_context = OP_NEW(SSL_Certificate_DisplayContext, (IDM_SITE_CERTIFICATES_BUTT))))
			m_site_cert_context->SetExternalOptionsManager(m_ssloptions);
		if (0 != (m_remembered_cert_context = OP_NEW(SSL_Certificate_DisplayContext, (IDM_TRUSTED_CERTIFICATE_BUTT))))
			m_remembered_cert_context->SetExternalOptionsManager(m_ssloptions);
		if (0 != (m_remembered_refused_cert_context = OP_NEW(SSL_Certificate_DisplayContext, (IDM_UNTRUSTED_CERTIFICATE_BUTT))))
			m_remembered_refused_cert_context->SetExternalOptionsManager(m_ssloptions);
		if (0 != (m_intermediate_cert_context = OP_NEW(SSL_Certificate_DisplayContext, (IDM_INTERMEDIATE_CERTIFICATE_BUTT))))
			m_intermediate_cert_context->SetExternalOptionsManager(m_ssloptions);
	}
}

/***********************************************************************************
**
**	~CertificateManagerDialog
**
***********************************************************************************/

CertificateManagerDialog::~CertificateManagerDialog()
{
	if (m_site_cert_context)
		OP_DELETE(m_site_cert_context);
	if (m_personal_cert_context)
		OP_DELETE(m_personal_cert_context);
	if (m_intermediate_cert_context)
		OP_DELETE(m_intermediate_cert_context);
	if (m_remembered_cert_context)
		OP_DELETE(m_remembered_cert_context);
	if (m_remembered_refused_cert_context)
		OP_DELETE(m_remembered_refused_cert_context);

	if(m_ssloptions && m_ssloptions->dec_reference() <=0)
	{
		OP_DELETE(m_ssloptions);
	}
	if (m_callback)
	{
		OP_DELETE(m_callback);
	}

	OP_DELETE(m_chooser);
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void CertificateManagerDialog::OnInit()
{
	m_personal_certs_treeview			= (OpTreeView*)GetWidgetByName("Personal_cert_treeview");
	m_authorities_certs_treeview		= (OpTreeView*)GetWidgetByName("Authorities_cert_treeview");
	m_intermediate_certs_treeview		= (OpTreeView*)GetWidgetByName("Intermediate_cert_treeview");
	m_remembered_certs_treeview			= (OpTreeView*)GetWidgetByName("Remembered_cert_treeview");
	m_remembered_refused_certs_treeview	= (OpTreeView*)GetWidgetByName("Remembered_refused_cert_treeview");
	if(!m_personal_certs_treeview || !m_authorities_certs_treeview || !m_remembered_certs_treeview || !m_remembered_refused_certs_treeview || !m_intermediate_certs_treeview)
		return;

	// initialize the treemodels
	OpString tmp_string;

	// Initialize the client cert list
	g_languageManager->GetString(Str::D_PERSONAL_CERT_HEADER, tmp_string);
	m_personal_certs_model.SetColumnData(0, tmp_string.CStr());
	g_languageManager->GetString(Str::DI_IDM_CERT_ISSUER_LABEL, tmp_string);
	m_personal_certs_model.SetColumnData(1, tmp_string.CStr());

	AddImportedCert(CertImportExportCallback::IMPORT_PERSONAL_CERT_MODE);

	m_personal_certs_treeview->SetTreeModel(&m_personal_certs_model, 0);
	m_personal_certs_treeview->SetSelectedItem(0);
	m_personal_certs_treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_CERTIFICATE_DETAILS)));

	// Initialize the CA list
	g_languageManager->GetString(Str::D_AUTHORITY_CERT_HEADER, tmp_string);
	m_authorities_certs_model.SetColumnData(0, tmp_string.CStr());

	AddImportedCert(CertImportExportCallback::IMPORT_AUTHORITIES_CERT_MODE);

	m_authorities_certs_treeview->SetTreeModel(&m_authorities_certs_model, 0);
	m_authorities_certs_treeview->SetSelectedItem(0);
	m_authorities_certs_treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_CERTIFICATE_DETAILS)));

	// Initialize the intermediate cert list
	g_languageManager->GetString(Str::D_INTERMEDIATE_CERT_HEADER, tmp_string);
	m_intermediate_certs_model.SetColumnData(0, tmp_string.CStr());

	AddImportedCert(CertImportExportCallback::IMPORT_INTERMEDIATE_CERT_MODE);

	m_intermediate_certs_treeview->SetTreeModel(&m_intermediate_certs_model, 0);
	m_intermediate_certs_treeview->SetSelectedItem(0);
	m_intermediate_certs_treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_CERTIFICATE_DETAILS)));

	// Initialize the remembered cert list
	g_languageManager->GetString(Str::D_CERTIFICATES_CERTIFICATE_LABEL, tmp_string);
	m_remembered_certs_model.SetColumnData(0, tmp_string.CStr());
	g_languageManager->GetString(Str::D_CERTIFICATES_SERVER_LABEL, tmp_string);
	m_remembered_certs_model.SetColumnData(1, tmp_string.CStr());
 	g_languageManager->GetString(Str::DI_IDM_HTTP_PORT_LABEL, tmp_string);
 	m_remembered_certs_model.SetColumnData(2, tmp_string.CStr());

	AddImportedCert(CertImportExportCallback::IMPORT_REMEMBERED_CERT_MODE);

	m_remembered_certs_treeview->SetTreeModel(&m_remembered_certs_model, 0);
	m_remembered_certs_treeview->SetSelectedItem(0);
	m_remembered_certs_treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_CERTIFICATE_DETAILS)));

	// Initialize the remembered refused cert list

	g_languageManager->GetString(Str::D_CERTIFICATES_CERTIFICATE_LABEL, tmp_string);
	m_remembered_refused_certs_model.SetColumnData(0, tmp_string.CStr());
	g_languageManager->GetString(Str::D_CERTIFICATES_SERVER_LABEL, tmp_string);
	m_remembered_refused_certs_model.SetColumnData(1, tmp_string.CStr());
	g_languageManager->GetString(Str::D_CERTIFICATES_EXPIRES_LABEL, tmp_string);
	m_remembered_refused_certs_model.SetColumnData(2, tmp_string.CStr());

	AddImportedCert(CertImportExportCallback::IMPORT_REMEMBERED_REFUSED_CERT_MODE);

	m_remembered_refused_certs_treeview->SetTreeModel(&m_remembered_refused_certs_model, 0);
	m_remembered_refused_certs_treeview->SetSelectedItem(0);
	m_remembered_refused_certs_treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_CERTIFICATE_DETAILS)));

	OpLabel *perscert_desc = (OpLabel*)GetWidgetByName("Perscert_desc");
	if (perscert_desc)
		perscert_desc->SetWrap(TRUE);

	OpLabel *authcert_desc = (OpLabel*)GetWidgetByName("Authcert_desc");
	if (authcert_desc)
		authcert_desc->SetWrap(TRUE);

	OpLabel *intermediate_desc = (OpLabel*)GetWidgetByName("Intermediate_desc");
	if (intermediate_desc)
		intermediate_desc->SetWrap(TRUE);

	OpLabel *remembered_desc = (OpLabel*)GetWidgetByName("Remembered_desc");
	if (remembered_desc)
		remembered_desc->SetWrap(TRUE);

	OpLabel *remembered_refused_desc = (OpLabel*)GetWidgetByName("Remembered_refused_desc");
	if (remembered_refused_desc)
		remembered_refused_desc->SetWrap(TRUE);

	if(g_input_manager)
		g_input_manager->UpdateAllInputStates();
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 CertificateManagerDialog::OnOk()
{
	//commit changes done to m_ssloptions

	if(m_personal_cert_context)
	{
		m_personal_cert_context->PerformAction(SSL_CERT_BUTTON_OK);
	}

	if(m_site_cert_context)
	{
		m_site_cert_context->PerformAction(SSL_CERT_BUTTON_OK);
	}

	if (m_remembered_cert_context)
	{
		m_remembered_cert_context->PerformAction(SSL_CERT_BUTTON_OK);
	}

	if (m_remembered_refused_cert_context)
	{
		m_remembered_refused_cert_context->PerformAction(SSL_CERT_BUTTON_OK);
	}

	if (m_intermediate_cert_context)
	{
		m_intermediate_cert_context->PerformAction(SSL_CERT_BUTTON_OK);
	}

	if(m_ssloptions)
	{
		g_ssl_api->CommitOptionsManager(m_ssloptions);
	}
	return 0;
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void CertificateManagerDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(g_input_manager)
		g_input_manager->UpdateAllInputStates();
	Dialog::OnChange(widget, changed_by_mouse);
}

/***********************************************************************************
**
**	OnClose
**
***********************************************************************************/

void CertificateManagerDialog::OnClose(BOOL user_initiated)
{
	//remove this if it's not necessary
	Dialog::OnClose(user_initiated);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL CertificateManagerDialog::OnInputAction(OpInputAction* action)
{

	switch (action->GetAction())
	{

	case OpInputAction::ACTION_GET_ACTION_STATE:		//this is not yet implemented for buttons in dialogs
		{
			OpInputAction* child_action = action->GetChildAction();
			switch(child_action->GetAction())
			{
			case OpInputAction::ACTION_IMPORT_CERTIFICATE:
				{
					// During import the action must be disabled
					child_action->SetEnabled(m_importing_exporting_enabled);
				}
				break;

			case OpInputAction::ACTION_SHOW_CERTIFICATE_DETAILS:
			case OpInputAction::ACTION_DELETE_CERTIFICATE:
			case OpInputAction::ACTION_EXPORT_CERTIFICATE:
				{
					BOOL enabled = (GetCurrentPage() == CMD_PERSONAL_PAGE && m_personal_certs_treeview->GetSelectedItemModelPos() >= 0) ||
								   (GetCurrentPage() == CMD_AUTHORITIES_PAGE && m_authorities_certs_treeview->GetSelectedItemModelPos() >= 0) ||
								   (GetCurrentPage() == CMD_INTERMEDIATE_PAGE && m_intermediate_certs_treeview->GetSelectedItemModelPos() >= 0) ||
								   (GetCurrentPage() == CMD_REMEMBERED_CERT_PAGE && m_remembered_certs_treeview->GetSelectedItemModelPos() >= 0) ||
								   (GetCurrentPage() == CMD_REMEMBERED_REFUSED_CERT_PAGE && m_remembered_refused_certs_treeview->GetSelectedItemModelPos() >= 0);

					// During export the action must be disabled
					if (child_action->GetAction() == OpInputAction::ACTION_EXPORT_CERTIFICATE && !m_importing_exporting_enabled)
						enabled = FALSE;

					child_action->SetEnabled(enabled);
					return TRUE;
				}
				break;
			}

			return FALSE;
		}
	case OpInputAction::ACTION_SHOW_CERTIFICATE_DETAILS:
		{
			SSL_Certificate_DisplayContext  *ssl_cert_display_context;
			SimpleTreeModel					*simple_tree_model;
			OpTreeView						*tree_view;

			GetCurrentPageSSLCertDisplayContext(&ssl_cert_display_context, &simple_tree_model, &tree_view);

			if (ssl_cert_display_context && tree_view->GetSelectedItemModelPos() > -1)
			{
				ssl_cert_display_context->SetCurrentCertificateNumber(tree_view->GetSelectedItemModelPos());
				CertificateDetailsDialog* dialog = OP_NEW(CertificateDetailsDialog, (ssl_cert_display_context));
				if (dialog)
					dialog->Init(this);
				return TRUE;
			}

			return FALSE;
		}

	case OpInputAction::ACTION_DELETE_CERTIFICATE:
		{
			SSL_Certificate_DisplayContext  *ssl_cert_display_context;
			SimpleTreeModel					*simple_tree_model;
			OpTreeView						*tree_view;

			GetCurrentPageSSLCertDisplayContext(&ssl_cert_display_context, &simple_tree_model, &tree_view);

			int pos = tree_view->GetSelectedItemModelPos();
			if (ssl_cert_display_context && pos > -1)
			{
				ssl_cert_display_context->SetCurrentCertificateNumber(pos);
				if (SSL_CERT_DELETE_CURRENT_ITEM == ssl_cert_display_context->PerformAction(SSL_CERT_BUTTON_INSTALL_DELETE))
				{
					simple_tree_model->Delete(pos);
					return TRUE;
				}
			}

			return FALSE;
		}

	case OpInputAction::ACTION_IMPORT_CERTIFICATE:
		{
			SSL_Certificate_DisplayContext  *ssl_cert_display_context;
			SimpleTreeModel					*simple_tree_model;
			OpTreeView						*tree_view;

			GetCurrentPageSSLCertDisplayContext(&ssl_cert_display_context, &simple_tree_model, &tree_view);

			if (!ssl_cert_display_context)
				return FALSE;

			if (!m_chooser)
				RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), FALSE);

			ImportCertificateChooserListener *listener = OP_NEW(ImportCertificateChooserListener, (GetOpWindow(), ssl_cert_display_context, this, GetCurrentPageCertImportCallbackMode()));
			if (listener)
			{
				m_chooser_listener = listener;
				DesktopFileChooserRequest& request = listener->GetRequest();
				g_languageManager->GetString(Str::D_IMPORT_CERTIFICATE, request.caption);
				request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
				request.fixed_filter = TRUE;
				OpString filter;
				OP_STATUS rc = g_languageManager->GetString(ssl_cert_display_context->GetTitle() == Str::SI_SITE_CERT_DLG_TITLE ?
							   Str::LocaleString(Str::SI_IMEXPORT_OTHER_CERTIFICATE_TYPES) :
							   Str::LocaleString(Str::SI_IMPORT_USER_CERTIFICATE_TYPES), filter);
				if (OpStatus::IsSuccess(rc) && filter.HasContent())
				{
					StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
				}
				if (OpStatus::IsSuccess(m_chooser->Execute(GetOpWindow(), listener, request)))
					return TRUE;
			}

			return FALSE;
		}

	case OpInputAction::ACTION_EXPORT_CERTIFICATE:
		{
			SSL_Certificate_DisplayContext  *ssl_cert_display_context;
			SimpleTreeModel					*simple_tree_model;
			OpTreeView						*tree_view;

			GetCurrentPageSSLCertDisplayContext(&ssl_cert_display_context, &simple_tree_model, &tree_view);

			if (!ssl_cert_display_context)
				return FALSE;

			if (!m_chooser)
				RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), FALSE);

			ExportCertificateChooserListener *listener = OP_NEW(ExportCertificateChooserListener, (GetOpWindow(), ssl_cert_display_context, tree_view, this, GetCurrentPageCertExportCallbackMode()));
			if (listener)
			{
				m_chooser_listener = listener;
				DesktopFileChooserRequest& request = listener->GetRequest();
				g_languageManager->GetString(Str::D_EXPORT_CERTIFICATE, request.caption);
				request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE;
				request.fixed_filter = TRUE;
				OpString filter;
				OP_STATUS rc = g_languageManager->GetString(ssl_cert_display_context->GetTitle() == Str::SI_SITE_CERT_DLG_TITLE ?
							   Str::LocaleString(Str::SI_IMEXPORT_OTHER_CERTIFICATE_TYPES) :
							   Str::LocaleString(Str::SI_EXPORT_USER_CERTIFICATE_TYPES), filter);

				if (OpStatus::IsSuccess(rc) && filter.HasContent())
				{
					StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
				}

				if (OpStatus::IsSuccess(m_chooser->Execute(GetOpWindow(), listener, request)))
					return TRUE;
			}

			return FALSE;
		}
	default:
		{
			return Dialog::OnInputAction(action);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CertificateManagerDialog::GetCurrentPageSSLCertDisplayContext(SSL_Certificate_DisplayContext **ssl_cert_display_context, SimpleTreeModel **simple_tree_model, OpTreeView **tree_view)
{
	switch(GetCurrentPage())
	{
		case CMD_PERSONAL_PAGE:
			*ssl_cert_display_context = m_personal_cert_context;
			*simple_tree_model = &m_personal_certs_model;
			*tree_view = m_personal_certs_treeview;
		break;

		case CMD_AUTHORITIES_PAGE:
			*ssl_cert_display_context = m_site_cert_context;
			*simple_tree_model = &m_authorities_certs_model;
			*tree_view = m_authorities_certs_treeview;
		break;

		case CMD_INTERMEDIATE_PAGE:
			*ssl_cert_display_context = m_intermediate_cert_context;
			*simple_tree_model = &m_intermediate_certs_model;
			*tree_view = m_intermediate_certs_treeview;
		break;

		case CMD_REMEMBERED_CERT_PAGE:
			*ssl_cert_display_context = m_remembered_cert_context;
			*simple_tree_model = &m_remembered_certs_model;
			*tree_view = m_remembered_certs_treeview;
		break;

		case CMD_REMEMBERED_REFUSED_CERT_PAGE:
			*ssl_cert_display_context = m_remembered_refused_cert_context;
			*simple_tree_model = &m_remembered_refused_certs_model;
			*tree_view = m_remembered_refused_certs_treeview;
		break;

		default:
			return FALSE;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CertImportExportCallback::Mode CertificateManagerDialog::GetCurrentPageCertImportCallbackMode()
{
	switch(GetCurrentPage())
	{
		case CMD_PERSONAL_PAGE:
			return CertImportExportCallback::IMPORT_PERSONAL_CERT_MODE;

		case CMD_AUTHORITIES_PAGE:
			return CertImportExportCallback::IMPORT_AUTHORITIES_CERT_MODE;

		case CMD_INTERMEDIATE_PAGE:
			return CertImportExportCallback::IMPORT_INTERMEDIATE_CERT_MODE;

		case CMD_REMEMBERED_CERT_PAGE:
			return CertImportExportCallback::IMPORT_REMEMBERED_CERT_MODE;

		case CMD_REMEMBERED_REFUSED_CERT_PAGE:
			return CertImportExportCallback::IMPORT_REMEMBERED_REFUSED_CERT_MODE;
	}

	// Should never get here
	return CertImportExportCallback::IMPORT_PERSONAL_CERT_MODE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CertImportExportCallback::Mode CertificateManagerDialog::GetCurrentPageCertExportCallbackMode()
{
	switch(GetCurrentPage())
	{
		case CMD_PERSONAL_PAGE:
			return CertImportExportCallback::EXPORT_PERSONAL_CERT_MODE;

		case CMD_AUTHORITIES_PAGE:
			return CertImportExportCallback::EXPORT_AUTHORITIES_CERT_MODE;

		case CMD_INTERMEDIATE_PAGE:
			return CertImportExportCallback::EXPORT_INTERMEDIATE_CERT_MODE;

		case CMD_REMEMBERED_CERT_PAGE:
			return CertImportExportCallback::EXPORT_REMEMBERED_CERT_MODE;

		case CMD_REMEMBERED_REFUSED_CERT_PAGE:
			return CertImportExportCallback::EXPORT_REMEMBERED_REFUSED_CERT_MODE;
	}

	// Should never get here
	return CertImportExportCallback::EXPORT_PERSONAL_CERT_MODE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CertificateManagerDialog::AddImportedCert(CertImportExportCallback::Mode mode)
{
	OpString tmp_string;
	int i = 0;

	switch (mode)
	{
		case CertImportExportCallback::IMPORT_PERSONAL_CERT_MODE:
		{
			m_personal_certs_model.DeleteAll();

			while (m_personal_cert_context->GetCertificateShortName(tmp_string, i))
			{
				m_personal_certs_model.AddItem(tmp_string.CStr());
				m_personal_cert_context->GetIssuerShortName(tmp_string, i);
				m_personal_certs_model.SetItemData(i, 1, tmp_string.CStr());
				i++;
			}
		}
		break;

		case CertImportExportCallback::IMPORT_AUTHORITIES_CERT_MODE:
		{
			m_authorities_certs_model.DeleteAll();

			while (m_site_cert_context->GetCertificateShortName(tmp_string, i++))
				m_authorities_certs_model.AddItem(tmp_string.CStr());
		}
		break;

		case CertImportExportCallback::IMPORT_INTERMEDIATE_CERT_MODE:
		{
			m_intermediate_certs_model.DeleteAll();

			while (m_intermediate_cert_context->GetCertificateShortName(tmp_string, i++))
				m_intermediate_certs_model.AddItem(tmp_string.CStr());
		}
		break;

		case CertImportExportCallback::IMPORT_REMEMBERED_CERT_MODE:
		{
			m_remembered_certs_model.DeleteAll();

			while (m_remembered_cert_context->GetCertificateShortName(tmp_string, i))
			{
				// Format of the string "server:port certname"

				// Sanity check:
				int colon = tmp_string.FindFirstOf((uni_char)':');
				int space = tmp_string.FindFirstOf((uni_char)' ');

				if(colon == KNotFound || space == KNotFound || space < colon)
					continue;

				// Get the server
				OpString server;
				server.Set(tmp_string.CStr(), colon);

				// Get the port
				OpString port;
				port.Set(tmp_string.SubString(colon+1).CStr(), space-colon);

				// Get the certificate name
				OpString cert_name;
				cert_name.Set(tmp_string.SubString(space));

				// Get the expiration time - currently not supported from core #323251
				OpString time;
				GetExpiredTimeString(m_remembered_cert_context, i, time);

				m_remembered_certs_model.AddItem(cert_name.CStr());
				m_remembered_certs_model.SetItemData(i, 1, server.CStr());
				m_remembered_certs_model.SetItemData(i, 2, port.CStr());
				// m_remembered_certs_model.SetItemData(i, 3, time); // See above

				i++;
			}
		}
		break;

		case CertImportExportCallback::IMPORT_REMEMBERED_REFUSED_CERT_MODE:
		{
			m_remembered_refused_certs_model.DeleteAll();

			while (m_remembered_refused_cert_context->GetCertificateShortName(tmp_string, i))
			{
				int space = tmp_string.FindFirstOf((uni_char)' ');
				OpString cert_name, server;
				if (space != KNotFound)
				{
					cert_name.Set(tmp_string.CStr(), space);
					server.Set(tmp_string.SubString(space));
				}
				else
				{
					cert_name.Set(tmp_string);
					server.Empty();
				}
				m_remembered_refused_certs_model.AddItem(cert_name.CStr());
				m_remembered_refused_certs_model.SetItemData(i, 1, server.CStr());
				time_t trusted_until = m_remembered_refused_cert_context->GetTrustedUntil(i);
				if (trusted_until > 0) // 0 signals error
				{
					OpString time_buf;
					time_buf.Reserve(64);
					tm* gmt_trusted_until = op_gmtime(&trusted_until);
					g_oplocale->op_strftime(time_buf.CStr(), time_buf.Length(), UNI_L("%x"), gmt_trusted_until);
					m_remembered_refused_certs_model.SetItemData(i, 2, time_buf.CStr());
				}
				i++;
			}
		}
		break;
	}

	// Even though the import may take time the current implementation
	// only supports importing one at a time, so the buttons are disabled
	// while importing, now the import is done and it is safe to enable them
	// again.
	EnableDisableImportExportButtons(TRUE);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CertificateManagerDialog::UpdateCTX(CertImportExportCallback::Mode mode)
{
	switch (mode)
	{
		case CertImportExportCallback::IMPORT_PERSONAL_CERT_MODE:
		{
			m_personal_cert_context->UpdatedCertificates();
		}
		break;

		case CertImportExportCallback::IMPORT_AUTHORITIES_CERT_MODE:
		{
			if (m_ssloptions)
			{
				g_ssl_api->CommitOptionsManager(m_ssloptions);
			}
			m_site_cert_context->UpdatedCertificates();
		}
		break;

		case CertImportExportCallback::IMPORT_INTERMEDIATE_CERT_MODE:
		{
			m_intermediate_cert_context->UpdatedCertificates();
		}
		break;

		case CertImportExportCallback::IMPORT_REMEMBERED_CERT_MODE:
		{
			m_remembered_cert_context->UpdatedCertificates();
		}
		break;

		case CertImportExportCallback::IMPORT_REMEMBERED_REFUSED_CERT_MODE:
		{
			m_remembered_refused_cert_context->UpdatedCertificates();
		}
		break;
	}

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS CertificateManagerDialog::GetExpiredTimeString(SSL_Certificate_DisplayContext* context,
														 INT32 index,
														 OpString& time_string)
{
	if(!context)
		return OpStatus::ERR;

	context->GetCertificateExpirationDate(time_string, index, TRUE);

	return OpStatus::OK;
}

void CertificateManagerDialog::EnableDisableImportExportButtons(BOOL enable)
{
	m_importing_exporting_enabled = enable;
}

CertImportExportCallback::CertImportExportCallback(Mode mode, OpWindow* window, CertificateManagerDialog* dialog, SSL_Certificate_DisplayContext* disp_ctx, const uni_char* filename, const uni_char* extension) :
	m_mode(mode),
	m_dialog(dialog),
	m_config(),
	m_in_progress(TRUE),
	m_installer(NULL),
	m_exporter(NULL)
{
	m_config.window = window;
	m_config.msg_handler= g_main_message_handler;
	m_config.finished_message = MSG_IMPORTED_OR_EXPORTED_CERTIFICATE;
	m_config.finished_id = mode;

	if (!disp_ctx || !m_dialog)
	{
		OP_ASSERT(!"Not possible to initiate anything, caller needs to set these!");
		return;
	}

	g_main_message_handler->SetCallBack(this, MSG_IMPORTED_OR_EXPORTED_CERTIFICATE, m_mode);

	switch (m_mode)
	{
		case CertImportExportCallback::IMPORT_PERSONAL_CERT_MODE:
		case CertImportExportCallback::IMPORT_AUTHORITIES_CERT_MODE:
		case CertImportExportCallback::IMPORT_INTERMEDIATE_CERT_MODE:
		case CertImportExportCallback::IMPORT_REMEMBERED_CERT_MODE:
		case CertImportExportCallback::IMPORT_REMEMBERED_REFUSED_CERT_MODE:
		{
			if(!disp_ctx->ImportCertificate(filename, m_config, m_installer))
			{
				m_in_progress = FALSE;
				return;
			}

			if (!m_installer)
			{
				m_dialog->AddImportedCert(m_mode);
				g_main_message_handler->UnsetCallBack(this, MSG_IMPORTED_OR_EXPORTED_CERTIFICATE, m_mode);
				m_in_progress = FALSE;
			}
			break;
		}

		case CertImportExportCallback::EXPORT_PERSONAL_CERT_MODE:
		case CertImportExportCallback::EXPORT_AUTHORITIES_CERT_MODE:
		case CertImportExportCallback::EXPORT_INTERMEDIATE_CERT_MODE:
		case CertImportExportCallback::EXPORT_REMEMBERED_CERT_MODE:
		case CertImportExportCallback::EXPORT_REMEMBERED_REFUSED_CERT_MODE:
		{
			OP_STATUS status = disp_ctx->ExportCertificate(filename, extension, &m_config, m_exporter);

			// If it returns an error (which I don't think it does) that should be handled somehow
			OP_ASSERT(OpStatus::IsSuccess(status));

			if (!m_exporter)
			{
				g_main_message_handler->UnsetCallBack(this, MSG_IMPORTED_OR_EXPORTED_CERTIFICATE, m_mode);
				m_in_progress = FALSE;
			}
			else if(m_exporter && uni_strcmp(extension, UNI_L("p12")) == 0 && OpStatus::IsSuccess(m_exporter->StartExport()))
			{
				// p12 Export is in progress - we will get a callback when it is done
			}
			else
			{
				OP_ASSERT(!"The corresponding delete of the m_exporter object is commented out. It caused compiler warnings because there was no implementation of the exporter interface.");
			}
			break;
		}

		default:
		{
			OP_ASSERT(!"Trying to initiate an import/export with unknown mode.");
			break;
		}
	}
}

CertImportExportCallback::~CertImportExportCallback()
{
	if (m_in_progress)
	{
		g_main_message_handler->UnsetCallBack(this, MSG_IMPORTED_OR_EXPORTED_CERTIFICATE, m_mode);
		if (m_installer)
			OP_ASSERT(!"m_installer object leaked. It's not good to call this destructor until import concludes, since the exporter can't be deleted while in progress. Ideally there should be a way to interrupt the installation and delete safely."); // TODO: Fix this!
		if (m_exporter)
			OP_ASSERT(!"m_exporter object leaked. It's not good to call this destructor until export concludes, since the exporter can't be deleted while in progress. Ideally there should be a way to interrupt the installation and delete safely."); // TODO: Fix this!
	}
	else
	{
		if (m_installer)
		{
			OP_DELETE(m_installer);
			m_installer = NULL;
		}
		if (m_exporter)
		{
			OP_ASSERT(!"If this assert hits, we've started using the async exporter. That's fine, but that also means we must delete it. Comment in the line below.");
//			delete m_exporter;
		}
	}
}

void CertImportExportCallback::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (par1 != m_mode)
	{
		OP_ASSERT(!"This is not the message ID we were expecting.");
		return;
	}
	if (m_installer && !(m_installer->Finished() && m_installer->InstallSuccess()))
	{
		// Install failed
		OpString message, info;
		Str::LocaleString string = m_installer->ErrorStrings(info);
		while (string != Str::S_NOT_A_STRING)
		{
			OpString tmp;
			g_languageManager->GetString(string, tmp);
			message.AppendFormat(UNI_L("%s\n"), tmp.CStr());
			string = m_installer->ErrorStrings(info);
		}
		// Display error dialog
		SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
		if (dialog)
		{
			OpString heading;
			g_languageManager->GetString(Str::D_CERTIFICATE_IMPORT_EXPORT_ERROR, heading);
			dialog->Init(WINDOW_NAME_CERTIFICATE_IMPORT_EXPORT_ERROR, heading, message, m_dialog, Dialog::TYPE_CLOSE, Dialog::IMAGE_ERROR);
		}
	}
	if (msg == MSG_IMPORTED_OR_EXPORTED_CERTIFICATE)
	{
		switch (m_mode)
		{
		case CertImportExportCallback::IMPORT_PERSONAL_CERT_MODE:
		case CertImportExportCallback::IMPORT_AUTHORITIES_CERT_MODE:
		case CertImportExportCallback::IMPORT_INTERMEDIATE_CERT_MODE:
		case CertImportExportCallback::IMPORT_REMEMBERED_CERT_MODE:
		case CertImportExportCallback::IMPORT_REMEMBERED_REFUSED_CERT_MODE:
			if (m_dialog)
			{
				m_dialog->UpdateCTX(m_mode);
				m_dialog->AddImportedCert(m_mode);
				if (m_installer)
				{
					OP_DELETE(m_installer);
					m_installer = NULL;
				}
			}
			break;

//		case CertImportExportCallback::EXPORT_PERSONAL_CERT_MODE:
//		case CertImportExportCallback::EXPORT_AUTHORITIES_CERT_MODE:
//		case CertImportExportCallback::EXPORT_INTERMEDIATE_CERT_MODE:
//		case CertImportExportCallback::EXPORT_REMEMBERED_CERT_MODE:
//		case CertImportExportCallback::EXPORT_REMEMBERED_REFUSED_CERT_MODE:
		default:
			// No real need for any UI action...
			if (m_exporter)
			{
				// It seems that the exporter is ref counted in core and does not need to be deleted by us
 				m_exporter = NULL;
			}
			break;
		}
	}
	g_main_message_handler->UnsetCallBack(this, MSG_IMPORTED_OR_EXPORTED_CERTIFICATE, m_mode);
	m_in_progress = FALSE;
}
