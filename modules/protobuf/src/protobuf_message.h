/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_PROTOBUF_MESSAGE_H
#define OP_PROTOBUF_MESSAGE_H

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf.h"

class OpProtobufDataRange;
class OpJSONInputStream;
class OpProtobufBitFieldRef;
template<typename T> class OpValueVector;
template<typename T> class OpProtobufValueVector;

class OpProtobufStringArrayRange;
class OpProtobufDataArrayRange;

class TempBuffer;
class UniString;
class OpData;

class OpProtobufWire
{
public:
	static OP_STATUS DecodeVarInt32(OpProtobufDataRange &range, int &len, INT32 &num);
	static OP_STATUS DecodeVarInt64(OpProtobufDataRange &range, int &len, INT64 &num);
	static OP_STATUS DecodeFixed32(OpProtobufDataRange &range, UINT32 &num);
	static OP_STATUS DecodeFixed64(OpProtobufDataRange &range, UINT64 &num);
};

class ByteBuffer;
class OpProtobufMessage;

// Helper methods for generated code

OP_STATUS OpProtobufAppend(ByteBuffer &to, const ByteBuffer &from, unsigned int max_len = ~0u);


class OpProtobufField
{
public:
	enum Quantifier
	{
		  Optional
		, Required
		, Repeated
	};

	/**
	 * Type of storage for fields with type 'bytes'
	 */
	enum BytesDataType
	{
		Bytes_ByteBuffer, ///< Use ByteBuffer for storage, this is the default
		Bytes_OpData ///< Use OpData for storage
	};

	/**
	 * Type of storage for fields with type 'string'
	 */
	enum StringDataType
	{
		String_OpString, ///< Use OpString for storage, this is the default
		String_TempBuffer, ///< Use TempBuffer for storage
		String_UniString ///< Use UniString for storage
	};

	OpProtobufField()
		: type(OpProtobufFormat::Int32)
		, number(0)
		, quantifier(Required)
		, name(NULL)
		, message(NULL)
		, message_id(0)
		, enum_id(0)
	{
		bytes_datatype = Bytes_ByteBuffer;
	}
	OpProtobufField(OpProtobufFormat::FieldType type, int number, const uni_char *name, Quantifier q = Required, const OpProtobufMessage *message = NULL, unsigned message_id = 0, unsigned enum_id = 0)
		: type(type)
		, number(number)
		, quantifier(q)
		, name(name)
		, message(message)
		, message_id(message_id)
		, enum_id(enum_id)
	{
		OP_ASSERT(type != OpProtobufFormat::Int32 && enum_id == 0 || type == OpProtobufFormat::Int32);
		if (type == OpProtobufFormat::Bytes)
			bytes_datatype = Bytes_ByteBuffer;
		else if (type == OpProtobufFormat::String)
			string_datatype = String_OpString;
	}
	~OpProtobufField() {}

	OpProtobufFormat::FieldType GetType() const { return type; }
	const OpProtobufMessage    *GetMessage() const { return message; }
	unsigned                    GetMessageId() const { return message_id; }
	int                         GetNumber() const { return number; }
	const uni_char             *GetName() const { return name; }
	Quantifier                  GetQuantifier() const { return quantifier; }
	unsigned                    GetEnumId() const { return enum_id; }
	BytesDataType GetBytesDataType() const
	{
		OP_ASSERT(type == OpProtobufFormat::Bytes);
		return bytes_datatype;
	}
	StringDataType GetStringDataType() const
	{
		OP_ASSERT(type == OpProtobufFormat::String);
		return string_datatype;
	}

