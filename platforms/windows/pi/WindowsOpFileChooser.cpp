/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "WindowsOpFileChooser.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/windows_ui/registry.h"

OP_STATUS DesktopFileChooser::Create(DesktopFileChooser** new_object)
{
	OP_ASSERT(new_object != NULL);
	WindowsOpFileChooserProxy *proxy = OP_NEW(WindowsOpFileChooserProxy, ());
	if (proxy)
	{
		if (OpStatus::IsSuccess(proxy->Init()))
		{
			*new_object = proxy;
			return OpStatus::OK;
		}
		else
			OP_DELETE(proxy);
	}
	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS WindowsOpFileChooserProxy::Init()
{
	OpComPtr<IFileOpenDialog> dlg;
	HRESULT hr = dlg.CoCreateInstance(__uuidof(FileOpenDialog));
	if(SUCCEEDED(hr))	// if we have the interface, that's enough
	{
		m_actual_chooser = OP_NEW(WindowsOpFileChooser, (this));
	}
	else
	{
		m_actual_chooser = OP_NEW(WindowsXPFileChooser, (this));
	}
	return m_actual_chooser ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/*
* WindowsFileChooserBase - This is the base class with functionality shared between the XP and Vista+ versions of the dialog
*/
WindowsFileChooserBase::WindowsFileChooserBase(WindowsOpFileChooserProxy* proxy)
	: m_listener(NULL)
	, m_settings(NULL)
	, m_proxy(proxy)
	, m_start_with_extension(FALSE)
	, m_parent_window(NULL)
	, m_chooser_window(NULL)
{
	g_main_message_handler->SetCallBack(this, FILE_CHOOSER_SHOW, (MH_PARAM_1)this);
}

WindowsFileChooserBase::~WindowsFileChooserBase()
{
	g_main_message_handler->UnsetCallBack(this, FILE_CHOOSER_SHOW, (MH_PARAM_1)this);
}

OP_STATUS WindowsFileChooserBase::Execute(OpWindow* parent, DesktopFileChooserListener* listener, const DesktopFileChooserRequest& settings)
{
	if (m_settings)
		return OpStatus::ERR;		//Calls to Execute while a session is in progress are prohibited

	if (!listener)
		return OpStatus::ERR_NULL_POINTER;

	if (settings.initial_path.CStr() && *(settings.initial_path.CStr()) == '"')
	{
		OP_ASSERT(!"We do not accept quoted filenames. See the documentation");
		return OpStatus::ERR;
	}

	if (!parent)
	{
		DesktopWindow *active_desktop_window = g_application->GetActiveDesktopWindow();
		m_parent_window = active_desktop_window ? ((WindowsOpWindow*)active_desktop_window->GetWindow())->m_hwnd : NULL;
	}
	else
	{
		m_parent_window = ((WindowsOpWindow*)parent)->GetParentHWND();
	}

	m_listener = listener;

	m_settings = &settings;

	if (m_settings->action != DesktopFileChooserRequest::ACTION_DIRECTORY)
		RETURN_IF_ERROR(MakeWinFilter());

	m_results.active_directory.Empty();
	m_results.files.DeleteAll();
	m_results.selected_filter = -1;

	g_main_message_handler->PostMessage(FILE_CHOOSER_SHOW, (MH_PARAM_1)this, 0);

	return OpStatus::OK;
}

OP_STATUS WindowsFileChooserBase::MakeWinFilter()
{
	m_filters.DeleteAll();
	m_filename_ext = NULL;
	OpString extension;

	//If we are allowed to change the filter and that we are saving, then make sure that the extension of the filetype to save is added to the filter if a filename is already available
	BOOL add_path_extension = !(m_settings->fixed_filter) && 
		(m_settings->action == DesktopFileChooserRequest::ACTION_FILE_SAVE || m_settings->action == DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE) &&
		m_settings->initial_path.HasContent() && *PathFindExtension(m_settings->initial_path.CStr());

	//Gets the extension of the supplied filename if needed
	if (add_path_extension)
		extension.AppendFormat(UNI_L("*%s"), PathFindExtension(m_settings->initial_path.CStr()));

	//Processes the list of extensions given to a filter string
	for (UINT32 i = 0; i < m_settings->extension_filters.GetCount(); i++)
	{
		OpFileSelectionListener::MediaType* filter = m_settings->extension_filters.Get(i);
		if (filter->media_type.IsEmpty())
			continue;

		OpAutoPtr<FileFilter> file_filter = OP_NEW(FileFilter, (filter->media_type.CStr()));
		RETURN_OOM_IF_NULL(file_filter.get());

		UINT32 j;
		for (j = 0; j < filter->file_extensions.GetCount(); j++)
		{
			if (!filter->file_extensions.Get(j))
			{
				RETURN_IF_ERROR(file_filter->AddFilter(UNI_L("*.*")));
			}
			else if (filter->file_extensions.Get(j)->HasContent())
			{
				if (filter->file_extensions.Get(j)->Compare("*") != 0)
				{
					RETURN_IF_ERROR(file_filter->AddFilter(filter->file_extensions.Get(j)->CStr()));
				}
				else
				{
					RETURN_IF_ERROR(file_filter->AddFilter(UNI_L("*.*")));
				}
				//if the filter already contains the extension of the supplied filename, no need to add it again
				if (add_path_extension && !filter->file_extensions.Get(j)->CompareI(extension))
				{
					add_path_extension = FALSE;
				}
			}
		}
		RETURN_IF_ERROR(m_filters.Add(file_filter.get()));
		file_filter.release();
	}

	OpString all_files;
	g_languageManager->GetString(Str::SI_IDSTR_ALL_FILES_ASTRIX, all_files);

	//Actually add the extension of the supplied filename
	if (add_path_extension)
	{
		uni_char description[80];
		g_op_system_info->GetFileTypeName(extension, description, 80);

		if (!m_filters.GetCount() || m_settings->extension_filters.GetCount() == 1 && m_settings->extension_filters.Get(0)->media_type.Compare(all_files) == 0)
			m_start_with_extension = TRUE;

		OpString filter_name;
		RETURN_IF_ERROR(filter_name.AppendFormat(UNI_L("%s(%s)"), description, extension.CStr()));

		OpAutoPtr<FileFilter> file_filter = OP_NEW(FileFilter, (filter_name.CStr()));

		if (m_settings->initial_filter == -1)
		{
			file_filter->AddFilter(extension.CStr());
			RETURN_IF_ERROR(m_filters.Insert(0, file_filter.get()));
		}
		else
		{
			file_filter->AddFilter(extension.CStr());
			RETURN_IF_ERROR(m_filters.Add(file_filter.get()));
		}
		m_filename_ext = PathFindExtension(m_settings->initial_path.CStr());

		file_filter.release();
	}

	if (!m_filters.GetCount() && !(m_settings->fixed_filter))
	{
		OpAutoPtr<FileFilter> file_filter = OP_NEW(FileFilter, (all_files));

		RETURN_OOM_IF_NULL(file_filter.get());

		file_filter->AddFilter(UNI_L("*.*"));

		RETURN_IF_ERROR(m_filters.Add(file_filter.get()));
		file_filter.release();
	}

	RETURN_IF_ERROR(BuildFileFilter());

	return OpStatus::OK;
}

void WindowsFileChooserBase::AppendExtensionIfNeeded(OpString& current_name, UINT cur_name_len, DWORD max_file, DWORD ext_index)
{
	uni_char* default_ext = NULL;

	//Gets the extension to append to files if needed
	if (m_settings->action == DesktopFileChooserRequest::ACTION_FILE_SAVE || m_settings->action == DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE)
	{
		if (m_filename_ext && m_settings->initial_filter == -1)
		{
			if (ext_index == 1)
				ext_index = m_settings->extension_filters.GetCount()+1;
			else
				ext_index--;
		}

		OpFileSelectionListener::MediaType* filter = m_settings->extension_filters.Get(ext_index - 1);

		default_ext = (filter && filter->file_extensions.Get(0)) ? filter->file_extensions.Get(0)->CStr() : m_filename_ext;

		if (default_ext)
			default_ext = uni_strrchr(default_ext, '.');
		if (default_ext && (uni_strchr(default_ext, '*') || uni_strchr(default_ext, '?')))
			default_ext = NULL;

		// quoted filename will not be appended with default_ext
		BOOL quote = FALSE;
		if(cur_name_len >=2 && current_name.CStr()[0] == '"' && current_name.CStr()[cur_name_len-1] == '"')
			quote = TRUE;

		// whether current_name already has a correct extension 
		BOOL ext = FALSE;
		if(default_ext)
		{
			UINT ext_len = uni_strlen(default_ext);

			if(cur_name_len >= ext_len && uni_stricmp(current_name.CStr()+cur_name_len-ext_len,default_ext) == 0 )
				ext = TRUE;
		}

		if(!ext && filter)
		{
			for (UINT32 j = 0; j < filter->file_extensions.GetCount(); j++)
			{
				const uni_char* extension = uni_strrchr(filter->file_extensions.Get(j)->CStr(), '.') ;
				UINT ext_len = 0;
				if(extension)
					ext_len = uni_strlen(extension);
				if(ext_len && cur_name_len >= ext_len && uni_strcmp(extension,current_name.CStr()+cur_name_len-ext_len) == 0 )
				{
					ext = TRUE;								
					break;
				}
			}
		}

		if ( !ext && !quote && default_ext && cur_name_len > 0 && cur_name_len < max_file - uni_strlen(default_ext) && !PathIsDirectory(current_name)
			&& !PathIsUNCServer(current_name) && !PathIsUNCServerShare(current_name))
		{
			current_name.Append(default_ext);
		}
	}
}

void WindowsFileChooserBase::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (!m_settings)		//Cancel was called before we reached this
		return;

	OP_ASSERT(msg == FILE_CHOOSER_SHOW);

	g_input_manager->SetMouseInputContext(NULL);

	if (m_settings->action == DesktopFileChooserRequest::ACTION_DIRECTORY)
		ShowFolderSelector();
	else
		ShowFileSelector();
}

void WindowsFileChooserBase::PrepareFileSelection(OpString& initial_dir, OpString& file_names, DWORD& MaxFile, uni_char*& file_name_start)
{
	// value is converted to an unsigned short within COMDLG32.DLL, at least on W2K, so max 0xFFFF
	MaxFile = m_settings->action==DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI ? 0xFFFF : MAX_PATH;

	if (!file_names.Reserve(MaxFile))
		return;

	// Tests whether the initial path is valid and whether it is a directory

	if (m_settings->initial_path.HasContent())
	{
		uni_char *file_path = (uni_char*)m_settings->initial_path.CStr();
		if (!PathIsDirectory(file_path))
		{
			file_name_start = uni_strrchr(file_path, '\\');
			if (file_name_start && *file_name_start)
			{
				*file_name_start = 0;
				file_names.Set(file_name_start+1);
				if (PathIsDirectory(file_path))
				{
					initial_dir.Set(file_path);
				}
			}
			else
				file_names.Set(file_path);

			uni_char* wrong_char = file_names.CStr();
			while (*wrong_char)
			{
				if (*wrong_char < 32 || uni_strchr(UNI_L("/?*|:<>\"\\"), *wrong_char))
					*wrong_char = '_';

				wrong_char++;
			}
			// XP dialog doesn't like really long filenames, truncate it here to MaxFile. Fixes DSK-231980.
			if(file_names.Length() > (int)MaxFile - initial_dir.Length())
			{
				file_names.CStr()[MaxFile - initial_dir.Length() - 5] = '\0';	// leave room for extension
			}
		}
		else
		{
			initial_dir.Set(file_path);
		}
	}
	RemoveExtension(file_names);
}

void WindowsFileChooserBase::RemoveExtension(OpString& file_names)
{
	if (!m_start_with_extension && file_names.HasContent() && 
		(m_settings->action == DesktopFileChooserRequest::ACTION_FILE_SAVE || m_settings->action == DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE))
	{
		uni_char *extension = PathFindExtension(file_names.CStr());
		if (extension == uni_strchr(file_names.CStr(), '.'))
			*extension = 0;		//We do not propose a file extension when saving
	}
}

void WindowsFileChooserBase::ShowFolderSelector()
{
	BROWSEINFO browseInfo;
	LPITEMIDLIST itemIDList;
	OpAutoPtr<OpString> newpath(OP_NEW(OpString, ()));
	if (!newpath.get())
		return;

	HWND hWnd_parent = 0;
	if (IsWindow(m_parent_window))
	{
		uni_char class_name[32];
		if (GetClassName(m_parent_window, class_name, 17) && !uni_strncmp(class_name, UNI_L(OPERA_WINDOW_CLASSNAME), 16))
		{
			hWnd_parent = m_parent_window;
		}
	}

	if (!hWnd_parent)
	{
		DesktopWindow *active_desktop_window = g_application->GetActiveDesktopWindow();
		hWnd_parent = active_desktop_window ? ((WindowsOpWindow*)active_desktop_window->GetWindow())->m_hwnd : NULL;
	}
	if (!newpath->Reserve(MAX_PATH))
	{
		return;
	}
	ZeroMemory(&browseInfo, sizeof(browseInfo));

	browseInfo.hwndOwner		= hWnd_parent;
	browseInfo.pidlRoot			= NULL;
	browseInfo.pszDisplayName	= newpath->CStr();
	browseInfo.lpszTitle		= m_settings->caption.CStr();
	browseInfo.ulFlags			= BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	browseInfo.lpfn				= WindowsFileChooserBase::BrowseCallbackProc;

	browseInfo.lParam		= (LPARAM)(this);

	//Required for BIF_USENEWUI
	CoInitialize(NULL);

	// Show the dialog
	itemIDList = SHBrowseForFolder(&browseInfo);

	m_chooser_window = NULL;	//The dialog window was closed

	if (!m_settings)			//The dialog was cancelled by calling Cancel()
	{
		return;					//nothing more to do
	}

	// Did user press ok?
	if(itemIDList)
	{
		// Get the path from the returned ITEMIDLIST
		if(SHGetPathFromIDList(itemIDList, newpath->CStr()))
		{
			LPMALLOC	shellMalloc;
			HRESULT		hr;

			// Get pointer to the shell's malloc interface
			hr = SHGetMalloc(&shellMalloc);

			if (SUCCEEDED(hr)) 
			{
				// Free the shell's memory
				shellMalloc->Free(itemIDList);

				// Release the interface
				shellMalloc->Release();

				m_results.files.Add(newpath.get());
				m_results.active_directory.Set(*newpath.get());
				newpath.release(); // the ownership has changed
			}
		}
	}

	m_settings = NULL;
	m_listener->OnFileChoosingDone((DesktopFileChooser*)this->m_proxy, m_results);
	CoUninitialize();
}

int CALLBACK WindowsFileChooserBase::BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	WindowsFileChooserBase* chooser = reinterpret_cast<WindowsFileChooserBase *>(lpData);
	switch(uMsg)
	{
	case BFFM_INITIALIZED:
		{
			chooser->m_chooser_window = hwnd;
			const uni_char *path = chooser->m_settings->initial_path.CStr();
			if (path && PathIsDirectory(path))
			{
				// set the desired start path for the browse for folder call
				SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)(path));
			}
		}
		break;
	}
	return 0;
}

