/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#ifndef _NETWORK_PREFS_OVERRIDE_H
#define _NETWORK_PREFS_OVERRIDE_H

#if defined(SELFTEST)

#include "modules/prefs/prefsmanager/collections/pc_network.h"

class Selftest_IntegerPrefOverride
{
private:

	PrefsCollectionNetwork::integerpref pref;

	int prev_value;

	BOOL new_value_is_default;
	int new_value;

	BOOL configured;

public:
	Selftest_IntegerPrefOverride();
	~Selftest_IntegerPrefOverride(){ShutDown();}

	OP_STATUS Construct(PrefsCollectionNetwork::integerpref a_pref, int a_value);
	OP_STATUS Construct(PrefsCollectionNetwork::integerpref a_pref);

	void ShutDown();
	OP_STATUS Activate();

	BOOL Active() const{return configured;}
protected:
	OP_STATUS SetPrefs(int a_value);
};


#endif // SELFTEST 
#endif // _NETWORK_PREFS_OVERRIDE_H
