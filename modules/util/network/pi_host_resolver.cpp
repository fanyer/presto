/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef PI_HOST_RESOLVER

// DNS protocol description can be found at http://www.netfor2.com/dns.htm

#include "modules/util/network/pi_host_resolver.h"
#include "modules/pi/pi_capabilities.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url_socket_wrapper.h"

static int get_int(unsigned char** p);
static int get_short(unsigned char** p);
static unsigned int skip_domain(unsigned char** p, unsigned int max_len);

#define T_A 1
#define C_IN 1
#define C_ANY 255

// PiOpHostResolverManager

PiOpHostResolverManager::PiOpHostResolverManager() :
	m_current_id(0),
	m_preferred_dns(0),
	m_remote_addr(NULL),
	m_local_addr(NULL)
{
}

PiOpHostResolverManager::~PiOpHostResolverManager()
{
	OP_DELETE(m_remote_addr);
	OP_DELETE(m_local_addr);
	Clear();
}

void PiOpHostResolverManager::Clear()
{
	m_queries.Clear();
	m_sockets.DeleteAll();
	m_dns_addresses.DeleteAll();
	m_resend_timers.DeleteAll();
}

OP_STATUS
PiOpHostResolverManager::MakeNewQuery(const uni_char* hostname, PiOpHostResolver* resolver)
{
	return MakeNewQuery(hostname, resolver, m_preferred_dns);
}

