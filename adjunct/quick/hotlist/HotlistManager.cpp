/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#include "core/pch.h"

#include "adjunct/quick/Application.h"

#include "adjunct/quick/dialogs/BookmarkDialog.h"
#include "adjunct/quick/dialogs/ContactPropertiesDialog.h"
#include "adjunct/quick/dialogs/GroupPropertiesDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/controller/AddToPanelController.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/hotlist/hotlistparser.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/models/DesktopBookmark.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"


#include "modules/bookmarks/bookmark_manager.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryItem.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpKeys.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/regexp/include/regexp_api.h"
#include "modules/url/url2.h"
#include "modules/util/filefun.h"
#include "modules/util/opfile/opsafefile.h"

#if defined(_MACINTOSH_)
# include "platforms/mac/util/macutils.h"
#endif

#if defined(MSWIN)
# include "platforms/windows/win_handy.h"
#endif

#if defined(_UNIX_DESKTOP_)
# include "platforms/posix/posix_file_util.h"
# include "platforms/posix/posix_native_util.h"
# include "platforms/unix/base/common/unixutils.h"
#endif

#ifdef M2_SUPPORT
# include "adjunct/m2/src/engine/header.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/index.h"
#endif // M2_SUPPORT


void ReplaceIllegalSinglelineText( OpString& text )
{
	INT32 length = text.Length();
	for( INT32 i=0; i<length; i++ )
	{
		if( text.CStr()[i] == '\n' )
			text.CStr()[i] = ' ';
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////

HotlistManager::HotlistManager() :
	m_request(NULL),
	m_chooser(0),
	m_bookmarks_modeldata(this, HotlistModel::OperaBookmark),
	m_contacts_modeldata(this, HotlistModel::ContactRoot, HotlistModel::OperaContact),
	m_notes_modeldata(this, HotlistModel::NoteRoot, HotlistModel::OperaNote),
	m_services_modeldata(this, HotlistModel::UniteServicesRoot, HotlistModel::OperaGadget),
	m_clipboard(1, TRUE)
{
}


HotlistManager::~HotlistManager()
{
	for (int i = HotlistModel::BookmarkRoot + 1; i <= HotlistModel::UniteServicesRoot; i++)
	{
		SaveModel(i);
	}

#ifdef WIC_USE_ANCHOR_SPECIAL
	OpStatus::Ignore(g_application->GetAnchorSpecialManager().RemoveHandler(this));
#endif // WIC_USE_ANCHOR_SPECIAL

	OP_DELETE(m_chooser);
	OP_DELETE(m_request);
}

HotlistManager::ModelData& HotlistManager::GetModelData(int type)
{
	OP_ASSERT(type >= HotlistModel::BookmarkRoot && type <= HotlistModel::UniteServicesRoot);

	if (type == HotlistModel::BookmarkRoot)
	{
		return m_bookmarks_modeldata;
	}
	else if (type == HotlistModel::ContactRoot)
	{
		return m_contacts_modeldata;
	}
	else if (type == HotlistModel::NoteRoot)
	{
		return m_notes_modeldata;
	}
	else if (type == HotlistModel::UniteServicesRoot)
	{
		return m_services_modeldata;
	}

	OP_ASSERT(FALSE); // should not happen
	return m_bookmarks_modeldata;
}


HotlistManager::ModelData* HotlistManager::GetModelData( OpTreeModel& target )
{
	int match = -1;

	for (int i = HotlistModel::BookmarkRoot; i <= HotlistModel::UniteServicesRoot; i++)
	{
		if (&GetHotlistModel(i) == &target)
		{
			// Section of weirdness: The original and now disabled code below causes a pointer
			// that is four bytes lower in memory than it should be to be returned using
			// SuSE 9.3 and gcc 3.3.5 [espen 2005-11-02]
			//
			// return &GetModelData(i);

			match = i;
			break;
		}
	}

	return match == -1 ? 0 : &GetModelData(match);
}


int HotlistManager::GetTypeFromModel(HotlistModel& model)
{
	for (int i = HotlistModel::BookmarkRoot; i <= HotlistModel::UniteServicesRoot; i++)
	{
		if (&GetHotlistModel(i) == &model)
		{
			return i;
		}
	}

	OP_ASSERT(FALSE); // should not happen
	return 0;
}

int HotlistManager::GetTypeFromId(int id)
{
	if (id >= HotlistModel::BookmarkRoot && id <= HotlistModel::UniteServicesRoot)
		return id;

	OP_ASSERT(GetItemByID(id));

	return GetTypeFromModel(*GetItemByID(id)->GetModel());
}

OP_STATUS HotlistManager::Init()
{
	HotlistParser parser;

	OpString filename;

	int max = HotlistModel::UniteServicesRoot;
#ifdef EMBROWSER_SUPPORT
	extern long gVendorDataID;
	if (gVendorDataID != 'OPRA')
		max = HotlistModel::BookmarkRoot;
#endif

	for (int i = HotlistModel::BookmarkRoot + 1; i <= max; i++)
	{
		GetFilenames( i, GetFilename(i), filename );
		parser.Parse( filename, (HotlistGenericModel&)GetHotlistModel(i), -1, GetDataFormat(i), FALSE );
		GetHotlistModel(i).AddActiveFolder();
		GetHotlistModel(i).GetTrashFolder();

	}

/*
	// Debug code for testing hashtable, remove once it works properly. adamm
	for (int i = 0; i < GetBookmarksModel()->GetItemCount(); i++)
	{
		HotlistModelItem *hotlist_item = GetBookmarksModel()->GetItemByIndex(i);

		printf("GUID, Full List: %s %s\n", hotlist_item->GetUniqueGUID().UTF8AllocL(), hotlist_item->GetName().UTF8AllocL());
	}

	OpHashIterator* it = GetBookmarksModel()->m_unique_guids.GetIterator();
	OP_STATUS ret_val = it->First();
	while (OpStatus::IsSuccess(ret_val))
	{
		uni_char * key = (uni_char *) it->GetKey();
		OpString8 tmp;
		tmp.Set(key);
				HotlistModelItem* hotlist_item = (HotlistModelItem* ) it->GetData();

		printf("GUID, Full List: %s %s Key: %s\n", hotlist_item->GetUniqueGUID().UTF8AllocL(), hotlist_item->GetName().UTF8AllocL(), tmp.CStr());

		HotlistModelItem* hotlist_item2 = 0;
		GetBookmarksModel()->m_unique_guids.GetData(key, &hotlist_item2);

		if (hotlist_item2 != hotlist_item)
			printf("Could not look up item!!!!\n");

		ret_val = it->Next();
	}
*/

#ifdef WIC_USE_ANCHOR_SPECIAL
	RETURN_IF_ERROR(g_application->GetAnchorSpecialManager().AddHandler(this));
#endif // WIC_USE_ANCHOR_SPECIAL

	return OpStatus::OK;
}

#ifdef WIC_USE_ANCHOR_SPECIAL
/*virtual*/ BOOL
HotlistManager::HandlesAnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info)
{
	const uni_char * rel_str = info.GetRel();
	if (rel_str && uni_stri_eq(rel_str, "SIDEBAR"))
	{
		return TRUE;
	}
	return FALSE;
}

/*virtual*/ BOOL 
HotlistManager::AnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info)
{
	const uni_char * rel_str = info.GetRel();

	OP_ASSERT(rel_str && uni_stri_eq(rel_str, "SIDEBAR"));
	if (rel_str && uni_stri_eq(rel_str, "SIDEBAR"))
	{
		bool show_dialog;
		OpAutoPtr<AddToPanelController> controller(OP_NEW(AddToPanelController, ()));
		RETURN_VALUE_IF_NULL(controller.get(), FALSE);
		RETURN_VALUE_IF_ERROR(controller->InitPanel(info.GetTitle(), info.GetURLNameRel(), show_dialog), FALSE);
		if (show_dialog && g_desktop_bookmark_manager->GetBookmarkModel()->Loaded())
		{
			DocumentDesktopWindow* active_wnd = g_application->GetActiveDocumentDesktopWindow();
			RETURN_VALUE_IF_ERROR(ShowDialog(controller.release(), g_global_ui_context, active_wnd), FALSE);
		}
		return TRUE;
	}
	return FALSE;
}

#endif // WIC_USE_ANCHOR_SPECIAL

BOOL HotlistManager::ImportCustomFileOnce()
{
	//import the custom.adr file in the Opera-catalog.

	OP_MEMORY_VAR BOOL returnvalue = FALSE;

	const OpStringC fCustomName = g_pcui->GetStringPref(PrefsCollectionUI::CustomBookmarkImportFilename);

	INT32 custombookmarksmerged = g_pcui->GetIntegerPref(PrefsCollectionUI::ImportedCustomBookmarks);

	if(!fCustomName.IsEmpty() && custombookmarksmerged != 1)
	{
		// added custom merge file

		HotlistModel& model = GetHotlistModel(HotlistModel::BookmarkRoot);
		HotlistParser parser;
		if( parser.Parse( fCustomName, model, -1, HotlistModel::OperaBookmark, TRUE ) )
		{
			model.SetDirty( TRUE );
			returnvalue = TRUE;
		}

		OpFile file;
		file.Construct(fCustomName.CStr());
		file.Delete();
		//reset so it doesn't do it next time we start
		OpString empty;
		TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::CustomBookmarkImportFilename, empty));

		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ImportedCustomBookmarks, 1));

	}

	return returnvalue;
}


void HotlistManager::GetFilenames( int type, OpString &write_name, OpString &read_name )
{
	if( type == HotlistModel::BookmarkRoot )
	{
		OpFile hotlistfile;
		TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::HotListFile, hotlistfile));
		read_name.Set(hotlistfile.GetFullPath());
	}
	else if( type == HotlistModel::ContactRoot )
	{
		OpFile contactlistfile;
		TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::ContactListFile, contactlistfile));
		read_name.Set(contactlistfile.GetFullPath());
	}
	else if( type == HotlistModel::NoteRoot )
	{
		OpFile notelistfile;
		TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::NoteListFile, notelistfile));
		read_name.Set(notelistfile.GetFullPath());
	}
#ifdef WEBSERVER_SUPPORT
	else if( type == HotlistModel::UniteServicesRoot )
	{
		OpString tmp_storage;
		read_name.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_USERPREFS_FOLDER, tmp_storage));
		// AJMTODO: Add preference
		read_name.Append(UNI_L("unite.adr"));
	}
#endif // WEBSERVER_SUPPORT
	else
	{
		return;
	}

	// Always write to the default read_name. Except when we read from a symbolic link. See below
	write_name.Set( read_name );

	// Pick the correct file to read
	OpFile file;
	RETURN_VOID_IF_ERROR(file.Construct(read_name.CStr()));
	BOOL ok = FALSE;
	file.Exists(ok);
	if( !ok )
	{
		// File suggested by preferences does not exist. Check for backup file.
		OpString backupName;
		backupName.Set( read_name );
		backupName.Append( UNI_L(".bak") );
		RETURN_VOID_IF_ERROR(file.Construct(backupName.CStr()));
		ok = FALSE;
		file.Exists(ok);
		if( ok )
		{
			OpFile src, dest;
			RETURN_VOID_IF_ERROR(src.Construct(backupName.CStr()));
			RETURN_VOID_IF_ERROR(dest.Construct(read_name.CStr()));
			OP_STATUS ok = dest.CopyContents(&src, TRUE);
			if (OpStatus::IsSuccess(ok))
			{
				// Backup file exists but we cannot copy it to read_name file.
				// Warn the user.
				// Do not overwrite the backup later on

				GetBackupEnabled(type) = FALSE;
			}
		}
	}


#if defined(_UNIX_DESKTOP_)
	// If the file we are about to use is a symbolic link then we have
	// to open the real file since we may want to write to the file.
	if (read_name.HasContent())
	{
		char realpath[_MAX_PATH];
		if (OpStatus::IsSuccess(PosixFileUtil::RealPath(read_name.CStr(), realpath)) &&
			OpStatus::IsSuccess(PosixNativeUtil::FromNative(realpath, &read_name)))
		{
			write_name.Set(read_name);
		}
	}
#endif
}

/******************************************************************
 *
 * Methods using file chooser
 *
 ******************************************************************/
void HotlistManager::Open( int type )
{
	SaveModel(type);
	SetFileChoosingContext(OPEN_FILE_CHOOSING);
	choosing_data.type = type;
	ChooseFile(  );
}
void HotlistManager::New( int type )
{
	SaveModel(type);
	SetFileChoosingContext(NEW_FILE_CHOOSING);
	choosing_data.type = type;
	ChooseFile( );
}
void HotlistManager::SaveAs( INT32 id, SaveAsMode mode )
{
	SetFileChoosingContext(SAVEAS_FILE_CHOOSING);
	choosing_data.type = GetTypeFromId(id);
	choosing_data.mode = mode;
	ChooseFile();
}
void HotlistManager::SaveSelectedAs( INT32 id, OpINT32Vector& id_list, SaveAsMode mode )
{
	if (id_list.GetCount() == 0)
		return;
	SetFileChoosingContext(SAVESELECTEDAS_FILE_CHOOSING);
	choosing_data.type = GetTypeFromId(id);
	choosing_data.mode = mode;
	OpINT32Vector* vec = OP_NEW(OpINT32Vector, ());
	for (UINT32 i = 0; i<id_list.GetCount(); i++)
	{
		vec->Add(id_list.Get(i));
	}
	choosing_data.id_list = vec; //&id_list;
	ChooseFile();
}
void HotlistManager::Import( INT32 id_arg, int format, const OpStringC& location, BOOL import_into_root_folder, BOOL show_import_info )
{

	OP_MEMORY_VAR INT32 id = id_arg; // may be referenced after TRAP, which might clobber it.

	HotlistModel* OP_MEMORY_VAR model = 0;

	if( 
	   format == HotlistModel::OperaBookmark ||
		format == HotlistModel::NetscapeBookmark ||
		format == HotlistModel::ExplorerBookmark ||
		format == HotlistModel::KDE1Bookmark ||
		format == HotlistModel::KonquerorBookmark
		)
	{
		model = &GetHotlistModel(HotlistModel::BookmarkRoot);
	}
	else if( format == HotlistModel::OperaContact )
	{
		model = &GetHotlistModel(HotlistModel::ContactRoot);
	}

	if (!model)
	{
		return;
	}

	if( !model->GetItemByID(id) )
	{
		id = GetTypeFromModel(*model);
	}

	if (location.HasContent())
	{
		OpString file_name;
		file_name.Set(location);
		ChosenImport(id, format, import_into_root_folder, show_import_info, model, &file_name);
	}
	else
	{
		SetFileChoosingContext(IMPORT_FILE_CHOOSING);
		choosing_data.type = model->GetModelType();
		choosing_data.format = format;
		choosing_data.into_root = import_into_root_folder;
		choosing_data.show_import = show_import_info;
		choosing_data.model = model;

		ChooseFile();
	}
}
/******************************************************************
 *
 * Choose File
 *
 ******************************************************************/
