/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef XPATH_MODULE_H
#define XPATH_MODULE_H

/* No global variables or initialization required. */
#ifdef XPATH_SUPPORT

#include "modules/hardcore/opera/module.h"

class OpHashFunctions;

class XpathModule
	: public OperaModule
{
public:
	XpathModule()
		: xmlexpandedname_hash_functions(NULL)
	{
	}

	virtual void InitL(const OperaInitInfo &info);
	virtual void Destroy();

	OpHashFunctions *xmlexpandedname_hash_functions;
};

#define XPATH_MODULE_REQUIRED

#endif // XPATH_SUPPORT
#endif // XPATH_MODULE_H
