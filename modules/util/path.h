/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_PATH_H
#define MODULES_UTIL_PATH_H

#ifdef UTIL_PATH_FUNCTIONS

OP_STATUS			OpPathDirFileCombine				(OUT OpString& dest, IN const OpStringC& directory, IN const OpStringC& filename);
uni_char*			OpPathDirFileCombine				(OUT uni_char* szDest, IN const uni_char* szDir, IN const uni_char* szFile);
uni_char*			OpPathAddSubdir						(OUT uni_char* szDest, IN const uni_char* szDir, IN const uni_char* szSubdir);
uni_char*			OpPathAppend						(OUT uni_char* szDest, IN const uni_char* szSubdir);
uni_char*			OpPathAddDirFileSeparator			(IO	uni_char* szDir);
BOOL				OpPathIsDirFileSep					(IN	const uni_char* szPathSepStart);
uni_char*			OpPathRemoveFileName				(IO	uni_char* szPath);
uni_char*			OpPathGetFileName					(IN	const uni_char* szPath);
uni_char*			OpPathCreateTempFileName			(IN const uni_char pszPrefix[4], OUT uni_char* szTempFile);
BOOL				OpPathIsValidFileNameChar			( uni_char ch);
void				OpPathFixInvalidFileName			( uni_char* pszFileName);

inline int			OpPathPathCompare					(const uni_char* psz1, const uni_char* psz2)	{ return uni_stricmp( psz1, psz2); }
inline int			OpPathExtCompare					(const uni_char* psz1, const uni_char* psz2)	{ return uni_stricmp( psz1, psz2); }
inline int			OpPathNameCompare					(const uni_char* psz1, const uni_char* psz2)	{ return uni_stricmp( psz1, psz2); }
inline int			OpPathDirCompare					(const uni_char* psz1, const uni_char* psz2)	{ return uni_stricmp( psz1, psz2); }
inline int			OpPathDriveCompare					(const uni_char* psz1, const uni_char* psz2)	{ return uni_stricmp( psz1, psz2); }

#ifdef MSWIN
uni_char *			OpPathGetFileTypeDescriptionFromOS	( uni_char* pszDest, int nMaxStrLen, const uni_char *pszExt);
const uni_char *	OpPathGetFileExtension				( const uni_char *pszFileName);
inline uni_char *	OpPathGetFileExtension				( uni_char *pszFileName) { return (uni_char*)OpPathGetFileExtension((const uni_char *)pszFileName); };

//
//	class ATempChangeOfActiveDirectory
//	Restores the old active directory in it's dtor
//
class ATempChangeOfActiveDirectory
{
	public:
					ATempChangeOfActiveDirectory( const uni_char* szNewActiveDir, BOOL fContainsFileName);
		virtual	   ~ATempChangeOfActiveDirectory();
	private:
		uni_char	m_szOldActiveDir[_MAX_PATH]; /* ARRAY OK 2009-04-24 johanh */
};
#endif // MSWIN

#endif // UTIL_PATH_FUNCTIONS

#endif // !MODULES_UTIL_PATH_H
