/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef PARSER_LOGGER_H
#define PARSER_LOGGER_H

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"


/**
 * Helper class that logs parser output to file (if command line option '-uiparserlog <filename>' is given)
 */
class ParserLogger : public FileLogTarget
{
public:
	/**
	 * Automatically indents a block of log entries.
	 *
	 * Example:
	 * @code
	 *	ParserLogger::AutoIndenter indenter(my_logger, "My block");
	 *	RETURN_IF_ERROR(...);
	 *	indenter.Done();
	 * @endcode
	 *
	 * 'indenter' will first output the message at the start of the block and
	 * indent the block.  When it goes out of scope, it will close the block
	 * with a suitable message depending on whether there was an error within
	 * the block or not (that's why Done() has to be called last on success).
	 */
	class AutoIndenter
	{
	public:
		/**
		 * @param logger the logger
		 * @param message the message the opens the block of entries
		 * @param param an optional parameter to @a message
		 */
		AutoIndenter(ParserLogger& logger, const OpStringC8& message, const OpStringC8& param = NULL);
		~AutoIndenter();

		/**
		 * Tells the indenter the block was successful.
		 */
		void Done() { m_success = true; }

	private:
		ParserLogger& m_logger;
		OpString8 m_message;
		bool m_success;
	};

	ParserLogger() : m_indentation(0) {}

	/**
	 * Outputs a log entry on error.
	 *
	 * To be used like this:
	 * @code
	 *	RETURN_IF_ERROR(log.Evaluate(Function(), "error message"));
	 * @endcode
	 *
	 * @param status indicates if there was an error
	 * @param output the log entry to output on error
	 * @param param an additional log entry parameter
	 * @return @a status
	 */
	OP_STATUS Evaluate(OP_STATUS status, const char* output)
		{ if (OpStatus::IsError(status)) OutputEntry(output); return status; }
	OP_STATUS Evaluate(OP_STATUS status, const char* output, const char* param)
		{ if (OpStatus::IsError(status)) OutputEntry(output, param); return status; }

	OP_STATUS OutputEntry(const OpStringC8& entry)
		{ return IsLogging() ? OutputEntryInternal(entry) : OpStatus::OK; }
	OP_STATUS OutputEntry(const OpStringC8& entry, const OpStringC8 & param)
		{ return IsLogging() ? OutputEntryInternal(entry, param) : OpStatus::OK; }

	void IncreaseIndentation()	{ m_indentation++; }
	void DecreaseIndentation()  { OP_ASSERT(m_indentation > 0); if (m_indentation > 0) m_indentation--; }
	void ResetIndentation()		{ m_indentation = 0; }

private:
	OP_STATUS OutputEntryInternal(const OpStringC8& entry);
	OP_STATUS OutputEntryInternal(const OpStringC8& entry, const OpStringC8 & param);

	UINT32					m_indentation;
};

#endif // PARSER_LOGGER_H