OP_STATUS
PiOpHostResolverManager::ReadDnsAddresses()
{
	OP_STATUS ret;
	unsigned int i = 0;
	OpSocketAddress *dns_address = NULL;
	OpUdpSocket *socket;
	OpTimer *timer;

	Clear();

	while (1)
	{
		ret = g_op_system_info->GetDnsAddress(&dns_address, i);
		OpAutoPtr<OpSocketAddress> ap_address(dns_address);

		if (ret != OpStatus::OK)
			return ret;
		if (dns_address == NULL)
			break;

		dns_address->SetPort(53);

		RETURN_IF_ERROR(SocketWrapper::CreateUDPSocket(&socket, this, SocketWrapper::NO_WRAPPERS, m_local_addr));
		OpAutoPtr<OpUdpSocket> ap_socket(socket);
		RETURN_IF_ERROR(socket->Bind(m_local_addr));

		RETURN_IF_ERROR(m_sockets.Add(socket));
		RETURN_IF_ERROR(m_sockets_address.Add(socket, dns_address));

		ap_socket.release();
		RETURN_IF_ERROR(m_dns_addresses.Add(dns_address));
		ap_address.release();

		if (!m_resend_timers.Get(i))
		{
			timer = OP_NEW(OpTimer, ());
			m_resend_timers.Add(timer);
			if (!m_resend_timers.Get(i))
				return OpStatus::ERR_NO_MEMORY;
			m_resend_timers.Get(i)->SetTimerListener(this);
		}

		i++;
	}

	RETURN_IF_ERROR(g_op_system_info->GetDnsSuffixes(&m_dns_suffixes));

	if (m_sockets.GetCount() == 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}

static BOOL CheckExternalResolver(const uni_char* hostname, PiOpHostResolver* resolver)
{
	BOOL found = FALSE;
	OpAutoVector<OpSocketAddress>* list = g_op_system_info->ResolveExternalHost(hostname);
	if (!list)
		return FALSE;

	for (UINT32 n = 0; n < list->GetCount(); n++)
	{
		OpSocketAddress* addr = list->Get(n);
		if (OpStatus::IsSuccess(resolver->SetAddress(addr)))
		{
			found = TRUE;
			break;
		}
	}
	OP_DELETE(list);
	return found;
}

OP_STATUS
PiOpHostResolverManager::MakeNewQuery(const uni_char* hostname, PiOpHostResolver* resolver, unsigned int index)
{
	if (!hostname || !resolver)
	{
		return OpStatus::ERR;
	}

	if (uni_strlen(hostname) > 450)
	{
		// Really long hostname. We use a static buffer and don't want to handle such long hostnames.
		return OpStatus::ERR;
	}

	if (!m_remote_addr)
		RETURN_IF_ERROR(OpSocketAddress::Create(&m_remote_addr));
	if (!m_local_addr)
		RETURN_IF_ERROR(OpSocketAddress::Create(&m_local_addr));

	if (g_op_system_info->GetExternalResolvePolicy() == OpSystemInfo::RESOLVE_EXTERNAL_FIRST
		&& CheckExternalResolver(hostname, resolver))
	{
		resolver->GetListener()->OnHostResolved(resolver);
		return OpStatus::OK;
	}

	if (m_sockets.GetCount() <= index)
	{
		RETURN_IF_ERROR(ReadDnsAddresses());
	}

	if (m_sockets.GetCount() <= index)
	{
		return OpStatus::ERR;
	}

	// Find out how many queries we need to send.
	const uni_char* search = uni_strchr(hostname, '.') ?  NULL : m_dns_suffixes.CStr();
	int num_reqs = 1;
	if (search && *search)
		++num_reqs;
	int i;
	if (search)
	{
		for (i=0;search[i]; ++i)
		{
			if (search[i] == ' ')
				++num_reqs;
		}
	}

	// Use tempbuffer for hostname construction
	TempBuffer tmphost;
	RETURN_IF_ERROR(tmphost.Expand(uni_strlen(hostname) + (search ? m_dns_suffixes.Length() : 0) + 2));

	uni_char* t = tmphost.GetStorage();

	Query* query = OP_NEW(Query, (resolver));
	if (!query)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Create queries. for a search string which says "x y z" we will send "host.x", "host.y", "host.z", "host"
	for (i=0; i < num_reqs; ++i)
	{
		SubQuery* sub_query = OP_NEW(SubQuery, ());
		if (!sub_query)
		{
			query->Out();
			OP_DELETE(query);
			return OpStatus::ERR_NO_MEMORY;
		}
		sub_query->id = ++m_current_id;
		sub_query->index = index;
		sub_query->Into(&query->sub_queries);

		uni_strcpy(t, hostname);
		if (search && *search)
		{
			if (*search == ' ')
				++search;
			uni_strcat(t, UNI_L("."));
			const uni_char* s = search;
			while (*search && *search != ' ')
				++search;
			int len = search - s;
			uni_strncat(t, s, len);
		}

		unsigned char* tmp = MakeQuery(sub_query->id, t, C_IN, T_A, &sub_query->len);
		sub_query->req = OP_NEWA(unsigned char, sub_query->len);
		if (sub_query->req == NULL)
		{
			query->Out();
			OP_DELETE(query);
			return OpStatus::ERR_NO_MEMORY;
		}
		op_memcpy(sub_query->req, tmp, sub_query->len);
	}

	if (!FindQuery(index))
	{
		m_resend_timers.Get(index)->Start(RESEND_TIME);
	}

	query->index = index;
	query->Into(&m_queries);

	return SendQuery(query);
}

void
PiOpHostResolverManager::RemoveQueriesForResolver(class PiOpHostResolver* resolver)
{
	for (Query* query = (Query*)m_queries.First(); query;)
	{
		Query* suc = (Query*)query->Suc();
		if (query->resolver == resolver)
		{
			query->Out();
			OP_DELETE(query);
		}

		query = suc;
	}
}

BOOL
PiOpHostResolverManager::FindQuery(UINT id, Query*& query, SubQuery*& sub_query)
{
	for (query = (Query*)m_queries.First(); query; query = (Query*)query->Suc())
	{
		for (sub_query = (SubQuery*)query->sub_queries.First(); sub_query; sub_query = (SubQuery*)sub_query->Suc())
		{
			if (sub_query->id == id)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL
PiOpHostResolverManager::FindQuery(int index)
{
	Query *query;

	for (query = (Query*)m_queries.First(); query; query = (Query*)query->Suc())
	{
		if (query->index == index)
		{
			return TRUE;
		}
	}

	return FALSE;
}

void
PiOpHostResolverManager::OnSocketDataReady(OpUdpSocket* socket)
{
	unsigned char mybuff[1024]; /* ARRAY OK 2009-04-24 johanh */
	int index;

	index = m_sockets.Find(socket);
	/* Expect a segfault below if this fails: the (wrapped?) socket we created
	 * != the socket that called this method */
	OP_ASSERT(index >= 0);

	UINT recv_len = 0;
	RETURN_VOID_IF_ERROR(socket->Receive(mybuff, sizeof(mybuff), m_remote_addr, &recv_len));
	if (recv_len <= 12)
	{
		return;
	}
	OP_ASSERT(recv_len <= sizeof(mybuff));
	if (recv_len > 1024)
		recv_len = 1024;

	UINT id = ((mybuff[0] << 8) | mybuff[1]);

	Query* query = NULL;
	SubQuery* sub_query = NULL;
	if (!FindQuery(id, query, sub_query))
		return;

	int qcount =  (mybuff[4] << 8) | mybuff[5];
	int ancount = (mybuff[6] << 8) | mybuff[7];
	int i;
	unsigned char* p = mybuff + 12; // p is an array of length 1012
	unsigned int count = 12;

	for (i = 0; i < qcount && count < recv_len; ++i)
	{
		count += skip_domain( &p, recv_len-count );
		p += 4;
		count += 4;
	}
	OP_ASSERT(count < recv_len);
	if (count >= recv_len)
		return;

	UINT address_count = 0;
	for (i = 0; i < ancount && count < recv_len; ++i)
	{
		count += skip_domain( &p, recv_len-count );
		unsigned int type, len;
		if (count + 10 >= recv_len)
			return;
		type = get_short( &p );
		p += 2; // cl = get_short( &p );
		p += 4; // ttl = get_int( &p );
		len = get_short( &p );
		count += 10;
		if (type == T_A)
		{
			if (count+4 >= recv_len)
				return;
			sub_query->m_addresses[address_count] = get_int(&p);
			p -= 4;
			++address_count;
			if (address_count == MAX_RESULTS)
				break;
		} // else ... handle IPv6 records, too ?
		p += len;
		count += len;
	}
	OP_ASSERT(count < recv_len);
	if (count >= recv_len)
		return;

	sub_query->m_address_count = address_count;

	SubQuery* s;
	for (s = (SubQuery*)query->sub_queries.First(); s != sub_query; s = (SubQuery*)s->Suc())
	{
		// Must wait for results from queries sent before this sub_query
		if (!s->HasResult())
			return;
	}

	BOOL success = address_count && sub_query->m_addresses[0];

	BOOL got_all_results = TRUE;
	for (s = (SubQuery*)query->sub_queries.First(); s; s = (SubQuery*)s->Suc())
		if (!s->HasResult())
		{
			got_all_results = FALSE;
			break;
		}

	// Got result from all earlier queries
	if (success || got_all_results)
		ReportResult(query);

	if (!FindQuery(index))
		m_resend_timers.Get(index)->Stop();
}

void
PiOpHostResolverManager::ReportResult(Query* query)
{
	SubQuery* sub_query;
	for (sub_query = (SubQuery*)query->sub_queries.First(); sub_query; sub_query = (SubQuery*)sub_query->Suc())
	{
		if (sub_query->m_address_count > 0)
			break;
	}

	PiOpHostResolver* resolver = query->resolver;
	BOOL success = sub_query && sub_query->m_address_count && sub_query->m_addresses[0];
	if (success)
	{
		resolver->SetAddresses(sub_query->m_addresses, sub_query->m_address_count);
	}

	query->Out();
	OP_DELETE(query);

	if (success)
	{
		resolver->SetDnsAddressIndex(0);
		resolver->GetListener()->OnHostResolved(resolver);
	}
	else
	{
		resolver->SetDnsAddressIndex(resolver->GetDnsAddressIndex()+1);
		if (m_dns_addresses.GetCount() > resolver->GetDnsAddressIndex())
		{
		    MakeNewQuery(resolver->GetHostname(), resolver, resolver->GetDnsAddressIndex());
		}
		else
		{
			resolver->SetDnsAddressIndex(0);
			if (g_op_system_info->GetExternalResolvePolicy() == OpSystemInfo::RESOLVE_EXTERNAL_LAST &&
				CheckExternalResolver(resolver->GetHostname(), resolver))
			{
				resolver->GetListener()->OnHostResolved(resolver);
				return;
			}
			resolver->GetListener()->OnHostResolverError(resolver, OpHostResolver::HOST_ADDRESS_NOT_FOUND);
		}
	}
}

OP_STATUS
PiOpHostResolverManager::SendQuery(Query* query)
{
	void *data = NULL;
	OpSocketAddress *dns_address = NULL;
	for (SubQuery* sub_query = (SubQuery*)query->sub_queries.First(); sub_query; sub_query = (SubQuery*)sub_query->Suc())
	{
		if (!sub_query->HasResult())
		{
			OpUdpSocket *socket = m_sockets.Get(sub_query->index);
			RETURN_IF_ERROR(m_sockets_address.GetData(socket, &data));
			dns_address = static_cast<OpSocketAddress *>(data);
			RETURN_IF_ERROR(socket->Send(sub_query->req, sub_query->len, dns_address));
		}
	}

	return OpStatus::OK;
}

void
PiOpHostResolverManager::OnTimeOut(OpTimer* timer)
{
	Head dump;
	Query* query;
	int index;

	index = m_resend_timers.Find(timer);
		// expect a segfault below if this fails
	OP_ASSERT(index >= 0);

	for (query = (Query*)m_queries.First(); query;)
	{
		Query* suc = (Query*)query->Suc();

		if (query->index == index)
		{
			SubQuery* sub_query;
			for (sub_query = (SubQuery*)query->sub_queries.First(); sub_query;)
			{
				SubQuery* suc = (SubQuery*)sub_query->Suc();
				if (!sub_query->HasResult())
				{
					++sub_query->resend_count;
					if (sub_query->resend_count == RESEND_TIMEOUT)
					{
						sub_query->Out();
						OP_DELETE(sub_query);
					}
				}
				sub_query = suc;
			}

			// List of sub queries is empty, all have timed out.
			if (query->sub_queries.Empty())
			{
				query->Out();
				query->Into(&dump);
			}
			else
			{
				BOOL got_all_results = FALSE;
				for (sub_query = (SubQuery*)query->sub_queries.First(); sub_query; sub_query = (SubQuery*)sub_query->Suc())
				{
					if (sub_query->HasResult())
						got_all_results = TRUE;
					else
					{
						got_all_results = FALSE;
						break;
					}
				}

				if (got_all_results)
				{
					ReportResult(query);
				}
				else
				OpStatus::Ignore(SendQuery(query));
			}
		}

		query = suc;
	}

	for (query = (Query*)dump.First(); query; query = (Query*)query->Suc())
	{
		PiOpHostResolver* resolver = query->resolver;
		resolver->SetDnsAddressIndex(resolver->GetDnsAddressIndex()+1);
		if (m_dns_addresses.GetCount() > resolver->GetDnsAddressIndex())
		{
			m_preferred_dns = resolver->GetDnsAddressIndex();
			MakeNewQuery(resolver->GetHostname(), resolver, resolver->GetDnsAddressIndex());
		}
		else
		{
			resolver->SetDnsAddressIndex(0);
			m_preferred_dns = 0;
			if (g_op_system_info->GetExternalResolvePolicy() == OpSystemInfo::RESOLVE_EXTERNAL_LAST &&
				CheckExternalResolver(resolver->GetHostname(), resolver))
			{
				resolver->GetListener()->OnHostResolved(resolver);
			}
			else
			{
				resolver->GetListener()->OnHostResolverError(query->resolver, OpHostResolver::TIMED_OUT);
			}
		}
	}
	dump.Clear();

	for (query = (Query*)m_queries.First(); query;)
	{
		Query* suc = (Query*)query->Suc();
		if (query->index == index)
		{
			m_resend_timers.Get(index)->Start(RESEND_TIME);
		}
		query = suc;
	}
}


// PiOpHostResolver

OP_STATUS
OpHostResolver::Create(OpHostResolver** host_resolver, OpHostResolverListener* listener)
{
	if (!g_pi_host_resolver_manager)
	{
		g_pi_host_resolver_manager = OP_NEW(PiOpHostResolverManager, ());
		if (!g_pi_host_resolver_manager)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	PiOpHostResolver* h = OP_NEW(PiOpHostResolver, (listener));
	if (h == NULL)
		return OpStatus::ERR_NO_MEMORY;
	*host_resolver = h;

	return OpStatus::OK;
}

PiOpHostResolver::PiOpHostResolver(OpHostResolverListener* listener) :
	m_listener(listener),
	m_address_count(0),
	m_index(0),
	m_hostname(NULL)
{
}

PiOpHostResolver::~PiOpHostResolver()
{
	if (g_pi_host_resolver_manager)
	{
		g_pi_host_resolver_manager->RemoveQueriesForResolver(this);
	}

	op_free(m_hostname);
}

OP_STATUS
PiOpHostResolver::Resolve(const uni_char* hostname)
{
	if (m_hostname)
		op_free(m_hostname);
	m_hostname = uni_strdup(hostname);

	if (m_hostname == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(g_pi_host_resolver_manager);
	return g_pi_host_resolver_manager->MakeNewQuery(hostname, this);
}

void
PiOpHostResolver::SetAddresses(unsigned int* addresses, UINT num)
{
	for (UINT i=0; i < num; ++i)
		m_addresses[i] = addresses[i];
	m_address_count = num;
}

BOOL
PiOpHostResolver::SetAddress(OpSocketAddress* addr)
{
	OpString str;
	if (OpStatus::IsSuccess(addr->ToString(&str)))
	{
		UINT a, b, c, d;
		if (uni_sscanf(str.CStr(), UNI_L("%u.%u.%u.%u"), &a, &b, &c, &d) == 4)
		{
			m_addresses[0] = ((((UINT) a) << 24) + (((UINT) b) << 16) + (((UINT) c) << 8) + d);
			m_address_count = 1;
			return TRUE;
		}
	}
	return FALSE;
}

#ifdef SYNCHRONOUS_HOST_RESOLVING
OP_STATUS
PiOpHostResolver::ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, Error* error)
{
	OP_ASSERT(!"PiOpHostResolver::ResolveSync(...) is not implemented.");
	return OpStatus::ERR;
}
#endif // SYNCHRONOUS_HOST_RESOLVING

OP_STATUS
PiOpHostResolver::GetLocalHostName(OpString* local_hostname, Error* error)
{
	OP_ASSERT(!"PiOpHostResolver::GetLocalHostName(...) is not implemented.");
	return OpStatus::ERR;
}

UINT
PiOpHostResolver::GetAddressCount()
{
	OP_ASSERT(m_address_count > 0);
	return m_address_count;
}

OP_STATUS
PiOpHostResolver::GetAddress(OpSocketAddress* socket_address, UINT index)
{
	OP_ASSERT(index < m_address_count);
	if (index >= m_address_count)
		return OpStatus::ERR;

	int addr = m_addresses[index];

	unsigned char a, b, c, d;
	a = ((unsigned char) ((addr) >> 24));
	b = ((unsigned char) ((addr) >> 16));
	c = ((unsigned char) (((UINT16) (addr)) >> 8));
	d = (unsigned char)addr;

	uni_char buf[16]; /* ARRAY OK 2009-04-24 johanh */
	uni_sprintf(buf, UNI_L("%d.%d.%d.%d"), a, b, c, d);
	socket_address->FromString(buf);

	return OpStatus::OK;
}

/*
 * Utility functions to generate and parse DNS-queries.
 *
 * Note: Not really readable.
 */

unsigned char*
PiOpHostResolverManager::MakeQuery(short id, const uni_char* req, int C, int Q, int* len)
{
	unsigned char* rp = m_query_buffer;
	const uni_char* p = req;

	// rd = 1
	// qd = [ name: req, cl: C, type: Q ]
	*(rp++)= id>>8; *(rp++) = id&255;
	*(rp++)= 1;
	*(rp++)= 0;

	*(rp++)= 0; *(rp++)= 1; // num reqs.
	*(rp++)= 0; *(rp++)= 0;
	*(rp++)= 0; *(rp++)= 0;
	*(rp++)= 0; *(rp++)= 0;

	for (int i=0; 1; ++i)
	{
		if( p[i] == '.' || p[i] == '\0' )
		{
			*(rp++) = i;
			for( int j = 0; j<i; j++ )
				*(rp++) = (unsigned char)p[j];
			if( p[i] == 0 ) break;
			p += i+1;
			i = 0;
		}
	}
	*rp++=0;
	*(rp++)= Q>>8; *(rp++) = Q&255;
	*(rp++)= C>>8; *(rp++) = C&255;

	*len = rp-m_query_buffer;

	return m_query_buffer;
}

static int get_int(unsigned char** p)
{
	int res = 0;
	res |= *((*p)++); res<<=8;
	res |= *((*p)++); res<<=8;
	res |= *((*p)++); res<<=8;
	res |= *((*p)++);
	return res;
}

static int get_short(unsigned char** p)
{
	int res = 0;
	res |= *((*p)++); res<<=8;
	res |= *((*p)++);
	return res;
}

static unsigned int skip_domain( unsigned char** _p, unsigned int max_len)
{
	int l;
	unsigned char* p = *_p;
	unsigned int count = 0;
	while( (l = *(p++)) && count < max_len)
	{
		count++;
		if( l & 0xc0 )
		{
			p++;
			count++;
			break;
		}
		else
		{
			count += l;
			p+=l;
		}
	}
	if (count < max_len)
		*_p = p;
	return count;
}

#endif // PI_HOST_RESOLVER
