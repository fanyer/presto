/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_CONNECTION_MANAGER_H
#define OP_SCOPE_CONNECTION_MANAGER_H

#ifdef SCOPE_SUPPORT

class OpScopeClient;

/**
 * Controls and creates connections to Opera instances. These functions will
 * fail with OpStatus::ERR if used after the scope module has been destroyed,
 * or before the scope module has initialized.
 */
class OpScopeConnectionManager
{
public:

#ifdef SCOPE_CONNECTION_CONTROL
	enum ConnectionStatus
	{
		/**
		 * Socket is connected.
		 */
		CONNECTION_OK
		/**
		 * Connection failed or socket closed.
		 */
		, CONNECTION_FAILED
		/**
		 * Not used.
		 */
		, CONNECTION_TIMEDOUT
		/**
		 * Disconnect after being connected.
		 */
		, DISCONNECTION_OK
		/**
		 * Not used.
		 */
		, DISCONNECTION_FAILED
		/**
		 * Socket is closed.
		 */
		, CONNECTION_CLOSED
	};
#endif // SCOPE_CONNECTION_CONTROL

	/**
	 * Prevent scope from connecting to a proxy, and disconnect if already connected.
	 * This is functionally the same as turning off all Scope features, but without
	 * the benefit of smaller footprint. Only use this function if you cannot decide
	 * at compile time if you need scope or not. Default is ENABLED.
	 *
	 * @return OpStatus::OK if disabled successfully, OpStatus::ERR if the Scope
	 *         module was not yet initialized or has been destroyed.
	 */
	static OP_STATUS Disable();

	/**
	 * Enables Scope to connect to a proxy. This is by default the case, so the
	 * function is only needed if Disable() was called previously.
	 *
	 * @return OpStatus::OK if enabled successfully, OpStatus::ERR if the Scope
	 *         module was not yet initialized or has been destroyed.
	 */
	static OP_STATUS Enable();

	/**
	 * Connects an OpScopeClient to this Opera instance. Will disconnect the
	 * existing client, if any.
	 *
	 * @param client The OpScopeClient to connect. Can not be NULL.
	 * @return OpStatus::OK if the OpScopeClient connected successfully.
	 */
	static OP_STATUS Connect(OpScopeClient *client);

	/**
	 * Connects an OpScopeClient to some Opera instance over a socket. A socket
	 * is opened on the specified port, awaiting connection from the other
	 * Opera instance.
	 *
	 * @param client The OpScopeClient to connect. Can not be NULL.
	 * @param port The port to open the socket on (e.g. 7001).
	 * @return OpStatus::OK if the socket was successfully opened.
	 */
	static OP_STATUS Listen(OpScopeClient *client, int port);

	/**
	 * Disconnects an OpScopeClient.
	 *
	 * @param client The OpScopeClient to disconnect. Can not be NULL.
	 * @return OpStatus::OK if a client was disconnected, or if there was no
	 *         client to disconnect. OpStatus::ERR if no disconnection could be
	 *         attempted.
	 */
	static OP_STATUS Disconnect(OpScopeClient *client);

	/**
	 * Check whether an OpScopeClient is connected to a host.
	 *
	 * @return TRUE if debugger is connected, false otherwise.
	 */
	static BOOL IsConnected();

	/**
	 * Returns the current IP address of the listener in OpScopeNetworkServer or
	 * one of the system addresses via OpSystemInfo::GetSystemIp().
	 *
	 * @param[out] address An OpString to store the address in.
	 * @return OpStatus::OK if @a address received the address, or any other error OpString.Set()
	 *         can return if it fails.
	 */
	static OP_STATUS GetListenerAddress(OpString &address);

}; // OpScopeConnectionManager

#endif // SCOPE_SUPPORT

#endif // OP_SCOPE_CONNECTION_MANAGER_H
