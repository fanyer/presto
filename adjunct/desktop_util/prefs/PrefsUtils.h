/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Axel Siebert (axel) / Arjan van Leeuwen (arjanl)
 */

#ifndef PREFSUTILS_H
#define PREFSUTILS_H

#include "modules/inputmanager/inputaction.h"

class PrefsFile;

/**
 * @brief Utilities for using prefs / prefsfile
 * @author Axel Siebert / Arjan van Leeuwen
 *
 */
namespace PrefsUtils
{
	/** Read a 32-bit integer value from a PrefsFile
	  * @param prefs_file PrefsFile to read from
	  * @param section Section that value is in
	  * @param key Key for value
	  * @param def_value Default value
	  * @param result Value that was read
	  */
	OP_STATUS ReadPrefsIntInt(PrefsFile* prefs_file,
							  const OpStringC8& section,
							  const OpStringC8& key,
							  int def_value,
							  int& result);

	/** Read an integer value from a PrefsFile
	  * @param prefs_file PrefsFile to read from
	  * @param section Section that value is in
	  * @param key Key for value
	  * @param def_value Default value
	  * @param result Value that was read (must be an integer type)
	  */
	template<class T>
	OP_STATUS ReadPrefsInt(PrefsFile* prefs_file,
						   const OpStringC8& section,
						   const OpStringC8& key,
						   int def_value,
						   T& result)
	{
		OP_STATUS status = ReadPrefsIntInt(prefs_file, section, key, def_value, def_value);
		result = static_cast<T>(def_value);
		return status;
	}


	/** Read an bool value from a PrefsFile
	  * @param prefs_file PrefsFile to read from
	  * @param section Section that value is in
	  * @param key Key for value
	  * @param def_value Default value
	  * @param result Value that was read (must be a bool)
	  */
	OP_STATUS ReadPrefsBool(PrefsFile* prefs_file,
						   const OpStringC8& section,
						   const OpStringC8& key,
						   bool def_value,
						   bool &result);

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
							  OpString& result);

	OP_STATUS ReadPrefsString(PrefsFile* prefs_file,
							  const OpStringC8& section,
							  const OpStringC8& key,
							  const OpStringC& def_value,
							  OpString& result);
	/** @overload */
	OP_STATUS ReadPrefsString(PrefsFile* prefs_file,
							  const OpStringC8& section,
							  const OpStringC8& key,
							  OpString8& result);

	OP_STATUS ReadFolderPath(PrefsFile* prefs_file, 
							 const OpStringC8& section, 
							 const OpStringC8& key, 
							 OpString& result, 
							 const OpStringC &default_result,
							 OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);

	/** Write an integer value to a PrefsFile
	  * @param prefs_file PrefsFile to write to
	  * @param section Section for value
	  * @param key Key for value
	  * @param value Value to write
	  */
	OP_STATUS WritePrefsInt(PrefsFile* prefs_file,
							const OpStringC8& section,
							const OpStringC8& key,
							int value);

	/** Write a string value to a PrefsFile
	  * @param prefs_file PrefsFile to write to
	  * @param section Section for value
	  * @param key Key for value
	  * @param value Value to write
	  */
	OP_STATUS WritePrefsString(PrefsFile* prefs_file,
							   const OpStringC8& section,
							   const OpStringC8& key,
							   const OpStringC& value);

	/** @overload */
	OP_STATUS WritePrefsString(PrefsFile* prefs_file,
							   const OpStringC8& section,
							   const OpStringC8& key,
							   const OpStringC8& value);

	OP_STATUS WritePrefsFileFolder(PrefsFile* prefs_file, 
							   const OpStringC8& section, 
							   const OpStringC8& key, 
							   const OpStringC& value, 
							   OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);


	/** Enable / disable a pref depending on which action is passed
	  * Typically used to set boolean prefs based on an action
	  * @param action Action to handle
	  * @param enable_action If action == enable_action, enable
	  * @param collection Which prefs collection to change
	  * @param setting Setting to change in collection
	  * @return Whether the pref was changed
	  */
	template<typename PrefsCollection, typename IntegerPref>
	BOOL SetPrefsToggleByAction(OpInputAction* action, OpInputAction::Action enable_action, PrefsCollection* collection, IntegerPref setting)
	{
		BOOL is_enabled = collection->GetIntegerPref(setting);
		BOOL enable = action->GetAction() == enable_action;
		if (is_enabled == enable)
			return FALSE;

		TRAPD(err, collection->WriteIntegerL(setting, enable));

		return TRUE;
	}
};

#endif // PREFSUTILS_H
