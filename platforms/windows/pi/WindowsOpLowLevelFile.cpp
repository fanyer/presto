/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Author: Petter Nilsen
 */
#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"

#include "WindowsOpLowLevelFile.h"
#include "WindowsOpDesktopResources.h"

#include <share.h>
#include <io.h>

OP_STATUS OpLowLevelFile::Create(OpLowLevelFile** new_file, const uni_char* path, BOOL serialized)
{
	OpString new_path;

	if (serialized)
	{
		// If this asserts goes off, core is not initialized. Core is needed for serialized paths.
		// Please correct the calling code, to either not use serialized path, or don't use OpLowLevelFile.
		OP_ASSERT(g_opera && g_op_system_info);
		RETURN_IF_ERROR(g_op_system_info->ExpandSystemVariablesInString(path, &new_path));
	}
	else
		RETURN_IF_ERROR(new_path.Set(path));

	OP_ASSERT(new_file != NULL);
	*new_file = OP_NEW(WindowsOpLowLevelFile, ());
	if (*new_file == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS res;
	res = ((WindowsOpLowLevelFile*)(*new_file))->Construct(new_path.CStr());

	if (OpStatus::IsError(res))
	{
		OP_DELETE(*new_file);
		*new_file = NULL;
	}
	return res;
}

WindowsOpLowLevelFile::WindowsOpLowLevelFile() :
	m_fp(NULL),
	m_file_open_mode(0),
	m_commit_mode(FALSE),
	m_text_mode(FALSE),
	m_filepos_dirty(FALSE),
	m_thread_handle(NULL),
	m_event_quit(NULL),
	m_event_quit_done(NULL),
	m_event_file(NULL),
	m_event_sync_done(NULL)
{
}

WindowsOpLowLevelFile::~WindowsOpLowLevelFile()
{
#ifdef PI_ASYNC_FILE_OP
	// clean up async thread, if it exists
	ShutdownThread();
#endif // PI_ASYNC_FILE_OP
	Close();
}

OP_STATUS
WindowsOpLowLevelFile::Construct(const uni_char* path)
{
	RETURN_IF_ERROR(m_path.Set(path));

	// Remove trailing (back)slashes
	int len = m_path.Length();
	if (len && (m_path[len-1] == '\\' || m_path[len-1] == '/'))
	{
		if (len == 1 || m_path[len-2] != ':')
		{
			m_path.Delete(len-1);
		}
	}

	return OpStatus::OK;
}

OP_STATUS WinErrToOpStatus(DWORD error)
{
	switch (error)
	{
	case ERROR_SUCCESS:  // == NO_ERROR
		return OpStatus::OK;
	case ERROR_DISK_FULL:
		return OpStatus::ERR_NO_DISK;
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
		return OpStatus::ERR_NO_MEMORY;
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
	case ERROR_WRONG_DISK:
	case ERROR_ACCESS_DENIED:
	case ERROR_BAD_NET_RESP: // When trying to check properties on a share
	case ERROR_DIR_NOT_EMPTY:
		return OpStatus::ERR_NO_ACCESS;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_INVALID_NAME:
	case ERROR_PATH_NOT_FOUND:
		return OpStatus::ERR_FILE_NOT_FOUND;
	case ERROR_SHARING_BUFFER_EXCEEDED: // Too many files open for sharing
		return OpStatus::ERR;
	case ERROR_INVALID_HANDLE:
		return OpStatus::ERR_BAD_FILE_NUMBER;

	default:
		OP_ASSERT(FALSE); // Unknown error. Please check and add it above.
		return OpStatus::ERR;
	}
}

OP_STATUS
WindowsOpLowLevelFile::GetWindowsLastError() const
{
	DWORD error = GetLastError();

	return WinErrToOpStatus(error);
}

OP_STATUS
WindowsOpLowLevelFile::ErrNoToStatus(int err)
{
	switch (err)
	{
		case ENOMEM:
			return OpStatus::ERR_NO_MEMORY;
		case ENOENT:
			return OpStatus::ERR_FILE_NOT_FOUND;
		case EACCES:
			return OpStatus::ERR_NO_ACCESS;
		case ENOSPC:
			return OpStatus::ERR_NO_DISK;
		case 0:
			return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS
WindowsOpLowLevelFile::GetFileInfo(OpFileInfo* info)
{
	int f = info->flags;
	if (f & OpFileInfo::FULL_PATH)
	{
		info->full_path = GetFullPath();
	}
	if (f & OpFileInfo::SERIALIZED_NAME)
	{
		info->serialized_name = GetSerializedName();
	}
	if (f & OpFileInfo::WRITABLE)
	{
		info->writable = IsWritable();
	}
	if (f & OpFileInfo::OPEN)
	{
		info->open = IsOpen();
	}
#ifdef PI_CAP_FILE_HIDDEN
	if (f & OpFileInfo::HIDDEN)
	{
		// this has to be modified to check if the file is really hidden or not
		info->hidden = IsHidden();
	}
#endif // PI_CAP_FILE_HIDDEN
	if (f & OpFileInfo::LENGTH)
	{
		RETURN_IF_ERROR(GetFileLength(&info->length));
	}
	if (f & OpFileInfo::POS)
	{
		RETURN_IF_ERROR(GetFilePos(&info->pos));
	}
	if (f & OpFileInfo::LAST_MODIFIED || f & OpFileInfo::MODE || f & OpFileInfo::CREATION_TIME)
	{
		struct _stat buf;
		if (Stat(&buf) == OpStatus::OK)
		{
			if (f & OpFileInfo::LAST_MODIFIED)
			{
				info->last_modified = buf.st_mtime;
			}

			if (f & OpFileInfo::MODE)
			{
				if (buf.st_mode & _S_IFDIR)
				{
					info->mode = OpFileInfo::DIRECTORY;
				}
				else
				{
					info->mode = OpFileInfo::FILE;
				}
			}
			if(f & OpFileInfo::CREATION_TIME)
			{
				info->creation_time = buf.st_ctime;
			}
		}
		else
		{
			OP_ASSERT(FALSE); // File not found
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

/** Change file information.
*
* Only applicable to existing files that are closed. OpStatus::ERR is
* returned otherwise.
*
* The 'flags' member of the given OpFileInfoChange object chooses which
* pieces of information to change. Before making any changes to the file,
* the implementation will check if all requested operations are supported,
* and, if that is not the case, return OpStatus::ERR_NOT_SUPPORTED
* immediately.
*
* @param changes What to change ('changes.flags'), and what to change it
* to (the other member variables of 'changes').
*
* @return OK if successful, ERR_NO_MEMORY on OOM, ERR_NO_ACCESS if write
* access was denied, ERR_FILE_NOT_FOUND if the file does not exist,
* ERR_NOT_SUPPORTED if (the file exists, but) at least one of the flags
* set is not supported by the platform or file system, ERR for other
* errors.
*/
#ifdef SUPPORT_OPFILEINFO_CHANGE

void TimetToFileTime( time_t t, LPFILETIME pft )
{
	LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
	pft->dwLowDateTime = (DWORD) ll;
	pft->dwHighDateTime = ll >>32;
}

OP_STATUS WindowsOpLowLevelFile::ChangeFileInfo(const OpFileInfoChange* changes)
{
	BOOL exists = FALSE;

	if(m_fp || !m_path.HasContent() || !changes)
	{
		// not allowed on open files
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(Exists(&exists));
	if(!exists)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}
	if(changes->flags & OpFileInfoChange::LAST_MODIFIED || changes->flags & OpFileInfoChange::CREATION_TIME)
	{
		HANDLE file = CreateFile(m_path.CStr(), GENERIC_READ | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(file)
		{
			FILETIME ft;
			BOOL success = TRUE;

			if(changes->flags & OpFileInfoChange::LAST_MODIFIED)
			{
				TimetToFileTime(changes->last_modified, &ft);
				success = SetFileTime(file, NULL, NULL, &ft);
			}
			if(changes->flags & OpFileInfoChange::CREATION_TIME)
			{
				TimetToFileTime(changes->creation_time, &ft);
				success = SetFileTime(file, &ft, NULL, NULL);
			}
			CloseHandle(file);
			if(!success)
			{
				OpStatus::ERR;
			}
		}
		else
		{
			return OpStatus::ERR_NO_ACCESS;
		}
	}
	if(changes->flags & OpFileInfoChange::WRITABLE)
	{
		int retval = _wchmod(m_path.CStr(), changes->writable ? _S_IWRITE : 0);
		if(retval < 0)
		{
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}
#endif // SUPPORT_OPFILEINFO_CHANGE

const uni_char*
WindowsOpLowLevelFile::GetFullPath() const
{
	return m_path.CStr();
}

const uni_char*
WindowsOpLowLevelFile::GetSerializedName() const
{
	//julienp:	This function is correct. We are using long names for serialized paths because of a mixup
	//			that happened with the short versions. Do not touch this function without talking to me or
	//			adamm first.

	if (m_serialized_path.IsEmpty())
	{
		if(OpStatus::IsError(MakeSerializedName()))
		{
			m_serialized_path.Empty();
			return m_path.CStr();
		}
	}
	return m_serialized_path.CStr();
}

OP_STATUS WindowsOpLowLevelFile::MakeSerializedName() const
{
	// If this asserts goes off, core is not initialized. Core is needed for serialized paths.
	// Please correct the calling code, to either not use serialized path, or don't use OpLowLevelFile.
	OP_ASSERT(g_opera && g_folder_manager);

	RETURN_IF_ERROR(m_serialized_path.Set(m_path));
	int path_len = m_path.Length();
	BOOL serialized = FALSE;

	WindowsOpDesktopResources resources;

	const uni_char* folder = 0;
	OpString folder_opstring;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, folder_opstring));
	folder = folder_opstring.CStr();
	RETURN_OOM_IF_NULL(folder);
	int folder_len = uni_strlen(folder);
	if (folder[folder_len - 1] == PATHSEPCHAR && path_len == folder_len - 1)
	{
		folder_len--;
	}
	if (m_serialized_path.CompareI(folder, folder_len) == 0)
	{
		m_serialized_path.Delete(0, folder_len);
		RETURN_IF_ERROR(m_serialized_path.Insert(0, "{SmallPreferences}"));
		serialized = TRUE;
	}

	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_LOCAL_HOME_FOLDER, folder_opstring));
	folder = folder_opstring.CStr();
	RETURN_OOM_IF_NULL(folder);
	folder_len = uni_strlen(folder);
	if (folder[folder_len - 1] == PATHSEPCHAR && path_len == folder_len - 1)
	{
		folder_len--;
	}
	if (m_serialized_path.CompareI(folder, folder_len) == 0)
	{
		m_serialized_path.Delete(0, folder_len);
		RETURN_IF_ERROR(m_serialized_path.Insert(0, "{LargePreferences}"));
		serialized = TRUE;
	}

	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, folder_opstring));
	folder = folder_opstring.CStr();
	RETURN_OOM_IF_NULL(folder);
	folder_len = uni_strlen(folder);
	if (folder[folder_len - 1] == PATHSEPCHAR && path_len == folder_len - 1)
	{
		folder_len--;
	}
	if (m_serialized_path.CompareI(folder, folder_len) == 0)
	{
		m_serialized_path.Delete(0, folder_len);
		RETURN_IF_ERROR(m_serialized_path.Insert(0, "{Resources}"));
		serialized = TRUE;
	}

	OpString home_folder;
	if (OpStatus::IsSuccess(resources.GetHomeFolder(home_folder)))
	{
		if (home_folder[home_folder.Length() - 1] != '\\')
			RETURN_IF_ERROR(home_folder.Append(UNI_L("\\")));

		folder = home_folder.CStr();
		folder_len = uni_strlen(folder);
		if (folder[folder_len - 1] == PATHSEPCHAR && path_len == folder_len - 1)
		{
			folder_len--;
		}
		if (m_serialized_path.CompareI(folder, folder_len) == 0)
		{
			m_serialized_path.Delete(0, folder_len);
			RETURN_IF_ERROR(m_serialized_path.Insert(0, "{Home}"));
			serialized = TRUE;
		}
	}

	if (serialized)
	{
		uni_char* c = m_serialized_path.CStr() + m_serialized_path.FindFirstOf('}');

		while (*(++c))
			if(*c == '\\')
				*c ='/';
	}

	return OpStatus::OK;
}

BOOL
WindowsOpLowLevelFile::IsWritable() const
{
	DWORD ret = GetFileAttributes(m_path.CStr());

	if (ret == 0xFFFFFFFF)
	{
		return TRUE;
	}
	if (ret & FILE_ATTRIBUTE_READONLY)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL
WindowsOpLowLevelFile::IsHidden() const
{
	DWORD ret = GetFileAttributes(m_path.CStr());

	if (ret == 0xFFFFFFFF)
	{
		return FALSE;
	}
	if (ret & FILE_ATTRIBUTE_HIDDEN)
	{
		return TRUE;
	}
	return FALSE;
}

static OP_STATUS MakeDir(const uni_char* path)
{
	DWORD err;

	// C: etc. should never fail
	if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
		path[1] == ':' && path[2] == 0)
	{
		return OpStatus::OK;
	}
	if (!CreateDirectory(path, NULL) && (err = GetLastError()) != ERROR_ALREADY_EXISTS)
	{
		return WinErrToOpStatus(err);
	}

	return OpStatus::OK;
}

static OP_STATUS MakePath(uni_char* path)
{
	int len = path ? uni_strlen(path) : 0;
	for (int i=0; i < len; ++i)
	{
		if (path[i] == '\\' || path[i] == '/')
		{
			path[i] = 0;
			OP_STATUS res = MakeDir(path);
			path[i] = PATHSEPCHAR;
			if (OpStatus::IsMemoryError(res))
			{
				return res;
			}
		}
	}
	return len ? MakeDir(path) : OpStatus::OK;
}

static OP_STATUS MakeParentFolderPath(uni_char* path)
{
	if (!path)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	int len = uni_strlen(path);
	while (--len >= 0)
	{
		if (path[len] == '\\' || path[len] == '/')
		{
			path[len] = 0;
			break;
		}
	}
	OP_STATUS err = MakePath(path);
	if (len >= 0)
	{
		path[len] = PATHSEPCHAR;
	}

	return err;
}
/*
Test for filename validity on Win32. The list of tests come from 
http://en.wikipedia.org/wiki/Filename

The tests are:

1) total path length greater than MAX_PATH

2) anything using the octets 0-31 or characters " < > | :
   (these are reserved for Windows use in filenames. In addition
   each file system has its own additional characters that are
   invalid. See KB article Q100108 for more details).

3) anything ending in "." (no matter how many)
   (filename doc, doc. and doc... all refer to the same file)

4) any segment in which the basename (before first period) matches
   one of the DOS device names
   (the list comes from KB article Q100108 although some people
   reports that additional names such as "COM5" are also special
   devices).

If the path fails ANY of these tests, the result must be to deny access.
*/
BOOL IsWin32FilenameValid(OpString& path)
{
	static const uni_char * const invalid_characters = UNI_L("?\"<>*|:");
	static const uni_char * const invalid_filenames[] = 
	{ 
		UNI_L("CON"), UNI_L("AUX"), UNI_L("COM1"), UNI_L("COM2"), UNI_L("COM3"), 
		UNI_L("COM4"), UNI_L("LPT1"), UNI_L("LPT2"), UNI_L("LPT3"), UNI_L("PRN"), UNI_L("NUL"), NULL
	};
	const uni_char *segment_start;
	unsigned int segment_length;
	const uni_char *pos;

	// test 1
	if(!path.HasContent() || path.Length() >= MAX_PATH)
	{
		/* Path too long (or missing) for Windows. Note that this test is not valid
		 * if the path starts with //?/ or \\?\. */		
		OP_ASSERT(!"Invalid filename provided");
		return FALSE;
	}
	pos = path.CStr();

	/* Skip any leading non-path components. This can be either a drive letter such as C:, or a UNC path such as \\SERVER\SHARE\. */
	if (pos[0] && pos[1] == ':') 
	{
		/* Skip leading drive letter */
		pos += 2;
	} 
	else 
	{
		if ((pos[0] == '\\' || pos[0] == '/') && (pos[1] == '\\' || pos[1] == '/')) 
		{
			/* Is a UNC, so skip the server name and share name */
			pos += 2;
			while (*pos && *pos != '/' && *pos != '\\')
			{
				pos++;
			}
			if (!*pos) 
			{
				/* No share name */
				OP_ASSERT(!"Invalid filename provided");
				return FALSE;
			}
			/* Move to start of share name */
			pos++;	
			while (*pos && *pos != '/' && *pos != '\\')
			{
				pos++;
			}
			if (!*pos) 
			{
				/* No path information */
				OP_ASSERT(!"Invalid filename provided");
				return FALSE;
			}
		}
	}
	while (*pos) 
	{
		unsigned int idx;
		unsigned int base_length;

		while (*pos == '/' || *pos == '\\') 
		{
			pos++;
		}
		if (*pos == '\0') 
		{
			break;
		}
		segment_start = pos;	/* start of segment */
		while (*pos && *pos != '/' && *pos != '\\') 
		{
			pos++;
		}
		segment_length = pos - segment_start;
		/* Now we have a segment of the path, starting at position "segment_start" and length "segment_length" */

		/* Test 2 */
		for(idx = 0; idx < segment_length; idx++) 
		{
			if ((segment_start[idx] > 0 && segment_start[idx] < 32) || uni_strchr(invalid_characters, segment_start[idx])) 
			{
				OP_ASSERT(!"Invalid filename provided");
				return FALSE;
			}
		}
		/* Test 3 */
		if (segment_start[segment_length - 1] == '.') 
		{
			OP_ASSERT(!"Invalid filename provided");
			return FALSE;
		}
		
		/* Test 4 */
		for (base_length = 0; base_length < segment_length; base_length++) 
		{
			if (segment_start[base_length] == '.') 
			{
				break;
			}
		}
		/* base_length is the number of characters in the base path of the segment (which could be the same as the whole segment length,
		   if it does not include any dot characters). */
		if (base_length == 3 || base_length == 4) 
		{
			for (idx = 0; invalid_filenames[idx]; idx++) 
			{
				if (uni_strlen(invalid_filenames[idx]) == base_length && !uni_strnicmp(invalid_filenames[idx], segment_start, base_length)) 
				{
					OP_ASSERT(!"Invalid filename provided");
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}
/*
* At least one of the following flags must be set: OPFILE_READ,
* OPFILE_WRITE, OPFILE_APPEND, OPFILE_UPDATE.
*/

OP_STATUS
WindowsOpLowLevelFile::Open(int mode)
{

	static const UINT MODE_STR_LEN = 6;

	m_file_open_mode = mode;

	uni_char mode_string[MODE_STR_LEN];
	uni_char* p = mode_string;
	BOOL create_path = FALSE;

	if(!IsWin32FilenameValid(m_path))
	{
		return OpStatus::ERR_BAD_FILE_NUMBER;
	}

	// check that we have all the flags we need
	if(!(mode & OPFILE_READ || mode & OPFILE_WRITE || mode & OPFILE_APPEND || mode & OPFILE_UPDATE))
	{
		// missing Open flags, abort
		OP_ASSERT(!"disallowed combination");
		return OpStatus::ERR;
	}
	if((mode & OPFILE_APPEND) && (mode & OPFILE_WRITE))
	{
		OP_ASSERT(!"disallowed combination");
		return OpStatus::ERR;
	}
	if((mode & OPFILE_UPDATE) && ((mode & OPFILE_UPDATE) != OPFILE_UPDATE))
	{
		OP_ASSERT(!"disallowed combination");
		return OpStatus::ERR;
	}
	if (mode & OPFILE_UPDATE)
	{
		*p++ = 'r';
		*p++ = '+';
		OP_ASSERT(!(mode & OPFILE_WRITE) && !(mode & OPFILE_APPEND));
	}
	else if (mode & OPFILE_APPEND)
	{
		*p++ = 'a';
		if (mode & OPFILE_READ)
		{
			*p++ = '+';
		}
		OP_ASSERT(!(mode & OPFILE_WRITE) && !(mode & OPFILE_UPDATE));
	}
	else
	if (mode & OPFILE_WRITE)
	{
		*p++ = 'w';
		if (mode & OPFILE_READ)
		{
			*p++ = '+';
		}
	}
	else
	{
		*p++ = 'r';
	}
	// if any of the file create flags are set, then create the path
	if(mode & (OPFILE_WRITE | OPFILE_APPEND | OPFILE_UPDATE))
	{
		create_path = TRUE;
	}
	if (mode & OPFILE_TEXT)
	{
		*p++ = 't';
	}
	else
	{
		*p++ = 'b';
	}

	if (mode & OPFILE_COMMIT)
	{
		*p++ = 'c'; //Microsoft-extension. "the contents of the file buffer are written directly to disk if either fflush or _flushall is called"
	}

	// none of our file handles should be inherited by other sub processes - DSK-281397
	*p++ = 'N';

	*p = 0;

	// If this assert goes off. Either you have added the possibility to have
	// more than 5 access constants, and therefore need to increase MODE_STR_LEN.
	// Or the pointer p is just not where it should be, and you need to find out why it is
	// not writing inside the mode_string array yourself.
	OP_ASSERT(&mode_string[MODE_STR_LEN] > p && p > &mode_string[0]);

	int shflag = _SH_DENYNO;

	if (mode & OPFILE_SHAREDENYWRITE && mode & OPFILE_SHAREDENYREAD)
	{
		shflag = _SH_DENYRW;
	}
	else if (mode & OPFILE_SHAREDENYWRITE)
	{
		shflag = _SH_DENYWR;
	}
	else if (mode & OPFILE_SHAREDENYREAD)
	{
		shflag = _SH_DENYRD;
	}

	if (m_path.CStr())
	{
		// avoid dialogs if we try to access files on volumes not containing any mounted disk,
		// eg. memory card readers and CD-ROMs.  See DSK-166772
		UINT error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
		m_fp = _wfsopen(m_path.CStr(), mode_string, shflag);

		if (!m_fp && create_path)
		{
			// [pettern 25102005]
			// ignore this on purpose as open will fail further down anyway. MakePath() might
			// fail on valid paths (like "F:").
			OpStatus::Ignore(MakeParentFolderPath(m_path.CStr()));

			m_fp = _wfsopen(m_path.CStr(), mode_string, shflag);
		}
		SetErrorMode(error_mode);
	}

	if (m_fp != NULL)
	{
		return OpStatus::OK;
	}
	else
	{
		const OP_STATUS status = ErrNoToStatus(errno);
		return OpStatus::IsError(status) ? status : OpStatus::ERR;
	}
}

OP_STATUS
WindowsOpLowLevelFile::Close()
{
	OP_STATUS result = OpStatus::OK;

	if (m_fp)
	{
		if (fclose(m_fp) != 0)
			result = OpStatus::ERR;
		m_fp = NULL;
		m_file_open_mode = 0;
	}
	return result;
}

OP_STATUS WindowsOpLowLevelFile::MakeDirectory()
{
	if (m_path.Length() >= MAX_PATH)
	{
		return OpStatus::ERR;
	}

	if (PathIsDirectory(m_path.CStr()))
	{
		return OpStatus::OK;
	}

	if (PathFileExists(m_path.CStr()))
	{
		return OpStatus::ERR;
	}

	uni_char path_copy[MAX_PATH];
	uni_strncpy(path_copy, m_path, MAX_PATH);
	uni_char* next_path;

	//If this is not a relative path, we should just skip the root
	if (PathIsRelative(path_copy))
	{
		next_path = path_copy;
	}
	else
	{
		next_path = uni_strchr(path_copy, '\\') + 1;
	}

	while (next_path && (next_path = uni_strchr(next_path, '\\')) != NULL )
	{
		*next_path = '\0';
		if (!PathIsDirectory(path_copy) && !CreateDirectory(path_copy, NULL))
		{
			if (GetLastError() == ERROR_ACCESS_DENIED)
			{
				return OpStatus::ERR_NO_ACCESS;
			}
			else
			{
				return OpStatus::ERR;
			}
		}

		*next_path = '\\';
		next_path++;
	}

	if (!CreateDirectory(m_path.CStr(), NULL))
	{
		if (GetLastError() == ERROR_ACCESS_DENIED)
		{
			return OpStatus::ERR_NO_ACCESS;
		}
		else
		{
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

BOOL
WindowsOpLowLevelFile::Eof() const
{
	OP_ASSERT(m_fp);
	if(feof(m_fp))
	{
		return TRUE;
	}
	return FALSE;
}

OP_STATUS
WindowsOpLowLevelFile::Exists(BOOL* exists) const
{
	*exists = FALSE;

	if (m_path.IsEmpty())
	{
		return OpStatus::OK;
	}
	// avoid dialogs if we try to access files on volumes not containing any mounted disk,
	// eg. memory card readers and CD-ROMs.  See DSK-166772
	UINT error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

	DWORD ret = GetFileAttributes(m_path.CStr());
	SetErrorMode(error_mode);

	if (ret != 0xFFFFFFFF)
	{
		*exists = TRUE;
		return OpStatus::OK;
	}

	switch (GetLastError())
	{
	case ERROR_ACCESS_DENIED:
		return OpStatus::ERR_NO_ACCESS;
	default:
		return OpStatus::OK;
	}
}

OP_STATUS
WindowsOpLowLevelFile::GetFilePos(OpFileLength* pos) const
{
	if(pos == NULL || m_fp == NULL)
	{
		return OpStatus::ERR;
	}

	__int64 result = _ftelli64(m_fp);
	if(result == -1L)
	{
		return OpStatus::ERR;
	}
	*pos = (OpFileLength)result;
	return OpStatus::OK;
}

OP_STATUS
WindowsOpLowLevelFile::SetFilePos(OpFileLength pos, OpSeekMode seek_mode)
{
	if(m_fp == NULL)
	{
		return OpStatus::ERR_BAD_FILE_NUMBER;
	}
	int whence = SEEK_CUR;

	switch (seek_mode)
	{
		case SEEK_FROM_START:   whence = SEEK_SET; break;
		case SEEK_FROM_END:     whence = SEEK_END; break;
		case SEEK_FROM_CURRENT: whence = SEEK_CUR; break;
	}
	int result = _fseeki64(m_fp, pos, whence);
	if(result != 0)
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

typedef BOOL (WINAPI *lpGetFileAttributesEx)(LPCTSTR lpFileName,  GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);

lpGetFileAttributesEx fpGetFileAttributesEx;

OP_STATUS
WindowsOpLowLevelFile::GetFileLength(OpFileLength* len) const
{
	OpFileLength result = 0;
	OP_STATUS status = OpStatus::OK;

	if (m_fp)
	{
		OpFileLength old = _ftelli64(m_fp);
		if (_fseeki64(m_fp, 0, SEEK_END) == 0)
		{
			result = _ftelli64(m_fp);
			if (_fseeki64(m_fp, old, SEEK_SET) != 0)
			{
				status = OpStatus::ERR;
			}
		}
		else
		{
			status = OpStatus::ERR;
		}
	}
	else
	{
		/* Opera won't start with GetFileAttributesEx() on W95, so we need to use GetProcAddress() */
		HMODULE hModule;

		if(!fpGetFileAttributesEx)
		{
			hModule = GetModuleHandleA("KERNEL32");

			fpGetFileAttributesEx = (lpGetFileAttributesEx) GetProcAddress(hModule, "GetFileAttributesExW");

			OP_ASSERT(fpGetFileAttributesEx != NULL);
		}
		WIN32_FILE_ATTRIBUTE_DATA data;
		BOOL ok = (*fpGetFileAttributesEx)(m_path.CStr(), GetFileExInfoStandard, &data);
		if (ok)
		{
			result = (((OpFileLength) data.nFileSizeHigh) << 32) | ((OpFileLength) data.nFileSizeLow);
		}
		else
		{
			status = GetWindowsLastError();
		}
	}

	*len = result;

	return status;
}

OP_STATUS
WindowsOpLowLevelFile::Write(const void* data, OpFileLength len)
{
	if (!m_fp)
	{
		return OpStatus::ERR_BAD_FILE_NUMBER;
	}

	OpFileLength bytes_written = fwrite(data, 1, (size_t) len, m_fp); // fwrite returns 0 when nothing written, not a negative number
	if (bytes_written < len)
	{
		if (ferror(m_fp))
		{
			const OP_STATUS status = ErrNoToStatus(errno);
			return OpStatus::IsError(status) ? status : OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

#ifdef PI_CAP_FILE_PRINTF
void WindowsOpLowLevelFile::Print(const char *fmt,va_list marker)
{
	if(m_fp)
		::vfprintf(m_fp, fmt, marker);
}
#endif

OP_STATUS
WindowsOpLowLevelFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	OP_ASSERT(m_fp);

	if(!IsOpen())
	{
		return OpStatus::ERR_BAD_FILE_NUMBER;
	}

	OpFileLength num_read = fread(data, 1, (size_t) len, m_fp);
	if (num_read < len)
	{
		if (ferror(m_fp))
		{
			const OP_STATUS status = ErrNoToStatus(errno);
			return OpStatus::IsError(status) ? status : OpStatus::ERR;
		}
	}
	if (bytes_read)
	{
		*bytes_read = num_read;
	}
	return OpStatus::OK;
}


#define READLINE_BUF_SIZE 4096

OP_STATUS
WindowsOpLowLevelFile::ReadLine(char** data)
{
	if (!m_fp || !data)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	char buf[READLINE_BUF_SIZE];
	OpString8 temp;

	// fgets() converts DOS-style CRLF newlines to LF only if file was opened with OPFILE_TEXT, so check both
	while (fgets(buf, READLINE_BUF_SIZE, m_fp) == buf)
	{
		RETURN_IF_ERROR(temp.Append(buf));
		int str_len = temp.Length();

		if (str_len)
		{
			// don't store newline character in the result string
			if (temp[str_len - 1] == '\n')
			{
				temp[str_len - 1] = '\0';
				if (str_len > 1 && temp[str_len - 2] == '\r')
				{
					temp[str_len - 2] = '\0';
				}
			}
			else
			{
				str_len = 0;
			}
		}

		if (str_len || feof(m_fp))
		{
			*data = SetNewStr(temp.CStr());
			return *data ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		}
	}
	if (!feof(m_fp) || ferror(m_fp))
	{
		*data = 0;
		return OpStatus::ERR;
	}

	// end of file needs empty string.
	*data = OP_NEWA(char, (1));
	if (!*data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	**data = 0;
	return OpStatus::OK;
}

OP_STATUS
WindowsOpLowLevelFile::SafeClose()
{
	if (!IsOpen())
	{
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(Flush());
	if (!(m_file_open_mode & OPFILE_COMMIT))
	{
		if (_commit(fileno(m_fp)) != 0)
		{
			return OpStatus::ERR;
		}
	}

	return Close();
}

OP_STATUS
WindowsOpLowLevelFile::SafeReplace(OpLowLevelFile* new_file)
{
	// Check if new_file really exists
	BOOL exists;
	RETURN_IF_ERROR(new_file->Exists(&exists));
	if (!exists)
	{
		return OpStatus::ERR;
	}

	// Make sure everything is flushed, Close() calls fflush() explicitly
	if (new_file->IsOpen())
	{
		new_file->Close();
	}
	// Rename the file
	if (!MoveFileEx(new_file->GetFullPath(), GetFullPath(), MOVEFILE_REPLACE_EXISTING))
		return GetWindowsLastError();
	return OpStatus::OK;
}

OP_STATUS
WindowsOpLowLevelFile::Delete()
{
	DWORD attr;

	attr = GetFileAttributes(m_path.CStr());
	if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		if (RemoveDirectory(m_path.CStr()))
		{
			return OpStatus::OK;
		}
		else
		{
			return GetWindowsLastError();
		}
	}

	if (DeleteFile(m_path.CStr()))
	{
		return OpStatus::OK;
	}
	else
	{
		return GetWindowsLastError();
	}
}
OP_STATUS
WindowsOpLowLevelFile::Flush()
{
	OP_STATUS result = OpStatus::ERR;

	if (m_fp)
	{
		if (fflush(m_fp) == 0)
		{
			result = OpStatus::OK;
		}
	}
	return result;
}

OP_STATUS
WindowsOpLowLevelFile::SetFileLength(OpFileLength len)
{
	if (!m_fp)
	{
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(Flush());
	int result = _chsize_s(_fileno(m_fp), len);
	if (result == 0)
	{
		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR;
	}
}

OP_STATUS WindowsOpLowLevelFile::Move(const class DesktopOpLowLevelFile *new_file)
{
	// Move the file
	OpString &new_name = ((WindowsOpLowLevelFile *) new_file)->m_path;

	if (0 == MoveFile(m_path.CStr(), new_name.CStr()))
	{
		DWORD err = GetLastError();
		if (err == ERROR_NOT_ENOUGH_MEMORY || err == ERROR_OUTOFMEMORY)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			return OpStatus::ERR;
		}
	}

	// Change the name of the file
	return m_path.Set(new_name);
}

OP_STATUS WindowsOpLowLevelFile::Stat(struct _stat *buffer)
{
	if (!buffer)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	int retval;

	retval = _wstat(GetFullPath(), buffer);
	if (retval == -1)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	return OpStatus::OK;
}

OP_STATUS WindowsOpLowLevelFile::ReadLine(char *string, int max_length)
{
	if (!string || !m_fp)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	*string = 0;

	if (fgets(string, max_length, m_fp) == NULL)
	{
		return (feof(m_fp)!=0 && ferror(m_fp)==0) ? OpStatus::OK : OpStatus::ERR;
	}

	return OpStatus::OK;
}

OpLowLevelFile*
WindowsOpLowLevelFile::CreateCopy()
{
	OP_ASSERT(m_path.CStr() != NULL);

	WindowsOpLowLevelFile* copy = OP_NEW(WindowsOpLowLevelFile, ());
	if (copy && m_path.CStr() != NULL)
	{
		if (OpStatus::IsError(copy->m_path.Set(m_path)))
		{
			OP_DELETE(copy);
			copy = NULL;
		}
	}

	return copy;
}

OpLowLevelFile*
WindowsOpLowLevelFile::CreateTempFile(const uni_char* prefix)
{
	OpString path;
	if (OpStatus::IsError(path.Set(m_path.CStr()))) return NULL;
	int slash = path.FindLastOf('\\');
	if (slash != KNotFound)
	{
		path.Delete(slash+1);
	}

	uni_char new_filename[MAX_PATH];
	if (0 == GetTempFileName(path.CStr(), prefix, 0, new_filename))
	{
		//Is missing directory cause of error?
		if (GetFileAttributes(path.CStr()) == 0xFFFFFFFF)
		{
			DWORD error = GetLastError();
			if(error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND)
			{
				//Create folder and try again
				OpStatus::Ignore(MakeParentFolderPath(path.CStr()));
				if (0 == GetTempFileName(path.CStr(), prefix, 0, new_filename))
				{
					return NULL;
				}
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			return NULL;
		}
	}

	WindowsOpLowLevelFile* new_file = new WindowsOpLowLevelFile();
	if (new_file == NULL || OpStatus::IsError(new_file->Construct(new_filename)))
	{
		return NULL;
	}

	return new_file;
}

OP_STATUS
WindowsOpLowLevelFile::CopyContents(const OpLowLevelFile* source)
{
	OP_ASSERT(source != NULL);
	if (source == NULL)
	{
		return OpStatus::ERR;
	}
	if (IsOpen() || source->IsOpen())
	{
		return OpStatus::ERR;
	}

	// Must read through OpLowLevelFile::Read().  source->GetFullPath() is not
	// guaranteed to point to a physical file.  (DSK-302759)

	// FIXME: But that means we have to use non-const methods of it!
	OpLowLevelFile* mutable_source = const_cast<OpLowLevelFile*>(source);

	RETURN_IF_ERROR(mutable_source->Open(OPFILE_READ));

	OP_STATUS result = OpStatus::ERR;

	const uni_char mode[] = UNI_L("wbN");
	const int share_flag = _SH_DENYWR;
	FILE* dest = _wfsopen(m_path.CStr(), mode, share_flag);
	if (dest == NULL)
	{
		OpStatus::Ignore(MakeParentFolderPath(m_path.CStr()));
		errno = 0;
		dest = _wfsopen(m_path.CStr(), mode, share_flag);
	}
	if (dest == NULL)
	{
		result = ErrNoToStatus(errno);
	}
	else
	{
		// Should this be a tweak maybe?
		const size_t buffer_size = 0x8000;
		char buffer[buffer_size];
		OpFileLength bytes_read;
		do
		{
			bytes_read = 0;
			result = mutable_source->Read(buffer, buffer_size, &bytes_read);
			if (OpStatus::IsError(result))
			{
				break;
			}
			if (bytes_read > 0)
			{
				errno = 0;
				if (fwrite(buffer, 1, bytes_read, dest) != bytes_read)
				{
					result = ErrNoToStatus(errno);
					break;
				}
			}
		} while (!source->Eof());

		if (OpStatus::IsSuccess(result) && ferror(dest) != 0)
		{
			result = OpStatus::ERR;
		}

		errno = 0;
		if (fclose(dest) != 0 && OpStatus::IsSuccess(result))
		{
			result = ErrNoToStatus(errno);
		}
	}

	const OP_STATUS close_result = mutable_source->Close();
	if (OpStatus::IsSuccess(result))
	{
		result = close_result;
	}

	return result;
}

#ifdef PI_ASYNC_FILE_OP
/* static */
unsigned __stdcall WindowsOpLowLevelFile::FileThreadFunc( void* pArguments )
{
	WindowsOpLowLevelFile* sync_file = NULL;
	OP_STATUS status = OpStatus::OK;
	BOOL abort = FALSE;
	// get the class we will operate on
	WindowsOpLowLevelFile* file = (WindowsOpLowLevelFile *)pArguments;
	// get a reference to all the data will we deal with
	OpVector<WindowsOpLowLevelFile::FileThreadData> *data_collection = &file->m_thread_data;
	// set up out events to wait for notifications on
	HANDLE ahEvents[] = { file->m_event_quit, file->m_event_file};

	while(!abort)
	{
		// wait for any of the events to be triggered
		DWORD res = ::WaitForMultipleObjects(2, ahEvents, FALSE, INFINITE);

		switch (res)
		{
			case WAIT_OBJECT_0: // Shut down!
				abort = TRUE;
				break;

			case (WAIT_OBJECT_0 + 1): // data is ready to be read/written/deleted
				{
					OpFileLength old_pos = 0;
					BOOL have_data = FALSE;
					WindowsOpLowLevelFile::FileThreadData *data = NULL;

					// Protect all access to the m_thread_data collection, try to keep the critical sections as short as possible.
					// The assumption is that no outside code modifies the items inside the collection, but only the collection itself,
					// and that all items are added at the end of the collection
					::EnterCriticalSection(&file->m_thread_cs);
					have_data = data_collection->GetCount() != 0;
					if(have_data)
					{
						data = data_collection->Get(0);
					}
					::LeaveCriticalSection(&file->m_thread_cs);

					while(have_data)
					{
						if(data->m_seek_pos != FILE_LENGTH_NONE)
						{
							// get the old position. We need to set the position back after finishing the operating, as per the specification
							status = data->m_file->GetFilePos(&old_pos);
							if(OpStatus::IsSuccess(status))
							{
								// do the seek first
								status = data->m_file->SetFilePos(data->m_seek_pos, SEEK_FROM_START);
							}
							if(OpStatus::IsError(status))
							{
								// we had an error, let's call off the operation now
								switch(data->m_operation)
								{
								case WindowsOpLowLevelFile::FileThreadData::THREAD_OP_READ:
									g_thread_tools->PostMessageToMainThread(MSG_ASYNC_FILE_READ, (MH_PARAM_1)new FileNotification(data->m_operation, data->m_file, data->m_listener, status, 0), NULL);
									break;

								case WindowsOpLowLevelFile::FileThreadData::THREAD_OP_WRITE:
									g_thread_tools->PostMessageToMainThread(MSG_ASYNC_FILE_WRITTEN, (MH_PARAM_1)new FileNotification(data->m_operation, data->m_file, data->m_listener, status, 0), NULL);
									break;
								}
							}
						}
						// search went fine, do the operation
						if(OpStatus::IsSuccess(status))
						{
							OpFileLength read;

							switch(data->m_operation)
							{
							case WindowsOpLowLevelFile::FileThreadData::THREAD_OP_SYNC:
								sync_file = data->m_file;
								break;

							case WindowsOpLowLevelFile::FileThreadData::THREAD_OP_READ:
								status = data->m_file->Read((void *)data->m_buffer, data->m_data_len, &read);
								if(old_pos)
								{
									status = data->m_file->SetFilePos(old_pos, SEEK_FROM_START);
								}
								g_thread_tools->PostMessageToMainThread(MSG_ASYNC_FILE_READ, (MH_PARAM_1)new FileNotification(data->m_operation, data->m_file, data->m_listener, status, read), NULL);
								break;

							case WindowsOpLowLevelFile::FileThreadData::THREAD_OP_WRITE:
								status = data->m_file->Write((void *)data->m_buffer, data->m_data_len);
								if(old_pos)
								{
									status = data->m_file->SetFilePos(old_pos, SEEK_FROM_START);
								}
								g_thread_tools->PostMessageToMainThread(MSG_ASYNC_FILE_WRITTEN, (MH_PARAM_1)new FileNotification(data->m_operation, data->m_file, data->m_listener, status, 0), NULL);
								break;

							case WindowsOpLowLevelFile::FileThreadData::THREAD_OP_DELETE:
								status = data->m_file->Delete();
								g_thread_tools->PostMessageToMainThread(MSG_ASYNC_FILE_DELETED, (MH_PARAM_1)new FileNotification(data->m_operation, data->m_file, data->m_listener, status, 0), NULL);
								break;

							case WindowsOpLowLevelFile::FileThreadData::THREAD_OP_FLUSH:
								status = data->m_file->Flush();
								g_thread_tools->PostMessageToMainThread(MSG_ASYNC_FILE_FLUSHED, (MH_PARAM_1)new FileNotification(data->m_operation, data->m_file, data->m_listener, status, 0), NULL);
								break;
							}
						}
						status = OpStatus::OK;

						::EnterCriticalSection(&file->m_thread_cs);
						// delete the item
						data_collection->Delete(0, 1);
						have_data = data_collection->GetCount() != 0;
						if(have_data)
						{
							data = data_collection->Get(0);
						}
						::LeaveCriticalSection(&file->m_thread_cs);
					}
					if(sync_file)
					{
						// event was fired based on a Sync() request, so signal the main thread that we're done now
						g_thread_tools->PostMessageToMainThread(MSG_ASYNC_FILE_SYNC, (MH_PARAM_1)sync_file, NULL);
					}
				}
				break;

			case WAIT_FAILED: // something wrong!
			default:
				OP_ASSERT(0);
		}
	}
	// signal main thread that we are quitting
	SetEvent(file->m_event_quit_done);

	_endthreadex(0);
	return 0;
}

OP_STATUS WindowsOpLowLevelFile::InitThread()
{
	// If this asserts goes off, core is not initialized. Core is needed for serialized paths.
	// Please correct the calling code, to either not use serialized path, or don't use OpLowLevelFile.
	OP_ASSERT(g_opera && g_thread_tools);

	if(!m_event_quit)
	{
		m_event_quit = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	if(!m_event_quit_done)
	{
		m_event_quit_done = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	if(!m_event_file)
	{
		m_event_file = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	if(!m_event_sync_done)
	{
		m_event_sync_done = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	if(!m_event_quit || !m_event_quit_done || !m_event_file || !m_event_sync_done)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	// Create the thread.
	if(!m_thread_handle)
	{
		::InitializeCriticalSection(&m_thread_cs);

		m_thread_handle = (HANDLE)_beginthreadex( NULL, 0, &FileThreadFunc, (void *)this, 0, NULL);
		if(!m_thread_handle)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	return OpStatus::OK;
}

OP_STATUS WindowsOpLowLevelFile::ReadAsync(OpLowLevelFileListener* listener, void* data, OpFileLength len, OpFileLength abs_pos)
{
	OP_STATUS status = OpStatus::OK;

	status = InitThread();
	if(OpStatus::IsError(status))
	{
		return status;
	}
	FileThreadData *thread_data = new FileThreadData();
	if(!thread_data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	thread_data->m_seek_pos = abs_pos;
	thread_data->m_buffer = data;
	thread_data->m_file = this;
	thread_data->m_data_len = len;
	thread_data->m_listener = listener;
	thread_data->m_operation = FileThreadData::THREAD_OP_READ;

	// protect all access to the m_thread_data collection
	::EnterCriticalSection(&m_thread_cs);

	if(OpStatus::IsError(m_thread_data.Add(thread_data)))
	{
		::LeaveCriticalSection(&m_thread_cs);
		return OpStatus::ERR_NO_MEMORY;
	}
	::LeaveCriticalSection(&m_thread_cs);

	// signal the thread to start the operation
	SetEvent(m_event_file);

	return OpStatus::OK;
}

OP_STATUS WindowsOpLowLevelFile::WriteAsync(OpLowLevelFileListener* listener, const void* data, OpFileLength len, OpFileLength pos)
{
	OP_STATUS status = OpStatus::OK;

	status = InitThread();
	if(OpStatus::IsError(status))
	{
		return status;
	}

	FileThreadData *thread_data = new FileThreadData();
	if(!thread_data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	thread_data->m_seek_pos = pos;
	thread_data->m_buffer = data;
	thread_data->m_file = this;
	thread_data->m_data_len = len;
	thread_data->m_listener = listener;
	thread_data->m_operation = FileThreadData::THREAD_OP_WRITE;

	// protect all access to the m_thread_data collection
	::EnterCriticalSection(&m_thread_cs);

	if(OpStatus::IsError(m_thread_data.Add(thread_data)))
	{
		::LeaveCriticalSection(&m_thread_cs);

		return OpStatus::ERR_NO_MEMORY;
	}
	::LeaveCriticalSection(&m_thread_cs);

	// signal the thread to start the operation
	SetEvent(m_event_file);

	return OpStatus::OK;
}

OP_STATUS WindowsOpLowLevelFile::FlushAsync(OpLowLevelFileListener* listener)
{
	OP_STATUS status = OpStatus::OK;

	status = InitThread();
	if(OpStatus::IsError(status))
	{
		return status;
	}
	FileThreadData *thread_data = new FileThreadData();
	if(!thread_data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	thread_data->m_file = this;
	thread_data->m_listener = listener;
	thread_data->m_operation = FileThreadData::THREAD_OP_FLUSH;

	// protect all access to the m_thread_data collection
	::EnterCriticalSection(&m_thread_cs);

	if(OpStatus::IsError(m_thread_data.Add(thread_data)))
	{
		::LeaveCriticalSection(&m_thread_cs);

		return OpStatus::ERR_NO_MEMORY;
	}
	::LeaveCriticalSection(&m_thread_cs);

	// signal the thread to start the operation
	SetEvent(m_event_file);

	return OpStatus::OK;
}

OP_STATUS WindowsOpLowLevelFile::DeleteAsync(OpLowLevelFileListener* listener)
{
	OP_STATUS status = OpStatus::OK;

	status = InitThread();
	if(OpStatus::IsError(status))
	{
		return status;
	}
	FileThreadData *thread_data = new FileThreadData();
	if(!thread_data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	thread_data->m_file = this;
	thread_data->m_listener = listener;
	thread_data->m_operation = FileThreadData::THREAD_OP_DELETE;

	// protect all access to the m_thread_data collection
	::EnterCriticalSection(&m_thread_cs);

	if(OpStatus::IsError(m_thread_data.Add(thread_data)))
	{
		::LeaveCriticalSection(&m_thread_cs);

		return OpStatus::ERR_NO_MEMORY;
	}
	::LeaveCriticalSection(&m_thread_cs);

	// signal the thread to start the operation
	SetEvent(m_event_file);

	return OpStatus::OK;
}

BOOL WindowsOpLowLevelFile::IsAsyncInProgress()
{
	::EnterCriticalSection(&m_thread_cs);

	BOOL in_progress = m_thread_data.GetCount() != 0;

	::LeaveCriticalSection(&m_thread_cs);

	return in_progress;
}

OP_STATUS WindowsOpLowLevelFile::Sync()
{
	// signal thread to dump all data
	if(!m_thread_handle)
	{
		return OpStatus::OK;
	}
	FileThreadData *thread_data = new FileThreadData();
	if(!thread_data)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	thread_data->m_file = this;
	thread_data->m_operation = FileThreadData::THREAD_OP_SYNC;

	// protect all access to the m_thread_data collection
	::EnterCriticalSection(&m_thread_cs);

	if(OpStatus::IsError(m_thread_data.Add(thread_data)))
	{
		::LeaveCriticalSection(&m_thread_cs);

		return OpStatus::ERR_NO_MEMORY;
	}
	::LeaveCriticalSection(&m_thread_cs);

	// signal the thread to start the operation
	SetEvent(m_event_file);

	// wait for completion
	HANDLE ahEvents[] = { m_event_sync_done };

	while(TRUE)
	{
		// wait for any of the events to be triggered
		DWORD res = ::MsgWaitForMultipleObjects(1, ahEvents, FALSE, 1000*30, QS_ALLINPUT);
		if(res == WAIT_OBJECT_0)
		{
			break;
		}
		else if(res == WAIT_OBJECT_0 + 1)
		{
			MSG msg ;

			// Read all of the messages in this next loop,
			// removing each message as we read it.
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				// Dispatch the message.
				DispatchMessage(&msg);
			} // End of PeekMessage while loop.
		}
		else
		{
			// timeout
			OP_ASSERT(FALSE);
			break;
		}
	}
	return OpStatus::OK;
}

#endif // PI_ASYNC_FILE_OP

// called from WindowsOpThreadTools.cpp
void HandleAsyncFileMessages(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
#ifdef PI_ASYNC_FILE_OP
	switch(msg)
	{
		case MSG_ASYNC_FILE_WRITTEN:
			{
				WindowsOpLowLevelFile::FileNotification *data = (WindowsOpLowLevelFile::FileNotification *)par1;
				if(data)
				{
					data->m_listener->OnDataWritten(data->m_file, data->m_status, 0);
				}
				else
				{
					OP_ASSERT(FALSE);
				}
				delete data;
			}
			break;

		case MSG_ASYNC_FILE_READ:
			{
				WindowsOpLowLevelFile::FileNotification *data = (WindowsOpLowLevelFile::FileNotification *)par1;
				if(data)
				{
					data->m_listener->OnDataRead(data->m_file, data->m_status, data->m_data_len);
				}
				else
				{
					OP_ASSERT(FALSE);
				}
				delete data;
			}
			break;

		case MSG_ASYNC_FILE_DELETED:
			{
				WindowsOpLowLevelFile::FileNotification *data = (WindowsOpLowLevelFile::FileNotification *)par1;
				if(data)
				{
					data->m_listener->OnDeleted(data->m_file, data->m_status);
				}
				else
				{
					OP_ASSERT(FALSE);
				}
				delete data;
			}
			break;

		case MSG_ASYNC_FILE_FLUSHED:
			{
				WindowsOpLowLevelFile::FileNotification *data = (WindowsOpLowLevelFile::FileNotification *)par1;
				if(data)
				{
					data->m_listener->OnFlushed(data->m_file, data->m_status);
				}
				else
				{
					OP_ASSERT(FALSE);
				}
				delete data;
			}
			break;

		case MSG_ASYNC_FILE_SYNC:
			{
				WindowsOpLowLevelFile *file = (WindowsOpLowLevelFile *)par1;
				SetEvent(file->m_event_sync_done);
			}
			break;
	}
#endif // PI_ASYNC_FILE_OP
}

void WindowsOpLowLevelFile::ShutdownThread()
{
#ifdef PI_ASYNC_FILE_OP
	Sync();
	// signal thread to quit
	if(m_event_quit)
	{
		SetEvent(m_event_quit);

		// wait for thread to signal it will quit
		if(WAIT_OBJECT_0 != (WaitForSingleObject(m_event_quit_done, 1000*15)))	// wait max 15 seconds
		{
			OP_ASSERT(FALSE);
		}
	}

	if(m_thread_handle)
	{
		CloseHandle(m_thread_handle);
		m_thread_handle = NULL;

		::DeleteCriticalSection(&m_thread_cs);
	}
	if(m_event_quit)
	{
		CloseHandle(m_event_quit);
		m_event_quit = NULL;
	}
	if(m_event_quit_done)
	{
		CloseHandle(m_event_quit_done);
		m_event_quit_done = NULL;
	}
	if(m_event_file)
	{
		CloseHandle(m_event_file);
		m_event_file = NULL;
	}
	if(m_event_sync_done)
	{
		CloseHandle(m_event_sync_done);
		m_event_sync_done = NULL;
	}
#endif // PI_ASYNC_FILE_OP
}

