/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef DEBUG_LOGGER_H
#define DEBUG_LOGGER_H

#include "platforms/posix/posix_logger.h"

class DebugLogger : public PosixLogger
{
public:
	/** Constructor
	  * @param ear Where to log stuff. Will take ownership. Can be NULL (for no logging)
	  */
	explicit DebugLogger(PosixLogListener *ear) : PosixLogger(PosixLogger::SYSDBG), m_ear(ear) { g_posix_log_boss->Set(SYSDBG, ear); }

	virtual ~DebugLogger() { g_posix_log_boss->Set(SYSDBG, NULL); OP_DELETE(m_ear); }

private:
	PosixLogListener* m_ear;
};

#endif // DEBUG_LOGGER_H
