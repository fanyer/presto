/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

#ifdef USE_COMMON_RESOURCES

#include "adjunct/desktop_util/resources/ResourceUpgrade.h"
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#include "adjunct/desktop_util/resources/ResourceFolders.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"

#include "modules/prefsfile/prefsfile.h"

#include "modules/util/opfile/opfile.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceUpgrade::UpdatePref(const uni_char* old_filename, const uni_char* new_filename, OperaInitInfo *pinfo)
{
	OpDesktopResources *dr_temp; // Just for the autopointer
	RETURN_IF_ERROR(OpDesktopResources::Create(&dr_temp));
	OpAutoPtr<OpDesktopResources> resources(dr_temp);
	if(!resources.get())
		return OpStatus::ERR_NO_MEMORY;

	OpString old_profile_path;
	resources->GetOldProfileLocation(old_profile_path);

	OpFile newfile, oldfile;
	OpString path;
//	path.Set(pinfo->default_folders[OPFILE_USERPREFS_FOLDER]);
	path.Set(pinfo->default_folders[OPFILE_HOME_FOLDER]);
	if (path[path.Length() - 1] != PATHSEPCHAR)
		path.Append(PATHSEP);
	path.Append(new_filename);
	RETURN_IF_ERROR(newfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	BOOL exists;
	if (OpStatus::IsError(newfile.Exists(exists)) || exists) {
		// Nothing to do
		return OpStatus::OK;
	}

	path.Set(pinfo->default_folders[OPFILE_HOME_FOLDER]);
	if (path[path.Length() - 1] != PATHSEPCHAR)
		path.Append(PATHSEP);
	path.Append(old_profile_path);
	path.Append(old_filename);
	RETURN_IF_ERROR(oldfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	if (OpStatus::IsError(oldfile.Exists(exists)) || !exists) {
		// Nothing to do
		return OpStatus::OK;
	}

	int res_count = (sizeof(mResourceUpgrades)/sizeof(*mResourceUpgrades)) - 1;
	if (res_count == 0) {
		return newfile.CopyContents(&oldfile, TRUE);
	}

	OpString filepath;
	filepath.Reserve(2048); // FIXME: Proper macro
	OpString prefPathFull;
	prefPathFull.Reserve(2048); // FIXME: Proper macro
	OpString defaultPathFull;
	defaultPathFull.Reserve(2048); // FIXME: Proper macro

	OpString8 scan;
	RETURN_IF_ERROR(oldfile.Open(OPFILE_READ));
	RETURN_IF_ERROR(newfile.Open(OPFILE_WRITE));
//	g_op_system_info->GetNewlineString()
	char section[512];
	section[0] = 0;

	OpFile logfile;
	OpString8 lognote;
	path.Set(pinfo->default_folders[OPFILE_LOCAL_HOME_FOLDER]);
	if (path[path.Length() - 1] != PATHSEPCHAR)
		path.Append(PATHSEP);
	path.Append(UNI_L("upgrade.log"));
	RETURN_IF_ERROR(logfile.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_ERROR(logfile.Open(OPFILE_APPEND));

	lognote.Set("--------");
	lognote.Append(NEWLINE);
	logfile.Write(lognote.CStr(), lognote.Length());
//	time_t now = g_timecache->CurrentTime();
//	struct tm *datelocal = localtime(&now);
//	lognote.Reserve(200);
//	g_oplocale->op_strftime(lognote.CStr(), 199, UNI_L("%Y-%m-%d %H:%M:%S"), datelocal);

	while (OpStatus::IsSuccess(oldfile.ReadLine(scan)) && !oldfile.Eof())
	{
		char key[512], value[2048];
		if (scan[0] == '[') {
			char* end = strchr(scan.CStr()+1, ']');
			if (end && (end-scan.CStr()) <= 512) {
				memcpy(section, scan.CStr()+1, end-scan.CStr()-1);
				section[end-scan.CStr()-1] = 0;
			}
		}
		else if (scan.Length()) {
			char* delim = strchr(scan.CStr(), '=');
			if (delim && (delim-scan.CStr()) < 512 && strlen(delim+1)<2048) {
				memcpy(key, scan.CStr(), delim-scan.CStr());
				key[delim-scan.CStr()] = 0;
				delim++;
				strcpy(value, delim);
				BOOL skip = FALSE;
				for (int i = 0; i < res_count; i++) {
					if ((mResourceUpgrades[i].section && (op_strcmp(mResourceUpgrades[i].section, section) == 0 || op_strcmp(mResourceUpgrades[i].section, "*") == 0)) &&
						(mResourceUpgrades[i].key && (op_strcmp(mResourceUpgrades[i].key, key) == 0 || op_strcmp(mResourceUpgrades[i].key, "*") == 0)))
					{
						filepath.SetFromUTF8(value);
						resources->ExpandSystemVariablesInString(filepath.CStr(), prefPathFull.CStr(), prefPathFull.Capacity());

						if (mResourceUpgrades[i].olddefaultparent == OPFILE_SERIALIZED_FOLDER) {
							filepath.Set(mResourceUpgrades[i].olddefaultpath);
							resources->ExpandSystemVariablesInString(filepath.CStr(), defaultPathFull.CStr(), defaultPathFull.Capacity());
						}
						else {
							ResourceFolders::GetFolder(mResourceUpgrades[i].olddefaultparent, pinfo, defaultPathFull);
#ifdef MSWIN
				if (uni_strnicmp(mResourceUpgrades[i].olddefaultpath, UNI_L("mail"), 5) != 0)
#endif
							defaultPathFull.Append(old_profile_path);
							defaultPathFull.Append(mResourceUpgrades[i].olddefaultpath);
						}
						OpFile defaultvaluefile;
						OpFileInfo::Mode mode;
						defaultvaluefile.Construct(defaultPathFull.CStr(), OPFILE_ABSOLUTE_FOLDER);
						BOOL identical;
						BOOL exists;
						if (mResourceUpgrades[i].newpath && 
							OpStatus::IsSuccess(defaultvaluefile.Exists(exists)) && exists && 
							OpStatus::IsSuccess(defaultvaluefile.GetMode(mode)) && mode==OpFileInfo::DIRECTORY
							&& resources->IsSameVolume(prefPathFull.CStr(), defaultPathFull.CStr())) {
#ifdef MSWIN
							if (prefPathFull.CompareI(defaultPathFull.CStr(), defaultPathFull.Length()) == 0)
#else
							if (prefPathFull.Compare(defaultPathFull.CStr(), defaultPathFull.Length()) == 0)
#endif
							{
								// The pref points to a file inside inside the old default folder. Change pref to the same filename inside the new folder.
								// That file will not exist yet, but it will soon (see ResourceUpgrade::CopyOldResources)
								OpString newParent;
								OpString8 newParent8;
								if (defaultPathFull[defaultPathFull.Length() - 1] != PATHSEPCHAR)
									RETURN_IF_ERROR(defaultPathFull.Append(PATHSEP));

								if (mResourceUpgrades[i].olddefaultparent == OPFILE_SERIALIZED_FOLDER) {
									filepath.Set(mResourceUpgrades[i].newpath);
									resources->ExpandSystemVariablesInString(filepath.CStr(), newParent.CStr(), newParent.Capacity());
								}
								else {
									ResourceFolders::GetFolder(mResourceUpgrades[i].newparent, pinfo, newParent);
									newParent.Append(mResourceUpgrades[i].newpath);
								}

								OpFile newfile;
								newfile.Construct(newParent.CStr(), OPFILE_ABSOLUTE_FOLDER);
								if (OpStatus::IsError(newfile.Exists(exists)) || exists) {
									// The new parent already exists, so we wouldn't be able to move the old folder to it -> keep the old value
									break;
								}

								if (newParent[newParent.Length() - 1] != PATHSEPCHAR)
									RETURN_IF_ERROR(newParent.Append(PATHSEP));
								if (prefPathFull.Length() > defaultPathFull.Length())
									newParent.Append(prefPathFull.CStr()+defaultPathFull.Length());
								resources->SerializeFileName(newParent.CStr(), prefPathFull.CStr(), prefPathFull.Capacity());

								newParent8.SetUTF8FromUTF16(prefPathFull.CStr());
								if (OpStatus::IsError(scan.Set(key))) {skip=TRUE;break;}
								if (OpStatus::IsError(scan.Append("="))) {skip=TRUE;break;}
								if (OpStatus::IsError(scan.Append(newParent8.CStr()))) {skip=TRUE;break;}
//								if (OpStatus::IsError(scan.Append(filename8.CStr()))) continue;
								lognote.Empty();
								lognote.AppendFormat("Pref entry \"%s\" changed from \"%s\" to \"%s\"%s", key, value, newParent8.CStr(), NEWLINE);
								logfile.Write(lognote.CStr(), lognote.Length());
								
							}
						}
						else if (OpStatus::IsSuccess(ResourceFolders::ComparePaths(prefPathFull, defaultPathFull, identical)) && identical
							&& resources->IsSameVolume(prefPathFull.CStr(), defaultPathFull.CStr()))
						{
							lognote.Empty();
							lognote.AppendFormat("Pref entry \"%s\", which used to be \"%s\", was removed%s", key, value, NEWLINE);
							logfile.Write(lognote.CStr(), lognote.Length());
							skip=TRUE;
							break;		// This is the default pref. Discard.
						}
					}
				}
				if (skip)
					continue;
			}
		}
		scan.Append(NEWLINE);
		newfile.Write(scan.CStr(), scan.Length());
	}
	newfile.Close();
	logfile.Close();
	return OpStatus::OK;
}

OP_STATUS ResourceUpgrade::CopyOldResources(OperaInitInfo *pinfo)
{
	OpDesktopResources *dr_temp; // Just for the autopointer
	RETURN_IF_ERROR(OpDesktopResources::Create(&dr_temp));
	OpAutoPtr<OpDesktopResources> resources(dr_temp);
	if(!resources.get())
		return OpStatus::ERR_NO_MEMORY;

	OpString old_profile_path;
	resources->GetOldProfileLocation(old_profile_path);

	OpFile logfile;
	OpString8 lognote;
	OpString logpath;
	logpath.Set(pinfo->default_folders[OPFILE_LOCAL_HOME_FOLDER]);
	if (logpath[logpath.Length() - 1] != PATHSEPCHAR)
		logpath.Append(PATHSEP);
	logpath.Append(UNI_L("upgrade.log"));
	RETURN_IF_ERROR(logfile.Construct(logpath.CStr(), OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_ERROR(logfile.Open(OPFILE_APPEND));

	BOOL exists;
	OpString filepath;

	filepath.Reserve(2048); // FIXME: Proper macro
	int len = (sizeof(mResourceUpgrades)/sizeof(*mResourceUpgrades))-1;
	OpString defaultPathFull;
	defaultPathFull.Reserve(2048); // FIXME: Proper macro
	OpString newPathFull;
	newPathFull.Reserve(2048); // FIXME: Proper macro
	for (int i = 0; i < len; i++)
	{
		OpFile newfile, oldfile;
		if (mResourceUpgrades[i].olddefaultpath && mResourceUpgrades[i].newpath)
		{
			// Old Resource
			if (mResourceUpgrades[i].olddefaultparent == OPFILE_SERIALIZED_FOLDER) {
				filepath.Set(mResourceUpgrades[i].olddefaultpath);
				resources->ExpandSystemVariablesInString(filepath.CStr(), defaultPathFull.CStr(), defaultPathFull.Capacity());
			}
			else {
				ResourceFolders::GetFolder(mResourceUpgrades[i].olddefaultparent, pinfo, defaultPathFull);
#ifdef MSWIN
				if (uni_strnicmp(mResourceUpgrades[i].olddefaultpath, UNI_L("mail"), 5) != 0)
#endif
				defaultPathFull.Append(old_profile_path);
				defaultPathFull.Append(mResourceUpgrades[i].olddefaultpath);
			}

			// New Resource
			if (mResourceUpgrades[i].newparent == OPFILE_SERIALIZED_FOLDER) {
				filepath.Set(mResourceUpgrades[i].newpath);
				resources->ExpandSystemVariablesInString(filepath.CStr(), newPathFull.CStr(), newPathFull.Capacity());
			}
			else {
				ResourceFolders::GetFolder(mResourceUpgrades[i].newparent, pinfo, newPathFull);
				newPathFull.Append(mResourceUpgrades[i].newpath);
			}

			RETURN_IF_ERROR(oldfile.Construct(defaultPathFull.CStr(), OPFILE_ABSOLUTE_FOLDER));
			RETURN_IF_ERROR(newfile.Construct(newPathFull.CStr(), OPFILE_ABSOLUTE_FOLDER));
			if (OpStatus::IsError(newfile.Exists(exists)) || exists) {
				// Nothing to do
				continue;
			}
			if (OpStatus::IsError(oldfile.Exists(exists)) || !exists) {
				// Nothing to do
				continue;
			}
			if (!resources->IsSameVolume(newPathFull.CStr(), defaultPathFull.CStr()))
			{
				// Do not update
				continue;
			}
			lognote.Empty();
			lognote.AppendFormat("Moving file/folder \"%ls\" to \"%ls\"%s", defaultPathFull.CStr(), newPathFull.CStr(), NEWLINE);
			logfile.Write(lognote.CStr(), lognote.Length());

//			newfile.CopyContents(&oldfile, TRUE);
			DesktopOpFileUtils::Move(&oldfile, &newfile);
		}
	}
	return OpStatus::OK;
}

OP_STATUS ResourceUpgrade::RemoveIncompatibleLanguageFile(PrefsFile *pf)
{
	OpString	saved_language_file_path;
	OP_STATUS	err;

	// Check we have a prefs file object
	if (!pf)
		return OpStatus::ERR;

	// Create the PI interface object
	OpDesktopResources *dr_temp; // Just for the autopointer
	RETURN_IF_ERROR(OpDesktopResources::Create(&dr_temp));
	OpAutoPtr<OpDesktopResources> desktop_resources(dr_temp);
	if(!desktop_resources.get())
		return OpStatus::ERR_NO_MEMORY;

	TRAP(err, pf->ReadStringL("User Prefs", "Language File", saved_language_file_path));
	RETURN_IF_ERROR(err);

	// Remove any serialised path from the preference setting
	OpString out_path;
	out_path.Reserve(MAX_PATH);
	RETURN_IF_ERROR(OpStatus::IsSuccess(desktop_resources->ExpandSystemVariablesInString(saved_language_file_path.CStr(), out_path.CStr(), MAX_PATH)));
	RETURN_IF_ERROR(saved_language_file_path.Set(out_path.CStr()));

	if (!saved_language_file_path.IsEmpty())
	{
		INT32 version = -1;

		OpFile file;
		RETURN_IF_ERROR(file.Construct(saved_language_file_path.CStr()));
		if (OpStatus::IsSuccess(file.Open(OPFILE_READ)))
		{
			for( INT32 i=0; i<100; i++ )
			{
				OpString8 buf;
				if (OpStatus::IsError(file.ReadLine(buf)))
					break;
					
				buf.Strip(TRUE, FALSE);
				if (buf.CStr() && op_strnicmp(buf.CStr(), "DB.version", 10) == 0)
				{
					for (const char *p = &buf.CStr()[10]; *p; p++)
					{
						if (op_isdigit(*p))
						{
							sscanf(p,"%d",&version);
							break;
						}
					}
					break;
				}
			}
		}

		// If the language file is less than version 811 throw it away!
		if (version < 811)
		{
			TRAP(err, pf->DeleteKeyL("User Prefs", "Language File"));
			RETURN_IF_ERROR(err);
			TRAP(err, pf->CommitL(TRUE, FALSE)); // Keep data loaded
			RETURN_IF_ERROR(err);
		}
	}
	
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif // USE_COMMON_RESOURCES