void HotlistManager::ChooseFile( )
{
	if (m_request)
		return;
	
	if (!m_chooser)
		RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

	m_request = OP_NEW(DesktopFileChooserRequest, ());
	if (!m_request)
	{
		return;
	}
	DesktopFileChooserRequest& request = *m_request;
	Str::LocaleString caption = Str::NOT_A_STRING;
	Str::LocaleString extensions = Str::SI_SAVE_FILE_TYPES;
	OpString filename;
	OpString filter;
	filename.Set(GetFilename(choosing_data.type));

	switch (GetFileChoosingContext())
	{
	case OPEN_FILE_CHOOSING:
		if( choosing_data.type == HotlistModel::BookmarkRoot )
			caption = Str::D_SELECT_BOOKMARK_FILE;
		else
			caption = Str::D_SELECT_CONTACT_FILE;
		extensions  = (Str::LocaleString)Str::S_OPERA_BOOKMARKFILE_EXTENSION;
		request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
		break;
	case NEW_FILE_CHOOSING:
		if(choosing_data.type == HotlistModel::BookmarkRoot)
			caption = Str::D_SELECT_NEW_BOOKMARK_FILE;
		else
			caption = Str::D_SELECT_NEW_CONTACT_FILE;
		extensions  = (Str::LocaleString)Str::S_OPERA_BOOKMARKFILE_EXTENSION;
		request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
		break;
	case SAVEAS_FILE_CHOOSING:
		if( choosing_data.mode == SaveAs_Export )
		{
			if( choosing_data.type == HotlistModel::BookmarkRoot )
				caption = Str::D_EXPORT_BOOKMARK_FILE;
			else
				caption = Str::D_EXPORT_CONTACT_FILE;
			extensions  = Str::S_OPERA_BOOKMARKFILE_EXTENSION;
		}
		else if( choosing_data.mode == SaveAs_Html )
		{
			if( choosing_data.type == HotlistModel::BookmarkRoot )
				caption = Str::D_SAVE_BOOKMARK_FILE_AS_HTML;
			else
				caption = Str::D_SAVE_CONTACT_FILE_AS_HTML;
			extensions  = Str::S_HTML_BOOKMARKFILE_EXTENSION;

			int pos = filename.FindLastOf( '.' );
			if( pos != 0 )
			{
				OpString tmp;
				tmp.Set( filename.CStr(), pos );
				filename.TakeOver(tmp);
			}
			filename.Append( UNI_L(".html") );
		}
		request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE;
		break;
	case SAVESELECTEDAS_FILE_CHOOSING:
		if( choosing_data.mode == SaveAs_Export )
		{
			if( choosing_data.type == HotlistModel::BookmarkRoot )
				caption = Str::D_SAVE_SELECTED_BOOKMARKS_AS;
			else
				caption = Str::D_SAVE_SELECTED_CONTACTS_AS;
			extensions  = Str::S_OPERA_BOOKMARKFILE_EXTENSION;
		}
		else if( choosing_data.mode == SaveAs_Html )
		{
			if( choosing_data.type == HotlistModel::BookmarkRoot )
				caption = Str::D_SAVE_SELECTED_BOOKMARKS_AS_HTML;
			else
				caption = Str::D_SAVE_SELECTED_CONTACTS_AS_HTML;
			extensions  = Str::S_HTML_BOOKMARKFILE_EXTENSION;

			int pos = filename.FindLastOf( '.' );
			if( pos != 0 )
			{
				OpString tmp;
				tmp.Set( filename.CStr(),pos );
				filename.TakeOver( tmp );
			}
			filename.Append( UNI_L(".html") );
		}
		request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE;
		break;
	case IMPORT_FILE_CHOOSING:
		if( choosing_data.format == HotlistModel::OperaBookmark )
		{
			caption     = Str::D_IMPORT_OPERA_BOOKMARK_FILE;
			extensions  = (Str::LocaleString)Str::S_OPERA_BOOKMARKFILE_EXTENSION;
		}
//	 	else if( choosing_data.format == HotlistModel::OperaNote)
// 		{
// 			caption     = Str::D_IMPORT_OPERA_NOTE_FILE;
// 			extensions  = Str::S_OPERA_BOOKMARKFILE_EXTENSION;
// 		}
		else if( choosing_data.format == HotlistModel::OperaContact )
		{
			caption     = Str::D_IMPORT_OPERA_CONTACT_FILE;
			extensions  = Str::S_OPERA_BOOKMARKFILE_EXTENSION;
		}
		else if( choosing_data.format == HotlistModel::NetscapeBookmark )
		{
			caption     = Str::D_IMPORT_NETSCAPE_BOOKMARK_FILE;
			extensions  = Str::S_HTML_BOOKMARKFILE_EXTENSION;
			filename.Wipe();
		}
		else if( choosing_data.format == HotlistModel::ExplorerBookmark )
		{
			filename.Wipe();
			caption     = Str::D_IMPORT_EXPLORER_BOOMARK_FOLDER;
		}
		else if( choosing_data.format == HotlistModel::KonquerorBookmark )
		{
			filename.Wipe();
			caption     = Str::D_IMPORT_KONQUEROR_BOOKMARK_FILE;
			extensions  = Str::S_KONQUEROR_BOOKMARKFILE_EXTENSION;
#ifdef _MACINTOSH_
			// Get default import location for the specified type of bookmark file.
			MacGetBookmarkFileLocation(choosing_data.format, filename);
#endif
		}
		else if( choosing_data.format == HotlistModel::KDE1Bookmark )
		{
			filename.Wipe();
			caption     = Str::D_IMPORT_KDE1_BOOMARK_FOLDER;
		}
		if(
#if !defined(_MACINTOSH_) // MacIE uses netscape format
			choosing_data.format == HotlistModel::ExplorerBookmark ||
#endif
			choosing_data.format == HotlistModel::KDE1Bookmark )
		{
#if defined(MSWIN)
			//	Get "Favorites folder" from registry
			OpString favdir;
			if (favdir.Reserve(2048))
			{
				GetFavoritesFolder( favdir.CStr(), favdir.Capacity());
				filename.Set(favdir.CStr());
			}
#endif

			request.action = DesktopFileChooserRequest::ACTION_DIRECTORY;
		}
		else
			request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
		break;
	}
	request.initial_path.Set(filename);
	g_languageManager->GetString(caption,request.caption);
	g_languageManager->GetString(extensions, filter);

	if (filter.HasContent())
		StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
	DesktopWindow* parent = g_application->GetActiveDesktopWindow(FALSE);

	if (OpStatus::IsError(m_chooser->Execute(parent ? parent->GetOpWindow() : 0, this, request)))
	{
		OP_DELETE(m_request);
		m_request = NULL;
	}
}
/******************************************************************
 *
 * Methods after file choosing
 *
 ******************************************************************/
void HotlistManager::ChosenNewOrOpen(int type, 
									 BOOL new_file, 
									 OpString* new_filename)
{

	BOOL sync_chosen_file = TRUE;
#ifdef SUPPORT_DATA_SYNC
	if (g_sync_manager->HasUsedSync() && type == HotlistModel::BookmarkRoot)
	{
		//  Should show dialog to choose between syncing file or not
		OpString message;
		g_languageManager->GetString(Str::D_SYNC_SWITCH_BOOKMARK_FILE, message);
		OpString caption;
		int result = SimpleDialog::ShowDialog(WINDOW_NAME_CHOOSE_SYNC, NULL, message.CStr(), caption.CStr(), Dialog::TYPE_OK_CANCEL, Dialog::IMAGE_QUESTION);
		if (result != Dialog::DIALOG_RESULT_YES)
		{
			sync_chosen_file = FALSE; //
		}
		// cancel switching file at all
		if (result == Dialog::DIALOG_RESULT_CANCEL)
		{
			return;
		}
	}
#endif

	HotlistModel& model = GetHotlistModel(type);

	OpString& model_filename = GetFilename(type);
	model_filename.Set( *new_filename );

	HotlistParser parser;

	// Remove the previous model
	model.Erase();

	// Use new one
	parser.Parse( model_filename, model, -1, GetDataFormat(type), FALSE );
	model.AddActiveFolder();
	model.GetTrashFolder();
	if( new_file )
	{
		model.SetDirty(TRUE);
	}

	if (type == HotlistModel::BookmarkRoot)
	{
		// Re-initialize bookmark storage provider
		((BookmarkModel&)model).InitStorageProvider(model_filename);

		if (sync_chosen_file)
		{
			model.SetModelIsSynced(TRUE);
		}
		else
		{
			model.SetModelIsSynced(FALSE);
		}
	}


	OpFile listfile;
	OP_STATUS rc = listfile.Construct(model_filename.CStr());

	if( type == HotlistModel::BookmarkRoot )
	{
		TRAP(rc, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::HotListFile, &listfile));
		TRAP(rc, g_prefsManager->CommitL());
	}
	else if( type == HotlistModel::ContactRoot )
	{
		TRAP(rc, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::ContactListFile, &listfile));
		TRAP(rc, g_prefsManager->CommitL());
	}
	else if( type == HotlistModel::NoteRoot )
	{
		TRAP(rc, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::NoteListFile, &listfile));
		TRAP(rc, g_prefsManager->CommitL());
	}
	// TODO:: Add for Services file
}

void HotlistManager::ChosenSaveAs(int type, 
								  SaveAsMode mode, 
								  OpString * filename)
{
	HotlistParser parser;

	if( mode == SaveAs_Html )
	{
		OP_ASSERT(type == HotlistModel::BookmarkRoot);
		parser.WriteHTML( *filename, *GetBookmarksModel(), TRUE, FALSE );
	}
	else
	{
		HotlistModel& model = GetHotlistModel(type);
		parser.Write( *filename, model, TRUE, FALSE );
	}

}

void HotlistManager::ChosenSaveSelectedAs(int type, 
										  SaveAsMode mode, 
										  OpINT32Vector* id_list, 
										  OpString * filename)
{
	if (!id_list || id_list->GetCount() == 0)
		return;

	HotlistModel& model = GetHotlistModel(type);
	for(UINT32 i=0; i<id_list->GetCount(); i++ )
	{
		HotlistModelItem *hmi = model.GetItemByID(id_list->Get(i));
		if( hmi )
		{
			hmi->SetIsMarked(TRUE);
		}
	}

	OP_DELETE(id_list);
	id_list = NULL;

	HotlistParser parser;
	if( mode == SaveAs_Html )
	{
		OP_ASSERT(type == HotlistModel::BookmarkRoot);
		parser.WriteHTML( *filename, *GetBookmarksModel(), FALSE, FALSE );
	}
	else
	{
		parser.Write( *filename, model, FALSE, FALSE );
	}
	INT32 y;

	for(y=0; y<model.GetItemCount(); y++ )
	{
		HotlistModelItem *hmi = model.GetItemByIndex(y);
		if( hmi )
		{
			hmi->SetIsMarked(FALSE);
		}
	}
}

void HotlistManager::ChosenImport(int id, 
								  int format, 
								  BOOL import_into_root_folder, 
								  BOOL show_import_info, 
								  HotlistModel* model, 
								  OpString* filename)
{

	INT32 parent_index = -1;
	HotlistModelItem *hli = model->GetItemByID(id);
	if( hli )
	{
		parent_index = model->GetIndexByItem( hli->IsFolder() ? hli : hli->GetParentFolder() );
	}

	INT32 start_count = model->GetCount();

	HotlistParser parser;
	if( OpStatus::IsSuccess(parser.Parse(*filename, *model, parent_index, format, TRUE, import_into_root_folder)) )
	{
		if (show_import_info)
		{
			if( model->GetCount() - start_count > 1 )
			{
				OpString tmp, message;
				g_languageManager->GetString(Str::S_HOTLIST_ENTRIES_IMPORTED, tmp);
				message.AppendFormat(tmp.CStr(), model->GetCount()-start_count);
				SimpleDialog::ShowDialog(WINDOW_NAME_ENTRIES_IMPORTED, NULL, message.CStr(), UNI_L("Opera"), Dialog::TYPE_OK, Dialog::IMAGE_INFO);
			}
			else if( model->GetCount() - start_count == 1 )
			{
				OpString tmp;
				g_languageManager->GetString(Str::S_HOTLIST_ENTRY_IMPORTED, tmp);
				SimpleDialog::ShowDialog(WINDOW_NAME_ENTRIES_IMPORTED, NULL, tmp.CStr(), UNI_L("Opera"), Dialog::TYPE_OK, Dialog::IMAGE_INFO);
			}
		}

		model->SetDirty( TRUE );
	}
}


// Implement HotlistModelListener methods
BOOL HotlistManager::OnHotlistSaveRequest( HotlistModel *model )
{
	return SaveModel(GetTypeFromModel(*model));
}

BOOL HotlistManager::SaveModel(HotlistModel& model, OpString& filename, BOOL enable_backup)
{
	if (model.GetModelType() == HotlistModel::BookmarkRoot)
	{
		return FALSE;
	}

	if( model.IsDirty() )
	{
		if( enable_backup )
		{
			if( SafeWrite( filename, model ) )
			{
				model.SetDirty( FALSE );
				return TRUE;
			}
		}
		else
		{
			HotlistParser parser;
			if( parser.Write(filename, model, TRUE, FALSE)==OpStatus::OK)
			{
				model.SetDirty( FALSE );
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL HotlistManager::SafeWrite(const OpString& filename, 
							   HotlistModel &model )
{
	HotlistParser parser;
	if( parser.Write(filename, model, TRUE, TRUE) != OpStatus::OK )
	{
		if( ShowSavingFailureDialog(filename) )
		{
			return SafeWrite(filename, model);
		}

		return FALSE;
	}

	return TRUE;
}


HotlistModelItem *HotlistManager::GetItemByID( INT32 id )
{
	HotlistModelItem *item = 0;

	for (int i = HotlistModel::BookmarkRoot; i <= HotlistModel::UniteServicesRoot; i++)
	{
		item = GetHotlistModel(i).GetItemByID(id);

		if (item)
		{
			return item;
		}
	}

	return item;
}


INT32 HotlistManager::GetFolderIdById( INT32 id )
{
	if (id >= HotlistModel::BookmarkRoot && id <= HotlistModel::UniteServicesRoot)
		return id;

	HotlistModelItem* hli = GetItemByID( id );
	if( hli )
	{
		if( hli->IsFolder() )
		{
			return hli->GetID();
		}
		else if( hli->GetParentFolder() )
		{
			return hli->GetParentFolder()->GetID();
		}
		else
		{
			return GetTypeFromModel(*hli->GetModel());
		}
	}

	return -1;
}


INT32 HotlistManager::GetFirstId(int type)
{
	HotlistModelItem *hmi = GetHotlistModel(type).GetItemByIndex(0);
	return hmi ? hmi->GetID() : -1;
}


INT32 HotlistManager::GetPersonalBarPosition( INT32 id )
{
	HotlistModelItem* hmi = GetItemByID( id );
	return hmi ? hmi->GetPersonalbarPos() : -1;
}


INT32 HotlistManager::GetPanelPosition( INT32 id )
{
	HotlistModelItem* hmi = GetItemByID( id );
	return hmi ? hmi->GetPanelPos() : -1;
}



void HotlistManager::SetPersonalBarPosition( INT32 id, INT32 position, BOOL broadcast )
{
	HotlistModelItem* hmi = GetItemByID( id );
	if( hmi )
	{
		if (!broadcast && hmi->GetModel()->GetModelType() == HotlistModel::BookmarkRoot)
			BookmarkWrapper(hmi)->SetIgnoreNextNotify(TRUE);

		hmi->SetPersonalbarPos( position );
		hmi->SetOnPersonalbar( position >= 0 );

		if (!broadcast && hmi->GetModel()->GetModelType() == HotlistModel::BookmarkRoot)
			BookmarkWrapper(hmi)->SetIgnoreNextNotify(FALSE);

		hmi->GetModel()->SetDirty( TRUE );
		if( broadcast )
		{
			INT32 index = hmi->GetModel()->GetIndexByItem(hmi);
			if( index != -1 )
			{
				hmi->Change();
				hmi->GetModel()->BroadcastHotlistItemChanged(hmi, FALSE, HotlistModel::FLAG_PERSONALBAR);
			}
		}
	}
}


void HotlistManager::SetPanelPosition( INT32 id, INT32 position, BOOL broadcast )
{
	HotlistModelItem* hmi = GetItemByID( id );
	// Don't allow dropping folders on panel
	if( hmi && !hmi->IsFolder())
	{
		if (!broadcast && hmi->GetModel()->GetModelType() == HotlistModel::BookmarkRoot)
			BookmarkWrapper(hmi)->SetIgnoreNextNotify(TRUE);

		hmi->SetPanelPos( position );

		if (!broadcast && hmi->GetModel()->GetModelType() == HotlistModel::BookmarkRoot)
			BookmarkWrapper(hmi)->SetIgnoreNextNotify(FALSE);
		//hmi->SetInPanel( position >= 0 );
		hmi->GetModel()->SetDirty( TRUE );
		if( broadcast )
		{
			INT32 index = hmi->GetModel()->GetIndexByItem(hmi);
			if( index != -1 )
			{
				hmi->Change();
				hmi->GetModel()->BroadcastHotlistItemChanged(hmi, FALSE, HotlistModel::FLAG_PANEL);
			}
		}
	}
}


BOOL HotlistManager::IsOnPersonalBar( OpTreeModelItem *item )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	return hmi && hmi->GetOnPersonalbar();
}


BOOL HotlistManager::IsInPanel( OpTreeModelItem *item )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	return hmi && hmi->GetInPanel();
}


BOOL HotlistManager::IsFolder( OpTreeModelItem *item )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	return hmi && hmi->IsFolder();
}


BOOL HotlistManager::IsGroup( OpTreeModelItem *item )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	return hmi && hmi->IsGroup();
}


BOOL HotlistManager::IsContact( const OpStringC &mail_address )
{
	HotlistModelItem *item = 0;
	OpString tmp;
	tmp.Set( mail_address );
	item = GetContactsModel()->GetByEmailAddress( tmp );
	return item && item->IsContact();
}

