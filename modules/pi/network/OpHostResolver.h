/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_HOST_RESOLVER_H
#define OP_HOST_RESOLVER_H

class OpSocketAddress;
class OpHostResolverListener;

/** @short Host name lookup operation.
 *
 * Only one request is active at any one time. When an API user wants
 * to interrupt the resolving, it may just delete the resolver object.
 */
class OpHostResolver
{
public:
	/** Enumeration of possible errors. */
	enum Error
	{
		/** No error */
		NETWORK_NO_ERROR,

		/** Secure socket error handled */
		ERROR_HANDLED,

		/** @deprecated Don't use */
		CANNOT_FIND_NETWORK,

		/** @deprecated Don't use */
		HOST_NOT_AVAILABLE,

		/** The specified host name was not found and could not be resolved */
		HOST_ADDRESS_NOT_FOUND,

		/** @deprecated Don't use */
		DNS_NOT_FOUND,

		/** Operation timed out */
		TIMED_OUT,

		/** Network link error */
		NETWORK_ERROR,

		/** Network went down while attempting to resolve the host name */
		INTERNET_CONNECTION_CLOSED,

		/** The system is temporarily without network access */
		OUT_OF_COVERAGE,

		/** @deprecated Don't use */
		NO_SOCKETS,

		/** Out of memory.
		 *
		 * Should only be used when an asynchronous operation runs out
		 * of memory. In other cases, return OpStatus::ERR_NO_MEMORY
		 * from the method that ran out of memory is the right thing
		 * to do.
		 */
		OUT_OF_MEMORY
	};

private:
	/** Create and return an OpHostResolver object.
	 *
	 * @param host_resolver (out) Set to the host resolver created
	 * @param listener Listener for the resolver
	 *
	 * NOTE: this method should only be called by SocketWrapper::CreateHostResolver.
	 * All core code should use SocketWrapper::CreateHostResolver instead of calling
	 * this directly.
	 *
	 * @return OK or ERR_NO_MEMORY
	 *
	 */
	static OP_STATUS Create(OpHostResolver** host_resolver, OpHostResolverListener* listener);

	friend class SocketWrapper;

public:
	/** Destructor.
	 *
	 * If a host resolve operation is still in progress at this point, it must
	 * be terminated. Listener methods are not to be called.
	 */
	virtual ~OpHostResolver() {}

	/** Start resolving a hostname to (one or several) socket address(es).
	 *
	 * If and when the hostname is resolved successfully,
	 * OpHostResolverListener::OnHostResolved() is called. On error,
	 * OpHostResolverListener::OnHostResolverError() is called.
	 *
	 * This is the only method in this API that may call listener
	 * methods. Listener methods may be called synchronously from this method
	 * directly, or asynchronously if it needs to wait the network data and
	 * results (which is the typical case).
	 *
	 * A resolve operation is concluded with either calling a listener method,
	 * or by returning OpStatus::ERR or OpStatus::ERR_NO_MEMORY, whichever
	 * comes first.
	 *
	 * Multiple calls to Resolve() on the same object are allowed, but only one
	 * operation at a time, meaning that before calling Resolve() a second
	 * time, the first operation must be concluded first. Behavior when failing
	 * to follow this rule is undefined.
	 *
	 * While a host resolve operation is in progress, all methods in this API,
	 * except for Resolve(), GetAddressCount() and GetAddress() may be called.
	 *
	 * @param hostname The hostname to resolve
	 *
	 * @return OK, ERR or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS Resolve(const uni_char* hostname) = 0;

#ifdef SYNCHRONOUS_HOST_RESOLVING
	/** Resolve a given hostname into a host address - synchronously.
	 *
	 * @deprecated This is a rather crappy method, as it, apart from blocking
	 * while host resolving is in progress, doesn't support multiple result
	 * addresses. Use Resolve() instead.
	 *
	 * @param hostname The hostname to resolve
	 * @param socket_address (out) Set to the resulting host address. Must be
	 * ignored unless OpStatus::OK is returned.
	 * @param error (out) Set to an error code if an error occurred. Must be
	 * ignored unless OpStatus::ERR is returned.
	 *
	 * @return OK, ERR or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, Error* error) = 0;
#endif // SYNCHRONOUS_HOST_RESOLVING

	/** Get the local machine name.
	 *
	 * This method will block while getting the local host name.  Note that
	 * Resolve() is likely to yield poor results on the name returned here; if
	 * you need the associated IP address, see OpSystemInfo::GetSystemIp(),
	 * instead.
	 *
	 * @param local_hostname (out) Set to the resulting name. Must be ignored
	 * unless OpStatus::OK is returned.
	 * @param error (out) Set to an error code if an error occurred. Must be
	 * ignored unless OpStatus::ERR is returned.
	 *
	 * @return OK, ERR or ERR_NO_MEMORY
	 */
	virtual OP_STATUS GetLocalHostName(OpString* local_hostname, Error* error) = 0;

