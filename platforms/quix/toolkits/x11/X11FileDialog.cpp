/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Espen Sand
 */
#include "core/pch.h"

#include "platforms/quix/toolkits/x11/X11FileDialog.h"

#include "platforms/posix/posix_native_util.h"
#include "platforms/posix/posix_file_util.h"
#include "platforms/quix/toolkits/x11/X11FileChooser.h"
#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/common/unix_opdesktopresources.h"
#include "platforms/unix/product/x11quick/iconsource.h"
#include "platforms/unix/product/x11quick/popupmenu.h"
#include "platforms/unix/product/x11quick/popupmenuitem.h"
#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/util/opfile/opfile.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpDropDown.h"

static const UINT32 FOLDER_TAG = 0;
static const UINT32 FILE_TAG = 1;
#define DIALOG_PATH_INI_FILE UNI_L("filedialogpaths.ini")
#define FOLDER_IMAGE "Folder"
#define DOCUMENT_IMAGE "Attachment Documents"

INT32 X11FileDialog::PlaceItem::next_unused_id = 1; // Do not use 0 as first, it has a special meaning.


OP_STATUS FileListModel::GetColumnData(OpTreeModel::ColumnData* column_data)
{
	OP_STATUS rc = SimpleTreeModel::GetColumnData(column_data);
	if (column_data->column == 1 || column_data->column == 2)
		column_data->custom_sort = TRUE;
	return rc;
}

INT32 FileListModel::CompareItems(INT32 column, OpTreeModelItem* item1, OpTreeModelItem* item2)
{
	if (column == 1)
	{
		SimpleTreeModelItem* s1 = static_cast<SimpleTreeModelItem*>(item1);
		SimpleTreeModelItem* s2 = static_cast<SimpleTreeModelItem*>(item2);

		FileListModel::UserData* ud1 = static_cast<FileListModel::UserData*>(s1->GetUserData());
		FileListModel::UserData* ud2 = static_cast<FileListModel::UserData*>(s2->GetUserData());

		if (ud1->size < ud2->size)
			return -1;
		else if( ud1->size > ud2->size)
			return 1;
		else
			return ud1->text.CompareI(ud2->text);
	}
	else if (column == 2)
	{
		SimpleTreeModelItem* s1 = static_cast<SimpleTreeModelItem*>(item1);
		SimpleTreeModelItem* s2 = static_cast<SimpleTreeModelItem*>(item2);

		FileListModel::UserData* ud1 = static_cast<FileListModel::UserData*>(s1->GetUserData());
		FileListModel::UserData* ud2 = static_cast<FileListModel::UserData*>(s2->GetUserData());

		if (ud1->type > ud2->type)
			return 1;
		else if (ud1->type < ud2->type)
			return -1;
		else if (ud1->time < ud2->time)
			return -1;
		else if (ud1->time > ud2->time)
			return 1;
		else
			return 0;
	}

	return 0;
}

X11FileDialog::X11FileDialog(X11FileChooserSettings& settings)
	: m_settings(settings)
	, m_paths_model(settings.type == ToolkitFileChooser::DIRECTORY ? 1 : 3)
	, m_places_model(1)
	, m_paths_tree_view(0)
	, m_places_tree_view(0)
	, m_back_image(0)
	, m_up_image(0)
	, m_cross_image(0)
	, m_num_fixed_places(0)
	, m_is_editing(FALSE)
	, m_block_path_change(BM_Ignore)
{
}


X11FileDialog::~X11FileDialog()
{
	TRAPD(rc,WritePlacesL());
	m_userdata_list.DeleteAll();
	m_undo_list.DeleteAll();
	m_places.DeleteAll();
	OP_DELETE(m_back_image);
	OP_DELETE(m_up_image);
	OP_DELETE(m_cross_image);
}


void X11FileDialog::OnInitVisibility()
{
	if (IsDirectorySelector())
	{
		ShowWidget("Type_label", FALSE);
		ShowWidget("Type_dropdown", FALSE);
	}
}