BOOL HotlistManager::IsBookmarkFolder( OpTreeModelItem *item )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	return hmi && hmi->IsFolder() && GetBookmarksModel()->GetIndexByItem(reinterpret_cast<GenericTreeModelItem *>(item)) > -1;
}

BOOL HotlistManager::ShowOnPersonalBar( INT32 id, BOOL state )
{
	OpTreeModelItem *item = GetItemByID( id );
	if( item && IsOnPersonalBar(item) != state )
	{
		ItemData item_data;
		if( GetItemValue( item, item_data ) )
		{
			item_data.personalbar_position = state ? 0 : -1;
			SetItemValue( item, item_data, FALSE , HotlistModel::FLAG_PERSONALBAR );
			return TRUE;
		}
	}

	return FALSE;
}

// Note. Returns false if state equals current state
BOOL HotlistManager::ShowInPanel( INT32 id, BOOL state )
{
	OpTreeModelItem *item = GetItemByID( id );
	if( item && IsInPanel(item) != state )
	{
		ItemData item_data;
		if( GetItemValue( item, item_data ) )
		{
			item_data.panel_position = state ? 0 : -1;
			SetItemValue( item, item_data, FALSE, HotlistModel::FLAG_PANEL );
			return TRUE;
		}
	}

	return FALSE;
}

BOOL HotlistManager::ShowSmallScreen( INT32 id, BOOL state )
{
	HotlistModelItem *item = GetItemByID( id );
	if( item && item->GetSmallScreen() != state )
	{
		ItemData item_data;
		if( GetItemValue( item, item_data ) )
		{
			item_data.small_screen = state;
			SetItemValue( item, item_data, FALSE, HotlistModel::FLAG_SMALLSCREEN );
			return TRUE;
		}
	}

	return FALSE;
}

// TODO: Handle duplicate -
BOOL HotlistManager::DropItem( const ItemData &item_data, OpTypedObject::Type type, INT32 onto, DesktopDragObject::InsertType insert_type, BOOL execute, OpTypedObject::Type drag_object_type, INT32 *first_id )
{
	HotlistGenericModel *model = 0;
	HotlistModelItem* to = GetItemByID( onto );

	if (to && !to->GetSubfoldersAllowed())
	{
		if (type == OpTypedObject::FOLDER_TYPE)
		{
			return FALSE;
		}

	}
	if (to && !to->GetSeparatorsAllowed())
	{
		if (type == OpTypedObject::SEPARATOR_TYPE)
		{
			return FALSE;
		}

	}

	if( to )
	{
		model = static_cast<HotlistGenericModel*>(to->GetModel());
	}
	else if( HotlistModel::GetIsRootFolder(onto) )
	{
		model = static_cast<HotlistGenericModel*>(&GetModelData(onto).GetHotlistModel());
	}
	if( !model )
	{
		return FALSE;
	}

#ifdef WEBSERVER_SUPPORT
	if (model->GetModelType() == HotlistModel::UniteServicesRoot)
	{
		if (insert_type == DesktopDragObject::INSERT_AFTER && !to
			|| insert_type == DesktopDragObject::INSERT_BEFORE && (!to->IsFolder() && !to->GetParentFolder()) && !to->GetPreviousItem())
			{
				return FALSE;
			}
	}
#endif


	if( execute )
	{
		UINT32 model_type = 0;

		switch (type)
		{
			case OpTypedObject::BOOKMARK_TYPE: model_type = HotlistModel::BookmarkRoot; break;
			case OpTypedObject::CONTACT_TYPE:  model_type = HotlistModel::ContactRoot;  break;
			case OpTypedObject::NOTE_TYPE:     model_type = HotlistModel::NoteRoot;     break;
		    case OpTypedObject::UNITE_SERVICE_TYPE:   model_type = HotlistModel::UniteServicesRoot;   break;
		}

		HotlistGenericModel tmp(model_type, TRUE);
		HotlistModelItem* hmi = 0;

		// We know better departement
		if( type == OpTypedObject::BOOKMARK_TYPE )
		{
			OpString str;
			str.Set( item_data.url );
			str.Strip();
			if( str.FindI(UNI_L("mailto:")) == 0 )
			{
				OP_STATUS status;
				hmi = tmp.AddItem(OpTypedObject::CONTACT_TYPE, status, /* parsed_item =*/FALSE);
				if( hmi ) // contact
				{
					SetItemValue( hmi, item_data, TRUE );
					hmi->SetMail( hmi->GetUrl().CStr() );
					hmi->SetUrl( UNI_L("") );
					hmi->SetIconName("0");
				}
			}
		}

		if( !hmi ) // not contact
		{

			OP_STATUS status;
			hmi = tmp.AddItem( type, status, FALSE );
			if( hmi )
			{
				if ( item_data.created.HasContent() )
				{
					hmi->SetCreated( uni_atoi(item_data.created.CStr()) );
				}
				else
				{
					hmi->SetCreated(g_timecache->CurrentTime());
				}

				SetItemValue( hmi, item_data, TRUE, HotlistModel::FLAG_UNKNOWN ); // (tmp model)
			}
		}

		if( hmi )
		{
			// These are not assigned in SetContactData()
			hmi->SetPersonalbarPos( hmi->GetOnPersonalbar() ? item_data.personalbar_position : -1 );
			hmi->SetPanelPos( hmi->GetInPanel() ? item_data.panel_position : -1 );

			if( !hmi->GetName().HasContent() )
			{
				if( hmi->IsBookmark() )
				{
					hmi->SetName(hmi->GetUrl().CStr());
				}
				else if( hmi->IsContact() )
				{
					hmi->SetName(hmi->GetMail().CStr());
				}

			}

			// Input item at new location
			model->SetDirty( TRUE );

			// If dropping a bookmark its a move, If dropping something else it's an add
			if (drag_object_type == OpTypedObject::DRAG_TYPE_BOOKMARK)
			{
				model->SetIsMovingItems(TRUE);
			}

			INT32 id = model->PasteItem( tmp, to, insert_type, HotlistModel::Personalbar|HotlistModel::Panel );

			model->GetItemByID(id);

			model->SetIsMovingItems(FALSE);

			if (!to || !to->IsGroup())
			{
				if( first_id )
				{
					*first_id = id;
				}
			}
		}
	}

    return TRUE;
}


// TODO: Handle duplicate -
BOOL HotlistManager::DropItem( INT32 id, INT32 onto, DropType drop_type, DesktopDragObject::InsertType insert_type, BOOL execute, INT32 *first_id, BOOL force_delete)
{
	HotlistGenericModel *model    = 0;
	HotlistModelItem* from = GetItemByID( id );
	HotlistModelItem* to   = GetItemByID( onto );

	//BOOL moving_state
	if (to)
	{
#ifdef WEBSERVER_SUPPORT
		// Disallow dragging trash
		if (from)
		{
			if (to->GetModel() && to->GetModel()->GetModelType() == HotlistModel::UniteServicesRoot)
			{
				if (from->GetIsTrashFolder() || from->IsRootService())
				{
					return FALSE;
				}
			}
		}

		// Check if inserting hmi after to is allowed
		if (to->GetModel() && from && (to->GetModel()->GetModelType() == HotlistModel::UniteServicesRoot))
		{
			if (! static_cast<HotlistGenericModel*>(to->GetModel())->IsDroppingGadgetAllowed(from, to, insert_type))
			{
				return FALSE;
			}
			if (drop_type == DROP_COPY)
			{
				return FALSE;
			}
			
		}

		if (to->GetModel() && to->GetModel()->GetModelType() == HotlistModel::UniteServicesRoot)
		{
			if (insert_type == DesktopDragObject::INSERT_AFTER && !to
				|| insert_type == DesktopDragObject::INSERT_BEFORE && (!to->IsFolder() && !to->GetParentFolder()) && !to->GetPreviousItem())
				{
					return FALSE;
				}
		}
#endif

		if ((!from || (from && !from->GetIsInsideTrashFolder())) && to && to->GetHasMoveIsCopy())
		{
			if (drop_type == DROP_MOVE)
			{
				drop_type = DROP_COPY;
			}
		}

		if (from && from->IsFolder() && !to->GetSubfoldersAllowed())
			return FALSE;

		if (from && from->IsSeparator() && !to->GetSeparatorsAllowed())
			return FALSE;

		// Make sure trash and target folders don't change level
		// Note: this code well the "else if" will need to be modified once target folders are
		// not only on the root as they are now.
		if (from && (from->GetIsTrashFolder() 
					 || (from->IsFolder() && from->GetTarget().HasContent()))
			)
		{
			// Don't move the trash folder into another folder
			if (insert_type == DesktopDragObject::INSERT_INTO && to->IsFolder())
				return FALSE;
			else if ((insert_type == DesktopDragObject::INSERT_BEFORE || insert_type == DesktopDragObject::INSERT_AFTER) && to->GetParentFolder())
				return FALSE;
		}
	}

	// Do not move on same item or between models.
	if( !from || from == to  )
	{
		return FALSE;
	}
	else if( !to )
	{
		if( HotlistModel::GetIsRootFolder(onto) )
		{
			model = static_cast<HotlistGenericModel*>(&GetModelData(onto).GetHotlistModel());
		}
	}
	else
	{
		model = static_cast<HotlistGenericModel*>(to->GetModel());
		
	}

	if( !model || model != from->GetModel() )
	{
		return FALSE;
	}

	// Do not move or copy a folder into its own child tree
	if( to && from->IsFolder() )
	{
		HotlistModelItem* hmi = to->GetParentFolder();
		while( hmi )
		{
			if( hmi == from )
			{
				return FALSE;
			}
			hmi = hmi->GetParentFolder();
		}
	}

	if( execute )
	{

		// if USE_MOVE_ITEM on, only way to get here is force_delete is TRUE
		// if force_delte is TRUE, item will be deleted completely, so SetIsMovingItems is not needed

		INT32 flag = (drop_type == DROP_MOVE) ? (HotlistModel::Personalbar|HotlistModel::Panel|HotlistModel::Trash) : 0;

		model->SetIsMovingItems(drop_type == DROP_MOVE);

		// Save item (and any children) to move
		HotlistGenericModel tmp(from->GetModel()->GetModelType(), TRUE);

		tmp.SetIsMovingItems(drop_type == DROP_MOVE);

		BOOL from_trash_to_normal = FALSE;

		if (from && from->IsBookmark() && from->GetIsInsideTrashFolder()
			&& (to && !to->GetIsTrashFolder() && !to->GetIsInsideTrashFolder()
				|| to && to->GetIsTrashFolder() && insert_type != DesktopDragObject::INSERT_INTO))
		{
			from_trash_to_normal = TRUE;
		}


		/**
		 * Get a copy of the item from in tmp model
		 * Paste this item from tmp into destination model
		 * Delete the original item from destination model
		 */
		tmp.CopyItem(from, flag);

		// Input saved item at new location
		model->SetDirty( TRUE );

		INT32 item_id = 0;

		/**
		 * Changed order of Delete and Paste.
		 * The original Order here -> Make copy, paste in, then delete
		 * is a problem, because we'll get the add event
		 * and then the removing event on the same bookmark
		 * This broke the hash table code because the item would first
		 * be added (adding the copy instead of the original item)
		 * and then removed (removing the copy)
		 *
		 **/
		if (to == NULL || !to->IsGroup())
		{
			if( drop_type == DROP_MOVE)
			{
				// Delete source item
				model->DeleteItem( from, FALSE, TRUE /* handle_trash*/, TRUE);
			}
		}
		if (!force_delete)
		{
			item_id = model->PasteItem( tmp, to, insert_type, flag);

			if( first_id )
			{
				*first_id = item_id;
			}

		}

		if (from_trash_to_normal)
		{
			HotlistModelItem* item = model->GetItemByID(*first_id);
			if (item && item->IsBookmark())
			{
				model->ItemUnTrashed(item);
			}
		}


		model->SetIsMovingItems(FALSE);
	}

	return TRUE;
}


/**
 * Copy
 * 
 * Copy items in id_list to clipboard
 *
 * @param id_list - items to copy
 *
 */
