//  ___________________________________________________________________________
//  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
//  FILENAME    WinShell.cpp
//
//  CREATED     DG-191298
//
//  DESCRIPION  Win95/NT4.0 ++ shell interface functions
//              I.e. stuff using the IShellFolder object, etc.
//
//              Win32 only
//  ___________________________________________________________________________
//  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?

#include "core/pch.h"

#include <limits.h>

#include "platforms/windows/win_handy.h"
#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows/windows_ui/registry.h"
#include "platforms/windows_common/utils/fileinfo.h"
#include "platforms/windows/utils/com_helpers.h"
#include "platforms/windows/utils/win_icon.h"
#include "platforms/windows/WindowsShortcut.h"
#include "adjunct/desktop_util/shortcuts/DesktopShortcutInfo.h"

#include "modules/util/str.h"
#include "modules/util/gen_str.h"
#include "modules/util/opfile/opfile.h"
#include "modules/url/url_man.h"

extern HINSTANCE hInst;

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
//	GetShellProtocolHandler
//	___________________________________________________________________________
//
BOOL GetShellProtocolHandler
(
	IN	const uni_char*	pszProtocol,			//	IN	e.g "mailto"	(an opt. trailing colon is ok)
	OUT	uni_char *	pszAppDescription,			//	OUT	e.g "Eudora"	(not always awailable)
	IN	int			nMaxAppDescriptionLen,		//	IN	max strlen() for 'pszAppDescription'
	OUT	uni_char *	pszOpenCommand,				//	OUT	value of HKEY_CLASSES_ROOT/shell/open/command
	IN	int			nMaxOpenCommandLen			//	IN	max strlen() for 'pszOpenCommand'
)
{
	OpString protocol;
	if (OpStatus::IsError(protocol.Set(pszProtocol)))
		return FALSE;

	//
	//	Reset
	//
	if (pszAppDescription && nMaxAppDescriptionLen>0)
		*pszAppDescription = 0;
	if (pszOpenCommand && nMaxOpenCommandLen>0)
		*pszOpenCommand = 0;
	//
	//	Get rid of trailing ':' if present
	//
	if (IsStr(protocol))
		uni_strtok(protocol, UNI_L(": /\\"));

	//
	//	Verify params
	//
	if (protocol.IsEmpty())
		return FALSE;

	//
	//	Get the HKEY_CLASSES_ROOT/shell/open/command value
	//

	uni_char szKeyName[MAX_PATH] = UNI_L("");
	uni_char szText[MAX_PATH] = UNI_L("");
	uni_char szCommand[MAX_PATH] = UNI_L("");

	if (OpRegReadStrValue(HKEY_CLASSES_ROOT, protocol.CStr(), UNI_L("URL Protocol"), szText, sizeof(szText)) != ERROR_SUCCESS)
		return FALSE;

	uni_sprintf(szKeyName, UNI_L("%s\\shell\\open\\command"), protocol.CStr());

	if (OpRegReadStrValue( HKEY_CLASSES_ROOT, szKeyName, NULL, szCommand, sizeof(szCommand)) != ERROR_SUCCESS || !IsStr(szCommand))
		return FALSE;

	//
	//	Protcol handler command found
	//
	if (pszOpenCommand)
	{
		OpString application;
		if (!application.Reserve(MAX_PATH))
			return TRUE;
		
		ExpandEnvironmentStrings(szCommand, application, application.Capacity());
		uni_strlcpy(pszOpenCommand, application, nMaxOpenCommandLen);
	}

	//
	//	Try to find the description.
	//
	if (pszAppDescription)
	{
		BOOL fClientFound= TRUE;
		BOOL fIsMailto	= uni_strni_eq(protocol, "MAILTO", 7);
		// BOOL fIsNews	= uni_strni_eq(protocol, "NEWS", 5);
		// BOOL fIsNntp	= uni_strni_eq(protocol, "NNTP", 5);

		uni_strcpy(szKeyName, UNI_L("SOFTWARE\\Clients"));
		if (fIsMailto)
			uni_strcat(szKeyName, UNI_L("\\Mail"));
		else
			fClientFound = FALSE;	//	DGTODO - don't know how to handle this properly
		
		if (fClientFound)
		{
			//
			//	Enum all clients to find the matching one.
			//
			OpAutoHKEY keyClients;
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_READ, &keyClients) == ERROR_SUCCESS)
			{
				DWORD index = 0;
				bool fStopEnum = false;

				do
				{
					uni_char szClient[MAX_PATH] = UNI_L("");
					DWORD size = MAX_PATH;

					if (RegEnumKeyEx(keyClients, index, szClient, &size, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
						break;

					uni_char szCmd[MAX_PATH] = UNI_L("");
					uni_char szKey[MAX_PATH] = UNI_L("");
						
					uni_snprintf(szKey, MAX_PATH, UNI_L("%s\\Protocols\\%s\\shell\\open\\command"), szClient, protocol);
					if ( OpRegReadStrValue( keyClients, szKey, NULL, szCmd, sizeof(szCmd)) == ERROR_SUCCESS)
					{
						if (uni_stricmp(szCmd, szCommand) == 0)
						{
							//
							//	found it
							//
							OpRegReadStrValue( keyClients, szClient, NULL, pszAppDescription, nMaxAppDescriptionLen);
							fStopEnum = true;
						}
					}

					++ index;

				} while (!fStopEnum );
			}
		}
		else
		{		
			//Just try to extract the description directly from the application
			OpString applicationpath;
			if (!applicationpath.Reserve(MAX_PATH))
				return TRUE;

			uni_sprintf(szKeyName, UNI_L("%s\\shell\\open\\command"), protocol.CStr());
			if (OpRegReadStrValue( HKEY_CLASSES_ROOT, szKeyName, NULL, applicationpath, applicationpath.Capacity()) == ERROR_SUCCESS)
			{
				//The applicationpath string can contain parameters etc. try to strip these
				OpString application;
				if (!application.Reserve(MAX_PATH))
					return TRUE;

				uni_char *applicationpath_stripped = szCommand;
				OpString app;
				if(applicationpath.FindFirstOf('\"') == 0)
				{
					OpStringC app_tmp(applicationpath.CStr()+1);
					int pos2 = app_tmp.FindFirstOf('\"');
					if(pos2 != KNotFound)
					{
						app.Set(app_tmp, pos2);
						applicationpath_stripped = app.CStr();
					}
				}
				ExpandEnvironmentStrings(applicationpath_stripped, application, application.Capacity());

				OpString desc;
				if (!desc.Reserve(MAX_PATH))
					return TRUE;

				GetFileDescription(application, &desc);

				if (desc.HasContent())
				{
					uni_strlcpy(pszAppDescription, desc, nMaxAppDescriptionLen);
				}

			}
		}

	}	
	return TRUE;
}


