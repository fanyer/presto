/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf_output_common.h"

#include "modules/protobuf/src/protobuf_output_common.h"
#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/protobuf.h"
#include "modules/protobuf/src/protobuf_message.h"

#include "modules/protobuf/src/opvaluevector.h"

#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"

/*static*/
int
OpProtobufOutput::GetFieldCount(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	switch (field.GetType())
	{
		case OpProtobufFormat::Double:
			return instance.FieldPtrOpValueVectorDouble(field_idx)->GetCount();
		case OpProtobufFormat::Float:
			return instance.FieldPtrOpValueVectorFloat(field_idx)->GetCount();
		case OpProtobufFormat::Int32:
		case OpProtobufFormat::Sint32:
		case OpProtobufFormat::Sfixed32:
			return instance.FieldPtrOpValueVectorINT32(field_idx)->GetCount();
		case OpProtobufFormat::Uint32:
		case OpProtobufFormat::Fixed32:
			return instance.FieldPtrOpValueVectorUINT32(field_idx)->GetCount();
		case OpProtobufFormat::Bool:
			// TODO: Repeated BOOLs should really have a more optimized class, OpBitvector is useless since it does not know how many bits it has.
			return instance.FieldPtrOpINT32Vector(field_idx)->GetCount();
		case OpProtobufFormat::String:
			return instance.GetStringArrayCount(field_idx);
		case OpProtobufFormat::Bytes:
			return instance.GetBytesArrayCount(field_idx);
		case OpProtobufFormat::Message:
			return instance.FieldPtrOpProtobufRepeatedItems(field_idx)->GetCount();
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_PROTO_SUPPORT
		case OpProtobufFormat::Int64:
		case OpProtobufFormat::Sint64:
		case OpProtobufFormat::Sfixed64:
			return instance.FieldPtrOpValueVectorINT64(field_idx)->GetCount();
		case OpProtobufFormat::Uint64:
		case OpProtobufFormat::Fixed64:
			return instance.FieldPtrOpValueVectorUINT64(field_idx)->GetCount();
#else // OP_PROTOBUF_64BIT_PROTO_SUPPORT
		case OpProtobufFormat::Int64:
		case OpProtobufFormat::Sint64:
		case OpProtobufFormat::Sfixed64:
		case OpProtobufFormat::Uint64:
		case OpProtobufFormat::Fixed64:
			OP_ASSERT(!"64bit values are not supported");
			return 0;
#endif // OP_PROTOBUF_64BIT_PROTO_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
		default:
			OP_ASSERT(!"Should not happen");
			return 0;
	}
}


#endif // PROTOBUF_SUPPORT