/*
* WindowsXPFileChooser - This is the XP version of the dialog
*/
WindowsXPFileChooser::WindowsXPFileChooser(WindowsOpFileChooserProxy* proxy) : WindowsFileChooserBase(proxy)
{
}

WindowsXPFileChooser::~WindowsXPFileChooser()
{
}


OP_STATUS WindowsXPFileChooser::BuildFileFilter()
{
	m_filter.Empty();

	for(UINT32 x = 0; x < m_filters.GetCount(); x++)
	{
		FileFilter *file_filter = m_filters.Get(x);
		if(x != 0)
		{
			RETURN_IF_ERROR(m_filter.AppendFormat(UNI_L("|%s|"), file_filter->GetDescription().CStr()));
		}
		else
		{
			RETURN_IF_ERROR(m_filter.AppendFormat(UNI_L("%s|"), file_filter->GetDescription().CStr()));
		}
		for(UINT32 y = 0; y < file_filter->GetFilterCount(); y++)
		{
			OpString* str = file_filter->GetFilter(y);
			if(y != 0)
			{
				RETURN_IF_ERROR(m_filter.AppendFormat(UNI_L(";%s"), str->CStr()));
			}
			else
			{
				RETURN_IF_ERROR(m_filter.Append(str->CStr()));
			}
		}
	}
	RETURN_IF_ERROR(m_filter.Append(UNI_L("|")));
	// Windows uses null chars, not |, but it's easier to use | in the append formats above. Replacing all of them here
	uni_char* filter_str = m_filter.CStr();
	for (UINT32 i = 0; filter_str[i] != 0; i++)
	{
		if (filter_str[i] == '|')
			filter_str[i] = 0;
	}
	return OpStatus::OK;
}

