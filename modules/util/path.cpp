/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; coding: iso-8859-1 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef UTIL_PATH_FUNCTIONS

#include "modules/util/path.h"
#include "modules/util/str.h"
#include "modules/pi/OpSystemInfo.h"

//
//	_path_DirFileSeps
//
//	E.g  the slash between the dir and filename in windows.  If more than
//	one is allowed then put the preferred one first.  If this should be a
//	string on some plattform then let me know - DG.
//
#ifdef MSWIN
const uni_char _path_DirFileSeps[] =
{
	UNI_L('\\'),
	UNI_L('/')
};
#elif defined(UNIX) || defined(_MACINTOSH_)
const uni_char _path_DirFileSeps[] = { '/' };
#endif

//
//	_OP_PATH_INVALID_FILENAME_CHARS
//	Invalid characters in a filename not includeing any path/drive info
//
#if defined(MSWIN)
	#define _OP_PATH_INVALID_FILENAME_CHARS	UNI_L("\\/:*?\"<>|")
#elif defined (UNIX) || defined(_MACINTOSH_)
# define _OP_PATH_INVALID_FILENAME_CHARS UNI_L("\\/*?\"| \t")
#endif



BOOL OpPathIsValidFileNameChar( uni_char ch)
{
	return ((unsigned)ch > 0x1f) && (uni_strchr( _OP_PATH_INVALID_FILENAME_CHARS, ch) == NULL);
}


//	OpPathFixInvalidFileName
//
//	Replace invalid filname characters in the argument string with an underscore.
//	NOTE: argument should only contain a filename. NOT any file or directory info.
void OpPathFixInvalidFileName
(
	uni_char* pszFileName	//	filename only (not containging any file or dir info)
)
{
	//
	//	fix invalid filename characters.
	//
	if (pszFileName)
	{
		uni_char *pch = pszFileName;
		while (*pch)
		{
			if (!OpPathIsValidFileNameChar(*pch))
				*pch = '_';
			++pch;
		}
	}

	//
	//	fix invalid lengths etc
	//
#ifdef MSWIN

# ifndef MAX_EXT
#   ifndef _MAX_EXT
#    define _MAX_EXT 3
#   endif
#  define MAX_EXT _MAX_EXT
# endif

	uni_char szFName[_MAX_FNAME] = UNI_L("");

	uni_char szExt[_MAX_EXT] = UNI_L("");
	_tsplitpath( pszFileName, NULL, NULL, szFName, szExt);
	_tmakepath( pszFileName, NULL, NULL, szFName, szExt);
#endif // MSWIN
}

//	OpPathDirFileCombine
//	Concatenates two strings that represent properly formed paths into one path,
OP_STATUS OpPathDirFileCombine(OUT OpString& dest, IN const OpStringC& directory, IN const OpStringC& filename)
{
	// reserve one extra character for possibly added separator
	if (!dest.Reserve(directory.Length()+1+filename.Length()))
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(dest.Set(directory));

	if (dest.HasContent())
	{
		OpPathAddDirFileSeparator(dest.CStr());
	}

	return dest.Append(filename);
}

uni_char*
OpPathDirFileCombine
(
	OUT uni_char*		szDest,
	IN	const uni_char*	szDir,
	IN	const uni_char*	szFile
)
{
	if (!szDest)
		return NULL;

	if (IsStr(szDir) && szDest != szDir)
	{
		uni_strcpy( szDest, szDir);
	}

	if (IsStr(szFile))
	{
		if (IsStr(szDest))
			OpPathAddDirFileSeparator( szDest);
		uni_strcat( szDest, szFile);
	}
	return szDest;
}

uni_char* OpPathAddSubdir
(
	OUT uni_char*		szDest,
	IN	const uni_char*	szDir,
	IN	const uni_char*	szSubdir
)
{
	return OpPathDirFileCombine( szDest, szDir, szSubdir);
}

uni_char* OpPathAppend(OUT uni_char* szDest, IN	const uni_char*	szSubdir)
{
	return OpPathDirFileCombine(szDest, szDest, szSubdir);
}

//	OpPathAddDirFileSeparator
//	Adds a dir/path separtor if missing (the slash in windows)
uni_char*
OpPathAddDirFileSeparator
(
	IO	uni_char*	szDir
)
{
	if (!szDir)
		return NULL;

	uni_char *pchLast = StrEnd( szDir);
	if (pchLast!= szDir)
		-- pchLast;

	if (!OpPathIsDirFileSep( pchLast))
	{
		*(pchLast+1) = _path_DirFileSeps[0];
		*(pchLast+2) = (uni_char)0;
	}
	return szDir;
}

