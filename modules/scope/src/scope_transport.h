/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef SCOPE_TRANSPORT_H
#define SCOPE_TRANSPORT_H

#include "modules/scope/scope_client.h"
#include "modules/util/OpHashTable.h"
#include "modules/scope/src/scope_tp_message.h"
#include "modules/scope/src/scope_service.h"

class OpProtobufInstanceProxy;
class OpScopeClient;
class OpScopeHostManager;

/**
 * Defines a scope host which is meant to be attached to a scope client.
 * The host will receive commands from the client and will send back responses
 * events and errors.
 *
 * The client will send messages to the host via the Receive() callback.
 *
 * If the host should be destroyed it is recommended to use ScheduleDestruction(),
 * this will delay destruction until the manager gets back control. This is usually
 * in the next message run.
 *
 * @note There is currently only meant to be one client attached to a host.
 *       Any new client will replace the existing one.
 */
class OpScopeHost
	: public ListElement<OpScopeHost>
{
public:
	OpScopeHost(OpScopeHostManager *host_manager = NULL);
	virtual ~OpScopeHost();

	/**
	 * Set the current host manager this host is related to.
	 */
	void SetHostManager(OpScopeHostManager *manager) { host_manager = manager; }
	/**
	 * @return The host manager this host is related to or @c NULL.
	 */
	OpScopeHostManager *GetHostManager() const { return host_manager; }

	/**
	 * Attach a client object to this host. If the host is already attached
	 * to a client it will first detach the original client before attaching to the
	 * new one.
	 * Once the client is attached it will call OnClienAttached().
	 */
	virtual OP_STATUS AttachClient(OpScopeClient *client);
	/**
	 * Detach the current client object from this host. If no client is attached
	 * nothing happens.
	 * Once the client is detached it will call OnClientDetached().
	 */
	virtual OP_STATUS DetachClient();

	/**
	 * Prepare for destruction of the host. It will be placed in a queue
	 * and deleted when execution returns to OpScopeManager.
	 * This approach allows hosts to delete itself in a safer manner.
	 * @note Only use this when a scope manager is connected.
	 */
	void ScheduleDestruction();

	/**
	 * Will be called when a new client is attached to the host.
	 * @note The pointers for host and client are setup prior to calling this.
	 */
	virtual OP_STATUS OnClientAttached(OpScopeClient *client) = 0;
	/**
	 * Will be called when an existing client is detached from the host.
	 * @note The pointers for host and client are reset prior to calling this.
	 */
	virtual OP_STATUS OnClientDetached(OpScopeClient *client) = 0;

	/**
	 * @return The client object which this host is attached to, or @c NULL if no client is attached.
	 */
	OpScopeClient *GetClient() const { return client; }

	/**
	 * Returns a string identifying the type of host, e.g. "builtin" or "remote"
	 */
	virtual const uni_char *GetType() const = 0;

	/**
	 * Return the version of the host, e.g 0 for STP/0 and 1 for STP/1.
	 * Sub-class must implement this to return the proper value.
	 */
	virtual int GetVersion() const = 0;

	/**
	 * Called whenever the clients sends a message to this host.
	 */
	virtual OP_STATUS Receive( OpScopeClient *client, OpAutoPtr<OpScopeTPMessage> &message ) = 0;
	/**
	 * Called whenever the client needs to be (re)configured in the host.
	 * If the parameter @a immediate is to to @c TRUE when the call returns it means that the client
	 * should not need to wait a response. This most likely happens for local host objects, but not
	 * for remote (network) hosts.
	 *
	 * @param client The client object which requested the configure call.
	 * @param tag The tag to use when sending the configure message. Only used if @a immediate returns @c FALSE.
	 * @param immediate This will be set to @c TRUE by the host if the client was immediately configured.
	 */
	virtual OP_STATUS ConfigureClient( OpScopeClient *client, unsigned int tag, BOOL &immediate ) = 0;

private:
	friend class OpScopeHostManager; // For SetClient

	void SetClient(OpScopeClient *c) { client = c; }

	OpScopeHostManager *host_manager;
	OpScopeClient *client;
};

