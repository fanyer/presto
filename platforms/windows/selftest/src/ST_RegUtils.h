/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
#ifndef ST_REG_UTILS_H
#define ST_REG_UTILS_H

#ifdef SELFTEST

#include "platforms/windows/windows_ui/Registry.h"

static void ST_DeleteRegKey(const OpStringC& key_name, BOOL should_exist)
{
	const BOOL key_deleted =
			ERROR_SUCCESS == RegDeleteKeyNT(HKEY_CURRENT_USER, key_name);
	OP_ASSERT(!should_exist || key_deleted
			|| !"Failed to delete temporary test registry key");
}

static BOOL ST_RegKeyExists(const OpStringC& key_name)
{
	HKEY dummy;
	const BOOL exists =
			ERROR_SUCCESS == RegOpenKey(HKEY_CURRENT_USER, key_name, &dummy);
	RegCloseKey(dummy);
	return exists;
}

#endif // SELFTEST
#endif // ST_REG_UTILS_H
