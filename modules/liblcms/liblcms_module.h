/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LIBLCMS_LIBLCMS_MODULE_H
#define MODULES_LIBLCMS_LIBLCMS_MODULE_H

#ifdef LCMS_3P_SUPPORT

#include "modules/hardcore/opera/module.h"


class LiblcmsModule: public OperaModule
{
public:
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy() {};
};


#define g_liblcms_module g_opera->liblcms_module

#define LIBLCMS_MODULE_REQUIRED

#endif // LCMS_3P_SUPPORT

#endif // MODULES_LIBLCMS_LIBLCMS_MODULE_H