OpPointerHashTable<void, OPENFILENAME> WindowsXPFileChooser::m_subclassed_dialog_pointers;

UINT CALLBACK WindowsXPFileChooser::SubclassedDialogProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	OPENFILENAME *ofn;
	
	if (OpStatus::IsSuccess(WindowsXPFileChooser::m_subclassed_dialog_pointers.GetData(hWnd, &ofn)))
	{
		WindowsXPFileChooser* chooser = (WindowsXPFileChooser*)ofn->lCustData;

		BOOL no_compare = FALSE;
		OpString key;

		switch (uMsg)
		{

			case WM_NCDESTROY:
				WindowsXPFileChooser::m_subclassed_dialog_pointers.Remove(hWnd, &ofn);
				break;

			case WM_COMMAND:
			{
				//	WORD wNotifyCode = HIWORD(wParam); // notification code
					WORD wID = LOWORD(wParam);         // item, control, or accelerator identifier
				//	HWND hwndCtl = (HWND) lParam;      // handle of control

				if (wID == IDOK)
				{
					OpString current_name;

					current_name.Reserve(ofn->nMaxFile);

					UINT cur_name_len;
					cur_name_len = GetDlgItemText(hWnd, cmb13, current_name.CStr(), ofn->nMaxFile);

					chooser->AppendExtensionIfNeeded(current_name, cur_name_len, ofn->nMaxFile, ofn->nFilterIndex);

					SetDlgItemText(hWnd, cmb13, current_name.CStr());

					//(julienp)Below is the fix for bug DSK-205417 for XP. It is caused by a "feature" of windows to add to the programs
					//useable for OpenWith for an extension when they saved (and maybe opened) a file with that extension
					//through the standard file selection dialog. This is used to counter that effect as we don't
					//really want it (especially not for save).
					//This part checks if Opera is already set as handler for openwith. The second part (further down)
					//removes the handler after it has been added, if it was not already present here.

					do
					{
						if (cur_name_len > 0)
						{
							OP_STATUS status = key.Set(UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\"));
							if(OpStatus::IsError(status))
								break;
							
							int pos = current_name.FindLastOf('.');
							if(pos == KNotFound)
								break;
							
							OpStringC sub_str = current_name.SubString(pos, KAll, &status);
							if(OpStatus::IsError(status))
								break;

							status = key.Append(sub_str);									
							if(OpStatus::IsError(status))
								break;

							status = key.Append(UNI_L("\\OpenWithList"));
							if(OpStatus::IsSuccess(status))
							{				
								HKEY extregkey;
								DWORD n_values;
								DWORD max_name_length;
								DWORD max_data_length;
								if (RegOpenKeyEx(HKEY_CURRENT_USER, key.CStr(), 0, KEY_READ, &extregkey) == ERROR_SUCCESS &&
									RegQueryInfoKey(extregkey, NULL, NULL, NULL, NULL, NULL, NULL, &n_values, &max_name_length, &max_data_length, NULL, NULL) == ERROR_SUCCESS)
								{
	
									OpString value_name;
									if (!value_name.Reserve(max_name_length))
									{
										no_compare = TRUE;
										break;
									}
		
									OpString value_data;
									if (!value_data.Reserve(max_data_length/sizeof(uni_char)))
									{
										no_compare = TRUE;
										break;
									}

									for (UINT32 i = 0; i<n_values; i++)
									{
										DWORD name_length = max_name_length+1;
										DWORD data_length = max_data_length;
										DWORD value_type;

										if (RegEnumValue(extregkey, i, value_name.CStr(), &name_length, NULL, &value_type, (LPBYTE)value_data.CStr(), &data_length) == ERROR_SUCCESS &&
											(value_type== REG_SZ && value_name.Length()==1 /* exclude "MRUList" */))
										{
											if (value_data.CompareI(UNI_L("Opera.exe")) == 0)
											{
												no_compare = TRUE;
												break;
											}
										}
									}
									
									RegCloseKey(extregkey);
								}
							}
						}
						else
						{
							no_compare = TRUE;
						}
					}
					while (false);
				}
				break;
			}
		}
		if (chooser->m_dialog_default_proc)
		{
			LRESULT res = CallWindowProc( chooser->m_dialog_default_proc, hWnd, uMsg, wParam, lParam);

			if (uMsg == WM_COMMAND && LOWORD(wParam) == IDOK && !no_compare)
			{
				do
				{
					HKEY extregkey;
					DWORD n_values;
					DWORD max_name_length;
					DWORD max_data_length;
					if (RegOpenKeyEx(HKEY_CURRENT_USER, key.CStr(), 0, KEY_READ | KEY_SET_VALUE, &extregkey) == ERROR_SUCCESS &&
						RegQueryInfoKey(extregkey, NULL, NULL, NULL, NULL, NULL, NULL, &n_values, &max_name_length, &max_data_length, NULL, NULL) == ERROR_SUCCESS)
					{

						OpString value_name;
						if (!value_name.Reserve(max_name_length))
							break;

						OpString value_data;
						if (!value_data.Reserve(max_data_length/sizeof(uni_char)))
							break;

						for (UINT32 i = 0; i<n_values; i++)
						{
							DWORD name_length = max_name_length+1;
							DWORD data_length = max_data_length;
							DWORD value_type;

							if (RegEnumValue(extregkey, i, value_name.CStr(), &name_length, NULL, &value_type, (LPBYTE)value_data.CStr(), &data_length) == ERROR_SUCCESS &&
								(value_type== REG_SZ && value_name.Length()==1 /* exclude "MRUList" */))
							{
								if (value_data.CompareI(UNI_L("Opera.exe")) == 0)
								{
									OpString MRU_list;

									RegDeleteValue(extregkey, value_name.CStr());
									OpSafeRegQueryValue(HKEY_CURRENT_USER, key.CStr(), MRU_list, UNI_L("MRUList"));
									UINT position = MRU_list.Find(value_name);
									if (position != KNotFound)
									{
										MRU_list.Delete(position, 1);
										RegSetValueEx(extregkey, UNI_L("MRUList"), 0, REG_SZ, (BYTE*)MRU_list.CStr(), (MRU_list.Length()+1)*sizeof(uni_char));
									}
								}
							}
						}
					}
					RegCloseKey(extregkey);
				}
				while (false);
			}

			return res;
		}
	}
	return 0;
}

