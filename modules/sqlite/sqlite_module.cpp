/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SQLITE_SUPPORT

#include "modules/sqlite/sqlite_module.h"
#include "modules/sqlite/sqlite3.h"

/*virtual*/
void SqliteModule::Destroy()
{
	sqlite3_shutdown();
}

#endif // SQLITE_SUPPORT
