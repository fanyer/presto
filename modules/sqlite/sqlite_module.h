/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SQLITE_MODULE_H
#define SQLITE_MODULE_H

#include "modules/hardcore/opera/module.h"

#ifdef SQLITE_SUPPORT

class SqliteModule : public OperaModule
{
public:
	SqliteModule() {}

	virtual void InitL(const OperaInitInfo& info) {}
	virtual void Destroy();
};

#define SQLITE_MODULE_REQUIRED

#endif //SQLITE_SUPPORT

#endif // SQLITE_MODULE_H
