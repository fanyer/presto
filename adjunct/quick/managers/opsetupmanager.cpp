/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/managers/opsetupmanager.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/util/filefun.h"
#include "modules/util/opstrlst.h"

// This can't be a desktop manager as it's used before the desktop managers are initialized
OpSetupManager* OpSetupManager::s_instance = NULL;

//==================================================================================
// Update these when new features are added that make older setups incompatible
// (This is only used for displaying 'incompatible version' dialogs to user)
#define CURRENT_TOOLBAR_VERSION_NUMBER		15
#define CURRENT_MENU_VERSION_NUMBER			2
#define CURRENT_KEYBOARD_VERSION_NUMBER		1
#define CURRENT_MOUSE_VERSION_NUMBER		1
//==================================================================================

#ifdef EMBROWSER_SUPPORT
extern long gVendorDataID;
#endif //EMBROWSER_SUPPORT

/***********************************************************************************
**
**	OpSetupManager
**
***********************************************************************************/

OpSetupManager::OpSetupManager()
	: m_mouse_prefsfile(NULL)
	, m_default_mouse_prefsfile(NULL)
	, m_keyboard_prefsfile(NULL)
	, m_default_keyboard_prefsfile(NULL)
	, m_menu_prefsfile(NULL)
	, m_custom_menu_prefsfile(NULL)
	, m_default_menu_prefsfile(NULL)
	, m_toolbar_prefsfile(NULL)
	, m_custom_toolbar_prefsfile(NULL)
	, m_default_toolbar_prefsfile(NULL)
	, m_override_dialog_prefsfile(NULL)
	, m_custom_dialog_prefsfile(NULL)
	, m_dialog_prefsfile(NULL)
#ifdef PREFS_HAVE_FASTFORWARD
	, m_fastforward_section(NULL)
#endif // PREFS_HAVE_FASTFORWARD
{
}

/***********************************************************************************
**
**	~OpSetupManager
**
***********************************************************************************/

OpSetupManager::~OpSetupManager()
{
	TRAPD(err, CommitL());
	OpStatus::Ignore(err);

	OP_DELETE(m_mouse_prefsfile);
	OP_DELETE(m_default_mouse_prefsfile);
	OP_DELETE(m_keyboard_prefsfile);
	OP_DELETE(m_default_keyboard_prefsfile);
	OP_DELETE(m_menu_prefsfile);
	OP_DELETE(m_toolbar_prefsfile);
	OP_DELETE(m_custom_menu_prefsfile);
	OP_DELETE(m_custom_toolbar_prefsfile);
	OP_DELETE(m_default_toolbar_prefsfile);
	OP_DELETE(m_override_dialog_prefsfile);
	OP_DELETE(m_custom_dialog_prefsfile);
	OP_DELETE(m_dialog_prefsfile);
	OP_DELETE(m_default_menu_prefsfile);
#ifdef PREFS_HAVE_FASTFORWARD
	OP_DELETE(m_fastforward_section);
#endif // PREFS_HAVE_FASTFORWARD
}


/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS OpSetupManager::Init()
{
	TRAPD(rc, InitL());
	return rc;
}


/***********************************************************************************
**
**	InitL
**
***********************************************************************************/

void OpSetupManager::InitL()
{
	OpFile mousefile; ANCHOR(OpFile, mousefile);
	OpFile keyboardfile; ANCHOR(OpFile, keyboardfile);
	OpFile toolbarfile; ANCHOR(OpFile, toolbarfile);
	OpFile menufile; ANCHOR(OpFile, menufile);
	OpFile dialogfile; ANCHOR(OpFile, dialogfile);
# ifdef PREFS_HAVE_FASTFORWARD
	OpFile fastforwardfile; ANCHOR(OpFile, fastforwardfile);
# endif // PREFS_HAVE_FASTFORWARD

	g_pcfiles->GetFileL(PrefsCollectionFiles::ToolbarConfig, toolbarfile);
	g_pcfiles->GetFileL(PrefsCollectionFiles::MenuConfig, menufile);
	g_pcfiles->GetFileL(PrefsCollectionFiles::DialogConfig,  dialogfile);
# ifdef PREFS_HAVE_FASTFORWARD
	g_pcfiles->GetFileL(PrefsCollectionFiles::FastForwardFile, fastforwardfile);
# endif // PREFS_HAVE_FASTFORWARD
	g_pcfiles->GetFileL(PrefsCollectionFiles::KeyboardConfig, keyboardfile);
	g_pcfiles->GetFileL(PrefsCollectionFiles::MouseConfig, mousefile);

	////////////////// toolbar configuration file //////////////////////
	m_toolbar_prefsfile = InitializeCustomPrefsFileL(&toolbarfile, "standard_toolbar.ini", OPFILE_TOOLBARSETUP_FOLDER);
	m_custom_toolbar_prefsfile = InitializeCustomPrefsFileL(NULL, "standard_toolbar.ini", OPFILE_UI_INI_CUSTOM_FOLDER);

	////////////////// default toolbar configuration file //////////////////////
	m_default_toolbar_prefsfile = InitializeCustomPrefsFileL(NULL, "standard_toolbar.ini", OPFILE_UI_INI_FOLDER);

	////////////////// dialog configuration file //////////////////////
	m_override_dialog_prefsfile = InitializeCustomPrefsFileL(&dialogfile, "dialog.ini", OPFILE_TOOLBARSETUP_FOLDER);
	m_dialog_prefsfile = InitializeCustomPrefsFileL(NULL, "dialog.ini", OPFILE_UI_INI_FOLDER);
	m_custom_dialog_prefsfile = InitializeCustomPrefsFileL(NULL, "dialog.ini", OPFILE_UI_INI_CUSTOM_FOLDER);

	////////////////// default menu configuration file //////////////////////
	m_default_menu_prefsfile = InitializeCustomPrefsFileL(NULL, "standard_menu.ini", OPFILE_UI_INI_FOLDER);

	////////////////// mouse, keyboard and menu configuration file //////////////////////
#ifdef EMBROWSER_SUPPORT
	if (gVendorDataID != 'OPRA')
	{
		m_mouse_prefsfile = InitializePrefsFileL(&mousefile, "embedded_mouse.ini", 0);
		m_keyboard_prefsfile = InitializePrefsFileL(&keyboardfile, "embedded_keyboard.ini", 0);
		m_menu_prefsfile = InitializePrefsFileL(&menufile, "embedded_menu.ini", 0);
	}
	else
#endif // EMBROWSER_SUPPORT
	{
		m_mouse_prefsfile = InitializePrefsFileL(&mousefile, "standard_mouse.ini", 0);
		m_keyboard_prefsfile = InitializePrefsFileL(&keyboardfile, "standard_keyboard.ini", 0);
		m_custom_menu_prefsfile = InitializeCustomPrefsFileL(NULL, "standard_menu.ini", OPFILE_UI_INI_CUSTOM_FOLDER);
		m_menu_prefsfile = InitializeCustomPrefsFileL(&menufile, "standard_menu.ini", OPFILE_MENUSETUP_FOLDER);
	}

	////////////////// default mouse configuration file //////////////////////
	m_default_mouse_prefsfile = InitializePrefsFileL(NULL, "standard_mouse.ini", 0);

	////////////////// default keyboard configuration file //////////////////////
	m_default_keyboard_prefsfile = InitializePrefsFileL(NULL, "standard_keyboard.ini", 0);

#ifdef PREFS_HAVE_FASTFORWARD
	////////////////// fastforward configuration file //////////////////////
	PrefsFile *fastforward_prefsfile = InitializeCustomPrefsFileL(&fastforwardfile, "fastforward.ini", OPFILE_UI_INI_FOLDER);
	if (fastforward_prefsfile)
	{
		m_fastforward_section = fastforward_prefsfile->ReadSectionL(UNI_L("Fast forward"));
		OP_DELETE(fastforward_prefsfile);

		// Load the custom fast forward over the top
		OpStackAutoPtr<PrefsFile> custom_fastforward_prefsfile (InitializeCustomPrefsFileL(NULL, "fastforward.ini", OPFILE_UI_INI_CUSTOM_FOLDER));

		if (custom_fastforward_prefsfile.get())
		{
			OpStackAutoPtr<PrefsSection> custom_fastforward_section (custom_fastforward_prefsfile->ReadSectionL(UNI_L("Fast forward")));

			// Copy this section over the last section
			m_fastforward_section->CopyKeysL(custom_fastforward_section.get());
		}
	}
#endif // PREFS_HAVE_FASTFORWARD

	ScanSetupFoldersL();	//need to know which files is where
}

