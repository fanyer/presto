/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT
# ifdef SCOPE_MESSAGE_TRANSCODING

#include "modules/scope/src/scope_transcoder.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_transport.h"
#include "modules/protobuf/src/protobuf_message.h"

/* OpScopeTranscodeManager */

OpScopeTranscodeManager::OpScopeTranscodeManager()
	: transcoding_format(OpScopeProxy::Format_None)
{
}

OP_STATUS
OpScopeTranscodeManager::SetTranscodingFormat(OpScopeProxy::TranscodingFormat format)
{
	transcoding_format = format;
	if (OpScopeTranscoder *transcoder = GetTranscoder())
		return transcoder->SetTranscoderFormat(format);
	return OpStatus::OK;
}

void
OpScopeTranscodeManager::GetTranscodingStats(unsigned &client_to_host, unsigned &host_to_client)
{
	if (OpScopeTranscoder *transcoder = GetTranscoder())
	{
		client_to_host = transcoder->GetClientToHostTranscodedCount();
		host_to_client = transcoder->GetHostToClientTranscodedCount();
	}
	else
		client_to_host = host_to_client = 0;
}

OP_STATUS
OpScopeTranscodeManager::OnNewTranscoder()
{
	if (OpScopeTranscoder *transcoder = GetTranscoder())
		return transcoder->SetTranscoderFormat(GetTranscodingFormat());
	return OpStatus::OK;
}

/* OpScopeTranscoder */

OpScopeTranscoder::OpScopeTranscoder(OpScopeTranscoderHost &host)
	: host(host)
	, transcoder_enabled(FALSE)
	, client_format(OpScopeTPHeader::None)
	, transcoded_client_to_host(0)
	, transcoded_host_to_client(0)
	, service_manager(NULL)
	, setup_state(SetupState_HostInfo)
	, setup_tag(1)
{
}

BOOL
OpScopeTranscoder::IsEnabled() const
{
	return transcoder_enabled;
}

BOOL
OpScopeTranscoder::IsActive() const
{
	return IsEnabled() && transcoding_format != OpScopeProxy::Format_None;
}

void
OpScopeTranscoder::Disable()
{
	SetEnabled(FALSE);
	OpStatus::Ignore(SetTranscoderFormat(OpScopeProxy::Format_None));
}

void
OpScopeTranscoder::SetEnabled(BOOL enabled)
{
	transcoder_enabled = enabled;
}

