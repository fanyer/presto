/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DESKTOPOPLOWLEVELFILE_H
#define DESKTOPOPLOWLEVELFILE_H

#include "modules/pi/system/OpLowLevelFile.h"

/**
 * DesktopOpLowLevelFile extends the OpLowLevelFile porting interface
 * with functionality specific for the desktop projects.
 */
class DesktopOpLowLevelFile : public OpLowLevelFile
{
public:
	/**
	 * Moves the file to a new location. Might not be able to move a file
	 * to a different partition or volume. DesktopOpFile has already
	 * checked that new_name does not exist.
	 *
	 * @param new_name New name of the file.
	 * @return Status.
	 */
	virtual OP_STATUS Move(const DesktopOpLowLevelFile *new_name) = 0;

	/**
	 * Get status information on a file. Returns OpStatus::ERR_FILE_NOT_FOUND if the
	 * file does not exist.
	 *
	 * @param buffer Status information returned for file
	 * @return Status.
	 */
	virtual OP_STATUS Stat(struct stat *buffer) = 0;

	/**
	 * Read the first max_length bytes of the current line from file
	 *
	 * @param string String to read data into
	 * @param max_length Maximum number of bytes to read
	 * @return Status.
	 */
	virtual OP_STATUS ReadLine(char *string, int max_length) = 0;
};

#endif