void OpSetupManager::CommitL()
{
	if (m_mouse_prefsfile)
		m_mouse_prefsfile->CommitL(FALSE, FALSE);

	if (m_keyboard_prefsfile)
		m_keyboard_prefsfile->CommitL(FALSE, FALSE);

	if (m_menu_prefsfile)
		m_menu_prefsfile->CommitL(FALSE, FALSE);

	if (m_toolbar_prefsfile)
		m_toolbar_prefsfile->CommitL(FALSE, FALSE);
}

/***********************************************************************************
**
**	GetCurrentSetupRevision
**
***********************************************************************************/

INT32 OpSetupManager::GetCurrentSetupRevision(enum OpSetupType type)
{
	switch(type)
	{
		case OPTOOLBAR_SETUP:
			return CURRENT_TOOLBAR_VERSION_NUMBER;
		case OPMENU_SETUP:
			return CURRENT_MENU_VERSION_NUMBER;
		case OPMOUSE_SETUP:
			return CURRENT_MOUSE_VERSION_NUMBER;
		case OPKEYBOARD_SETUP:
			return CURRENT_KEYBOARD_VERSION_NUMBER;
		default:
			return -1;
	}
}

/***********************************************************************************
**
**	InitializePrefsFileL
**
***********************************************************************************/

PrefsFile* OpSetupManager::InitializePrefsFileL(OpFile* defaultfile, const char* fallbackfile, INT32 flag )
{
    PrefsFile* prefsfile = OP_NEW_L(PrefsFile, (PREFS_STD));
    OpFile* file; // warning: variable 'file' might be clobbered by 'longjmp' or 'vfork'
    OP_STATUS err; // FIXME: status values saved here aren't checked for error !
    BOOL fileexists = FALSE;

	if (defaultfile && OpStatus::IsSuccess(defaultfile->Exists(fileexists)) && fileexists)
	{
		file = defaultfile;
	}
    else
    {
		file = OP_NEW_L(OpFile, ()); // FIXME: leaks prefsfile if it LEAVE()s

		OpString fallback;
		LEAVE_IF_ERROR(fallback.Set(fallbackfile));
        err = file->Construct(fallback.CStr(), OPFILE_USERPREFS_FOLDER);
        err = file->Exists(fileexists);
        if (!fileexists) // fallback to system folder:
        {
            OP_DELETE(file);
            file = OP_NEW_L(OpFile, ()); // FIXME: leaks prefsfile if it LEAVE()s
            err = file->Construct(fallback.CStr(), OPFILE_UI_INI_FOLDER);
        }
    }

	// FIXME: handle errors !
    TRAP(err, prefsfile->ConstructL());
    TRAP(err, prefsfile->SetFileL(file));
    TRAP(err, prefsfile->LoadAllL());

	// ... but note that error handling mustn't leak these (or prefsfile) !
	if (file != defaultfile)
		OP_DELETE(file);

	return prefsfile;
}