// TODO: Clipboard and moving state
void HotlistManager::Copy( OpINT32Vector &id_list )
{
	OpString global_clipboard_text;

	m_clipboard.Erase();

	UINT32 i;
	for( i=0; i<id_list.GetCount(); i++ )
	{
		HotlistModelItem *hmi = GetItemByID(id_list.Get(i));
		if( hmi )
		{
			m_clipboard.CopyItem( hmi, 0 ); // And any children as well.

			OpInfoText text;
			text.SetAutoTruncate(FALSE);

			hmi->GetInfoText(text);

			if (text.HasStatusText())
			{
				if (global_clipboard_text.HasContent())
				{
					global_clipboard_text.Append(UNI_L("\n"));
				}

				global_clipboard_text.Append(text.GetStatusText().CStr());
			}
		}
	}

	if (global_clipboard_text.HasContent())
	{
		g_desktop_clipboard_manager->PlaceText(global_clipboard_text.CStr(), g_application->GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
		// Mouse selection
		g_desktop_clipboard_manager->PlaceText(global_clipboard_text.CStr(), g_application->GetClipboardToken(), true);
#endif
	}
}

/**
 *
 * Copy
 *
 * Adds item with fields item_data to clipboard
 *
 * @param item_data
 * @param append
 * @flag_changed
 *
 * ( This is used by HistoryView and LinksView )
 *
 */
void HotlistManager::Copy( const ItemData& item_data, BOOL append, INT32 flag_changed )
{
	if( !append )
	{
		g_desktop_clipboard_manager->PlaceText(UNI_L(""));
		m_clipboard.Erase();
	}

	OP_STATUS status;
	HotlistModelItem* hmi = m_clipboard.AddItem( OpTypedObject::BOOKMARK_TYPE, status, /*parsed_item*/FALSE );
	SetItemValue( hmi, item_data, FALSE, flag_changed );

	OpInfoText text;
	text.SetAutoTruncate(FALSE);
	hmi->GetInfoText(text);

	if (text.HasStatusText())
	{
		OpString global_clipboard_text;

		g_desktop_clipboard_manager->GetText(global_clipboard_text);

		if (global_clipboard_text.HasContent())
		{
			global_clipboard_text.Append(UNI_L("\n"));
		}

		global_clipboard_text.Append(text.GetStatusText().CStr());

		if (global_clipboard_text.HasContent())
		{
			g_desktop_clipboard_manager->PlaceText(global_clipboard_text.CStr(), g_application->GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
			g_desktop_clipboard_manager->PlaceText(global_clipboard_text.CStr(), g_application->GetClipboardToken(), true);
#endif
		}
	}
}


/**
 * Delete
 * 
 * @param id_list - ids of items to delete
 * @param save_to_clipboard - If True, then the items are saved to clipboard
 *
 * NOTE: The items must keep there GUIDS when copied to the clipboard,
 *       in case they are later dropped into the model, in which case
 *       this should be considered a move of one and the same item
 *
 * NOT move to trash, Total delete, or copy to clipboard
 *
 * This is either if !save_to_clipboard -> DeleteItem == MoveToTrash
 *      gjøres m kall på DeleteItem(hmi, copy_to_trash)
 *
 * or if save_to_clipboard -> MoveItems To Clipboard
 *      gjøres med 1) Copy to clipboard, 2) Delete Item totally from model
 *
 *
 * Should be changed to either
 *   1) MoveToTrash -> Call DeleteItem(hmi, copy_to_trash) which will call MoveItem
 * or
 *   2) MoveItem(from model to clipboard)
 *
 */
BOOL HotlistManager::Delete(OpINT32Vector &id_list, BOOL save_to_clipboard)
{
	if( id_list.GetCount() == 0 )
	{
		return FALSE;
	}

	m_clipboard.SetIsMovingItems(TRUE);

	if( save_to_clipboard )
	{
		Copy(id_list);
	}

	BOOL deletion_really_wanted = TRUE;
	UINT32 i;
	for(i = 0; i < id_list.GetCount(); i++)
	{
		HotlistModelItem *hmi = GetItemByID(id_list.Get(i));
		if( hmi )
		{
			// No cutting target folders
			if (save_to_clipboard == TRUE && hmi->GetTarget().HasContent())
				continue;

			hmi->GetModel()->SetDirty( TRUE );
			if (hmi->IsGadget() 
			#ifdef WEBSERVER_SUPPORT
			|| hmi->IsUniteService()
			#endif // WEBSERVER_SUPPORT
			)
			{
				GadgetModelItem * gadget_item = static_cast<GadgetModelItem *>(hmi);
				
				#ifdef WEBSERVER_SUPPORT
				// show deletion warnings for unite services
				if (hmi->IsUniteService())
				{
					OpString title, message;
					g_languageManager->GetString(Str::S_DELETE, title);
					
					if (gadget_item->IsGadgetRunning())
					{
						if (save_to_clipboard)
						{	// permanent deletion, gadget running
							g_languageManager->GetString(Str::D_WEBSERVER_DELETE_RUNNING_SERVICE_WARNING, message);
						}
						else
						{	// move to trash, gadget running
							g_languageManager->GetString(Str::D_WEBSERVER_MOVE_RUNNING_SERVICE_TO_TRASH_WARNING, message);
						}
					}
					else if (save_to_clipboard)
					{	// permanent deletion, gadget not running
						g_languageManager->GetString(Str::D_WEBSERVER_DELETE_SERVICE_WARNING, message);
					}

					if (deletion_really_wanted && message.HasContent())
					{
						INT32 result = 0;

						SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
						if (dialog)
						{
							dialog->SetOkTextID(Str::S_DELETE);
							dialog->SetProtectAgainstDoubleClick(SimpleDialog::ForceOff);
							dialog->Init(WINDOW_NAME_BOOKMARK_DELETE_CONFIRM, title, message, g_application->GetActiveDesktopWindow(), Dialog::TYPE_OK_CANCEL, Dialog::IMAGE_WARNING, TRUE, &result);
							if (result == Dialog::DIALOG_RESULT_NO)
							{
								deletion_really_wanted = FALSE;
							}
						}
					}
				}
				#endif // WEBSERVER_SUPPORT
				if (deletion_really_wanted)
				{
					if (save_to_clipboard)
					{
						// Uninstall
						gadget_item->UninstallGadget();
					}
					else
					{
						// Close
						gadget_item->ShowGadget(FALSE, TRUE);
					}
				}
			}
			if (deletion_really_wanted)
			{
#ifdef HOTLIST_USE_MOVE_ITEM
				if (!save_to_clipboard) // Move item to trash
				{
					hmi->GetModel()->MoveItem(hmi->GetModel(), hmi->GetIndex(), hmi->GetModel()->GetTrashFolder(), INSERT_INTO);
				}
				else //  Move item from model to clipboard
				{
					m_clipboard.MoveItem(hmi->GetModel(), hmi->GetIndex(), NULL, INSERT_INTO);
				}
#else
				static_cast<HotlistGenericModel*>(hmi->GetModel())->DeleteItem( hmi, !save_to_clipboard );
#endif
			}
		}

	}

#ifndef HOTLIST_USE_MOVE_ITEM
	m_clipboard.SetIsMovingItems(FALSE);
#endif

	return TRUE;
}


/**
 * Paste
 *
 * Paste items from clipboard into model target, relative to item at.
 * (insert type is INSERT_INTO)
 * 
 * @param target - 
 * @param at - 
 *
 *
 */
BOOL HotlistManager::Paste( OpTreeModel* target, OpTreeModelItem* at )
{
	if( m_clipboard.GetItemCount() > 0 )
	{
		ModelData* data = GetModelData(*target);

		// Check for max items in folder limit (Only bookmarks model)
		if( data )
		{
			data->GetHotlistModel().SetDirty(TRUE);
			// TODO: Use MoveItem to clipboard
			static_cast<HotlistGenericModel&>(data->GetHotlistModel()).PasteItem( m_clipboard, at, DesktopDragObject::INSERT_INTO, 0 );
		}
	}
	return TRUE;
}

BOOL HotlistManager::ShowTargetDeleteDialog(HotlistModelItem* hmi_src)
{
#ifdef SUPPORT_DATA_SYNC
	// Message and delete!
	OpString title, message;

	// Show a special message for Opera Mini
	if (!hmi_src->GetTarget().CompareI("mini"))
	{
		g_languageManager->GetString(Str::D_BOOKMARK_PROP_MINI_DELETE_TITLE, title);
		g_languageManager->GetString(Str::D_BOOKMARK_PROP_MINI_DELETE_TEXT, message);
	}
	else
	{
		// General target message
		OpString format;

		g_languageManager->GetString(Str::D_BOOKMARK_PROP_TARGET_DELETE_TITLE, title);
		g_languageManager->GetString(Str::D_BOOKMARK_PROP_TARGET_DELETE_TEXT, format);
		message.AppendFormat(format.CStr(), hmi_src->GetName().CStr());
	}

	// Show the dialog and stop if they press NO.
	if (Dialog::DIALOG_RESULT_NO == SimpleDialog::ShowDialog(WINDOW_NAME_TARGET_DELETE, g_application->GetActiveDesktopWindow(), message.CStr(), title.CStr(), Dialog::TYPE_YES_NO, Dialog::IMAGE_WARNING))
		return FALSE;

#endif
	return TRUE;
}

BOOL HotlistManager::EmptyTrash( OpTreeModel* target )
{
	ModelData* data = GetModelData(*target);
	if( data )
	{
		data->GetHotlistModel().SetDirty(TRUE);
		data->GetHotlistModel().EmptyTrash();
	}
	return TRUE;
}


BOOL HotlistManager::HasClipboardData()
{
	return m_clipboard.GetItemCount() > 0;
}

BOOL HotlistManager::HasClipboardItems()
{
	return m_clipboard.HasItems();
}

BOOL HotlistManager::HasClipboardItems(INT32 item_type)
{
	return m_clipboard.HasItems(item_type);
}

// Note: Checks for folders, separators, and items of item_type
BOOL HotlistManager::HasClipboardData(INT32 item_type)
{
	for (INT32 i = 0; i < m_clipboard.GetCount(); i++)
	{
		HotlistModelItem* item = m_clipboard.GetItemByIndex(i);
		if (item && (item ->GetType() == item_type
					 || item->IsFolder() || item->IsSeparator()))
			return TRUE;
	}

	return FALSE;
}

INT32 HotlistManager::GetItemTypeFromModelType(INT32 model_type)
{
	switch(model_type)
	{
	case HotlistModel::BookmarkRoot: return OpTypedObject::BOOKMARK_TYPE;
	case HotlistModel::NoteRoot:     return OpTypedObject::NOTE_TYPE;
	case HotlistModel::ContactRoot:  return OpTypedObject::CONTACT_TYPE;
	case HotlistModel::UniteServicesRoot:   return OpTypedObject::UNITE_SERVICE_TYPE;
	default: return -1;
	}
}

BOOL HotlistManager::EditItem_ByEmail( const OpStringC& mail_address , DesktopWindow* parent )
{
	HotlistModelItem *item = GetContactsModel()->GetByEmailAddress( mail_address );

	return item && EditItem( item->GetID(), parent );
}

BOOL HotlistManager::EditItem_ByEmailOrAddIfNonExistent( const OpStringC& mail_address, DesktopWindow* parent )
{
	HotlistModelItem *item = GetContactsModel()->GetByEmailAddress( mail_address );

	// if it doesn't exist add the item
	if (!item)
	{
		ItemData item_data;
		item_data.mail.Set(mail_address);

		if (NewContact(item_data, HotlistModel::ContactRoot, parent, TRUE))
			item = GetContactsModel()->GetByEmailAddress( mail_address );
	}

	return item && EditItem( item->GetID(), parent );
}

BOOL HotlistManager::EditItem( INT32 id, DesktopWindow* parent )
{
	HotlistModelItem* item = GetItemByID(id);

	if( item && !item->IsSeparator())
	{
		int type = GetTypeFromModel(*item->GetModel());

		if( type == HotlistModel::ContactRoot )
		{
			if (item->IsGroup())
			{
				GroupPropertiesDialog* dialog = OP_NEW(GroupPropertiesDialog, ());
				if (dialog)
					dialog->Init(parent, item);
			}
			else
			{
				ContactPropertiesDialog* dialog = OP_NEW(ContactPropertiesDialog, ());
				if (dialog)
					dialog->Init(parent, item->GetModel(), item, item->GetParentFolder() );
			}
			return TRUE;
		}
		else if(type == HotlistModel::NoteRoot )
		{
			// Note properties
			return TRUE;
		}
#ifdef GADGET_SUPPORT
		else if(type == HotlistModel::UniteServicesRoot)
		{
			GetDesktopGadgetManager()->ShowUniteServiceSettings(item);
			return TRUE;
		}
#endif // GADGET_SUPPORT
		else if(type == HotlistModel::BookmarkRoot)
		{
			EditBookmarkDialog* dialog = OP_NEW(EditBookmarkDialog, (FALSE));
			if (dialog)
				dialog->Init(parent, item);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL HotlistManager::AddNickToContact(INT32 contact_id, OpStringC& nick)
{
	HotlistModelItem* item = GetItemByID(contact_id);

	if (item)
	{
		OpString nicks;

		nicks.Set(item->GetShortName().CStr());

		if (nicks.HasContent())
		{
			nicks.Append(UNI_L(", "));
		}

		nicks.Append(nick.CStr());

		item->SetShortName(nicks.CStr()); // this will trigger the internal notification
		item->Change();
		item->GetModel()->SetDirty(TRUE);
		return TRUE;
	}

	return FALSE;
}

void HotlistManager::SetupPictureUrl(ItemData& item_data)
{
	// Determine email name and domain
	int p1 = item_data.mail.Find("@");
	if( p1 == KNotFound )
	{
		return;
	}

	OpString name, domain;
	name.Set(item_data.mail.CStr(), p1);
	domain.Set(item_data.mail.SubString(p1+1));

	// Try to open an ini file
	OpFile file;
	file.Construct(UNI_L("contactpicture.ini"), OPFILE_HOME_FOLDER);
	BOOL exist = FALSE;
	file.Exists(exist);
	if( !exist )
	{
		file.Construct(UNI_L("contactpicture.ini"), OPFILE_RESOURCES_FOLDER); // should (perhaps) be in INI folder
	}


	PrefsFile* prefs = OP_NEW(PrefsFile, (PREFS_INI));
	TRAPD(err, prefs->ConstructL());
	TRAP(err, prefs->SetFileL(&file));
	TRAP(err, prefs->LoadAllL());

	// Load the picture url template for the domain
	OpString picture_url;
	TRAP(err, prefs->ReadStringL(domain, UNI_L("picture url"), picture_url));

	if( picture_url.HasContent() )
	{
		while(1)
		{
			int p1 = picture_url.Find("%");
			if( p1 == KNotFound )
				break;

			int flag = uni_toupper(picture_url.CStr()[p1+1]);
			if( flag == 'M' ) // Replace %M with full mail address
			{
				OpString tmp;
				tmp.Set(picture_url.CStr(), p1 );
				tmp.Append(item_data.mail);
				tmp.Append(&picture_url.CStr()[p1+2]);
				picture_url.Set(tmp);
			}
			if( flag == 'N' ) // Replace %N with name without domain
			{
				OpString tmp;
				tmp.Set(picture_url.CStr(), p1 );
				tmp.Append(name);
				tmp.Append(&picture_url.CStr()[p1+2]);
				picture_url.Set(tmp);
			}

			else
			{
				break;
			}
		}

		item_data.pictureurl.Set(picture_url);
	}
	OP_DELETE(prefs);
}

BOOL HotlistManager::NewContact( const ItemData& item_data,
								 INT32 parent_id, DesktopWindow* parent,
								 BOOL interactive )
{
	if( parent_id <= HotlistModel::MaxRoot )
	{
		parent_id = HotlistModel::ContactRoot;
	}

	return NewItem( parent_id, parent, OpTreeModelItem::CONTACT_TYPE, &item_data, interactive, 0 );
}


BOOL HotlistManager::NewContact( INT32 parent_id, DesktopWindow* parent )
{
	if( parent_id <= HotlistModel::MaxRoot )
	{
		parent_id = HotlistModel::ContactRoot;
	}

	return NewItem( parent_id, parent, OpTreeModelItem::CONTACT_TYPE, 0, TRUE, 0 );
}


BOOL HotlistManager::NewGroup( INT32 parent_id, DesktopWindow* parent )
{
	if( parent_id <= HotlistModel::MaxRoot )
	{
		parent_id = HotlistModel::ContactRoot;
	}

	return NewItem( parent_id, parent, OpTreeModelItem::GROUP_TYPE, 0, TRUE, 0 );
}


BOOL HotlistManager::NewFolder( INT32 parent_id, DesktopWindow* parent, OpTreeView* treeView )
{
	return NewItem( parent_id, parent, OpTreeModelItem::FOLDER_TYPE, 0, TRUE, treeView );
}

BOOL HotlistManager::NewFolder(const OpStringC& name, INT32 parent_id, INT32* got_id)
{
	HotlistManager::ItemData item_data;

	item_data.name.Set(name);

	return NewItem( parent_id, NULL, OpTreeModelItem::FOLDER_TYPE, &item_data, FALSE, NULL, got_id);
}

BOOL HotlistManager::NewNote( const ItemData& item_data, INT32 parent_id, DesktopWindow* parent, OpTreeView* treeView )
{
	return NewItem( parent_id, parent, OpTreeModelItem::NOTE_TYPE, &item_data, treeView != NULL, treeView );
}


BOOL HotlistManager::NewSeparator(INT32 target_id, INT32* got_id)
{

	HotlistGenericModel *model = 0;
	HotlistModelItem* to = GetItemByID( target_id );

	if( to )
	{
		model = static_cast<HotlistGenericModel*>(to->GetModel());
	}
	else if( HotlistModel::GetIsRootFolder(target_id) )
	{
		model = static_cast<HotlistGenericModel*>(&GetModelData(target_id).GetHotlistModel());
	}

	if( !model )
	{
		return FALSE;
	}

	if (to && !to->GetSeparatorsAllowed() ||
		to && to->GetParentFolder() && !to->GetParentFolder()->GetSeparatorsAllowed())
	{
		return FALSE;
		// to = NULL;
	}

	HotlistGenericModel tmp(1, TRUE); // Separators can be added to any type

#ifndef HOTLIST_USE_MOVE_ITEM
	tmp.SetIsMovingItems(TRUE);
	model->SetIsMovingItems(TRUE);
#endif // HOTLIST_USE_MOVE_ITEM

	if (to && !to->GetSeparatorsAllowed() || 
		to && to->GetParentFolder() && !to->GetParentFolder()->GetSeparatorsAllowed())
	{
		return FALSE;
		//to = NULL;
	}

	OP_STATUS status;

	// TODO: Change to create item and add it to model at correct place

	INT32 id;
#ifdef HOTLIST_USE_MOVE_ITEM

	HotlistModelItem* hmi = tmp.AddItem(OpTypedObject::SEPARATOR_TYPE, status, /*parsed_item=*/FALSE);

	if (!to)
	{
		id = model->MoveItem(&tmp, hmi->GetIndex(), NULL, INSERT_INTO);
	}
	else
	{
		id = model->MoveItem(&tmp, hmi->GetIndex(), to, INSERT_AFTER);
	}
#else

	tmp.AddItem(OpTypedObject::SEPARATOR_TYPE, status, /*parsed_item=*/FALSE);

	if (!to)
	{
		id = model->PasteItem( tmp, to, DesktopDragObject::INSERT_INTO, 0 );
	}
	else
	{
		id = model->PasteItem( tmp, to, DesktopDragObject::INSERT_AFTER, 0);
	}
#endif // HOTLIST_USE_MOVE_ITEM

	if (got_id)
	{
		*got_id = id;
	}

#ifndef HOTLIST_USE_MOVE_ITEM
	model->SetIsMovingItems(FALSE);
#endif

	return TRUE;
}


#ifdef GADGET_SUPPORT
BOOL HotlistManager::NewRootService(OpGadget& root_gadget)
{
#ifdef WEBSERVER_SUPPORT
	// Don't add the root service twice -- this may be attempted on first run
	// after an upgrade.
	if (GetUniteServicesModel() && GetUniteServicesModel()->GetRootService())
	{
		GetUniteServicesModel()->GetRootService()->Change();
		return TRUE;
	}
#endif

	ItemData item_data; // needed?

	INT32 new_id;

	{
		if (!NewItem( HotlistModel::UniteServicesRoot, NULL,
					  OpTreeModelItem::UNITE_SERVICE_TYPE,
#ifdef WEBSERVER_SUPPORT
					  &item_data,
#else
					  NULL,
#endif
					  FALSE, NULL, &new_id, FALSE, TRUE))
			return FALSE;
	}

	HotlistModelItem* item = GetItemByID(new_id);

#ifdef WEBSERVER_SUPPORT
	if (OpStatus::IsError(static_cast<UniteServiceModelItem*>(item)->InstallRootService(root_gadget)))
#endif // WEBSERVER_SUPPORT
	{
		//could not install gadget, delete hotlist model item and bail out
		item->GetModel()->SetDirty( TRUE );	// model is changing
		static_cast<HotlistGenericModel*>(item->GetModel())->DeleteItem( item, FALSE );
		return FALSE;
	}
	
	return TRUE;
}


#ifdef WEBSERVER_SUPPORT

BOOL HotlistManager::NewUniteService(const OpStringC& address, 
									 const OpStringC& orig_url, 
									 INT32* got_id, 
									 BOOL clean_uninstall, 
									 BOOL launch_after_install,
									 const OpStringC& force_widget_name, 
									 const OpStringC& force_service_name,
									 const OpStringC& shared_path,
									 OpTreeView* treeView)
{
	//FIXME, use active root?
	OpString address_local;
 	address_local.Set(address);

 	OP_ASSERT(GetDesktopGadgetManager()->IsUniteServicePath(address_local));
	if (GetOperaAccountManager()->GetIsOffline())
	{
		if( !AskEnterOnlineMode(TRUE) )
		{
				return OpStatus::ERR;
		}
	}


	ItemData item_data; // needed?

	INT32 new_id;
	if (!NewItem( HotlistModel::UniteServicesRoot, NULL,
				  OpTreeModelItem::UNITE_SERVICE_TYPE,
				  &item_data,
				  FALSE, treeView, &new_id))
		return FALSE;


	HotlistModelItem* item = GetItemByID(new_id);

	if (item && OpStatus::IsError(static_cast<GadgetModelItem*>(item)->InstallGadget(address, orig_url, clean_uninstall, 
		launch_after_install, force_widget_name, force_service_name, shared_path, URL_UNITESERVICE_INSTALL_CONTENT)))
	{
		//could not install gadget, delete hotlist model item and bail out
		item->GetModel()->SetDirty( TRUE );	// model is changing
		static_cast<HotlistGenericModel*>(item->GetModel())->DeleteItem( item, FALSE );
		return FALSE;
	}

	if (got_id)
		*got_id = new_id;

	if (launch_after_install) // don't open the panel when installing default services
	{
		Hotlist* hotlist = GetHotlist();
		if(hotlist)
		{
			if(hotlist->GetAlignment() == OpBar::ALIGNMENT_OFF)
			{
				hotlist->SetAlignment(OpBar::ALIGNMENT_LEFT, FALSE);
			}
			hotlist->ShowPanelByType(OpTypedObject::PANEL_TYPE_UNITE_SERVICES, TRUE);
		}
	}	
	return TRUE;
}
#endif // WEBSERVER_SUPPORT

#endif // GADGET_SUPPORT

/**
 * HotlistManager::NewItem
 *
 * @param BOOL allow_duplicates By default TRUE. If FALSE and type is bookmark,
 *                              item is not added if item with same url already exists
 *
 **/

BOOL HotlistManager::NewItem( INT32 parent_id, DesktopWindow* parent,
							   OpTreeModelItem::Type type,
							   const ItemData* item_data,
							   BOOL interactive, OpTreeView* treeView, INT32* got_id, BOOL allow_duplicates, BOOL root_service )
{
	// find folder
	HotlistModelItem* folder = GetItemByID( parent_id );
	//OP_ASSERT(folder);

	while( folder && !folder->IsFolder() )
	{
		folder = folder->GetParentFolder();
	}

	// find model
	HotlistGenericModel& model = static_cast<HotlistGenericModel&>(GetHotlistModel(GetTypeFromId(parent_id)));


	// Never add items to the trash folder
	if( folder && /* !model.GetIsSyncing() && **/ folder->GetIsInsideTrashFolder() )
	{
		parent_id = GetTypeFromModel(*folder->GetModel());
		folder = 0;
	}

	// Make sure treeview isn't matching text, to prevent the newly created item
	// being invisible
	if (treeView)
		OpStatus::Ignore(treeView->SetText(0));

	// TODO: 
	HotlistModelItem* item = 0;
	item = model.AddTemporaryItem( type );

	if( !item )
	{
		return FALSE;
	}

	if( item_data )
	{
		SetItemValue( item, *item_data, TRUE, HotlistModel::FLAG_UNKNOWN );
	}

	if( interactive 
		&& !item->IsSeparator()
		)
	{
		if( !parent )
		{
			parent = g_application->GetActiveBrowserDesktopWindow();
		}

		if ((item->IsFolder() || item->IsNote()) && treeView


			)
		{
			RegisterTemporaryItem( item, folder ? folder->GetID() : -1, got_id );

			if (item->IsNote())
			{
				// selecting it will invoke editing

				treeView->SetSelectedItemByID(item->GetID());
			}
			else
			{
				INT32 pos = treeView->GetItemByID( item->GetID());
				if (pos != -1)
				{
					treeView->EditItem(pos);
				}
			}
		}
		else if( GetTypeFromModel(model) == HotlistModel::ContactRoot  )
		{
			if (type == OpTreeModelItem::GROUP_TYPE)
			{
				GroupPropertiesDialog* dialog = OP_NEW(GroupPropertiesDialog, ());
				if (dialog)
					dialog->Init(parent, item);
			}
			else
			{
				ContactPropertiesDialog* dialog = OP_NEW(ContactPropertiesDialog, ());
				if (dialog)
					dialog->Init( parent, &model, item, folder );
			}
		}
	}
	else
	{
		// The name of the item can be empty. This can happen on DnD drops
		// of image-links and others. Since we do not use a dialog we
		// fill in the url or e-mail address as name.
		if( !item->GetName().HasContent() )
		{
			if( item->IsBookmark() )
			{
				item->SetName(item->GetUrl().CStr());
			}
			else if( item->IsContact() )
			{
				item->SetName(item->GetMail().CStr());
			}
		}

#ifdef WEBSERVER_SUPPORT

		if (item->IsUniteService() && root_service)
			static_cast<UniteServiceModelItem*>(item)->SetIsRootService();

#endif
		
		BOOL retval = RegisterTemporaryItem( item, folder ? folder->GetID() : -1, got_id); 
		if (item->IsUniteService())
		{
#ifdef WEBSERVER_SUPPORT
			HotlistModel* model = item->GetModel();
			UniteServiceModelItem* unite_item = static_cast<UniteServiceModelItem*>(model->GetItemByID(*got_id));
			if (model && unite_item && unite_item->IsRootService())
			{
				model->SetRootService(unite_item);
			}
#endif
			if(treeView)
			{
				treeView->SetSelectedItemByID(item->GetID());
			}
		}
		return retval;
	}
	return TRUE;

}

BOOL HotlistManager::RegisterTemporaryItem( OpTreeModelItem *item, INT32 parent_id, INT32* got_id )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	if( hmi && hmi->GetTemporaryModel())
	{
		return static_cast<HotlistGenericModel*>(hmi->GetTemporaryModel())->RegisterTemporaryItem( hmi, parent_id, got_id );
	}
	return FALSE;
}

BOOL HotlistManager::RegisterTemporaryItemOrdered( OpTreeModelItem *item, HotlistModelItem* previous )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	if( hmi && hmi->GetTemporaryModel())
	{
		return static_cast<HotlistGenericModel*>(hmi->GetTemporaryModel())->RegisterTemporaryItemOrdered( hmi, previous );
	}
	return FALSE;
}


BOOL HotlistManager::RegisterTemporaryItemFirstInFolder( OpTreeModelItem *item, HotlistModelItem* parent )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	if( hmi && hmi->GetTemporaryModel())
	{
		return static_cast<HotlistGenericModel*>(hmi->GetTemporaryModel())->RegisterTemporaryItemFirstInFolder( hmi, parent );
	}
	return FALSE;
}

BOOL HotlistManager::NewItemOrdered(  HotlistGenericModel* model,
									  OpTreeModelItem::Type type,
									  const ItemData* item_data,
									  HotlistModelItem* previous )
{
	if (!model)
	{
		return FALSE;
	}

	HotlistModelItem* item = model->AddTemporaryItem( type );

	if( !item )
	{
		return FALSE;
	}

	if( item_data )
	{
		SetItemValue( item, *item_data, TRUE , HotlistModel::FLAG_UNKNOWN );
	}

	return RegisterTemporaryItemOrdered( item, previous );
}

BOOL HotlistManager::NewItemFirstInFolder(  HotlistGenericModel* model,
											OpTreeModelItem::Type type,
											const ItemData* item_data,
											HotlistModelItem* parent )
{
	if (!model)
	{
		return FALSE;
	}

	HotlistModelItem* item = model->AddTemporaryItem( type );

	if( !item )
	{
		return FALSE;
	}

	if( item_data )
	{
		SetItemValue( item, *item_data, TRUE, HotlistModel::FLAG_UNKNOWN );
	}

	return RegisterTemporaryItemFirstInFolder( item, parent );
}



BOOL HotlistManager::RemoveTemporaryItem( OpTreeModelItem *item )
{
	if (item)
	{
		HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
		if( hmi && hmi->GetTemporaryModel())
		{
			return static_cast<HotlistGenericModel*>(hmi->GetTemporaryModel())->RemoveTemporaryItem( item );
		}
	}
	return FALSE;
}



BOOL HotlistManager::GetMailAndName(INT32 id, OpString &mail, OpString &name, BOOL include_folder)
{
	HotlistModelItem* hmi = GetItemByID( id );
	if( !hmi || (hmi->IsFolder() && !include_folder) )
	{
		return FALSE;
	}

	if( hmi->IsFolder() )
	{
		HotlistModel* model = hmi->GetModel();
		if( model )
		{
			INT32 index = hmi->GetChildIndex();
			while( index != -1 )
			{
				if( !model->IsFolderAtIndex( index ) )
				{
					hmi = model->GetItemByIndex(index);
					if( !hmi )
					{
						break;
					}
					else
					{
						ItemData item_data;
						if( GetItemValue( hmi, item_data ) )
						{
							if( item_data.mail.HasContent() )
							{
								if( mail.HasContent() )
									mail.Append(UNI_L(","));
								mail.Append( item_data.mail );
							}
							if( name.IsEmpty() )
							{
								// Note: Only first name is saved
								name.Set( item_data.name );
							}
						}
					}
				}
				index = model->GetSiblingIndex( index );
			}
		}
	}
	else
	{
		ItemData item_data;
		if( GetItemValue( id, item_data ) )
		{
			mail.Set( item_data.mail );
			name.Set( item_data.name );
		}
	}

	return mail.HasContent();
}

BOOL HotlistManager::GetMailAndName(OpINT32Vector& id_list, OpString& address, OpString& name, BOOL include_folder_content)
{
	if( id_list.GetCount() == 0 )
	{
		return FALSE;
	}

	UINT32 i;
	for( i=0; i<id_list.GetCount(); i++ )
	{
		OpString tmp_address, tmp_name;

		GetMailAndName( id_list.Get(i), tmp_address, tmp_name, include_folder_content );

		if( tmp_address.HasContent() )
		{
			if (!address.IsEmpty() )
			{
				address.Append(UNI_L(","));
			}
			address.Append(tmp_address);
		}
		if( tmp_name.HasContent() )
		{
			if( name.IsEmpty() )
			{
				// Note: Only first name is saved
				name.Set(tmp_name.CStr());
			}
		}
	}

	return address.HasContent();
}

OP_STATUS HotlistManager::AppendRFC822FormattedItem(const OpStringC& name, const OpStringC& address, OpString& rfc822_address, BOOL& needs_comma) const
{
	OP_STATUS ret = OpStatus::OK;;
#ifdef M2_SUPPORT
	OpString tmp_rfc822_address;
	if (address.HasContent() && name.HasContent() && address.Compare(name)!=0)
	{
		if ((ret=Header::From::GetFormattedEmail(name, address, UNI_L(""), tmp_rfc822_address))!=OpStatus::OK)
			return ret;
	}
	else
	{
		if ((ret=tmp_rfc822_address.Set(address.HasContent() ? address.CStr() : name.CStr())) != OpStatus::OK)
			return ret;
	}

	if (tmp_rfc822_address.HasContent())
	{
		if (needs_comma)
		{
			if ((ret=rfc822_address.Append(UNI_L(", "))) != OpStatus::OK)
				return ret;
		}

		if ((ret=rfc822_address.Append(tmp_rfc822_address)) == OpStatus::OK)
			needs_comma = TRUE;

	}
#endif

	return ret;
}

OP_STATUS HotlistManager::RecursiveAppendRFC822FormattedFolderItems(OpINT32Vector& id_list, HotlistModel* folder_model, INT32 child_index, OpString& rfc822_address, BOOL& needs_comma)
{
	if (!folder_model)
		return OpStatus::OK; //Nothing to do

	OP_STATUS ret;
	while(child_index != -1)
	{
		HotlistModelItem* hmi = folder_model->GetItemByIndex(child_index);

		// If the item isn't a contact or folder blank the address and bail
		if (!hmi->IsContact() && !hmi->IsFolder())
		{
			rfc822_address.Empty();
			break;
		}

		if(!hmi)
		{
			break;
		}

		if(folder_model->IsFolderAtIndex(child_index))
		{
			if ((ret=RecursiveAppendRFC822FormattedFolderItems(id_list, folder_model, folder_model->GetChildIndex(child_index), rfc822_address, needs_comma)) != OpStatus::OK)
				return ret;
		}
		else
		{
			ItemData item_data;
			if(GetItemValue(hmi, item_data))
			{
				if ((ret=AppendRFC822FormattedItem(item_data.name, item_data.mail, rfc822_address, needs_comma)) != OpStatus::OK)
					return ret;
			}
		}

		id_list.RemoveByItem(hmi->GetID());

		child_index = folder_model->GetSiblingIndex(child_index);
	}
	return OpStatus::OK;
}

BOOL HotlistManager::GetRFC822FormattedNameAndAddress(OpINT32Vector& id_list, OpString& rfc822_address, BOOL include_folder_content)
{
	rfc822_address.Empty();

	if (id_list.GetCount() == 0)
		return FALSE;

	BOOL needs_comma = FALSE;
	HotlistModelItem* hmi;
	while (id_list.GetCount() > 0)
	{
		hmi = GetItemByID(id_list.Get(0));

		// If the item isn't a contact or folder blank the address and bail
		if (!hmi->IsContact() && !hmi->IsFolder())
		{
			rfc822_address.Empty();
			break;
		}

		if(!hmi || (hmi->IsFolder() && !include_folder_content))
		{
			id_list.Remove(0);
			continue;
		}

		if(hmi->IsFolder())
		{
			HotlistModel* model = hmi->GetModel();
			if(model)
			{
				if (needs_comma)
				{
					rfc822_address.Append(UNI_L(", "));
					needs_comma = FALSE;
				}

				OpString folder_name;
				folder_name.Set(hmi->GetName());
				rfc822_address.Append(folder_name.HasContent() ? folder_name.CStr() : UNI_L("\"\""));

				rfc822_address.Append(UNI_L(":"));

				if (OpStatus::IsSuccess(RecursiveAppendRFC822FormattedFolderItems(id_list, model, hmi->GetChildIndex(), rfc822_address, needs_comma)))
				{
					rfc822_address.Append(UNI_L("; "));
					needs_comma = TRUE;
				}
			}
		}
		else
		{
			ItemData item_data;
			if(GetItemValue(id_list.Get(0), item_data))
			{
				OpString primary_email;

				// Get only the primary e-mail
				hmi->GetPrimaryMailAddress(primary_email);

				AppendRFC822FormattedItem(item_data.name, primary_email, rfc822_address, needs_comma);
			}
		}

		id_list.Remove(0);
	}

	return rfc822_address.HasContent();
}

BOOL HotlistManager::GetItemValue( INT32 id, ItemData& data/*, INT32& flag_changed */ )
{
	return GetItemValue(GetItemByID(id),data);
}

BOOL HotlistManager::GetItemValue( OpTreeModelItem* item, ItemData& data/*, INT32& flag_changed*/ )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);

	if( !hmi)
	{
		return FALSE;
	}

	data.name.Set( hmi->GetName() );
	data.url.Set( hmi->GetUrl() );
	data.image.Set( hmi->GetImage() );
	data.direct_image_pointer = hmi->GetImage();
	data.description.Set( hmi->GetDescription() );
	data.shortname.Set( hmi->GetShortName() );
	data.created.Set( hmi->GetCreatedString() );
	data.visited.Set( hmi->GetVisitedString() );
	data.small_screen = hmi->GetSmallScreen();
	data.personalbar_position = hmi->GetPersonalbarPos();
	data.panel_position = hmi->GetPanelPos();
	data.unique_guid.Set(hmi->GetUniqueGUID());
	if(hmi->GetParentFolder())
	{
		HotlistModelItem *parent = hmi->GetParentFolder();
		data.folder.Set(parent->GetName());
	}

	if (hmi->IsContact())
	{
		// Contacts
		data.mail.Set( hmi->GetMail() );
		data.phone.Set( hmi->GetPhone() );
		data.fax.Set( hmi->GetFax() );
		data.postaladdress.Set( hmi->GetPostalAddress() );
		data.pictureurl.Set( hmi->GetPictureUrl() );
		data.conaxnumber.Set( hmi->GetConaxNumber() );
	}

	return TRUE;
}


