/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef SCOPE_PROTOCOL_SERVICE_H
#define SCOPE_PROTOCOL_SERVICE_H

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/scope_tp_message.h"
#include "modules/scope/src/generated/g_scope_protocol_service_interface.h"

class OpScopeNetwork;
class OpScopeServiceManager;

class OpScopeProtocolService
	: public OpScopeProtocolService_SI
{
public:
	OpScopeProtocolService(OpScopeServiceManager *manager = NULL);
	virtual ~OpScopeProtocolService();

	static OP_STATUS CreateServicesMessage(OpScopeServiceManager *manager, OpAutoPtr<OpScopeTPMessage> &msg);

	/**
	 * Creates a new STP message which is used to enable a specific service.
	 *
	 * @param msg msg A new message is created here which will contain the message header and serialized payload.
	 * @param service_name Name of service to enable.
	 * @param tag Tag value to place in message.
	 * @return OpStatus::OK on success, otherwise an OpStatus::ERR_* status.
	 */
	static OP_STATUS CreateEnableMessage(OpAutoPtr<OpScopeTPMessage> &msg, const uni_char *service_name, int tag);
	/**
	 * Creates a new STP message which is used to configure the scope service to use a specific format in events.
	 *
	 * @param msg A new message is created here which will contain the message header and serialized payload.
	 * @param client The client used to serialize the message, if null it uses OpScopeClient::SerializeDefault.
	 * @param tag Tag value to place in message.
	 * @param type Format use for serialization.
	 * @param version Version for the new message.
	 * @return OpStatus::OK on success, otherwise an OpStatus::ERR_* status.
	 */
	static OP_STATUS CreateConfigureMessage(OpAutoPtr<OpScopeTPMessage> &msg, OpScopeClient *client, int tag, OpScopeTPHeader::MessageType type = OpScopeTPHeader::JSON, OpScopeTPHeader::MessageVersion version = OpScopeTPHeader::Version_1);

public:
	// Request/Response functions
	virtual OP_STATUS DoConnect(const ClientInfo &in);
	virtual OP_STATUS DoDisconnect();
	virtual OP_STATUS DoEnable(const ServiceSelection &in, ServiceResult &out);
	virtual OP_STATUS DoDisable(const ServiceSelection &in, ServiceResult &out);
	virtual OP_STATUS DoInfo(const ServiceSelection &in, ServiceInfo &out);
	virtual OP_STATUS DoQuit();
	virtual OP_STATUS DoHostInfo(HostInfo &out);
	virtual OP_STATUS DoMessageInfo(const MessageSelection &in, MessageInfoList &out);
	virtual OP_STATUS DoEnumInfo(const EnumArg &in, EnumList &out);

	OP_STATUS SetHostInfo(HostInfo &info) const;

	enum MessageFlag
	{
		MessageFlag_IncludeAll = 0x1,
		MessageFlag_IncludeRelated = 0x2,
		MessageFlag_ShowEnums = 0x4
	};

	OP_STATUS SetServiceInfo(Service &info, const OpScopeService *service) const;
	static OP_STATUS IntrospectService(ServiceInfo &info, const OpScopeService *service);
	OP_STATUS IntrospectMessageList(MessageInfoList &info, const OpScopeService *service, const OpValueVector<UINT32> &message_list, unsigned flags);
	OP_STATUS IntrospectMessage(MessageInfoList &message_info_list, const OpProtobufMessage *message, unsigned flags);
	OP_STATUS IntrospectEnumList(EnumList &info, const OpScopeService *service, const OpValueVector<UINT32> &enum_list, BOOL include_all);
	OP_STATUS IntrospectEnum(EnumList::Enum &enum_data, const OpScopeService *service, unsigned enum_id);
	OP_STATUS ReportUnknownService(const OpString &name);
	static OP_STATUS ParseFormat(const OpString &str, OpScopeTPMessage::MessageType &type);
	static OP_STATUS GenerateFormat(OpScopeTPHeader::MessageType type, const uni_char *&name);

	OP_STATUS DisconnectClient(OpScopeClient *client);
	//OP_STATUS RegisterClient(const OpString &uuid, OpScopeTPMessage::MessageType type, OpScopeClient *client, int &client_id);
	//OP_STATUS UnregisterClient(const OpString &uuid, int client_id, OpScopeClient *transport);
	OP_STATUS EnableServiceForClient(const OpString &name);
	OP_STATUS DisableServiceForClient(const OpString &name);
	OP_STATUS DisableServiceForClient(OpScopeService *service, OpScopeClient *client);

	OP_STATUS ReportNonExistingMessage(unsigned message_id);
	OP_STATUS ReportNonExistingEnum(unsigned enum_id);
};

#endif // SCOPE_HTTP_LOGGER_H