UINT CALLBACK WindowsXPFileChooser::FileChooserHook( HWND hDlgChild, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hDlg = GetParent(hDlgChild);

	if (uMsg == WM_NOTIFY && lParam)
	{
		OFNOTIFY *pofnotify = (OFNOTIFY*) lParam;
		if (((NMHDR)(pofnotify->hdr)).code == CDN_INITDONE )
		{
			WindowsXPFileChooser* chooser = reinterpret_cast<WindowsXPFileChooser*>(pofnotify->lpOFN->lCustData);
			WindowsXPFileChooser::m_subclassed_dialog_pointers.Add(hDlg, pofnotify->lpOFN);
			WNDPROC wpOld = (WNDPROC)SetWindowLongPtr((hDlg), GWLP_WNDPROC, (LPARAM)(WNDPROC)(WindowsXPFileChooser::SubclassedDialogProc));
			chooser->m_dialog_default_proc = wpOld;

			chooser->m_chooser_window = hDlg;
		}
	}
	return 0;	//	0 == process the message
}

void WindowsXPFileChooser::ShowFileSelector()
{
	if (!m_settings)			//The dialog was cancelled by calling Cancel()
		return;					//nothing more to do

	OpString initial_dir;
	OpString file_names;
	DWORD MaxFile;
	uni_char* file_name_start = NULL;

	PrepareFileSelection(initial_dir, file_names, MaxFile, file_name_start);

	HWND hWnd_parent = 0;

	if (IsWindow(m_parent_window))
	{
		uni_char class_name[32];
		if (GetClassName(m_parent_window, class_name, 17) && !uni_strncmp(class_name, UNI_L(OPERA_WINDOW_CLASSNAME), 16))
		{
			hWnd_parent = m_parent_window;
		}
	}

	if (!hWnd_parent)
	{
		DesktopWindow *active_desktop_window = g_application->GetActiveDesktopWindow();
		hWnd_parent = active_desktop_window ? ((WindowsOpWindow*)active_desktop_window->GetWindow())->m_hwnd : NULL;
	}

	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize			= sizeof (OPENFILENAME);

	ofn.hwndOwner			= hWnd_parent;
	ofn.hInstance			= NULL;
	ofn.lpstrFilter			= m_filter.CStr();
	ofn.lpstrCustomFilter	= NULL;
	ofn.nMaxCustFilter		= 0;
	// julienp:	Need to cast, pi specifies a 0-based int and the windows API specifies a 1-based DWORD
	//			Also m_initial_filter = -1 means the same nFilterIndex = 0
	ofn.nFilterIndex		= (DWORD)(m_settings->initial_filter+1);

	ofn.lpstrFile			= file_names.CStr();
	ofn.nMaxFile			= MaxFile;

	ofn.lpstrFileTitle		= NULL;
	ofn.nMaxFileTitle		= 0;

	ofn.lpstrInitialDir		= initial_dir;

	ofn.lpstrTitle			= m_settings->caption.CStr();

	DWORD dwFlags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

	if ( m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI)
		dwFlags |= OFN_ALLOWMULTISELECT;

	if ( m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN || m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI)
		dwFlags |= OFN_FILEMUSTEXIST;

	if ( m_settings->action == DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE )
		dwFlags |= OFN_OVERWRITEPROMPT;

	dwFlags |= OFN_LONGNAMES | OFN_EXPLORER | OFN_ENABLEHOOK | OFN_ENABLESIZING;

	ofn.Flags				= dwFlags;
	ofn.lCustData			= (LPARAM)this;
	ofn.lpfnHook			= (LPOFNHOOKPROC)WindowsXPFileChooser::FileChooserHook;


	BOOL result = FALSE;

	if (m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN || m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI)
		result = GetOpenFileName( (OPENFILENAME*) &ofn);
	else
		result = GetSaveFileName( (OPENFILENAME*) &ofn);

	m_chooser_window = NULL;	//The dialog window was closed

	if (file_name_start)
		*file_name_start = '\\';

	m_filters.DeleteAll();

	if (!m_proxy)		//The FileChooser proxy was destroyed
	{							//we should bail out as fast as possible
		OP_DELETE(this);
		return;
	}

	if (!m_settings)			//The dialog was cancelled by calling Cancel()
		return;					//nothing more to do

	if (!result)		//error
	{
		DWORD err = CommDlgExtendedError();

		if (!err == CDERR_GENERALCODES) // If the user did not cancel
		{
			OpString msg_top, msg_bottom, title, msg_full;

			if (OpStatus::IsSuccess(g_languageManager->GetString(Str::D_FILECHOOSER_INIT_ERROR, msg_top)) &&
				OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_DEF_STRING, msg_bottom)) &&
				OpStatus::IsSuccess(g_languageManager->GetString(Str::S_OPERA, title)) &&
				OpStatus::IsSuccess(msg_full.AppendFormat(msg_top.CStr(), err)) &&
				OpStatus::IsSuccess(msg_full.AppendFormat(UNI_L("\r\n\r\n%s"), msg_bottom.CStr())))
					OpStatus::Ignore(SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, msg_full.CStr(), title.CStr(), Dialog::TYPE_OK, Dialog::IMAGE_ERROR));
		}
		m_settings = NULL;
		m_listener->OnFileChoosingDone((DesktopFileChooser*)this->m_proxy, m_results);
	}
	else
	{
		if (m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI && PathIsDirectory(file_names.CStr()))
		{
			//	MULTIPLE SELECTION - First item is the directory only, followed by the file names

			m_results.active_directory.Set(file_names);
			uni_char* file_name= file_names.CStr() + uni_strlen(file_names.CStr());
			BOOL add_backslash = *(file_name - 1) != '\\';
			file_name++;
			while (*file_name)
			{
				OpString* file = new OpString();
				if (file)
				{
					file->AppendFormat(UNI_L("%s%s%s"), file_names.CStr(), add_backslash?UNI_L("\\"):UNI_L(""), file_name);
					m_results.files.Add(file);
					file_name += uni_strlen(file_name) + 1;
				}
			}
		}
		else
		{
			//	SINGLE SELECTION - first item is full path
			OpString *file = new OpString();
			if (file)
			{
				file->Set(file_names);
				m_results.files.Add(file);
				int last_backslash = file_names.FindLastOf('\\');
				file_names[last_backslash] = 0;
				m_results.active_directory.Set(file_names);
				file_names[last_backslash] = '\\';
			}
		}

		if (m_settings->get_selected_filter)
			m_results.selected_filter = ofn.nFilterIndex-1;

		m_settings = NULL;
		m_listener->OnFileChoosingDone((DesktopFileChooser*)this->m_proxy, m_results);
	}
}