//	OpPathIsDirFileSep
//
//	Returns TRUE if the pointer points to a valid Dir/File separator
BOOL
OpPathIsDirFileSep
(
	IN	const uni_char* szPathSepStart
)
{
	if (!szPathSepStart)
		return FALSE;

	for( unsigned int i=0; i<ARRAY_SIZE(_path_DirFileSeps); i++)
	{
		if (*szPathSepStart == _path_DirFileSeps[i])
			return TRUE;
	}
	return FALSE;
}


//	OpPathRemoveFileName
//
//	Removes the filename part from a full path.
//	NOTE "szPath" is assumed to really contain a filename
uni_char*
OpPathRemoveFileName
(
	IO	uni_char*	szPath
)
{
	uni_char *szFileName = OpPathGetFileName( szPath);
	if (szFileName)
	{
		*szFileName = 0;
		if (szFileName>szPath)
			*(szFileName-1) = 0;	//	get rid of dirsep if any
	}
	return szFileName;
}



//	OpPathGetFileName
//
//	Returns a ptr to the filename part in 'szPath'
//	NOTE: 'szPath' is assumed to really contain a filename.
uni_char*
OpPathGetFileName
(
	IN	const uni_char*	szPath
)
{
	if( !szPath)
		return NULL;

	uni_char *p = (uni_char*)szPath;
	uni_char *szFName = p;

	while( *p)
	{
		if( OpPathIsDirFileSep(p))
			szFName = p+1;
		p++;
	}
	return szFName;

}


#if defined(MSWIN)
uni_char * OpPathGetFileTypeDescriptionFromOS
(
	OUT	uni_char *		pszDest,
	IN	int				nMaxStrLen,
	IN	const uni_char *pszExt
)
{
	OP_ASSERT(!"Deprecated API: Use OpSystemInfo directly");
	//	Param sanity check
	if (! (nMaxStrLen>0 && pszDest && IsStr(pszExt)))
		return NULL;
	*pszDest = 0;
	uni_char szDummyFileName[_MAX_PATH] = UNI_L("dummy_file_name.");

	//	Make sure space avail to construct dummy filename
	if ( (uni_strlen(szDummyFileName) + uni_strlen(pszExt) + 1) > ARRAY_SIZE(szDummyFileName))
		return NULL;

	//	Create dummy filename and ask Windows about it
	uni_strcat( szDummyFileName, pszExt);

	g_op_system_info->GetFileTypeName(szDummyFileName, pszDest, nMaxStrLen);
	return pszDest;
}


//	OpPathGetFileExtension
//
//	Return a pointer to where the filename extension starts or should start
const uni_char * OpPathGetFileExtension( const uni_char *pszFileName)
{
	if( !pszFileName)
		return NULL;

	if (uni_str_eq(pszFileName, ".."))//	Not '..'
		return StrEnd( pszFileName);

	const uni_char *pszExt = uni_strrchr( pszFileName, '.');

	if(	!pszExt									//	'.' not found
		||	(pszExt==pszFileName)				//	A filename must be preceding the '.'
		|| !(*pszExt))							//	An extension must follow the '.'
		return StrEnd( pszFileName);

	return pszExt+1;
}

ATempChangeOfActiveDirectory::ATempChangeOfActiveDirectory( const uni_char*szNewActiveDir, BOOL fContainsFileName)
{
	BOOL res = GetCurrentDirectory(MAXSTRLEN(m_szOldActiveDir), m_szOldActiveDir);
	OP_ASSERT(res);

	uni_char szDir[_MAX_PATH]; /* ARRAY OK 2009-04-24 johanh */
	szDir[0] = 0;

	uni_strlcpy(szDir, szNewActiveDir, ARRAY_SIZE(szDir));
	if( fContainsFileName)
		OpPathRemoveFileName( szDir);

	if (IsStr(szDir) && uni_stricmp(m_szOldActiveDir, szDir)!=0)
	{
		res = SetCurrentDirectory((LPCTSTR)szDir);
		OP_ASSERT(res);
	}
}

ATempChangeOfActiveDirectory::~ATempChangeOfActiveDirectory()
{
	if (*m_szOldActiveDir)
	{
		if(!SetCurrentDirectory((LPCTSTR)m_szOldActiveDir))
		{
			OP_ASSERT(!"SetCurrentDirectory failed!");
		}
	}
}
#endif // MSWIN

#endif // UTIL_PATH_FUNCTIONS
