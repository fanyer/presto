/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAUTILS_H
#define VEGAUTILS_H

# ifdef VEGA_SUPPORT

class VEGARefCount
{
public:
	VEGARefCount() : m_refcount(1) {}

	static void IncRef(VEGARefCount* obj)
	{
		if (obj)
			obj->m_refcount++;
	}

	static void DecRef(VEGARefCount* obj)
	{
		if (obj && --obj->m_refcount == 0)
			OP_DELETE(obj);
	}

protected:
	virtual ~VEGARefCount() { OP_ASSERT(m_refcount == 0); }

private:
	int m_refcount;
};

# endif // VEGA_SUPPORT

#endif // VEGAUTILS_H