	void Update(OpProtobufFormat::FieldType type_, int number_, const uni_char *name_, Quantifier q = Required, const OpProtobufMessage *message_ = NULL, int message_id_ = 0, unsigned enum_id_ = 0)
	{
		OP_ASSERT(type_ != OpProtobufFormat::Int32 && enum_id_ == 0 || type_ == OpProtobufFormat::Int32);
		type = type_;
		number = number_;
		quantifier = q;
		name = name_;
		message = message_;
		message_id = message_id_;
		enum_id = enum_id_;
	}
	void SetType(OpProtobufFormat::FieldType t) { OP_ASSERT(t != OpProtobufFormat::Int32 && enum_id == 0 || t == OpProtobufFormat::Int32); type = t; }
	void SetMessage(const OpProtobufMessage *m) { message = m; }
	void SetMessage(unsigned message_id_) { message_id = message_id_; }
	void SetNumber(int n) { number = n; }
	void SetName(const uni_char *n) { name = n; }
	void SetQuantifier(Quantifier q) { quantifier = q; }
	void SetEnumId(unsigned id) { OP_ASSERT(type != OpProtobufFormat::Int32 && id == 0 || type == OpProtobufFormat::Int32); enum_id = id; }
	void SetBytesDataType(BytesDataType datatype)
	{
		OP_ASSERT(type == OpProtobufFormat::Bytes);
		bytes_datatype = datatype;
	}
	void SetStringDataType(StringDataType datatype)
	{
		OP_ASSERT(type == OpProtobufFormat::String);
		string_datatype = datatype;
	}

private:
	OpProtobufFormat::FieldType type;
	int					        number;
	Quantifier                  quantifier;
	const uni_char             *name;
	const OpProtobufMessage    *message; // This points to the sub-message, only used when field type is Message
	unsigned                    message_id; // The ID of the sub-message, 0 if no sub-message
	unsigned                    enum_id; // The ID of the enum, 0 if no enum
	union
	{
		BytesDataType bytes_datatype;
		StringDataType string_datatype;
	};
};

enum { OpProtobufOffsetEmpty = -1 };

class OpProtobufMessage
{
public:
	enum Flag
	{
		Flag_None = 0,
	};
	typedef void *(*MakeFunc)();
	typedef void (*DestroyFunc)(void *);

	OpProtobufMessage()
	: bitfield_offset(-1)
	, size_offset(-1)
	, internal_id(-1)
	, parent_id(-1)
	, fields(NULL)
	, offsets(NULL)
	, field_count(0)
	, name(NULL)
	, make(NULL)
	, destroy(NULL)
	, is_initialized(FALSE)
	, flags(Flag_None)
	{}
	OpProtobufMessage(unsigned internal_id, unsigned parent_id,
					  int field_count, OpProtobufField *fields,
					  int *offsets, int bitfield_offset,
					  int size_offset,
					  const char *name,
					  MakeFunc make = NULL, DestroyFunc destroy = NULL,
					  unsigned flags = Flag_None)
	: bitfield_offset(bitfield_offset)
	, size_offset(size_offset)
	, internal_id(internal_id)
	, parent_id(parent_id)
	, fields(fields)
	, offsets(offsets)
	, field_count(field_count)
	, name(name)
	, make(make)
	, destroy(destroy)
	, is_initialized(FALSE)
	, flags(flags)
	{}
	~OpProtobufMessage();

	int                    GetInternalId() const { return internal_id; }
	int                    GetParentId() const { return parent_id; }
	int                    GetFieldCount() const { return field_count; }
	const OpProtobufField *GetFields() const { return fields; }
	inline const OpProtobufField &GetField(int idx) const;
	const int             *GetFieldOffsets() const { return offsets; }
	inline int             GetFieldOffset(int idx) const;
	inline int             GetOffsetToBitfield() const { return bitfield_offset; }
	inline int             GetOffsetToEncodedSize() const { return size_offset; }
	const char            *GetName() const { return name; }
	BOOL                   IsInitialized() const { return is_initialized; }
	unsigned               GetFlags() const { return flags; }

	MakeFunc GetMakeFunction() const { return make; }
	DestroyFunc GetDestroyFunction() const { return destroy; }

	void                   SetInternalId(int id) { internal_id = id; }
	void                   SetParentId(int id) { parent_id = id; }
	void SetIsInitialized(BOOL init) { is_initialized = init; }
	void                   SetFlags(unsigned f) { flags = f; }

private:
	int                    bitfield_offset;
	int                    size_offset;
	int                    internal_id;
	int                    parent_id;
	OpProtobufField	       *fields;
	int                    *offsets;
	int                    field_count;
	const char            *name;
	MakeFunc               make;
	DestroyFunc            destroy;
	volatile BOOL          is_initialized; // volatile to avoiding caching issues
	unsigned               flags;
};

/*inline*/
const OpProtobufField &
OpProtobufMessage::GetField(int idx) const
{
	OP_ASSERT(idx >= 0 && idx < field_count);
	OP_ASSERT(fields != NULL);
	return fields[idx];
}

/*inline*/
int
OpProtobufMessage::GetFieldOffset(int idx) const
{
	OP_ASSERT(idx >= 0 && idx < field_count);
	OP_ASSERT(offsets != NULL);
	return offsets[idx];
}

class OpProtobufRepeatedItems : private OpGenericVector
{
public:
	OpProtobufRepeatedItems(UINT32 stepsize = 10) : OpGenericVector(stepsize) {}

