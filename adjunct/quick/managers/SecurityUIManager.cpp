/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Marius Blomli
 */

#include "core/pch.h"

#include "adjunct/quick/managers/SecurityUIManager.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/CertificateWarningController.h"
#include "adjunct/quick/dialogs/CertificateDetailsDialog.h"
#include "adjunct/quick/dialogs/CertificateInstallWizard.h"
#include "adjunct/quick/dialogs/SecurityPasswordDialog.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"

#include "modules/dochand/win.h"
#include "modules/dochand/docman.h"

SecurityUIManager::UiControllerHolder::UiControllerHolder(DialogContext& ui_controller,
		OpWindowCommander* window_commander, void* context)
	: m_ui_controller(&ui_controller)
	, m_window_commander(window_commander)
	, m_context(context)
{
	m_ui_controller->SetListener(this);
}

SecurityUIManager::UiControllerHolder::~UiControllerHolder()
{
	if (m_ui_controller)
	{
		m_ui_controller->SetListener(NULL);
		m_ui_controller->CancelDialog();
	}
}

void SecurityUIManager::UiControllerHolder::OnDialogClosing(DialogContext* ui_controller)
{
	OP_ASSERT(m_ui_controller == ui_controller);
	m_ui_controller = NULL;
	Out();
	OP_DELETE(this);
}


/***********************************************************************************
**
** SecurityUIManager
**
***********************************************************************************/

SecurityUIManager::SecurityUIManager()
{
	g_windowCommanderManager->SetSSLListener(this);
}

SecurityUIManager::~SecurityUIManager()
{
	g_windowCommanderManager->SetSSLListener(0);
}

/***********************************************************************************
**
** OnCertificateBrowsingNeeded
**
***********************************************************************************/
void SecurityUIManager::OnCertificateBrowsingNeeded(OpWindowCommander* wic, SSLCertificateContext* context, SSLCertificateReason reason, SSLCertificateOption options)
{
	Dialog* dialog = NULL;
	DesktopWindow* dw = NULL;
	CertificateWarningController* controller = NULL;
	if (wic)
		dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(wic->GetOpWindow());

	if ( ((SSL_Certificate_DisplayContext*)context)->GetClientCertificateCandidates()  ||
		 ((SSL_Certificate_DisplayContext*)context)->GetTitle() == Str::SI_INSTALL_CA_CERTIFICATE_DLG_TITLE  ||
		 ((SSL_Certificate_DisplayContext*)context)->GetTitle() == Str::SI_INSTALL_USER_CERTIFICATE_DLG_TITLE ||
		 ((SSL_Certificate_DisplayContext*)context)->GetTitle() == Str::SI_TITLE_SSL_IMPORT_KEY_AND_CERTIFICATE  )
	{
		dialog = (Dialog*)OP_NEW(CertificateInstallWizard, (context, options));
	}
	else
	{
		controller = OP_NEW(CertificateWarningController, (context, options, dw != NULL));
	}
	
	if (controller)
	{
		UiControllerHolder* holder = OP_NEW(UiControllerHolder, (*controller, wic, context));
		if (holder)
		{
			holder->Into(&m_cert_ui_controllers);
			ShowDialog(controller, g_global_ui_context, dw);
			UpdateDocumentIcon(wic);
		}
	}  		
	else if (dialog)
	{
		SecurityDialogWrapper * dialog_wrapper = OP_NEW(SecurityDialogWrapper, (wic, dialog, context));
		if (dialog_wrapper)
		{
			dialog_wrapper->Into(&certList);
			dialog->Init(dw);
			UpdateDocumentIcon(wic);
		}
		else
			OP_DELETE(dialog);
	}
}

/***********************************************************************************
**
** DisplayPasswordDialog
**
***********************************************************************************/

void SecurityUIManager::OnSecurityPasswordNeeded(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback)
{
	DesktopWindow* dw;
	if (wic)
		dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(wic->GetOpWindow());
	else
		dw = g_application->GetActiveDesktopWindow(FALSE);

	SecurityPasswordDialog* dialog = OP_NEW(SecurityPasswordDialog, (callback));
	if (dialog)
	{
		SecurityDialogWrapper * dialog_wrapper = OP_NEW(SecurityDialogWrapper, (wic, dialog, callback));
		if (dialog_wrapper)
		{
			dialog_wrapper->Into(&passList);
			dialog->Init(dw);
		}
		else
			OP_DELETE(dialog);
	}
}

