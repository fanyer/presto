/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WEBSERVER_WEBSERVER_MODULE_H
#define WEBSERVER_WEBSERVER_MODULE_H

#ifdef WEBSERVER_SUPPORT
#include "modules/hardcore/opera/module.h"

# ifdef WEBSERVER_RENDEZVOUS_SUPPORT
class WebserverRendezvous;
# endif // WEBSERVER_RENDEZVOUS_SUPPORT

class WebServer;
class WebserverSandboxProtocolManager;
class WebserverPrivateGlobalData; 
class WebserverUserManager;

class WebserverModule : public OperaModule
{
public:
	WebserverModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	WebServer* m_webserver;					// The unique global web server instance

	WebServer* m_dragonflywebserver;		// The web server to handle UPnP requests for Dragonfly

	WebserverPrivateGlobalData *m_webserverPrivateGlobalData;

	WebserverUserManager *m_webserverUserManager;
};

#define WEBSERVER_MODULE_REQUIRED

#define g_webserver	g_opera->webserver_module.m_webserver

#ifdef SCOPE_SUPPORT
#define g_dragonflywebserver	g_opera->webserver_module.m_dragonflywebserver
#endif // SCOPE_SUPPORT

#define g_webserverPrivateGlobalData	g_opera->webserver_module.m_webserverPrivateGlobalData

#define g_webserverUserManager g_opera->webserver_module.m_webserverUserManager

#endif // WEBSERVER_SUPPORT

#endif // WEBSERVER_WEBSERVER_MODULE_H