	// Expose some of the protected members from OpGenericVector which are interesting to repeated items.
	OP_STATUS Add(void* item) { return OpGenericVector::Add(item); }
	void      AddL(void* item) { LEAVE_IF_ERROR(OpGenericVector::Add(item)); }
	void*     Get(UINT32 idx) const { return OpGenericVector::Get(idx); }
	UINT32    GetCount() const { return OpGenericVector::GetCount(); }

private:
	OpProtobufRepeatedItems(const OpProtobufRepeatedItems &);
	OpProtobufRepeatedItems &operator =(const OpProtobufRepeatedItems &);
};

template<typename T>
class OpProtobufMessageVector
{
public:
	OpProtobufMessageVector() {}
	~OpProtobufMessageVector();

	OP_STATUS Add(T* item) { return items.Add(reinterpret_cast<void *>(item)); }
	void      AddL(T* item) { items.AddL(reinterpret_cast<void *>(item)); }
	T*        AddNew() { T* item = OP_NEW(T, ()); if (item) items.Add(reinterpret_cast<void *>(item)); return item; }
	T*        AddNewL() { T* item = OP_NEW_L(T, ()); items.Add(reinterpret_cast<void *>(item)); return item; }
	T*        Get(UINT32 idx) const { return reinterpret_cast<T *>(items.Get(idx)); }
	UINT32    GetCount() const { return items.GetCount(); }

	static void *Make() { return OP_NEW(T, ()); }
	static void Destroy(void *m) { OP_DELETE(reinterpret_cast<T *>(m)); }

private:
	OpProtobufRepeatedItems items;
	OpProtobufMessageVector(const OpProtobufMessageVector &);
	OpProtobufMessageVector &operator =(const OpProtobufMessageVector &);
};

template<typename T>
OpProtobufMessageVector<T>::~OpProtobufMessageVector()
{
	for (unsigned int i = 0; i < items.GetCount(); ++i)
	{
		T *msg = Get(i);
		OP_DELETE(msg);
	}
	// NOTE: The destructor of OpProtobufRepeatedItems (ie. OpGenericVector) will only deallocate the pointer
	//       array, not the members, which is correct in this case
}

template<typename T>
T *op_field_ptr(void *obj, int offset)
{
	return reinterpret_cast<T *>(reinterpret_cast<char *>(obj) + offset);
}

#define FIELD_PTR(Name, Type) Type *FieldPtr##Name(int field_idx) const \
	{ \
		OP_ASSERT(message != NULL); \
		if (message == NULL || field_idx < 0 || field_idx >= message->GetFieldCount()) \
			return NULL; \
		OP_ASSERT(instance_ptr != NULL); \
		return op_field_ptr< Type >(instance_ptr, message->GetFieldOffset(field_idx)); \
	}


class OpProtobufInstanceProxy
{
public:
	OpProtobufInstanceProxy(const OpProtobufMessage *message, void *instance_ptr)
		: message(message)
		, instance_ptr(instance_ptr)
	{
		OP_ASSERT(message != NULL);
	}
	~OpProtobufInstanceProxy() {}

	const OpProtobufMessage *GetProtoMessage() const { OP_ASSERT(message != NULL); return message; }
	void *GetInstancePtr() const { return instance_ptr; }
	OpProtobufBitFieldRef GetBitfield() const;
	INT32 *GetCurrentIDPtr() const;
	int GetInstanceEncodedSize() const;
	void SetInstanceEncodedSize(int size) const;

/*  // TODO: This method does not seem to be used, if it is used it must be converted to a macro/non-template method.
	template<typename T>
	OP_STATUS ExtractFieldPtr(int field_idx, T *&type_ptr) const
	{
		if (field_idx < 0 || field_idx >= message.GetFieldCount())
			return OpStatus::ERR;
		OP_ASSERT(instance_ptr != NULL);
		type_ptr = op_field_ptr<T>(instance_ptr, message.GetFieldOffset(field_idx));
		return OpStatus::OK;
	}*/

