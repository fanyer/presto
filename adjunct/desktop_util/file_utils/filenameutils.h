/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef FILENAMEUTILS_H
#define FILENAMEUTILS_H

/**
 * Convert a file://localhost to a fullpath
 *
 * @param filename as OpString&
 *
 * @returns OpStatus
 */
OP_STATUS ConvertURLtoFullPath(OpString& filename);


/**
 * Converts a fullpath filename to a URL
 *
 * @param URL as OpString&
 * @param file_name as uni_char*
 *
 * @returns OpStatus
 */

OP_STATUS ConvertFullPathtoURL(OpString& URL, const uni_char * file_name);

/**
 * Strips off the file name part of a path and returns the rest.
 *
 * @param full_path Must be an absolute path
 * 
 * @returns OpStatus
 */
 OP_STATUS GetDirFromFullPath(OpString& full_path);

 /**
 * Strips off the file name part of a path and returns the rest.
 *
 * @param full_path Must be an absolute path
 * @param path		The result after removing the file name from full_path. 
 *
 * @returns OpStatus
 */
 OP_STATUS GetDirFromFullPath(const uni_char* full_path, OpString& path);


/**
 * Checks if the last char in the string is a trailing slash.
 *
 * @param path The path can be to a file or directory or anything else
 *
 * @return BOOL
 */
 BOOL HasTrailingSlash(const uni_char* path);

 /**
  * Checks if the extension of the file/path is equal to 'extension' case insensitive.
  *
  * @param a file name or full path
  * @param an extension to look for
  * 
  * @return TRUE if the file/path has the same extension as 'extension'
  *			FALSE if the extension is not the same or is missing.
  */
BOOL HasExtension(const uni_char* file, const char* extension);

#endif // FILENAMEUTILS_H
