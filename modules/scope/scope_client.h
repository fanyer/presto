/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_CLIENT_H
#define OP_SCOPE_CLIENT_H

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_tp_message.h"

class OpScopeClient;
class OpScopeHost;
class OpScopeService;
class OpProtobufInstanceProxy;

/**
 * A common interface for clients and hosts. It provides functionality for
 * attaching hosts and clients together and a way to schedule destruction
 * of host and client objects.
 */
class OpScopeHostManager
	: public MessageObject
{
public:
	OpScopeHostManager();
	virtual ~OpScopeHostManager();

	OP_STATUS Construct();

	/**
	 * Schedule a client object for destruction at a later time. The client
	 * will be placed in a queue and deleted when execution returns to the
	 * manager in the next message loop run.
	 */
	void ScheduleDestruction(OpScopeClient *client);
	/**
	 * Schedule a host object for destruction at a later time. The host
	 * will be placed in a queue and deleted when execution returns to the
	 * manager in the next message loop run.
	 */
	void ScheduleDestruction(OpScopeHost *host);

	/**
	 * Report a client object which is about to or is currently being destructed.
	 * This will remove the client from the destruction queue and report the
	 * incident to any sub-classes.
	 */
	void ReportDestruction(OpScopeClient *client);
	/**
	 * Report a host object which is about to or is currently being destructed.
	 * This will remove the host from the destruction queue and report the
	 * incident to any sub-classes.
	 */
	void ReportDestruction(OpScopeHost *host);

	/**
	 * Attaches a host and client to each other. Once they are attached it
	 * calls OpScopeClient::OnHostAttached and OpScopeHost::OnClientAttached.
	 */
	static OP_STATUS AttachClientHost(OpScopeClient *client, OpScopeHost *host);
	/**
	 * Detaches a host and client from each other. Once they are detached it
	 * calls OpScopeClient::OnHostDetached and OpScopeHost::OnClientDetached.
	 */
	static OP_STATUS DetachClientHost(OpScopeClient *client, OpScopeHost *host);

protected:
	/**
	 * Handles the following callback messages:
	 * - MSG_SCOPE_MANAGER_CLEANUP
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * Called whenever a host object is being destructed (or is about to).
	 * Sub-classes can re-implement this to keep track of deleted hosts.
	 */
	virtual void OnHostDestruction(OpScopeHost *host) {}
	/**
	 * Called whenever a client object is being destructed (or is about to).
	 * Sub-classes can re-implement this to keep track of deleted clients.
	 */
	virtual void OnClientDestruction(OpScopeClient *client) {}

private:
	/**
	 * Destroys all host and clients objects which are in the destruction
	 * queue. It calls OnHostDestruction() and OnClientDescruction() before
	 * the objects are actually deleted. This allows a sub-class to receive
	 * notice when clients and hosts are deleted from the system.
	 */
	void DestructAll();

	/**
	 * A list of client objects which are to be deleted. After an object is
	 * placed in the queue a message (MSG_SCOPE_MANAGER_CLEANUP) is posted to
	 * itself, the message will be picked up at the next run and any objects
	 * are deleted.
	 * @see ScheduleDestruction, HandleCallback
	 */
	List<OpScopeClient> client_dumpster;
	/**
	 * A list of host objects which are to be deleted.
	 * @see client_dumpster for more details.
	 */
	List<OpScopeHost> host_dumpster;
};

/**
 * Defines a scope client which is meant to be attached to scope host.
 * The client will send commands to the host and receive responses, events
 * and errors from the host.
 *
 * Sending commands is done with Send() and SendToHost(). The host will send
 * messages to the client via the Receive() callback.
 *
 * To setup (or reset) the client it should call Configure(). This will contact
 * the host and let it know the wanted STP format of message, it will also reset
 * any settings if the client was previously configured.
 *
 * If the client should be destroyed it is recommended to use ScheduleDestruction(),
 * this will delay destruction until the manager gets back control. This is usually
 * in the next message run.
 */
