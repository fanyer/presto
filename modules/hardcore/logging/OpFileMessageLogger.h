/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_LOGGING_OPFILEMESSAGELOGGER_H
#define MODULES_HARDCORE_LOGGING_OPFILEMESSAGELOGGER_H

#ifdef HC_MESSAGE_LOGGING
#include "modules/hardcore/logging/OpMessageLogger.h"
#include "modules/util/OpSharedPtr.h"
#include "modules/opdata/OpStringStream.h"

class OpLogFormatter;
class OpFile;

/** OpMessageLogger implementation that writes to file.
 *
 * Writes the message log to a text file.
 *
 * In case of an error during writing the data to file, the logger will
 * disable itself, ignoring any further log requests.
 */
class OpFileMessageLogger : public OpMessageLogger
{
public:
	/** Construct an OpFileMessageLogger.
	 *
	 * @param logFile File for logging. Must already be created. If it's not
	 * yet opened, it will be. If it's opened but not writable, it will be closed
	 * and opened again in writable mode. If the file cannot be opened, logging
	 * will become disabled.
	 * @param timeProvider Used to get the time at which a log event occured.
	 */
	OpFileMessageLogger(OpSharedPtr<OpFile> logFile,
						OpSharedPtr<OpLogFormatter> logFormatter);
	~OpFileMessageLogger();

	virtual void BeforeDispatch(const OpTypedMessage* msg);
	virtual void AfterDispatch(const OpTypedMessage* msg);
	virtual void Log(const OpTypedMessage* msg, const uni_char* action);
	virtual bool IsAbleToLog() const;

	/** Create a ready for use OpFileMessageLogger.
	 *
	 * Creates an OpFileMessageLogger that uses a OpReadableLogFormatter and
	 * writes to @a logFileNameBase[address of component manager].txt. If an error
	 * occurs while opening the file for writing or creating the logger, an
	 * empty pointer is returned.
	 *
	 * @param logFileNameBase Base of the output file's name. The complete name
	 * will be @a logFileNameBase concatenated with OpComponentManager's address
	 * and .txt extension.
	 * @return Ready to use logger or empty pointer if something went wrong.
	 */
	static OpSharedPtr<OpFileMessageLogger> Create(const uni_char *logFileNameBase);
private:
	/** Write the string stream to file.
	 *
	 * @param in StringStream to dump.
	 */
	void WriteToFile(const OpUniStringStream& in);

	/** Check if an operation returned successfully.
	 *
	 * Sets the logger in an "unable to log" state if @a in is not a success.
	 * This prohibits further attempts to write to a file and makes some methods
	 * return immediately.
	 *
	 * @param in Status of the checked operation.
	 */
	void CheckOpStatus(OP_STATUS in);

	OpSharedPtr<OpFile> m_logFile;
	OpSharedPtr<OpLogFormatter> m_logFormatter;

	/// When false, no more logging is performed.
	bool m_ableToLog;
};

#endif // HC_MESSAGE_LOGGING
#endif // MODULES_HARDCORE_LOGGING_OPFILEMESSAGELOGGER_H
