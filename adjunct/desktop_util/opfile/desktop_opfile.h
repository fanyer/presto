/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOP_OPFILE_H
#define DESKTOP_OPFILE_H

#ifdef _MACINTOSH_
#include <sys/stat.h>
#endif

class OpFile;

/**
 * Access to the extended OpFile APIs for desktop platforms. Please note
 * that using this class requires the OpLowLevelFile::Create()
 * implementation to create DesktopOpLowLevelFile-derived objects.
 *
 * This is only a container class with static methods, the class is needed
 * since OpFile declares it as a friend, and DesktopOpFileUtils needs access
 * to the instance variables in OpFile.
 */
class DesktopOpFileUtils
{
public:
	/**
	 * Moves the file to a new location. Might not be able to move a file
	 * to a different partition or volume. Returns OpStatus::ERR if the
	 * destination already exists.
	 *
	 * @param old_location Old location of the file.
	 * @param new_location New location of the file.
	 * @return Status.
	 */
	static OP_STATUS Move(OpFile *old_location, const OpFile *new_location);

	/**
	 * Get status information on a file. Returns OpStatus::ERR_FILE_NOT_FOUND if the
	 * file does not exist.
	 *
	 * @param file File to query for information
	 * @param buffer Status information returned for file
	 * @return Status.
	 */
	static OP_STATUS Stat(const OpFile *file, struct stat *buffer);

	/**
	 * Check if filename exists as a file.
	 *
	 * @param filename Full path to file
	 * @param exists TRUE if file exists, FALSE if it does not
	 * @return Status.
	 */
	static OP_STATUS Exists(const OpStringC& filename, BOOL& exists);

	/**
	 * Check if filename exists as a file.  On failure to check, assume it
	 * does.
	 *
	 * @param filename Full path to file
	 * @return @c FALSE iff the file doesn't exist for sure
	 */
	static BOOL MightExist(const OpStringC& filename);

	/**
	 * Read the first max_length bytes of the current line from file
	 *
	 * @param string String to read data into
	 * @param max_length Maximum number of bytes to read
	 * @param file File to read from
	 * @return Status.
	 */
	static OP_STATUS ReadLine(char *string, int max_length, const OpFile *file);

	/**
	 * Copy (recursive) from OpFile to another OpFile
	 *
	 * @param target Target to copy to
	 * @param src Src to copy from
	 * @param recursive Recursive copy
	 * @param fail_if_exists Fail if target file exists
	 * @return Status.
	 */
	static OP_STATUS Copy(OpFile& target, OpFile& src, BOOL recursive, BOOL fail_if_exists);


	/**
	 * Check if a folder is empty.
	 *
	 * @param path the folder path
	 * @param empty receives the check result.  The value is only reliable if
	 *		OpStatus::OK is returned.
	 * @return OpStatus::OK on success, or an error status otherwise.  Most
	 *		notably, en error is returned if @a path doesn't point to a folder,
	 *      or the folder contents cannot be listed.
	 */
	static OP_STATUS IsFolderEmpty(const OpStringC& path, BOOL& empty);
};

#endif
