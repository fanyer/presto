/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "adjunct/quick/dialogs/SaveSessionDialog.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"

SaveSessionDialog::SaveSessionDialog()
{
}

void SaveSessionDialog::OnInit()
{
	OpFile file;
	TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::WindowsStorageFile, file));
	OpString nameonly;
	OpString session_name;

	if (file.GetFullPath())
	{
		nameonly.Set(file.GetName(), MAX_PATH-1);
		OpString tmp_storage;
		session_name.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SESSION_FOLDER, tmp_storage));
		session_name.Append(nameonly);
	}
	else	//default to a name in the session folder
	{
		OpString tmp_storage;
		session_name.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SESSION_FOLDER, tmp_storage));
		session_name.Append(UNI_L("windows.win"));
	}
	if (nameonly.Length() > 4)
	{
		nameonly.Delete(nameonly.Length() - 4);
	}
	SetWidgetText("Sessionfile_edit", nameonly.CStr());	
	
	BOOL fUseAtStartup =
		(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType)==STARTUP_HISTORY || g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType)==STARTUP_WINHOMES);
	
	SetWidgetValue( "Use_session_on_startup", fUseAtStartup);

#ifdef PERMANENT_HOMEPAGE_SUPPORT
	BOOL permanent_homepage = (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage));
	if (permanent_homepage)
	{
		OpCheckBox* startup_checkbox = (OpCheckBox*)GetWidgetByName("Use_session_on_startup");
		if (startup_checkbox)
		{
			startup_checkbox->SetVisibility(FALSE);
		}
	}
#endif
}


UINT32 SaveSessionDialog::OnOk()
{
	return Dialog::OnOk();
}


BOOL SaveSessionDialog::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OK)
	{
		OpString filename;
		GetWidgetText("Sessionfile_edit", filename);

		if (((DesktopOpSystemInfo*)g_op_system_info)->RemoveIllegalFilenameCharacters(filename, TRUE))
		{
			//make sure it has win as extension
			int length = filename.Length();
			if (length < 4 || uni_stricmp(UNI_L(".win"), filename.CStr() + (length - 4)))
			{
				filename.Append(UNI_L(".win"));
			}
			OpString tmp_storage;
			filename.Insert(0, g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SESSION_FOLDER, tmp_storage));

			OpFile file;
			OP_STATUS err = file.Construct(filename.CStr());
			if (OpStatus::IsSuccess(err))
			{
				// check if we overwrite an existing file
				BOOL exists;
				if (OpStatus::IsSuccess(file.Exists(exists)) && exists)
				{
					OpString message, title;
					g_languageManager->GetString(Str::D_OVERWRITE_SESSION_WARNING, message);
					g_languageManager->GetString(Str::D_OVERWRITE_SESSION_LABEL, title);

					int result = SimpleDialog::ShowDialog(WINDOW_NAME_WARN_OVERWRITE_SESSION, this, message.CStr(), title.CStr(), Dialog::TYPE_YES_NO, Dialog::IMAGE_WARNING);
					if (result != Dialog::DIALOG_RESULT_YES)
					{
						return FALSE;
					}
				}

				// actually save

				BOOL use_at_startup = GetWidgetValue("Use_session_on_startup");
				BOOL only_save_avtive_window = GetWidgetValue("Only_save_active_window");

				g_session_auto_save_manager->SaveCurrentSession(filename.CStr(), FALSE, TRUE, only_save_avtive_window);
				if (use_at_startup)
					TRAP(err,g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::WindowsStorageFile, &file));

				//	we might have to change the startup type if he wants to use it at startup
				if (use_at_startup)
				{
					STARTUPTYPE startupType =
						STARTUPTYPE(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType));
					if (startupType!=STARTUP_HISTORY || startupType!=STARTUP_WINHOMES)
						startupType = STARTUP_HISTORY;
					TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StartupType, startupType));
				}
				TRAP(err,g_prefsManager->CommitL());
			}
		}
	}

	return Dialog::OnInputAction(action);
}
