/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_FILEFUN_H
#define MODULES_UTIL_FILEFUN_H

#include "modules/util/opfile/opfile.h"

#ifndef NO_EXTERNAL_APPLICATIONS
OP_STATUS Execute(const uni_char* app, const uni_char* filename);
#endif // !NO_EXTERNAL_APPLICATIONS

#ifdef UTIL_SPLIT_FILENAME_INTO_COMPONENTS
/**
 * Splits the candidate filename into its path, filename and extension components
 *
 * @param candidate The candidate name to split.
 * @param path Path component on return. Will be ignored if NULL.
 * @param filename Filename component on return. Will be ignored if NULL.
 * @param extension Extension component on return. Will be ignored if NULL.
 *
 */
void SplitFilenameIntoComponentsL(const OpStringC &candidate, OpString *path, OpString *filename, OpString *extension);
#endif // UTIL_SPLIT_FILENAME_INTO_COMPONENTS

/**
 * Replaces the 'candidate' argument with the file path it points to if
 * 'candidate' is a symbolic link
 *
 * @param candidate The candidate name to examine.
 * @param candidate_max_len Total buffer size of candidate.
 */
void ReplaceWithSymbolicLink( uni_char* candidate, INT32 candidate_max_len );

#ifdef CREATE_UNIQUE_FILENAME
/**
 * Makes the filename unique for the folder it specifies.
 *
 * @param[out] filename as OpString&
 *
 * @returns OpStatus:OK on success or OpStatus::ERR or error.
 */
OP_STATUS CreateUniqueFilename(OpString& filename);
/**
 * Makes the filename unique for the folder it specifies.
 *
 * @param[out] filename as OpString&
 * @param folder The folder where the file is meant to be placed.
 * @param file_component The name of the filename.
 *
 * @returns OpStatus:OK on success or OpStatus::ERR or error.
 */
OP_STATUS CreateUniqueFilename(OpString& filename, OpFileFolder folder, const OpStringC& file_component);
OP_STATUS CreateUniqueFilename(OpString& filename, OpStringC& path_component, OpStringC& file_component, OpStringC& extension_component, BOOL found_extension);
#endif // CREATE_UNIQUE_FILENAME

/** Find the last path separator in a substring.
 *
 * The substring to search is denoted by [start, end), i.e. the character
 * pointed to by end is not searched.
 */
inline const uni_char * GetLastPathSeparator(const uni_char* start, const uni_char* end)
{
	while (end > start)
	{
		if (*--end == PATHSEPCHAR)
			return end;
	}
	return NULL;
}

/**
 * Finds the longest prefix of full_path that equals root_path, when compared
 * using OpSystemInfo::PathsEqual(). It then returns the part of full_path
 * following that prefix, or NULL if no full_path prefix matching root_path
 * could be found.
 *
 * root_path must be absolute and point either to full_path or to a parent of
 * full_path. The comparison is performed with OpSystemInfo::PathsEqual() so
 * the exact notion of "same path" is platform specific, e.g. a symlink may be
 * equal to the file it points to.
 *
 * Example:
 *   FindRelativePath("/a/../b/file.txt", "/b/");
 * would return "file.txt" because "/a/../b/" points to the same directory as
 * "/b/".
 *
 * @param full_path Full path which is searched for root_path.
 * @param root_path Absolute path to search for at the beginning of full_path.
 * @return the remaining part of full_path or NULL if no match for root_path has been found.
 *         The pointer returned points to a substring of full_path so it has the same lifetime.
 */
const uni_char* FindRelativePath(const uni_char* full_path, const uni_char* root_path);

#endif // !MODULES_UTIL_FILEFUN_H
