/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_PROTOBUF_OUTPUT_H
#define OP_PROTOBUF_OUTPUT_H

#ifdef PROTOBUF_SUPPORT

#include "modules/util/adt/opvector.h"
#include "modules/protobuf/src/protobuf_wireformat.h"

#ifdef PROTOBUF_JSON_SUPPORT
#include "modules/protobuf/src/json_tools.h"
#endif // PROTOBUF_JSON_SUPPORT

#if defined(PROTOBUF_JSON_SUPPORT) || defined(PROTOBUF_XML_SUPPORT)
class ByteBuffer;
class TempBuffer;
class OpScopeFormatter;
#endif // defined(PROTOBUF_JSON_SUPPORT) || defined(PROTOBUF_XML_SUPPORT)

class OpProtobufInstanceProxy;
class OpProtobufField;
class OpProtobufDataOutputRange;

class OpProtobufBasicOutputStream
{
public:
	OpProtobufBasicOutputStream(OpProtobufDataOutputRange *out_range);

	const OpProtobufDataOutputRange *OutputRange() const { return out_range; }

	OP_STATUS WriteFieldHeader(OpProtobufWireFormat::WireType wire_type, int number);
	OP_STATUS WriteVarInt32(INT32 num);
	OP_STATUS WriteVarInt64(INT64 num);
	OP_STATUS WriteUint32(UINT32 num);
	OP_STATUS WriteUint64(UINT64 num);
	OP_STATUS WriteFloat(float num);
	OP_STATUS WriteDouble(double num);
	OP_STATUS WriteBool(BOOL v);
	OP_STATUS WriteString(OpProtobufStringChunkRange range);
	OP_STATUS WriteBytes(const OpProtobufDataChunkRange &range);

private:
	OpProtobufDataOutputRange *out_range;
};

class OpProtobufOutputStream : public OpProtobufBasicOutputStream
{
public:
	OpProtobufOutputStream(OpProtobufDataOutputRange *out_range);

	OP_STATUS Write(const OpProtobufInstanceProxy &instance);

private:
	OP_STATUS WriteMessage(const OpProtobufInstanceProxy &instance);
	OP_STATUS WriteField(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);
	int CalculateMessageSize(const OpProtobufInstanceProxy &instance) const;
	int CalculateFieldSize(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field) const;

	int         level;
};

#ifdef PROTOBUF_JSON_SUPPORT

class OpJSONOutputStream
{
public:
	OpJSONOutputStream(ByteBuffer &out);

	OP_STATUS Write(const OpProtobufInstanceProxy &instance);

private:
	OP_STATUS WriteMessage(const OpProtobufInstanceProxy &instance, BOOL first = TRUE);
	OP_STATUS WriteField(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, BOOL embed = FALSE, BOOL first = TRUE);

	ByteBuffer &out;
};

#endif // PROTOBUF_JSON_SUPPORT

#ifdef PROTOBUF_XML_SUPPORT

class OpXMLOutputStream
{
public:
	OpXMLOutputStream(ByteBuffer &out);

	OP_STATUS Write(const OpProtobufInstanceProxy &instance);

private:
	OP_STATUS WriteMessage(const OpProtobufInstanceProxy &instance);
	OP_STATUS WriteField(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);

	OP_STATUS WriteDouble(double value);
	OP_STATUS WriteSignedLong(long value);
	OP_STATUS WriteUnsignedLong(long value);
	OP_STATUS WriteString(OpProtobufStringChunkRange range);
	OP_STATUS WriteCString(const char *str, int len = -1);
	OP_STATUS WriteBytes(OpProtobufDataChunkRange range);

	ByteBuffer &out;
};

#endif // PROTOBUF_XML_SUPPORT

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_OUTPUT_H