void X11FileDialog::OnInit()
{
	PlaceItem::next_unused_id = 1;

	SetTitle(m_settings.caption);

	OpString text;
	g_languageManager->GetString(Str::D_FILEDIALOG_PLACES, text);
	m_places_model.SetColumnData(0, text);
	g_languageManager->GetString(Str::D_FILEDIALOG_NAME, text);
	m_paths_model.SetColumnData(0, text);
	if (!IsDirectorySelector())
	{
		g_languageManager->GetString(Str::D_FILEDIALOG_SIZE, text);
		m_paths_model.SetColumnData(1, text);
		g_languageManager->GetString(Str::D_FILEDIALOG_MODIFIED, text);
		m_paths_model.SetColumnData(2, text);
	}

	m_places_tree_view = static_cast<OpTreeView*>(GetWidgetByName("Places_treeview"));
	m_paths_tree_view = static_cast<OpTreeView*>(GetWidgetByName("Paths_treeview"));

	if (m_places_tree_view)
	{
		m_places_tree_view->SetTreeModel(&m_places_model);
		m_places_tree_view->SetUserSortable(FALSE);
		m_places_tree_view->SetDragEnabled(TRUE);
	}

	if (m_paths_tree_view)
	{
		m_paths_tree_view->SetTreeModel(&m_paths_model);
		m_paths_tree_view->Sort(0, TRUE);
		m_paths_tree_view->SetDragEnabled(TRUE);
		m_paths_tree_view->SetMultiselectable(m_settings.type == ToolkitFileChooser::FILE_OPEN_MULTI);
	}

	UINT8* data;
	UINT32 size;
	OpButton* back_button = static_cast<OpButton*>(GetWidgetByName("Back_button"));
	if (back_button)
	{
		IconSource::GetLeftArrow(data, size);
		m_back_image = OP_NEW(SimpleFileImage,(data, size));
		if (m_back_image)
		{
			OpWidgetImage widget_image;
			Image img = m_back_image->GetImage();
			widget_image.SetBitmapImage(img);
			back_button->SetButtonStyle(OpButton::STYLE_IMAGE);
			back_button->GetForegroundSkin()->SetWidgetImage(&widget_image);
		}
	}

	OpButton* up_button = static_cast<OpButton*>(GetWidgetByName("Up_button"));
	if (up_button)
	{
		IconSource::GetUpArrow(data, size);
		m_up_image = OP_NEW(SimpleFileImage,(data, size));
		if (m_up_image)
		{
			OpWidgetImage widget_image;
			Image img = m_up_image->GetImage();
			widget_image.SetBitmapImage(img);
			up_button->SetButtonStyle(OpButton::STYLE_IMAGE);
			up_button->GetForegroundSkin()->SetWidgetImage(&widget_image);
		}
	}

	OpButton* folder_button = static_cast<OpButton*>(GetWidgetByName("Folder_button"));
	if (folder_button)
	{
		IconSource::GetCross(data, size);
		m_cross_image = OP_NEW(SimpleFileImage,(data, size));
		if (m_cross_image)
		{
			OpWidgetImage widget_image;
			Image img = m_cross_image->GetImage();
			widget_image.SetBitmapImage(img);
			folder_button->SetButtonStyle(OpButton::STYLE_IMAGE);
			folder_button->GetForegroundSkin()->SetWidgetImage(&widget_image);
		}
	}

	// Fixed places that can not be modified
	UnixOpDesktopResources::Folders folders;
	UnixOpDesktopResources::GetHomeFolder(folders.home);
	UnixOpDesktopResources::SetupPredefinedFolders(folders);

	OpString name;
	g_languageManager->GetString(Str::D_FILEDIALOG_ENTRY_HOME, name);
	AddPlace(name, OPFILE_FOLDER_COUNT, UNI_L("~"), PlaceItem::FixedFolder);
	if (folders.desktop.HasContent())
	{
		g_languageManager->GetString(Str::D_FILEDIALOG_ENTRY_DESKTOP, name);
		AddPlace(name, OPFILE_DESKTOP_FOLDER, folders.desktop, PlaceItem::FixedFolder);
	}
	g_languageManager->GetString(Str::D_FILEDIALOG_ENTRY_FILE_SYSTEM, name);
	AddPlace(name, OPFILE_FOLDER_COUNT, UNI_L("/"), PlaceItem::FixedFolder);
	m_places_model.AddSeparator();

	INT32 num_places = m_places_model.GetItemCount();

	if (folders.documents.HasContent())
	{
		g_languageManager->GetString(Str::D_FILEDIALOG_ENTRY_DOCUMENTS, name);
		AddPlace(name, OPFILE_DOCUMENTS_FOLDER, folders.documents, PlaceItem::FixedFolder);
	}
	if (folders.download.HasContent())
	{
		g_languageManager->GetString(Str::D_FILEDIALOG_ENTRY_DOWNLOADS, name);
		AddPlace(name, OPFILE_DOWNLOAD_FOLDER, folders.download, PlaceItem::FixedFolder);
	}
	if (folders.pictures.HasContent())
	{
		g_languageManager->GetString(Str::D_FILEDIALOG_ENTRY_PICTURES, name);
		AddPlace(name, OPFILE_PICTURES_FOLDER, folders.pictures, PlaceItem::FixedFolder);
	}
	if (folders.videos.HasContent())
	{
		g_languageManager->GetString(Str::D_FILEDIALOG_ENTRY_VIDEOS, name);
		AddPlace(name, OPFILE_VIDEOS_FOLDER, folders.videos, PlaceItem::FixedFolder);
	}
	if (folders.music.HasContent())
	{
		g_languageManager->GetString(Str::D_FILEDIALOG_ENTRY_MUSIC, name);
		AddPlace(name, OPFILE_MUSIC_FOLDER, folders.music, PlaceItem::FixedFolder);
	}
	if (num_places < m_places_model.GetItemCount())
		m_places_model.AddSeparator();

	m_num_fixed_places = m_places_model.GetItemCount();

	// User configurable places
	TRAPD(rc,ReadPlacesL());

	OpString filename;
	if (IsDirectorySelector())
	{
		filename.Set(m_settings.path);
	}
	else
	{
		// Use first filter entry if calling code has not made up its mind
		if(m_settings.activefilter<0)
			m_settings.activefilter = 0;

		OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("Type_dropdown"));
		if (dropdown)
		{
			dropdown->SetListener(this);

			OpHashIterator*	it = m_settings.filter.GetIterator();
			if (it && OpStatus::IsSuccess(it->First()))
			{
				do
				{
					X11FileChooserSettings::FilterData* f = static_cast<X11FileChooserSettings::FilterData*>(it->GetData());
					if (f->media.HasContent() && f->extension.HasContent())
					{
						OpString tmp;
						tmp.AppendFormat(UNI_L("%s (%s)"), f->media.CStr(), f->extension.CStr());
						dropdown->AddItem(tmp.CStr());
					}
				}
				while(OpStatus::IsSuccess(it->Next()));
			}

			if (dropdown->CountItems() > m_settings.activefilter)
				dropdown->SelectItem(m_settings.activefilter, TRUE);
		}


		// We have seen an initial path name of the form 'filename.*'. Should it
		// occur we replace the broken extension with the first extension in our list
		int pos = m_settings.path.Find(UNI_L(".*"));
		if (pos != KNotFound && pos == m_settings.path.Length()-2)
		{
			X11FileChooserSettings::FilterData* f=0;
			if (OpStatus::IsSuccess(m_settings.filter.GetData(0, &f)))
			{
				int pos = f->extension.FindLastOf('.');
				if (pos != KNotFound)
				{
					OpString ext;
					ext.Set(&f->extension.CStr()[pos]);

					pos = m_settings.path.FindLastOf('.');
					if (pos != KNotFound)
						m_settings.path.Delete(pos);
					m_settings.path.Append(ext);
				}
				else
				{
					pos = m_settings.path.FindLastOf('.');
					if (pos != KNotFound)
						m_settings.path.Delete(pos);
				}
			}
		}

		OpString dn, fn;
		if (UnixUtils::SplitDirectoryAndFilename(m_settings.path, dn, fn))
		{
			m_settings.path.Set(dn);
			filename.Set(fn);
		}
		else
		{
			m_settings.path.Set("/");
			filename.Empty();
		}
	}

	if (m_places_tree_view)
	{
		for (INT32 i=0; i<m_places_model.GetCount(); i++)
		{
			PlaceItem* item = static_cast<PlaceItem*>(m_places_model.GetItemUserData(i));
			if (item && item->path.HasContent())
			{
				if (!item->path.Compare(m_settings.path))
				{
					m_places_tree_view->SetSelectedItemByID(item->id);
					break;
				}
			}
		}
	}

	SetDirectory(m_settings.path, FALSE, FALSE);
	SetWidgetText("Edit_field", filename);
	SetWidgetFocus("Edit_field");
}


void X11FileDialog::ReadPlacesL()
{
	OpFile file; ANCHOR(OpFile, file);
	LEAVE_IF_ERROR(file.Construct( DIALOG_PATH_INI_FILE, OPFILE_HOME_FOLDER));

	BOOL exists;
	if (OpStatus::IsSuccess(file.Exists(exists)) && exists)
	{
		OpStackAutoPtr<PrefsFile> prefs(OP_NEW_L(PrefsFile, (PREFS_STD)));
		ANCHOR(OpStackAutoPtr<PrefsFile>, prefs);
		prefs->ConstructL();
		prefs->SetFileL(&file);
		prefs->LoadAllL();

		const uni_char* section = UNI_L("Paths");

		OpString key; ANCHOR(OpString, key);
		int path_num = prefs->ReadIntL(section, UNI_L("Items"));
		for (int i=0; i<path_num; i++)
		{
			OpStringC name, path;
			key.Empty();
			key.AppendFormat(UNI_L("Name%d"), i);
			if (key.HasContent())
				name = prefs->ReadStringL(section, key);
			key.Empty();
			key.AppendFormat(UNI_L("Path%d"), i);
			if (key.HasContent())
				path = prefs->ReadStringL(section, key);

			if (name.HasContent() && path.HasContent())
				AddPlace(name, OPFILE_FOLDER_COUNT, path.CStr(), PlaceItem::CustomFolder);
		}
	}
}