enum {
    ASSOCF_INIT_NOREMAPCLSID           = 0x00000001,  //  do not remap clsids to progids
    ASSOCF_INIT_BYEXENAME              = 0x00000002,  //  executable is being passed in
    ASSOCF_OPEN_BYEXENAME              = 0x00000002,  //  executable is being passed in
    ASSOCF_INIT_DEFAULTTOSTAR          = 0x00000004,  //  treat "*" as the BaseClass
    ASSOCF_INIT_DEFAULTTOFOLDER        = 0x00000008,  //  treat "Folder" as the BaseClass
    ASSOCF_NOUSERSETTINGS              = 0x00000010,  //  dont use HKCU
    ASSOCF_NOTRUNCATE                  = 0x00000020,  //  dont truncate the return string
    ASSOCF_VERIFY                      = 0x00000040,  //  verify data is accurate (DISK HITS)
    ASSOCF_REMAPRUNDLL                 = 0x00000080,  //  actually gets info about rundlls target if applicable
    ASSOCF_NOFIXUPS                    = 0x00000100,  //  attempt to fix errors if found
    ASSOCF_IGNOREBASECLASS             = 0x00000200,  //  dont recurse into the baseclass
	ASSOCF_INIT_IGNOREUNKNOWN          = 0x00000400,  //  dont use the "Unknown" progid, instead fail
};
typedef DWORD ASSOCF;
typedef enum {
    ASSOCSTR_COMMAND      = 1,  //  shell\verb\command string
    ASSOCSTR_EXECUTABLE,        //  the executable part of command string
    ASSOCSTR_FRIENDLYDOCNAME,   //  friendly name of the document type
    ASSOCSTR_FRIENDLYAPPNAME,   //  friendly name of executable
    ASSOCSTR_NOOPEN,            //  noopen value
    ASSOCSTR_SHELLNEWVALUE,     //  query values under the shellnew key
    ASSOCSTR_DDECOMMAND,        //  template for DDE commands
    ASSOCSTR_DDEIFEXEC,         //  DDECOMMAND to use if just create a process
    ASSOCSTR_DDEAPPLICATION,    //  Application name in DDE broadcast
    ASSOCSTR_DDETOPIC,          //  Topic Name in DDE broadcast
    ASSOCSTR_INFOTIP,           //  info tip for an item, or list of properties to create info tip from
    ASSOCSTR_QUICKTIP,          //  same as ASSOCSTR_INFOTIP, except, this list contains only quickly retrievable properties
    ASSOCSTR_TILEINFO,          //  similar to ASSOCSTR_INFOTIP - lists important properties for tileview
    ASSOCSTR_CONTENTTYPE,       //  MIME Content type
    ASSOCSTR_DEFAULTICON,       //  Default icon source
    ASSOCSTR_SHELLEXTENSION,    //  Guid string pointing to the Shellex\Shellextensionhandler value.
    ASSOCSTR_MAX                //  last item in enum...
} ASSOCSTR;
typedef HRESULT (STDAPICALLTYPE *LPFNASSOCQUERYSTRING)(ASSOCF, ASSOCSTR, LPCWSTR, LPCWSTR, LPWSTR, DWORD *);

LPFNASSOCQUERYSTRING s_AssocQueryString = NULL;

HMODULE s_shlwapi = NULL;

