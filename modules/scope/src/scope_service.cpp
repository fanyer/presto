/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/scope_network.h"
#include "modules/scope/src/scope_tp_message.h"
#include "modules/scope/src/scope_transport.h"
#include "modules/protobuf/src/protobuf_message.h"
#include "modules/scope/src/scope_default_message.h"
#include "modules/scope/src/scope_network.h"
#include "modules/xmlutils/xmlfragment.h"

/* OpScopeService */

/* Error messages */

/*static*/
const uni_char *
OpScopeService::GetInvalidStatusFieldText()
{
	return UNI_L("Status field must not be set for calls");
}

/*static*/
const uni_char *
OpScopeService::GetIncorrectServiceText()
{
	return UNI_L("Message was sent to incorrect service");
}

/*static*/
const uni_char *
OpScopeService::GetCommandNotFoundText()
{
	return UNI_L("The command ID was not found");
}

/*static*/
const uni_char *
OpScopeService::GetInitAsyncCommandFailedText()
{
	return UNI_L("Async command could not be initialized");
}

/*static*/
const uni_char *
OpScopeService::GetParseCommandMessageFailedText()
{
	return UNI_L("Unknown error while parsing command message");
}

/*static*/
const uni_char *
OpScopeService::GetCommandExecutionFailedText()
{
	return UNI_L("Unknown error while executing command");
}

/*static*/
const uni_char *
OpScopeService::GetCommandResponseFailedText()
{
	return UNI_L("Unknown error while sending response");
}

/* Code */

OpScopeService::OpScopeService(const uni_char* name, OpScopeServiceManager *service_manager, ControlType control)
	: name(name)
	, manager(NULL)
	, is_enabled(control == CONTROL_MANUAL ? FALSE : TRUE)
	, client(NULL)
	, is_response_sent(FALSE)
	, control(control)
{
	if (service_manager)
		service_manager->RegisterService(this);
}

OpScopeService::~OpScopeService()
{
	async_commands.Clear();
	if (GetManager())
		GetManager()->UnregisterService(this);
	if (GetControlType() == CONTROL_MANUAL && IsEnabled())
		OpStatus::Ignore(Disable());
}

