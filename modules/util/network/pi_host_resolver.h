/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_NETWORK_PI_HOST_RESOLVER_H
#define MODULES_UTIL_NETWORK_PI_HOST_RESOLVER_H

#ifdef PI_HOST_RESOLVER

#include "modules/pi/network/OpHostResolver.h"
#include "modules/pi/network/OpUdpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/util/simset.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

#define MAX_RESULTS     10 // Max number of IP addresses returned
#define RESEND_TIME    500 // Timer interval (0.5 seconds)
#define RESEND_TIMEOUT  60 // times RESEND_TIME for timeout value (30 seconds)

/**
 * Used by PiOpHostResolver to make DNS-queries.
 */
class PiOpHostResolverManager: private OpUdpSocketListener, private OpTimerListener
{
public:
	PiOpHostResolverManager();
	~PiOpHostResolverManager();

	OP_STATUS MakeNewQuery(const uni_char* hostname, class PiOpHostResolver* resolver);

	/**
	 * Issue a new query. To DNS-server number 'index' where 0 is the
	 * primary DNS-server, 1 is the secondary DNS-server and so on.
	 * Calls ReadDnsAddresses if no DNS.
	 *
	 * @param hostname Hostname to lookup. Can not be more than 450
	 * characters in length.
	 *
	 * @param resolver a pointer to a resolver object for this
	 * particular lookup.
	 *
	 * @param index which DNS-server to use.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM. OpStatus::ERR if no
	 * DNS-servers where found, if hostname is more than 450
	 * characters long, if either hostname or resolver is NULL or if
	 * DNS-server with index 'index' do not exist. Otherwise
	 * OpStatus::OK.
	 */
	OP_STATUS MakeNewQuery(const uni_char* hostname, class PiOpHostResolver* resolver, unsigned int index);
	void RemoveQueriesForResolver(class PiOpHostResolver* resolver);

	UINT GetPreferredDns() { return m_preferred_dns; }
	void SetPreferredDns(UINT preferred_dns) { m_preferred_dns = preferred_dns; }

	/**
	 * Get DNS-addresses from the system.
	 */
	OP_STATUS ReadDnsAddresses();
private:
	// OpUdpSocketListener
	void OnSocketDataReady(OpUdpSocket* socket);

	// OpTimerListener
	void OnTimeOut(OpTimer* timer);

	void Clear();

	class Query : public Link
	{
	public:
		Query(PiOpHostResolver* resolver) : resolver(resolver) {}
		~Query() { sub_queries.Clear(); }

		PiOpHostResolver* resolver;
		Head sub_queries;
		int index;
	};

	class SubQuery : public Link
	{
	public:
		SubQuery() : req(NULL), len(0), id(0), resend_count(0), m_address_count(-1) {}
		~SubQuery() { OP_DELETEA(req); }

		BOOL HasResult() const { return m_address_count != -1; }

		unsigned char* req;
		int len;
		UINT16 id;
		UINT resend_count;

		unsigned int m_addresses[MAX_RESULTS];
		int m_address_count;
		int index;
	};

	OP_STATUS SendQuery(Query* query);
	unsigned char* MakeQuery(short id, const uni_char *req, int C, int Q, int* len);
	void ReportResult(Query* query);
	BOOL FindQuery(UINT id, Query*& query, SubQuery*& sub_query);
	BOOL FindQuery(int index);

	OpVector<OpUdpSocket> m_sockets;
	OpHashTable m_sockets_address;
	OpVector<OpTimer> m_resend_timers;
	OpVector<OpSocketAddress> m_dns_addresses;
	UINT m_resend_count;
	Head m_queries;
	UINT16 m_current_id;
	unsigned char m_query_buffer[512]; /* ARRAY OK 2009-04-24 janv */
	OpString m_dns_suffixes;
	UINT m_preferred_dns;
	OpSocketAddress *m_remote_addr, *m_local_addr;
};

/**
 * PiOpHostResolver handles hostname lookup for one host. To do this
 * PiOpHostResolverManager is used. PiHostResolverManager handles each
 * specific DNS-query. If the primary DNS-server answer that it does not
 * recognize the host or any DNS-server that may know the answer then
 * the secondary DNS-server is tried and so on. When there are no more
 * DNS-servers to try the PiHostResolverManager reports
 * OpHostResolver::HOST_ADDRESS_NOT_FOUND to the listener associated
 * with this resolver object.
 *
 * If the DNS-server does not answer and the request timeout then the
 * secondary DNS-server is tried and so on. After a timeout the first
 * server in order that does answer will be used as preferred DNS and
 * used in subsequent queries. If no DNS answers the preferred address
 * will be set to the primary DNS again.
 *
 */
class PiOpHostResolver : public OpHostResolver
{
public:
	PiOpHostResolver(OpHostResolverListener* listener);
	~PiOpHostResolver();

	/**
	 * Set which DNS-server to use. 0 for primary, 1 for secondary and
	 * so on.
	 */
	void SetDnsAddressIndex(unsigned int new_index) { m_index = new_index; }

	/**
	 * Get which DNS-server to use.
	 */
	unsigned int GetDnsAddressIndex() { return m_index; }

	/**
	 * Start resolving a hostname.
	 */
	OP_STATUS Resolve(const uni_char* hostname);

#ifdef SYNCHRONOUS_HOST_RESOLVING
	OP_STATUS ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, Error* error);
#endif // SYNCHRONOUS_HOST_RESOLVING

	OP_STATUS GetLocalHostName(OpString* local_hostname, Error* error);
	UINT GetAddressCount();
	OP_STATUS GetAddress(OpSocketAddress* socket_address, UINT index);

	OpHostResolverListener* GetListener() const { return m_listener; }

	void SetAddresses(unsigned int* addresses, UINT num);
	BOOL SetAddress(OpSocketAddress* address);

	const uni_char *GetHostname() { return m_hostname; }
private:
	OpHostResolverListener* m_listener;

	unsigned int m_addresses[MAX_RESULTS];
	UINT m_address_count;
	int m_index;
	uni_char *m_hostname;
};

#endif // PI_HOST_RESOLVER

#endif // !MODULES_UTIL_NETWORK_PI_HOST_RESOLVER_H