/***************************************
Get application associated to a filename
****************************************/
OP_STATUS GetShellFileHandler
(
	const uni_char*	file_name,				//  IN  e.g "mp3" *or* "C:\metallica - sad but true.mp3"
	OpString&		app_name				//	OUT	e.g "C:\Program Files\winamp.exe
)
{
	if (!s_shlwapi)
	{
		// This API was introduced with Internet Explorer 5,
		// and we sure don't want to depend on that, so load it on demand
		HMODULE shlwapi = WindowsUtils::SafeLoadLibrary(UNI_L("SHLWAPI.DLL"));
		if (!shlwapi)
			shlwapi = (HMODULE)-1;
		else
			s_AssocQueryString = (LPFNASSOCQUERYSTRING)GetProcAddress(shlwapi, "AssocQueryStringW");

		s_shlwapi = shlwapi;
	}

	OpString extension;
	
	extension.Set(file_name);
	if (extension.IsEmpty())
		return OpStatus::ERR;

	int ext_start = extension.FindLastOf('.');
	if(ext_start > 0)
	{
		extension.Delete(0,ext_start);
	}
	else if (ext_start < 0)
	{
		extension.Insert(0, UNI_L("."), 1);
	}

	if (s_AssocQueryString)
	{
		DWORD size;
		HRESULT ret = s_AssocQueryString(ASSOCF_NOTRUNCATE | ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_COMMAND, extension.CStr(), NULL, NULL, &size);
		if (ret != S_FALSE)
			return OpStatus::ERR;

		if(!app_name.Reserve(size))
			return OpStatus::ERR_NO_MEMORY;

		ret = s_AssocQueryString(ASSOCF_NOTRUNCATE | ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_COMMAND, extension.CStr(), NULL, app_name.CStr(), &size);
		if (ret != S_OK)
			return OpStatus::ERR;

		if (app_name.Compare(UNI_L("%1"),2) == 0 || app_name.Compare(UNI_L("\"%1\""),4) == 0)
		{
			app_name.Empty();
			RETURN_IF_ERROR(app_name.AppendFormat(UNI_L("\"%s\""), file_name));
		}
		else
			ParseShellCommand(app_name);

		return OpStatus::OK;
	}
	else
	{
		uni_char szTmpDir[_MAX_PATH];
		uni_char szTmpFile[_MAX_PATH];

		GetTempPath(_MAX_PATH, szTmpDir);

		UINT ret = GetTempFileName(szTmpDir, UNI_L("Opr"), 1818 , szTmpFile);
		if(ret != 0)
		{

			OpString szTmpFileExt;

			RETURN_VALUE_IF_ERROR(szTmpFileExt.Set(szTmpFile), FALSE);
			RETURN_VALUE_IF_ERROR(szTmpFileExt.Append(extension), FALSE);

			HANDLE hExtFile = CreateFile(szTmpFileExt,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_DELETE_ON_CLOSE, NULL);
			if (hExtFile != INVALID_HANDLE_VALUE)
			{
				if(app_name.Reserve(MAX_PATH) == NULL)
				{
					CloseHandle(hExtFile);
					return OpStatus::ERR_NO_MEMORY;
				}

				HINSTANCE fRes = FindExecutable(szTmpFileExt, NULL, app_name.CStr());

				if (fRes > (HINSTANCE)32)
				{
					DWORD res = GetShortPathName(szTmpFileExt, szTmpFile, MAX_PATH);
					CloseHandle(hExtFile);
					if (res == 0 || res > MAX_PATH)
						return OpStatus::ERR;

					if (app_name.CompareI(szTmpFile) == 0)
						RETURN_IF_ERROR(app_name.Set(file_name));
					RETURN_IF_ERROR(app_name.Insert(0, "\""));
					return app_name.Append("\"");
				}
			 
				CloseHandle(hExtFile);
			}
		}

		return GetMainShellFileHandler (extension, app_name);
	}
}

LPITEMIDLIST GetPidlFromPath( HWND hWndOwner, IShellFolder *pIShellFolder, LPCTSTR tchPath)
{
    ITEMIDLIST  * pidl = NULL;      //  return value
    WCHAR * pwchPathToUse;


    //  Make sure we have an unicode string
    pwchPathToUse = (WCHAR*)tchPath;

    OP_ASSERT( pwchPathToUse);
    if( pwchPathToUse)
    {
        //
        //  Get the pidl from that path.
        //

        ULONG nCharsParsed;
        pIShellFolder->ParseDisplayName( hWndOwner, NULL, pwchPathToUse, &nCharsParsed, &pidl, NULL);

        //  if the string was allocated then free it.
        if( pwchPathToUse != (WCHAR*)tchPath)
            delete [] pwchPathToUse;
    }

    return pidl;
}





ULONG GetRefCount( IUnknown * pIUnknown)
{
    pIUnknown->AddRef();
    return pIUnknown->Release();
}

void SplitPath( const uni_char *szPath, uni_char *szDirectory, uni_char *szFileName)
{
	uni_char * szFileNamePtr;
	
	OP_ASSERT( szPath && szDirectory && szFileName);

	uni_strlcpy( szDirectory, szPath, _MAX_PATH);
	szFileNamePtr = StringFileName( szDirectory);
	uni_strlcpy( szFileName, szFileNamePtr, _MAX_FNAME);
	*szFileNamePtr = 0;
}


//  *** DG-201298
//  ___________________________________________________________________________
//  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯?
//  GetShortcutTarget
//
//  Get's the accosiated path (and description) for a shortcut file.
//  If the file is not a shortcut or an error occurs the link file name is
//  used.
//  ___________________________________________________________________________
//
HRESULT GetShortcutTarget
(
    HWND    hWndParent,         //  The shell might want to display some messages
    LPCTSTR pszShortcutFile,    //  Path to shortcutfile
    BOOL    fResolve,           //  Resolve broken shortcuts ?
    LPTSTR   pszPath,           //  OUT: Resolved path to the file pointed to by the shortcut
    int     cbPath,             //  Size in bytes of 'pszPath'
    LPTSTR   pszDescription,    //  OUT: Filedescription (use NULL if not needed)
    int     cbDescription       //  Size in bytes of 'szDescription'
)
{
    HRESULT hres;
    IShellLink* psl;
    WIN32_FIND_DATA wfd;

    //  We return the filename if the file si not a shortcut.
    int len = min((size_t)cbPath, uni_strlen(pszShortcutFile));
    memmove(pszPath, pszShortcutFile, UNICODE_SIZE(len));    //  Memmove because they might overlap.
    pszPath[len] = 0;

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl);
    if (SUCCEEDED(hres))
    {
        IPersistFile* ppf;

        // Get a pointer to the IPersistFile interface.
        hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hres))
        {
            // Ensure shortcutpath is Unicode.
            unsigned short wsz[MAX_PATH];
			uni_strcpy(wsz, pszShortcutFile);


            // Load the shell link.
            hres = ppf->Load( wsz, STGM_READ);
            if (SUCCEEDED(hres))
            {
                // Resolve the link ?
                if( fResolve)
                    hres = psl->Resolve(hWndParent, SLR_ANY_MATCH);

                if (SUCCEEDED(hres))
                {
                    // Get the path to the link target.
                    hres = psl->GetPath( pszPath, cbPath, (WIN32_FIND_DATA *)&wfd, 0);
                    if( SUCCEEDED(hres) && pszDescription )
                    {
                        // Get the description of the target.
                        hres = psl->GetDescription( pszDescription, cbDescription);
                    }
                }
            }
            // Release pointer to IPersistFile interface.
            ppf->Release();
        }
        // Release pointer to IShellLink interface.
        psl->Release();
    }
    return hres;
}

