/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Axel Siebert (axel) / Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/prefs/PrefsUtils.h"

#include "modules/prefsfile/prefsfile.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opfolder.h"

/***********************************************************************************
 ** Read an integer value from a PrefsFile
 **
 ** PrefsUtils::ReadPrefsIntInt
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::ReadPrefsIntInt(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, int def_value, int& result)
{
	TRAPD(status, result = prefs_file->ReadIntL(section, key, def_value));
	return status;
}

/***********************************************************************************
 ** Read a bool value from a PrefsFile
 **
 ** PrefsUtils::ReadPrefsBool
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::ReadPrefsBool(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, bool def_value, bool &result)
{
	int intresult;
	OP_STATUS status = ReadPrefsIntInt(prefs_file, section, key, def_value, intresult);
	result = intresult != 0;
	return status;
}

/***********************************************************************************
 ** Read a string value from a PrefsFile
 **
 ** PrefsUtils::ReadPrefsString
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::ReadPrefsString(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, const OpStringC& def_value, OpString& result)
{
	TRAPD(status, prefs_file->ReadStringL(section, key, result, def_value));
	return status;
}

/***********************************************************************************
 ** Read a string value from a PrefsFile
 **
 ** PrefsUtils::ReadPrefsString
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::ReadPrefsString(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, OpString& result)
{
	return ReadPrefsString(prefs_file, section, key, NULL, result);
}


/***********************************************************************************
 ** Read a string value from a PrefsFile
 **
 ** PrefsUtils::ReadPrefsString
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::ReadPrefsString(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, OpString8& result)
{
	OpString result16;
	RETURN_IF_ERROR(ReadPrefsString(prefs_file, section, key, result16));
	return result.Set(result16.CStr());
}

/***********************************************************************************
 ** Read file folder from a PrefsFile
 **
 ** PrefsUtils::ReadFolderPath
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::ReadFolderPath(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, OpString& result, const OpStringC &default_result, OpFileFolder folder/*=OPFILE_ABSOLUTE_FOLDER*/)
{
	RETURN_IF_LEAVE(prefs_file->ReadStringL(section, key, result, default_result));
	
	OpFile tmp_opfile;
	if(OpStatus::IsSuccess(tmp_opfile.Construct(result.CStr(), OPFILE_SERIALIZED_FOLDER)))
		RETURN_IF_ERROR(result.Set(tmp_opfile.GetFullPath()));

	return OpStatus::OK;
}

/***********************************************************************************
 ** Write an integer value to a PrefsFile
 **
 ** PrefsUtils::WritePrefsInt
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::WritePrefsInt(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, int value)
{
	TRAPD(status, prefs_file->WriteIntL(section, key, value));
	return status;
}


/***********************************************************************************
 ** Write a string value to a PrefsFile
 **
 ** PrefsUtils::WritePrefsString
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::WritePrefsString(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, const OpStringC& value)
{
	TRAPD(status, prefs_file->WriteStringL(section, key, value));
	return status;
}


/***********************************************************************************
 ** Write a string value to a PrefsFile
 **
 ** PrefsUtils::WritePrefsString
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::WritePrefsString(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, const OpStringC8& value)
{
	OpString value16;
	RETURN_IF_ERROR(value16.Set(value));
	TRAPD(status, prefs_file->WriteStringL(section, key, value16));
	return status;
}

/***********************************************************************************
 ** Write file folder to a PrefsFile
 **
 ** PrefsUtils::WritePrefsFileFolder
 **
 ***********************************************************************************/
OP_STATUS PrefsUtils::WritePrefsFileFolder(PrefsFile* prefs_file, const OpStringC8& section, const OpStringC8& key, const OpStringC& value, OpFileFolder folder/*=OPFILE_ABSOLUTE_FOLDER*/)
{
	OpFile tmp_opfile;
	if (OpStatus::IsSuccess(tmp_opfile.Construct(value, folder)))
		RETURN_IF_ERROR(prefs_file->WriteStringL(section, key, tmp_opfile.GetSerializedName()));
	else
		RETURN_IF_ERROR(prefs_file->WriteStringL(section, key, value));

	return OpStatus::OK;
}
