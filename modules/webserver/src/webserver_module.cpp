/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995 - 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver_module.h" 

#include "modules/webserver/webserver-api.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
#include "modules/webserver/src/rendezvous/webserver_rendezvous.h"
#endif //WEBSERVER_RENDEZVOUS_SUPPORT

#include "modules/prefs/prefsmanager/collections/pc_webserver.h"

WebserverModule::WebserverModule()
	: m_webserver(NULL)
	, m_dragonflywebserver(NULL)
	, m_webserverPrivateGlobalData(NULL)
	, m_webserverUserManager(NULL)
{}

void WebserverModule::InitL(const OperaInitInfo& info)
{
	OpString user_file;
	OP_ASSERT(g_folder_manager);
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_WEBSERVER_FOLDER, user_file));
	LEAVE_IF_ERROR(user_file.Append("/users.xml"));
		
	LEAVE_IF_ERROR(WebserverUserManager::Init(user_file));
	LEAVE_IF_ERROR(WebServer::Init());
#ifdef SCOPE_SUPPORT
	LEAVE_IF_ERROR(WebServer::InitDragonflyUPnPServer());
#endif // SCOPE_SUPPORT
}

void WebserverModule::Destroy()
{
	if (m_webserver)
		m_webserver->Stop();

	if(m_dragonflywebserver)
		m_dragonflywebserver->Stop();

	WebserverUserManager::Destroy();

	OP_DELETE(m_dragonflywebserver);
	m_dragonflywebserver = NULL;
	OP_DELETE(g_webserver);
	g_webserver = NULL;
	
	OP_DELETE(g_webserverPrivateGlobalData);
	g_webserverPrivateGlobalData = NULL;
}
#endif // WEBSERVER_SUPPORT
