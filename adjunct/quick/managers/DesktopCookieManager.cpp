 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  *
  * Manuela Hutter
  */

#include "core/pch.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/AskCookieController.h"
#include "adjunct/quick/dialogs/CookieWarningDialog.h"
#include "adjunct/quick/managers/DesktopCookieManager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"


/***********************************************************************************
**
** Constructor
**
***********************************************************************************/
DesktopCookieManager::DesktopCookieManager()
{
	g_cookie_API->SetCookieListener(this); // handle upcoming cookies (and cookie errors)
}


/***********************************************************************************
**
** Destructor
**
***********************************************************************************/
DesktopCookieManager::~DesktopCookieManager()
{
	g_cookie_API->SetCookieListener(0);
}


/***********************************************************************************
**
** OnCookieSecurityWarning
**
***********************************************************************************/
void	DesktopCookieManager::OnCookieSecurityWarning(OpWindowCommander* windowcommander, const char* url, const char* cookie, BOOL no_ip_address)
{
	DesktopWindow* parent_window = NULL;
	if (windowcommander)
	{
		parent_window = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(windowcommander->GetOpWindow());
		if (!parent_window)
		{
			parent_window = g_application->GetActiveDesktopWindow();
		}
	
		if (parent_window)
		{
			CookieWarningDialog* dialog = OP_NEW(CookieWarningDialog, (url, cookie, no_ip_address));
			if (dialog)
			{
				OP_STATUS status = dialog->Init(parent_window, 0, TRUE);
				if (OpStatus::IsError(status))
					OP_DELETE(dialog);
			}
		}
	}
}


/***********************************************************************************
**
** OnAskCookie
**
***********************************************************************************/
void DesktopCookieManager::OnAskCookie(OpWindowCommander* windowcommander, AskCookieContext* context)
{
	DesktopWindow* parent_window = NULL;
	if (windowcommander)
	{
		parent_window = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(windowcommander->GetOpWindow());
	}

	if (!parent_window)
	{
		parent_window = g_application->GetActiveDesktopWindow();
	}
	if (!parent_window)
	{
		context->OnAskCookieCancel(); // at least handle cookie so that the browser doesn't wait forever
		return;
	}

	AskCookieController* controller = OP_NEW(AskCookieController, (context));
		
	if (OpStatus::IsError(ShowDialog(controller, g_global_ui_context, parent_window)))	
		context->OnAskCookieCancel(); // at least handle cookie so that the browser doesn't wait forever
}
