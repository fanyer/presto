/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_REGEXP_REGEXP_MODULE_H
#define MODULES_REGEXP_REGEXP_MODULE_H

//No globals yet
//#define REGEXP_MODULE_REQUIRED

#ifdef REGEXP_MODULE_REQUIRED

#include "modules/hardcore/opera/module.h"

class RegExpModule : public OperaModule
{
public:
	RegExpModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();
};

#endif // REGEXP_MODULE_REQUIRED

#endif // !MODULES_REGEXP_REGEXP_MODULE_H
