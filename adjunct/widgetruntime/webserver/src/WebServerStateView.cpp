/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "WebServerStateView.h"

#include "adjunct/quick/webserver/view/WebServerErrorDescr.h"
#include "adjunct/quick/webserver/view/WebServerStatusDialog.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/webserver/webserver-api.h"


WebServerStateView::WebServerStateView()
	: m_status_dialog(NULL),
	  m_web_server_controller(NULL),
	  m_oam_status(OperaAccountManager::Disabled),
	  m_web_server_status(WEBSERVER_OK)
{
}


OP_STATUS WebServerStateView::Init(WebServerController& web_server_controller)
{
	m_web_server_controller = &web_server_controller;
	return OpStatus::OK;
}


OP_STATUS WebServerStateView::ShowStatusDialog()
{
	if (NULL != m_status_dialog)
	{
		m_status_dialog->Close(TRUE);
		m_status_dialog = NULL;
	}

	WebServerStatusContext context;

	switch (m_oam_status)
	{
		case OperaAccountManager::Error:
		{
			OP_ASSERT(OperaAccountManager::OK != m_oam_status);

			context.m_needs_suggestion = TRUE;
			RETURN_IF_ERROR(
					context.m_status_icon_name.Set("Unite Failed Large"));

			const WebServerErrorDescr* error =
					WebServerErrorDescr::FindByStatus(m_web_server_status);
			const Str::LocaleString msg_id = NULL != error
					? error->m_msg_id : Str::S_WEBSERVER_ERROR;
			RETURN_IF_ERROR(g_languageManager->GetString(
						msg_id, context.m_status_text));
			break;
		}

		case OperaAccountManager::OK:
			context.m_needs_suggestion = FALSE;
			RETURN_IF_ERROR(
					context.m_status_icon_name.Set("Unite Enabled Large"));
			RETURN_IF_ERROR(g_languageManager->GetString(
						Str::D_WEBSERVER_STATUS_DIALOG_RUNNING,
						context.m_status_text));
			break;

		case OperaAccountManager::Busy:
		case OperaAccountManager::Offline:
		case OperaAccountManager::Disabled:
		default:
			context.m_needs_suggestion = FALSE;
			RETURN_IF_ERROR(
					context.m_status_icon_name.Set("Unite Disabled Large"));
			RETURN_IF_ERROR(g_languageManager->GetString(
						Str::D_WEBSERVER_STATUS_DIALOG_DISABLED,
						context.m_status_text));
	}

	// One Unite-enabled widget per device currently, sorry.
	context.m_services_running = 1;
#ifdef WEBSERVER_CAP_STATS_TRANSFER
	context.m_size_uploaded = g_webserver ? g_webserver->GetBytesUploaded() : 0;
	context.m_size_downloaded = g_webserver ? g_webserver->GetBytesDownloaded() : 0;
#endif // WEBSERVER_CAP_STATS_TRANSFER
#ifdef WEBSERVER_CAP_STATS_NUM_USERS
	context.m_people_connected = g_webserver ? g_webserver->GetLastUsersCount(
			WebServerStatusDialog::S_NUM_MIN * 60) : 0;
#endif // WEBSERVER_CAP_STATS_NUM_USERS

	OP_ASSERT(NULL != m_web_server_controller);
	m_status_dialog =
			OP_NEW(WebServerStatusDialog, (*m_web_server_controller, context));
	if (NULL == m_status_dialog)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	g_input_manager->UpdateAllInputStates();
	m_status_dialog->SetDialogListener(this);

	RETURN_IF_ERROR(m_status_dialog->Init(NULL));

	return OpStatus::OK;
}


void WebServerStateView::OnClose(Dialog* dialog)
{
	if (NULL != m_status_dialog)
	{
		OP_ASSERT(dialog == m_status_dialog);
		m_status_dialog = NULL;
	}
}


OP_STATUS WebServerStateView::OnLoggedIn()
{
	m_oam_status = OperaAccountManager::Busy;

	return OpStatus::OK;
}


OP_STATUS WebServerStateView::OnWebServerSetUpCompleted(
		const OpStringC& shared_secret)
{
	// Protect against the "set up" and "started" notifications arriving out of
	// order.
	if (OperaAccountManager::OK != m_oam_status)
	{
		m_oam_status = OperaAccountManager::Busy;
	}

	return OpStatus::OK;
}


OP_STATUS WebServerStateView::OnWebServerStarted()
{
	m_oam_status = OperaAccountManager::OK;
	m_web_server_status = WEBSERVER_OK;

	return OpStatus::OK;
}


void WebServerStateView::OnWebServerStopped()
{
	m_oam_status = OperaAccountManager::Disabled;
	m_web_server_status = WEBSERVER_OK;
}


void WebServerStateView::OnWebServerError(WebserverStatus web_server_status)
{
	m_oam_status = OperaAccountManager::Error;
	m_web_server_status = web_server_status;

	OpStatus::Ignore(ShowStatusDialog());
}


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