OP_STATUS
OpScopeTranscoder::PreProcessMessageToClient(const OpScopeTPMessage &message, OpScopeClient *client, BOOL &skip)
{
	skip = FALSE;
	if (!IsActive())
		return OpStatus::OK;

	// Check if we receive a response to our scope.Connect call, if so do not send it to the client
	if (message.CommandID() == OpScopeProtocolService::Command_Connect &&
		message.ServiceName().Compare(UNI_L("scope")) == 0 &&
		message.Tag() == 0)
	{
		skip = TRUE;
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeTranscoder::TranscodeMessageToClient(OpScopeTPMessage &message_copy, OpScopeClient *client, const OpScopeTPMessage &message)
{
	if (!IsActive())
		return OpStatus::OK;

	// If transcoding is on we need to convert it to a format the client can understand, if possible
	OpScopeTPHeader::MessageType type = client->GetMessageType();
	if (type != message_copy.Type())
	{
		const OpProtobufMessage *proto_message = GetProtobufMessage(service_manager, &message);
		OP_ASSERT(proto_message);
		RETURN_VALUE_IF_NULL(proto_message, OpStatus::ERR_NULL_POINTER);

		OpProtobufMessage::MakeFunc make = proto_message->GetMakeFunction();
		void *instance_ptr = make();
		OP_ASSERT(instance_ptr);
		RETURN_OOM_IF_NULL(instance_ptr);
		OpProtobufInstanceProxy proxy(proto_message, instance_ptr);

		OP_STATUS status = TranscodeToClient(client, proxy, message_copy, message, type);

		OpProtobufMessage::DestroyFunc destroy = proto_message->GetDestroyFunction();
		destroy(instance_ptr);

		if (OpStatus::IsError(status))
			return status;
		++transcoded_host_to_client;
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeTranscoder::PreProcessMessageFromClient(OpScopeTPMessage &message, OpScopeClient *client, BOOL &skip)
{
	skip = FALSE;
	if (!IsEnabled())
		return FALSE;

	// If the client sends a connect signal we need to take action
	if (message.CommandID() == OpScopeProtocolService::Command_Connect && message.ServiceName().Compare(UNI_L("scope")) == 0)
	{
		skip = FALSE;

		// Record the wanted format, needed when turning off transcoding to reset the format
		OpScopeProtocolService::ClientInfo client_info;
		OpProtobufInstanceProxy proxy(OpScopeProtocolService::ClientInfo::GetMessageDescriptor(g_scope_manager->GetDescriptorSet().scope_), static_cast<void *>(&client_info));
		OpScopeTPError error;
		RETURN_IF_ERROR(client->Parse(message, proxy, error));
		RETURN_IF_ERROR(OpScopeProtocolService::ParseFormat(client_info.GetFormat(), client_format));
		if (transcoding_format != OpScopeProxy::Format_None)
		{
			// Transcoding is currently active, change the format to the transcoder format
			OpScopeTPHeader::MessageType message_format = ToMessageType(transcoding_format);
			OP_ASSERT(message_format != OpScopeTPHeader::None);
			const uni_char *format_name = NULL;
			RETURN_IF_ERROR(OpScopeProtocolService::GenerateFormat(message_format, format_name));
			RETURN_IF_ERROR(client_info.SetFormat(format_name));
			RETURN_IF_ERROR(client->Serialize(message, proxy, message.Type()));
			// TODO: Is this correct?
		}
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeTranscoder::TranscodeMessageFromClient(OpScopeTPMessage &new_message, OpScopeClient *client, const OpScopeTPMessage &message)
{
	if (!IsActive())
		return OpStatus::OK;

	// Transcode to given format by using an instance and protocol buffer definitions
	const OpProtobufMessage *proto_message = GetProtobufMessage(service_manager, &message);
	OP_ASSERT(proto_message);
	RETURN_VALUE_IF_NULL(proto_message, OpStatus::ERR_NULL_POINTER);

	OpProtobufMessage::MakeFunc make = proto_message->GetMakeFunction();
	void *instance_ptr = make();
	OP_ASSERT(instance_ptr);
	RETURN_OOM_IF_NULL(instance_ptr);
	OpProtobufInstanceProxy proxy(proto_message, instance_ptr);

	OP_STATUS status = TranscodeFromClient(client, proxy, new_message, message, transcoding_format);

	OpProtobufMessage::DestroyFunc destroy = proto_message->GetDestroyFunction();
	destroy(instance_ptr);

	if (OpStatus::IsError(status))
		return status;
	++transcoded_client_to_host;
	return OpStatus::OK;
}

BOOL
OpScopeTranscoder::IsSetupFinished() const
{
	return setup_state == SetupState_Finished;
}

OP_STATUS
OpScopeTranscoder::ProcessSetup(const OpScopeTPMessage *message)
{
	switch (setup_state)
	{
	case SetupState_HostInfo:
		{
			setup_state = SetupState_Finished;

			OpAutoPtr<OpScopeTPMessage> message(OP_NEW(OpScopeTPMessage, ()));
			RETURN_OOM_IF_NULL(message.get());
			RETURN_IF_ERROR(message->SetServiceName(UNI_L("scope")));
			message->SetCommandID(OpScopeProtocolService::Command_HostInfo);
			message->SetTag(setup_tag++);
			message->SetVersion(OpScopeTPHeader::Version_1);

			OpScopeDefaultMessage input;
			OpProtobufInstanceProxy instance(OpScopeDefaultMessage::GetMessageDescriptor(), static_cast<void *>(&input));
			RETURN_IF_ERROR(OpScopeClient::SerializeDefault(*message, instance, OpScopeTPHeader::ProtocolBuffer));
			RETURN_IF_ERROR(host.SendMessage(message));

			setup_state = SetupState_HostInfoResponse;
			break;
		}
	case SetupState_HostInfoResponse:
		{
			OP_ASSERT(message);
			if (!message)
				return OpStatus::OK;

			// Make sure the remote host has the exact same services and version as this instance, if not disable transcoder alltogether
			if (message->ServiceName().Compare(UNI_L("scope")) != 0 || message->CommandID() != OpScopeProtocolService::Command_HostInfo)
				break;

			transcoder_enabled = FALSE;
			OP_STATUS status = HandleTranscoderResult(message);
			setup_state = SetupState_Finished;

			// If HandleTranscoderResult() fails in any way we disable the transcoder
			if (OpStatus::IsError(status))
				transcoder_enabled = FALSE;

			// Continue to SetupState_Finished
		}
	case SetupState_Finished:
		break;
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeTranscoder::HandleTranscoderResult(const OpScopeTPMessage *message)
{
	// We need the service manager to enable the transcoder
	if (!service_manager)
		return OpStatus::ERR;

	OpScopeProtocolService::HostInfo host_info;
	OpProtobufInstanceProxy instance(GetProtobufMessage(service_manager, message), reinterpret_cast<void *>(&host_info));
	if (instance.GetProtoMessage() == NULL)
		return OpStatus::ERR_NO_MEMORY;
	OpScopeTPError stp_error;
	RETURN_IF_ERROR(OpScopeClient::ParseDefault(*message, instance, stp_error));

	BOOL version_check = TRUE;
	for (unsigned i = 0; i < host_info.GetServiceList().GetCount(); ++i)
	{
		OpScopeProtocolService::Service *service_info = host_info.GetServiceList().Get(i);
		RETURN_VALUE_IF_NULL(service_info, OpStatus::ERR_NULL_POINTER);
		OpScopeService *service = service_manager->FindService(service_info->GetName());
		if ( !service || !TranscoderVersionCheck(service_info->GetVersion(), service->GetMajorVersion(), service->GetMinorVersion()) )
		{
			version_check = FALSE;
			break;
		}
	}
	if (version_check)
		transcoder_enabled = TRUE;
	return OpStatus::OK;
}

OP_STATUS
OpScopeTranscoder::SetTranscoderFormat(OpScopeProxy::TranscodingFormat format)
{
	OP_STATUS status = DoSetTranscoderFormat(format);
	if (OpStatus::IsError(status))
	{
		SetEnabled(FALSE);
		transcoding_format = OpScopeProxy::Format_None;
	}
	return status;
}

OP_STATUS
OpScopeTranscoder::DoSetTranscoderFormat(OpScopeProxy::TranscodingFormat format)
{
	transcoding_format = format;
	if (host.GetSTPVersion() >= 1)
	{
		// Notify the host of the format change
		if (format != OpScopeProxy::Format_None)
		{
			OpAutoPtr<OpScopeTPMessage> msg;
			RETURN_IF_ERROR(OpScopeProtocolService::CreateConfigureMessage(msg, NULL, 0, ToMessageType(format)));
			RETURN_IF_ERROR(host.SendMessage(msg));
		}
		else
		{
			if (client_format != OpScopeTPHeader::None)
			{
				OpAutoPtr<OpScopeTPMessage> msg;
				RETURN_IF_ERROR(OpScopeProtocolService::CreateConfigureMessage(msg, NULL, 0, client_format));
				RETURN_IF_ERROR(host.SendMessage(msg));
			}
		}
	}
	return OpStatus::OK;
}

const OpProtobufMessage *
OpScopeTranscoder::GetProtobufMessage(OpScopeServiceManager *service_manager, const OpScopeTPMessage *message)
{
	OP_ASSERT(service_manager);

	unsigned command_id = message->CommandID();
	OpScopeService *service = service_manager->FindService(message->ServiceName());
	const OpScopeCommand *command = NULL;
	OP_ASSERT(service);
	if (!service)
		return NULL;

	OpScopeService::CommandRange commands = service->GetCommands();
	for (; !commands.IsEmpty(); commands.PopFront())
	{
		if (static_cast<unsigned>(commands.Front()->number) == command_id)
		{
			command = commands.Front();
			break;
		}
	}
	OP_ASSERT(command);
	if (!command)
		return NULL;

	const OpProtobufMessage *proto_message = NULL;
	if (message->TransportType() == OpScopeTPHeader::STP_Call)
	{
		OP_ASSERT(command->type == OpScopeCommand::Type_Call);
		if (command->message_id)
		{
			proto_message = service->GetMessage(command->message_id);
			OP_ASSERT(proto_message);
		}
	}
	else if (message->TransportType() == OpScopeTPHeader::STP_Response)
	{
		OP_ASSERT(command->type == OpScopeCommand::Type_Call);
		if (command->response_id)
		{
			proto_message = service->GetMessage(command->response_id);
			OP_ASSERT(proto_message);
		}
	}
	else if (message->TransportType() == OpScopeTPHeader::STP_Event)
	{
		OP_ASSERT(command->type == OpScopeCommand::Type_Event);
		if (command->response_id)
		{
			proto_message = service->GetMessage(command->response_id);
			OP_ASSERT(proto_message);
		}
	}
	else if (message->TransportType() == OpScopeTPHeader::STP_Error)
	{
		proto_message = OpScopeService_SI::ErrorInfo::GetMessageDescriptor(g_scope_manager->GetDescriptorSet().scope_service_);
		OP_ASSERT(proto_message);
	}
	if (!proto_message)
	{
		// If no message descriptor it means it uses the default message object.
		proto_message = OpScopeDefaultMessage::GetMessageDescriptor();
	}
	// TODO: Use default message descriptor when no message was found
	return proto_message;
}

/*static*/
OpScopeTPHeader::MessageType
OpScopeTranscoder::ToMessageType(OpScopeProxy::TranscodingFormat format)
{
	if (format == OpScopeProxy::Format_ProtocolBuffer)
		return OpScopeTPHeader::ProtocolBuffer;
	else if (format == OpScopeProxy::Format_JSON)
		return OpScopeTPHeader::JSON;
	else if (format == OpScopeProxy::Format_XML)
		return OpScopeTPHeader::XML;
	OP_ASSERT(!"Unknown transcoding format, should not happen.");
	return OpScopeTPHeader::None;
}

/*static*/
OP_STATUS
OpScopeTranscoder::TranscodeFromClient( OpScopeClient *client, OpProtobufInstanceProxy &proxy, OpScopeTPMessage &to, const OpScopeTPMessage &from, OpScopeProxy::TranscodingFormat format )
{
	// Tell the client to convert from given format and into an instance
	OpScopeTPError error;
	RETURN_IF_ERROR(client->Parse(from, proxy, error));

	// Then convert from instance to actual format to send on the wire
	OpScopeTPHeader::MessageType type = ToMessageType(format);
	return OpScopeClient::SerializeDefault(to, proxy, type);
}

OP_STATUS
OpScopeTranscoder::TranscodeToClient( OpScopeClient *client, OpProtobufInstanceProxy &proxy, OpScopeTPMessage &to, const OpScopeTPMessage &from, OpScopeTPHeader::MessageType type )
{
	// Convert from wire format and into an instance
	OP_ASSERT(from.Type() == OpScopeTPHeader::ProtocolBuffer || from.Type() == OpScopeTPHeader::JSON || from.Type() == OpScopeTPHeader::XML);
	OpScopeTPError error;
	RETURN_IF_ERROR(OpScopeClient::ParseDefault(from, proxy, error));

	// Then tell client to convert from instance to desired format
	return client->Serialize(to, proxy, type);
}

/*static*/
BOOL
OpScopeTranscoder::TranscoderVersionCheck(const OpString &remote_version, int service_major_version, int service_minor_version)
{
	int major_end = remote_version.Find(".");
	if (major_end == KNotFound)
		return FALSE;
	OpString major_str;
	OpStatus::Ignore(major_str.Set(remote_version.CStr(), major_end));
	int minor_end = remote_version.Find(".", major_end + 1);
	OpString minor_str;
	OpStatus::Ignore(minor_str.Set(remote_version.SubString(major_end + 1).CStr(), minor_end == KNotFound ? KAll : (minor_end - major_end + 1) ));
	int remote_major_version = uni_atoi(major_str.CStr());
	int remote_minor_version = uni_atoi(minor_str.CStr());
	return remote_major_version == service_major_version && remote_minor_version == service_minor_version;
}

void
OpScopeTranscoder::SetServiceManager(OpScopeServiceManager *manager)
{
	service_manager = manager;
}

/* OpScopeProxy */

/*static*/
OP_STATUS
OpScopeProxy::SetTranscodingFormat(TranscodingFormat format)
{
	return g_scope_manager->SetTranscodingFormat(format);
}

/*static*/
void
OpScopeProxy::GetTranscodingStats(unsigned &client_to_host, unsigned &host_to_client)
{
	g_scope_manager->GetTranscodingStats(client_to_host, host_to_client);
}

# endif // SCOPE_MESSAGE_TRANSCODING
#endif // SCOPE_SUPPORT
