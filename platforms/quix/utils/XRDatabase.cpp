/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/utils/XRDatabase.h"
#include "platforms/utilix/x11_all.h"

XRDatabase::XRDatabase()
	: m_database(0)
{
}

XRDatabase::~XRDatabase()
{
	XrmDestroyDatabase(m_database);
}

OP_STATUS XRDatabase::Init(const char* databasestring)
{
	if (!databasestring)
		return OpStatus::ERR;

	XrmInitialize();
	m_database = XrmGetStringDatabase(databasestring);
	if (!m_database)
		return OpStatus::ERR;

	return OpStatus::OK;
}

int XRDatabase::GetIntegerValue(const char* key, int default_value)
{
	const char* value = GetValue(key);
	return value ? op_atoi(value) : default_value;
}

bool XRDatabase::GetBooleanValue(const char* key, bool default_value)
{
	const char* value = GetValue(key);
	if (!value)
		return default_value;

	switch (value[0])
	{
		case 't': /* fallthrough */
		case 'T': /* fallthrough */
		case 'y': /* fallthrough */
		case 'Y': /* fallthrough */
		case '1':
			return true;

		case 'f': /* fallthrough */
		case 'F': /* fallthrough */
		case 'n': /* fallthrough */
		case 'N': /* fallthrough */
		case '0':
			return false;

		case 'o':
		case 'O':
			switch (value[1])
			{
				case 'n': /* fallthrough */
				case 'N':
					return true;
				case 'f': /* fallthrough */
				case 'F':
					return false;
			}
	}

	return default_value;
}

const char* XRDatabase::GetStringValue(const char* key, const char* default_value)
{
	const char* value = GetValue(key);
	return value ? value : default_value;
}

const char* XRDatabase::GetValue(const char* key)
{
	char* type = 0;
	XrmValue value;

	if (XrmGetResource(m_database, key, key, &type, &value) &&
		type && !op_strcmp(type, "String"))
	{
		return value.addr;
	}

	return 0;
}