void WindowsXPFileChooser::Cancel()
{
	m_settings = NULL;

	if (m_chooser_window)
		SendMessage(m_chooser_window, WM_COMMAND, IDCANCEL, NULL);	//Closes the filechooser
}

void WindowsXPFileChooser::Destroy()
{
	if (m_chooser_window)
	{
		m_proxy = NULL;
		if (m_settings)				//Trying to destroy without having cancelled
			Cancel();
	}
	else
		OP_DELETE(this);
}

/******************************************************************************
*
* WindowsOpFileChooser - Vista and higher specific dialogs
*
******************************************************************************/

WindowsOpFileChooser::WindowsOpFileChooser(WindowsOpFileChooserProxy* proxy) : WindowsFileChooserBase(proxy), m_file_filters(NULL), m_file_filters_count(0), m_event_cookie(0)
{

}

WindowsOpFileChooser::~WindowsOpFileChooser()
{
	FreeFilterSpecArray();
}

// convert a IShellItem path to a OpString normalized path
BOOL PathFromShellItem(IShellItem* item, OpString& path )
{
	HRESULT hr;
	LPOLESTR pwsz = NULL;

	hr = item->GetDisplayName(SIGDN_FILESYSPATH, &pwsz);

	if ( FAILED(hr) )
		return false;

	path.Set(pwsz);
	CoTaskMemFree ( pwsz );

	return TRUE;
}