PrefsFile* OpSetupManager::InitializeCustomPrefsFileL(OpFile* defaultfile, const char* pref_filename, OpFileFolder op_folder)
{
    BOOL fileexists = FALSE;

    PrefsFile* prefsfile = OP_NEW_L(PrefsFile, (PREFS_STD));
    OpFile* file; // warning: variable 'file' might be clobbered by 'longjmp' or 'vfork'
    OP_STATUS err; // FIXME: status values saved here aren't checked for error !

	if (defaultfile && OpStatus::IsSuccess(defaultfile->Exists(fileexists)) && fileexists)
	{
		file = defaultfile;
	}
	else
	{
		file = OP_NEW_L(OpFile, ()); // FIXME: leaks prefsfile if it LEAVE()s
		OpString filename;
		LEAVE_IF_ERROR(filename.Set(pref_filename));
		err = file->Construct(filename.CStr(), op_folder);
	}

	// FIXME: handle errors !
	TRAP(err, prefsfile->ConstructL());
	TRAP(err, prefsfile->SetFileL(file));
	TRAP(err, prefsfile->LoadAllL());

	// ... but note that error handling mustn't leak these (or prefsfile) !
	if (file != defaultfile)
		OP_DELETE(file);
	
    return prefsfile;
}

/***********************************************************************************
**
**	ScanSetupFoldersL
**
***********************************************************************************/

void OpSetupManager::ScanSetupFoldersL()
{
    m_toolbar_setup_list.DeleteAll();
    m_menu_setup_list.DeleteAll();
    m_mouse_setup_list.DeleteAll();
    m_keyboard_setup_list.DeleteAll();

    //manually add the known files to the top of the vector, we are a bit bold here, but lets assume the installer makes no mistakes
    // these are today :
    //
    // toolbar
    //  standard_toolbar.ini *
    //  minimal_toolbar.ini
    // keyboard
    //  standard_keyboard.ini *
    // mouse
    //  standard_mouse.ini *
    // menu
    //  standard_menu.ini *
    //
    // * these are fallback-files, meaning if a copy is made, an empty will be sufficient

	OpString defaultsfolder;
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_UI_INI_FOLDER, defaultsfolder));

	OpString* defaults_standard_toolbarfile = OP_NEW(OpString, ());
	defaults_standard_toolbarfile->SetL(defaultsfolder);
	defaults_standard_toolbarfile->AppendL(UNI_L("standard_toolbar.ini"));
	m_toolbar_setup_list.Add(defaults_standard_toolbarfile); // FIXME: MAP

	OpString* defaults_standard_menufile = OP_NEW(OpString, ());
	defaults_standard_menufile->SetL(defaultsfolder);
	defaults_standard_menufile->AppendL(UNI_L("standard_menu.ini"));
	m_menu_setup_list.Add(defaults_standard_menufile);
	RemoveNonExistingFiles(m_menu_setup_list);

	OpString* defaults_standard_mousefile = OP_NEW(OpString, ());
	defaults_standard_mousefile->SetL(defaultsfolder);
	defaults_standard_mousefile->AppendL(UNI_L("standard_mouse.ini"));
	m_mouse_setup_list.Add(defaults_standard_mousefile);
	RemoveNonExistingFiles(m_mouse_setup_list);

	OpString* defaults_standard_keyboardfile = OP_NEW(OpString, ());
	defaults_standard_keyboardfile->SetL(defaultsfolder);
	defaults_standard_keyboardfile->AppendL(UNI_L("standard_keyboard.ini"));
	m_keyboard_setup_list.Add(defaults_standard_keyboardfile);

	// 9.2 compatable file
	OpString* defaults_standard_keyboard_compat_file = OP_NEW(OpString, ());
	defaults_standard_keyboard_compat_file->SetL(defaultsfolder);
	defaults_standard_keyboard_compat_file->AppendL(UNI_L("standard_keyboard_compat.ini"));
	m_keyboard_setup_list.Add(defaults_standard_keyboard_compat_file);

#if defined(UNIX)
	OpString* unix_keyboardfile = OP_NEW(OpString, ());
	unix_keyboardfile->SetL(defaultsfolder);
	unix_keyboardfile->AppendL(UNI_L("unix_keyboard.ini"));
	m_keyboard_setup_list.Add(unix_keyboardfile);
#endif
	RemoveNonExistingFiles(m_keyboard_setup_list);


    // then scan the user directory

	OpString wildcard; ANCHOR(OpString, wildcard);
	wildcard.SetL(UNI_L("*.ini"));
	OpString directory; ANCHOR(OpString, directory);

	OpString mousesetupdirectory;
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MOUSESETUP_FOLDER, mousesetupdirectory));
	directory.SetL(mousesetupdirectory);
	ScanFolderL(wildcard, directory, OPMOUSE_SETUP); // FIXME: MAP

	OpString keyboardsetupdirectory;
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_KEYBOARDSETUP_FOLDER, keyboardsetupdirectory));
	directory.SetL(keyboardsetupdirectory);
	ScanFolderL(wildcard, directory, OPKEYBOARD_SETUP);

	OpString toolbarsetupdirectory;
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TOOLBARSETUP_FOLDER, toolbarsetupdirectory));
	directory.SetL(toolbarsetupdirectory);
	ScanFolderL(wildcard, directory, OPTOOLBAR_SETUP);

	OpString menusetupdirectory;
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MENUSETUP_FOLDER, menusetupdirectory));
	directory.SetL(menusetupdirectory);
	ScanFolderL(wildcard, directory, OPMENU_SETUP);

	// We need at least one working file of each type
	if (m_toolbar_setup_list.GetCount() == 0 || m_menu_setup_list.GetCount() == 0 || m_mouse_setup_list.GetCount() == 0 || m_keyboard_setup_list.GetCount() == 0)
		LEAVE(OpStatus::ERR);

}

/***********************************************************************************
**
**  CheckFolders
**
***********************************************************************************/

void OpSetupManager::RemoveNonExistingFiles(OpVector<OpString>& list)
{
	for (UINT32 i=0; i < list.GetCount();)
	{
		OpFile file;
		file.Construct(*list.Get(i));
		BOOL exists;
		if (OpStatus::IsError(file.Exists(exists)) || !exists)
			list.Delete(i);
		else
			i++;
	}
}


/***********************************************************************************
**
**  IsEditable
**
***********************************************************************************/