class OpScopeClient
	: public ListElement<OpScopeClient>
{
public:
	/**
	 * Initialize a client by setting the message type to OpScopeTPMessage::ProtocolBuffer
	 * and setting the host manager. The host manager can also be set at a later
	 * time with SetHostManager().
	 *
	 * @param scope_manager The host manager which is meant to manage this client.
	 */
	OpScopeClient(OpScopeHostManager *host_manager = NULL);
	/**
	 * Initialize a client by setting the message type to @a t and setting the
	 * host manager. The host manager can also be set at a later time with
	 * SetHostManager().
	 *
	 * @param host_manager The host manager which is meant to manage this client.
	 */
	OpScopeClient(OpScopeTPMessage::MessageType t, OpScopeHostManager *host_manager = NULL);
	virtual ~OpScopeClient();

	/**
	 * Set the current host manager this client is related to.
	 */
	void SetHostManager(OpScopeHostManager *manager) { host_manager = manager; }
	/**
	 * @return The host manager this client is related to or @c NULL.
	 */
	OpScopeHostManager *GetHostManager() const { return host_manager; }

	/**
	 * @return The host object which this client is attached to, or @c NULL if no host is attached.
	 */
	OpScopeHost *GetHost() const;
	/**
	 * @return The type of STP message this client expects to receive.
	 */
	OpScopeTPMessage::MessageType GetMessageType() const;
	/**
	 * Sets the message type this client expects from the host to @a type.
	 */
	void SetMessageType(OpScopeTPMessage::MessageType type);

	/**
	 * Attach a host object to this client. If the client is already attached
	 * to a host it will first detach the original host before attaching to the
	 * new one.
	 * Once the host is attached it will call OnHostAttached().
	 */
	virtual OP_STATUS AttachHost(OpScopeHost *host);
	/**
	 * Detach the current host object from this client. If no host is attached
	 * nothing happens.
	 * Once the host is detached it will call OnHostDetached().
	 */
	OP_STATUS DetachHost();

	/**
	 * Prepare for destruction of the client. It will be placed in a queue
	 * and deleted when execution returns to OpScopeHostManager.
	 * This approach allows clients to delete itself in a safer manner.
	 * @note Only use this when a host manager is connected.
	 */
	void ScheduleDestruction();

	// Helper methods
	/**
	 * Send an STP/0 message to specified service in the host.
	 * @param service Name of the service to send the message to, e.g. @c "prefs"
	 * @param msg The STP/0 message to send.
	 * @note This does not work if the communication between the host and client
	 *       is set to use STP/1.
	 */
	virtual OP_STATUS Send(const uni_char *service, const uni_char *msg);
	/**
	 * Send an STP/1 message to specified service in the host.
	 * This is meant as a convenience function for Javascript based clients.
	 *
	 * @param service Name of the service to send the message to, e.g. @c "prefs"
	 * @oaram command_id The ID of the command which should be executed in the service.
	 * @param data The Javascript data to send to the service.
	 * @param es_runtime The runtime object which owns the Javascript data.
	 * @note This does not work if the communication between the host and client
	 *       is set to use STP/0.
	 */
	virtual OP_STATUS Send(const uni_char *service, unsigned command_id, unsigned tag, ES_Object *data, ES_Runtime *es_runtime);
	/**
	 * Send an STP message to the host. It will be delivered to the specified service
	 * and command in the host.
	 *
	 * @param message The STP message.
	 */
	virtual OP_STATUS SendToHost(OpAutoPtr<OpScopeTPMessage> &message);
	/**
	 * Call this to (re)configure the client in the host.
	 * If the parameter @a immediate is to to @c TRUE when the call returns it means that the client
	 * should not need to wait a response. This most likely happens for local host objects, but not
	 * for remote (network) hosts.
	 * If @a immediate returns @c FALSE the clients needs wait until a message matching the @a tag
	 * value is received.
	 *
	 * @param tag The tag to use when sending the configure message. Only used if @a immediate returns @c FALSE.
	 * @param immediate This will be set to @c TRUE by the host if the client was immediately configured.
	 */
	virtual OP_STATUS Configure(unsigned int tag, BOOL &immediate);

	/**
	 * Gets the version of the connected OpScopeHost (if any).
	 * @return The version of the host, or -1 if no OpScopeHost is connected.
	 */
	int GetHostVersion() const;

	// Abstract methods for subclasses to implement

	/**
	 * Will be called when a new host is attached to the client.
	 * @note The pointers for host and client are setup prior to calling this.
	 */
	virtual OP_STATUS OnHostAttached(OpScopeHost *host) = 0;
	/**
	 * Will be called when an existing host is detached from the client.
	 * @note The pointers for host and client are reset prior to calling this.
	 */
	virtual OP_STATUS OnHostDetached(OpScopeHost *host) = 0;
	/**
	 * Will be called when the system was unable to find a new host for the
	 * client. For instance if the listener for incoming connections failed
	 * to setup.
	 */
	virtual OP_STATUS OnHostMissing() = 0;

	/**
	 * Returns a string identifying the type of client, e.g. "socket" or "dom"
	 */
	virtual const uni_char *GetType() const = 0;

	/**
	 * Called whenever the host sends a message to this client.
	 */
	virtual OP_STATUS Receive(OpAutoPtr<OpScopeTPMessage> &message) = 0;

	virtual OP_STATUS Serialize(OpScopeTPMessage &to, const OpProtobufInstanceProxy &from, OpScopeTPHeader::MessageType type) = 0;
	virtual OP_STATUS Parse(const OpScopeTPMessage &from, OpProtobufInstanceProxy &to, OpScopeTPError &error) = 0;

	// Default serializers
	virtual OP_STATUS SerializeError(OpScopeTPMessage &to, const OpScopeTPError &error_data, OpScopeTPHeader::MessageType type);
	static OP_STATUS SerializeErrorDefault(OpScopeTPMessage &msg, const OpScopeTPError &error_data, OpScopeTPHeader::MessageType type);
	static OP_STATUS SerializeDefault(OpScopeTPMessage &msg, const OpProtobufInstanceProxy &proxy, OpScopeTPHeader::MessageType type);
	static OP_STATUS ParseDefault(const OpScopeTPMessage &from, OpProtobufInstanceProxy &to, OpScopeTPError &error);

private:
	friend class OpScopeHostManager; // For SetHost

	void SetHost(OpScopeHost *h);

	OpScopeHostManager *host_manager;
	OpScopeHost *host;
	OpScopeTPMessage::MessageType message_type;
};
#endif // SCOPE_SUPPORT

#endif // OP_SCOPE_CLIENT_H