// If the URL changes on a bookmark, the bookmark is no longer the same
// seen to the history module and vps
// Wee need to remove the bookmark and add it again with the new url
void HotlistManager::RemoveBookmarkFromHistory(HotlistModelItem* item)
{
	if (item->IsBookmark() && static_cast<Bookmark*>(item)->GetHistoryItem())
	{
		Bookmark* bookmark = static_cast<Bookmark*>(item);

		OpVector<HistoryItem::Listener> result;
		if (OpStatus::IsSuccess(bookmark->GetHistoryItem()->GetListenersByType(OpTypedObject::BOOKMARK_TYPE, result)))
		{
			// If this is the only bookmark with this url, remove it from history
			if (result.GetCount() == 1 && static_cast<Bookmark*>(result.Get(0)) == item)
			{
				DesktopHistoryModel::GetInstance()->OnHotlistItemRemoved(item, FALSE);
			}
		}

		// Reset m_history_item on bookmark
		bookmark->RemoveHistoryItem();
	}
}

BOOL HotlistManager::SetItemValue( INT32 id, const ItemData& data, BOOL validate, UINT32 flag_changed )
{
	return SetItemValue(GetItemByID(id),data,validate, flag_changed);
}

BOOL HotlistManager::SetItemValue( OpTreeModelItem *item, const ItemData& data, BOOL validate, UINT32 flag_changed )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType( item );

	if( !hmi )
	{
		return FALSE;
	}

	if ( flag_changed & HotlistModel::FLAG_NAME )
	{
		// We do not want to validate the text of a note. The text that
		// is stored in the NAME field is the actual note text which is
		// allowed to contain newlines. Fixes bug #169263 [espen 2007-07-05]
		if( validate && !hmi->IsNote() )
		{
			OpString tmp;
			tmp.Set( data.name );
			ReplaceIllegalSinglelineText( tmp );
			hmi->SetName( tmp.CStr() );
		}
		else
		{
			hmi->SetName( data.name.CStr() );
		}

	}

	if( !hmi->IsFolder() && (flag_changed & HotlistModel::FLAG_URL))
	{
		// Don't tell history / vps about temporary items
		if (!hmi->IsBookmark() || IsClipboard(hmi->GetModel()) || hmi->GetTemporaryModel())
		{
			hmi->SetUrl( data.url.CStr() );
		}
		else
		{
			// If url changes on a bookmark, it must be removed
			// from history and then readded with the new url
			if (hmi->GetUrl().CompareI(data.url) != 0)
			{
				RemoveBookmarkFromHistory((HotlistModelItem*)hmi);
			}
			hmi->SetUrl( data.url.CStr() );

			// Tell history this item was added
			// This will also do SetHistoryItem on the bookmark item
			DesktopHistoryModel::GetInstance()->OnHotlistItemAdded((HotlistModelItem*)item);
		}
	}
	if ( flag_changed & HotlistModel::FLAG_DESCRIPTION )
	{
		hmi->SetDescription( data.description.CStr() );
	}

	if ( flag_changed & HotlistModel::FLAG_NICK )
	{
		if ( validate )
		{
			OpString tmp;
			tmp.Set( data.shortname );
			ReplaceIllegalSinglelineText( tmp );

			hmi->SetShortName( tmp.CStr() );
		}
		else
		{
			hmi->SetShortName( data.shortname.CStr() );
		}
	}

	if ( flag_changed & HotlistModel::FLAG_PERSONALBAR )
	{
		hmi->SetPersonalbarPos( data.personalbar_position );
	}

	if ( flag_changed & HotlistModel::FLAG_PANEL )
	{
		hmi->SetPanelPos( data.panel_position );
	}

	if ( flag_changed & HotlistModel::FLAG_SMALLSCREEN )
		hmi->SetSmallScreen(data.small_screen);


	if ( flag_changed & HotlistModel::FLAG_GUID
		 && data.unique_guid.HasContent())
		{
			if (hmi->GetModel())
				hmi->SetUniqueGUID(data.unique_guid.CStr(), hmi->GetModel()->GetModelType());
			else
				hmi->SetUniqueGUID(data.unique_guid.CStr(), 0);
		}