// Create a IShellItem from a normalized OpString path
BOOL ShellItemFromPath(OpString& path, OpComPtr<IShellItem>& item)
{
	HRESULT hr = OPSHCreateItemFromParsingName(path.CStr(), NULL, __uuidof(IShellItem), reinterpret_cast<void **>(&item.p));
	if(FAILED(hr))
	{
		return FALSE;
	}
	return TRUE;
}

// free allocated filters in the COMDLG_FILTERSPEC C-array format
void WindowsOpFileChooser::FreeFilterSpecArray()
{
	if(!m_file_filters)
		return;

	COMDLG_FILTERSPEC *filter = m_file_filters;
	for(UINT32 x = 0; x < m_file_filters_count; x++)
	{
		OP_DELETEA(filter->pszName);
		OP_DELETEA(filter->pszSpec);

		filter++;
	}
	OP_DELETEA(m_file_filters);
	m_file_filters = NULL;
	m_file_filters_count = 0;
}

// build a COMDLG_FILTERSPEC array based on the vector of file types and extensions
OP_STATUS WindowsOpFileChooser::BuildFileFilter()
{
	FreeFilterSpecArray();

	if(!m_filters.GetCount())
		return OpStatus::OK;

	m_file_filters_count = m_filters.GetCount();
	m_file_filters = OP_NEWA(COMDLG_FILTERSPEC, m_file_filters_count);
	if(!m_file_filters)
		return OpStatus::ERR_NO_MEMORY;

	op_memset(m_file_filters, 0, sizeof(COMDLG_FILTERSPEC) * m_file_filters_count);

	COMDLG_FILTERSPEC *file_filters = m_file_filters;

	UINT32 x;
	for(x = 0; x < m_file_filters_count; x++)
	{
		OpString filters;
		FileFilter *file_filter = m_filters.Get(x);

		file_filters->pszName = uni_strdup(file_filter->GetDescription().CStr());

		for(UINT32 y = 0; y < file_filter->GetFilterCount(); y++)
		{
			OpString* str = file_filter->GetFilter(y);
			if(y != 0)
			{
				RETURN_IF_ERROR(filters.AppendFormat(UNI_L(";%s"), str->CStr()));
			}
			else
			{
				RETURN_IF_ERROR(filters.Append(str->CStr()));
			}
		}
		file_filters->pszSpec = uni_strdup(filters.CStr());
		file_filters++;
	}
	return OpStatus::OK;
}

