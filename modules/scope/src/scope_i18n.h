/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_I18N_H
#define SCOPE_I18N_H

#ifdef SCOPE_I18N

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_i18n_interface.h"

class OpScopeI18n
	: public OpScopeI18n_SI
{
public:
	OpScopeI18n();
	virtual ~OpScopeI18n();

	// Request/Response functions
	virtual OP_STATUS DoGetLanguage(Language &out);
	virtual OP_STATUS DoGetString(const GetStringArg &in, String &out);

private:
};

#endif // SCOPE_I18N
#endif // SCOPE_I18N_H
