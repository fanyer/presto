/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef XSLT_MODULE_H
#define XSLT_MODULE_H

/* No global variables or initialization required. */
#if 0

#ifdef XSLT_SUPPORT

#include "modules/hardcore/opera/module.h"

class XsltModule
	: public OperaModule
{
public:
	XsltModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();
};

#define XSLT_MODULE_REQUIRED

#endif // XSLT_SUPPORT
#endif // 0
#endif // XSLT_MODULE_H