bool OpSetupManager::IsEditable(UINT32 index, OpSetupType type)
{
	OpVector<OpString>* list = NULL;

	switch (type)
	{
		case OPTOOLBAR_SETUP:
			list = &m_toolbar_setup_list;
		break;

		case OPMENU_SETUP:
			list = &m_menu_setup_list;
		break;

		case OPMOUSE_SETUP:
			list = &m_mouse_setup_list;
		break;

		case OPKEYBOARD_SETUP:
			list = &m_keyboard_setup_list;
		break;
	}

	if (list && index < list->GetCount())
	{
		OpFile file;
		file.Construct(*list->Get(index));

		BOOL exists;
		if (OpStatus::IsSuccess(file.Exists(exists)) && exists)
		{
			OpString defaultsfolder;
			g_folder_manager->GetFolderPath(OPFILE_UI_INI_FOLDER, defaultsfolder);
			OpStringC name = file.GetFullPath();
			if (name.Find(defaultsfolder) != 0)
				return true;
		}
	}

	return false;
}


/***********************************************************************************
**
**	ScanFolderL
**
***********************************************************************************/

void OpSetupManager::ScanFolderL( const OpString& searchtemplate, const OpString& rootfolder, enum OpSetupType type)
{
#ifdef DIRECTORY_SEARCH_SUPPORT
	if( rootfolder.Length() > 0 && searchtemplate.Length() > 0 )
	{
		OpFolderLister *dirlist = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, searchtemplate.CStr(), rootfolder.CStr());
		if (!dirlist)
			return;

		while(dirlist->Next())
		{
			const uni_char *candidate = dirlist->GetFullPath();
			if( candidate )
			{
				OpString* filename = OP_NEW_L(OpString, ());
				filename->Set( candidate );
				switch(type)
				{
				case OPTOOLBAR_SETUP:
					m_toolbar_setup_list.Add(filename);
					break;
				case OPMENU_SETUP:
					m_menu_setup_list.Add(filename);
					break;
				case OPMOUSE_SETUP:
					m_mouse_setup_list.Add(filename);
					break;
				case OPKEYBOARD_SETUP:
					m_keyboard_setup_list.Add(filename);
					break;
				}
			}
		}
		OP_DELETE(dirlist);
	}
#endif // DIRECTORY_SEARCH_SUPPORT
}

/***********************************************************************************
**
**	GetTypeCount
**
***********************************************************************************/
UINT32 OpSetupManager::GetTypeCount(enum OpSetupType type)
{
	switch(type)
	{
	case OPTOOLBAR_SETUP:
		return m_toolbar_setup_list.GetCount();
	case OPMENU_SETUP:
		return m_menu_setup_list.GetCount();
	case OPMOUSE_SETUP:
		return m_mouse_setup_list.GetCount();
	case OPKEYBOARD_SETUP:
		return m_keyboard_setup_list.GetCount();
	}
	return 0;
}

/***********************************************************************************
**
**	SelectSetupFile
**
***********************************************************************************/

OP_STATUS OpSetupManager::SelectSetupFile(INT32 index, enum OpSetupType type, BOOL broadcast_change)
{
	PrefsFile* setupfile;
	TRAPD(rc, setupfile = GetSetupPrefsFileL(index, type));
	if (OpStatus::IsSuccess(rc))
	{
		rc = SelectSetupByFile(setupfile, type, broadcast_change);
	}
	return rc;
}

/***********************************************************************************
**
**	SelectSetupByFile
**
***********************************************************************************/

OP_STATUS OpSetupManager::SelectSetupByFile(PrefsFile* setupfile, enum OpSetupType type, BOOL broadcast_change)
{
	INT32 setupfile_version = 0;

	if (setupfile)
	{
		setupfile_version = setupfile->ReadIntL(UNI_L("Version"),UNI_L("File Version"), -1);
	}

	// if the file didn't have a file version entry we 'assume' it is a empty fallback file
	// else check check if the file is a compatible file
	if(setupfile_version != -1 && setupfile_version < GetCurrentSetupRevision(type))
	{
		if (g_application)
		{
			SetupVersionError* versiondialog = OP_NEW(SetupVersionError, ());
			versiondialog->Init(g_application->GetActiveDesktopWindow());
		}
		return OpStatus::ERR;
	}
	switch(type)
	{
	case OPSKIN_SETUP:
		{
			OP_ASSERT(FALSE);			// this is not possible, use g_skin_manager instead
		}
		break;
	case OPTOOLBAR_SETUP:
		{
			OP_DELETE(m_toolbar_prefsfile);
			m_toolbar_prefsfile = setupfile;

			TRAPD(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::ToolbarConfig, (OpFile*)setupfile->GetFile()));
		}
		break;
	case OPMENU_SETUP:
		{
			OP_DELETE(m_menu_prefsfile);
			m_menu_prefsfile = setupfile;
			TRAPD(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::MenuConfig, (OpFile*)setupfile->GetFile()));
		}
		break;
	case OPMOUSE_SETUP:
		{
			OP_DELETE(m_mouse_prefsfile);
			m_mouse_prefsfile = setupfile;
			TRAPD(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::MouseConfig, (OpFile*)setupfile->GetFile()));
		}
		break;
	case OPKEYBOARD_SETUP:
		{
			OP_DELETE(m_keyboard_prefsfile);
			m_keyboard_prefsfile = setupfile;
			TRAPD(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::KeyboardConfig, (OpFile*)setupfile->GetFile()));
		}
		break;
	}

	if(broadcast_change)
	{
		BroadcastChange(type);
	}
	TRAPD(err, g_prefsManager->CommitL());

	return OpStatus::OK;
}

/***********************************************************************************
**
**	SelectSetupByFile
**
***********************************************************************************/

OP_STATUS OpSetupManager::SelectSetupByFile(const uni_char* filename, enum OpSetupType type, BOOL broadcast_change)
{
	OpAutoPtr<OpFile> file(OP_NEW(OpFile, ()));
	RETURN_OOM_IF_NULL(file.get());
	PrefsFile* pfile = OP_NEW(PrefsFile, (PREFS_STD));
	RETURN_OOM_IF_NULL(pfile);
	TRAPD(err, pfile->ConstructL());
	TRAP(err, pfile->SetFileL(file.get()));
	TRAP(err, pfile->LoadAllL());
	return SelectSetupByFile(pfile, type, broadcast_change);
}

