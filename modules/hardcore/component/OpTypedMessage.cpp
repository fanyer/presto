/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/hardcore/component/OpTypedMessage.h"
#include "modules/hardcore/component/OpChannel.h"
#include "modules/hardcore/component/OpComponentManager.h"
#include "modules/hardcore/component/Messages.h"
#include "modules/otl/list.h"
#include "modules/hardcore/mem/mem_man.h"

// For protobuf serialization/deserialization
#include "modules/protobuf/src/protobuf_input.h"
#include "modules/protobuf/src/protobuf_output.h"
#include "modules/protobuf/src/protobuf_message.h"
#include "modules/protobuf/src/protobuf_debug.h"

#include "modules/hardcore/src/generated/g_proto_hardcore_component.h"
#include "modules/protobuf/src/generated/g_protobuf_descriptors.h"


/* static */ OpMessageAddress
OpMessageAddress::Deserialize(const OpData& data)
{
	int m = -1;
	int o = -1;
	int h = -1;
	OpAutoPtr< OtlCountedList<OpData> > triple(data.Split('.', 2));
	if (triple->Length() != 3 ||
		OpStatus::IsError(triple->PopFirst().ToInt(&m)) ||
		OpStatus::IsError(triple->PopFirst().ToInt(&o)) ||
		OpStatus::IsError(triple->PopFirst().ToInt(&h)))
		return OpMessageAddress();
	OP_ASSERT(triple->IsEmpty());
	return OpMessageAddress(m, o, h);
}

OP_STATUS
OpMessageAddress::Serialize(OpData& data) const
{
	return data.AppendFormat("%d.%d.%d", cm, co, ch);
}

// Used in Deserialize and Serialize
typedef OpHardcoreComponent_MessageSet::TypedMessage TypedMessage;
typedef OpHardcoreComponent_MessageSet::TypedMessage::Address MessageAddress;

void
ExportAddress(MessageAddress &out_address, const OpMessageAddress &in_address)
{
	out_address.SetComponentManager(in_address.cm);
	out_address.SetComponent(in_address.co);
	out_address.SetChannel(in_address.ch);
}

OpMessageAddress
ImportAddress(const MessageAddress &address)
{
	return OpMessageAddress(address.GetComponentManager(), address.GetComponent(), address.GetChannel());
}

#if defined(DEBUG) && !defined(NO_CORE_COMPONENTS)
Debug& operator<<(Debug& d, const struct OpMessageAddress& addr)
{
	return d << "<" << addr.cm << "." << addr.co << "." << addr.ch << ">";
}
#endif // DEBUG && !NO_CORE_COMPONENTS

#ifdef OPDATA_STRINGSTREAM
OpUniStringStream& operator<<(OpUniStringStream& in, const struct OpMessageAddress& addr)
{
	return in << UNI_L("<") << addr.cm << UNI_L(".") << addr.co << UNI_L(".") << addr.ch << UNI_L(">");
}
#endif // OPDATA_STRINGSTREAM

/* static */ OpTypedMessage*
OpSerializedMessage::Deserialize(const OpSerializedMessage* message)
{
	OP_ASSERT(message);
	TypedMessage typed_message;

	OpProtobufInputStream stream;
	RETURN_VALUE_IF_ERROR(stream.Construct(message->data), NULL);
	OpProtobufInstanceProxy instance(TypedMessage::GetMessageDescriptor(PROTOBUF_DESCRIPTOR(desc_hardcore_component)), reinterpret_cast<void *>(&typed_message));
	OP_STATUS status = stream.Read(instance);
	if (OpStatus::IsError(status))
	{
		PROTOBUF_DEBUG("OpSerializedMessage", ("Failed to decode TypedMessage"));
		return NULL;
	}

	OpMessageAddress src = ImportAddress(typed_message.GetSource());
	OpMessageAddress dst = ImportAddress(typed_message.GetDestination());
	if (!src.IsValid() || !dst.IsValid())
		return NULL;

	double delay = typed_message.GetDueTime();
	if (delay > 0)
		delay -= g_component_manager->GetRuntimeMS();

	int type_value = typed_message.GetType();
	OP_ASSERT(OpTypedMessage::VerifyType(type_value));
	if (OpTypedMessage::VerifyType(type_value))
	{
		OpTypedMessage* msg = OpTypedMessage::DeserializeData(type_value, src, dst, delay, typed_message.GetData());
#ifdef PROTOBUF_DEBUG_SUPPORT
		if (msg == NULL)
		{
			PROTOBUF_DEBUG("OpSerializedMessage", ("Failed to decode message with ID=%d", type_value));
		}
#endif // PROTOBUF_DEBUG_SUPPORT
		return msg;
	}

	// LOG("Unknown message type '" OPDATA_FORMAT "'!", OPDATA_FORMAT_PARAM(type));
	return NULL;
}

