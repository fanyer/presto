/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas) and Espen Sand (espen)
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/plugins/unix_pluginpath.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/util/opfile/opfile.h"
#include "platforms/posix/posix_file_util.h"
#include "platforms/posix/posix_native_util.h"

// static
PluginPathList* PluginPathList::Self()
{
	static OpAutoPtr<PluginPathList> pluginPathList;

	if (!pluginPathList.get())
	{
		pluginPathList = OP_NEW(PluginPathList, ());
#ifdef ENABLE_MEMORY_DEBUGGING
		if (pluginPathList.get())
			OpMemDebug_Disregard(pluginPathList.get());
#endif // ENABLE_MEMORY_DEBUGGING
	}

	return pluginPathList.get();
}

#ifdef PREFS_WRITE
void PluginPathList::WriteL(OpVector<PluginPathElement>& list)
{
	OpFile personal_ini_file;
	LEAVE_IF_ERROR(personal_ini_file.Construct(UNI_L("pluginpath.ini"), OPFILE_HOME_FOLDER));

	OpStackAutoPtr<PrefsFile> personal_prefs(OP_NEW_L(PrefsFile, (PREFS_STD)));
	personal_prefs->ConstructL();
	personal_prefs->SetFileL(&personal_ini_file);
	personal_prefs->LoadAllL();
	personal_prefs->DeleteSectionL(UNI_L("Paths")); // So that the path order is preserved

	ANCHORD(OpString, path);
	ANCHORD(OpAutoVector<PluginPathElement>, tmp_plugin_path_list);
	tmp_plugin_path_list.SetAllocationStepSize(list.GetCount());

	for (UINT32 i=0; i<list.GetCount(); i++)
	{
		PluginPathElement* e = list.Get(i);
		if (e && e->path.Length() > 0)
		{
			personal_prefs->WriteIntL(UNI_L("Paths"), e->path.CStr(), e->state);

			PluginPathElement* plugin_path = OP_NEW_L(PluginPathElement, (*e));
			ANCHOR_PTR(PluginPathElement, plugin_path);
			LEAVE_IF_ERROR(tmp_plugin_path_list.Add(plugin_path));
			ANCHOR_PTR_RELEASE(plugin_path);

			if (e->state == 1)
			{
				if (!path.IsEmpty())
					path.AppendL(":");
				path.AppendL(e->path.CStr());
			}
		}
	}

	m_plugin_path_list.Swap(tmp_plugin_path_list);
	tmp_plugin_path_list.DeleteAll();

	personal_prefs->CommitL();

	g_pcapp->WriteStringL(PrefsCollectionApp::PluginPath, path);

}
#endif //PREFS_WRITE

