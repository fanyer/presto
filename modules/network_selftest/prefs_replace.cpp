/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#include "core/pch.h"

#ifdef SELFTEST

#include "modules/network_selftest/prefs_replace.h"

Selftest_IntegerPrefOverride::Selftest_IntegerPrefOverride()
		: pref((PrefsCollectionNetwork::integerpref) 0),
		prev_value(0),
		new_value_is_default(FALSE), new_value(0),
		configured(FALSE)
{
}

OP_STATUS Selftest_IntegerPrefOverride::Construct(PrefsCollectionNetwork::integerpref a_pref, int a_value)
{
	pref = a_pref;
	new_value = a_value;
	prev_value = g_pcnet->GetIntegerPref(pref);

	return Activate();
}

OP_STATUS Selftest_IntegerPrefOverride::Construct(PrefsCollectionNetwork::integerpref a_pref)
{
	pref = a_pref;
	new_value_is_default = TRUE;
	prev_value = g_pcnet->GetIntegerPref(pref);

	return Activate();
}

OP_STATUS Selftest_IntegerPrefOverride::SetPrefs(int a_value)
{
#ifdef PREFS_WRITE
	RETURN_IF_LEAVE(g_pcnet->WriteIntegerL(pref, a_value));
#endif

	return OpStatus::OK;
}

void Selftest_IntegerPrefOverride::ShutDown()
{
	if(!configured)
		return;

	OpStatus::Ignore(SetPrefs(prev_value));
	configured = TRUE;
}

OP_STATUS Selftest_IntegerPrefOverride::Activate()
{
	if(new_value_is_default)
	{
#ifdef PREFS_WRITE
		RETURN_IF_ERROR(g_pcnet->ResetIntegerL(pref));
#endif
		configured = TRUE;
	}
	else
	{
#ifdef PREFS_WRITE
		RETURN_IF_ERROR(SetPrefs(new_value));
		configured = TRUE;
#else
		// We can't write, but it might be okay if we are trying to set the value that it already is
		configured = new_value == prev_value;
#endif
	}

	return OpStatus::OK;
}

#endif // SELFTEST
