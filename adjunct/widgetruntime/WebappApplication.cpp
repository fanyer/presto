/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef WEBAPP_SUPPORT

#include "adjunct/widgetruntime/WebappApplication.h"
#include "adjunct/widgetruntime/WebappWindow.h"
#include "adjunct/widgetruntime/pi/opgadgetutilspi.h"

#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/ManagerHolder.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"


WebappApplication* WebappApplication::Create()
{
	if(!g_application)
		return OP_NEW(WebappApplication, ());
	OP_ASSERT(!"Application class is already instantiated.");
	return NULL;
}


WebappApplication::WebappApplication()
	: TemplateApplication(APPLICATION_TYPE_WEBAPP)
	, m_webapp(NULL)
{}


OP_STATUS WebappApplication::FinalizeStart()
{
	// FIXME:  This is a temporary hack that overrides the default "viewer action"
	//         for gadgets.
	//
	//         see: same hack in Application::StartBrowser with detailed comment
    Viewer* viewer = g_viewers->FindViewerByMimeType(UNI_L("application/x-opera-widgets"));
    OP_ASSERT(NULL != viewer);
    if (NULL != viewer)
    {
        viewer->SetAction(VIEWER_ASK_USER, TRUE);
        g_viewers->OnViewerChanged(viewer);
    }
	// end of hack

	// FIXME:  ManagerHolder initialization is copy-pasted from
	//		   Application::StartBrowser().  Find a way to reuse it instead.
	// FIXME:  Now that we have ManagerHolder, we should be able to selectively
	//         initialize managers (e.g., do Browselets need SpeedDialManager?)
	//         and reduce resources usage somewhat.
	m_manager_holder = OP_NEW(ManagerHolder, ());
	if (!m_manager_holder)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	RETURN_IF_ERROR(m_manager_holder->InitManagers());

	CommandLineArgument* appArg = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::App);
	m_webapp = OP_NEW(WebappWindow, ());
	m_webapp->Init(appArg->m_string_value);
	m_webapp->Show(TRUE, NULL, OpWindow::RESTORED);
	return OpStatus::OK;
}


BOOL WebappApplication::AnchorSpecial( OpWindowCommander * commander,	const OpDocumentListener::AnchorSpecialInfo & info )
{
	BOOL isExternal =  m_url_handler.IsExternalUrl(info.GetURLName());
	if( isExternal )
	{
		OpGadgetUtilsPI::ExecuteDefaultBrowser( info.GetURLName() );
	}
	return isExternal;
}

#endif // WEBAPP_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