BOOL CALLBACK EnumResNameProc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam)
{
	LONG_PTR *res_name_or_id = (LONG_PTR *)lParam;
	if (IS_INTRESOURCE(lpszName))
	{
		*res_name_or_id = (LONG_PTR)lpszName;
	}
	else
	{
		OpString *res_name = (OpString*)*res_name_or_id;
		res_name->Set(lpszName);
	}

	//We need only the first icon
	return FALSE;
}

OP_STATUS GetFirstIconFromBinary(const uni_char* binary, OpBitmap** icon, INT32 icon_size)
{
	DWORD flags;
	if (IsSystemWinVista())
		flags = LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;
	else
		flags = DONT_RESOLVE_DLL_REFERENCES; // DSK-337506

	//Open the binary file
	HMODULE module = LoadLibraryEx(binary, NULL, flags);
	if (!module)
		return OpStatus::ERR_FILE_NOT_FOUND;

	HICON res_icon = NULL;

	do
	{
		OpString res_name;

		LONG_PTR res_id_or_name = (LONG_PTR)&res_name;

		//Find the first RT_GROUP_ICON
		//forget about the return value - it will return FALSE, indicating an error according to the documentation,
		//but it actually does that because EnumResNameProc tells it to stop, not because it failed
		EnumResourceNames(module, RT_GROUP_ICON, EnumResNameProc, (LONG_PTR)&res_id_or_name);
		if (res_id_or_name == (INTPTR)&res_name && res_name.IsEmpty())
			break;

		//Find the address of the first RT_GROUP_ICON
		HRSRC res_info = FindResource(module, (res_id_or_name == (INTPTR)&res_name)?res_name.CStr():MAKEINTRESOURCE((int)res_id_or_name), RT_GROUP_ICON);
		if (!res_info)
			break;

		//Load the RT_GROUP_ICON from previously obtained address
		HGLOBAL res_data = LoadResource(module, res_info);
		if (!res_data)
			break;

		LPBYTE resource = (LPBYTE)LockResource(res_data);
		if (!resource)
			break;

		//Gets the id of the icon in the group that has is closest to the current color depth and the required size
		int res_id = LookupIconIdFromDirectoryEx(resource, TRUE, icon_size, icon_size, LR_DEFAULTCOLOR);
		if (!res_id)
			break;

		//Gets the address of that icon
		res_info = FindResource(module, MAKEINTRESOURCE(res_id), RT_ICON);
		if (!res_info)
			break;

		//Load the icon from previously obtained address
		res_data = LoadResource(module, res_info);
		if (!res_data)
			break;

		resource = (LPBYTE)LockResource(res_data);
		if (!resource)
			break;

		//Copies the icon from the resource, making it usable
		res_icon = CreateIconFromResourceEx(resource, SizeofResource(module, res_info), TRUE, 0x00030000, icon_size, icon_size, LR_DEFAULTCOLOR);
	}
	while (0);

	FreeLibrary(module);

	if (!res_icon)
	{
		return OpStatus::ERR;
	}

	OP_STATUS err = IconUtils::CreateIconBitmap(icon, res_icon);
	DestroyIcon(res_icon);
	return err;
}

OP_STATUS GetFileHandlerInfo(const uni_char* handler, OpString *filedescription, OpBitmap** icon, INT32 icon_size)
{
	// unquote handler's name (ParseShellCommand() ensures that it is quoted at this point)
	uni_char* position = (uni_char*)handler + 1;
	uni_char* file_end = uni_strchr(position, '"');
	*file_end = 0;

	// if handler is rundll32 and is followed by quoted argument then get info about that argument instead
	if (uni_stristr(position, UNI_L("rundll32")) != NULL)
	{
		uni_char* arg_start = uni_strchr(file_end + 1, '"');
		if (arg_start)
		{
			++ arg_start;
			uni_char* arg_end = uni_strchr(arg_start, '"');
			if (arg_end)
			{
				*file_end = '"';
				position = arg_start;
				file_end = arg_end;
				*file_end = 0;
			}
		}
	}

	OP_STATUS err1 = GetFileDescription(position, filedescription);
	OP_STATUS err2 = GetFirstIconFromBinary(position, icon, icon_size);

	*file_end = '"';

	if (OpStatus::IsError(err1))
		filedescription->Empty();
	if (OpStatus::IsError(err2))
		*icon = NULL;

	RETURN_IF_ERROR(err1);
	return err2;
}

