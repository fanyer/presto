/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_tp_message.h"
#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_network.h"
#include "modules/protobuf/src/protobuf_output.h"
#include "modules/protobuf/src/protobuf_input.h"
#include "modules/protobuf/src/protobuf_input_common.h"
#include "modules/protobuf/src/protobuf_ecmascript.h"
#include "modules/scope/src/generated/g_scope_manager.h"

/* OpScopeHostManager */

OpScopeHostManager::OpScopeHostManager()
{
}

OpScopeHostManager::~OpScopeHostManager()
{
	g_main_message_handler->UnsetCallBack(this, MSG_SCOPE_MANAGER_CLEANUP, 0);

	DestructAll();
}

OP_STATUS
OpScopeHostManager::Construct()
{
	g_main_message_handler->SetCallBack(this, MSG_SCOPE_MANAGER_CLEANUP, 0);
	return OpStatus::OK;
}

void
OpScopeHostManager::DestructAll()
{
	for (OpScopeClient *client = client_dumpster.First(); client;)
	{
		OpScopeClient *to_delete = client;
		client = client->Suc();

		to_delete->Out();
		to_delete->SetHostManager(NULL); // Avoid multiple calls to OnClientDestruction
		OnClientDestruction(to_delete);
		OP_DELETE(to_delete);
	}
	OP_ASSERT(client_dumpster.Empty());
	for (OpScopeHost *host = host_dumpster.First(); host;)
	{
		OpScopeHost *to_delete = host;
		host = host->Suc();

		to_delete->Out();
		to_delete->SetHostManager(NULL); // Avoid multiple calls to OnHostDestruction
		OnHostDestruction(to_delete);
		OP_DELETE(to_delete);
	}
	OP_ASSERT(host_dumpster.Empty());
}

void
OpScopeHostManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_SCOPE_MANAGER_CLEANUP)
		DestructAll();
}

void
OpScopeHostManager::ScheduleDestruction(OpScopeClient *client)
{
	OP_ASSERT(client);
	BOOL first = client_dumpster.First() == NULL && host_dumpster.First() == NULL;
	client->Into(&client_dumpster);
	if (first)
		g_main_message_handler->PostMessage(MSG_SCOPE_MANAGER_CLEANUP, 0, 0, 0);
}

void
OpScopeHostManager::ScheduleDestruction(OpScopeHost *host)
{
	OP_ASSERT(host);
	BOOL first = client_dumpster.First() == NULL && host_dumpster.First() == NULL;
	host->Into(&host_dumpster);
	if (first)
		g_main_message_handler->PostMessage(MSG_SCOPE_MANAGER_CLEANUP, 0, 0, 0);
}

void
OpScopeHostManager::ReportDestruction(OpScopeClient *client)
{
	for (OpScopeClient *it = client_dumpster.First(); it;)
	{
		if (it == client)
		{
			client->Out();
			break;
		}
		it = it->Suc();
	}
	client->SetHostManager(NULL); // Avoid multiple calls to OnClientDestruction
	OnClientDestruction(client);
}

void
OpScopeHostManager::ReportDestruction(OpScopeHost *host)
{
	for (OpScopeHost *it = host_dumpster.First(); it;)
	{
		if (it == host)
		{
			host->Out();
			break;
		}
		it = it->Suc();
	}
	host->SetHostManager(NULL); // Avoid multiple calls to OnHostDestruction
	OnHostDestruction(host);
}

