/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 */

#ifndef WEBSERVER_COMMON_H_
#define WEBSERVER_COMMON_H_
#include "modules/util/adt/opvector.h"



class OpStringVector : public OpVector<OpString>
{
public:
	OpStringVector(UINT32 stepsize = 10) : OpVector<OpString>(stepsize) {}
	OP_STATUS SetVectorName(const uni_char *name) { OP_ASSERT(m_vector_name.IsEmpty()); return  m_vector_name.Set(name); }
	const uni_char *GetVectorName(){ return m_vector_name.CStr(); }
private:
	OpString m_vector_name;
};

class OpAutoStringVector : public OpStringVector
{
public:
	OpAutoStringVector(UINT32 stepsize = 10) : OpStringVector(stepsize) {}

	virtual ~OpAutoStringVector()
	{
		// Explicit |this| so that the calls depend on the template type
		UINT32 count = this->GetCount();
		for (UINT32 i = 0; i < count; i++)
		{
			OpString *str = this->Get(i); 
			OP_DELETE(str);
		}
	}
};

#endif /*WEBSERVER_COMMON_H_*/