/***********************************************************************************
**
**	ReloadSetupFile
**
***********************************************************************************/

OP_STATUS OpSetupManager::ReloadSetupFile(enum OpSetupType type)
{
	PrefsFile* file = GetSetupFile(type);
	if(file)
	{
		file->Flush();
		TRAPD(err, file->LoadAllL());
		g_input_manager->Flush();
		return OpStatus::OK;
	}

	return OpStatus::ERR_NULL_POINTER;
}

/***********************************************************************************
**
**	GetSetupName
**
***********************************************************************************/

OP_STATUS OpSetupManager::GetSetupName(OpString* filename, INT32 index, enum OpSetupType type)
{
	OpString actualfilename;

	PrefsFile* prefsfile;
	TRAPD(ret, prefsfile = GetSetupPrefsFileL(index, type, &actualfilename, FALSE));
	RETURN_IF_ERROR(ret);

	OpString name;
	TRAPD(ignore_ret, prefsfile->ReadStringL(UNI_L("INFO"),UNI_L("NAME"), name));

	OP_DELETE(prefsfile);

	if (name.HasContent())
	{
		ret = filename->Set(name);
	}
	else
	{
		int patseppos = actualfilename.FindLastOf(PATHSEPCHAR);
		if (KNotFound != patseppos)		//we got a pathseparator, remove it
			ret = actualfilename.Set(actualfilename.SubString(patseppos+1));
		if (OpStatus::IsSuccess(ret))
		{
			int dot = actualfilename.FindLastOf('.');
			if (KNotFound != dot)			//we got a dot, terminate at it
				ret = filename->Set(actualfilename.CStr(), dot);
			else
				ret = filename->Set(actualfilename);
		}
	}

	return ret;
}


/***********************************************************************************
**
**	GetIndexOfMouseSetup
**
***********************************************************************************/

INT32 OpSetupManager::GetIndexOfMouseSetup()
{
	UINT no = GetMouseConfigurationCount();
	OpString currentmousefile; ANCHOR(OpString, currentmousefile);
	currentmousefile.Set(((OpFile*)m_mouse_prefsfile->GetFile())->GetFullPath()); // FIXME: MAP
	while(no)
	{
		if(m_mouse_setup_list.Get(--no)->CompareI(currentmousefile) == 0)
		{
			return no;
		}
	}
	return -1;
}

/***********************************************************************************
**
**	GetIndexOfKeyboardSetup
**
***********************************************************************************/

INT32 OpSetupManager::GetIndexOfKeyboardSetup()
{
	UINT no = GetKeyboardConfigurationCount();
	OpString currentkeyboardfile; ANCHOR(OpString, currentkeyboardfile);
	currentkeyboardfile.Set(((OpFile*)m_keyboard_prefsfile->GetFile())->GetFullPath()); // FIXME: MAP
	while(no)
	{
		if(m_keyboard_setup_list.Get(--no)->CompareI(currentkeyboardfile) == 0)
		{
			return no;
		}
	}
	return -1;
}

/***********************************************************************************
**
**	GetIndexOfMenuSetup
**
***********************************************************************************/

INT32 OpSetupManager::GetIndexOfMenuSetup()
{
	UINT no = GetMenuConfigurationCount();
	OpString currentfile; ANCHOR(OpString, currentfile);
	currentfile.Set(((OpFile*)m_menu_prefsfile->GetFile())->GetFullPath()); // FIXME: MAP
	while(no)
	{
		if(m_menu_setup_list.Get(--no)->CompareI(currentfile) == 0)
		{
			return no;
		}
	}
	return -1;
}

/***********************************************************************************
**
**	GetIndexOfToolbarSetup
**
***********************************************************************************/

INT32 OpSetupManager::GetIndexOfToolbarSetup()
{
	UINT no = GetToolbarConfigurationCount();
	OpString currenttoolbarfile; ANCHOR(OpString, currenttoolbarfile);
	currenttoolbarfile.Set(((OpFile*)m_toolbar_prefsfile->GetFile())->GetFullPath()); // FIXME: MAP
	while(no)
	{
		if(m_toolbar_setup_list.Get(--no)->CompareI(currenttoolbarfile) == 0)
		{
			return no;
		}
	}
	return -1;
}

/***********************************************************************************
**
**	DeleteSetup
**
***********************************************************************************/

void OpSetupManager::DeleteSetupL(INT32 index, enum OpSetupType type)
{
	OpString* discard_filename = NULL;
	INT32 currentindex = -1;

	switch(type)
	{
	case OPTOOLBAR_SETUP:
		{
			if(index == 0)
			{
				LEAVE(OpStatus::ERR);
			}
			currentindex = GetIndexOfToolbarSetup();
			discard_filename = m_toolbar_setup_list.Get(index);
		}
		break;
	case OPMENU_SETUP:
		{
			if(index == 0)
			{
				LEAVE(OpStatus::ERR);
			}
			currentindex = GetIndexOfMenuSetup();
			discard_filename = m_menu_setup_list.Get(index);
		}
		break;
	case OPMOUSE_SETUP:
		{
			if(index == 0)
			{
				LEAVE(OpStatus::ERR);
			}
			currentindex = GetIndexOfMouseSetup();
			discard_filename = m_mouse_setup_list.Get(index);
		}
		break;
	case OPKEYBOARD_SETUP:
		{
			if(index == 0)
			{
				LEAVE(OpStatus::ERR);
			}
			currentindex = GetIndexOfKeyboardSetup();
			discard_filename = m_keyboard_setup_list.Get(index);
		}
		break;
	}

	if(currentindex == index)
	{
		INT32 newidx = (index == 0) ? 1 : index - 1;
		LEAVE_IF_ERROR(SelectSetupFile(newidx, type));
	}

	OpFile filetodelete;
	filetodelete.Construct(discard_filename->CStr());
	filetodelete.Delete();

	ScanSetupFoldersL();
}