void X11FileDialog::WritePlacesL()
{
	OpFile file; ANCHOR(OpFile, file);
	LEAVE_IF_ERROR(file.Construct( DIALOG_PATH_INI_FILE, OPFILE_HOME_FOLDER));
	OpStackAutoPtr<PrefsFile> prefs(OP_NEW_L(PrefsFile, (PREFS_STD)));
	ANCHOR(OpStackAutoPtr<PrefsFile>, prefs);
	prefs->ConstructL();
	prefs->SetFileL(&file);
	prefs->LoadAllL();

	const uni_char* section = UNI_L("Paths");

	prefs->DeleteSectionL(section);

	OpString key; ANCHOR(OpString, key);
	int path_num=0;
	for (UINT32 i=0; i<m_places.GetCount(); i++)
	{
		PlaceItem* item = m_places.Get(i);
		if (item->type == PlaceItem::CustomFolder)
		{
			key.Empty();
			key.AppendFormat(UNI_L("Name%d"), path_num);
			if (key.HasContent())
				prefs->WriteStringL( section, key, item->title);
			key.Empty();
			key.AppendFormat(UNI_L("Path%d"), path_num);
			if (key.HasContent())
				prefs->WriteStringL( section, key, item->path);
			path_num++;
		}
	}

	prefs->WriteIntL( section, UNI_L("Items"), path_num );

	prefs->CommitL();
}

void X11FileDialog::AddPlace(const OpStringC& name, OpFileFolder path_id, const uni_char* path, PlaceItem::PlaceType type)
{
	PlaceItem* item = OP_NEW(PlaceItem,(type));
	if (item)
	{
		// Try to show home folder using the user name if specified by a '~'
		BOOL is_home = path && uni_strcmp(path, UNI_L("~")) == 0;

		item->title.Set(name);

		if (path)
		{
			OpStringC tmp = path;
			if (tmp.Find("file://localhost") == 0)
				path += 16;

			OpString expanded_path;
			g_op_system_info->ExpandSystemVariablesInString(path, &expanded_path);

			// Make sure path ends in one and only one '/'
			expanded_path.Append("/");
			UnixUtils::CanonicalPath(expanded_path);

			item->path.Set(expanded_path);
		}
		else if(path_id != OPFILE_FOLDER_COUNT)
		{
			// The folder path code ensures the paths always end with a '/'
			OpString tmp_storage;
			item->path.Set(g_folder_manager->GetFolderPathIgnoreErrors(path_id, tmp_storage));
		};

		if (item->title.IsEmpty() || is_home)
		{
			int pos = item->path.Length();
			if (pos > 1)
			{
				// p2 will point to last charater that is not a '/'
				const uni_char* p2 = &item->path.CStr()[pos-1];
				while (p2 > item->path.CStr() && *p2 == '/')
					p2--;
				// p1 will point to last '/' before p2
				const uni_char* p1 = p2;
				while (p1 > item->path.CStr() && *p1 != '/')
					p1--;
				if (p2 > p1)
					item->title.Set( p1+1, p2-p1);
			}
		}

		if (OpStatus::IsError(m_places.Add(item)))
			OP_DELETE(item);
		else
		{
			item->id = PlaceItem::next_unused_id++;
			m_places_model.AddItem(item->title, FOLDER_IMAGE, 0, -1, item, item->id);
		}
	}
}

void X11FileDialog::SetDirectory(const OpStringC& path, BOOL add_to_undo_list, BOOL force)
{
	OpString candidate;
	RETURN_VOID_IF_ERROR(candidate.Set(path));
	if (candidate.Length() == 0 || candidate.CStr()[candidate.Length()-1] != '/')
		RETURN_VOID_IF_ERROR(candidate.Append("/"));

	if (!force && !m_active_directory.Compare(candidate))
		return;

	if (add_to_undo_list && m_active_directory.HasContent())
	{
		OpString* s = OP_NEW(OpString,());
		if (!s || OpStatus::IsError(s->Set(m_active_directory)) || OpStatus::IsError(m_undo_list.Add(s)))
			OP_DELETE(s);
	}

	RETURN_VOID_IF_ERROR(m_active_directory.Set(candidate));

	OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("Path_dropdown"));
	if (dropdown)
	{
		dropdown->Clear();

		OpString text;
		if (m_active_directory.Length() > 0)
		{
			const uni_char* p = &m_active_directory.CStr()[m_active_directory.Length()-1];
			while (p > m_active_directory.CStr())
			{
				if (OpStatus::IsError(text.Set(m_active_directory, p-m_active_directory.CStr())))
					break;
				dropdown->AddItem(text);
				for (p--; p >m_active_directory.CStr() && *p != '/'; p--)
				{}
			}
		}
		dropdown->AddItem(UNI_L("/"));
		dropdown->InvalidateAll();
	}

	if (m_settings.type == ToolkitFileChooser::FILE_OPEN ||
		m_settings.type == ToolkitFileChooser::FILE_OPEN_MULTI)
		SetWidgetText("Edit_field", OpString());

	UpdatePathList();

	if (IsDirectorySelector())
	{
		// OpTreeView uses a timer to delay update. If the treeview has focus we do
		// not want OnChange() to overrule the directory we set here

		SetWidgetText("Edit_field", m_active_directory);
		if (m_paths_tree_view && m_paths_tree_view->IsFocused() && m_paths_model.GetCount())
			m_block_path_change = BM_Yes;
	}
	else
	{
		if (m_paths_tree_view)
		{
			m_block_path_change = BM_No;
			OnChange(m_paths_tree_view, FALSE); // Force testing of selected file to enable OK button and fill edit field
		}
	}
}