OP_STATUS GetFileDescription(const uni_char* file, OpString* filedescription, OpString* productName)
{
//	DWORD filetype = 0;
//	GetBinaryType(file, &filetype);

	if (!file)
		return OpStatus::ERR;

	WindowsCommonUtils::FileInformation fileinfo;

	UniString description;
	UniString prod_name;

	if (OpStatus::IsSuccess(fileinfo.Construct(file)))
	{
		if (filedescription)
			if (OpStatus::IsError(fileinfo.GetInfoItem(UNI_L("FileDescription"), description)))
				OpStatus::Ignore(fileinfo.GetInfoItem(UNI_L("FileDescription"), description, UNI_L("040904E4")));

		if (productName)
			if (OpStatus::IsError(fileinfo.GetInfoItem(UNI_L("ProductName"), prod_name)))
				OpStatus::Ignore(fileinfo.GetInfoItem(UNI_L("ProductName"), prod_name, UNI_L("040904E4")));
	}

	//(julienp)Couldn't find resource information in the file.
	//We just return the filename without extension.

	OpString filename;
	const uni_char* name = uni_strrchr(file, '\\');
	RETURN_IF_ERROR(filename.Set(name? name + 1: file));
	uni_char* ext = uni_strrchr(filename.CStr(), '.');
	if(ext)
		*ext=0;

	if (filedescription && description.IsEmpty())
	{
		RETURN_IF_ERROR(filedescription->Set(filename));
	}
	else if (filedescription)
	{
		const uni_char* description_str;
		RETURN_IF_ERROR(description.CreatePtr(&description_str, TRUE));		//TODO: Use UniString to OpString conversion function when available
		RETURN_IF_ERROR(filedescription->Set(description_str));
	}

	if (productName && prod_name.IsEmpty())
	{
		RETURN_IF_ERROR(productName->Set(filename));
	}
	else if (productName)
	{
		const uni_char* prod_name_str;
		RETURN_IF_ERROR(prod_name.CreatePtr(&prod_name_str, TRUE));		//TODO: Use UniString to OpString conversion function when available
		RETURN_IF_ERROR(productName->Set(prod_name_str));
	}
	
	return OpStatus::OK;
}

