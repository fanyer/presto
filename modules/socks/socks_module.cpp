/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#ifdef SOCKS_SUPPORT

#include "modules/socks/socks_module.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url_socket_wrapper.h"

/** Global PrefsCollectionSocks object (singleton). */
#define g_pc_network (g_opera->prefs_module.PrefsCollectionNetwork())

void
SocksModule::refresh()
{
	OP_ASSERT(m_is_dirty);

	if (!m_listener_registered)
	{
		m_listener_registered = TRUE;
		g_pc_network->RegisterListenerL(this);
	}

	const uni_char* server_port = g_pc_network->GetSOCKSProxyServer(NULL);
	m_socks_enabled = (server_port != NULL);
	if (m_socks_enabled)
	{
		if (!m_proxy_socket_address)
		{
			OP_STATUS stat = OpSocketAddress::Create(&m_proxy_socket_address);
			if (!OpStatus::IsSuccess(stat))
			{
				m_socks_enabled = FALSE;
				return;
			}
		}
		const uni_char* colon_pos = uni_strchr(server_port, ':');
		if (colon_pos)
		{
			uni_char server[256];
			UINT ncpy = (UINT) MIN(ARRAY_SIZE(server)-1, (size_t)(colon_pos - server_port));
			uni_strncpy(server, server_port, ncpy);
			server[ncpy] = 0;
			OP_STATUS stat = m_proxy_socket_address->FromString(server);
			if (OpStatus::IsError(stat))
				resolveHost(server);
			m_proxy_socket_address->SetPort(uni_atoi(colon_pos+1));
		}
		else
		{
			OP_STATUS stat = m_proxy_socket_address->FromString(server_port);
			if (OpStatus::IsError(stat))
				resolveHost(server_port);
			m_proxy_socket_address->SetPort(1080);
		}

		//    +----+------+----------+------+----------+
		//    |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//    +----+------+----------+------+----------+
		//    | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//    +----+------+----------+------+----------+

		m_user_pass_datagram.Delete(0);
		OpString8 aux;
		char ch;

		ch = 1; // version
		if (OpStatus::OK != m_user_pass_datagram.Append(&ch, 1))
			goto FAILED;

		const uni_char* user; user = g_pc_network->GetSOCKSUser(NULL);
		ch = user ? uni_strlen(user) : 0;
		if (ch > 100) // username length should not exceed 100
			ch = 100;
		if (OpStatus::OK != m_user_pass_datagram.Append(&ch, 1))
			goto FAILED;
		if (OpStatus::OK != aux.Set(user, ch) || OpStatus::OK != m_user_pass_datagram.Append(aux))
			goto FAILED;

		if (ch > 0)
		{
			const uni_char* pass; pass = g_pc_network->GetSOCKSPass(NULL);
			ch = pass ? uni_strlen(pass) : 0;
			if (ch > 100) // password length should not exceed 100
				ch = 100;
			if (OpStatus::OK != m_user_pass_datagram.Append(&ch, 1))
				goto FAILED;
			if (OpStatus::OK != aux.Set(pass, ch) || OpStatus::OK != m_user_pass_datagram.Append(aux))
			{
			FAILED:
				m_socks_enabled = FALSE;
				return;
			}
		}
	}
	m_is_dirty = FALSE;
}

BOOL
SocksModule::resolveHost(const uni_char* hostname)
{
	OP_STATUS status;
	if (m_host_resolver == NULL)
	{
		status = SocketWrapper::CreateHostResolver(&m_host_resolver, this);
		if (OpStatus::IsError(status))
			return FALSE;
	}
	status = m_host_resolver->Resolve(hostname);
	if (OpStatus::IsError(status))
		return FALSE;
	return TRUE;
}


void
SocksModule::InitL(const OperaInitInfo& info)
{
	m_proxy_socket_address = NULL;
	m_listener_registered = FALSE;
	m_host_resolver = NULL;
	m_is_dirty = TRUE;
	m_autoproxy_socks_server_store = NULL;
}

void
SocksModule::Destroy()
{
	if (m_proxy_socket_address)
		OP_DELETE(m_proxy_socket_address);
	if (m_listener_registered)
		g_pc_network->UnregisterListener(this);
	if (m_host_resolver)
		OP_DELETE(m_host_resolver);
	if (m_autoproxy_socks_server_store)
		OP_DELETE(m_autoproxy_socks_server_store);
}

void
SocksModule::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	OP_ASSERT(id == OpPrefsCollection::Network);
	if (static_cast<PrefsCollectionNetwork::integerpref>(pref) == PrefsCollectionNetwork::UseSOCKSProxy)
		m_is_dirty = TRUE;
}

void
SocksModule::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC & newvalue)
{
	OP_ASSERT(id == OpPrefsCollection::Network);
	if (static_cast<PrefsCollectionNetwork::stringpref>(pref) == PrefsCollectionNetwork::SOCKSProxy
	 || static_cast<PrefsCollectionNetwork::stringpref>(pref) == PrefsCollectionNetwork::SOCKSUser
	 || static_cast<PrefsCollectionNetwork::stringpref>(pref) == PrefsCollectionNetwork::SOCKSPass)
		m_is_dirty = TRUE;
}

#undef g_pc_network

void
SocksModule::OnHostResolved(OpHostResolver* host_resolver)
{
	if (host_resolver->GetAddressCount() == 0)
		return;

	host_resolver->GetAddress(m_proxy_socket_address, 0);
}


void
SocksModule::OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error)
{
	// nop
}


ServerName*
SocksModule::GetSocksServerName(ServerName* original)
{
	if (!m_autoproxy_socks_server_store)
	{
		m_autoproxy_socks_server_store = OP_NEW(ServerName_Store, (3));
		if (!m_autoproxy_socks_server_store || OpStatus::IsError(m_autoproxy_socks_server_store->Construct()))
			return NULL;
	}

	ServerName* socks_sn = m_autoproxy_socks_server_store->GetServerName(original->Name(), /*create*/TRUE);
	if (!socks_sn)
		return NULL;

	if (original->IsHostResolved())
		socks_sn->SetSocketAddress(original->SocketAddress());

	return socks_sn;
}

#endif // SOCKS_SUPPORT