void X11FileDialog::UpdatePathList()
{
	if (m_paths_tree_view)
	{
		int id = 0;

		m_paths_model.DeleteAll();
		m_userdata_list.DeleteAll();

		// Block UI update while model gets populated. Speeds up operation a lot
		GenericTreeModel::ModelLock lock(&m_paths_model);

		// 1. Add all directories
		OpFolderLister* lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), m_active_directory.CStr());
		if (lister)
		{
			for (BOOL more = lister->Next(); more; more = lister->Next())
			{
				OpStringC filename = lister->GetFileName();
				if (lister->IsFolder() && filename.Compare(UNI_L("..")) && filename.Compare(UNI_L(".")))
				{
					if (!m_settings.show_hidden && filename.FindFirstOf('.') == 0)
						continue;

					FileListModel::UserData* ud = OP_NEW(FileListModel::UserData,(FOLDER_TAG, 0, 0));
					if (!ud || OpStatus::IsError(ud->text.Set(filename)) || OpStatus::IsError(m_userdata_list.Add(ud)))
					{
						OP_DELETE(ud);
						continue;
					}

					int pos = m_paths_model.AddItem(filename.CStr(), FOLDER_IMAGE, 0, -1, ud, id++);

					if (!IsDirectorySelector())
					{
						PosixNativeUtil::NativeString path (lister->GetFullPath());
						struct stat sbuf;
						if (stat(path.get(), &sbuf) == 0)
						{
							OpString modified;
							if (OpStatus::IsSuccess(FormatTime(modified, UNI_L("%x"), sbuf.st_mtime)))
								m_paths_model.SetItemData(pos, 2, modified.CStr());
							ud->time = sbuf.st_mtime;
						}
					}
				}
			}
			OP_DELETE(lister);
		}

		if (!IsDirectorySelector())
		{
			// 2 Add all files that match the active filter
			OpStringC extension = UNI_L("*");
			X11FileChooserSettings::FilterData* f=0;
			if (OpStatus::IsSuccess(m_settings.filter.GetData(m_settings.activefilter, &f)))
				extension = f->extension.CStr();

			// The active filter may contain more than one extension
			OpAutoVector<OpString> extension_list;
			if (OpStatus::IsSuccess(StringUtils::SplitString(extension_list, extension, ' ', FALSE)))
			{
				for (UINT32 i=0; i<extension_list.GetCount(); i++)
				{
					lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, extension_list.Get(i)->CStr(), m_active_directory.CStr());
					if (lister)
					{
						for (BOOL more = lister->Next(); more; more = lister->Next())
						{
							OpStringC filename = lister->GetFileName();
							if (!lister->IsFolder())
							{
								if (!m_settings.show_hidden && filename.FindFirstOf('.') == 0)
									continue;

								FileListModel::UserData* ud = OP_NEW(FileListModel::UserData,(FILE_TAG, 0, 0));
								if (!ud || OpStatus::IsError(ud->text.Set(filename)) || OpStatus::IsError(m_userdata_list.Add(ud)))
								{
									OP_DELETE(ud);
									continue;
								}

								int pos = m_paths_model.AddItem(filename.CStr(), DOCUMENT_IMAGE, 2, -1, ud, id++);

								PosixNativeUtil::NativeString path(lister->GetFullPath());
								struct stat sbuf;
								if (stat(path.get(), &sbuf) == 0)
								{
									OpString bytesize;
									if (StrFormatByteSize(bytesize, sbuf.st_size, SFBS_FORCEKB))
										m_paths_model.SetItemData(pos, 1, bytesize.CStr());
									OpString modified;
									if (OpStatus::IsSuccess(FormatTime(modified, UNI_L("%x"), sbuf.st_mtime)))
										m_paths_model.SetItemData(pos, 2, modified.CStr());
									ud->size = sbuf.st_size;
									ud->time = sbuf.st_mtime;
								}
							}
						}
						OP_DELETE(lister);
					}
				}
			}
		}

		m_paths_tree_view->SetSelectedItem(0, FALSE, FALSE, FALSE);
	}
}


void X11FileDialog::UpdateTypedExtension()
{
	if (m_settings.type == ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE ||
		m_settings.type == ToolkitFileChooser::FILE_SAVE)
	{
		OpString filename;
		GetWidgetText("Edit_field", filename);
		filename.Strip();
		if (filename.IsEmpty())
			return;

		X11FileChooserSettings::FilterData* f=0;
		RETURN_VOID_IF_ERROR(m_settings.filter.GetData(m_settings.activefilter, &f));

		OpAutoVector<OpString> extension_list;
		RETURN_VOID_IF_ERROR(StringUtils::SplitString(extension_list, f->extension, ' ', FALSE));
		if (extension_list.GetCount() > 0)
		{
			OpStringC extension = extension_list.Get(0)->CStr();
			if (extension.Compare("*"))
			{
				const uni_char* p = uni_strrchr(filename.CStr(), '.');
				if (p && p > filename.CStr())
				{
					filename.Delete(p-filename.CStr());
					int pos = extension.Find(".");
					RETURN_VOID_IF_ERROR(filename.Append(&extension.CStr()[pos==KNotFound ? 0 : pos]));
					SetWidgetText("Edit_field", filename);
				}
			}
		}
	}
}


void X11FileDialog::OnChange(OpWidget* widget, BOOL changed_by_mouse)
{
	OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("Path_dropdown"));
	if (dropdown && widget == dropdown)
	{
		OpString directory;
		directory.Set(dropdown->GetItemText(dropdown->GetSelectedItem()));
		SetDirectory(directory, TRUE, FALSE);
	}
	else if (m_places_tree_view && m_places_tree_view == widget)
	{
		if (m_places_tree_view->IsFocused())
		{
			PlaceItem* item = static_cast<PlaceItem*>(m_places_model.GetItemUserData(m_places_tree_view->GetSelectedItemModelPos()));
			if (item && item->path.HasContent())
				SetDirectory(item->path, TRUE, FALSE);
		}
	}
	else if (m_paths_tree_view && m_paths_tree_view == widget)
	{
		if (m_block_path_change == BM_No || (m_block_path_change == BM_Ignore && m_paths_tree_view->IsFocused()))
		{
			OpStringC text = m_paths_model.GetItemString(m_paths_tree_view->GetSelectedItemModelPos());
			if (text.HasContent())
			{
				if (IsDirectorySelector())
				{
					OpString directory;
					if (OpStatus::IsSuccess(directory.Set(m_active_directory)) && OpStatus::IsSuccess(directory.Append(text)))
						SetWidgetText("Edit_field", directory);
				}
				else
				{
					FileListModel::UserData* ud = static_cast<FileListModel::UserData*>(m_paths_model.GetItemUserData(m_paths_tree_view->GetSelectedItemModelPos()));
					if (ud->type == FILE_TAG)
						SetWidgetText("Edit_field", text);
				}
			}
		}
		m_block_path_change = BM_Ignore;
		EnableOkButton(!m_is_editing && VerifyInput(FALSE));
	}
	else
	{
		dropdown = static_cast<OpDropDown*>(GetWidgetByName("Type_dropdown"));
		if (dropdown && widget == dropdown)
		{
			INT32 index = dropdown->GetSelectedItem();
			X11FileChooserSettings::FilterData* f=0;
			if (OpStatus::IsSuccess(m_settings.filter.GetData(index, &f)))
				m_settings.activefilter = index;
			UpdatePathList();
			UpdateTypedExtension();
		}
		else
		{
			OpEdit* edit = static_cast<OpEdit*>(GetWidgetByName("Edit_field"));
			if (edit && widget == edit)
				EnableOkButton(!m_is_editing && VerifyInput(FALSE));
		}
	}

	Dialog::OnChange(widget, changed_by_mouse);
}



BOOL X11FileDialog::OnItemEditVerification(OpWidget *widget, INT32 pos, INT32 column, const OpString& text)
{
	if (m_paths_tree_view && m_paths_tree_view == widget)
		return OnFolderComplete(pos, text, FALSE);

	return TRUE;
}

