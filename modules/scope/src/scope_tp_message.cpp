/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_tp_message.h"

#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/ecmascript/ecmascript.h"

/* OpScopeTPHeader */

OP_STATUS
OpScopeTPHeader::SetType(int t)
{
	if (t < MessageTypeMin || t > MessageTypeMax)
		return OpStatus::ERR;
	type = (MessageType)t;
	return OpStatus::OK;
}

/* OpScopeTPMessage */

OpScopeTPMessage::~OpScopeTPMessage()
{
	RETURN_VOID_IF_ERROR(Free());
}

OP_STATUS
OpScopeTPMessage::Construct(const ByteBuffer &msg, MessageType type)
{
	return SetData(msg, type);
}

void
OpScopeTPMessage::Clear()
{
	OpScopeTPHeader::Clear();
	if (Type() == ProtocolBuffer || Type() == JSON || Type() == XML)
	{
		if (data)
			data->Clear();
	}
}

ByteBuffer *
OpScopeTPMessage::Data()
{
	OP_ASSERT(Type() == ProtocolBuffer || Type() == JSON || Type() == XML);
	if (Type() != ProtocolBuffer && Type() != JSON && Type() != XML)
		return NULL;

	return data;
}

const ByteBuffer *
OpScopeTPMessage::Data() const
{
	OP_ASSERT(Type() == ProtocolBuffer || Type() == JSON || Type() == XML);
	if (Type() != ProtocolBuffer && Type() != JSON && Type() != XML)
		return NULL;

	return data;
}

OP_STATUS
OpScopeTPMessage::SetData(const ByteBuffer &msg, MessageType type)
{
	RETURN_IF_ERROR(CreateEmptyData(type));
	return OpScopeCopy(msg, *data);
}

OP_STATUS
OpScopeTPMessage::CreateEmptyData(MessageType type)
{
	OP_ASSERT(type == ProtocolBuffer || type == JSON || type == XML);
	if (type != ProtocolBuffer && type != JSON && type != XML)
		return OpStatus::ERR;
	RETURN_IF_ERROR(Free());

	SetType(type);
	data = OP_NEW(ByteBuffer, ());
	RETURN_OOM_IF_NULL(data);
	return OpStatus::OK;
}

ES_Object *
OpScopeTPMessage::GetESObject() const
{
	OP_ASSERT(Type() == ECMAScript);
	if (Type() != ECMAScript)
		return NULL;

	return es_object;
}

ES_Runtime *
OpScopeTPMessage::GetESRuntime() const
{
	OP_ASSERT(Type() == ECMAScript);
	if (Type() != ECMAScript)
		return NULL;

	return es_runtime;
}

ES_Object *
OpScopeTPMessage::ReleaseESObject()
{
	OP_ASSERT(Type() == ECMAScript);
	if (Type() != ECMAScript)
		return NULL;
	ES_Object *obj = es_object;
	if (obj)
		es_runtime->Unprotect(obj);
	es_object = NULL;
	es_runtime = NULL;
	SetType(None);

	return obj;
}

OP_STATUS
OpScopeTPMessage::SetESObject(ES_Object *obj, ES_Runtime *runtime)
{
	RETURN_IF_ERROR(Free());

	es_object = obj;
	if (!runtime->Protect(es_object))
	{
		es_object = NULL;
		return OpStatus::ERR;
	}
	es_runtime = runtime;
	SetType(ECMAScript);
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPMessage::Free()
{
	if (Type() == ECMAScript)
	{
		if (es_object)
			es_runtime->Unprotect(es_object);
		es_object = NULL;
	}
	else if (Type() == ProtocolBuffer ||
			 Type() == JSON ||
			 Type() == XML)
	{
		OP_DELETE(data);
		data = NULL;
	}
	SetType(None);
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPMessage::Copy(const OpScopeTPMessage &msg, BOOL include_data)
{
	RETURN_IF_ERROR(Free());

	RETURN_IF_ERROR(OpScopeTPHeader::Copy(msg));
	if (include_data && msg.data)
	{
		if (msg.Type() == ProtocolBuffer || msg.Type() == XML || msg.Type() == JSON)
			return SetData(*msg.data, msg.Type());
		else if (msg.Type() == ECMAScript)
			return SetESObject(msg.es_object, msg.es_runtime);
		else
			return OpStatus::OK;
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPMessage::Copy(const OpScopeTPHeader &msg)
{
	RETURN_IF_ERROR(Free());

	return OpScopeTPHeader::Copy(msg);
}

/*static*/
OP_STATUS
OpScopeTPMessage::Clone(OpAutoPtr<OpScopeTPMessage> &out, const OpScopeTPMessage &in, BOOL include_data)
{
	out.reset(OP_NEW(OpScopeTPMessage, ()));
	RETURN_OOM_IF_NULL(out.get());
	RETURN_IF_ERROR(out->SetServiceName(in.ServiceName()));
	return out->Copy(in, include_data);
}

/*static*/
OP_STATUS
OpScopeTPMessage::Clone(OpAutoPtr<OpScopeTPMessage> &out, const OpScopeTPHeader &in)
{
	out.reset(OP_NEW(OpScopeTPMessage, ()));
	RETURN_OOM_IF_NULL(out.get());
	return out->Copy(in);
}

/* OpScopeTPError */

OP_STATUS
OpScopeTPError::Copy(const OpScopeTPError &error)
{
	status = error.status;
	static_description = error.static_description;
	if (error.description.Length() > 0)
		RETURN_IF_ERROR(description.Set(error.description));
	line = error.line;
	col = error.col;
	offset = error.offset;
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPError::SetFormattedDescription(const uni_char *format, ...)
{
	va_list args;
	va_start(args, format);

	TempBuffer buf;
	OP_STATUS err = buf.AppendVFormat(format, args);

	va_end(args);

	RETURN_IF_ERROR(err);

	return SetDescription(buf.GetStorage());
}


#endif // SCOPE_SUPPORT