// 	if(data.visited.Length()) hmi->SetVisited();
// 	if(data.created.Length()) hmi->SetCreated();

	if (hmi->IsContact())
	{
		// Contacts
		hmi->SetMail( data.mail.CStr() );
		hmi->SetPhone( data.phone.CStr() );
		hmi->SetFax( data.fax.CStr() );
		hmi->SetPostalAddress( data.postaladdress.CStr() );
		hmi->SetPictureUrl( data.pictureurl.CStr() );
		hmi->SetIconName( data.image.CStr() );
		hmi->SetConaxNumber( data.conaxnumber.CStr() );
	}

	HotlistModel* model = hmi->GetModel();


	if (model)
	{
		hmi->GetModel()->SetDirty( TRUE );
 		hmi->Change( TRUE );
		
		if (!hmi->GetModel()->GetIsTemporary())
		{
			hmi->GetModel()->BroadcastHotlistItemChanged(hmi, FALSE, flag_changed);
		}
	}

	return TRUE;
}


BOOL HotlistManager::GetItemValue_ByEmail( const OpStringC& email, ItemData& item_data )
{
	OpString tmp;
	tmp.Set( email );
	HotlistModelItem *item = GetContactsModel()->GetByEmailAddress( tmp );

	return GetItemValue( item, item_data );
}


BOOL HotlistManager::Rename( OpTreeModelItem *item, OpString& text )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	if( hmi )
	{
		hmi->SetName( text.CStr() );
		hmi->GetModel()->SetDirty( TRUE );

		INT32 index = hmi->GetModel()->GetIndexByItem(hmi);
		if( index != -1 )
		{
			hmi->GetModel()->BroadcastHotlistItemChanged(hmi, FALSE, HotlistModel::FLAG_NAME);
			hmi->Change(TRUE);
		}

		return TRUE;
	}
	return FALSE;
}

/* OBS: This gives back a list sorted in the order set in preferences, which will be the last one set,
   meaning if you have several windows open with different sortings, the menu in the all of them
   will have its hotlist items sorted as in the last one opened */
void HotlistManager::GetModelItemList(INT32 type, INT32 id, BOOL only_folder_contents, OpVector<HotlistModelItem>& items)
{
	INT32 index = -1;
	HotlistModel *model = 0;

	if( id == 1 || id == 0 )
	{
		INT32 id = GetFirstId(type);
		if( id == -1 )
		{
			return;
		}

		HotlistModelItem *item = GetItemByID(id);
		if( !item )
		{
			return;
		}
		model = item->GetModel();
		index = item->GetModel()->GetIndexByItem( item );
	}
	else
	{
		HotlistModelItem *item = GetItemByID(id);
		if( item )
		{
			if ( item->IsFolder() )
			{
				model = item->GetModel();
				index = item->GetChildIndex();
			}
			else if (!only_folder_contents)
			{
				// the case where the id was an item and not a folder
				items.Add(item);
			}
		}
	}

	if( index != -1 && model )
	{
		INT32 sort_column = -1;
		BOOL sort_ascending = TRUE;

		if (type == HotlistModel::BookmarkRoot)
		{
			// This gets the last one stored.
			sort_column    = g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSortBy);
			sort_ascending = g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSortAsc);

			// Some corrections so that we can keep using settings from 6.x
			if( sort_column > 6 )
			{
				if( sort_column == 16 ) // Unsorted
				{
					sort_column = -1;
					sort_ascending = 1;
				}
				else if( sort_column == 1024 ) // Name
					sort_column = 0;
				else if( sort_column == 2048 ) // Created
					sort_column = 4;
				else if( sort_column == 4096 ) // Visited
					sort_column = 5;
			}
		}
		else if (type == HotlistModel::ContactRoot)
		{
			sort_column = 0;
		}

		model->GetSortedList(index, 0, sort_column, sort_ascending, items);
	}
}

static void ParseAddressList(OpStringC raw_list, OpString& parsed_list)
{
	OpString buf;
	const uni_char *pos = raw_list.CStr();

	while (1)
	{
		const uni_char *prev_pos = pos;
		while (*pos && *pos != ',') pos++;

		buf.Set(prev_pos, pos-prev_pos);
		buf.Strip();

		parsed_list.Append("<");
		parsed_list.Append(buf);
		parsed_list.Append(">");

		if (!*pos)
			break;

		parsed_list.Append(", ");
		pos++;
	}
}

void HotlistManager::GetContactAddresses(INT32 id, OpString &string,
	const OpStringC& email_addresses)
{
	OpVector<HotlistModelItem> items;

	// TODO: Do you really need a sorted list here?
	g_hotlist_manager->GetModelItemList(HotlistModel::ContactRoot, id, FALSE, items);

	for( UINT32 i=0; i < items.GetCount(); i++ )
	{
		HotlistModelItem *hli = items.Get(i);

		if (hli->IsFolder())
			continue;

		if (string.HasContent())
		{
			string.Append(UNI_L(", "));
		}

		const uni_char* name = hli->GetName().CStr();
		const uni_char* mail = 0;

		if (email_addresses.HasContent())
			mail = email_addresses.CStr();
		else
			mail = hli->GetMail().CStr();

		if (name)
		{
			if( uni_strchr(name, ',') )
			{
				string.Append(UNI_L("\""));
				string.Append(name);
				string.Append(UNI_L("\""));
			}
			else
			{
				string.Append(name);
			}
		}

		if (mail)
		{
			if (name)
			{
				OpString parsed_list;
				ParseAddressList(mail, parsed_list);

				string.Append(UNI_L(" "));
				string.Append(parsed_list);
			}
			else
			{
				string.Append(mail);
			}
		}
	}
}

void HotlistManager::GetContactNameList( const OpStringC &pattern, uni_char*** list, INT32* num_item )
{
#define NEW_METHOD_FOR_MATCHING

#ifdef NEW_METHOD_FOR_MATCHING

	if (!pattern.HasContent())
		return;

	const uni_char* string_to_match = pattern.CStr();

	OpVector<uni_char> matched_items;

	HotlistModel& model = GetHotlistModel(HotlistModel::ContactRoot);
	for (INT32 hotlist_index = 0, model_count = model.GetItemCount();
		hotlist_index < model_count; ++hotlist_index)
	{
		HotlistModelItem *item = model.GetItemByIndex(hotlist_index);
		if ((item != 0) && item->IsContact())
		{
#ifdef M2_SUPPORT
			// Don't suggest people we ignore
			if (g_m2_engine->GetIndexById(((ContactModelItem*)(item))->GetM2IndexId()) && g_m2_engine->GetIndexById(((ContactModelItem*)(item))->GetM2IndexId())->IsIgnored())
				continue;
#endif // M2_SUPPORT
			// Get all information about this contact.
			OpAutoVector<OpString> email_addresses;

			const OpStringC& name = item->GetName();
			item->GetMailAddresses(email_addresses);

			// First we try to match on the name.
			if (name.HasContent() && name.FindI(string_to_match) != KNotFound)
			{
				// GetIsInsideTrashFolder is slow. Do not move it
				if (!item->GetIsInsideTrashFolder())
				{
					// We want to add one entry for each e-mail address
					// this contact has.
					uni_char* cur_string;

					for (UINT email_index = 0; email_index < email_addresses.GetCount(); ++email_index)
					{
						const OpStringC& current_email = *email_addresses.Get(email_index);
						int nlen = name.Length();

#ifdef M2_SUPPORT
						if (Header::From::NeedQuotes(name, TRUE))
						{
							cur_string = OP_NEWA(uni_char, nlen + 3);
							if (!cur_string)
								continue;

							uni_sprintf(cur_string, UNI_L("\"%s\""), name.CStr());
						}
						else
#endif
						{
							cur_string = OP_NEWA(uni_char, nlen + 1);
							if (!cur_string)
								continue;

							uni_strcpy(cur_string, name.CStr());
						}

						matched_items.Add(cur_string);

						OpString parsed_email;
						ParseAddressList(current_email, parsed_email);

						if (parsed_email.HasContent())
						{
							cur_string = OP_NEWA(uni_char, parsed_email.Length() + 2);
							if (cur_string)
							{
								uni_sprintf(cur_string, UNI_L(" %s"), parsed_email.CStr());
								matched_items.Add(cur_string);
							}
						}
					}
				}
			}
			else
			{
				// match on the email addresses.
				for (UINT email_index = 0; email_index < email_addresses.GetCount(); ++email_index)
				{
					const OpStringC& current_email = *email_addresses.Get(email_index);

					if (current_email.HasContent() && current_email.FindI(string_to_match) != KNotFound)
					{

						// GetIsInsideTrashFolder is slow. Do not move it
						if (!item->GetIsInsideTrashFolder())
						{
							// We add this email address.
							uni_char* cur_string;
							int nlen = name.Length();

#ifdef M2_SUPPORT
							if (Header::From::NeedQuotes(name, TRUE))
							{
								cur_string = OP_NEWA(uni_char, nlen + 3);
								if (!cur_string)
									continue;

								uni_sprintf(cur_string, UNI_L("\"%s\""), name.CStr());
							}
							else
#endif
							{
								if (nlen)
								{
									cur_string = OP_NEWA(uni_char, nlen + 1);
									if (!cur_string)
										continue;

									uni_strcpy(cur_string, name.CStr());
								}
								else
								{
									cur_string = OP_NEWA(uni_char, current_email.Length() + 1);
									if (!cur_string)
										continue;

									uni_strcpy(cur_string, current_email.CStr());
								}
							}

							matched_items.Add(cur_string);

							OpString parsed_email;
							ParseAddressList(current_email, parsed_email);

							if (parsed_email.HasContent())
							{
								cur_string = OP_NEWA(uni_char, parsed_email.Length() + 2);
								if (cur_string)
								{
									uni_sprintf(cur_string, UNI_L(" %s"), parsed_email.CStr());
									matched_items.Add(cur_string);
								}
							}
						} // Not in trash
					}
				}
			}
		} // Not contact
	}


	// Now we have a nice vector with the matching items
	uni_char** matches = OP_NEWA(uni_char*, matched_items.GetCount());
	if (matches)
	{
		for (UINT32 match_index = 0; match_index < matched_items.GetCount(); ++match_index)
		{
			matches[match_index] = matched_items.Get(match_index);
		}

		*num_item = matched_items.GetCount() / 2;
	}
	else
	{
		matched_items.DeleteAll();
		*num_item = 0;
	}

	*list = matches;

#else

	OpINT32Vector index_list;

	HotlistModel& model = GetHotlistModel(HotlistModel::ContactRoot);

	if( pattern.CStr() )
	{
		const uni_char* string_to_match = pattern.CStr();
		int length = pattern.Length();

		for( INT32 i=0; i< model.GetItemCount(); i++ )
		{
			HotlistModelItem *item = model.GetItemByIndex(i);
			if( item && item->IsContact() )
			{
				const OpStringC& s = item->GetName();
				if( s.CStr() )
				{
					if (uni_strnicoll(s.CStr(), string_to_match, length) == 0)
					{
						// GetIsInsideTrashFolder is slow. Do not move it
						if( !item->GetIsInsideTrashFolder() )
						{
							index_list.Add( i );
						}
						continue;
					}
				}

				if( item->GetMail().CStr() )
				{
					if (uni_strnicoll(item->GetMail().CStr(), string_to_match, length) == 0)
					{
						// GetIsInsideTrashFolder is slow. Do not move it
						if( !item->GetIsInsideTrashFolder() )
						{
							index_list.Add( i );
						}
						continue;
					}
				}
			}
		}
	}

	uni_char** matched_items = 0;
	*num_item = index_list.GetCount();

	if( index_list.GetCount() > 0 )
	{
		matched_items = OP_NEWA(uni_char*, index_list.GetCount() * 2);
		if( matched_items )
		{
			int j=0;
			for( UINT32 i=0; i<index_list.GetCount(); i++ )
			{
				HotlistModelItem *item = model.GetItemByIndex(index_list.Get(i));

				const uni_char * name = item->GetName().CStr();
				matched_items[j] = 0;
				if( name )
				{
					if (Header::From::NeedQuotes(name, TRUE))
					{
						if( matched_items[j] = OP_NEWA(uni_char, uni_strlen(name)+3) )
							uni_sprintf(matched_items[j], UNI_L("\"%s\""), name );
					}
					else
					{
						if( matched_items[j] = OP_NEWA(uni_char, uni_strlen(name)+1) )
							uni_strcpy(matched_items[j], name);
					}
				}
				if( !matched_items[j] )
				{
					if( matched_items[j] = OP_NEWA(uni_char, 1) )
						matched_items[j][0] = 0;
				}

				j++;

				const uni_char * email = item->GetMail().CStr();
				matched_items[j] = 0;
				if( email )
				{
					OpString parsed_list;
					ParseAddressList(email, parsed_list);

					if (parsed_list.HasContent())
					{
						if( matched_items[j] = OP_NEWA(uni_char, parsed_list.Length() + 2) )
							uni_sprintf(matched_items[j], UNI_L(" %s"), parsed_list.CStr());
					}
				}
				if( !matched_items[j] )
				{
					if( matched_items[j] = OP_NEWA(uni_char, 1) )
						matched_items[j][0] = 0;
				}

				j++;
			}
		}
		else
		{
			*num_item = 0;
		}
	}

	*list = matched_items;
#endif
}


