/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jonny Rein Eriksen
**
*/

#include "core/pch.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/url_man.h"
#include "modules/url/url_dns_man.h"
#include "modules/url/url_socket_wrapper.h"

UrlDNSManager::UrlDNSManager()
{
}

UrlDNSManager::~UrlDNSManager()
{
}

void UrlDNSManager::PreDestructStep()
{
	resolver_list.Clear();
}

UrlDNSManager *UrlDNSManager::CreateL()
{
	return OP_NEW_L(UrlDNSManager, ());
}

OP_STATUS UrlDNSManager::Resolve(ServerName *server_name, OpHostResolverListener *object
#if defined(OPERA_PERFORMANCE) || defined (SCOPE_RESOURCE_MANAGER)
		, BOOL prefetch /*= TRUE*/
#endif // OPERA_PERFORMANCE || SCOPE_RESOURCE_MANAGER
)
{
	OP_STATUS status = OpStatus::OK;
	ResolverListenerListItem *tempListener = NULL;

	// Check if we are already looking up this server name.
	ResolverListItem *temp = (ResolverListItem *) resolver_list.First();
	while (temp)
	{
		if (temp->server_name == server_name)
			break;
		temp = (ResolverListItem *) temp->Suc();
	}

	tempListener = OP_NEW(ResolverListenerListItem, ());
	if (!tempListener)
		return OpStatus::ERR_NO_MEMORY;

	tempListener->object = object;

	if (!temp)
	{
		temp = OP_NEW(ResolverListItem, ());
		if (!temp)
		{
			OP_DELETE(tempListener);
			return OpStatus::ERR_NO_MEMORY;
		}

		OpString hostname;
		if(OpStatus::IsError(status = hostname.Set((const char*)server_name->Name())) || OpStatus::IsError(status =  SocketWrapper::CreateHostResolver(&temp->resolver, this)))
		{
			OP_DELETE(tempListener);
			OP_DELETE(temp);
			return status;
		}

		temp->server_name = server_name;
#if defined(OPERA_PERFORMANCE) || defined (SCOPE_RESOURCE_MANAGER)
		temp->prefetch = prefetch;
		temp->time_dns_request_started = g_op_time_info->GetRuntimeMS();
#endif // OPERA_PERFORMANCE || SCOPE_RESOURCE_MANAGER
		temp->Into(&resolver_list);
		server_name->SetIsResolvingHost(TRUE);
		tempListener->Into(&temp->resolver_listener_list);
		status = temp->resolver->Resolve(hostname.CStr());
	}
	else
		tempListener->Into(&temp->resolver_listener_list);

#if defined(OPERA_PERFORMANCE) || defined (SCOPE_RESOURCE_MANAGER)
	if (temp->prefetch && !prefetch)
	{
		// Prefetch of this dns address was already in progress.
		// But now the comm loading part is starting and this
		// is the point in time that dns lookup would normally start.
		// Log the time saved so far.
		double time_dns_request_completed =  g_op_time_info->GetRuntimeMS();
		double ts = time_dns_request_completed - temp->time_dns_request_started;
		urlManager->GetNetworkStatisticsManager()->addDNSDelay(ts);
		temp->prefetch = prefetch;
	}
#endif // OPERA_PERFORMANCE || SCOPE_RESOURCE_MANAGER

	return status;
}

ResolverListItem *UrlDNSManager::FindResolverListItem(OpHostResolver* aHostResolver)
{
	ResolverListItem *temp = (ResolverListItem *) resolver_list.First();
	while (temp)
	{
		if (temp->resolver == aHostResolver)
			return temp;
		temp = (ResolverListItem *) temp->Suc();
	}
	return NULL;
}