/*static*/
OP_STATUS
OpScopeHostManager::AttachClientHost(OpScopeClient *client, OpScopeHost *host)
{
	OP_ASSERT(!client->GetHost());
	OP_ASSERT(!host->GetClient());

	// Set pointers only
	client->SetHost(host);
	host->SetClient(client);

	// Notify client & host
	RETURN_IF_ERROR(client->OnHostAttached(host));
	RETURN_IF_ERROR(host->OnClientAttached(client));

	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeHostManager::DetachClientHost(OpScopeClient *client, OpScopeHost *host)
{
	OP_ASSERT(client && host);
	OP_ASSERT(client->GetHost() == host && host->GetClient() == client);

	// Reset pointers only
	client->SetHost(NULL);
	host->SetClient(NULL);

	// Notify client & host
	RETURN_IF_ERROR(client->OnHostDetached(host));
	RETURN_IF_ERROR(host->OnClientDetached(client));

	return OpStatus::OK;
}

/* OpScopeHost */

OpScopeHost::OpScopeHost(OpScopeHostManager *host_manager)
	: host_manager(host_manager)
	, client(NULL)
{
}

OpScopeHost::~OpScopeHost()
{
	OP_ASSERT(!client);
}

/*virtual*/
OP_STATUS
OpScopeHost::AttachClient(OpScopeClient *new_client)
{
	if (GetClient())
		RETURN_IF_ERROR(OpScopeHostManager::DetachClientHost(GetClient(), this));
	if (new_client)
		RETURN_IF_ERROR(OpScopeHostManager::AttachClientHost(new_client, this));

	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeHost::DetachClient()
{
	if (GetClient())
		RETURN_IF_ERROR(OpScopeHostManager::DetachClientHost(GetClient(), this));

	return OpStatus::OK;
}

void
OpScopeHost::ScheduleDestruction()
{
	OP_ASSERT(host_manager);
	if (host_manager)
		host_manager->ScheduleDestruction(this);
}

/* OpScopeServiceManager */

void
OpScopeServiceManager::ServiceRange::PopFront()
{
	OP_ASSERT(front);
	front = next;
	next = next ? next->Suc() : NULL;
}


OpScopeServiceManager::OpScopeServiceManager()
{
	core_version.AppendL(CORE_VERSION_STRING); // we store the entire version for the host info
}

OpScopeServiceManager::~OpScopeServiceManager()
{
	while (OpScopeService* service = services.First())
	{
		service->SetManager(NULL);
		service->Out();
	}
}

OpScopeService *
OpScopeServiceManager::FindService(const uni_char *name) const
{
	ServiceRange range(GetServices());
	for (; !range.IsEmpty(); range.PopFront())
	{
		if (uni_str_eq(range.Front()->GetName(), name))
			return range.Front();
	}
	return NULL;
}

OpScopeService *
OpScopeServiceManager::FindService(const OpString &name) const
{
	return FindService(name.CStr());
}

OpScopeService *
OpScopeServiceManager::FindService(const char *name) const
{
	ServiceRange range(GetServices());
	for (; !range.IsEmpty(); range.PopFront())
	{
		if (uni_str_eq(range.Front()->GetName(), name))
			return range.Front();
	}
	return NULL;
}

OP_STATUS
OpScopeServiceManager::DisableServices()
{
	//
	// Make all services aware of the disconnect
	//
	ServiceRange range(GetServices());
	for (; !range.IsEmpty(); range.PopFront())
	{
		OpScopeService *service = range.Front();
		if (service->IsEnabled() && service->GetControlType() == OpScopeService::CONTROL_MANUAL)
			service->Disable();
	}

	return OpStatus::OK;
}

void
OpScopeServiceManager::RegisterService(OpScopeService *service)
{
	if (service->GetManager())
		service->GetManager()->UnregisterService(service);

	service->Into(&services);
	service->SetManager(this);
}

void
OpScopeServiceManager::UnregisterService(OpScopeService *service)
{
	service->Out();
	service->SetManager(NULL);
}
/* OpScopeBuiltinHost */

/*virtual*/
OP_STATUS
OpScopeBuiltinHost::OnClientAttached(OpScopeClient *new_client)
{
	OP_ASSERT(new_client && new_client == GetClient());

	ServiceRange range(GetServices());
	for (; !range.IsEmpty(); range.PopFront())
	{
		range.Front()->SetClient(new_client);
	}

	// Report the service list to the client
	OpAutoPtr<OpScopeTPMessage> msg;
	RETURN_IF_ERROR(OpScopeProtocolService::CreateServicesMessage(this, msg));
	RETURN_IF_ERROR(new_client->Receive(msg));

	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeBuiltinHost::OnClientDetached(OpScopeClient *old_client)
{
	OP_ASSERT(old_client);

	return DisableServices();
}

/*virtual*/
int
OpScopeBuiltinHost::GetVersion() const
{
	return 1;
}

/*virtual*/
OP_STATUS
OpScopeBuiltinHost::Receive(OpScopeClient *client, OpAutoPtr<OpScopeTPMessage> &message)
{
	const OpString &serviceName = message->ServiceName();
	OpScopeService *service = FindService(serviceName);
	if (service)
	{
		if (!service->IsEnabled())
		{
			// Service is not enabled, send back error
			OpScopeTPError error(OpScopeTPMessage::ServiceNotEnabled, UNI_L("Service is not enabled"));
			OpAutoPtr<OpScopeTPMessage> error_message;
			RETURN_IF_ERROR(OpScopeTPMessage::Clone(error_message, *message.get(), FALSE));
			error_message->SetVersion(OpScopeTPMessage::Version_1);
			RETURN_IF_ERROR(client->SerializeError(*error_message.get(), error, message->Type()));
			return client->Receive(error_message);
		}

		OP_STATUS status = service->OnReceive(client, *message.get());

		if (OpStatus::IsSuccess(status))
			return OpStatus::OK;

		OpScopeTPError error;
		error.SetStatus(OpScopeTPMessage::InternalError);
		OpAutoPtr<OpScopeTPMessage> error_message;
		RETURN_IF_ERROR(OpScopeTPMessage::Clone(error_message, *message.get(), FALSE));
		error_message->SetVersion(OpScopeTPMessage::Version_1);
		if (OpStatus::IsMemoryError(status))
		{
			// TODO: Perhaps the error message should be statically (or pre-) allocated to avoid allocation problems when sending it out
			error.SetStatus(OpScopeTPMessage::OutOfMemory);
			error.SetStaticDescription(UNI_L("Service encountered OOM while decoding incoming request"));
		}
		else if (status == OpStatus::ERR_PARSING_FAILED)
			error.SetStaticDescription(UNI_L("Service was not able to parse the input from the incoming request"));
		else if (status == OpStatus::ERR_NO_SUCH_RESOURCE)
			error.SetStaticDescription(UNI_L("Unable to find the specified resource"));
		else if (status == OpStatus::ERR_NO_DISK)
			error.SetStaticDescription(UNI_L("Disk is full"));
		else if (status == OpStatus::ERR_NO_ACCESS)
			error.SetStaticDescription(UNI_L("Attempting to write a read-only entity"));
		else if (status == OpStatus::ERR_NOT_DELAYED)
			error.SetStaticDescription(UNI_L("Posted message was not delayed"));
		else if (status == OpStatus::ERR_OUT_OF_RANGE)
			error.SetStaticDescription(UNI_L("Input out of range"));
		else if (status == OpStatus::ERR_FILE_NOT_FOUND)
			error.SetStaticDescription(UNI_L("File not found or could not be opened"));
		else if (status == OpStatus::ERR_BAD_FILE_NUMBER)
			error.SetStaticDescription(UNI_L("Wrong socket or file ID"));
		else if (status == OpStatus::ERR_NULL_POINTER)
			error.SetStaticDescription(UNI_L("Service encountered a null pointer while decoding incoming request"));
		else if (status == OpStatus::ERR_NOT_SUPPORTED)
			error.SetStaticDescription(UNI_L("Service does not support this request"));
		else
			error.SetStaticDescription(UNI_L("Service encountered an internal error while decoding the incoming request"));
		RETURN_IF_ERROR(client->SerializeError(*error_message.get(), error, message->Type()));
		return client->Receive(error_message);
	}

	// Service was not found, send error
	OpScopeTPError error(OpScopeTPMessage::ServiceNotFound, UNI_L("Service was not found"));
	OpAutoPtr<OpScopeTPMessage> error_message;
	RETURN_IF_ERROR(OpScopeTPMessage::Clone(error_message, *message.get(), FALSE));
	error_message->SetVersion(OpScopeTPMessage::Version_1);
	RETURN_IF_ERROR(client->SerializeError(*error_message.get(), error, message->Type()));
	RETURN_IF_ERROR(client->Receive(error_message));
	return OpStatus::OK;
}

OP_STATUS
OpScopeBuiltinHost::ConfigureClient(OpScopeClient *client, unsigned int tag, BOOL &immediate)
{
	OP_ASSERT(client);
	immediate = TRUE; // Tell caller that the connect is immediate, no need to wait for response message

	// Code from OpScopeProtocolService::DoConnect()
	// Disable all currently enabled services
	return DisableServices();
}

/* OpScopeClient */

OpScopeClient::OpScopeClient(OpScopeHostManager *host_manager)
	: host_manager(host_manager)
	, host(NULL)
	, message_type(OpScopeTPMessage::ProtocolBuffer)
{
}

OpScopeClient::OpScopeClient(OpScopeTPMessage::MessageType t, OpScopeHostManager *host_manager)
	: host_manager(host_manager)
	, host(NULL)
	, message_type(t)
{
}

/*virtual*/
OpScopeClient::~OpScopeClient()
{
	OP_ASSERT(!host);
}

OpScopeHost *
OpScopeClient::GetHost() const
{
	return host;
}

OpScopeTPMessage::MessageType 
OpScopeClient::GetMessageType() const
{
	return message_type;
}

void 
OpScopeClient::SetHost(OpScopeHost *h) 
{
	host = h;
}

void 
OpScopeClient::SetMessageType(OpScopeTPMessage::MessageType type)
{
	message_type = type;
}

/* virtual */ OP_STATUS
OpScopeClient::Send(const uni_char *service, const uni_char *data)
{
	if (GetHost()->GetVersion() != 0)
		return OpStatus::ERR;
	OP_ASSERT(data != NULL);
	if (data == NULL)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<OpScopeTPMessage> msg( OP_NEW(OpScopeTPMessage,
		(OpScopeTPHeader::STP_Call, service, 0, OpScopeTPHeader::OK, 0, OpScopeTPHeader::Version_0)) );
	RETURN_OOM_IF_NULL(msg.get());
	RETURN_IF_ERROR( msg->CreateEmptyData(OpScopeTPHeader::JSON) );
	RETURN_IF_ERROR( msg->Data()->AppendBytes((const char *)data, uni_strlen(data)*sizeof(uni_char)) );
	return SendToHost(msg);
}

/* virtual */ OP_STATUS
OpScopeClient::Send(const uni_char *service, unsigned command_id, unsigned tag, ES_Object *data, ES_Runtime *es_runtime)
{
	if (!GetHost() || GetHost()->GetVersion() == 0)
		return OpStatus::ERR;
	OP_ASSERT(data != NULL);
	if (data == NULL)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<OpScopeTPMessage> msg( OP_NEW(OpScopeTPMessage,
		(OpScopeTPHeader::STP_Call, service, command_id, OpScopeTPHeader::OK, tag, OpScopeTPHeader::Version_1)) );
	RETURN_OOM_IF_NULL(msg.get());
	RETURN_IF_ERROR( msg->SetESObject(data, es_runtime) );
	return SendToHost(msg);
}

/*virtual*/
OP_STATUS
OpScopeClient::SendToHost(OpAutoPtr<OpScopeTPMessage> &message)
{
	return GetHost()->Receive(this, message);
}

OP_STATUS
OpScopeClient::Configure(unsigned int tag, BOOL &immediate)
{
	return GetHost()->ConfigureClient(this, tag, immediate);
}

int 
OpScopeClient::GetHostVersion() const
{
	return GetHost() ? GetHost()->GetVersion() : -1;
}

/*virtual*/
OP_STATUS
OpScopeClient::AttachHost(OpScopeHost *new_host)
{
	if (GetHost())
		RETURN_IF_ERROR(OpScopeHostManager::DetachClientHost(this, GetHost()));
	if (new_host)
		RETURN_IF_ERROR(OpScopeHostManager::AttachClientHost(this, new_host));

	return OpStatus::OK;
}

OP_STATUS
OpScopeClient::DetachHost()
{
	if (GetHost())
		return OpScopeHostManager::DetachClientHost(this, GetHost());
	return OpStatus::OK;
}

void
OpScopeClient::ScheduleDestruction()
{
	OP_ASSERT(host_manager);
	if (host_manager)
		host_manager->ScheduleDestruction(this);
}

/*virtual*/
OP_STATUS
OpScopeClient::SerializeError(OpScopeTPMessage &msg, const OpScopeTPError &error_data, OpScopeTPHeader::MessageType type)
{
	msg.SetTransportType(OpScopeTPMessage::STP_Error);
	msg.SetStatus(error_data.GetStatus());

	OpScopeService_SI::ErrorInfo info;
	if (error_data.GetDescription() != NULL)
		info.SetDescription(error_data.GetDescription());
	if (error_data.GetLine() >= 0)
		info.SetLine(error_data.GetLine());
	if (error_data.GetColumn() >= 0)
		info.SetColumn(error_data.GetColumn());
	if (error_data.GetOffset() >= 0)
		info.SetOffset(error_data.GetOffset());

	OpProtobufInstanceProxy proxy(OpScopeService_SI::ErrorInfo::GetMessageDescriptor(g_scope_manager->GetDescriptorSet().scope_service_), reinterpret_cast<void *>(&info));
	RETURN_IF_ERROR(Serialize(msg, proxy, type));

	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeClient::SerializeErrorDefault(OpScopeTPMessage &msg, const OpScopeTPError &error_data, OpScopeTPHeader::MessageType type)
{
	msg.SetTransportType(OpScopeTPMessage::STP_Error);
	msg.SetStatus(error_data.GetStatus());

	OpScopeService_SI::ErrorInfo info;
	if (error_data.GetDescription() != NULL)
		info.SetDescription(error_data.GetDescription());
	if (error_data.GetLine() >= 0)
		info.SetLine(error_data.GetLine());
	if (error_data.GetColumn() >= 0)
		info.SetColumn(error_data.GetColumn());
	if (error_data.GetOffset() >= 0)
		info.SetOffset(error_data.GetOffset());

	OpProtobufInstanceProxy proxy(OpScopeService_SI::ErrorInfo::GetMessageDescriptor(g_scope_manager->GetDescriptorSet().scope_service_), reinterpret_cast<void *>(&info));
	RETURN_IF_ERROR(SerializeDefault(msg, proxy, type));

	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeClient::SerializeDefault(OpScopeTPMessage &msg, const OpProtobufInstanceProxy &proxy, OpScopeTPHeader::MessageType type)
{
	if (type == OpScopeTPMessage::ProtocolBuffer)
	{
		RETURN_IF_ERROR(msg.CreateEmptyData(type));
		OpProtobufByteBufferOutputRange out_range(*msg.Data());
		OpProtobufOutputStream stream(&out_range);
		RETURN_IF_ERROR(stream.Write(proxy));
	}
	else if (type == OpScopeTPMessage::JSON)
	{
		RETURN_IF_ERROR(msg.CreateEmptyData(type));
		OpJSONOutputStream stream(*msg.Data());
		RETURN_IF_ERROR(stream.Write(proxy));
	}
	else if (type == OpScopeTPMessage::XML)
	{
		RETURN_IF_ERROR(msg.CreateEmptyData(type));
		OpXMLOutputStream stream(*msg.Data());
		RETURN_IF_ERROR(stream.Write(proxy));
	}
	else if (type == OpScopeTPMessage::ECMAScript)
	{
		OP_ASSERT(!"ECMAScript objects cannot be serialized with OpScopeTransport::SerializeDefault");
		return OpStatus::ERR;
	}
	else if (type == OpScopeTPMessage::None)
	{
		OP_ASSERT(!"Cannot serialize message without a format (type None)");
		return OpStatus::ERR;
	}
	else
	{
		OP_ASSERT(!"Unknown OpScopeTPMessage::Type, add extra 'else if' condition");
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpScopeClient::ParseDefault(const OpScopeTPMessage &msg, OpProtobufInstanceProxy &proxy, OpScopeTPError &error)
{
	OpScopeTPMessage::MessageType type = msg.Type();
	if (type == OpScopeTPMessage::ProtocolBuffer)
	{
		// TODO: Add extraction of parser error from proto input stream
		OpHeapArrayAnchor<char> data(msg.Data()->Copy());
		OpProtobufInputStream stream;
		RETURN_IF_ERROR(stream.Construct(reinterpret_cast<unsigned char *>(data.Get()), msg.Data()->Length()));
		OP_STATUS status = stream.Read(proxy);
		if (status == OpStatus::ERR_PARSING_FAILED)
		{
			error.SetStatus(OpScopeTPHeader::BadRequest);
			error.SetStaticDescription(UNI_L("Parser error while parsing Protocol Buffer input"));
		}
		else if (OpStatus::IsError(status))
		{
			error.SetStatus(OpScopeTPHeader::InternalError);
			error.SetStaticDescription(UNI_L("Internal error while parsing Protocol Buffer input"));
		}
		return status;
	}
	else if (type == OpScopeTPMessage::JSON)
	{
		TempBuffer data;
		OP_STATUS status = OpProtobufUtils::ConvertUTF8toUTF16(data, *msg.Data());
		if (OpStatus::IsError(status))
		{
			error.SetStatus(OpScopeTPHeader::BadRequest);
			error.SetStaticDescription(UNI_L("Failed to convert JSON input from UTF-8 to UTF-16"));
			return status;
		}
		OpJSONInputStream stream;
		status = stream.Construct(data.GetStorage(), data.Length());
		if (OpStatus::IsError(status))
		{
			error.SetStatus(OpScopeTPHeader::InternalError);
			error.SetStaticDescription(UNI_L("Failed to initialize JSON parser"));
			return status;
		}
		status = stream.Read(proxy);
		if (status == OpStatus::ERR_PARSING_FAILED || stream.GetError().IsSet())
		{
			error.SetStatus(OpScopeTPHeader::BadRequest);
			if (stream.GetError().IsSet())
			{
				error.SetDescription(stream.GetError().GetDescription());
				error.SetLine(stream.GetError().GetLine());
				error.SetColumn(stream.GetError().GetColumn());
				error.SetOffset(stream.GetError().GetOffset());
			}
			else
				error.SetStaticDescription(UNI_L("Internal error while parsing JSON input"));
		}
		else if (OpStatus::IsError(status))
		{
			error.SetStatus(OpScopeTPHeader::InternalError);
			error.SetStaticDescription(UNI_L("Internal error while parsing JSON input"));
		}
		return status;
	}
	else if (type == OpScopeTPMessage::XML)
	{
		OpHeapArrayAnchor<char> data(msg.Data()->Copy());
		OpXMLInputStream stream;
		OP_STATUS status = stream.Construct(data.Get(), msg.Data()->Length());
		if (stream.HasErrorObject())
		{
			error.SetStatus(OpScopeTPHeader::BadRequest);
			RETURN_IF_ERROR(error.SetDescription(stream.GetError().GetDescription()));
			error.SetLine(stream.GetError().GetLine());
			error.SetColumn(stream.GetError().GetColumn());
			error.SetOffset(-1);
			if (status == OpStatus::OK)
				status = OpStatus::ERR;
			return status;
		}
		if (OpStatus::IsSuccess(status))
		{
			status = stream.Read(proxy);
		}
		if (status == OpStatus::ERR_PARSING_FAILED)
		{
			error.SetStatus(OpScopeTPHeader::BadRequest);
			error.SetStaticDescription(UNI_L("Error while parsing XML input"));
		}
		else if (OpStatus::IsError(status))
		{
			error.SetStatus(OpScopeTPHeader::InternalError);
			error.SetStaticDescription(UNI_L("Internal error while parsing XML input"));
		}
		return status;
	}
	else if (type == OpScopeTPMessage::ECMAScript)
	{
		// TODO: Add extraction of parser error from ES input stream
		OpESInputStream stream;
		RETURN_IF_ERROR(stream.Construct(msg.GetESObject(), msg.GetESRuntime()));
		OP_STATUS status = stream.Read(proxy);
		if (status == OpStatus::ERR_PARSING_FAILED)
		{
			error.SetStatus(OpScopeTPHeader::BadRequest);
			error.SetStaticDescription(UNI_L("Parser error while parsing ECMAScript object"));
		}
		else if (OpStatus::IsError(status))
		{
			error.SetStatus(OpScopeTPHeader::InternalError);
			error.SetStaticDescription(UNI_L("Internal error while parsing ECMAScript object"));
		}
		return status;
	}
	else if (type == OpScopeTPMessage::None)
	{
		OP_ASSERT(!"Cannot parse message without a format (type None)");
		return OpStatus::ERR;
	}
	else
	{
		OP_ASSERT(!"Unknown OpScopeTPMessage::Type, add extra 'else if' condition");
		return OpStatus::ERR;
	}
}
#endif // SCOPE_SUPPORT
