/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/prefs/PrefsUtils.h"
#include "adjunct/desktop_util/prefs/PropertyPrefs.h"

namespace PrefsUtils
{

OP_STATUS ReadPrefsString(PrefsFile* prefs_file,
		const OpStringC8& section, const OpStringC8& key,
		const OpStringC& def_value, OpProperty<OpString>& result)
{
	OpString value;
	RETURN_IF_ERROR(ReadPrefsString(prefs_file, section, key, def_value, value));
	result.Set(value);
	return OpStatus::OK;
}

OP_STATUS ReadPrefsString(PrefsFile* prefs_file,
		const OpStringC8& section, const OpStringC8& key,
		OpProperty<OpString>& result)
{
	return ReadPrefsString(prefs_file, section, key, NULL, result);
}

}
