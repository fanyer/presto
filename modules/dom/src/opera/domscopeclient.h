/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Johannes Hoff jhoff@opera.com
*/

#ifndef MODULES_DOM_DOMSCOPECLIENT_H
#define MODULES_DOM_DOMSCOPECLIENT_H

#ifdef JS_SCOPE_CLIENT

#include "modules/scope/scope_client.h"
class ES_Value;
class DOM_Runtime;
class ES_Object;

class JS_ScopeClient : public OpScopeClient
{
	DOM_Runtime *owner;
	// callbacks
	ES_Object *cb_host_connected;
	ES_Object *cb_receive;
	ES_Object *cb_host_quit;

	class ServiceCallback : public Link
	{
	public:
		ServiceCallback(unsigned int tag, ES_Object *cb_service_enabled)
			: tag(tag)
			, cb_service_enabled(cb_service_enabled)
		{}

		unsigned int  tag;
		ES_Object    *cb_service_enabled;
	};
	Head serviceCallbacks; // Contains ServiceCallback objects.
	enum State
	{
		/**
		 * Client is not connected to a scope host and is waiting for a
		 * connection in OnHostAttached(). This is the initial state.
		 */
		  State_Idle
		/**
		 * Client is waiting for the service list from the scope host.
		 */
		, State_ServiceList
		/**
		 * The service list has been receieved and a scope.Connect call has
		 * been made to the scope host, awaiting the response to the message.
		 */
		, State_ConnectResponse
		/**
		 * Connect response was receievied, client is fully connected to the
		 * scope host and normal processing may occur.
		 */
		, State_Connected
	};
	State state;
	unsigned tag_counter; // Used to get a unique tag to be used internally, ie. not the DOM client itself.
	unsigned connect_tag;
	OpString service_list;
	BOOL host_quit_sent;

	OP_STATUS SetString(ES_Value *value, ByteBuffer *data, TempBuffer &tmp);

	/**
	 * Disconnects the client from the JS script by calling the quit-callback.
	 * Does nothing if @a host_quit_sent has been set to @c TRUE.
	 */
	OP_STATUS DisconnectJS();
public:
	JS_ScopeClient(DOM_Runtime *origining_runtime);

	/**
	 * Disconnects the client from the host.
	 *
	 * @note If the JS client is to be notified of the client being removed it
	 *       must be done by calling Disconnect() before deleting the object.
	 */
	~JS_ScopeClient();

	OP_STATUS Construct(ES_Object *host_connected, ES_Object *host_receive, ES_Object *host_quit);
	virtual OP_STATUS EnableService(const uni_char *name, ES_Object *cb_service_enabled, unsigned int tag);

	/**
	 * Disconnects the client from scope and optionally notifies the JS code
	 * by calling the quit-callback.
	 *
	 * @param allow_js If @c TRUE then it will call the quit-callback
	 *                 (unless already called), if @c FALSE it will only
	 *                 disconnect from scope.
	 */
	void Disconnect(BOOL allow_js);

	virtual const uni_char *GetType() const { return UNI_L("javascript"); };

	// From OpScopeClient

	virtual OP_STATUS OnHostAttached(OpScopeHost *host);
	virtual OP_STATUS OnHostDetached(OpScopeHost *host);
	virtual OP_STATUS OnHostMissing();

	virtual OP_STATUS Receive(OpAutoPtr<OpScopeTPMessage> &message);

	virtual OP_STATUS Serialize(OpScopeTPMessage &to, const OpProtobufInstanceProxy &from, OpScopeTPHeader::MessageType type);
	virtual OP_STATUS Parse(const OpScopeTPMessage &from, OpProtobufInstanceProxy &to, OpScopeTPError &error);

	static OP_STATUS SerializeECMAScript(OpScopeTPMessage &msg, const OpProtobufInstanceProxy &proxy, ES_Runtime *runtime);
	OP_STATUS OnServices(OpAutoPtr<OpScopeTPMessage> &message);
	OP_STATUS OnQuit();
	OP_STATUS OnHostConfigured();
};

#endif // JS_SCOPE_CLIENT

#endif // MODULES_DOM_DOMSCOPECLIENT_H

