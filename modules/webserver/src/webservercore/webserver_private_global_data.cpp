/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * Web server implementation 
 */

#include "core/pch.h"
#ifdef WEBSERVER_SUPPORT

#include "modules/webserver/src/webservercore/webserver_private_global_data.h"
#include "modules/webserver/src/resources/webserver_auth.h"
#include "modules/stdlib/util/opdate.h"

WebserverPrivateGlobalData::WebserverPrivateGlobalData()
	: m_requestCount(0)
{
}

WebserverPrivateGlobalData::~WebserverPrivateGlobalData()
{
}

void WebserverPrivateGlobalData::SignalOOM()
{
	/* FIXME: Implement this */	
}


#endif /* WEBSERVER_SUPPORT */