/***********************************************************************************
**
**	DuplicateSetupL
**
***********************************************************************************/

void OpSetupManager::DuplicateSetupL(INT32 index, enum OpSetupType type, BOOL copy_contents, BOOL index_check, BOOL modified, UINT32 *returnindex)
{
#if defined(SUPPORT_ABSOLUTE_TEXTPATH)	//this should really get into the next generation of OpFile
	PrefsFile* duplicated = GetSetupPrefsFileL(index, type, NULL, FALSE);
	ANCHOR_PTR(PrefsFile, duplicated);

	OpFile* source = (OpFile*)duplicated->GetFile();

	OpString only_filename;
	ANCHOR(OpString, only_filename);

	OpString only_extension;
	ANCHOR(OpString, only_extension);

	OpString* duplicate_filename = OP_NEW(OpString, ());
	LEAVE_IF_NULL(duplicate_filename);

	duplicate_filename->ReserveL(_MAX_PATH);
	uni_strncpy(duplicate_filename->DataPtr(), source->GetName(), _MAX_PATH-1);

	int pos = duplicate_filename->FindLastOf('.');
	if(pos != KNotFound)
	{
		only_filename.SetL(duplicate_filename->CStr(), pos);
		only_extension.SetL(duplicate_filename->SubString(pos+1));
	}
	else
	{
		only_filename.SetL(*duplicate_filename);
	}

	BOOL exist = FALSE;
	LEAVE_IF_ERROR(source->Exists(exist));

	if (exist)
	{
		OpString only_path;
		ANCHOR(OpString, only_path);

		switch(type)
		{
		case OPMOUSE_SETUP:
			{
				LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MOUSESETUP_FOLDER, only_path));
			}
			break;
		case OPKEYBOARD_SETUP:
			{
				LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_KEYBOARDSETUP_FOLDER, only_path));
			}
			break;
		case OPTOOLBAR_SETUP:
			{
				LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TOOLBARSETUP_FOLDER, only_path));
			}
			break;
		case OPMENU_SETUP:
			{
				LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MENUSETUP_FOLDER, only_path));
			}
			break;
		}

		LEAVE_IF_ERROR(CreateUniqueFilename(*duplicate_filename, only_path, only_filename, only_extension, TRUE));
	}

	OpFile destination;
	ANCHOR(OpFile, destination);

	if(copy_contents)
	{
		destination.Construct(duplicate_filename->CStr());
		LEAVE_IF_ERROR(destination.CopyContents(source, TRUE));
	}
	else
	{
		LEAVE_IF_ERROR(destination.Construct(duplicate_filename->CStr()));
		LEAVE_IF_ERROR(destination.Open(OPFILE_WRITE | OPFILE_TEXT));
		destination.Close();
	}

	ANCHOR_PTR_RELEASE(duplicated);
	OP_DELETE(duplicated);
#endif //(SUPPORT_ABSOLUTE_TEXTPATH)

	switch(type)
	{
	case OPTOOLBAR_SETUP:
		{
			m_toolbar_setup_list.Add(duplicate_filename);
			if(returnindex)
			{
				*returnindex=m_toolbar_setup_list.GetCount()-1;
			}
		}
		break;
	case OPMENU_SETUP:
		{
			m_menu_setup_list.Add(duplicate_filename);
			if(returnindex)
			{
				*returnindex=m_menu_setup_list.GetCount()-1;
			}
		}
		break;
	case OPMOUSE_SETUP:
		{
			m_mouse_setup_list.Add(duplicate_filename);
			if(returnindex)
			{
				*returnindex=m_mouse_setup_list.GetCount()-1;
			}
		}
		break;
	case OPKEYBOARD_SETUP:
		{
			m_keyboard_setup_list.Add(duplicate_filename);
			if(returnindex)
			{
				*returnindex=m_keyboard_setup_list.GetCount()-1;
			}
		}
		break;
	}

	OpString copyofstr;
	ANCHOR(OpString, copyofstr);

	OpString name;
	ANCHOR(OpString, name);

	LEAVE_IF_ERROR(GetSetupName(&name, index, type));

	if(modified)
	{
		OpString modified_str;
		copyofstr.SetL(name);
		g_languageManager->GetStringL(Str::S_MODIFIED_SETUPFILE, modified_str);
		copyofstr.AppendL(UNI_L(" "));
		copyofstr.AppendL(modified_str);
	}
	else
	{
		g_languageManager->GetStringL(Str::S_COPY_OF_SETUPFILE, copyofstr);
		copyofstr.AppendL(UNI_L(" "));
		copyofstr.AppendL(name);
	}

	RenameSetupPrefsFileL(copyofstr.CStr(), GetTypeCount(type)-1, type);	//the duplicate is always the last
}

/***********************************************************************************
**
**	GetSetupPrefsFileL
**
***********************************************************************************/

