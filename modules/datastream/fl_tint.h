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


#ifndef __FL_TINT_H__
#define __FL_TINT_H__

#ifdef DATASTREAM_UINT_ENABLED
#ifdef DATASTREAM_USE_INTEGRALTYPE_TEMPLATE

#include "modules/datastream/fl_int.h"


/** Typed Templated integer record Datastream object 
 *	
 *	Can be used to create records for enums and smaller integers
 */
template<class T, uint32 type_size> class DataStream_IntegralType : public DataStream_UIntBase
{
public:
	/** Constructor:
	 *	Set the value to the specified value (default numerical 0)
	 *
	 *	@param	val Initial value of the integer
	 */
	DataStream_IntegralType(T val= (T)0) : DataStream_UIntBase(type_size, val){};

	/** Conversion to the actual type */
	operator T() const {return (T) value;};

	/** Assignment operator, from other template instance */
	DataStream_IntegralType &operator =(const DataStream_IntegralType &val){value = val.value; return *this;}
	/** Assignment operator, from actual type */
	DataStream_IntegralType &operator =(const T &val){value = (uint32) val; return *this;}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_IntegralType";}
#endif
};
#endif // DATASTREAM_USE_INTEGRALTYPE_TEMPLATE

/** Variable length integer record Datastream object 
 *	
 *	Public API to DataStream_UIntBase
 */
typedef DataStream_UIntBase DataStream_UIntVarLength;
#if 0
class DataStream_UIntVarLength : public DataStream_UIntBase
{
public:
	/** Contructor 
	 *
	 *	Constructs an unsigned integer with the given size and value
	 *
	 *	@param	sz	Size (in bytes) of the integer
	 *	@param	val Initial value of the integer
	 */
	DataStream_UIntVarLength(uint32 sz, uint32 val=0): DataStream_UIntBase(sz, val){};

	/** Conversion to uint32 */
	operator uint32() const {return value;};

	/** Assignment operator, from uint32 */
	DataStream_UIntVarLength &operator =(const uint32 val){value = val; return *this;}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_UIntVarLength";}
#endif
};
#endif

#endif // DATASTREAM_UINT_ENABLED

#endif // __FL_TINT_H__