/* static */ OpSerializedMessage*
OpTypedMessage::Serialize(const OpTypedMessage* message)
{
	OP_ASSERT(message && message->IsValid());

	TypedMessage typed_message;

	ExportAddress(typed_message.GetSourceRef(), message->GetSrc());
	ExportAddress(typed_message.GetDestinationRef(), message->GetDst());

	typed_message.SetDueTime(message->GetDueTime());
	typed_message.SetType(message->GetType());

	OpData &message_data = typed_message.GetDataRef();
	RETURN_VALUE_IF_ERROR(message->Serialize(message_data), NULL);

	OpData data;
	OpProtobufOpDataOutputRange range(data);
	OpProtobufOutputStream stream(&range);
	OpProtobufInstanceProxy instance(TypedMessage::GetMessageDescriptor(PROTOBUF_DESCRIPTOR(desc_hardcore_component)), reinterpret_cast<void *>(&typed_message));
	RETURN_VALUE_IF_ERROR(stream.Write(instance), NULL);

	return OP_NEW(OpSerializedMessage, (data));
}

OP_STATUS
OpTypedMessage::Reply(OpTypedMessage* reply) const
{
	if (!g_component)
	{
		OP_DELETE(reply);
		return OpStatus::ERR_NULL_POINTER;
	}
	return g_component->ReplyTo(*this, reply);
}

OpTypedMessage::OpTypedMessage(
	OpMessageType type,
	const OpMessageAddress& src,
	const OpMessageAddress& dst,
	double delay)
	: m_type(type)
	, m_src(src)
	, m_dst(dst)
	, m_due_time(0)
{
	if (delay > 0 && g_component_manager)
		m_due_time = g_component_manager->GetRuntimeMS() + delay;
}

/* virtual */ double
OpTypedMessage::GetDelay(double now /* = 0 */) const
{
	if (!now && g_component_manager)
		now = g_component_manager->GetRuntimeMS();
	return MAX(0, m_due_time - now);
}

#ifdef OPDATA_STRINGSTREAM
OpUniStringStream& OpTypedMessage::ToStream(OpUniStringStream& in) const
{
	const char* msgType = GetTypeString();
	if (!msgType)
		return in << UNI_L("Unknown Type (NULL!)");

	if (GetType() != OpLegacyMessage::Type)
	{
		OpString16 messageType;
		OpStatus::Ignore(messageType.SetFromUTF8(msgType));
		return in << messageType.CStr();
	}

	// There are many Legacy messages so we must log the particular type.
	// Type of an OpLegacyMessage is returned by OpLegacyMessage::GetMessage().

	// This is temporary until OpLegacyMessage gets a virtual ToStream!
	const OpLegacyMessage& legacy = static_cast<const OpLegacyMessage&>(*this);
#ifdef MESSAGE_NAMES
	OpString16 messageName;
	OpStatus::Ignore(messageName.SetFromUTF8(GetStringFromMessage(legacy.GetMessage())));
	in << messageName.CStr();
#else
	in  << UNI_L("Legacy #") << legacy.GetMessage();
#endif
	if (legacy.HasParam1())
		in << UNI_L(" param1: ") << static_cast<long>(legacy.GetParam1());
	if (legacy.HasParam2())
		in << UNI_L(" param2: ") << static_cast<long>(legacy.GetParam2());
	return in;
}
#endif // OPDATA_STRINGSTREAM

#ifdef OPDATA_STRINGSTREAM
OpUniStringStream& operator<<(OpUniStringStream& in, const OpTypedMessage& msg)
{
	return msg.ToStream(in) << UNI_L(" ") << msg.GetSrc()
							<< UNI_L(" >> ") << msg.GetDst()
							<< UNI_L(" due: ") << msg.GetDueTime();
}
#endif // OPDATA_STRINGSTREAM

/* Manual implementations of specific OpTypedMessages follow */
OP_USE_MEMORY_POOL_IMPL(g_memory_manager->GetLegacyMessagePool(), OpLegacyMessage);