BOOL HotlistManager::Reparent( OpTreeModelItem *item, INT32 target_parent_id )
{
	HotlistModelItem* hmi = HotlistModel::GetItemByType(item);
	if(hmi)
	{
		OP_ASSERT(hmi->GetModel()->GetModelType() != HotlistModel::BookmarkRoot);
		HotlistModelItem* target_folder = GetItemByID(target_parent_id);
		if( !target_folder )
		{
			return FALSE;
		}
		else
		{
			return static_cast<HotlistGenericModel*>(hmi->GetModel())->Reparent( hmi, target_folder );
		}
	}

	return FALSE;
}


BOOL HotlistManager::HasNickname(const OpStringC &candidate, HotlistModelItem* exclude_hmi, BOOL exact, BOOL only_one)
{
#if defined(USE_HISTORY_NICK_SEARCH)
	CoreHistoryModelPage* page = 0;
	OP_STATUS status = g_globalHistory->GetBookmarkByNick(candidate, page);
	return (OpStatus::IsSuccess(status) && page);
#else
	OpINT32Vector id_list;
	GetNicknameList(candidate, exact, id_list, exclude_hmi );
	return only_one ? id_list.GetCount() == 1 : id_list.GetCount() > 0;
#endif
}

/**
 * Return list of ids or indices for nick 'candidate'
 *
 * @param use_id - if TRUE - the list contains ids of bookmarks, if FALSE, list contains their indices.
 *
 **/
void HotlistManager::GetNicknameList(const OpStringC &candidate, BOOL exact, OpINT32Vector& id_list, HotlistModelItem* exclude_hmi, BOOL use_id)
{
	if( !candidate.CStr() )
	{
		return;
	}

	HotlistModel& model = GetHotlistModel(HotlistModel::BookmarkRoot);

	for( INT32 i=0; i< model.GetItemCount(); i++ )
	{
		HotlistModelItem *item = model.GetItemByIndex(i);
		if( item && (item->IsBookmark() || item->IsFolder()) )
		{
			if( exclude_hmi && item == exclude_hmi )
			{
				continue;
			}

			const uni_char* name = item->GetShortName().CStr();

			if( name )
			{
				if( exact )
				{
					if( uni_stricmp( name, candidate.CStr() ) == 0 )
					{
						// GetIsInsideTrashFolder is very slow. Do not move it
						if( !item->GetIsInsideTrashFolder() )
						{
							if (use_id)
								id_list.Add( item->GetID() );
							else
								id_list.Add( item->GetIndex() );
						}

					}
				}
				else
				{
					if( uni_stristr( name, candidate.CStr()) == name ) // if matches start of name
					{
						// GetIsInsideTrashFolder is very slow. Do not move it
						if( !item->GetIsInsideTrashFolder() )
						{
							if (use_id)
								id_list.Add( item->GetID() );
							else
								id_list.Add( item->GetIndex() );
						}
					}
				}
			}
		}
	}
}

/**
 * ResolveNickname
 *
 * @param address - nickname
 * @param urls - contains bookmarks corresponding to the nickname
 *  (one if nick for bookmark, may be more if address is a nick for a folder)
 *
 **/
BOOL HotlistManager::ResolveNickname(const OpStringC& address, OpVector<OpString>& urls)
{
#if defined(USE_HISTORY_NICK_SEARCH)
#warning "ResolveNickname Not implemented"
#else
	OpINT32Vector idx_list;
	GetNicknameList(address, /* exact = */ true, idx_list, 0, FALSE /* get indexes */ );

	// Only open if there is exactly one match
	if( idx_list.GetCount() == 1 )
	{
		OpINT32Vector url_list;

		HotlistModel* model = GetBookmarksModel();
		if (!model) return FALSE;

		// Get the folder or bookmark with this nick, if any
		HotlistModelItem* hmi = model->GetItemByIndex(idx_list.Get(0));

		if (!hmi)
			return FALSE;

		if (hmi->IsFolder())
			model->GetIndexList(idx_list.Get(0), url_list, FALSE, 1, OpTypedObject::BOOKMARK_TYPE);
		else
			url_list.Add(idx_list.Get(0));

		for (UINT32 i = 0; i < url_list.GetCount(); i++)
		{
			// Translate from indices of bookmarks to urls
			HotlistModelItem* item = model->GetItemByIndex( url_list.Get(i) );
			OpString* s = OP_NEW(OpString, ());
			s->Set( item->GetResolvedUrl().CStr() );
			if (OpStatus::IsError(urls.Add( s )))
				OP_DELETE(s);
		}
		return TRUE;
	}
#endif
	return FALSE;
}

/*
  Note:
  If exact FALSE; open if there is only one matching nick (start of string matches only one nick)
  Nicks of items in Trash folder are ignored.
*/
BOOL HotlistManager::OpenByNickname(const OpStringC &candidate, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, BOOL exact)
{

	static BOOL busy = FALSE;
	if( busy )
	{
		return FALSE;
	}

	busy = TRUE;

	BOOL success = FALSE;

	OpINT32Vector id_list;

#if defined(USE_HISTORY_NICK_SEARCH)
	if (exact)
	{
		CoreHistoryModelPage* page = 0;
		OP_STATUS status = g_globalHistory->GetBookmarkByNick(candidate, page);


		if (OpStatus::IsSuccess(status) && page)
		{
			OpVector<CoreHistoryModelItem::Listener> result;
			page->GetListenersByType(OpTypedObject::BOOKMARK_TYPE, result);

			// Grab the first, there should really be only one

			CoreHistoryModelItem::Listener* listener = result.Get(0);
			OP_ASSERT(listener);

			if (listener)
			{
				BookmarkModelItem* bm_item = (BookmarkModelItem*) listener;
				id_list.Add(bm_item->GetID());

				success = OpenUrls( id_list, new_window, new_page, in_background);
			}
		}
	}
	else // Not exact match
	{
		OpVector<CoreHistoryModelItem> items;
		OP_STATUS status = g_globalHistory->GetItems(candidate.CStr(), items);
		if (OpStatus::IsSuccess(status) && items.GetCount() > 0)
		{
			UINT32 i;
			BookmarkModelItem* bm_item = NULL;
			BOOL found_one = FALSE;

			for (i = 0; i < items.GetCount(); i++)
			{
				CoreHistoryModelPage* item = (CoreHistoryModelPage*)items.Get(i);
				if (item->IsNick())
				{
					if (found_one) // Already found one item, so there is more than one match
					{
						success = FALSE;
						busy    = FALSE;
						return success;
					}
					else
					{
						OpVector<CoreHistoryModelItem::Listener> result;
						CoreHistoryModelPage* bm_item;

						if (OpStatus::IsError(g_globalHistory->GetBookmarkByNick(candidate.CStr(), bm_item)))
							continue;
						if (OpStatus::IsError(bm_item->GetListenersByType(OpTypedObject::BOOKMARK_TYPE, result)))
							continue;

						// Grab the first, there should really be only one
						CoreHistoryModelItem::Listener* listener = result.Get(0);
						OP_ASSERT(listener);

						if (listener)
						{
							BookmarkModelItem* bookmark_item = (BookmarkModelItem*) listener;
							id_list.Add(bookmark_item->GetID());
							found_one = TRUE;

						}
					}
				}
			}
			if (id_list.GetCount() == 1)
			{
				success = OpenUrls( id_list, new_window, new_page, in_background);
				busy    = FALSE;
				return success;
			}
		}
	}
#else
	GetNicknameList(candidate, exact, id_list, 0 );

	// Only open if there is exactly one match
	if( id_list.GetCount() == 1 )
	{
		success = OpenUrls( id_list, new_window, new_page, in_background );
	}
#endif
	busy = FALSE;

	return success;
}

#ifdef GADGET_SUPPORT
// The three states are: closed, open hidden, open shown
GadgetModelItem::GMI_State  HotlistManager::GetSelectedGadgetsState(OpINT32Vector& id_list, INT32 model_type)
{
	GadgetModelItem::GMI_State state = GadgetModelItem::NoneSelected; // Default is the highest state, None Selected

	// This function WILL modify the id_list to include all items in selected sub folders, so
	// call it only once for the lifetime of the id_list

	//the items should be in this model
	HotlistModel& model = GetHotlistModel(model_type);

	// Loop the entire list checking the state as we go
	for( UINT32 i=0; i < id_list.GetCount(); i++ )
	{
		// Look for the id in the model
		HotlistModelItem *hmi = model.GetItemByID(id_list.Get(i));

		if (hmi)
		{
			if (hmi->IsFolder())
			{
				// There's a folder in the list: Add all children to the id_list
				INT32 index = hmi->GetChildIndex();
				while( index != -1 )
				{
					id_list.Add(static_cast<HotlistModelItem*>(model.GetItemByIndex(index))->GetID());
					index = model.GetSiblingIndex(index);
				}
			}
			else if (model_type == HotlistModel::UniteServicesRoot && hmi->IsUniteService())
			{
				//It's a gadget, but is it running?
				if (static_cast<GadgetModelItem*>(hmi)->IsGadgetRunning())
				{
// #ifdef WEBSERVER_SUPPORT // Hidden state is not used anymore for widgets. Unite Services are always in hidden state
// 					if (static_cast<GadgetModelItem*>(hmi)->GetGadgetStyle() == DesktopGadget::GADGET_STYLE_HIDDEN)
// 					{
// 						// Don't set Hidden if we have already set it to Shown
// 						state = min(state, GadgetModelItem::Hidden);
// 					}
// 					else
// #endif
					{
						state = min(state, GadgetModelItem::Shown);
					}
					
				}
				else
				{
					state = min(state, GadgetModelItem::Closed);
				}
			}
		}
	}

	return state;
}


BOOL HotlistManager::ShowGadgets( const OpINT32Vector& id_list, BOOL show, BOOL only_test_for_visibility_change, INT32 model_type)
{
	//copy the items in to internal list, we might be modifying it to add folder contents
	OpINT32Vector gadget_id_list;
	gadget_id_list.DuplicateOf(id_list);

	//the items should be in this model
	HotlistModel& model = GetHotlistModel(model_type);

	//flag to check if something has been done
	BOOL is_possible = FALSE;
	//loop through the list, check the running status and started/stopped if needed
	for( UINT32 i=0; i < gadget_id_list.GetCount(); i++ )
	{
		//look for the id in the model, and run/stop the gadget if needed
		HotlistModelItem *hmi = model.GetItemByID(gadget_id_list.Get(i));

		if (hmi)
		{
			if (hmi->IsFolder())
			{
				// There's a folder in the list: Add all children to the gadget_id_list
				INT32 index = hmi->GetChildIndex();
				while( index != -1 )
				{
					gadget_id_list.Add(static_cast<HotlistModelItem*>(model.GetItemByIndex(index))->GetID());
					index = model.GetSiblingIndex( index );
				}
			}
			else if (model_type == HotlistModel::UniteServicesRoot && hmi->IsUniteService())
			{
				//It's a gadget
					if (only_test_for_visibility_change)
					{

						// Here we lie a bit. It is only really possible if this gadget is not
						// (yet) in the desired state. But reshowing an active widget is actually possible
						// because it will activate (move input focus/window to front) it again.
						if (static_cast<GadgetModelItem*>(hmi)->IsGadgetRunning() != show)
						{
							is_possible = TRUE;
					 	}
					}
					else
					{
						//show/activate or hide the gadget, always possible
						static_cast<GadgetModelItem*>(hmi)->ShowGadget(show);
						is_possible = TRUE;
					}


			}
		}
	}
	return is_possible;
}


void HotlistManager::OpenGadgets(const OpINT32Vector& id_list, BOOL open, INT32 model_type)
{
	//the items should be in this model
	HotlistModel& model = GetHotlistModel(model_type);

	for( UINT32 i=0; i < id_list.GetCount(); i++ )
	{
		//look for the id in the model, and run/stop the gadget if needed
		HotlistModelItem *hmi = model.GetItemByID(id_list.Get(i));

		if (hmi)
		{
			if (!hmi->IsFolder())
			{
				static_cast<GadgetModelItem*>(hmi)->ShowGadget(open, TRUE);
#ifdef WEBSERVER_SUPPORT
				if (hmi->IsUniteService())
				{
					OpGadget* op_gadget = (static_cast<UniteServiceModelItem*>(hmi))->GetOpGadget();
					if (op_gadget)
					{
						if (open &&  GetDesktopGadgetManager())
							GetDesktopGadgetManager()->AddAutoStartService(op_gadget->GetIdentifier());
						else if (GetDesktopGadgetManager())
							GetDesktopGadgetManager()->RemoveAutoStartService(op_gadget->GetIdentifier());
					}
				}
#endif // WEBSERVER_SUPPORT
			}
		}
	}
}

#endif // GADGET_SUPPORT

#ifdef GADGET_SUPPORT
BOOL HotlistManager::OpenGadget(INT32 id)
{
	OpINT32Vector id_list;
	id_list.Add( id );

	HotlistModelItem* item = GetItemByID(id);
	if (item && item->GetModel())
	{
		return ShowGadgets( id_list, TRUE, FALSE, item->GetModel()->GetModelType());
	}
	else
	{
		return FALSE;
	}
}
#endif // GADGET_SUPPORT

#ifdef GADGET_SUPPORT
BOOL HotlistManager::OpenGadget( const OpStringC& gadget_id, DesktopWindow** window, INT32 model_type)
{
	HotlistModelItem* hmi = ((HotlistGenericModel&)GetHotlistModel(model_type)).GetByGadgetIdentifier( gadget_id );
	if (!hmi)
		return FALSE;	//couldn't find a gadget with the id
	OP_STATUS ret = OpStatus::ERR;
	if (hmi->IsGadget())
		ret = static_cast<GadgetModelItem*>(hmi)->ShowGadget(TRUE);
	//return window if desired
	if (OpStatus::IsSuccess(ret) && window)
		*window = static_cast<GadgetModelItem*>(hmi)->GetDesktopGadget();
	return OpStatus::IsSuccess(ret);
}
#endif // GADGET_SUPPORT

BOOL HotlistManager::OpenContact( INT32 id, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window, INT32 position )
{
	OpINT32Vector id_list;
	id_list.Add( id );
	return OpenContacts( id_list, new_window, new_page, in_background, target_window, position );
}