	FIELD_PTR(OpValueVectorDouble,     OpValueVector<double>)
	FIELD_PTR(OpValueVectorFloat,      OpValueVector<float>)
	FIELD_PTR(OpValueVectorINT32,      OpValueVector<INT32>)
	FIELD_PTR(OpValueVectorUINT32,     OpValueVector<UINT32>)
	FIELD_PTR(OpValueVectorBOOL,       OpValueVector<BOOL>)
	FIELD_PTR(Double,                  double)
	FIELD_PTR(Float,                   float)
	FIELD_PTR(BOOL,                    BOOL)
	FIELD_PTR(OpINT32Vector,           OpINT32Vector)
	FIELD_PTR(OpProtobufRepeatedItems, OpProtobufRepeatedItems)
	FIELD_PTR(Void,                    void)
	FIELD_PTR(VoidPtr,                 void*)
	FIELD_PTR(INT32,                   INT32)
	FIELD_PTR(UINT32,                  UINT32)

#ifdef OP_PROTOBUF_64BIT_SUPPORT
	FIELD_PTR(OpValueVectorINT64,      OpValueVector<INT64>)
	FIELD_PTR(OpValueVectorUINT64,     OpValueVector<UINT64>)
	FIELD_PTR(INT64,                   INT64)
	FIELD_PTR(UINT64,                  UINT64)
#endif // OP_PROTOBUF_64BIT_SUPPORT

	const OpProtobufField &GetField(int field_idx) const
	{
		OP_ASSERT(!(message == NULL || field_idx < 0 || field_idx >= message->GetFieldCount()));
		return message->GetField(field_idx);
	}

	// String

	BOOL IsFieldOpString(int field_idx) const;

	OpProtobufStringChunkRange GetStringRange(int field_idx) const;

	int GetStringArrayCount(int field_idx) const;

	OpProtobufStringArrayRange GetStringArray(int field_idx) const;

	OP_STATUS SetString(int field_idx, const OpProtobufStringChunkRange &range);

	OP_STATUS SetString(int field_idx, const OpProtobufDataChunkRange &range);

	OP_STATUS AddString(int field_idx, const OpProtobufStringChunkRange &range);

	OP_STATUS AddString(int field_idx, const OpProtobufDataChunkRange &range);

	OP_STATUS AddString(int field_idx, OpString *str, BOOL takeover=FALSE);

	// Bytes

	BOOL IsFieldByteBuffer(int field_idx) const;

	OpProtobufDataChunkRange GetBytesRange(int field_idx) const;

	int GetBytesArrayCount(int field_idx) const;

	OpProtobufDataArrayRange GetBytesArray(int field_idx) const;

	OP_STATUS SetBytes(int field_idx, const OpProtobufDataChunkRange &range);

	OP_STATUS AddBytes(int field_idx, const OpProtobufDataChunkRange &range);

	// Message

	const OpProtobufMessage *GetFieldProtoMessage(int field_idx) const;

private:
	// String

	OP_STATUS Put(OpString &str, OpProtobufStringChunkRange range);
	OP_STATUS Put(OpString &str, const OpProtobufDataChunkRange &range);
	OP_STATUS Put(UniString &str, OpProtobufStringChunkRange range);
	OP_STATUS Put(UniString &str, const OpProtobufDataChunkRange &range);
	OP_STATUS Put(TempBuffer &str, OpProtobufStringChunkRange range);
	OP_STATUS Put(TempBuffer &str, const OpProtobufDataChunkRange &range);

	// Bytes

	OP_STATUS Put(ByteBuffer &buffer, const OpProtobufDataChunkRange &range);
	OP_STATUS Put(OpData &buffer, const OpProtobufDataChunkRange &range);

	// Accessors for ByteBuffer
	FIELD_PTR(ByteBuffer,              ByteBuffer)
	FIELD_PTR(OpAutoVectorByteBuffer,  OpAutoVector<ByteBuffer>)
	// Accessors for OpData
	FIELD_PTR(OpData,                  OpData)
	FIELD_PTR(OpValueVectorOpData,     OpProtobufValueVector<OpData>)
	// Accessors for OpString
	FIELD_PTR(OpString,                OpString)
	FIELD_PTR(OpAutoVectorOpString,    OpAutoVector<OpString>)
	// Accessors for UniString
	FIELD_PTR(UniString,               UniString)
	FIELD_PTR(OpValueVectorUniString,  OpProtobufValueVector<UniString>)
	// Accessors for TempBuffer
	FIELD_PTR(TempBuffer,              TempBuffer)
	FIELD_PTR(OpAutoVectorTempBuffer,  OpAutoVector<TempBuffer>)

	const OpProtobufMessage *message;
	void                    *instance_ptr;

	friend class OpJSONInputStream; // OpJSONInputStream::ReadField requires access to FieldOpString for an optimization
#ifdef SELFTEST
	friend class ST_protob_1ne7ala_er_message; // For test "protobuf.protocolbuffer_message"
#endif
};

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_MESSAGE_H