PrefsFile* OpSetupManager::GetSetupPrefsFileL(volatile INT32 index, enum OpSetupType type, OpString* actualfilename, BOOL index_check)
{
	// before we load, make sure the current setup of that type is commited

	PrefsFile* current = GetSetupFile(type);

	if (current)
	{
		current->CommitL(FALSE, FALSE);
	}

#ifndef PREFS_NO_WRITE
	// if the index is one of the locked indexes, take a copy of the file and return this.
	if (index_check)
	{
		BOOL locked_index = index == 0;

		// We provide more than one keyboard file that is locked
		if (!locked_index && type == OPKEYBOARD_SETUP)
		{
			OpString* name = m_keyboard_setup_list.Get(index);
			if (name)
			{
				OpString defaultsfolder;
				g_folder_manager->GetFolderPath(OPFILE_UI_INI_FOLDER, defaultsfolder);
				locked_index = name->Find(defaultsfolder) == 0;
			}
		}

		if (locked_index)
		{
			// Copy content to new file if we do not modify the first file. A missing
			// section will cause a fallback to the first file.
			volatile BOOL copy_contents = type == OPKEYBOARD_SETUP && index > 0;
			UINT32 newindex;
			DuplicateSetupL(index, type, copy_contents, FALSE, TRUE, &newindex);
			SelectSetupFile(newindex, type, FALSE);
			index = newindex;
		}
	}
#endif // PREFS_NO_WRITE

	OpString* filename = NULL;
	switch(type)
	{
	case OPTOOLBAR_SETUP:
		{
			filename = m_toolbar_setup_list.Get(index);
		}
		break;
	case OPMENU_SETUP:
		{
			filename = m_menu_setup_list.Get(index);
		}
		break;
	case OPMOUSE_SETUP:
		{
			filename = m_mouse_setup_list.Get(index);
		}
		break;
	case OPKEYBOARD_SETUP:
		{
			filename = m_keyboard_setup_list.Get(index);
		}
		break;
	}

	if(filename == NULL)
	{
		LEAVE(OpStatus::ERR_NULL_POINTER);
	}

	if(actualfilename)
	{
		actualfilename->SetL(*filename);
	}

	OpFile file;
	LEAVE_IF_ERROR(file.Construct(filename->CStr()));


	PrefsFile* prefsfile = OP_NEW(PrefsFile, (PREFS_STD));
	ANCHOR_PTR(PrefsFile, prefsfile);

	prefsfile->ConstructL();
	prefsfile->SetFileL(&file);
	prefsfile->LoadAllL();

	ANCHOR_PTR_RELEASE(prefsfile);
	return prefsfile;
}

/***********************************************************************************
**
**	RenameSetupPrefsFileL
**
***********************************************************************************/

void OpSetupManager::RenameSetupPrefsFileL(const uni_char* newname, INT32 index, enum OpSetupType type)
{
	PrefsFile* prefsfile = GetSetupPrefsFileL(index, type);

	OpString name;
	prefsfile->WriteStringL(UNI_L("INFO"),UNI_L("NAME"), newname);
	prefsfile->CommitL();

	OP_DELETE(prefsfile);
}

/***********************************************************************************
**
**	GetStringL
**
***********************************************************************************/
void OpSetupManager::GetStringL(OpString &string, const OpStringC8 &sectionname, const OpStringC &key, enum OpSetupType type, BOOL master)
{
	PrefsSection* section = GetSectionL(sectionname, type, NULL, master);
	if(section)
	{
		OP_STATUS err = string.Set(section->Get(key));
		OP_DELETE(section);
		LEAVE_IF_ERROR(err);
	}
}

/***********************************************************************************
**
**	GetIntL
**
***********************************************************************************/

int OpSetupManager::GetIntL(const OpStringC8 &sectionname, const OpStringC &key, enum OpSetupType type, int defval, BOOL master)
{
	PrefsSection* section = GetSectionL(sectionname, type, NULL, master);
	int result_int = defval;
	if(section)
	{
		const uni_char *result;
		result = section->Get(key);
		if(result)			// we could try to get it from the default file, if section is in the "user-file"
		{
			result_int = uni_strtol(result, NULL, 0);
		}
		OP_DELETE(section);
	}
	return result_int;
}

/***********************************************************************************
**
**	GetIntL
**
***********************************************************************************/

int OpSetupManager::GetIntL(PrefsSection* section, const OpStringC8 &key, int defval)
{
	if(section)
	{
		const uni_char *result;
		OpString key_uni;
		key_uni.Set(key);
		result = section->Get(key_uni);

		if(result)			// we could try to get it from the default file, if section is in the "user-file"
		{
			return uni_strtol(result, NULL, 0);
		}
	}
	return defval;
}

/***********************************************************************************
**
**	SetKeyL
**
***********************************************************************************/
OP_STATUS OpSetupManager::SetKeyL(const OpStringC8 &section, const OpStringC8 &key, int value, enum OpSetupType type)
{
	PrefsFile* file = GetSetupFile(type);

	if(file)
	{
		return file->WriteIntL(section, key, value);
	}

	return OpStatus::ERR;
}
/***********************************************************************************
**
**	SetKeyL
**
***********************************************************************************/
OP_STATUS OpSetupManager::SetKeyL(const OpStringC8 &section, const OpStringC8 &key, const OpStringC &value, enum OpSetupType type)
{
	PrefsFile* file = GetSetupFile(type);

	if(file)
	{
		return file->WriteStringL(section, key, value);
	}
	return OpStatus::ERR;
}

/***********************************************************************************
**
**	GetSectionL
**
***********************************************************************************/

PrefsSection* OpSetupManager::GetSectionL(const char* name, enum OpSetupType type, BOOL* was_default, BOOL master)
{
	PrefsSection* section = NULL;
	switch(type)
	{
	case OPDIALOG_SETUP:
		{
			if(!m_override_dialog_prefsfile->IsSection(name) || master)
			{
				if(!m_custom_dialog_prefsfile->IsSection(name) || master)
				{
					section = m_dialog_prefsfile->ReadSectionL(name);
					if(was_default)
					{
						*was_default = TRUE;
					}
				}
				else
				{
					section = m_custom_dialog_prefsfile->ReadSectionL(name);
				}
			}
			else
			{
				section = m_override_dialog_prefsfile->ReadSectionL(name);
			}
		}
		break;
	case OPTOOLBAR_SETUP:
		{
			// TODO: Add platform specific sections that override default
			if(!m_toolbar_prefsfile->IsSection(name) || master)
			{
				if(!m_custom_toolbar_prefsfile->IsSection(name) || master)
				{
					section = m_default_toolbar_prefsfile->ReadSectionL(name);
					if(was_default)
					{
						*was_default = TRUE;
					}
				}
				else
				{
					section = m_custom_toolbar_prefsfile->ReadSectionL(name);
				}
			}
			else
			{
				section = m_toolbar_prefsfile->ReadSectionL(name);
			}
		}
		break;
	case OPMENU_SETUP:
		{
			if(!m_menu_prefsfile->IsSection(name) || master)
			{
				if(!m_custom_menu_prefsfile->IsSection(name) || master)
				{
					section = m_default_menu_prefsfile->ReadSectionL(name);
					if(was_default)
					{
						*was_default = TRUE;
					}
				}
				else
				{
					section = m_custom_menu_prefsfile->ReadSectionL(name);
				}
			}
			else
			{
				section = m_menu_prefsfile->ReadSectionL(name);
			}
		}
		break;
	case OPMOUSE_SETUP:
		{
			if(!m_mouse_prefsfile->IsSection(name) || master)
			{
				section = m_default_mouse_prefsfile->ReadSectionL(name);
				if(was_default)
				{
					*was_default = TRUE;
				}
			}
			else
			{
				section = m_mouse_prefsfile->ReadSectionL(name);
			}
		}
		break;
	case OPKEYBOARD_SETUP:
		{
			if(!m_keyboard_prefsfile->IsSection(name) || master)
			{
				section = m_default_keyboard_prefsfile->ReadSectionL(name);
				if(was_default)
				{
					*was_default = TRUE;
				}
			}
			else
			{
				section = m_keyboard_prefsfile->ReadSectionL(name);
			}
		}
		break;
	}
	return section;
}