BOOL HotlistManager::OpenContacts( const OpINT32Vector& id_list, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window, INT32 position )
{
	OpenURLSetting setting;
	setting.m_new_window    = new_window;
	setting.m_new_page      = new_page;
	setting.m_in_background = in_background;
	setting.m_target_window = target_window;
	setting.m_target_position = position;

	BOOL shift = FALSE;
	BOOL control = FALSE;
	if( setting.m_new_window == MAYBE )
	{
		setting.m_new_window = (shift && !control) ? YES : NO;
	}
	if( setting.m_new_page == MAYBE )
	{
		setting.m_new_page = (shift && !control) ? YES : NO;
	}
	if( setting.m_in_background == MAYBE )
	{
		setting.m_in_background = (shift && control) ? YES : NO;
	}

	HotlistModel& model = GetHotlistModel(HotlistModel::ContactRoot);

	UINT32 i;
	OpINT32Vector check_list;
	for( i=0; i<id_list.GetCount(); i++ )
	{
		// Verify that we do not add a child of another
		INT32 index = model.GetIndexByItem(GetItemByID(id_list.Get(i)));
		if( index != -1 )
		{
			BOOL add = TRUE;
			for( UINT32 j=0; j<check_list.GetCount(); j++ )
			{
				if( model.IsChildOf(check_list.Get(j), index) )
				{
					add = FALSE;
					break;
				}
			}
			if( add )
			{
				check_list.Add( index );
			}
		}
	}

	OpINT32Vector index_list;
	for( i=0; i<check_list.GetCount(); i++ )
	{
		HotlistModelItem* hmi = model.GetItemByIndex(check_list.Get(i));
		int probe_depth = hmi && hmi->IsFolder() ? 1 : 0;

		model.GetIndexList(check_list.Get(i), index_list, FALSE, probe_depth, OpTypedObject::CONTACT_TYPE);
	}

	UINT32 limit = g_pcui->GetIntegerPref(PrefsCollectionUI::ConfirmOpenBookmarkLimit);
	if( limit > 0 && index_list.GetCount() > limit )
	{
		OpString message, title;

		g_languageManager->GetString(Str::S_ABOUT_TO_OPEN_X_CONTACTS, message);
		g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_CONTACTS, title);

		BOOL do_not_show_again = FALSE;

		int result = SimpleDialog::ShowDialogFormatted(WINDOW_NAME_OPEN_NUM_CONTACTS, target_window, message.CStr(), title.CStr(), Dialog::TYPE_OK_CANCEL, Dialog::IMAGE_INFO, &do_not_show_again, index_list.GetCount() );

		if (do_not_show_again)
		{
			g_pcui->WriteIntegerL(PrefsCollectionUI::ConfirmOpenBookmarkLimit, 0);
		}

		if( result != Dialog::DIALOG_RESULT_OK )
		{
			return FALSE;
		}
	}

	for( i=0; i<index_list.GetCount(); i++ )
	{
		HotlistModelItem* item = model.GetItemByIndex(index_list.Get(i));
		if( item )
		{
			OpString candidate;
			candidate.Set( item->GetMail() );
			if( candidate.IsEmpty() )
			{
				continue;
			}

#ifdef M2_SUPPORT
			g_application->GoToMailView( 0, candidate.CStr(), NULL, TRUE, setting.m_new_page == YES,  TRUE );
			setting.m_new_page = YES;
#endif

		}
	}

	return TRUE;
}




BOOL HotlistManager::OpenUrl( INT32 id, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window, INT32 position, DesktopWindowCollectionItem* parent, BOOL ignore_modifier_keys)
{
	OpINT32Vector id_list;
	id_list.Add( id );
	return OpenUrls( id_list, new_window, new_page, in_background, target_window, position, parent, ignore_modifier_keys);
}


/**
 * HotlistManager::OpenUrls
 * // TODO:
 *
 **/
BOOL HotlistManager::OpenUrls( const OpINT32Vector& id_list, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window, INT32 position, DesktopWindowCollectionItem* parent, BOOL ignore_modifier_keys)
{

	OpenURLSetting setting;
	setting.m_new_window    = new_window;
	setting.m_new_page      = new_page;
	setting.m_in_background = in_background;
	setting.m_target_window = target_window;
	setting.m_target_position = position;
	setting.m_target_parent = parent;
	setting.m_ignore_modifier_keys = ignore_modifier_keys;

	// The model of the first id determines what model to use for all
	HotlistModel *probe_model = 0;
	if( id_list.GetCount() > 0 )
	{
		HotlistModelItem* hmi = HotlistManager::GetItemByID( id_list.Get(0) );
		if( hmi )
		{
			probe_model = hmi->GetModel();

		}
	}

	HotlistModel& model = probe_model ? *probe_model : GetHotlistModel(HotlistModel::BookmarkRoot);

	UINT32 i;
	OpINT32Vector check_list; // Items from index_list but only those not children of each other
	OpINT32Vector index_list;

	if( id_list.GetCount() == 1 && id_list.Get(0) == HotlistModel::BookmarkRoot )
	{
		// This must be done better [espen]
		model.GetIndexList( 0, index_list, TRUE, 0, OpTypedObject::BOOKMARK_TYPE);
	}
	else
	{
		for( i=0; i<id_list.GetCount(); i++ )
		{
			// Verify that we do not add a child of another
			INT32 index = model.GetIndexByItem(GetItemByID(id_list.Get(i)));
			if( index != -1 )
			{
				BOOL add = TRUE;
				for( UINT32 j=0; j<check_list.GetCount(); j++ )
				{
					if( model.IsChildOf(check_list.Get(j), index) )
					{
						add = FALSE;
						break;
					}
				}
				if( add )
				{
					check_list.Add( index );
				}
			}
		}

		for( i=0; i<check_list.GetCount(); i++ )
		{
			HotlistModelItem* hmi = model.GetItemByIndex(check_list.Get(i));
			int probe_depth = hmi && hmi->IsFolder() ? 1 : 0;

			model.GetIndexList(check_list.Get(i), index_list, FALSE, probe_depth, OpTypedObject::BOOKMARK_TYPE);
			if( index_list.GetCount() == 0 )
			{
				// This must be done better [espen]
				model.GetIndexList(check_list.Get(i), index_list, FALSE, probe_depth, OpTypedObject::CONTACT_TYPE);
			}
		}
	}

	INT32 limit = g_pcui->GetIntegerPref(PrefsCollectionUI::ConfirmOpenBookmarkLimit);
	if( limit > 0 && index_list.GetCount() > (UINT32)limit )
	{
		OpString message, title;

		g_languageManager->GetString(Str::S_ABOUT_TO_OPEN_X_BOOKMARKS, message);
		g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_BOOKMARKS, title);

		BOOL do_not_show_again = FALSE;

		int result = SimpleDialog::ShowDialogFormatted(WINDOW_NAME_CONFIRM_OPEN_BOOKMARK_LIMIT, target_window, message.CStr(), title.CStr(), Dialog::TYPE_OK_CANCEL, Dialog::IMAGE_INFO, &do_not_show_again, index_list.GetCount() );

		if (do_not_show_again)
		{
			g_pcui->WriteIntegerL(PrefsCollectionUI::ConfirmOpenBookmarkLimit, -limit);
		}

		if( result != Dialog::DIALOG_RESULT_OK )
		{
			return FALSE;
		}
	}

	// Potential longjmp (TRAP on Unix) clobberate of model forces us to mimic it here:
	HotlistModel& remodel = probe_model ? *probe_model : GetHotlistModel(HotlistModel::BookmarkRoot);

	for( i=0; i<index_list.GetCount(); i++ )
	{
		HotlistModelItem* item = remodel.GetItemByIndex(index_list.Get(i));
		if( item )
		{
			setting.m_address.Set( item->GetUrl() );
			g_application->OpenURL( setting );
			if( item->GetUpgradeIcon() )
			{
				item->SetUpgradeIcon(FALSE);
				//FIXME: Temporary fix for bug #DSK-267730 (Too many favicon requests sent to redir.opera.com)
				//g_favicon_manager->LoadBookmarkIcon(item->GetUrl(), item->GetIconFile());
			}

			// We're opening more than one url, need new page for next one
		 	if (new_page != YES && new_window != YES)
 				setting.m_new_page = YES;

			// Increment position if set so that we open new tabs to the right
			if (setting.m_target_position >= 0)
				setting.m_target_position ++;

		}
	}

	return TRUE;
}

/**
 * HotlistManager::GetOpenUrls
 *
 **/
BOOL HotlistManager::GetOpenUrls( OpINT32Vector& index_list, const OpINT32Vector& id_list)
{

	// The model of the first id determines what model to use for all
	HotlistModel *probe_model = 0;
	if( id_list.GetCount() > 0 )
	{
		HotlistModelItem* hmi = HotlistManager::GetItemByID( id_list.Get(0) );
		if( hmi )
		{
			probe_model = hmi->GetModel();

		}
	}

	HotlistModel& model = probe_model ? *probe_model : GetHotlistModel(HotlistModel::BookmarkRoot);

	UINT32 i;
	OpINT32Vector check_list; // Items from index_list but only those not children of each other

	if( id_list.GetCount() == 1 && id_list.Get(0) == HotlistModel::BookmarkRoot )
	{
		// This must be done better [espen]
		model.GetIndexList( 0, index_list, TRUE, 0, OpTypedObject::BOOKMARK_TYPE);
	}
	else
	{
		for( i=0; i<id_list.GetCount(); i++ )
		{
			// Verify that we do not add a child of another
			INT32 index = model.GetIndexByItem(GetItemByID(id_list.Get(i)));
			if( index != -1 )
			{
				BOOL add = TRUE;
				for( UINT32 j=0; j<check_list.GetCount(); j++ )
				{
					if( model.IsChildOf(check_list.Get(j), index) )
					{
						add = FALSE;
						break;
					}
				}
				if( add )
				{
					check_list.Add( index );
				}
			}
		}

		for( i=0; i<check_list.GetCount(); i++ )
		{
			HotlistModelItem* hmi = model.GetItemByIndex(check_list.Get(i));
			int probe_depth = hmi && hmi->IsFolder() ? 1 : 0;

			model.GetIndexList(check_list.Get(i), index_list, FALSE, probe_depth, OpTypedObject::BOOKMARK_TYPE);
			if( index_list.GetCount() == 0 )
			{
				// This must be done better [espen]
				model.GetIndexList(check_list.Get(i), index_list, FALSE, probe_depth, OpTypedObject::CONTACT_TYPE);
			}
		}
	}
	return TRUE;
}

/**
 * HotlistManager::OpenOrderedUrls
 *
 * Opens the urls represented by index_list in the order given by the list
 *
 **/
BOOL HotlistManager::OpenOrderedUrls( const OpINT32Vector& index_list, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, DesktopWindow* target_window, INT32 position, DesktopWindowCollectionItem* parent, BOOL ignore_modifier_keys)
{

	OpenURLSetting setting;
	setting.m_new_window    = new_window;
	setting.m_new_page      = new_page;
	setting.m_in_background = in_background;
	setting.m_target_window = target_window;
	setting.m_target_position = position;
	setting.m_target_parent = parent;
	setting.m_ignore_modifier_keys = ignore_modifier_keys;

	INT32 limit = g_pcui->GetIntegerPref(PrefsCollectionUI::ConfirmOpenBookmarkLimit);
	if( limit > 0 && index_list.GetCount() > (UINT32)limit )
	{
		OpString message, title;

		g_languageManager->GetString(Str::S_ABOUT_TO_OPEN_X_BOOKMARKS, message);
		g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_BOOKMARKS, title);

		BOOL do_not_show_again = FALSE;

		int result = SimpleDialog::ShowDialogFormatted(WINDOW_NAME_CONFIRM_OPEN_BOOKMARK_LIMIT, target_window, message.CStr(), title.CStr(), Dialog::TYPE_OK_CANCEL, Dialog::IMAGE_INFO, &do_not_show_again, index_list.GetCount() );

		if (do_not_show_again)
		{
			g_pcui->WriteIntegerL(PrefsCollectionUI::ConfirmOpenBookmarkLimit, -limit);
		}

		if( result != Dialog::DIALOG_RESULT_OK )
		{
			return FALSE;
		}
	}

	// Potential longjmp (TRAP on Unix) clobberate of model forces us to mimic it here:
	HotlistModel& remodel = GetHotlistModel(HotlistModel::BookmarkRoot);

	for( UINT32 i=0; i<index_list.GetCount(); i++ )
	{
		HotlistModelItem* item = remodel.GetItemByIndex(index_list.Get(i));
		if (item)
		{
			setting.m_address.Set(item->GetUrl());
			setting.m_is_hotlist_url = TRUE;
			g_application->OpenURL(setting);
			if( item->GetUpgradeIcon() )
			{
				item->SetUpgradeIcon(FALSE);
				//FIXME: Temporary fix for bug #DSK-267730 (Too many favicon requests sent to redir.opera.com)
				//g_favicon_manager->LoadBookmarkIcon(item->GetUrl(), item->GetIconFile());
			}

			// We're opening more than one url, need new page for next one
		 	if (new_page != YES && new_window != YES)
 				setting.m_new_page = YES;
		}
	}

	return TRUE;
}


void HotlistManager::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (result.files.GetCount())
	{
		switch (GetFileChoosingContext())
		{
		case OPEN_FILE_CHOOSING:
			ChosenNewOrOpen(choosing_data.type, FALSE, result.files.Get(0));
			break;
		case NEW_FILE_CHOOSING:
			ChosenNewOrOpen(choosing_data.type, TRUE, result.files.Get(0));
			break;
		case SAVEAS_FILE_CHOOSING:
			ChosenSaveAs(choosing_data.type, choosing_data.mode, result.files.Get(0));
			break;
		case SAVESELECTEDAS_FILE_CHOOSING:
			ChosenSaveSelectedAs(choosing_data.type, choosing_data.mode, choosing_data.id_list, result.files.Get(0));
			choosing_data.id_list = NULL;
			break;
		case IMPORT_FILE_CHOOSING:
			ChosenImport(choosing_data.type, choosing_data.format, choosing_data.into_root, choosing_data.show_import,
						 choosing_data.model, result.files.Get(0));
			break;
		default:
			break;
		}
	}
	ResetFileChoosingContext();
	OP_DELETE(m_request);
	m_request = NULL;
}


void HotlistManager::ShowMaximumReachedDialog(HotlistModelItem* hmi)
{
	if(hmi)
	{
		OpString title, message, format;

		g_languageManager->GetString(Str::D_BOOKMARK_PROP_MAX_ITEMS_TITLE, title);
		g_languageManager->GetString(Str::D_BOOKMARK_PROP_MAX_ITEMS_TEXT, format);
		message.AppendFormat(format.CStr(), hmi->GetMaxItems());

		SimpleDialog::ShowDialog(WINDOW_NAME_MAX_BOOKMARKS_REACHED, g_application->GetActiveDesktopWindow(), message.CStr(), title.CStr(), Dialog::TYPE_OK, Dialog::IMAGE_INFO);
	}
}

BOOL HotlistManager::ShowSavingFailureDialog(const OpString& filename)
{
	OpString message, title, tmp;
	g_languageManager->GetString(Str::D_SAVING_FAILED_TITLE,title);
	// Could not save %s file (bookmarks/notes/contacts/widgets)
	g_languageManager->GetString(Str::D_HOTLIST_SAVE_FAILED,message);
	message.Append("\n\n");
	message.Append(filename);
	message.Append("\n\n");
	g_languageManager->GetString(Str::D_TRY_AGAIN_QUESTION,tmp);
	message.Append(tmp);

	return Dialog::DIALOG_RESULT_YES == SimpleDialog::ShowDialog(WINDOW_NAME_SAVE_FAILURE,
		0, message.CStr(), title.CStr(), Dialog::TYPE_YES_NO, Dialog::IMAGE_WARNING );
}

void HotlistManager::OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child)
{
#ifdef GADGET_SUPPORT
	// If item is in Trash; Uninstall gadget and remove icon file
	if(item->GetIsInsideTrashFolder())
	{
		//uninstall the gadget when deleted from trash, and there is no other reference left
		if ((item->IsGadget() || item->IsUniteService()) && (static_cast<HotlistGenericModel*>(item->GetModel())->GetGadgetCountByIdentifier(item->GetGadgetIdentifier()) == 1))
		{
			static_cast<GadgetModelItem*>(item)->UninstallGadget();
		}
	}
#endif // GADGET_SUPPORT
}

DesktopGadgetManager * HotlistManager::GetDesktopGadgetManager()
{
	return g_desktop_gadget_manager; 
}

OperaAccountManager * HotlistManager::GetOperaAccountManager()
{
	return g_desktop_account_manager;
}

Hotlist * HotlistManager::GetHotlist()
{
	return g_application->GetActiveBrowserDesktopWindow() ? g_application->GetActiveBrowserDesktopWindow()->GetHotlist() : NULL;
}

BOOL HotlistManager::AskEnterOnlineMode(BOOL test_offline_mode_first)
{
	return g_application->AskEnterOnlineMode(test_offline_mode_first);
}

BOOL HotlistManager::ShowPanelByName(const uni_char* name, BOOL show, BOOL focus)
{
	Hotlist* hotlist = GetHotlist();
	if (hotlist)
		return hotlist->ShowPanelByName(name,show,focus);
	return FALSE;
}
