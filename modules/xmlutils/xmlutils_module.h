/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef XMLUTILS_MODULE_H
#define XMLUTILS_MODULE_H

#if defined XML_CONFIGURABLE_DOCTYPES

#include "modules/hardcore/opera/module.h"

class XMLConfiguredDoctypes;

class XmlutilsModule
	: public OperaModule
{
public:
	XmlutilsModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

#ifdef XML_CONFIGURABLE_DOCTYPES
	XMLConfiguredDoctypes *configureddoctypes;
#endif // XML_CONFIGURABLE_DOCTYPES
};

#define XMLUTILS_MODULE_REQUIRED

#endif // XML_CONFIGURABLE_DOCTYPES
#endif // XMLUTILS_MODULE_H