/**
 * A common API between services. An instance of this class will be set in
 * active services. It provides a way to find other services by name with
 * FindService() as well as a way to iterate over all registered services
 * with GetServices().
 *
 * Registration of services is done with RegisterService() and UnregisterService().
 *
 * GetCoreVersion() is provided as a convenient way to get the core version.
 */
class OpScopeServiceManager
{
public:
	OpScopeServiceManager();
	~OpScopeServiceManager();

	/**
	 * Provides a range over a list of services.
	 * Call Front() to access the front-most service. While IsEmpty() returns
	 * @c FALSE one can call PopFront() to advance the front to the next
	 * service (ie. shortening the range).
	 *
	 * The range keeps track of the next service so it is possible to remove
	 * the current service from the list while iterating.
	 *
	 * A typical loop like like:
	 * @code
	 * ServiceRange r;
	 * for (; !r.IsEmpty(); r.PopFront())
	 *     r.Front();
	 * @endcode
	 */
	class ServiceRange
	{
	public:
		ServiceRange(const List<OpScopeService> &list)
			: front(list.First())
			, next(front ? front->Suc() : NULL)
		{
		}

		/**
		 * @return @c TRUE when the range is empty.
		 */
		BOOL IsEmpty() const { return !front; }
		/**
		 * @return The front-most service object.
		 * @note Only call when IsEmpty() returns @c FALSE.
		 */
		OpScopeService *Front() const { return front; }

		/**
		 * Advance the front of the range, ie. shortening it by one.
		 * @note Only call when IsEmpty() returns @c FALSE.
		 */
		void PopFront();

	private:
		OpScopeService *front, *next;
	};

	/**
	 * Returns a range object which spans all registered services.
	 */
	ServiceRange GetServices() const { return ServiceRange(services); }

	/**
	 * Finds the service named @a name and returns the service object. If no
	 * service was found it returns @c NULL.
	 */
	OpScopeService *FindService(const OpString &name) const;

	/**
	 * Finds the service named @a name and returns the service object. If no
	 * service was found it returns @c NULL.
	 */
	OpScopeService *FindService(const uni_char *name) const;

	/**
	 * Finds the service named @a name and returns the service object. If no
	 * service was found it returns @c NULL.
	 *
	 * @note This will only find services with names that are all ASCII
	 *       characters, which is normally not an issue.
	 */
	OpScopeService *FindService(const char *name) const;

	/**
	 * Registers a new service in the manager. If the service is already
	 * registered in a different manager it is first unregistered from
	 * that manager.
	 */
	void RegisterService(OpScopeService *service);
	/**
	 * Unregisters the service from this manager.
	 */
	void UnregisterService(OpScopeService *service);

	/**
	 * Disables all registered services.
	 */
	OP_STATUS DisableServices();

	/**
	 * Convenience function for returning the core version.
	 */
	const OpString &GetCoreVersion() const { return core_version; }

private:
	/**
	 * \brief A list of all services in the host
	 *
	 * All registered scope services are listed here.
	 */
	List<OpScopeService> services;
	OpString core_version; // holds core version
};

/**
 * @copydoc OpScopeServiceManager::FindService(const char *)
 *
 * The returned service object is automatically cast to the specified
 * service class.
 *
 * @note This function cannot be placed as a member of OpScopeServiceManager due to limitations in the ADS compiler
 */
template<typename T>
T *OpScopeFindService(OpScopeServiceManager *manager, const char *name)
{
	OP_ASSERT(manager);
	return static_cast<T *>(manager->FindService(name));
}

/**
 * Defines the builtin host in scope. It also acts a service manager and contains
 * all the registered services.
 */
class OpScopeBuiltinHost
	: public OpScopeHost
	, public OpScopeServiceManager
{
public:
	OpScopeBuiltinHost() {};
	virtual ~OpScopeBuiltinHost() {}

	// From OpScopeHost

	virtual OP_STATUS OnClientAttached(OpScopeClient *client);
	virtual OP_STATUS OnClientDetached(OpScopeClient *client);

	const uni_char *GetType() const { return UNI_L("builtin"); }

	virtual int GetVersion() const;

	virtual OP_STATUS Receive( OpScopeClient *client, OpAutoPtr<OpScopeTPMessage> &message );
	virtual OP_STATUS ConfigureClient( OpScopeClient *client, unsigned int tag, BOOL &immediate );
};

#endif // SCOPE_TRANSPORT_H
