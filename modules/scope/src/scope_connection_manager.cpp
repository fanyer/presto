/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/scope/scope_connection_manager.h"
#include "modules/scope/src/scope_manager.h"

#ifdef SCOPE_SUPPORT

/* static */ OP_STATUS
OpScopeConnectionManager::Disable()
{
	if (!g_scope_manager)
		return OpStatus::ERR;

	g_scope_manager->Disable();

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpScopeConnectionManager::Enable()
{
	if (!g_scope_manager)
		return OpStatus::ERR;

	g_scope_manager->Enable();

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpScopeConnectionManager::Connect(OpScopeClient *client)
{
	if (!g_scope_manager)
		return OpStatus::ERR;

	if (!client)
		return OpStatus::ERR_NULL_POINTER;
			
	return g_scope_manager->AddClient(client, 0);
}

/* static */ OP_STATUS
OpScopeConnectionManager::Listen(OpScopeClient *client, int port)
{
	if (!g_scope_manager)
		return OpStatus::ERR;

	if (!client)
		return OpStatus::ERR_NULL_POINTER;

	if (port < 0)
		return OpStatus::ERR;

	return g_scope_manager->AddClient(client, port);
}

/* static */ OP_STATUS
OpScopeConnectionManager::Disconnect(OpScopeClient *client)
{
	if (!g_scope_manager)
		return OpStatus::ERR;

	return g_scope_manager->RemoveClient(client);
}

/* static */ BOOL 
OpScopeConnectionManager::IsConnected()
{
	return (g_scope_manager && g_scope_manager->IsConnected());
}

/* static */ OP_STATUS 
OpScopeConnectionManager::GetListenerAddress(OpString &address)
{
	if (!g_scope_manager)
		return OpStatus::ERR;

	OpStringC serveraddress = UNI_L("0.0.0.0");
#ifdef SCOPE_ACCEPT_CONNECTIONS
	if (g_scope_manager->GetNetworkServer())
		serveraddress = g_scope_manager->GetNetworkServer()->GetAddress();
#endif // SCOPE_ACCEPT_CONNECTIONS

#ifdef OPSYSTEMINFO_GETSYSTEMIP
	if (serveraddress.IsEmpty() || serveraddress.Compare(UNI_L("0.0.0.0")) == 0)
		return g_op_system_info->GetSystemIp(address);
#endif // OPSYSTEMINFO_GETSYSTEMIP

	return address.Set(serveraddress.CStr());
}

#endif // SCOPE_SUPPORT
