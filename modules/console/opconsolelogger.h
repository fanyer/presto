/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined OPCONSOLELOGGER_H && defined OPERA_CONSOLE_LOGFILE
#define OPCONSOLELOGGER_H

#include "modules/console/opconsoleengine.h"

class OpFileDescriptor;
class OpConsoleFilter;

/**
 * Log error messages to file. This class will receive the messages from
 * OpConsoleEngine and log them to a file if they fulfill the criteria
 * that it has determined.
 */
class OpConsoleLogger : protected OpConsoleListener
{
public:
	/**
	 * First-phase constructor.
	 */
	OpConsoleLogger()
		: m_file(NULL), m_filter(NULL)
	{}

	/**
	 * Second-phase constructor. Non-leaving version, which assumes ownership
	 * of the arguments passed to it. You must only call one of the
	 * second-phase constructors.
	 *
	 * @param logfile File to log to. OpConsoleLogger assumes ownership.
	 * @param filter Filter for which to log. OpConsoleLogger assumes ownership.
	 */
	void Construct(OpFileDescriptor *logfile, OpConsoleFilter *filter);

	/**
	 * Second-phase constructor. Leaving version, which creates copies of the
	 * arguments passed to it. You must only call one of the
	 * second-phase constructors.
	 *
	 * @param logfile File to log to.
	 * @param filter Filter for which to log.
	 */
	void ConstructL(const OpFileDescriptor *logfile, const OpConsoleFilter *filter);

	virtual ~OpConsoleLogger();

	/**
	 * Get the associated log file.
	 */
	const OpFileDescriptor *GetFile()
	{ return m_file; }

	/**
	 * Get the associated filter.
	 */
	const OpConsoleFilter *GetFilter()
	{ return m_filter; }

protected:
	// Inherited interfaces
	virtual OP_STATUS NewConsoleMessage(unsigned int id, const OpConsoleEngine::Message *message);

	/**
	 * Log the message to a file. This is called from NewConsoleMessage()
	 * when it has determined that it wants to handle the message.
	 *
	 * @param id Number of the posted message.
	 * @param message Pointer to the actual message. May not be NULL.
	 */
	virtual OP_STATUS LogMessage(int id, const OpConsoleEngine::Message *message);

	// Data
	/** File to log to. */
	OpFileDescriptor *m_file;

	/** The setting that defines what type of messages to forward to
	  * the OpConsoleViewHandler object.
	  */
	OpConsoleFilter *m_filter;
};

#endif // OPCONSOLELOGGER_H
