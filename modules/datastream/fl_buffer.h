/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef __FL_BUFFER_H__
#define __FL_BUFFER_H__

/** Creates a binary array of the specified, hardcoded, length, and acesses it as a Byte Array*/
template <uint32 L> 
class DataStream_LimitedBuffer : public DataStream_ByteArray
{
private:
	byte buffer[L]; /* ARRAY OK 2009-05-25 yngve */
	
public:
	DataStream_LimitedBuffer():DataStream_ByteArray(buffer, L){op_memset(buffer, 0, L);}
	virtual ~DataStream_LimitedBuffer(){op_memset(buffer, 0, L);}

	/** Array dereference, Access byte val directly read/write */
	byte &operator[](uint32 val){return buffer[val < L ? val : 0];}
	/** Array dereference, Access byte val, readonly */
	byte operator[](uint32 val) const{return buffer[val < L ? val : 0];}

	/** Assignment operator */
	DataStream_LimitedBuffer &operator =(const DataStream_LimitedBuffer &src){op_memcpy(buffer, src.buffer, L); return *this;}

	/** Copy the src buffer */
	void Set(const byte *src){if(src) op_memcpy(buffer, src, L);}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_LimitedBuffer";}
#endif
};

#endif // __FL_BUFFER_H__
