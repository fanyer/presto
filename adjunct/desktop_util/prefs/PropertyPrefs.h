/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef PROPERTY_PREFS_H
#define PROPERTY_PREFS_H

#include "adjunct/desktop_util/adt/opfilteredproperty.h"
#include "adjunct/desktop_util/adt/opproperty.h"

/**
 * @file
 * Utilities for using prefs / prefsfile with OpProperty.
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */

class PrefsFile;

namespace PrefsUtils
{
	/** Read a string value from a PrefsFile
	  * @param prefs_file PrefsFile to read from
	  * @param section Section that value is in
	  * @param key Key for value
	  * @param def_value Default value
	  * @param result Value that was read
	  */
	OP_STATUS ReadPrefsString(PrefsFile* prefs_file,
							  const OpStringC8& section,
							  const OpStringC8& key,
							  const OpStringC& def_value,
							  OpProperty<OpString>& result);

	OP_STATUS ReadPrefsString(PrefsFile* prefs_file,
							  const OpStringC8& section,
							  const OpStringC8& key,
							  OpProperty<OpString>& result);

	/** Read an integer value from a PrefsFile
	  * @param prefs_file PrefsFile to read from
	  * @param section Section that value is in
	  * @param key Key for value
	  * @param def_value Default value
	  * @param result Value that was read (property type must be an integer type)
	  */
	template <typename T>
	inline OP_STATUS ReadPrefsInt(PrefsFile* prefs_file,
						   const OpStringC8& section,
						   const OpStringC8& key,
						   int def_value,
						   OpProperty<T>& result)
	{
		RETURN_IF_ERROR(ReadPrefsIntInt(prefs_file, section, key, def_value, def_value));
		result.Set(static_cast<T>(def_value));
		return OpStatus::OK;
	}

	inline OP_STATUS ReadPrefsBool(PrefsFile* prefs_file,
			const OpStringC8& section, const OpStringC8& key, bool def_value,
			OpProperty<bool>& result)
	{
		bool result_b;
		RETURN_IF_ERROR(ReadPrefsBool(prefs_file, section, key, def_value, result_b));
		result.Set(result_b);
		return OpStatus::OK;
	}

	template <typename T>
	inline OP_STATUS ReadPrefsInt(PrefsFile* prefs_file,
			const OpStringC8& section, const OpStringC8& key, int def_value,
			OpFilteredProperty<T>& result)
	{
		return ReadPrefsInt(prefs_file, section, key, def_value,
				static_cast<OpProperty<T>&>(result));
	}
};

#endif // PROPERTY_PREFS_H
