/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_protocol_service.h"
#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/scope/src/scope_tp_message.h"
#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/generated/g_scope_manager.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"
#include "modules/url/uamanager/ua.h"
#include "modules/pi/OpSystemInfo.h"

template <typename T>
class OpUnicodeWrapper
{
public:
	OpUnicodeWrapper(T &container) : container(container) {}

	const T &Container() const { return container; }
	T &Container() { return container; }

	OP_STATUS Append(const uni_char *str)
	{
		unsigned int len = uni_strlen(str)*2;
		return OpProtobufUtils::Append(container, OpProtobufUtils::Bytes((char *)str, len));
	}

private:
	T &container;
};

OpScopeProtocolService::OpScopeProtocolService(OpScopeServiceManager *manager)
	: OpScopeProtocolService_SI(NULL, manager, CONTROL_FORCED)
{
}

/* virtual */
OpScopeProtocolService::~OpScopeProtocolService()
{
}

OP_STATUS
OpScopeProtocolService::DoConnect(const ClientInfo &in)
{
	if (!GetManager())
		return OpStatus::ERR;

	OpScopeTPMessage::MessageType type;
	RETURN_IF_ERROR(ParseFormat(in.GetFormat(), type));

	OpScopeClient *client = GetClient();
	OP_ASSERT(client != NULL);
	if (client == NULL)
		return OpStatus::ERR_NULL_POINTER;

	client->SetMessageType(type);
	// Disable all currently enabled services
	return GetManager()->DisableServices();
}

OP_STATUS
OpScopeProtocolService::DoDisconnect()
{
	if (!GetManager())
		return OpStatus::ERR;

	OpScopeClient *client = GetClient();
	OP_ASSERT(client != NULL);
	if (client == NULL)
		return OpStatus::ERR;

	// Disable all currently enabled services
	return GetManager()->DisableServices();
}