OP_STATUS GetShellDefaultIcon (const uni_char* file_name, OpBitmap** icon, INT32 icon_size)
{
	uni_char szTmpDir[_MAX_PATH];
	uni_char szTmpFile[_MAX_PATH];

	OpString extension;
	
	extension.Set(file_name);
	if (extension.IsEmpty())
		return OpStatus::ERR;

	int ext_start = extension.FindLastOf('.');
	if (ext_start > 0)
	{
		extension.Delete(0,ext_start);
	}

	UINT ret = GetTempPath(_MAX_PATH, szTmpDir);
	if (ret == 0 || ret > _MAX_PATH)
		return OpStatus::ERR;

	ret = GetTempFileName(szTmpDir, UNI_L("Opr"), 1818 , szTmpFile);
	if(ret == 0)
		return OpStatus::ERR;

	OpString szTmpFileExt;
	RETURN_IF_ERROR(szTmpFileExt.Set(szTmpFile));
	RETURN_IF_ERROR(szTmpFileExt.Append(extension));

	HANDLE hExtFile = CreateFile(szTmpFileExt.CStr(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (hExtFile == INVALID_HANDLE_VALUE)
		return OpStatus::ERR;

	OP_STATUS err = IconUtils::CreateIconBitmap(icon, szTmpFileExt.CStr(), icon_size);
	CloseHandle(hExtFile);
	return err;

/*		OpString ext;

	ext.Set(file_name);

	int ext_start = ext.FindLastOf('.');
	if(ext_start > -1)
	{
		ext.Set(ext.SubString(ext_start));
	}

	WCHAR filetype[256];     // registry name for this filetype 
	LONG  filetypelen = sizeof(filetype); // length of above 

	if(RegQueryValueW(HKEY_CLASSES_ROOT, ext.CStr(), filetype, &filetypelen) == ERROR_SUCCESS)
	{
		filetypelen /= sizeof(WCHAR);
		filetype[filetypelen] = '\0';

		OpString typekey;
		OpString icon_string;
		LONG commandlen = _MAX_PATH;
		if (!icon_string.Reserve(commandlen))
			return OpStatus::ERR_NO_MEMORY;

		typekey.Set(filetype);
		typekey.Append(UNI_L("\\DefaultIcon"));

		// Check registry, HKEY_CLASS_ROOT\<filetype>\DefaultIcon
		if (RegQueryValueW(HKEY_CLASSES_ROOT, typekey.CStr(), icon_string.CStr(), &commandlen) == ERROR_SUCCESS)
		{
			const uni_char comma[2]=UNI_L(",");
			int comma_position = icon_string.FindLastOf(comma[0]);
			if (get_big_icon)
			{
				if (comma_position != KNotFound && ExtractIconEx(icon_string.SubString(0,comma_position).CStr(), uni_atoi(icon_string.SubString(comma_position+1)), &icon, NULL, 1)==1)
					return OpStatus::OK;
			}
			else
			{
				if (comma_position != KNotFound && ExtractIconEx(icon_string.SubString(0,comma_position).CStr(), uni_atoi(icon_string.SubString(comma_position+1)), NULL, &icon, 1)==1)
					return OpStatus::OK;
			}
		}
	}*/
}


OP_STATUS GetMainShellFileHandler (const OpString& extension, OpString& app_command)
{
	OpString filetype;
	RETURN_IF_ERROR(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, extension.CStr(), filetype));
	return GetShellCommand(filetype, app_command);
}

OP_STATUS AddAppToList(OpString *app_command, OpVector<OpString>& app_name_list, OpString *excluded_app)
{
	OpAutoPtr<OpString> app_command_ap(app_command);

	if (app_command->IsEmpty())
		return OpStatus::OK;

	if (!excluded_app || (excluded_app && app_command->FindI(*excluded_app) == KNotFound))
	{
		for (UINT32 i = 0; i < app_name_list.GetCount() ; i++)
			if (!app_command->CompareI(*(app_name_list.Get(i))))
				return OpStatus::OK;

		RETURN_IF_ERROR(app_name_list.Add(app_command));
		app_command_ap.release();
	}
	else if (excluded_app)
	{
		int excl_len = excluded_app->Length();
		if (excl_len >= 5 && !excluded_app->SubString(excl_len-5, 4).CompareI(".dll") && excluded_app->CompareI("rundll", 6))
		{
			excluded_app->Set(*app_command);
		}
	}
	return OpStatus::OK;
}

OP_STATUS ExtractOpenWithData(HKEY root_key, const uni_char* key_name, BOOL from_value_datas, BOOL from_key_names, BOOL as_prog_id, OpVector<OpString>& app_name_list, OpString *excluded_app)
{
	OpAutoHKEY extregkey;
	if (RegOpenKeyEx(root_key, key_name, 0, KEY_READ, &extregkey) == ERROR_SUCCESS)
	{
		DWORD n_subkeys;
		DWORD max_key_name_length;
		DWORD n_values;
		DWORD max_name_length;
		DWORD max_data_length;
		if (RegQueryInfoKey(extregkey, NULL, NULL, NULL, &n_subkeys, &max_key_name_length, NULL, &n_values, &max_name_length, &max_data_length, NULL, NULL) == ERROR_SUCCESS)
		{
			OpString value_name;
			RETURN_OOM_IF_NULL(value_name.Reserve(max_name_length+1));

			OpString value_data;
			if (from_value_datas)
			{
				RETURN_OOM_IF_NULL(value_data.Reserve(max_data_length/sizeof(uni_char)));
			}

			for (UINT32 i = 0; i<n_values; i++)
			{
				DWORD name_length = max_name_length+1;
				DWORD data_length = max_data_length;
				DWORD value_type;

				if (RegEnumValue(extregkey, i, value_name.CStr(), &name_length, NULL, &value_type, from_value_datas ? (LPBYTE)value_data.CStr() : NULL, from_value_datas ? &data_length : NULL) == ERROR_SUCCESS &&
					(!from_value_datas || value_type== REG_SZ && value_name.Length()==1 /* exclude "MRUList" */))
				{
					OpString* app_command = new OpString;
					RETURN_OOM_IF_NULL(app_command);

					OP_STATUS err;
					if (from_value_datas)
						if (as_prog_id)
							err = GetShellCommand(value_data, *app_command);
						else
							err = GetAppCommand(value_data, *app_command);
					else
						if (as_prog_id)
							err = GetShellCommand(value_name, *app_command);
						else
							err = GetAppCommand(value_name, *app_command);
					if (OpStatus::IsSuccess(err))
					{
						RETURN_IF_ERROR(AddAppToList(app_command, app_name_list, excluded_app));
					}
					else
						delete app_command;
				}
			}

			if (from_key_names)
			{
				OpString key_name;
				RETURN_OOM_IF_NULL(key_name.Reserve(max_key_name_length+1));

				for (UINT32 i=0;i<n_subkeys;i++)
				{
					DWORD name_length = max_key_name_length+1;

					if (RegEnumKeyEx(extregkey, i, key_name.CStr(), &name_length, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
					{
						OpString* app_command = new OpString;
						RETURN_OOM_IF_NULL(app_command);

						OP_STATUS err;
						if (as_prog_id)
							err = GetShellCommand(key_name, *app_command);
						else
							err = GetAppCommand(key_name, *app_command);
						if (OpStatus::IsSuccess(err))
						{
							RETURN_IF_ERROR(AddAppToList(app_command, app_name_list, excluded_app));
						}
						else
							delete app_command;
					}
				}
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS GetAllShellFileHandlers (const uni_char* file_name, OpVector<OpString>& app_name_list, OpString *excluded_app)
{
	OpString extkey;
	
	extkey.Set(file_name);
	if (extkey.IsEmpty())
		return OpStatus::ERR;

	int ext_start = extkey.FindLastOf('.');
	if (ext_start > 0)
	{
		extkey.Delete(0,ext_start);
	}

	OpAutoPtr<OpString> main_handler = new OpString;
	RETURN_OOM_IF_NULL(main_handler.get());

	if (OpStatus::IsSuccess(GetMainShellFileHandler(extkey, *main_handler)))
	{
		if (main_handler->HasContent() && (!excluded_app || main_handler->FindI(*excluded_app) == KNotFound))
		{
			RETURN_IF_ERROR(app_name_list.Add(main_handler.get()));
			main_handler.release();
		}
		else if (main_handler->HasContent() && excluded_app && !excluded_app->SubString(excluded_app->Length()-5, 4).CompareI(".dll") && excluded_app->SubString(0, 6).CompareI("rundll"))
		{
			RETURN_IF_ERROR(excluded_app->Set(*main_handler));
		}
	}

	OpString open_with_key;
	RETURN_IF_ERROR(open_with_key.Set(extkey));
	RETURN_IF_ERROR(open_with_key.Append("\\OpenWithList"));
	RETURN_IF_ERROR(ExtractOpenWithData(HKEY_CLASSES_ROOT, open_with_key.CStr(), FALSE, TRUE, FALSE, app_name_list, excluded_app));
	RETURN_IF_ERROR(open_with_key.Insert(0,"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\"));
	RETURN_IF_ERROR(ExtractOpenWithData(HKEY_CURRENT_USER, open_with_key.CStr(), TRUE, FALSE, FALSE, app_name_list, excluded_app));
	RETURN_IF_ERROR(open_with_key.Set(extkey));
	RETURN_IF_ERROR(open_with_key.Append("\\OpenWithProgids"));
	RETURN_IF_ERROR(ExtractOpenWithData(HKEY_CLASSES_ROOT, open_with_key.CStr(), FALSE, FALSE, TRUE, app_name_list, excluded_app));
	RETURN_IF_ERROR(open_with_key.Insert(0,"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\"));
	return ExtractOpenWithData(HKEY_CURRENT_USER, open_with_key.CStr(), FALSE, FALSE, TRUE, app_name_list, excluded_app);
}

OP_STATUS GetAppCommand(const OpString& app_name, OpString& app_command)
{
	OpString appkey;
	RETURN_IF_ERROR(appkey.Set("Applications\\"));
	RETURN_IF_ERROR(appkey.Append(app_name));

	return GetShellCommand(appkey, app_command);
}

OP_STATUS GetShellCommand(const OpString& key, OpString& app_command)
{
	OpString typekey;
	RETURN_IF_ERROR(typekey.Set(key));
	RETURN_IF_ERROR(typekey.Append(UNI_L("\\shell")));

	OpString verb;

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, typekey.CStr(), verb)) && verb.HasContent())
	{
		RETURN_IF_ERROR(typekey.Append("\\"));
		RETURN_IF_ERROR(typekey.Append(verb));
		RETURN_IF_ERROR(typekey.Append("\\command"));

		if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, typekey.CStr(), app_command)))
		{
			ParseShellCommand(app_command);
			return OpStatus::OK;
		}
	}

	RETURN_IF_ERROR(typekey.Set(key));
	RETURN_IF_ERROR(typekey.Append(UNI_L("\\shell\\open\\command")));

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, typekey.CStr(), app_command)))
	{
		ParseShellCommand(app_command);
	}
	else
	{
		RETURN_IF_ERROR(typekey.Set(key));
		RETURN_IF_ERROR(typekey.Append(UNI_L("\\shell\\play\\command")));

		if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, typekey.CStr(), app_command)))
		{
			ParseShellCommand(app_command);
		}
		else
		{
			RETURN_IF_ERROR(typekey.Set(key));
			RETURN_IF_ERROR(typekey.Append(UNI_L("\\shell\\edit\\command")));

			if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, typekey.CStr(), app_command)))
			{
				ParseShellCommand(app_command);
			}
			else
			{
				RETURN_IF_ERROR(typekey.Set(key));
				RETURN_IF_ERROR(typekey.Append(UNI_L("\\shell")));

				DWORD key_name_length = 255;
				uni_char key_name[255];

				OP_STATUS status = OpStatus::ERR;
				OpAutoHKEY typeregkey;
				if (RegOpenKeyEx(HKEY_CLASSES_ROOT, typekey, 0, KEY_READ, &typeregkey) == ERROR_SUCCESS)
				{
					if (RegEnumKeyEx(typeregkey, 0, key_name, &key_name_length, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
					{
						RETURN_IF_ERROR(typekey.Append("\\"));
						RETURN_IF_ERROR(typekey.Append(key_name));
						RETURN_IF_ERROR(typekey.Append("\\command"));

						status = OpSafeRegQueryValue(HKEY_CLASSES_ROOT, typekey.CStr(), app_command);
					}
				}
				if (OpStatus::IsSuccess(status))
				{
					ParseShellCommand(app_command);
				}
				else
				{
					app_command.Empty();
					return status;
				}
			}
		}
	}

	return OpStatus::OK;
}

