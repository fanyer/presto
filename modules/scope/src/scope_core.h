/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_CORE_H
#define SCOPE_CORE_H

#ifdef SCOPE_CORE

#include "modules/scope/src/scope_core.h"
#include "modules/scope/src/generated/g_scope_core_interface.h"
#include "modules/idle/idle_detector.h"

class OpScopeCore
	: public OpScopeCore_SI
	, public OpActivityListener
{
public:
	OpScopeCore();
	virtual ~OpScopeCore();

	// OpScopeService
	virtual OP_STATUS OnServiceEnabled();
	virtual OP_STATUS OnServiceDisabled();

	// OpActivityListener.
	virtual OP_STATUS OnActivityStateChanged(OpActivityState state);

	// Request/Response functions
	virtual OP_STATUS DoGetBrowserInformation(BrowserInformation &out);
	virtual OP_STATUS DoClearPrivateData(const ClearPrivateDataArg &in);

private:
};

#endif // SCOPE_CORE
#endif // SCOPE_CORE_H