/***********************************************************************************
**
** OnCertificateBrowsingCancel
**
***********************************************************************************/

void SecurityUIManager::OnCertificateBrowsingCancel(OpWindowCommander* wic, SSLCertificateContext* context)
{
	SecurityDialogWrapper * wrapper = FindWrapper(wic, context);
	if (wrapper)
	{
		wrapper->GetDialog()->CloseDialog(FALSE, TRUE);
	}
	else
	{
		UiControllerHolder* holder = FindUiController(wic, context);
		if (holder)
		{
			holder->Out();
			OP_DELETE(holder);
		}
	}
}

/***********************************************************************************
**
** OnSecurityPasswordCancel
**
***********************************************************************************/

void SecurityUIManager::OnSecurityPasswordCancel(OpWindowCommander* wic)
{
	SecurityDialogWrapper * wrapper = FindWrapper(wic);
	if (wrapper)
	{
		wrapper->GetDialog()->CloseDialog(FALSE);
		wrapper->Out();
		OP_DELETE(wrapper);
	}
}

/*
 * Helper functions
 */

SecurityUIManager::SecurityDialogWrapper * SecurityUIManager::FindWrapper(OpWindowCommander * commander, SSLCertificateContext* context)
{
	for (SecurityDialogWrapper * runner = certList.First(); runner; runner = runner->Suc())
	{
		if (runner->Equals(commander, context))
			return runner;
	}
	return NULL;
}

SecurityUIManager::SecurityDialogWrapper * SecurityUIManager::FindWrapper(OpWindowCommander * commander, SSLSecurityPasswordCallback* callback/* = NULL*/)
{
	for (SecurityDialogWrapper * runner = passList.First(); runner; runner = runner->Suc())
	{
		if (runner->Equals(commander, callback))
			return runner;
	}
	return NULL;
}

SecurityUIManager::UiControllerHolder* SecurityUIManager::FindUiController(
		OpWindowCommander* commander, SSLCertificateContext* context)
{
	for (UiControllerHolder* holder = m_cert_ui_controllers.First(); holder; holder = holder->Suc())
		if (holder->Matches(commander, context))
			return holder;
	return NULL;
}

BOOL SecurityUIManager::HasCertReqProcessed(OpWindowCommander *commander)
{
	if (commander && commander->IsLoading())
	{
		for (SecurityDialogWrapper* runner = certList.First(); runner; runner = runner->Suc())
		{
			if (commander == runner->GetWindowCommander())
				return FALSE;
		}
		for (UiControllerHolder* holder = m_cert_ui_controllers.First(); holder; holder = holder->Suc())
		{
			if (commander == holder->GetWindowCommander())
				return FALSE;
		}
	}
	return TRUE;
}

void SecurityUIManager::UpdateDocumentIcon(OpWindowCommander *commander)
{
	if (!commander || !commander->GetWindow())
		return;

	OpVector<DesktopWindow> dw_list;
	OpStatus::Ignore(g_application->GetDesktopWindowCollection().GetDesktopWindows(dw_list));
	UINT32 i, count = dw_list.GetCount();
	for (i = 0; i < count; i++)
	{
		DesktopWindow *dw = dw_list.Get(i);
		OpWindowCommander *cmdr = dw->GetWindowCommander();
		if (!cmdr || 
			!cmdr->GetWindow() || 
			!cmdr->GetWindow()->DocManager() || 
			!dw->GetBrowserView())
			continue;

		URL current_url(commander->GetWindow()->DocManager()->GetCurrentURL());
		URL url(cmdr->GetWindow()->DocManager()->GetCurrentURL());
		if (url == current_url)
		{
			dw->GetBrowserView()->UpdateWindowImage(TRUE);
			dw->SetWidgetImage(dw->GetBrowserView()->GetFavIcon(FALSE));
		}
	}
}