void X11FileDialog::OnItemEditAborted(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	if (m_paths_tree_view && m_paths_tree_view == widget)
	{
		if (m_rename_path.IsEmpty())
		{
			UINT32 index = m_userdata_list.GetCount();
			if (index > 0)
			{
				index --;
				m_userdata_list.Delete(index,1);
				m_paths_model.Delete(index,1);
			}
		}
	}
	OnEditModeChanged(FALSE);
}

void X11FileDialog::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	if (m_places_tree_view && m_places_tree_view == widget)
	{
		pos = m_places_tree_view->GetModelPos(pos);

		PlaceItem* item = static_cast<PlaceItem*>(m_places_model.GetItemUserData(pos));
		if (item && item->type == PlaceItem::CustomFolder)
		{
			text.Strip();
			if (text.HasContent())
			{
				item->title.Set(text);
				m_places_model.SetItemData(pos, column, text.CStr(), FOLDER_IMAGE);
			}
		}
	}
	else if (m_paths_tree_view && m_paths_tree_view == widget)
	{
		if (!OnFolderComplete(pos, text, TRUE))
			UpdatePathList();
	}

	OnEditModeChanged(FALSE);
}



void X11FileDialog::OnEditModeChanged(BOOL state)
{
	if (m_is_editing != state)
	{
		m_is_editing = state;

		if (!m_is_editing)
			m_rename_path.Empty();

		EnableOkButton( !m_is_editing && VerifyInput(FALSE));
		SetWidgetEnabled("Path_dropdown", !m_is_editing);
		SetWidgetEnabled("Type_dropdown", !m_is_editing);
	}
}



BOOL X11FileDialog::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	if (m_is_editing)
		return TRUE;

	if (m_paths_tree_view && m_paths_tree_view == widget)
	{
		PopupMenu* menu = OP_NEW(PopupMenu,());
		if (!menu || OpStatus::IsError(menu->Init(0)))
		{
			OP_DELETE(menu);
			return TRUE;
		}

		BOOL valid_sub_folder = FALSE;
		if (m_paths_tree_view->GetSelectedItemCount() == 1)
		{
			FileListModel::UserData* ud = static_cast<FileListModel::UserData*>(m_paths_model.GetItemUserData(m_paths_tree_view->GetSelectedItemModelPos()));
			valid_sub_folder = ud->type == FOLDER_TAG;
			if (valid_sub_folder)
			{
				OpString candidate;
				candidate.Set(m_active_directory);
				candidate.Append(m_paths_model.GetItemString(m_paths_tree_view->GetSelectedItemModelPos()));
				PosixNativeUtil::NativeString filename(candidate.CStr());
				valid_sub_folder = access(filename.get(), W_OK) == 0;
			}
		}
		PosixNativeUtil::NativeString active_directory(m_active_directory.CStr());
		BOOL valid_folder = access(active_directory.get(), W_OK) == 0;

		OpWidgetImage widget_image;
		if (m_settings.show_hidden)
			widget_image.SetImage("Checkmark");

		OpString name;

		// We can choose any kind of action. We only use it locally
		OpInputAction* action = OP_NEW( OpInputAction,(OpInputAction::ACTION_CHANGE));
		g_languageManager->GetString(Str::D_FILEDIALOG_MENU_SHOWHIDDEN, name);
		menu->AddItem(name, 0, action, m_settings.show_hidden ? PopupMenuItem::MI_CHECKED : 0, &widget_image);
		if (m_settings.type == ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE ||
			m_settings.type == ToolkitFileChooser::FILE_SAVE ||
			IsDirectorySelector())
		{
			menu->AddSeparator();
			action = OP_NEW( OpInputAction,(OpInputAction::ACTION_NEW_FOLDER));
			g_languageManager->GetString(Str::D_FILEDIALOG_MENU_ADDFOLDER, name);
			menu->AddItem(name, 0, action, valid_folder ? 0 : PopupMenuItem::MI_DISABLED, 0);
			action = OP_NEW( OpInputAction,(OpInputAction::ACTION_EDIT_ITEM));
			g_languageManager->GetString(Str::D_FILEDIALOG_MENU_EDITFOLDER, name);
			menu->AddItem(name, 0, action, valid_sub_folder ? 0 : PopupMenuItem::MI_DISABLED, 0);
			menu->AddSeparator();
			action = OP_NEW( OpInputAction,(OpInputAction::ACTION_DELETE));
			g_languageManager->GetString(Str::D_FILEDIALOG_MENU_REMOVEFOLDER, name);
			menu->AddItem(name, 0, action, valid_sub_folder ? 0 : PopupMenuItem::MI_DISABLED, 0);
		}

		const OpPoint anchor = OpPoint(menu_point.x, menu_point.y) + widget->GetScreenRect().TopLeft();
		switch(PopupMenu::Exec(menu, anchor, true, false))
		{
			case OpInputAction::ACTION_CHANGE:
			{
				m_settings.show_hidden = !m_settings.show_hidden;
				TRAPD(rc,g_pcunix->WriteIntegerL(PrefsCollectionUnix::FileSelectorShowHiddenFiles, m_settings.show_hidden));
				SetDirectory(m_active_directory, FALSE, TRUE);
			}
			break;

			case OpInputAction::ACTION_NEW_FOLDER:
				NewFolder();
			break;

			case OpInputAction::ACTION_EDIT_ITEM:
				EditSelectedPathItem();
			break;

			case OpInputAction::ACTION_DELETE:
			{
				OpString filename;
				RETURN_VALUE_IF_ERROR(filename.Set(m_active_directory), TRUE);
				RETURN_VALUE_IF_ERROR(filename.Append(m_paths_model.GetItemString(m_paths_tree_view->GetSelectedItemModelPos())), TRUE);
				PosixNativeUtil::NativeString native_filename(filename.CStr());
				if (rmdir(native_filename.get()) == 0)
					UpdatePathList();
			}
			break;
		}
		return TRUE;
	}
	else if (m_places_tree_view && m_places_tree_view == widget)
	{
		PlaceItem* item = static_cast<PlaceItem*>(m_places_model.GetItemUserData(m_places_tree_view->GetSelectedItemModelPos()));
		if (item)
		{
			PopupMenu* menu = OP_NEW(PopupMenu,());
			if (!menu || OpStatus::IsError(menu->Init(0)))
			{
				OP_DELETE(menu);
				return TRUE;
			}

			OpString name;

			OpInputAction* action = OP_NEW( OpInputAction,(OpInputAction::ACTION_EDIT_ITEM));
			g_languageManager->GetString(Str::D_FILEDIALOG_MENU_EDIT, name);
			menu->AddItem(name, 0, action, item->type == PlaceItem::CustomFolder ? 0 : PopupMenuItem::MI_DISABLED, 0);
			menu->AddSeparator();
			action = OP_NEW( OpInputAction,(OpInputAction::ACTION_DELETE));
			g_languageManager->GetString(Str::D_FILEDIALOG_MENU_REMOVE, name);
			menu->AddItem(name, 0, action, item->type == PlaceItem::CustomFolder ? 0 : PopupMenuItem::MI_DISABLED, 0);

			const OpPoint anchor = OpPoint(menu_point.x, menu_point.y) + widget->GetScreenRect().TopLeft();
			switch(PopupMenu::Exec(menu, anchor, true, false))
			{
				case OpInputAction::ACTION_EDIT_ITEM:
					EditSelectedPlaceItem();
				break;

				case OpInputAction::ACTION_DELETE:
				{
					m_places.Delete(item);
					m_places_model.Delete(m_places_tree_view->GetSelectedItemModelPos());
				}
				break;
			}
		}
		return TRUE;
	}

	return Dialog::OnContextMenu(widget, child_index, menu_point, NULL, FALSE);
}


