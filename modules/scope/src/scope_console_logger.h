/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_CONSOLE_LOGGER_H
#define SCOPE_CONSOLE_LOGGER_H

#ifdef SCOPE_CONSOLE_LOGGER

#include "modules/scope/src/scope_service.h"
#include "modules/console/opconsoleengine.h"

#include "modules/scope/src/generated/g_scope_console_logger_interface.h"

class OpScopeWindowManager;

class OpScopeConsoleLogger
	: public OpScopeConsoleLogger_SI
	, protected OpConsoleListener
{
public:
	OpScopeConsoleLogger();
	virtual ~OpScopeConsoleLogger();

	virtual OP_STATUS Construct();

	virtual OP_STATUS OnPostInitialization();

protected:
	// From OpConsoleListener
	virtual OP_STATUS NewConsoleMessage(unsigned int id, const OpConsoleEngine::Message *message);

private:
	// Request/Response functions
	virtual OP_STATUS DoClear();
	virtual OP_STATUS DoListMessages(ConsoleMessageList &out);

	BOOL AcceptWindowID(unsigned window_id);
	OP_STATUS SetConsoleMessage(const OpConsoleEngine::Message *msg, ConsoleMessage &out);

	OpScopeWindowManager *window_manager;
};

#endif // SCOPE_CONSOLE_LOGGER

#endif // SCOPE_CONSOLE_LOGGER_H
