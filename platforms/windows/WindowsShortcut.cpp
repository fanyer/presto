/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "platforms/windows/WindowsShortcut.h"

#include "adjunct/desktop_util/shortcuts/DesktopShortcutInfo.h"
#include "adjunct/quick/managers/FavIconManager.h"

#include "modules/pi/opbitmap.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/path.h"

#include "platforms/windows/pi/WindowsOpDesktopResources.h"
#include "platforms/windows/utils/win_icon.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/utils/com_helpers.h"

WindowsShortcut::WindowsShortcut()
	: m_shortcut_info(NULL)
{
}


OP_STATUS WindowsShortcut::Init(const DesktopShortcutInfo& shortcut_info)
{
	m_shortcut_info = &shortcut_info;
	
	RETURN_IF_ERROR(MakePaths());

	return OpStatus::OK;
}


OP_STATUS WindowsShortcut::Init(const DesktopShortcutInfo& shortcut_info,
		const OpStringC& shortcut_path)
{
	RETURN_IF_ERROR(Init(shortcut_info));
	RETURN_IF_ERROR(m_shortcut_path.Set(shortcut_path));
	return OpStatus::OK;
}


OpStringC WindowsShortcut::GetShortcutFilePath() const
{
	return m_shortcut_path;
}


OP_STATUS WindowsShortcut::MakePaths()
{
	OP_ASSERT(NULL != m_shortcut_info);

	switch (m_shortcut_info->m_destination)
	{
		case DesktopShortcutInfo::SC_DEST_DESKTOP:
			RETURN_IF_ERROR(WindowsOpDesktopResources::GetFolderPath(FOLDERID_Desktop, CSIDL_DESKTOPDIRECTORY, m_shortcut_path));
			break;
		case DesktopShortcutInfo::SC_DEST_COMMON_DESKTOP:
			RETURN_IF_ERROR(WindowsOpDesktopResources::GetFolderPath(FOLDERID_PublicDesktop, CSIDL_COMMON_DESKTOPDIRECTORY, m_shortcut_path));
			break;
		case DesktopShortcutInfo::SC_DEST_MENU:
			RETURN_IF_ERROR(WindowsOpDesktopResources::GetFolderPath(FOLDERID_Programs, CSIDL_PROGRAMS, m_shortcut_path));
			break;
		case DesktopShortcutInfo::SC_DEST_COMMON_MENU:
			RETURN_IF_ERROR(WindowsOpDesktopResources::GetFolderPath(FOLDERID_CommonPrograms, CSIDL_COMMON_PROGRAMS, m_shortcut_path));
			break;
		case DesktopShortcutInfo::SC_DEST_QUICK_LAUNCH:
			RETURN_IF_ERROR(WindowsOpDesktopResources::GetFolderPath(FOLDERID_QuickLaunch, CSIDL_APPDATA, m_shortcut_path));

			if (!IsSystemWinVista())
			{
				RETURN_IF_ERROR(m_shortcut_path.Append("\\Microsoft\\Internet Explorer\\Quick Launch"));

				OpFile quick_launch_folder;
				RETURN_IF_ERROR(quick_launch_folder.Construct(m_shortcut_path));

				BOOL exists;
				if (OpStatus::IsError(quick_launch_folder.Exists(exists)) || !exists)
					RETURN_IF_ERROR(quick_launch_folder.MakeDirectory());
			}
			break;
		case DesktopShortcutInfo::SC_DEST_TEMP:
			{
				OpFile temp;
				RETURN_IF_ERROR(temp.Construct(UNI_L(""), OPFILE_TEMP_FOLDER));
				RETURN_IF_ERROR(m_shortcut_path.Set(temp.GetFullPath()));
			}
			break;
		case DesktopShortcutInfo::SC_DEST_NONE:
			m_shortcut_path.Empty();
			break;
		default:
			OP_ASSERT(!"Unknown shortcut destination");
			return OpStatus::ERR;
	}

	if (m_shortcut_path.HasContent())
	{
		RETURN_IF_ERROR(m_shortcut_path.AppendFormat(UNI_L("\\%s.lnk"),
					m_shortcut_info->m_file_name.CStr()));
	}

	return OpStatus::OK;
}


