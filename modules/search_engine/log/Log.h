/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef IO_LOG_H
#define IO_LOG_H

#include "modules/util/opfile/opfile.h"

class OutputLogDevice;
class InputLogDevice;

/**
 * General text and binary logging interface for writing to various devices and reading logs from log files.
 */
class SearchEngineLog
{
public:
	enum Device
	{
		NullDev    = 0,
		StdOut     = 1,  // not supported now
		StdErr     = 2,  // not supported now
		File       = 3,  ///< absolute path to file
		FolderFile = 4,  ///< path with OpFileFolder
		DebugOut   = 5,  // not supported now
		SysLog     = 6,  // not supported now

		FOverwrite = 256,  // OR with File or OpFile to truncate a the file, if it exists
		LogOnce    = 512   // suppress duplicate messages
	};

	enum Severity
	{
		Debug   = 1,   ///< verbose data for debugging
		Info    = 2,   ///< informational messages
		Notice  = 4,   ///< unusual situation that merits investigation; a significant event that is typically part of normal day-to-day operation
		Warning = 8,   ///< recoverable errors
		Crit    = 16,  ///< critical situations
		Alert   = 32,  ///< immediate action required
		Emerg   = 64   ///< system is or will be unusable if situation is not resolved
	};

	/**
	 * Creates a new log for writing
	 * @param device SearchEngineLog::Device where to write messages
	 * @param ... SearchEngineLog::File: uni_char *filename, SearchEngineLog::FolderFile: uni_char *filename, OpFileFolder folder
	 * @return NULL on error
	 */
	static OutputLogDevice *CreateLog(int device, ...);

	/**
	 * Rename filename to filename.1, filename.1 to filename.2 etc., up to max_files. Max allowed value is 99
	 */
	static OP_STATUS ShiftLogFile(int max_files, const uni_char *filename, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);

	/**
	 * Open an existing log for reading
	 */
	static InputLogDevice *OpenLog(const uni_char *filename, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);

	/*
	what is missing here:

	OP_STATUS Init();
	OP_STATUS SetLogDevice(int severity, Device device);

	void Write(int module_id, int severity, const char *msg_id, const char *format, ...);

	filtering based on module_id
	preferences in ini file
	g_log instance
	*/

	/** @return string for the highest of given ORed severities */
	static const char *SeverityStr(int severity);
};

/**
 * writes ASCII, UNICODE (converted into UTF-8) and binary data
 */
class OutputLogDevice
{
public:
	OutputLogDevice(BOOL once = TRUE) {m_mask = -1; m_hash = 0; m_once = once;}
	virtual ~OutputLogDevice(void) {};

	/** suppress some type of messages
	 * @param severity allowed ORed Log::Severity values
	 */
	void Mask(int severity) {m_mask = severity;}

	/**
	 * @param severity if multiple severity values are ORed, the highest allowed by Mask will be used
	 * @param id user value, can be NULL
	 */
	virtual void VWrite(int severity, const char *id, const char *format, va_list args) = 0;

	void Write(int severity, const char *id, const char *format, ...);

	/** UNICODE variant of Write */
	virtual void VWrite(int severity, const char *id, const uni_char *format, va_list args) = 0;

	void Write(int severity, const char *id, const uni_char *format, ...);

	/** write binary data */
	virtual void Write(int severity, const char *id, int size, const void *data) = 0;

	/** dump file into log */
	virtual void WriteFile(int severity, const char *id, const uni_char *file_name, OpFileFolder = OPFILE_ABSOLUTE_FOLDER) = 0;

protected:
	OP_BOOLEAN MakeLine(OpString8 &dst, int severity, const char *id, const char *format, va_list args);
	OP_BOOLEAN MakeLine(OpString8 &dst, int severity, const char *id, const uni_char *format, va_list args);
	OP_STATUS MakeBLine(OpString8 &dst, int severity, const char *id);

	static int Hash(const char *data, int size);

	int m_mask;
	int m_hash;
	BOOL m_once;
};

/**
 * reads from existing file log
 */
class InputLogDevice
{
public:
	InputLogDevice(void);
	~InputLogDevice(void);

	OP_STATUS Construct(const uni_char *filename, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);

	/**
	 * @return TRUE is there are no more messages in the log
	 */
	BOOL End(void);

	/**
	 * read the next log message
	 * @return IS_FALSE if nothing was read due to end of file
	 */
	OP_BOOLEAN Read(void);

	double MTime(void) const {return m_time;}

	SearchEngineLog::Severity Severity(void) const {return m_severity;}

	const char *Id(void) const {return m_id.CStr();}

	BOOL Binary(void) const {return m_binary;}

	const char *Message(void) const {return m_message.CStr();}

	const uni_char *UMessage(void);

	int DataSize(void) const {return (int)m_size;}

	const void *Data(void);

	/** save binary data into a file */
	OP_STATUS SaveData(const uni_char *file_name, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);

protected:
	void Decompress(void);

	OpFile m_file;

	double m_time;
	SearchEngineLog::Severity m_severity;
	BOOL m_binary;
	OpString8 m_id;
	OpString8 m_message;
	OpString m_umessage;
	OpFileLength m_size;
	OpFileLength m_compressed_size;
	char *m_data;
};

#endif  // IO_LOG_H