void WindowsOpFileChooser::ShowFileSelector()
{
	if (!m_settings)			
		return;	

	OpString initial_dir;
	OpString file_names;
	DWORD MaxFile;
	uni_char* file_name_start = NULL;

	PrepareFileSelection(initial_dir, file_names, MaxFile, file_name_start);

	HWND hWnd_parent = 0;

	if (IsWindow(m_parent_window))
	{
		uni_char class_name[32];
		if (GetClassName(m_parent_window, class_name, 17) && !uni_strncmp(class_name, UNI_L(OPERA_WINDOW_CLASSNAME), 16))
		{
			hWnd_parent = m_parent_window;
		}
	}

	if (!hWnd_parent)
	{
		DesktopWindow *active_desktop_window = g_application->GetActiveDesktopWindow();
		hWnd_parent = active_desktop_window ? ((WindowsOpWindow*)active_desktop_window->GetWindow())->m_hwnd : NULL;
	}
	HRESULT hr;
	if (m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN || m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI)
	{
		OpComPtr<IFileOpenDialog> dlg;
		hr = dlg.CoCreateInstance(__uuidof(FileOpenDialog));
		if(SUCCEEDED(hr))
		{
			m_active_dialog = dlg;
		}
	}
	else
	{
		OpComPtr<IFileSaveDialog> dlg;
		hr = dlg.CoCreateInstance(__uuidof(FileSaveDialog));
		if(SUCCEEDED(hr))
		{
			m_active_dialog = dlg;
		}
	}
	OP_ASSERT(SUCCEEDED(hr));
	if(FAILED(hr))
	{
		return;
	}
	m_active_dialog->SetFileTypes(m_filters.GetCount(), m_file_filters);
	m_active_dialog->SetTitle(m_settings->caption.CStr());

	OpComPtr<IShellItem> initial_dir_item;

	if(initial_dir.HasContent() && ShellItemFromPath(initial_dir, initial_dir_item))
	{
		m_active_dialog->SetDefaultFolder(initial_dir_item);
	}
	// julienp:	Need to cast, pi specifies a 0-based int and the windows API specifies a 1-based DWORD
	//			Also m_initial_filter = -1 means the same file type index = 0
	m_active_dialog->SetFileTypeIndex((UINT)m_settings->initial_filter + 1);

	m_active_dialog->SetFileName(file_names.CStr());

	OpString ext;

	// we modify it so make a copy
	FileFilter *filter = m_settings->initial_filter > -1 ? m_filters.Get(m_settings->initial_filter) : 
											(m_filters.GetCount() ? m_filters.Get(0) : NULL);
	if(filter && filter->GetFilter(0) && OpStatus::IsSuccess(ext.Set(*filter->GetFilter(0))))
	{
		if(ext.CStr()[0] == '*')
		{
			// strip off the first *
			ext.Set(ext.SubString(1));
		}
		if(ext.CStr()[0] == '.')
		{
			// strip off the first .
			ext.Set(ext.SubString(1));
		}
		m_active_dialog->SetDefaultExtension(ext.CStr());
	}
	FILEOPENDIALOGOPTIONS dwFlags = FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM;

	if ( m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI)
		dwFlags |= FOS_ALLOWMULTISELECT;

	if ( m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN || m_settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI)
		dwFlags |= FOS_FILEMUSTEXIST;

	if ( m_settings->action == DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE )
		dwFlags |= FOS_OVERWRITEPROMPT;

	m_active_dialog->SetOptions(dwFlags);

	OpAutoPtr<WindowsVistaDialogEventHandler> obj(OP_NEW(WindowsVistaDialogEventHandler, ()));
	OpComQIPtr<IFileDialogEvents> events((IUnknown *)obj.get());

	obj->m_results_ref = &m_results;
	obj->m_settings = m_settings;
	obj->m_chooser = this;

	hr = m_active_dialog->Advise(events, &m_event_cookie);

	BOOL advised = SUCCEEDED(hr);
	if(advised)
	{
		// the dialog now owns it
		obj.release();
	}

	m_active_dialog->Show(hWnd_parent);

	// extra checks, we might have been forcefully closed in the call to Show
	if(m_event_cookie && advised && m_active_dialog.p)
	{
		m_active_dialog->Unadvise(m_event_cookie);
	}

	if (file_name_start)
		*file_name_start = '\\';

	UINT selected_type = 0;
	if(m_settings && m_settings->get_selected_filter && SUCCEEDED(m_active_dialog->GetFileTypeIndex(&selected_type)) && selected_type != 0)
	{
		m_results.selected_filter = selected_type - 1;
	}
	FreeFilterSpecArray();

	m_active_dialog.Release();

	if (!m_proxy)		//The FileChooser proxy was destroyed
	{							//we should bail out as fast as possible
		OP_DELETE(this);
		return;
	}

	if (!m_settings)			//The dialog was cancelled by calling Cancel()
		return;					//nothing more to do

	m_settings = NULL;
	m_listener->OnFileChoosingDone((DesktopFileChooser*)this->m_proxy, m_results);
}

