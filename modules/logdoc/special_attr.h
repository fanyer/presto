/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef COMPLEX_ATTR_H
#define COMPLEX_ATTR_H

class ComplexAttr
{
public:

	enum
	{
		T_UNKNOWN
	};

public:
	ComplexAttr() {}
	virtual ~ComplexAttr() {}

	virtual BOOL IsA(int type) { return type == T_UNKNOWN; }

	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to) { *copy_to = OP_NEW(ComplexAttr, ()); return *copy_to ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }
};

#endif // COMPLEX_ATTR_H
