/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef FILELOGGER_H
#define  FILELOGGER_H

#include "modules/search_engine/log/Log.h"
#include "modules/util/opfile/opfile.h"

class FileLogger : public OutputLogDevice
{
public:
	FileLogger(BOOL once = TRUE) : OutputLogDevice(once) {}
	virtual ~FileLogger(void);

	OP_STATUS Open(BOOL truncate, const uni_char *filename, OpFileFolder = OPFILE_ABSOLUTE_FOLDER);

	OP_STATUS Clear(void);
	void Flush(void) {m_file.Flush();}

	/**
	 * @param severity if multiple severity values are ORed, the highest allowed by Mask will be used
	 * @param id user value, can be NULL
	 */
	virtual void VWrite(int severity, const char *id, const char *format, va_list args);
	virtual void VWrite(int severity, const char *id, const uni_char *format, va_list args);
	virtual void Write(int severity, const char *id, int size, const void *data);
	virtual void WriteFile(int severity, const char *id, const uni_char *file_name, OpFileFolder = OPFILE_ABSOLUTE_FOLDER);

protected:
	OpFile m_file;
};

#endif  // FILELOGGER_H