void UrlDNSManager::OnHostResolved(OpHostResolver* aHostResolver)
{
	OP_ASSERT(aHostResolver);
	if(!aHostResolver)
		return;

	ResolverListItem *temp = FindResolverListItem(aHostResolver);
	if (temp)
	{
		temp->server_name->SetIsResolvingHost(FALSE);
#if defined(OPERA_PERFORMANCE) || defined(SCOPE_RESOURCE_MANAGER)
		if (temp->prefetch)
		{
			//We managed to complete the resolve before the Comm code even started connecting to the server. Log time saved
			double time_dns_request_completed =  g_op_time_info->GetRuntimeMS();
			double ts = time_dns_request_completed - temp->time_dns_request_started;
			urlManager->GetNetworkStatisticsManager()->addDNSDelay(ts);
		}
#endif // OPERA_PERFORMANCE || SCOPE_RESOURCE_MANAGER

		temp->server_name->SetCrossNetwork(FALSE);

		UINT count = aHostResolver->GetAddressCount();
		for (UINT i = 0; i < count; ++i)
		{
			OpStackAutoPtr<OpSocketAddress> a(NULL);
			OpSocketAddress *a1;
			RAISE_AND_RETURN_VOID_IF_ERROR(OpSocketAddress::Create(&a1));
			a.reset(a1);
			RAISE_AND_RETURN_VOID_IF_ERROR(aHostResolver->GetAddress(a.get(), i));
			OpSocketAddressNetType current_nettype = a->GetNetType();
			OP_STATUS op_err;
			if (i == 0)
			{
				op_err = temp->server_name->SetSocketAddress(a.get());
				if (op_err == OpStatus::ERR_OUT_OF_RANGE)
				{
					temp->server_name->SetCrossNetwork(TRUE);
					temp->server_name->SetAttemptedNetType(current_nettype);
				}
				else
					RAISE_AND_RETURN_VOID_IF_ERROR(op_err);
			}
			else
			{
				op_err = temp->server_name->AddSocketAddress(a.release());
				if(op_err == OpStatus::ERR_OUT_OF_RANGE)
				{
					temp->server_name->SetCrossNetwork(TRUE);
					if(current_nettype < temp->server_name->GetAttemptedNetType())
						temp->server_name->SetAttemptedNetType(current_nettype);
				}
				else
					RAISE_AND_RETURN_VOID_IF_ERROR(op_err);
			}
		}

		temp->notifying_listeners = TRUE;
		ResolverListenerListItem *tempListener = (ResolverListenerListItem *) temp->resolver_listener_list.First();
		while (tempListener)
		{
			ResolverListenerListItem *nextListener = (ResolverListenerListItem *) tempListener->Suc();;
			if (tempListener->object)
				tempListener->object->OnHostResolved(aHostResolver);

			tempListener = nextListener;
		}
		temp->notifying_listeners = FALSE;

		// the temp object can be deleted during OnHostResolved above
		temp = FindResolverListItem(aHostResolver);
		if (temp == NULL)
			return;

		tempListener = (ResolverListenerListItem *) temp->resolver_listener_list.First();
		while (tempListener)
		{
			ResolverListenerListItem *oldListener = tempListener;
			tempListener = (ResolverListenerListItem *) tempListener->Suc();
			oldListener->Out();
			OP_DELETE(oldListener);
		}

		temp->Out();
		OP_DELETE(temp);
	}
}

void UrlDNSManager::OnHostResolverError(OpHostResolver* aHostResolver, OpHostResolver::Error aError)
{
	ResolverListItem *temp = FindResolverListItem(aHostResolver);

	if (temp)
	{
		temp->notifying_listeners = TRUE;
		ResolverListenerListItem *tempListener = (ResolverListenerListItem *) temp->resolver_listener_list.First();
		while (tempListener)
		{
			ResolverListenerListItem *nextListener = (ResolverListenerListItem *) tempListener->Suc();;
			if (tempListener->object)
				tempListener->object->OnHostResolverError(aHostResolver, aError);

			tempListener = nextListener;
		}
		temp->notifying_listeners = FALSE;

		// the temp object can be deleted during OnHostResolverError above
		temp = FindResolverListItem(aHostResolver);
		if (temp == NULL)
			return;

		tempListener = (ResolverListenerListItem *) temp->resolver_listener_list.First();
		while (tempListener)
		{
			ResolverListenerListItem *oldListener = tempListener;
			tempListener = (ResolverListenerListItem *) tempListener->Suc();
			oldListener->Out();
			OP_DELETE(oldListener);
		}

		temp->Out();
		OP_DELETE(temp);
	}
}

void UrlDNSManager::RemoveListener(ServerName *server_name, OpHostResolverListener *object)
{
	// Check if we are already looking up this server name.
	ResolverListItem *temp = (ResolverListItem *) resolver_list.First();
	while (temp)
	{
		if (temp->server_name == server_name)
			break;
		temp = (ResolverListItem *) temp->Suc();
	}
	if (temp)
	{
		ResolverListenerListItem *tempListener = (ResolverListenerListItem *) temp->resolver_listener_list.First();
		while (tempListener)
		{
			ResolverListenerListItem *oldListener = tempListener;
			tempListener = (ResolverListenerListItem *) tempListener->Suc();
			if (oldListener->object == object)
			{
				oldListener->Out();
				OP_DELETE(oldListener);
				break;
			}
		}

		if (temp->resolver_listener_list.Empty() && !temp->notifying_listeners)
		{
			temp->Out();
			OP_DELETE(temp);
		}
	}
}

#ifdef SCOPE_RESOURCE_MANAGER
double UrlDNSManager::GetPrefetchTimeSpent(const ServerName* server_name)
{
	if(!server_name)
		return 0;
	ResolverListItem *temp = (ResolverListItem *) resolver_list.First();
	while (temp)
	{
		if (temp->server_name == server_name)
			break;
		temp = (ResolverListItem *) temp->Suc();
	}
	if (temp)
		return g_op_time_info->GetRuntimeMS() - temp->time_dns_request_started;

	return 0;
}
#endif // SCOPE_RESOURCE_MANAGER