void X11FileDialog::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	OpString candidate;

	if (m_paths_tree_view && m_paths_tree_view == widget)
	{
		OpStringC text = m_paths_model.GetItemString(m_paths_tree_view->GetSelectedItemModelPos());
		if (text.HasContent())
		{
			RETURN_VOID_IF_ERROR(candidate.Set("file://localhost"));
			RETURN_VOID_IF_ERROR(candidate.Append(m_active_directory));
			RETURN_VOID_IF_ERROR(candidate.Append(text));
		}
	}
	else if (m_places_tree_view && m_places_tree_view == widget)
	{
		PlaceItem* item = static_cast<PlaceItem*>(m_places_model.GetItemUserData(m_places_tree_view->GetSelectedItemModelPos()));
		if (item && item->path.HasContent())
		{
			RETURN_VOID_IF_ERROR(candidate.Set("file://localhost"));
			RETURN_VOID_IF_ERROR(candidate.Append(item->path));
		}
	}

	if (candidate.HasContent())
	{
		OpDragObject* drag_object;
		if (OpStatus::IsSuccess(OpDragObject::Create(drag_object, DRAG_TYPE_LINK)))
		{
			DragDrop_Data_Utils::AddURL(drag_object, candidate);
			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
}

void X11FileDialog::OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	HandleDragDrop(widget, drag_object, pos, x, y, true);
}

void X11FileDialog::OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	HandleDragDrop(widget, drag_object, pos, x, y, false);
}

void X11FileDialog::HandleDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y, bool drop)
{
	DesktopDragObject* drag_object = (DesktopDragObject*)op_drag_object;

	if (m_places_tree_view && m_places_tree_view == widget && DragDrop_Data_Utils::HasURL(drag_object))
	{
		if (pos == -1 || drop)
		{
			TempBuffer buffer;
			RETURN_VOID_IF_ERROR(DragDrop_Data_Utils::GetURL(drag_object, &buffer, FALSE));

			// Strip 'file://localhost' if file name starts with 'file://localhost/'
			OpStringC tmp(buffer.GetStorage());
			int offset = tmp.Compare("file://localhost/", 17) ? 0 : 16;

			OpString filename;
			RETURN_VOID_IF_ERROR(filename.Set(tmp.CStr()+offset));
			filename.Strip();

			if (!drop)
			{
				OpFile file;
				if (OpStatus::IsSuccess(file.Construct(filename)))
				{
					OpFileInfo::Mode mode;
					if (OpStatus::IsSuccess(file.GetMode(mode)) && mode == OpFileInfo::DIRECTORY)
						drag_object->SetDropType(DROP_COPY);
				}
			}
			else
			{
				AddPlace(0, OPFILE_FOLDER_COUNT, filename, PlaceItem::CustomFolder);
			}
		}
	}
	else
	{
		drag_object->SetDropType(DROP_NOT_AVAILABLE);
	}
}


void X11FileDialog::OnCancel()
{
	m_settings.selected_files.DeleteAll();
	m_settings.selected_path.Empty();
	m_settings.dialog = 0;
}


BOOL X11FileDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_CHANGE:
				{
					OpStringC type = child_action->GetActionDataString();
					if (type.Compare("up") == 0)
						child_action->SetEnabled(!m_is_editing && m_active_directory.Compare(UNI_L("/"))!=0);
					if (type.Compare("back") == 0)
						child_action->SetEnabled(!m_is_editing && m_undo_list.GetCount()>0);
					if (type.Compare("folder") == 0)
						child_action->SetEnabled(
							!m_is_editing &&
							(m_settings.type == ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE ||
							 m_settings.type == ToolkitFileChooser::FILE_SAVE ||
							 IsDirectorySelector()));
					return TRUE;
				}
				case OpInputAction::ACTION_CANCEL:
					child_action->SetEnabled(!m_is_editing);
					return TRUE;
			}
		}
		break;

		case OpInputAction::ACTION_OK:
		{
			if (m_places_tree_view && m_places_tree_view->IsFocused())
				return TRUE;
			if (!IsDirectorySelector() && VerifyInputDirectory(TRUE))
				return TRUE;
			if (!VerifyInput(TRUE))
				return TRUE;
			m_settings.dialog = 0;
		}
		break;

		case OpInputAction::ACTION_GO_TO_PARENT_DIRECTORY:
			Up();
			return TRUE;
		break;

		case OpInputAction::ACTION_UNDO:
			Back();
			return TRUE;
		break;

		case OpInputAction::ACTION_EDIT_ITEM:
		{
			if (m_paths_tree_view && m_paths_tree_view->IsFocused())
				EditSelectedPathItem();
			else if (m_places_tree_view && m_places_tree_view->IsFocused())
				EditSelectedPlaceItem();
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_CHANGE:
		{
			OpStringC type = action->GetActionDataString();
			if (type.Compare("paths") == 0)
			{
				if (m_paths_tree_view)
				{
					OpStringC text = m_paths_model.GetItemString(m_paths_tree_view->GetSelectedItemModelPos());
					if (text.HasContent())
					{
						FileListModel::UserData* ud = static_cast<FileListModel::UserData*>(m_paths_model.GetItemUserData(m_paths_tree_view->GetSelectedItemModelPos()));
						if (ud->type == FOLDER_TAG)
						{
							OpString candidate;
							candidate.Set(m_active_directory);
							candidate.Append(text);
							SetDirectory(candidate, TRUE, FALSE);
						}
						else if (ud->type == FILE_TAG)
						{
							// Allow Enter/Return and doble mouse clicks to select and open the file
							SetWidgetText("Edit_field", text);
							OpInputAction* action = OP_NEW(OpInputAction,(OpInputAction::ACTION_OK));
							if (action)
								g_input_manager->InvokeAction(action, this);
						}
					}
				}
				return TRUE;
			}
			else if (type.Compare("places") == 0)
			{
				if (m_places_tree_view)
				{
					PlaceItem* item = static_cast<PlaceItem*>(m_places_model.GetItemUserData(m_places_tree_view->GetSelectedItemModelPos()));
					if (item && item->path.HasContent())
						SetDirectory(item->path, TRUE, FALSE);
				}
				return TRUE;
			}
			else if (type.Compare("up") == 0)
			{
				Up();
				return TRUE;
			}
			else if (type.Compare("back") == 0)
			{
				Back();
				return TRUE;
			}
			else if (type.Compare("folder") == 0)
			{
				NewFolder();
				return TRUE;
			}
		}
		break;
	}

	return Dialog::OnInputAction(action);
}