OP_STATUS
OpScopeProtocolService::EnableServiceForClient(const OpString &name)
{
	if (!GetManager())
		return OpStatus::ERR;

	if (name.Compare(UNI_L("scope")) == 0)
	{
		// Enabling "scope" service is not allowed
		return OpStatus::ERR;
	}

	OpScopeService *service = GetManager()->FindService(name);
	if (service == NULL)
		return ReportUnknownService(name);

	OpScopeClient *client = GetClient();
	if (client == NULL)
		return OpStatus::ERR;

	if (service->IsEnabled()) // If the service is already enabled, send back error
		return SetCommandError(OpScopeTPHeader::ServiceAlreadyEnabled);

	RETURN_IF_ERROR(service->Enable());

	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::DoEnable(const ServiceSelection &in, ServiceResult &out)
{
	if (!GetManager())
		return OpStatus::ERR;

	const OpString &name = in.GetName();
	RETURN_IF_ERROR(EnableServiceForClient(name));

	RETURN_IF_ERROR(out.SetName(name));
	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::DisableServiceForClient(const OpString &name)
{
	if (!GetManager())
		return OpStatus::ERR;

	if (name.Compare(UNI_L("scope")) == 0)
	{
		// Disabling "scope" service is not allowed
		return OpStatus::ERR;
	}

	OpScopeService *service = GetManager()->FindService(name);
	if (service == NULL)
		return ReportUnknownService(name);
	OpScopeClient *client = GetClient();
	if (client == NULL)
		return OpStatus::ERR;

	return DisableServiceForClient(service, client);
}

OP_STATUS
OpScopeProtocolService::DisableServiceForClient(OpScopeService *service, OpScopeClient *client)
{
	if (!GetManager())
		return OpStatus::ERR;

	if (!service->IsEnabled()) // If the service is not enabled, send back error
		return OpStatus::ERR;

	RETURN_IF_ERROR(service->Disable());

	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::DoDisable(const ServiceSelection &in, ServiceResult &out)
{
	if (!GetManager())
		return OpStatus::ERR;

	const OpString &name = in.GetName();
	RETURN_IF_ERROR(DisableServiceForClient(name));

	RETURN_IF_ERROR(out.SetName(name));
	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::DoInfo(const ServiceSelection &in, ServiceInfo &out)
{
	if (!GetManager())
		return OpStatus::ERR;

	OpScopeService *service = GetManager()->FindService(in.GetName());
	if (service == NULL)
	{
		return ReportUnknownService(in.GetName());
	}
	return IntrospectService(out, service);
}

OP_STATUS
OpScopeProtocolService::DoMessageInfo(const MessageSelection &in, MessageInfoList &out)
{
	if (!GetManager())
		return OpStatus::ERR;

	OpScopeService *service = GetManager()->FindService(in.GetServiceName());
	if (service == NULL)
	{
		return ReportUnknownService(in.GetServiceName());
	}
	BOOL include_all = in.HasIncludeAll() && in.GetIncludeAll();
	BOOL include_related = in.HasIncludeRelated() && in.GetIncludeRelated();
	unsigned flags = (include_all ? MessageFlag_IncludeAll : 0) |
					 (include_related ? MessageFlag_IncludeRelated : 0) |
					 MessageFlag_ShowEnums;
	return IntrospectMessageList(out, service, in.GetIdList(), flags);
}

OP_STATUS
OpScopeProtocolService::DoEnumInfo(const EnumArg &in, EnumList &out)
{
	if (!GetManager())
		return OpStatus::ERR;

	OpScopeService *service = GetManager()->FindService(in.GetServiceName());
	if (service == NULL)
		return ReportUnknownService(in.GetServiceName());
	return IntrospectEnumList(out, service, in.GetIdList(), in.GetIncludeAll());
}

OP_STATUS
OpScopeProtocolService::DoHostInfo(HostInfo &out)
{
	if (!GetManager())
		return OpStatus::ERR;

	return SetHostInfo(out);
}

OP_STATUS
OpScopeProtocolService::DoQuit()
{
	if (!GetManager())
		return OpStatus::ERR;

	// TODO: Disconnect client from all services, and disable services which have no clients
	// TODO: Reset connection when there are no more clients

	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeProtocolService::CreateServicesMessage(OpScopeServiceManager *manager, OpAutoPtr<OpScopeTPMessage> &msg)
{
	if (!manager)
		return OpStatus::ERR;

	ByteBuffer b;
	OpUnicodeWrapper<ByteBuffer> data(b);
	BOOL first = TRUE;

	OpScopeServiceManager::ServiceRange range(manager->GetServices());
	for (; !range.IsEmpty(); range.PopFront())
	{
		OpScopeService *service = range.Front();
		if (first)
			first = FALSE;
		else
			RETURN_IF_ERROR(data.Append(UNI_L(",")));
		RETURN_IF_ERROR(data.Append(service->GetName()));
	}

	msg.reset(OP_NEW(OpScopeTPMessage, (OpScopeTPMessage::STP_Event, UNI_L("*services"), Command_OnServices, OpScopeTPMessage::OK, 0, OpScopeTPMessage::Version_0)));
	RETURN_OOM_IF_NULL(msg.get());
	return msg->Construct(b, OpScopeTPMessage::JSON); // NOTE: This will not be encoded as JSON but as STP/0, hence the Version_0 above.
}

/*static*/
OP_STATUS
OpScopeProtocolService::CreateEnableMessage(OpAutoPtr<OpScopeTPMessage> &msg, const uni_char *service_name, int tag)
{
	msg.reset(OP_NEW(OpScopeTPMessage,
					 (OpScopeTPHeader::STP_Call, UNI_L("*enable"), 0,
					  OpScopeTPHeader::OK, 0,
					  OpScopeTPHeader::Version_0)));
	RETURN_OOM_IF_NULL(msg.get());
	RETURN_IF_ERROR(msg->CreateEmptyData(OpScopeTPHeader::JSON));
	return msg->Data()->AppendBytes((const char *)service_name, uni_strlen(service_name)*2);
}

/*static*/
OP_STATUS
OpScopeProtocolService::CreateConfigureMessage(OpAutoPtr<OpScopeTPMessage> &msg, OpScopeClient *client, int tag, OpScopeTPHeader::MessageType type, OpScopeTPHeader::MessageVersion version)
{
	OP_ASSERT(type == OpScopeTPHeader::JSON || type == OpScopeTPHeader::XML || type == OpScopeTPHeader::ProtocolBuffer);
	if (type != OpScopeTPHeader::JSON && type != OpScopeTPHeader::XML && type != OpScopeTPHeader::ProtocolBuffer)
	{
		return OpStatus::ERR;
	}

	msg.reset(OP_NEW(OpScopeTPMessage,
					 (OpScopeTPHeader::STP_Call, UNI_L("scope"), OpScopeProtocolService::Command_Connect,
					  OpScopeTPHeader::OK, tag, version)));
	RETURN_OOM_IF_NULL(msg.get());

	OpScopeProtocolService::ClientInfo client_info;
	RETURN_IF_ERROR(client_info.SetFormat( type == OpScopeTPHeader::JSON ? UNI_L("json") :
		                                  (type == OpScopeTPHeader::XML  ? UNI_L("xml")
										                                 : UNI_L("protobuf")) ));
	OpProtobufInstanceProxy proxy(OpScopeProtocolService::ClientInfo::GetMessageDescriptor(g_scope_manager->GetDescriptorSet().scope_), static_cast<void *>(&client_info));
	if (client)
		return client->Serialize(*msg.get(), proxy, OpScopeTPHeader::JSON);
	else
		return OpScopeClient::SerializeDefault(*msg.get(), proxy, OpScopeTPHeader::JSON);
}

OP_STATUS
OpScopeProtocolService::SetHostInfo(HostInfo &info) const
{
	if (!GetManager())
		return OpStatus::ERR;

	// TODO: Add RETURN_IF_ERROR
	info.SetStpVersion(OpScopeTPHeader::DefaultVersion);
	RETURN_IF_ERROR(info.SetCoreVersion(GetManager()->GetCoreVersion()));
	RETURN_IF_ERROR(info.SetPlatform(g_op_system_info->GetPlatformStr()));
	OpString tmp;
	RETURN_IF_ERROR(tmp.Set(g_op_system_info->GetPlatformStr()));
	RETURN_IF_ERROR(info.SetOperatingSystem(tmp));
	char ua_str[256]; // ARRAY OK 2008-11-14 jborsodi
	g_uaManager->GetUserAgentStr(ua_str, sizeof(ua_str), (const uni_char*)0);
	RETURN_IF_ERROR(tmp.Set(ua_str));
	RETURN_IF_ERROR(info.SetUserAgent(tmp));

	OpProtobufMessageVector<Service> &services = info.GetServiceListRef();

	OpScopeServiceManager::ServiceRange range(GetManager()->GetServices());
	for (; !range.IsEmpty(); range.PopFront())
	{
		OpAutoPtr<Service> serviceInfo(OP_NEW(Service, ()));
		RETURN_OOM_IF_NULL(serviceInfo.get());
		RETURN_IF_ERROR(SetServiceInfo(*serviceInfo.get(), range.Front()));
		RETURN_IF_ERROR(services.Add(serviceInfo.release()));
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::SetServiceInfo(Service &info, const OpScopeService *service) const
{
	const uni_char *name = service->GetName();
	info.SetName(name);
	info.SetVersion(service->GetVersionString());
	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::ReportUnknownService(const OpString &name)
{
	OpScopeTPError error(OpScopeTPHeader::ServiceNotFound);
	OpString description;
	RETURN_IF_ERROR(description.Append(UNI_L("Service '")));
	RETURN_IF_ERROR(description.Append(name));
	RETURN_IF_ERROR(description.Append(UNI_L("' does not exist")));
	RETURN_IF_ERROR(error.SetDescription(description));
	return SetCommandError(error);
}

/*static*/
OP_STATUS
OpScopeProtocolService::IntrospectService(ServiceInfo &info, const OpScopeService *service)
{
	CommandRange commands = service->GetCommands();
	OpProtobufMessageVector<CommandInfo> &commandInfos = info.GetCommandListRef();
	for (CommandRange r = commands; !r.IsEmpty(); r.PopFront())
	{
		const OpScopeCommand &cmd = *r.Front();
		if (cmd.type != OpScopeCommand::Type_Call)
			continue;
		CommandInfo *command = commandInfos.AddNew();
		if (command == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(command->SetName(cmd.name));
		command->SetNumber(cmd.number);
		command->SetMessageID(cmd.message_id);
		command->SetResponseID(cmd.response_id);
	}
	OpProtobufMessageVector<EventInfo> &eventInfos = info.GetEventListRef();
	for (CommandRange r = commands; !r.IsEmpty(); r.PopFront())
	{
		const OpScopeCommand &cmd = *r.Front();
		if (cmd.type != OpScopeCommand::Type_Event)
			continue;
		EventInfo *evt = eventInfos.AddNew();
		if (evt == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(evt->SetName(cmd.name));
		evt->SetNumber(cmd.number);
		evt->SetMessageID(cmd.response_id);
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::ReportNonExistingMessage(unsigned message_id)
{
	uni_char buffer[33+11+1]; // ARRAY OK 2009-08-07 jborsodi
	OpScopeTPError error(OpScopeTPHeader::BadRequest);
	RETURN_IF_ERROR(FormatCommandError(error, buffer, ARRAY_SIZE(buffer), UNI_L("Message with ID %d does not exist"), message_id));
	RETURN_IF_ERROR(SetCommandError(error));
	return OpStatus::ERR;
}

OP_STATUS
OpScopeProtocolService::ReportNonExistingEnum(unsigned enum_id)
{
	uni_char buffer[30+11+1]; // ARRAY OK 2010-04-14 jborsodi
	OpScopeTPError error(OpScopeTPHeader::BadRequest);
	RETURN_IF_ERROR(FormatCommandError(error, buffer, ARRAY_SIZE(buffer), UNI_L("Enum with ID %d does not exist"), enum_id));
	RETURN_IF_ERROR(SetCommandError(error));
	return OpStatus::ERR;
}

OP_STATUS
OpScopeProtocolService::IntrospectMessageList(MessageInfoList &message_info_list, const OpScopeService *service, const OpValueVector<UINT32> &message_list, unsigned flags)
{
	if (flags & MessageFlag_IncludeAll)
	{
		MessageRange range(service->GetMessages());
		for (; !range.IsEmpty(); range.PopFront())
		{
			const OpProtobufMessage *message = range.Front();
			if (message == NULL)
				return ReportNonExistingMessage(range.FrontID());

			RETURN_IF_ERROR(IntrospectMessage(message_info_list, message, flags & ~MessageFlag_IncludeRelated));
		}
	}
	else
	{
		for (unsigned i = 0; i < message_list.GetCount(); ++i)
		{
			unsigned message_id = message_list.Get(i);
			const OpProtobufMessage *message = service->GetMessage(message_id);
			if (message == NULL)
				return ReportNonExistingMessage(message_id);

			RETURN_IF_ERROR(IntrospectMessage(message_info_list, message, flags));
		}
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::IntrospectMessage(MessageInfoList &message_info_list, const OpProtobufMessage *message, unsigned flags)
{
	MessageInfo *info = message_info_list.AppendNewMessageList();
	if (info == NULL)
		return OpStatus::ERR_NO_MEMORY;

	info->SetId(message->GetInternalId());
	RETURN_IF_ERROR(info->SetName(message->GetName()));
	if (message->GetParentId() != 0)
		info->SetParentID(message->GetParentId());

	// Fields
	for (int field_idx = 0; field_idx < message->GetFieldCount(); ++field_idx)
	{
		const OpProtobufField &field = message->GetField(field_idx);
		MessageInfo::FieldInfo *field_info = info->GetFieldListRef().AddNew();
		if (field_info == NULL)
			return OpStatus::ERR_NO_MEMORY;
		OpProtobufField::Quantifier qf = field.GetQuantifier();
		unsigned qf_value = 0;
		if (qf == OpProtobufField::Required)
			qf_value = 0;
		else if (qf == OpProtobufField::Optional)
			qf_value = 1;
		else if (qf == OpProtobufField::Repeated)
			qf_value = 2;
		unsigned type = static_cast<UINT32>(field.GetType());
		OP_ASSERT(type >= 1 && type <= 16);

		RETURN_IF_ERROR(field_info->SetName(field.GetName()));
		field_info->SetNumber(field.GetNumber());
		if (qf_value != 0)
			field_info->SetQuantifier(qf_value);
		field_info->SetType(type);
		if (field.GetType() == OpProtobufFormat::Message && field.GetMessage() != NULL)
		{
			field_info->SetMessageID(field.GetMessage()->GetInternalId());
			if (flags & MessageFlag_IncludeRelated)
			{
				unsigned count = message_info_list.GetMessageList().GetCount(), i;
				for (i = 0; i < count; ++i)
				{
					if (message_info_list.GetMessageList().Get(i)->GetId() == static_cast<unsigned>(field.GetMessage()->GetInternalId()))
						break;
				}
				if (i == count)
					RETURN_IF_ERROR(IntrospectMessage(message_info_list, field.GetMessage(), flags));
			}
		}
		if (flags & MessageFlag_ShowEnums && field.GetEnumId() != 0)
		{
			field_info->SetEnumID(field.GetEnumId());
		}
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::IntrospectEnumList(EnumList &list, const OpScopeService *service, const OpValueVector<UINT32> &enum_list, BOOL include_all)
{
	if (include_all)
	{
		EnumIDRange enums = service->GetEnumIDs();
		for (; !enums.IsEmpty(); enums.PopFront())
		{
			EnumList::Enum *enum_data = list.AppendNewEnumList();
			RETURN_OOM_IF_NULL(enum_data);
			RETURN_IF_ERROR(IntrospectEnum(*enum_data, service, enums.Front()));
		}
	}
	else
	{
		for (unsigned i = 0; i < enum_list.GetCount(); ++i)
		{
			unsigned enum_id = enum_list.Get(i);
			EnumList::Enum *enum_data = list.AppendNewEnumList();
			RETURN_OOM_IF_NULL(enum_data);
			RETURN_IF_ERROR(IntrospectEnum(*enum_data, service, enum_id));
		}
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeProtocolService::IntrospectEnum(EnumList::Enum &enum_data, const OpScopeService *service, unsigned enum_id)
{
	if (!service->HasEnum(enum_id))
		return ReportNonExistingEnum(enum_id);
	const uni_char *name = NULL;
	unsigned count = 0;
	RETURN_IF_ERROR(service->GetEnum(enum_id, name, count));
	enum_data.SetId(enum_id);
	RETURN_IF_ERROR(enum_data.SetName(name));
	for (unsigned idx = 0; idx < count; ++idx)
	{
		EnumList::Enum::Value *value = enum_data.AppendNewValueList();
		RETURN_OOM_IF_NULL(value);
		const uni_char *value_name = NULL;
		unsigned number = 0;
		RETURN_IF_ERROR(service->GetEnumValue(enum_id, idx, value_name, number));
		RETURN_IF_ERROR(value->SetName(value_name));
		value->SetNumber(number);
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeProtocolService::ParseFormat(const OpString &str, OpScopeTPMessage::MessageType &type)
{
	if (str.Compare(UNI_L("protobuf")) == 0)
		type = OpScopeTPMessage::ProtocolBuffer;
	else if (str.Compare(UNI_L("json")) == 0)
		type = OpScopeTPMessage::JSON;
	else if (str.Compare(UNI_L("xml")) == 0)
		type = OpScopeTPMessage::XML;
	else
		return OpStatus::ERR;
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeProtocolService::GenerateFormat(OpScopeTPHeader::MessageType type, const uni_char *&name)
{
	if (type == OpScopeTPMessage::ProtocolBuffer)
		name = UNI_L("protobuf");
	else if (type == OpScopeTPMessage::JSON)
		name = UNI_L("json");
	else if (type == OpScopeTPMessage::XML)
		name = UNI_L("xml");
	else
		return OpStatus::ERR;
	return OpStatus::OK;
}

#endif // SCOPE_SUPPORT