void WindowsOpFileChooser::Cancel()
{
	m_settings = NULL;

	if(m_event_cookie)
	{
		if (m_active_dialog.p)
		{
			m_active_dialog->Unadvise(m_event_cookie);
		}
		m_event_cookie = 0;
	}
	if(m_active_dialog.p)
	{
		/* HRESULT hr = */ m_active_dialog->Close(S_FALSE);
	}
}

void WindowsOpFileChooser::Destroy()
{
	if (m_active_dialog.p)
	{
		m_proxy = NULL;
		if (m_settings)				//Trying to destroy without having cancelled
			Cancel();
	}
	else
		OP_DELETE(this);
}

/******************************************************************************
*
* Support methods
*
******************************************************************************/

OP_STATUS FileFilter::AddFilter(const OpStringC& filter)
{
	OpAutoPtr<OpString> str(OP_NEW(OpString, ()));
	if(!str.get()) 
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	RETURN_IF_ERROR(str->Set(filter));
	RETURN_IF_ERROR(m_filters.Add(str.get()));
	str.release();

	return OpStatus::OK;
}

/******************************************************************************
*
* Event handler for dialog
*
******************************************************************************/

HRESULT WindowsVistaDialogEventHandler::OnFileOk(IFileDialog* dlg)
{
	const DesktopFileChooserRequest *settings = m_settings;
	OpComPtr<IShellItem> single_item;

	HRESULT hr = dlg->GetResult(&single_item);
	if(SUCCEEDED(hr))
	{
		// get the file name from the shell item
		
		OpString *file = OP_NEW(OpString, ());
		if (file && PathFromShellItem(single_item, *file))
		{
			m_results_ref->files.Add(file);
		}
	}
	else
	{
		if (settings->action == DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI)
		{
			OpComQIPtr<IFileOpenDialog> dlg_open = dlg;

			// multiple items selected?
			OpComPtr<IShellItemArray> item_array;
			hr = dlg_open->GetResults(&item_array);
			if(SUCCEEDED(hr))
			{
				DWORD count;

				hr = item_array->GetCount(&count);
				if(SUCCEEDED(hr))
				{
					for(DWORD x = 0; x < count; x++)
					{
						OpComPtr<IShellItem> item;

						hr = item_array->GetItemAt(x, &item);
						if(SUCCEEDED(hr))
						{
							// get the file name from the shell item
							OpString *file = OP_NEW(OpString, ());
							if (file && PathFromShellItem(item, *file))
							{
								m_results_ref->files.Add(file);
							}
						}
					}
				}
			}
		}
	}

	return S_OK;
}
HRESULT WindowsVistaDialogEventHandler::OnFolderChanging(IFileDialog* pfd, IShellItem* psiFolder)
{
	return S_OK;
}
HRESULT WindowsVistaDialogEventHandler::OnFolderChange(IFileDialog* dlg)
{
	OpComPtr<IShellItem> single_item;

	HRESULT hr = dlg->GetFolder(&single_item);
	if(SUCCEEDED(hr))
	{
		OpString path;
		if(PathFromShellItem(single_item, path))
		{
			m_results_ref->active_directory.Set(path.CStr());
		}
	}
	return hr;
}
HRESULT WindowsVistaDialogEventHandler::OnSelectionChange(IFileDialog* pfd)
{
	return S_OK;
}
HRESULT WindowsVistaDialogEventHandler::OnShareViolation(IFileDialog* pfd, IShellItem* psi, FDE_SHAREVIOLATION_RESPONSE* pResponse)
{
	return S_OK;
}
HRESULT WindowsVistaDialogEventHandler::OnTypeChange(IFileDialog* pfd)
{
	return S_OK;
}
HRESULT WindowsVistaDialogEventHandler::OnOverwrite(IFileDialog* pfd, IShellItem* psi, FDE_OVERWRITE_RESPONSE* pResponse)
{
	return S_OK;
}

HRESULT WindowsVistaDialogEventHandler::QueryInterface(REFIID iid, void ** pvvObject)
{
	if (iid == IID_IUnknown || iid == IID_IDispatch || iid == IID_IFileDialogEvents)
	{
		*pvvObject = static_cast<IFileDialogEvents *>(this);
		AddRef();
		return S_OK;
	}
	*pvvObject=NULL;
	return E_NOINTERFACE;
}

ULONG WindowsVistaDialogEventHandler::AddRef()
{
	return ++m_ref_counter;
}

ULONG WindowsVistaDialogEventHandler::Release()
{
	if (!(--m_ref_counter))
	{
		OP_DELETE(this);
		return 0;
	}
	return m_ref_counter;
}