void PluginPathList::ReadL(const OpStringC& default_path, OpString& new_path)
{
	OpFile shared_ini_file;
	LEAVE_IF_ERROR(shared_ini_file.Construct(UNI_L("pluginpath.ini"), OPFILE_INI_FOLDER));

	OpFile personal_ini_file;
	LEAVE_IF_ERROR(personal_ini_file.Construct(UNI_L("pluginpath.ini"), OPFILE_HOME_FOLDER));

	OpStackAutoPtr<PrefsFile> shared_prefs(OP_NEW_L(PrefsFile, (PREFS_STD)));
	shared_prefs->ConstructL();
	shared_prefs->SetFileL(&shared_ini_file);
	shared_prefs->LoadAllL();

	OpStackAutoPtr<PrefsFile> personal_prefs(OP_NEW_L(PrefsFile, (PREFS_STD)));
	personal_prefs->ConstructL();
	personal_prefs->SetFileL(&personal_ini_file);
	personal_prefs->LoadAllL();


	// Holds every path entry we want to examine
	ANCHORD(OpAutoVector<OpString>, candidate_list);

	OpString* s = NULL;
	ANCHOR_PTR(OpString, s);
	BOOL exists;

	// 1 Add path to the the non-default install directory in case that has been used
	const char* prefix_plugin_path = op_getenv("OPERA_PREFIX_PLUGIN_PATH");
	if (prefix_plugin_path && *prefix_plugin_path)
	{
		s = OP_NEW_L(OpString, ());
		s->SetL(prefix_plugin_path);
		LEAVE_IF_ERROR(candidate_list.Add(s));
	}

	// 2 Add the default plugin install path first. Will speed up detection on startup.
	s = OP_NEW_L(OpString, ());
	s->SetL(UNI_L("/usr/lib/opera/plugins"));
	LEAVE_IF_ERROR(candidate_list.Add(s));


	// 3 Add paths from the shared pluginpath.ini file that comes with the install package
	exists = FALSE;
	LEAVE_IF_ERROR(shared_ini_file.Exists(exists));
	if (exists)
	{
		OpStackAutoPtr<PrefsSection> shared_section(shared_prefs->ReadSectionL(UNI_L("Paths")));
		if (shared_section.get())
		{
			const PrefsEntry* shared_entry = (const PrefsEntry*)shared_section->Entries();
			while (shared_entry)
			{
				if (shared_entry->Key() && *shared_entry->Key())
				{
					s = OP_NEW_L(OpString, ());
					s->SetL(shared_entry->Key());
					LEAVE_IF_ERROR(candidate_list.Add(s));
				}
				shared_entry = (const PrefsEntry*)shared_entry->Suc();
			}
		}

		// Add entries from KDE plugin path file. The path to this file is saved in the shared pluginpath.ini file
		OpString kde_plugin_file;
		shared_prefs->ReadStringL("KDE", "plugins", kde_plugin_file);
		OpFile kde_file; ANCHOR(OpFile, kde_file);
		if (kde_plugin_file.IsEmpty())
			return;

		LEAVE_IF_ERROR(kde_file.Construct(kde_plugin_file.CStr(), OPFILE_SERIALIZED_FOLDER));
		BOOL kde_plugin_exists;
		if (OpStatus::IsError(kde_file.Exists(kde_plugin_exists)))
			kde_plugin_exists = FALSE;
		if (kde_plugin_exists)
		{
			OpStackAutoPtr<PrefsFile> kde_prefs(OP_NEW_L(PrefsFile, (PREFS_INI)));
			kde_prefs->ConstructL();
			kde_prefs->SetFileL(&kde_file);
			kde_prefs->LoadAllL();

			OpString line; ANCHOR(OpString, line);
			kde_prefs->ReadStringL("Misc", "scanPaths", line);
			OpString expanded_line; ANCHOR(OpString, expanded_line);
			LEAVE_IF_ERROR(g_op_system_info->ExpandSystemVariablesInString(line.CStr(), &expanded_line));
			if (!expanded_line.IsEmpty())
			{
				uni_char* p = uni_strtok(expanded_line.CStr(),UNI_L(","));
				while (p)
				{
					s = OP_NEW_L(OpString, ());
					s->SetL(p);
					s->Strip();
					LEAVE_IF_ERROR(candidate_list.Add(s));
					p = uni_strtok(NULL,UNI_L(","));
				}
			}
		}
	}


	// 4 If we have no personal pluginpath.ini file then use default path

	exists = FALSE;
	LEAVE_IF_ERROR(personal_ini_file.Exists(exists));
	if (!exists)
 	{
		ANCHORD(OpString, plugin_paths);
		plugin_paths.SetL(default_path);

		if (plugin_paths.CStr() && plugin_paths.Length() > 10)
		{
			uni_char* p = uni_strtok(&plugin_paths.CStr()[10], UNI_L(":"));
			while (p)
			{
				s = OP_NEW_L(OpString, ());
				s->SetL(p);
				LEAVE_IF_ERROR(candidate_list.Add(s));
				p = uni_strtok(NULL,UNI_L(","));
			}
		}
	}

	// 5 Add some fallbacks
	const char *fallback_paths[] =
	{
		"/opt/netscape/plugins",           // SuSE
		"/usr/lib/netscape/plugins-libc6", // Debian
		"/usr/local/netscape/plugins",     // Redhat
		0
	};
	for (int i=0; fallback_paths[i]; i++)
	{
		s = OP_NEW_L(OpString, ());
		s->SetL(fallback_paths[i]);
		LEAVE_IF_ERROR(candidate_list.Add(s));
	}

	ANCHORD(OpString, key);
	ANCHORD(OpString, realkey);
	key.ReserveL(_MAX_PATH);
	realkey.ReserveL(_MAX_PATH);
	BOOL must_commit = FALSE;

	/* 6 Write new candidate entries to the personal pluginpath.ini file if the entry
	 * a) is not in the personal file already; and
	 * b) is a valid path.
	 */
	for (UINT32 i=0; i<candidate_list.GetCount(); i++)
	{
		// The entry can be using '~' or environment flags
		if (OpStatus::IsSuccess(g_op_system_info->ExpandSystemVariablesInString(candidate_list.Get(i)->CStr(), &key)) && !key.IsEmpty())
		{
			// check if file exists and get real path
			PosixNativeUtil::NativeString key_path (key.CStr());
			int accessval = access(key_path.get(), R_OK);

			char realpath[_MAX_PATH + 1]; /* ARRAY OK molsson 2011-12-12 */
			OP_STATUS realpathval = PosixFileUtil::RealPath(key_path.get(), realpath);
			if (accessval == 0 && OpStatus::IsSuccess(realpathval) &&
				OpStatus::IsSuccess(PosixNativeUtil::FromNative(realpath, &realkey)))
			{
				int value = personal_prefs->ReadIntL(UNI_L("Paths"),realkey.CStr(),-100);
				if (value == -100)
				{
#ifdef PREFS_WRITE
					personal_prefs->WriteIntL(UNI_L("Paths"), realkey.CStr(), 1);
#endif //PREFS_WRITE
					must_commit = TRUE;
				}
			}
		}
	}

	if( must_commit )
	{
		personal_prefs->CommitL();
		personal_prefs->LoadAllL();
	}


	// 7 We now have a personal ini file that we can read from
	m_plugin_path_list.DeleteAll();
	PrefsSection* personal_section = personal_prefs->ReadSectionL(UNI_L("Paths"));
	if (personal_section)
	{
		const PrefsEntry* personal_entry = (const PrefsEntry*)personal_section->Entries();
		while (personal_entry)
		{
			if (personal_entry->Key() && *personal_entry->Key())
			{
				if (OpStatus::IsSuccess(g_op_system_info->ExpandSystemVariablesInString(personal_entry->Key(), &key)) && !key.IsEmpty())
				{
					PosixNativeUtil::NativeString key_path (key.CStr());
					int accessval = access(key_path.get(), R_OK);

					if (personal_entry->Value() && *personal_entry->Value())
					{
						INT32 value = uni_atoi(personal_entry->Value());

						PluginPathElement* item = OP_NEW_L(PluginPathElement, ());
						ANCHOR_PTR(PluginPathElement, item);
						item->path.SetL(key.CStr());
						item->state = accessval == 0 ? value : 0;

						LEAVE_IF_ERROR(m_plugin_path_list.Add(item));
						ANCHOR_PTR_RELEASE(item);

						if (accessval == 0 && value == 1)
						{
							if (!new_path.IsEmpty())
							{
								new_path.AppendL(UNI_L(":"));
							}
							new_path.AppendL(key.CStr());
						}
					}
				}
			}
			personal_entry = (const PrefsEntry*)personal_entry->Suc();
		}
		OP_DELETE(personal_section);
	}
}

#endif // X11API && NS4P_COMPONENT_PLUGINS