OP_STATUS WindowsShortcut::Deploy()
{
	OP_ASSERT(NULL != m_shortcut_info);
	
	OP_ASSERT(m_shortcut_path.HasContent());
	
	if (m_shortcut_path.IsEmpty())
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	OP_STATUS result = OpStatus::ERR;
	m_hr = S_OK;
	m_err_loc = 0;
	OpComPtr<IShellLink> shell_link;
	OpComPtr<IPersistFile> persist_file;

	do
	{
		HRESULT m_hr = shell_link.CoCreateInstance(CLSID_ShellLink);
		if(FAILED(m_hr))
			break;
		m_err_loc++;

		m_hr = shell_link->SetPath(m_shortcut_info->m_command.CStr());
		if(FAILED(m_hr))
			break;
		m_err_loc++;

		m_hr = shell_link->SetWorkingDirectory(m_shortcut_info->m_working_dir_path.CStr());
		if(FAILED(m_hr))
			break;
		m_err_loc++;

		m_hr = shell_link->SetDescription(m_shortcut_info->m_comment.CStr());
		if(FAILED(m_hr))
			break;
		m_err_loc++;

		if(m_shortcut_info->m_icon_name.HasContent())
		{
			m_hr = shell_link->SetIconLocation(m_shortcut_info->m_icon_name.CStr(), m_shortcut_info->m_icon_index);
			if(FAILED(m_hr))
				break;
			m_err_loc++;
		}

		m_hr = shell_link.QueryInterface<IPersistFile>(&persist_file);
		if(FAILED(m_hr))
			break;
		m_err_loc++;

		m_hr = persist_file->Save(m_shortcut_path.CStr(), TRUE);
		if(FAILED(m_hr))
			break;
		m_err_loc++;

		result = OpStatus::OK;
	}
	while (FALSE);
	
	return result;
}

OP_STATUS WindowsShortcut::ExecuteContextmenuVerb(const uni_char *app_path, const char *verb)
{
	// This method accepts only full/absolute path to an application
	uni_char* path_sep = uni_strrchr(app_path, UNI_L(PATHSEPCHAR));
	if (!path_sep || *(path_sep+1) == '\0')
		return OpStatus::ERR;

	OpString file_part;
	RETURN_IF_ERROR(file_part.Set(path_sep + 1)); //Take out the path seperator

	OpString dir_part;
	RETURN_IF_ERROR(dir_part.Set(app_path, path_sep - app_path));

	OP_STATUS status = OpStatus::ERR;
	LPSHELLFOLDER pdf = NULL, psf = NULL;
	LPITEMIDLIST pidl = NULL, pitm = NULL;
	LPCONTEXTMENU pcm = NULL;
	HMENU hmenu = NULL;

	if (SUCCEEDED(SHGetDesktopFolder(&pdf))
		&& SUCCEEDED(pdf->ParseDisplayName(NULL, NULL, dir_part.CStr(), NULL, &pidl,  NULL))
		&& SUCCEEDED(pdf->BindToObject(pidl, NULL, IID_IShellFolder, (void **)&psf))
		&& SUCCEEDED(psf->ParseDisplayName(NULL, NULL, file_part.CStr(), NULL, &pitm,  NULL))
		&& SUCCEEDED(psf->GetUIObjectOf(NULL, 1, (LPCITEMIDLIST *)&pitm, IID_IContextMenu, NULL, (void **)&pcm))
		&& (hmenu = CreatePopupMenu()) != NULL
		&& SUCCEEDED(pcm->QueryContextMenu(hmenu, 0, 1, INT_MAX, CMF_NORMAL)))
	{
		CMINVOKECOMMANDINFO ici = { sizeof(CMINVOKECOMMANDINFO), 0 };
		ici.hwnd = NULL;
		ici.lpVerb = verb;
		if (SUCCEEDED(pcm->InvokeCommand(&ici)))
			status = OpStatus::OK;
	}

	if (hmenu)
		DestroyMenu(hmenu);
	if (pcm)
		pcm->Release();
	if (pitm)
		CoTaskMemFree(pitm);
	if (psf)
		psf->Release();
	if (pidl)
		CoTaskMemFree(pidl);
	if (pdf)
		pdf->Release();

	return status;
}

