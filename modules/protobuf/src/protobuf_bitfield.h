/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_PROTOBUF_BITFIELD_H
#define OP_PROTOBUF_BITFIELD_H

#ifdef PROTOBUF_SUPPORT

// Note: Bits work MSB to LSB, this means that bit #0 in a bitfield is not the same as bit #0 in a normal unsigned char.
class OpProtobufBitFieldRef
{
public:
	typedef unsigned char Type;
	enum {PartSize = (sizeof(Type)*8),
		MSB = PartSize - 1};

	OpProtobufBitFieldRef() : parts(NULL), size(0) {}
	OpProtobufBitFieldRef(Type *parts, unsigned int size) : parts(parts), size(size) {}

	BOOL IsSet(unsigned int offset) const
	{
		OP_ASSERT(parts != NULL);
		OP_ASSERT(offset < size);
		return GetMaskedPart(offset) != 0;
	}
	BOOL IsUnset(unsigned int offset) const
	{
		OP_ASSERT(parts != NULL);
		OP_ASSERT(offset < size);
		return GetMaskedPart(offset) == 0;
	}
	void SetBit(unsigned int offset)
	{
		OP_ASSERT(parts != NULL);
		OP_ASSERT(offset < size);
		parts[offset/PartSize] |= (1 << (MSB - (offset % PartSize)));
	}
	void ClearBit(unsigned int offset)
	{
		OP_ASSERT(parts != NULL);
		OP_ASSERT(offset < size);
		parts[offset/PartSize] &= ~(1 << (MSB - (offset % PartSize)));
	}
	void AssignBit(unsigned int offset, BOOL b)
	{
		if (b)
			SetBit(offset);
		else
			ClearBit(offset);
	}
private:
	Type GetMaskedPart(unsigned int offset) const
	{
		OP_ASSERT(parts != NULL);
		OP_ASSERT(offset < size);
		return (parts[offset/PartSize] & (1 << (MSB - (offset % PartSize))));
	}

	Type        *parts;
	unsigned int size;
};

template <int SIZE>
class OpProtobufBitField
{
public:
	typedef OpProtobufBitFieldRef::Type Type;
	enum {PartSize = OpProtobufBitFieldRef::PartSize}; // Number of bits per part
	enum {Count = (SIZE + PartSize - 1)/PartSize};
	enum {Size = SIZE};

	OpProtobufBitField(BOOL initial = FALSE)
	{
		Type def = initial ? ~0 : 0;
		for(int i = 0; i < Count; ++i)
			parts[i] = def;
	}
	OpProtobufBitField(const Type *initial)
	{
		for(int i = 0; i < Count; ++i)
			parts[i] = initial[i];
	}

	OpProtobufBitFieldRef Reference();
	const OpProtobufBitFieldRef Reference() const;
	Type *Field() { return &parts[0]; }
	Type *Field() const { return const_cast<Type *>(&parts[0]); }

private:
	Type parts[Count];
};

template<int SIZE>
OpProtobufBitFieldRef OpProtobufBitField<SIZE>::Reference()
{
	return OpProtobufBitFieldRef(Field(), Size);
}

template<int SIZE>
const OpProtobufBitFieldRef OpProtobufBitField<SIZE>::Reference() const
{
	return OpProtobufBitFieldRef(Field(), Size);
}

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_BITFIELD_H