/** Adds Chrome application command to AppList
 *
 * This function fixes DSK-301447 - Web page right click menu item "Open With"
 * does not show Chrome installation.
 *
 * When Chrome is not installed as a default Internet browser then application
 * command is not added to regular registry entries for Internet clients.
 * This function search Chrome specific entry. If Chrome application command
 * is already added to the list it will be not added second time (see
 * AddAppToList).
 *
 * @param app_name_list List to which will be added Chrome application command
 *
 * @return OpStatus::OK if error has not occurred
 */
OP_STATUS AddChromeToURLHandlers(OpVector<OpString>& app_name_list)
{
	OpString* app_command = new OpString;
	RETURN_OOM_IF_NULL(app_command);
	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, UNI_L("ChromeHTML\\shell\\open\\command"), *app_command)))
	{
		ParseShellCommand(*app_command);
		RETURN_IF_ERROR(AddAppToList(app_command, app_name_list, NULL));
	}
	else
	{
		delete app_command;
	}

	return OpStatus::OK;
}

OP_STATUS GetURLHandlers (OpVector<OpString>& app_name_list)
{
	OpString first_key;
	OpString key_name;
	OpString commandkey;

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Clients\\StartMenuInternet"), key_name)))
	{
		RETURN_IF_ERROR(commandkey.Set("SOFTWARE\\Clients\\StartMenuInternet\\"));
		RETURN_IF_ERROR(commandkey.Append(key_name));
		RETURN_IF_ERROR(commandkey.Append("\\shell\\open\\command"));

		RETURN_IF_ERROR(first_key.Set(key_name));
		OpString* app_command = new OpString;
		RETURN_OOM_IF_NULL(app_command);
		if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, commandkey.CStr(), *app_command)))
		{
			ParseShellCommand(*app_command);
			RETURN_IF_ERROR(AddAppToList(app_command, app_name_list, NULL));
		}
		else
		{
			delete app_command;
		}
	}

	OpAutoHKEY browsersregkey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Clients\\StartMenuInternet"), 0, KEY_READ, &browsersregkey) == ERROR_SUCCESS)
	{
		DWORD n_subkeys;
		DWORD max_name_length;
		if (RegQueryInfoKey(browsersregkey, NULL, NULL, NULL, &n_subkeys, &max_name_length, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			RETURN_OOM_IF_NULL(key_name.Reserve(max_name_length+1));
			for (UINT32 i=0;i<n_subkeys;i++)
			{
				DWORD name_length= max_name_length+1;
				if (RegEnumKeyEx(browsersregkey, i, key_name.CStr(), &name_length, NULL, NULL, NULL, NULL) == ERROR_SUCCESS &&
					(first_key.IsEmpty() || key_name.Compare(first_key)))
				{
					RETURN_IF_ERROR(commandkey.Set("SOFTWARE\\Clients\\StartMenuInternet\\"));
					RETURN_IF_ERROR(commandkey.Append(key_name));
					RETURN_IF_ERROR(commandkey.Append("\\shell\\open\\command"));

					OpString* app_command = new OpString;
					RETURN_OOM_IF_NULL(app_command);

					if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, commandkey.CStr(), *app_command)))
					{
						ParseShellCommand(*app_command);
						RETURN_IF_ERROR(AddAppToList(app_command, app_name_list, NULL));
					}
					else
					{
						delete app_command;
					}
				}
			}
		}
	}

	RETURN_IF_ERROR(AddChromeToURLHandlers(app_name_list));

	return OpStatus::OK;
}

