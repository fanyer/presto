/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf_input_common.h"
#include "modules/protobuf/src/protobuf_data.h"

/*static*/
OP_STATUS
OpProtobufInput::AddBool(BOOL value, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	if (field.GetQuantifier() == OpProtobufField::Repeated)
		return instance.FieldPtrOpINT32Vector(field_idx)->Add(value);
	else
		*instance.FieldPtrBOOL(field_idx) = value;
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufInput::AddString(const uni_char *text, int len, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		return instance.AddString(field_idx, OpProtobufStringChunkRange(text, len));
	}
	else
		return instance.SetString(field_idx, OpProtobufStringChunkRange(text, len));
}

/*static*/
OP_STATUS
OpProtobufInput::AddString(const OpProtobufDataChunkRange &range, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		return instance.AddString(field_idx, range);
	}
	else
	{
		return instance.SetString(field_idx, range);
	}
}

/*static*/
OP_STATUS
OpProtobufInput::AddBytes(const OpProtobufDataChunkRange &range, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		return instance.AddBytes(field_idx, range);
	}
	else
	{
		return instance.SetBytes(field_idx, range);
	}
}

/*static*/
OP_STATUS
OpProtobufInput::AddMessage(void *&sub_instance_ptr, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	const OpProtobufMessage *sub_message = field.GetMessage();
	if (sub_message == NULL)
		return OpStatus::ERR;
	OpProtobufRepeatedItems *sub_msg_vector_ptr = instance.FieldPtrOpProtobufRepeatedItems(field_idx);
	if (sub_msg_vector_ptr == NULL)
		return OpStatus::ERR;
	OpProtobufMessage::MakeFunc make_sub_msg = sub_message->GetMakeFunction();
	if (make_sub_msg == NULL)
		return OpStatus::ERR;
	OpProtobufMessage::DestroyFunc destroy_sub_msg = sub_message->GetDestroyFunction();
	if (destroy_sub_msg == NULL)
		return OpStatus::ERR;
	sub_instance_ptr = make_sub_msg();
	if (sub_instance_ptr == NULL)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = sub_msg_vector_ptr->Add(sub_instance_ptr);
	if (OpStatus::IsError(status))
	{
		destroy_sub_msg(sub_instance_ptr);
		sub_instance_ptr = NULL;
		return status;
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufInput::CreateMessage(void *&sub_instance_ptr, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	const OpProtobufMessage *sub_message = field.GetMessage();
	if (sub_message == NULL)
		return OpStatus::ERR;
	void **sub_msg_ptr_ptr = instance.FieldPtrVoidPtr(field_idx);
	if (sub_msg_ptr_ptr == NULL)
		return OpStatus::ERR;
	OpProtobufMessage::MakeFunc make_sub_msg = sub_message->GetMakeFunction();
	if (make_sub_msg == NULL)
		return OpStatus::ERR;
	OpProtobufMessage::DestroyFunc destroy_sub_msg = sub_message->GetDestroyFunction();
	if (destroy_sub_msg == NULL)
		return OpStatus::ERR;
	sub_instance_ptr = make_sub_msg();
	if (sub_instance_ptr == NULL)
		return OpStatus::ERR_NO_MEMORY;
	destroy_sub_msg(*sub_msg_ptr_ptr);
	*sub_msg_ptr_ptr = sub_instance_ptr;
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufInput::IsValid(OpProtobufInstanceProxy &instance, const OpProtobufMessage *message)
{
	OP_ASSERT(message != NULL);
	if (message == NULL)
		return OpStatus::ERR_NULL_POINTER;
	if (message->GetOffsetToBitfield() < 0)
		return OpStatus::OK; // Cannot determine so we assume everything is ok

	const OpProtobufField *fields = message->GetFields();
	OpProtobufBitFieldRef has_bits = instance.GetBitfield();
	for (int idx = 0; idx < message->GetFieldCount(); ++idx)
	{
		const OpProtobufField &field = fields[idx];
		if (field.GetQuantifier() == OpProtobufField::Required)
		{
			if (has_bits.IsUnset(idx))
				return OpStatus::ERR_PARSING_FAILED;
		}
	}
	return OpStatus::OK;
}

#endif // PROTOBUF_SUPPORT
