/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_OLDDEBUG_OLDDEBUG_MODULE_H
#define MODULES_OLDDEBUG_OLDDEBUG_MODULE_H

#ifdef OLDDEBUG_ENABLED
#define OLDDEBUG_MODULE_REQUIRED

#include "modules/hardcore/opera/module.h"
#include "modules/util/simset.h"

class OlddebugModule : public OperaModule
{
public:
	OlddebugModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	AutoDeleteHead debug_files;
	char printbuffer[10000];    /* ARRAY OK 2004-07-30 yngve */
};

#define g_odbg_debug_files (g_opera->olddebug_module.debug_files)
#define g_odbg_printbuffer (g_opera->olddebug_module.printbuffer)

#endif // OLDDEBUG_ENABLED

#endif // !MODULES_OLDDEBUG_OLDDEBUG_MODULE_H