void ParseShellCommand(OpString& app_command)
{
	OP_ASSERT(app_command.HasContent());
	if (app_command.IsEmpty())
		return;

	uni_char expanded_command[MAX_PATH];

	ExpandEnvironmentStrings(app_command.CStr(), expanded_command, MAX_PATH);
	app_command.Set(expanded_command);

	//No need to do any fancy stuff if this is directly a correct path (except quoting)

	if (PathFileExists(app_command.CStr()))
	{
		app_command.Insert(0,UNI_L("\""));
		app_command.Append(UNI_L("\""));
		return;
	}

	app_command.Strip(TRUE, TRUE);
	OpString app_parameters;

	BOOL quoted = app_command[0] == '"';

	if(quoted)
	{
		app_command.Delete(0,1);
		int app_end = app_command.FindFirstOf('"');
		if (app_end != KNotFound)
		{
			uni_char *quote_pos = app_command.CStr()+app_end;
			app_parameters.Set(quote_pos+1);
			*quote_pos = 0;
		}
	}
	else
	{
		int app_end = app_command.FindFirstOf(' ');
		if (app_end != KNotFound)
		{
			uni_char *space_pos = app_command.CStr()+app_end;
			app_parameters.Set(space_pos);
			*space_pos = 0;
		}
	}

	while (!PathFileExists(app_command))
	{	
		UINT full_exe_len = SearchPath(NULL, app_command, UNI_L(".exe"), 0, NULL, NULL);
		if (full_exe_len == 0)
		{
			uni_char *first_param_end;
			if (quoted || app_parameters.IsEmpty() || (first_param_end = uni_strchr(app_parameters.CStr()+1, ' ')) == NULL)
			{
				app_command.Empty();
				return;
			}

			int next_param_pos = first_param_end - app_parameters.CStr();
			app_command.Append(app_parameters, next_param_pos);
			app_parameters.Delete(0, next_param_pos);
		}
		else
		{
			OpString full_app_command;
			// Reserve two extra characters for the quotes that will be added later
			if (!full_app_command.Reserve(full_exe_len+1))
			{
				app_command.Empty();
				return;
			}

			SearchPath(NULL, app_command, UNI_L(".exe"), full_exe_len, full_app_command, NULL);

			app_command.TakeOver(full_app_command);
			break;
		}
	}

	app_command.Insert(0,UNI_L("\""));
	app_command.Append(UNI_L("\""));

	if (app_command.FindI(UNI_L("rundll32")) != KNotFound)
	{
		OpString dll_name;
		dll_name.TakeOver(app_parameters);
		dll_name.Strip(TRUE, TRUE);

		if (dll_name[0] == '"')
		{
			dll_name.Delete(0,1);
			int dll_end = dll_name.FindFirstOf('"');
			if (dll_end != KNotFound)
			{
				uni_char *quote_pos = dll_name.CStr()+dll_end;
				app_parameters.Set(quote_pos+1);
				*quote_pos = 0;
			}
		}
		else
		{
			int dll_end = dll_name.FindFirstOf(' ');
			if (dll_end != KNotFound)
			{
				uni_char *space_pos = dll_name.CStr()+dll_end;
				app_parameters.Set(space_pos);
				*space_pos = 0;

				dll_end = dll_name.FindLastOf(',');
				if (dll_end != KNotFound)
				{
					uni_char *comma_pos = dll_name.CStr()+dll_end;
					app_parameters.Insert(0, comma_pos);
					*comma_pos = 0;
				}
			}
		}
	
		if (!PathFileExists(dll_name))
		{
			int full_dll_len = SearchPath(NULL, dll_name, UNI_L(".dll"), 0, NULL, NULL);
			if (full_dll_len == 0)
			{
				app_command.Empty();
				return;
			}

			OpString full_dll;
			if (!full_dll.Reserve(full_dll_len))
			{
				app_command.Empty();
				return;
			}

			SearchPath(NULL, dll_name, UNI_L(".dll"), full_dll_len, full_dll, NULL);

			dll_name.TakeOver(full_dll);
		}

		app_command.AppendFormat(UNI_L(" \"%s\""), dll_name.CStr());
	}

	if (app_command.FindI(UNI_L("explorer.exe")) != KNotFound)
	{
		// This fixes DSK-331765 - "Opening zip archives with native Windows association".
		// The default app_command for .zip files looks like this :
		// %SystemRoot%\Explorer.exe /idlist,%I,%L
		// This string is located in registry here : HKEY_CLASSES_ROOT\CompressedFolder\shell\Open\Command
		// Documentation about %I : http://www.geoffchappell.com/studies/windows/shell/explorer/cmdline.htm
		// We are not using an ITEMIDLIST method, so the simplest way to fix is to replace it.
		app_parameters.ReplaceAll(UNI_L("/idlist,%I,%L"), UNI_L("%s"), 1);
	}

	uni_char* position = app_parameters.CStr();
	if (position)
	{
		while ((position = uni_strchr(position, '%')) != NULL)
		{
			position++;
			switch (*position)
			{
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '*':
					app_parameters.Delete(position-app_parameters.CStr()-1, 2);
					break;
				case '1':
				case 'L':
				case 'l':
					*position = 's';
					break;
			}
		}

		app_command.Append(app_parameters);
	}
}