/***********************************************************************************
**
**	DeleteSectionL
**
***********************************************************************************/
BOOL OpSetupManager::DeleteSectionL(const char* name, enum OpSetupType type)
{
	PrefsFile* file = GetSetupFile(type);
	if(file)
	{
		BOOL ret = file->DeleteSectionL(name);
		file->CommitL(FALSE, FALSE);
		return ret;
	}
	return OpStatus::ERR;
}

PrefsFile* OpSetupManager::GetSetupFile(OpSetupType type, BOOL check_readonly, BOOL copy_contents)
{
	switch(type)
	{
	case OPTOOLBAR_SETUP:
		{
			if(check_readonly && GetIndexOfToolbarSetup() == 0)	 // if the file is one of the read-onlys, copy to the user directory
			{
				UINT32 newindex;
				TRAPD(err,DuplicateSetupL(0, type, copy_contents, TRUE, TRUE, &newindex));
				SelectSetupFile(newindex, type, FALSE);
			}
			return m_toolbar_prefsfile;
		}
	case OPMENU_SETUP:
		{
			if(check_readonly && GetIndexOfMenuSetup() == 0)	 // if the file is one of the read-onlys, copy to the user directory
			{
				UINT32 newindex;
				TRAPD(err,DuplicateSetupL(0, type, copy_contents, TRUE, TRUE, &newindex));
				SelectSetupFile(newindex, type, FALSE);
			}
			return m_menu_prefsfile;
		}
	case OPMOUSE_SETUP:
		{
			if(check_readonly && GetIndexOfMouseSetup() == 0)	 // if the file is one of the read-onlys, copy to the user directory
			{
				UINT32 newindex;
				TRAPD(err,DuplicateSetupL(0, type, copy_contents, TRUE, TRUE, &newindex));
				SelectSetupFile(newindex, type, FALSE);
			}
			return m_mouse_prefsfile;
		}
	case OPKEYBOARD_SETUP:
		{
			if(check_readonly && GetIndexOfKeyboardSetup() == 0)	 // if the file is one of the read-onlys, copy to the user directory
			{
				UINT32 newindex;
				TRAPD(err,DuplicateSetupL(0, type, copy_contents, TRUE, TRUE, &newindex));
				SelectSetupFile(newindex, type, FALSE);
			}
			return m_keyboard_prefsfile;
		}
	default:
		{
			OP_ASSERT(FALSE);	//something is seriously wrong!
		}
	}
	return NULL;
}

const PrefsFile* OpSetupManager::GetDefaultSetupFile(OpSetupType type)
{
	switch(type)
	{
	case OPTOOLBAR_SETUP:
		{
			return m_default_toolbar_prefsfile;
		}
	case OPMENU_SETUP:
		{
			return m_default_menu_prefsfile;
		}
	case OPMOUSE_SETUP:
		{
			return m_default_mouse_prefsfile;
		}
	case OPKEYBOARD_SETUP:
		{
			return m_default_keyboard_prefsfile;
		}
	default:
		{
			OP_ASSERT(FALSE);	//something is seriously wrong!
		}
	}
	return NULL;
}

/***********************************************************************************
**
**	MergeSectionIntoExisting
**
***********************************************************************************/

OP_STATUS OpSetupManager::MergeSetupIntoExisting(PrefsFile* file, enum OpSetupType type, SetupPatchMode patchtype, OpFile*& backupfile)
{
//	const uni_char* fixedsections[] = { UNI_L("info"), UNI_L("version"), NULL};

	PrefsFile* currentfile = NULL;

	currentfile = GetSetupFile(type, TRUE, FALSE);
	//we need copy the previous setup to a temp-file if the user wants to revert to old settings...

	OP_ASSERT(FALSE);	//we need tempfile here!
	return OpStatus::ERR;
}

void OpSetupManager::BroadcastChange(OpSetupType type)
{
	switch(type)
	{
	case OPSKIN_SETUP:
		{
			g_application->SettingsChanged(SETTINGS_SKIN);
		}
		break;
	case OPTOOLBAR_SETUP:
		{
			g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);
		}
		break;
	case OPMENU_SETUP:
		{
			g_application->SettingsChanged(SETTINGS_MENU_SETUP);
		}
		break;
	case OPMOUSE_SETUP:
		{
			g_application->SettingsChanged(SETTINGS_MOUSE_SETUP);
		}
		break;
	case OPKEYBOARD_SETUP:
		{
			g_application->SettingsChanged(SETTINGS_KEYBOARD_SETUP);
		}
		break;
	}
}

BOOL OpSetupManager::IsFallback(const uni_char* fullpath, OpSetupType type)
{

	const PrefsFile* currentfile = GetDefaultSetupFile(type);


	const uni_char* currentsetup = ((OpFile*)currentfile->GetFile())->GetFullPath();

	if(!currentsetup)
	{
		return FALSE;
	}

	return (uni_stricmp(currentsetup, fullpath) == 0);

}