/*
All currently pinned programs have a symbolic link in the folder 
C:\Users\Username\AppData\Roaming\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar. 
Solution to check if a program is pinned or not is to look if one of the symbolic links is 
pointing to the program's executable:
1. Get the first symbolic link from the '...\TaskBar' folder
2. Retrieve the path to the exe the symbolic link is pointing to
3. Compare the path with the path I am looking for, if equal, stop and return true
4. If not, get the next symbolic link
*/
OP_STATUS WindowsShortcut::CheckIfApplicationIsPinned(const uni_char *app_abs_path, BOOL &is_pinned)
{
	is_pinned = FALSE;

	OP_STATUS status = OpStatus::ERR; 
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	//Get User Pinned folder:
	//C:\Users\Username\AppData\Roaming\Microsoft\Internet Explorer\Quick Launch\User Pinned
	OpString user_pinned_path;
	RETURN_IF_ERROR(WindowsOpDesktopResources::GetFolderPath(FOLDERID_UserPinned, 0, user_pinned_path, FALSE));
	
	//Append '\TaskBar\*.lnk'
	OpString user_pinned_path_ex;
	RETURN_IF_ERROR(user_pinned_path_ex.Set(user_pinned_path));
	RETURN_IF_ERROR(user_pinned_path_ex.Append(UNI_L("\\TaskBar\\*.lnk")));

	//Get all symbolic links in the directory until a link is pointing to 'app_abs_path'
	hFind = FindFirstFile(user_pinned_path_ex.CStr(), &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
		return OpStatus::ERR;

	do
	{
		if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			//Create a string with the complete path to the link
			OpString link_path;
			if (OpStatus::IsError(link_path.Set(user_pinned_path))
				|| OpStatus::IsError(link_path.Append(L"\\TaskBar\\")) 
				|| OpStatus::IsError(link_path.Append(ffd.cFileName)))
				break;

			//Get the path the link is pointing to
			OpString link_target;
			if (OpStatus::IsSuccess(GetPathLinkIsPointingTo(link_path, link_target)) 
				&& link_target.HasContent())
			{
				//If the returned path is the same as 'app_abs_path', leave the loop
				if (link_target.CompareI(app_abs_path) == 0)
				{
					is_pinned = TRUE;				
					status = OpStatus::OK;
					break;
				}
			}
		}
	}
	while (FindNextFile(hFind, &ffd) != 0);

	FindClose(hFind);

	return status;
}

//Get the path a given symbolic link is pointing to, using IShellLink 
OP_STATUS WindowsShortcut::GetPathLinkIsPointingTo(const OpString &link_file, OpString &app_path) 
{ 
	OP_STATUS status = OpStatus::ERR;
	uni_char path[MAX_PATH]; 
	IShellLink* psl = NULL;
	IPersistFile* ppf = NULL;
	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl))
		&& SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))
		&& SUCCEEDED(ppf->Load(link_file.CStr(), STGM_READ))
		&& SUCCEEDED(psl->Resolve(NULL, SLR_NO_UI))
		&& SUCCEEDED(psl->GetPath(path, MAX_PATH, NULL, SLGP_UNCPRIORITY)))
	{
		if (OpStatus::IsSuccess(app_path.Set(path)))
			status = OpStatus::OK;
	}

	if (ppf)
		ppf->Release();
	if (psl)
		psl->Release(); 

	return status;
}