BOOL X11FileDialog::GetPreferredPlacement(OpToolTip* tooltip, OpRect &ref_rect, PREFERRED_PLACEMENT &placement)
{
	if (m_paths_tree_view)
	{
		placement = OpToolTipListener::PREFERRED_PLACEMENT_BOTTOM;
		ref_rect = m_paths_tree_view->GetEditRect();
		ref_rect.OffsetBy(m_paths_tree_view->GetScreenRect().TopLeft());
		return TRUE;
	}
	return FALSE;
}


BOOL X11FileDialog::Up()
{
	OpString candidate;
	candidate.Set(m_active_directory);
	if (candidate.Length() > 1)
	{
		const uni_char* p = &candidate.CStr()[candidate.Length()-2];
		if (p > candidate.CStr())
		{
			for (p--; p>candidate.CStr() && *p != '/'; p--)
			{}
			candidate.Delete(p-candidate.CStr());
			SetDirectory(candidate, TRUE, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL X11FileDialog::Back()
{
	while (m_undo_list.GetCount() > 0)
	{
		UINT32 num_element = m_undo_list.GetCount();

		OpString& s = *m_undo_list.Get(num_element-1);
		OpFile file;
		OpFileInfo::Mode mode;

		if (OpStatus::IsSuccess(file.Construct(s)) &&
			OpStatus::IsSuccess(file.GetMode(mode)) &&
			mode == OpFileInfo::DIRECTORY)
		{
			SetDirectory(s, FALSE, FALSE);
			m_undo_list.Delete(m_undo_list.Get(num_element-1));
			return TRUE;
		}
		else
			m_undo_list.Delete(m_undo_list.Get(num_element-1));
	}
	return FALSE;
}


BOOL X11FileDialog::EditSelectedPlaceItem()
{
	if (m_places_tree_view)
	{
		PlaceItem* item = static_cast<PlaceItem*>(m_places_model.GetItemUserData(m_places_tree_view->GetSelectedItemModelPos()));
		if (item && item->type == PlaceItem::CustomFolder)
		{
			m_places_tree_view->EditItem(m_places_tree_view->GetSelectedItemPos(), 0);
			OnEditModeChanged(TRUE);
			return TRUE;
		}
	}
	return FALSE;
}


BOOL X11FileDialog::EditSelectedPathItem()
{
	if (m_settings.type == ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE ||
		m_settings.type == ToolkitFileChooser::FILE_SAVE)
	{
		if (m_paths_tree_view)
		{
			// Only folders can be changed (so far)
			int pos = m_paths_tree_view->GetSelectedItemModelPos();
			FileListModel::UserData* ud = static_cast<FileListModel::UserData*>(m_paths_model.GetItemUserData(pos));
			if (ud->type == FOLDER_TAG)
			{
				OpString rename_path;
				if( OpStatus::IsSuccess(rename_path.Set(m_active_directory)) &&
					OpStatus::IsSuccess(rename_path.Append(m_paths_model.GetItemString(pos))))
				{
					PosixNativeUtil::ToNative(rename_path.CStr(), &m_rename_path);
					if (access(m_rename_path.CStr(), W_OK) == 0)
					{
						m_paths_tree_view->EditItem(m_paths_tree_view->GetSelectedItemPos(), 0);
						OnEditModeChanged(TRUE);
						return TRUE;
					}
				}
				m_rename_path.Empty();
			}
		}
	}
	return FALSE;
}


BOOL X11FileDialog::NewFolder()
{
	if (m_settings.type == ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE ||
		m_settings.type == ToolkitFileChooser::FILE_SAVE)
	{
		if (m_paths_tree_view)
		{
			FileListModel::UserData* ud = OP_NEW(FileListModel::UserData,(FOLDER_TAG, 0, 0));
			if (!ud || OpStatus::IsError(m_userdata_list.Add(ud)))
			{
				OP_DELETE(ud);
				return FALSE;
			}

			int id = m_paths_model.GetItemCount() + 1;
			if (OpStatus::IsError(m_paths_model.AddItem(UNI_L(""), FOLDER_IMAGE, 1, -1, ud, id)))
			{
				// m_userdata_list will manage and delete the 'ud' instance
				return FALSE;
			}

			m_paths_tree_view->SetSelectedItemByID(id);
			m_paths_tree_view->EditItem(m_paths_tree_view->GetSelectedItemPos(), 0);

			OpString message;
			g_languageManager->GetString(Str::D_FILEDIALOG_MSG_NEW_FOLDER, message);
			ShowMessage(message);

			m_rename_path.Empty();
			OnEditModeChanged(TRUE);
			return TRUE;
		}
	}
	return FALSE;
}


BOOL X11FileDialog::OnFolderComplete(int pos, const OpString& text, BOOL exec)
{
	OpString filename;

	filename.Set(text);
	filename.Strip();
	if (filename.IsEmpty() ||
		filename.Find("/") != KNotFound ||
		!filename.Compare(UNI_L("..")) ||
		!filename.Compare(UNI_L(".")))
	{
		if (!exec)
		{
			OpString message;
			g_languageManager->GetString(Str::D_FILEDIALOG_MSG_FOLDER_INVALID, message);
			ShowMessage(message);
		}
		return FALSE;
	}

	OpString tmp_candidate; // We need filename intact below
	RETURN_VALUE_IF_ERROR(tmp_candidate.Set(m_active_directory),FALSE);
	RETURN_VALUE_IF_ERROR(tmp_candidate.Append(filename),FALSE);
	PosixNativeUtil::NativeString candidate(tmp_candidate.CStr());

	if (m_rename_path.HasContent() && !m_rename_path.Compare(candidate.get()))
		return TRUE;

	if (access(candidate.get(), F_OK) == 0)
	{
		if (!exec)
		{
			OpString message;
			g_languageManager->GetString(Str::D_FILEDIALOG_MSG_FOLDER_EXISTS, message);
			ShowMessage(message);
		}
		return FALSE;
	}

	if (exec)
	{
		if (m_rename_path.HasContent())
		{
			if (rename(m_rename_path.CStr(), candidate.get()))
				return FALSE;
		}
		else
		{
			RETURN_VALUE_IF_ERROR(PosixFileUtil::CreatePath(candidate.get(), false), FALSE);
		}

		pos = m_paths_tree_view->GetModelPos(pos);

		FileListModel::UserData* ud = static_cast<FileListModel::UserData*>(m_paths_model.GetItemUserData(pos));
		if (ud->type == FOLDER_TAG)
		{
			struct stat sbuf;
			if (stat(candidate.get(),&sbuf) == 0)
			{
				OpString modified;
				if (OpStatus::IsSuccess(FormatTime(modified, UNI_L("%x"), sbuf.st_mtime)))
					m_paths_model.SetItemData(pos, 2, modified.CStr());
				ud->time = sbuf.st_mtime;
			}
		}
		m_paths_model.SetItemData(pos, 0, filename.CStr(), FOLDER_IMAGE);

		if (filename.Find(".") == 0)
		{
			m_settings.show_hidden = TRUE;
			TRAPD(rc,g_pcunix->WriteIntegerL(PrefsCollectionUnix::FileSelectorShowHiddenFiles, m_settings.show_hidden));
		}
	}

	return TRUE;
}


BOOL X11FileDialog::VerifyInput(BOOL on_ok)
{
	if (on_ok)
	{
		m_settings.selected_files.DeleteAll();
		m_settings.selected_path.Empty();
	}
	else
	{
		if (VerifyInputDirectory(FALSE))
			return TRUE;
	}

	OpAutoVector<OpString> list;

	OpString text;
	GetWidgetText("Edit_field", text);
	// Do not strip text here. We may have a filename with white spaces at the front and/or in the end
	if (text.HasContent())
	{
		OpString expanded;
		RETURN_VALUE_IF_ERROR(g_op_system_info->ExpandSystemVariablesInString(text.CStr(), &expanded), FALSE);
		if (expanded.HasContent())
			RETURN_VALUE_IF_ERROR(text.Set(expanded), FALSE);
		if (text.Length() > 0 && text.CStr()[0] != '/')
			RETURN_VALUE_IF_ERROR(text.Insert(0, m_active_directory), FALSE);
	}
	else if (IsDirectorySelector())
		text.Set(m_active_directory);

	if (text.IsEmpty())
	{
		if (m_settings.type == ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE ||
			m_settings.type == ToolkitFileChooser::FILE_SAVE ||
			m_settings.type == ToolkitFileChooser::FILE_OPEN)
			return FALSE;
	}
	else
	{
		OpString* s = OP_NEW(OpString,());
		if (!s || OpStatus::IsError(s->Set(text)) || OpStatus::IsError(list.Add(s)))
		{
			OP_DELETE(s);
			return FALSE;
		}
	}

	if (m_settings.type == ToolkitFileChooser::FILE_OPEN_MULTI && m_paths_tree_view)
	{
		OpINT32Vector items;
		m_paths_tree_view->GetSelectedItems(items,TRUE);
		for (UINT32 i=0; i<items.GetCount(); i++)
		{
			FileListModel::UserData* ud = static_cast<FileListModel::UserData*>(m_paths_model.GetItemUserDataByID(items.Get(i)));
			if (ud->type == FOLDER_TAG)
				continue;

			OpString candidate;
			if (OpStatus::IsError(candidate.Set(m_active_directory)) ||
				OpStatus::IsError(candidate.Append(m_paths_model.GetItemStringByID(items.Get(i)))))
				continue;

			if (candidate.Compare(text))
			{
				OpString* s = OP_NEW(OpString,());
				if( !s || OpStatus::IsError(s->Set(candidate)) || OpStatus::IsError(list.Add(s)))
					OP_DELETE(s);
			}
		}
	}

	if (list.GetCount() == 0)
		return FALSE;

	for (UINT32 i=0; i<list.GetCount(); i++)
	{
		if (!VerifyInputFilename(*list.Get(i), on_ok))
			return FALSE;
	}

	if (on_ok)
	{
		for (UINT32 i=0; i<list.GetCount(); i++)
		{
			OpString8* s = OP_NEW(OpString8,());
			if (!s || OpStatus::IsError(s->SetUTF8FromUTF16(*list.Get(i))) || OpStatus::IsError(m_settings.selected_files.Add(s)))
				OP_DELETE(s);
		}
		m_settings.selected_path.SetUTF8FromUTF16(m_active_directory);
	}

	return TRUE;
}


BOOL X11FileDialog::VerifyInputFilename(const OpString& filename, BOOL on_ok)
{
	if (filename.IsEmpty())
		return FALSE;

	OpFile file;
	if (OpStatus::IsError(file.Construct(filename)) )
		return FALSE;

	BOOL exists = FALSE;
	if (OpStatus::IsError(file.Exists(exists)))
		return FALSE;

	if (IsDirectorySelector())
	{
		OpFileInfo::Mode mode;
		return exists && OpStatus::IsSuccess(file.GetMode(mode)) && mode == OpFileInfo::DIRECTORY;
	}

	if (exists)
	{
		OpFileInfo::Mode mode;
		if (OpStatus::IsError(file.GetMode(mode)) || mode == OpFileInfo::DIRECTORY)
			return FALSE;
	}
	else if (m_settings.type == ToolkitFileChooser::FILE_OPEN ||
			 m_settings.type == ToolkitFileChooser::FILE_OPEN_MULTI)
	{
		// I do not think we need to open non-existing files in Opera
		return FALSE;
	}

	if (m_settings.type == ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE ||
		m_settings.type == ToolkitFileChooser::FILE_SAVE)
	{
		if (!file.IsWritable())
			return FALSE;
	}

	if (on_ok && m_settings.type == ToolkitFileChooser::FILE_SAVE_PROMPT_OVERWRITE)
	{
		if (exists)
		{
			BOOL replace = UnixUtils::PromptFilenameAlreadyExist(this, filename, TRUE);
			if (!replace)
				return FALSE;
		}
	}

	return TRUE;
}


BOOL X11FileDialog::VerifyInputDirectory(BOOL change_directory)
{
	OpString text;
	GetWidgetText("Edit_field", text);
	// Do not strip text here. We may have a filename with white spaces at the front and/or in the end
	if (text.IsEmpty())
		return FALSE;

	OpString expanded;
	RETURN_VALUE_IF_ERROR(g_op_system_info->ExpandSystemVariablesInString(text.CStr(), &expanded), FALSE);
	if (expanded.HasContent())
		RETURN_VALUE_IF_ERROR(text.Set(expanded), FALSE);

	int pos = text.Find("/");
	if (pos != 0)
		RETURN_VALUE_IF_ERROR(text.Insert(0,m_active_directory), FALSE);

	UnixUtils::CanonicalPath(text);
	if (text.IsEmpty())
		return FALSE;

	OpFile file;
	if (OpStatus::IsSuccess(file.Construct(text)))
	{
		BOOL exists;
		if (OpStatus::IsSuccess(file.Exists(exists)) && exists)
		{
			OpFileInfo::Mode mode;
			if (OpStatus::IsSuccess(file.GetMode(mode)) && mode == OpFileInfo::DIRECTORY)
			{
				if (change_directory)
				{
					SetWidgetText("Edit_field", OpString());
					SetDirectory(text, TRUE, FALSE);
				}
				return TRUE;
			}
		}
	}

	return FALSE;
}


void X11FileDialog::ShowMessage(const OpStringC& message)
{
	m_tooltip_message.Set(message);
	g_application->SetToolTipListener(this);
}
