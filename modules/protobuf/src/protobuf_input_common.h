/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_PROTOBUF_INPUT_COMMON_H
#define OP_PROTOBUF_INPUT_COMMON_H

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf_message.h"
#include "modules/protobuf/src/opvaluevector.h"

/* OpProtobufInput */

#define ADD_SCALAR(Name, T) \
	static \
	OP_STATUS \
	AddScalar##Name(T value, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field) \
	{ \
		if (field.GetQuantifier() == OpProtobufField::Repeated) \
			return instance.FieldPtrOpValueVector##Name(field_idx)->Add(value); \
		else \
			*instance.FieldPtr##Name(field_idx) = value; \
		return OpStatus::OK; \
	}

class OpProtobufInput
{
public:

	ADD_SCALAR(Double,                  double)
	ADD_SCALAR(Float,                   float)
	ADD_SCALAR(BOOL,                    BOOL)
	ADD_SCALAR(INT32,                   INT32)
	ADD_SCALAR(UINT32,                  UINT32)
#ifdef OP_PROTOBUF_64BIT_SUPPORT
	ADD_SCALAR(INT64,                   INT64)
	ADD_SCALAR(UINT64,                  UINT64)
#endif // OP_PROTOBUF_64BIT_SUPPORT

	//template<typename T>
	//static OP_STATUS AddScalar(T d, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);
	static OP_STATUS AddBool(BOOL d, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);
	static OP_STATUS AddString(const uni_char *text, int len, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);
	static OP_STATUS AddString(const OpProtobufDataChunkRange &range, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);
	static OP_STATUS AddBytes(const OpProtobufDataChunkRange &range, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);
	static OP_STATUS AddMessage(void *&sub_instance_ptr, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);
	static OP_STATUS CreateMessage(void *&sub_instance_ptr, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);

	// checks all required fields
	static OP_STATUS IsValid(OpProtobufInstanceProxy &instance, const OpProtobufMessage *message);
};

class OpProtobufError
{
public:
	OpProtobufError()
		: line(-1), column(-1), offset(-1)
	{}

	OP_STATUS Construct(const uni_char *description_ = NULL, int line_ = -1, int column_ = -1, int offset_ = -1)
	{
		RETURN_IF_ERROR(description.Set(description));
		line = line_;
		column = column_;
		offset = offset_;
		return OpStatus::OK;
	}

	/**
	 * Returns TRUE if the error information has been set on the object.
	 */
	BOOL IsSet() const { return description.Length() > 0 || line >= 0 || column >= 0 || offset >= 0; }

	const OpString &GetDescription() const { return description; }
	int             GetLine() const { return line; }
	int             GetColumn() const { return column; }
	int             GetOffset() const { return offset; }

	OP_STATUS SetDescription(const uni_char *description_) { return description.Set(description_); }
	OP_STATUS SetDescription(const char *description_) { return description.Set(description_); }
	void      SetLine(int line_) { line = line_; }
	void      SetColumn(int column_) { column = column_; }
	void      SetOffset(int offset_) { offset = offset_; }

private:
	OpString description;
	int      line;
	int      column;
	int      offset;
};

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_INPUT_COMMON_H
