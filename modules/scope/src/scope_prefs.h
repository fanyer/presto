/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_PREFS_H
#define SCOPE_PREFS_H

#ifdef SCOPE_PREFS

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_prefs_interface.h"

class Header_List;

class OpScopePrefs
	: public OpScopePrefs_SI
{
public:
	OpScopePrefs();
	virtual ~OpScopePrefs();

	// Request/Response functions
	virtual OP_STATUS DoGetPref(const GetPrefArg &in, PrefValue &out);
	virtual OP_STATUS DoListPrefs(const ListPrefsArg &in, PrefList &out);
	virtual OP_STATUS DoSetPref(const SetPrefArg &in);

private:

	OP_STATUS SetPref(const prefssetting &pref, Pref &out);
};

#endif // SCOPE_PREFS
#endif // SCOPE_PREFS_H