	/** Get the number of resolved addresses available from the last resolve operation.
	 *
	 * This number will be reset by a call to Resolve().
	 *
	 * @return The resulting number of resolved addresses. Is 0 initially, and
	 * when host resolving fails.
	 */
	virtual UINT GetAddressCount() = 0;

	/** Get an address from the last resolve operation.
	 *
	 * These are the addresses that resulted from the last Resolve()
	 * operation. They will be reset by a call to Resolve().
	 *
	 * @param socket_address (out) Will be set to the address at the specified
	 * index. Must be ignored unless OpStatus::OK is returned.
	 * @param index Address index. Must be less than the value returned from
	 * GetAddressCount(). The first address is at index 0, the second at index
	 * 1, and so on. Addresses are sorted in order of preference.
	 *
	 * @return OK if successful, ERR if index is out of range, ERR_NO_MEMORY if
	 * memory allocation failed.
	 */
	virtual OP_STATUS GetAddress(OpSocketAddress* socket_address, UINT index) = 0;
};

/** Listener for OpHostResolver.
 *
 * Note: Unless explicitly mentioned, any method call on an object of this
 * class may cause the calling host resolver to be deleted. Therefore, make
 * sure that there is nothing on the call stack referencing the host resolver
 * after the listener method has returned.
 */
class OpHostResolverListener
{
public:
	virtual ~OpHostResolverListener() {}

	/** The host name has been resolved to an address successfully.
	 *
	 * This concludes an host resolve operation.
	 *
	 * At this point the API user should probably call
	 * OpHostResolver::GetAddressCount(), and pick between the available
	 * addresses got from OpHostResolver::GetAddress().
	 *
	 * When done with getting the resulting address(es), the API user may
	 * re-use the OpHostResolver object for another operation or delete it.
	 *
	 * @param host_resolver The resolver which is done resolving.
	 */
	virtual void OnHostResolved(OpHostResolver* host_resolver) = 0;

	/** Variant of OnHostResolved including TTL info.
	 *
	 * Override this function if your listener can utilize TTL information.
	 * The normal OnHostResolved (without TTL) might still be called if the
	 * platform host resolver does not provide this information. If this
	 * function is implemented instead, the implementation of the normal
	 * OnHostResolved above should call this function with ttl set to 0.
	 *
	 * @param host_resolver The resolver which is done resolving.
	 * @param ttl The number of seconds until this address must be rechecked,
	 *        or 0 if ttl information is not available.
	 */
	virtual void OnHostResolved(OpHostResolver* host_resolver, unsigned int ttl) { OnHostResolved(host_resolver); }

	/** An error has occured while resolving the host name.
	 *
	 * This concludes an host resolve operation.
	 *
	 * Now the API user may re-use the OpHostResolver object for another
	 * operation or delete it.
	 *
	 * @param host_resolver The resolver on which the error has occurred
	 * @param error The error that occurred.
	 */
	virtual void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error) = 0;
};

#ifdef PI_INTERNET_CONNECTION
#include "modules/url/url_dns_wrapper.h"
#endif // PI_INTERNET_CONNECTION

#endif	// !OP_HOST_RESOLVER_H
