/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT
#if defined(_BITTORRENT_SUPPORT_)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "modules/util/gen_math.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"

// FIXME: see below
#ifdef UNIX
# include "platforms/posix/posix_native_util.h"
#endif // UNIX
// </FIXME>

#include <cassert>
#ifdef _MACINTOSH_
	#include <sys/stat.h>
	#include "platforms/mac/util/macutils.h"
#endif

#include "bt-util.h"
#include "p2p-opfile64.h"
#include "bt-globals.h"


OpFile64::OpFile64()
:
	m_bytesWritten(0),
	m_bytesRead(0),
	m_mode(kClosed),
#if defined(_WINDOWS_)
	m_share_mode(kShareFull),
	m_fp(INVALID_HANDLE_VALUE)
#elif defined(UNIX) || defined(_MACINTOSH_)
	m_share_mode(kShareFull),
	m_fp(0)
#else
	m_share_mode(kShareFull)
#endif
{
	BT_RESOURCE_ADD("OpFile64", this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::~OpFile64()"
#endif

OpFile64::~OpFile64()
{
#if defined(_WINDOWS_)
	if(m_fp != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(m_fp);
		m_fp = INVALID_HANDLE_VALUE;
	}
#elif defined(UNIX) || defined(_MACINTOSH_)
	if ( m_fp != 0 )
	{
		fclose(m_fp);
		m_fp = 0;
	}
#else
	if(m_opfile.IsOpen())
	{
		m_opfile.Close();
	}
#endif
	DEBUGTRACE8_RES(UNI_L("*** DESTRUCTOR for %s ("), _METHODNAME_);
	DEBUGTRACE8_RES(UNI_L("0x%08x)\n"), this);

	BT_RESOURCE_REMOVE(this);
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::Open()"
#endif

OP_STATUS OpFile64::Open(FileKind fileType, OpenMode openMode, ShareMode shareMode, TranslationMode translationMode)
{
	DEBUGTRACE_FILE(UNI_L("* Opening OpFile64 (0x%08lx)"), this);
	DEBUGTRACE_FILE(UNI_L(", '%s'\n"), m_pathname.CStr());

	/* FIXME: change this class to use DesktopLowLevelFile or, in some other
	 * way, UnixFile or platform #if-ery !
	 */

#if defined(_WINDOWS_)
	DWORD dwDesiredAccess = 0;
	DWORD dwCreation = 0;
	DWORD dwShare = 0;

	if(m_fp != INVALID_HANDLE_VALUE)
	{
		return OpStatus::ERR_BAD_FILE_NUMBER;
	}
	switch(openMode)					// Do *NOT* exclude this switch. It's here for error-checking.
	{
		case kOpenRead:					// "r"
			dwDesiredAccess = GENERIC_READ;
			dwCreation = OPEN_EXISTING;
			break;

		case kOpenAppendAndRead:          // "a+"
		case kOpenAppend:                 // "a"
		case kOpenReadAndWrite:			// "r+"
			dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
			dwCreation = OPEN_EXISTING;
			break;
		case kOpenWrite:					// "w"
			dwDesiredAccess = GENERIC_WRITE;
			dwCreation = OPEN_EXISTING;
			break;
		case kOpenReadAndWriteNew:		// "w+"
			dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
			dwCreation = CREATE_NEW;
			break;

		default:
			return OpStatus::ERR;
	}
	switch(shareMode)
	{
	  case kShareFull:
		dwShare = FILE_SHARE_READ|FILE_SHARE_WRITE;
		break;
	  case kShareDenyRead:
		dwShare = FILE_SHARE_WRITE;
		break;
	  case kShareDenyWrite:
		dwShare = FILE_SHARE_READ;;
		break;
	  case kShareDenyReadAndWrite:
		dwShare = 0;
		break;
	  default:							// check for parameter errors
		return OpStatus::ERR;
	}
	m_fp = CreateFile(m_pathname, dwDesiredAccess, dwShare,
		NULL, dwCreation, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL );

	if(m_fp != INVALID_HANDLE_VALUE)
	{
		m_mode = openMode;
		m_share_mode = shareMode;

		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_ACCESS;
#elif defined(UNIX) || defined(_MACINTOSH_)
	if( m_fp )
	{
		return OpStatus::ERR_BAD_FILE_NUMBER;
	}

#if defined(_MACINTOSH_) || defined(UNIX)
	char mode_buffer[5];
	char* p = mode_buffer;
#else
	uni_char mode_buffer[5];
	uni_char* p = mode_buffer;
#endif

	switch(openMode)
	{
	  case kOpenRead:					// "r"
		*p++ = 'r';
		break;
	  case kOpenReadAndWrite:			// "r+"
		*p++ = 'r';
		*p++ = '+';
        break;
	  case kOpenWrite:					// "w"
		*p++ = 'w';
		break;
      case kOpenAppend:                 // "a"
        *p++ = 'a';
        break;
      case kOpenAppendAndRead:          // "a+"
        *p++ = 'a';
        *p++ = '+';
        break;
	  case kOpenReadAndWriteNew:		// "w+"
		*p++ = 'w';
		*p++ = '+';
        break;
	  default:
		return OpStatus::ERR;
	}
	*p++ = 0; // terminate;

#ifdef UNIX
	PosixNativeUtil::NativeString pathname(m_pathname.CStr());
	m_fp = fopen(pathname.get(), mode_buffer);
#elif defined(_MACINTOSH_)
	char *filename8 = StrToLocaleEncoding(m_pathname.CStr());
	m_fp = fopen(filename8, mode_buffer);
	OP_DELETEA(filename8);
#else
# error("Need to implement")
#endif
	if( m_fp )
	{
		m_mode = openMode;
		m_share_mode = shareMode;
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_ACCESS;
#else
	return m_opfile.Open(fileType, openMode, shareMode, translationMode);
#endif
}
/*
void OpFile64::ConstructL(SpecialFolder specialFolder, const OpStringC& filename)
{
	const OpLocation &location = OpLocation::GetSpecialFolder(specialFolder);

	m_pathname.SetL(location.Path());
	m_pathname.AppendL(filename);
	OP_ASSERT(m_pathname.Length()<=MAX_PATH);

	return;
}
*/
void OpFile64::ConstructL(const OpStringC &location, const OpStringC& filename)
{
	m_pathname.SetL(location);

	int seppos = m_pathname.FindLastOf(PATHSEPCHAR);
	if(seppos != KNotFound)
	{
		if(seppos != m_pathname.Length() - 1)
		{
			m_pathname.Append(PATHSEP);
		}
	}
	m_pathname.AppendL(filename);

	OP_ASSERT(m_pathname.Length()<=MAX_PATH);
}

BOOL OpFile64::IsOpen()
{
#if defined(_WINDOWS_)
	return(m_fp != INVALID_HANDLE_VALUE);
#elif defined(UNIX) || defined(_MACINTOSH_)
	return m_fp != 0;
#else
	return m_opfile.IsOpen();
#endif
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::SetFilePos()"
#endif

BOOL OpFile64::SetFilePos(OpFileLength position, SeekMode seekMode)
{
#if defined(_WINDOWS_)
	BTPROFILE(FALSE);

	if(m_fp == INVALID_HANDLE_VALUE)
	{
		return OpStatus::ERR_BAD_FILE_NUMBER;
	}
	DWORD seek = SEEK_FROM_CURRENT;

	switch(seekMode)
	{
		case SEEK_FROM_START:
			seek = FILE_BEGIN;
			break;

		case SEEK_FROM_CURRENT:
			seek = FILE_CURRENT;
			break;

		case SEEK_FROM_END:
			seek = FILE_END;
			break;
	}
	DWORD nOffsetHigh = (LONG)(position >> 32);
	DWORD nOffsetLow	= (DWORD)(position & 0x00000000FFFFFFFF);
	SetFilePointer(m_fp, (DWORD)nOffsetLow, (LONG *)&nOffsetHigh, seek);
	if(GetLastError() != NO_ERROR)
	{
		return FALSE;
	}
	return TRUE;
#elif defined(UNIX) || defined(_MACINTOSH_)
	if( !m_fp)
	{
		return OpStatus::ERR_BAD_FILE_NUMBER;
	}

	int whence;
	switch(seekMode)
	{
		case SEEK_FROM_START:
			whence = SEEK_SET;
			break;

		case SEEK_FROM_CURRENT:
			whence = SEEK_CUR;
			break;

		case SEEK_FROM_END:
			whence = SEEK_END;
			break;
#ifdef _DEBUG
	default:
		OP_ASSERT(!"Unexpected seek mode");
		whence = SEEK_SET;
		break;
#endif
	}

	int err_code = fseeko(m_fp, position, whence);
	return -1 != err_code;
#else
	return m_opfile.SetFilePos((long)position, seekMode);
#endif

}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::GetFilePos()"
#endif

OpFileLength OpFile64::GetFilePos()
{
#if defined(_WINDOWS_)
	BTPROFILE(FALSE);
	if(m_fp == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	DWORD nOffsetHigh = 0;
	DWORD nOffsetLow	= 0;
	nOffsetLow = SetFilePointer(m_fp, (DWORD)nOffsetLow, (LONG *)&nOffsetHigh, FILE_CURRENT);

	return (OpFileLength)(nOffsetLow) | ((OpFileLength)nOffsetHigh << 32);
#elif defined(UNIX) || defined(_MACINTOSH_)
	return ftello(m_fp);
#else
	OpFileLength length;
	m_opfile.GetFilePos(length);
	return length;
#endif

}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::Length()"
#endif

BOOL OpFile64::Length(OpFileLength *length)
{
#if defined(_WINDOWS_)
	if(m_fp == INVALID_HANDLE_VALUE || length == NULL)
	{
		return FALSE;
	}
	DWORD dwHighSize = 0;
	DWORD dwLowSize = GetFileSize(m_fp, &dwHighSize);

	*length = (OpFileLength)(dwLowSize) | ((OpFileLength)dwHighSize << 32);

	return TRUE;
#elif defined(UNIX) || defined(_MACINTOSH_)
	if( !m_fp || !length )
	{
		return FALSE;
	}

	fflush(m_fp); // Not sure if this is really necessary [espen]
	struct stat buf;
	int err_code = fstat( fileno(m_fp), &buf );
	if( err_code == 0 )
	{
		*length = buf.st_size;
		return TRUE;
	}
	return FALSE;
#else
	return m_opfile.Length((long *)length);
#endif
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::Read()"
#endif

BOOL OpFile64::Read(void *buffer, OpFileLength bytes)
{
#if defined(_WINDOWS_)
	BTPROFILE(FALSE);

	if(m_fp == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	DWORD read;
	return ReadFile(m_fp, buffer, (DWORD)bytes, &read, NULL);
#elif defined(UNIX) || defined(_MACINTOSH_)
	if( !m_fp )
	{
		return FALSE;
	}

	// At most 2^31 bytes at a time.
	unsigned char* buf = (unsigned char*)buffer;
	while( bytes > 0)
	{
		unsigned int bytes_to_read = bytes >= 0xFFFFFFF ? 0xFFFFFFF-1 : bytes;

		size_t read_elements = fread(buf, 1, bytes_to_read, m_fp);
		if( read_elements != bytes_to_read )
			return /*feof(m_fp) ? TRUE : */FALSE;

		bytes -= bytes_to_read;
		buf   += bytes_to_read;
	}
	return TRUE;

#else
	return m_opfile.Read(buffer, (unsigned long)bytes);
#endif
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::Write()"
#endif

BOOL OpFile64::Write(const void *buffer, OpFileLength bytes)
{
#if defined(_WINDOWS_)
	BTPROFILE(FALSE);

	DWORD written;
	if(m_fp == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	return WriteFile(m_fp, buffer, (DWORD)bytes, &written, NULL);
#elif defined(UNIX) || defined(_MACINTOSH_)
	if( !m_fp )
	{
		return FALSE;
	}

	// At most 2^31 bytes at a time.
	unsigned char* buf = (unsigned char*)buffer;
	while( bytes > 0)
	{
		unsigned int bytes_to_write = bytes >= 0xFFFFFFF ? 0xFFFFFFF-1 : bytes;

		size_t written_elements = fwrite(buf, 1, bytes_to_write, m_fp);
		if( written_elements != bytes_to_write )
			return FALSE;

		bytes -= bytes_to_write;
		buf   += bytes_to_write;
	}
	return TRUE;
#else
	return m_opfile.Write(buffer, (unsigned long)bytes);
#endif
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::Close()"
#endif

BOOL OpFile64::Close()
{
	DEBUGTRACE_FILE(UNI_L("* Closing OpFile64 (0x%08lx)"), this);
	DEBUGTRACE_FILE(UNI_L(", '%s'\n"), m_pathname.CStr());

#if defined(_WINDOWS_)
	BOOL success;
	if(m_fp == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	success = CloseHandle(m_fp);
	if(success)
	{
		m_fp = INVALID_HANDLE_VALUE;
	}
	return success;
#elif defined(UNIX) || defined(_MACINTOSH_)
	if( !m_fp )
	{
		return FALSE;
	}

	fclose(m_fp);
	m_fp = 0;
	return TRUE;
#else
	return m_opfile.Close();
#endif
}

#ifdef _METHODNAME_
#undef _METHODNAME_
#define _METHODNAME_ "OpFile64::Flush()"
#endif

BOOL OpFile64::Flush()
{
#if defined(_WINDOWS_)
	BTPROFILE(FALSE);

	if(m_fp == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	return ::FlushFileBuffers(m_fp);
#elif defined(UNIX) || defined(_MACINTOSH_)
	if( !m_fp )
	{
		return FALSE;
	}
	fflush(m_fp);
	return TRUE;
#else
	return m_opfile.Flush();
#endif
}

OP_STATUS OpFile64::MakeDir(const uni_char* path)
{
#if defined(_WINDOWS_)
	if (!CreateDirectory(path, NULL))
	{
		DWORD error = GetLastError();

		if(error != ERROR_ALREADY_EXISTS)
		{
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;

#elif defined(UNIX) || defined(_MACINTOSH_)

#if defined(UNIX)
	PosixNativeUtil::NativeString nativepath (path);
	int ret = mkdir(nativepath.get(), 0777 );
#else
	char *dirName = StrToLocaleEncoding(path);

	int ret = mkdir(dirName, 0777);

	OP_DELETEA(dirName);
#endif

	if(ret == 0 || errno == EEXIST)
	{
		return OpStatus::OK;
	}
	return OpStatus::ERR;
#else
	return OpStatus::ERR;
#endif

}

OP_STATUS OpFile64::MakePath(uni_char* path)
{
	int len = path ? uni_strlen(path) : 0;
	for (int i=0; i < len; ++i)
	{
		if (path[i] == '\\' || path[i] == '/')
		{
			path[i] = 0;
			OP_STATUS res = MakeDir(path);
			path[i] = PATHSEPCHAR;
			if (OpStatus::IsMemoryError(res)) return res;
		}
	}
	return OpStatus::OK;

//	return len ? MakeDir(path) : OpStatus::OK;
}

#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