/* virtual */
OP_STATUS
OpScopeService::OnPostInitialization()
{
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeService::OnServiceEnabled()
{
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeService::OnServiceDisabled()
{
	return OpStatus::OK;
}

OP_STATUS
OpScopeService::Enable()
{
	OP_ASSERT(!IsEnabled());
	// Special case for always-enabled services
	if (GetControlType() == CONTROL_FORCED)
		return OpStatus::OK;

	if (IsEnabled())
		return OpStatus::ERR;

	SetIsEnabled(TRUE);
	async_commands.Clear(); // Remove any active async commands
	if (OpStatus::IsError(OnServiceEnabled()))
	{
		SetIsEnabled(FALSE);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeService::Disable()
{
	OP_ASSERT(IsEnabled());
	// Special case for always-enabled services
	if (GetControlType() == CONTROL_FORCED)
		return OpStatus::OK;

	if (!IsEnabled())
		return OpStatus::ERR;

	SetIsEnabled(FALSE);
	async_commands.Clear(); // Remove any active async commands
	return OnServiceDisabled();
}

/* virtual */
OP_STATUS
OpScopeService::OnReceive( OpScopeClient *client, const OpScopeTPMessage &msg )
{
	OP_ASSERT(this->client == client);

	is_response_sent = FALSE;
	RETURN_IF_ERROR(current_command.Copy(msg));
	command_error.SetStatus(OpScopeTPHeader::OK);

	OP_STATUS status = DoReceive(client, msg);

	is_response_sent = FALSE;
	OpScopeTPHeader tmp_header;
	RETURN_IF_ERROR(current_command.Copy(tmp_header));

	// Check if the command or any sub-calls sets a command error, if so send this to the client
	// If response is already sent we cannot report any kind of error
	// TODO: Perhaps use the scope.OnError event for this?
	if (command_error.GetStatus() != OpScopeTPHeader::OK && !IsResponseSent())
	{
		RETURN_IF_ERROR(SendError(client, msg, command_error));
		return OpStatus::OK;
	}
	return status;
}

OP_STATUS
OpScopeService::SetCommandError(OpScopeTPHeader::MessageStatus status, const uni_char *description, int line, int col, int offset)
{
	OP_ASSERT(status != OpScopeTPHeader::OK);
	OpScopeTPError temp(status, description, line, col, offset);
	RETURN_IF_ERROR(command_error.Copy(temp));
	return OpStatus::ERR;
}

OP_STATUS
OpScopeService::SetCommandError(const OpScopeTPError &error)
{
	OP_ASSERT(error.GetStatus() != OpScopeTPHeader::OK);
	RETURN_IF_ERROR(command_error.Copy(error));
	return OpStatus::ERR;
}

OP_STATUS
OpScopeService::ReportCommandStatus(OP_STATUS status)
{
	OP_ASSERT(OpStatus::IsError(status));

	OpScopeTPError error;
	error.SetStatus(OpScopeTPMessage::InternalError);
	if (OpStatus::IsMemoryError(status))
	{
		error.SetStatus(OpScopeTPMessage::OutOfMemory);
		error.SetStaticDescription(UNI_L("Service encountered OOM while decoding incoming request"));
	}
	else if (status == OpStatus::ERR_NO_SUCH_RESOURCE)
		error.SetStaticDescription(UNI_L("Unable to find the specified resource"));
	else if (status == OpStatus::ERR_OUT_OF_RANGE)
		error.SetStaticDescription(UNI_L("Input out of range"));
	else if (status == OpStatus::ERR_PARSING_FAILED)
		error.SetStaticDescription(UNI_L("Parsing failed while executing command"));
	else if (status == OpStatus::ERR_NO_DISK)
		error.SetStaticDescription(UNI_L("Disk is full"));
	else if (status == OpStatus::ERR_NO_ACCESS)
		error.SetStaticDescription(UNI_L("Attempting to write a read-only entity"));
	else if (status == OpStatus::ERR_NOT_DELAYED)
		error.SetStaticDescription(UNI_L("Posted message was not delayed"));
	else if (status == OpStatus::ERR_FILE_NOT_FOUND)
		error.SetStaticDescription(UNI_L("File not found or could not be opened"));
	else if (status == OpStatus::ERR_BAD_FILE_NUMBER)
		error.SetStaticDescription(UNI_L("Wrong socket or file ID"));
	else if (status == OpStatus::ERR_NULL_POINTER)
		error.SetStaticDescription(UNI_L("Encountered a null pointer"));
	else if (status == OpStatus::ERR_NOT_SUPPORTED)
		error.SetStaticDescription(UNI_L("Unsupported operation"));
	else
		error.SetStaticDescription(GetCommandExecutionFailedText());
	return SetCommandError(error);
}

OP_STATUS
OpScopeService::FormatCommandError(OpScopeTPError &error, uni_char *buffer, size_t count, const uni_char *format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	int result = uni_vsnprintf(buffer, count, format, arg_ptr);
	va_end(arg_ptr);
	if (result != 0)
		RETURN_IF_ERROR(error.SetDescription(buffer));
	return result == 0 ? OpStatus::ERR : OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeService::DoReceive( OpScopeClient *, const OpScopeTPMessage & )
{
	// TODO: Remove this implementation
	return OpStatus::OK;
}

OP_STATUS
OpScopeService::SendAsyncError( unsigned int async_tag, const OpScopeTPError &error_data )
{
	AsyncCommand *async_command = GetAsyncCommand(async_tag);
	RETURN_VALUE_IF_NULL(async_command, OpStatus::ERR_NO_SUCH_RESOURCE);
	OP_ASSERT(client != NULL);
	if (!client) 
	{
		CleanupAsyncCommand(async_tag);
		return OpStatus::ERR_NULL_POINTER;
	}
	OpAutoPtr<OpScopeTPMessage> out_msg;
	OP_STATUS status = OpScopeTPMessage::Clone(out_msg, async_command->header);
	CleanupAsyncCommand(async_tag);
	RETURN_IF_ERROR(status);

	RETURN_IF_ERROR(client->SerializeError(*out_msg.get(), error_data, out_msg->Type()));
	return client->Receive(out_msg);
}

OP_STATUS
OpScopeService::SendAsyncError(unsigned async_tag, unsigned responseID, OP_STATUS command_status)
{
	if (!OpStatus::IsError(command_status))
		return OpStatus::OK;

	AsyncCommand *async_command = GetAsyncCommand(async_tag);
	RETURN_VALUE_IF_NULL(async_command, OpStatus::ERR_NO_SUCH_RESOURCE);

	OP_ASSERT(client != NULL);
	if (!client)
	{
		CleanupAsyncCommand(async_tag);
		return OpStatus::ERR_NULL_POINTER;
	}

	// Only STP/1 can receive errors
	if (client->GetHost()->GetVersion() < 1)
	{
		CleanupAsyncCommand(async_tag);
		return OpStatus::OK;
	}

	OpScopeTPError error;
	error.SetStatus(OpScopeTPMessage::InternalError);
	OpAutoPtr<OpScopeTPMessage> error_message;
	OP_STATUS status = OpScopeTPMessage::Clone(error_message, async_command->header);
	CleanupAsyncCommand(async_tag);
	RETURN_IF_ERROR(status);

	if (OpStatus::IsMemoryError(command_status))
	{
		// TODO: Perhaps the error message should be statically (or pre-) allocated to avoid allocation problems when sending it out
		error.SetStatus(OpScopeTPMessage::OutOfMemory);
		error.SetStaticDescription(UNI_L("OOM while executing command"));
	}
	else if (command_status == OpStatus::ERR_PARSING_FAILED)
		error.SetStaticDescription(UNI_L("Parsing failed while executing command"));
	else if (command_status == OpStatus::ERR_NULL_POINTER)
		error.SetStaticDescription(UNI_L("Null pointer while executing command"));
	else if (command_status == OpStatus::ERR_NOT_SUPPORTED)
		error.SetStaticDescription(UNI_L("Unsupported action encountered while executing command"));
	else
		error.SetStaticDescription(UNI_L("Internal error while executing command"));
	RETURN_IF_ERROR(client->SerializeError(*error_message.get(), error, error_message->Type()));
	return client->Receive(error_message);
}

OP_STATUS
OpScopeService::SendError( OpScopeClient *client, const OpScopeTPMessage &orig_message, const OpScopeTPError &error_data )
{
	OP_ASSERT(client != NULL);
	if (client == NULL)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<OpScopeTPMessage> outmsg;
	RETURN_IF_ERROR(OpScopeTPMessage::Clone(outmsg, orig_message, FALSE));
	outmsg->SetVersion(OpScopeTPMessage::Version_1);
	outmsg->SetServiceName(GetName());

	RETURN_IF_ERROR(client->SerializeError(*outmsg.get(), error_data, orig_message.Type()));
	RETURN_IF_ERROR(client->Receive(outmsg));
	is_response_sent = TRUE;

	return OpStatus::OK;
}

OP_STATUS
OpScopeService::SerializeResponse(OpScopeClient *client, OpAutoPtr<OpScopeTPMessage> &out, const OpScopeTPHeader &original, const OpProtobufInstanceProxy &proxy, unsigned int responseID)
{
	OP_ASSERT(client != NULL);
	if (client == NULL)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(OpScopeTPMessage::Clone(out, original));
	out->SetTransportType(OpScopeTPMessage::STP_Response);
	RETURN_IF_ERROR(out->SetServiceName(GetName()));

	out->SetCommandID(responseID);
	OP_STATUS status = client->Serialize(*out.get(), proxy, original.Type());
	if (OpStatus::IsError(status))
	{
		// Failed to serialize the message, report the error
		// This will cause an error message to be sent back to the client
		return ReportCommandStatus(status);
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeService::SendResponse( OpScopeClient *client, const OpScopeTPHeader &original, const OpProtobufInstanceProxy &proxy )
{
	OP_ASSERT(client != NULL);
	if (client == NULL)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<OpScopeTPMessage> out_msg;
	RETURN_IF_ERROR(SerializeResponse(client, out_msg, original, proxy, original.CommandID()));
	RETURN_IF_ERROR(client->Receive(out_msg));
	is_response_sent = TRUE;
	return OpStatus::OK;
}

OP_STATUS
OpScopeService::SendAsyncResponse( unsigned int async_tag, const OpProtobufInstanceProxy &proxy, unsigned int responseID )
{
	AsyncCommand *async_command = GetAsyncCommand(async_tag);
	RETURN_VALUE_IF_NULL(async_command, OpStatus::ERR_NO_SUCH_RESOURCE);

	OP_ASSERT(client != NULL);
	OP_STATUS status = OpStatus::ERR_NULL_POINTER;
	if (client)
	{
		OpAutoPtr<OpScopeTPMessage> out_msg;
		status = SerializeResponse(client, out_msg, async_command->header, proxy, responseID);
		if (OpStatus::IsSuccess(status))
		{
			status = client->Receive(out_msg);
			is_response_sent = TRUE;
		}
	}

	CleanupAsyncCommand(async_tag);
	return status;
}

OP_STATUS
OpScopeService::SendEvent( const OpProtobufInstanceProxy &proxy, unsigned int responseID )
{
	OP_ASSERT(client);
	if (client == NULL)
		return OpStatus::ERR_NULL_POINTER;
	OpScopeTPMessage::MessageType type = client->GetMessageType();

	OpScopeTPHeader::MessageVersion version = OpScopeTPHeader::DefaultVersion;
	OpAutoPtr<OpScopeTPMessage> out_msg(OP_NEW(OpScopeTPMessage ,(OpScopeTPMessage::STP_Event, GetName(), responseID, OpScopeTPMessage::OK, 0, version)));
	RETURN_OOM_IF_NULL(out_msg.get());

	out_msg->SetCommandID(responseID);
	client->Serialize(*out_msg.get(), proxy, type);
	RETURN_IF_ERROR(client->Receive(out_msg));
	return OpStatus::OK;
}

/*static*/
OpScopeTPHeader::MessageStatus
OpScopeService::ErrorCode(OP_STATUS status)
{
	if (OpStatus::IsSuccess(status))
		return OpScopeTPHeader::OK;
	switch (status)
	{
	case OpStatus::ERR_NO_MEMORY:
		return OpScopeTPHeader::OutOfMemory;
	case OpStatus::ERR:
	case OpStatus::ERR_PARSING_FAILED:
		return OpScopeTPHeader::BadRequest;
	default:
		return OpScopeTPHeader::InternalError;
	}
}

OP_STATUS
OpScopeService::InitAsyncCommand(const OpScopeTPHeader &header, unsigned &async_tag)
{
	unsigned tag = 1;
	AsyncCommand *async_command = async_commands.First();
	if (async_command)
	{
		unsigned min_tag = UINT_MAX, max_tag = 1;
		for (; async_command; async_command = async_command->Suc())
		{
			min_tag = MIN(min_tag, async_command->tag);
			max_tag = MAX(max_tag, async_command->tag);
		}
		tag = min_tag > 1 ? min_tag - 1 : max_tag + 1;
	}
	OP_ASSERT(tag < UINT_MAX && tag >= 1);
	async_command = OP_NEW(AsyncCommand, (tag));
	RETURN_OOM_IF_NULL(async_command);
	RETURN_IF_ERROR(async_command->Construct(header));
	async_command->Into(&async_commands);
	async_tag = tag;
	return OpStatus::OK;
}

void
OpScopeService::CleanupAsyncCommand(unsigned async_tag)
{
	AsyncCommand *async_command = GetAsyncCommand(async_tag);
	if (async_command)
	{
		async_command->Out();
		OP_DELETE(async_command);
	}
}

OpScopeService::AsyncCommand *
OpScopeService::GetAsyncCommand(unsigned async_tag)
{
	for (AsyncCommand *async_command = async_commands.First(); async_command; async_command = async_command->Suc())
	{
		if (async_command->tag == async_tag)
			return async_command;
	}
	return NULL;
}

OP_STATUS
OpScopeService::ParseMessage( OpScopeClient *client, const OpScopeTPMessage &msg, OpProtobufInstanceProxy &proxy )
{
	OpScopeTPError error;
	OP_STATUS status = client->Parse(msg, proxy, error);
	if (error.GetStatus() != OpScopeTPHeader::OK)
		SetCommandError(error);
	return status;
}

/*virtual*/
OpScopeService::EnumIDRange
OpScopeService::GetEnumIDs() const
{
	// Default is to return an empty range
	return EnumIDRange(NULL, 0);
}

/*virtual*/
unsigned
OpScopeService::GetEnumCount() const
{
	// Service has no enums by default
	return 0;
}

/*virtual*/
BOOL
OpScopeService::HasEnum(unsigned enum_id) const
{
	return FALSE;
}

/*virtual*/
OP_STATUS
OpScopeService::GetEnum(unsigned enum_id, const uni_char *&name, unsigned &value_count) const
{
	// Service has no enums by default
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

/*virtual*/
OP_STATUS
OpScopeService::GetEnumValue(unsigned enum_id, unsigned idx, const uni_char *&value_name, unsigned &value_number) const
{
	// Service has no enums by default
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}


/* virtual */ void
OpScopeService::FilterChanged()
{
}

/* virtual */ void
OpScopeService::WindowClosed(Window *window)
{
}

void 
OpScopeService::SilentError(OP_STATUS status)
{
	if(OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);
}

/* OpScopeMetaService */

OpScopeMetaService::OpScopeMetaService(const uni_char* id, OpScopeServiceManager *manager)
	: OpScopeService(id, manager, CONTROL_FORCED)
{
}

/* virtual */ 
OpScopeMetaService::~OpScopeMetaService()
{
}

/* virtual */ const char *
OpScopeMetaService::GetVersionString() const
{
	return "0.0";
}

/* virtual */ int 
OpScopeMetaService::GetMajorVersion() const
{
	return 0;
}

/* virtual */ int 
OpScopeMetaService::GetMinorVersion() const
{
	return 0;
}

/* virtual */ const char *
OpScopeMetaService::GetPatchVersion() const
{
	return "0";
}

#endif // SCOPE_SUPPORT
