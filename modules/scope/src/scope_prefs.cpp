/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_PREFS

#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_prefs.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

/* OpScopePrefs */

OpScopePrefs::OpScopePrefs()
	: OpScopePrefs_SI()
{
}

/* virtual */
OpScopePrefs::~OpScopePrefs()
{
}

OP_STATUS
OpScopePrefs::DoGetPref(const GetPrefArg &in, PrefValue &out)
{
	// Convert to OpString8:
	OpString8 section, key;
	RETURN_IF_ERROR(section.Set(in.GetSection()));
	RETURN_IF_ERROR(key.Set(in.GetKey()));

	BOOL found = FALSE;
	TRAPD(err, found = g_prefsManager->GetPreferenceL(section.CStr(), key.CStr(), out.GetValueRef(), in.GetMode() == GetPrefArg::MODE_DEFAULT));
	
	if (OpStatus::IsError(err) || !found)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Could find pref, or pref not readable"));
	
	return OpStatus::OK;
}

OP_STATUS
OpScopePrefs::DoListPrefs(const ListPrefsArg &in, PrefList &out)
{
	OpAutoArray<prefssetting> prefs;
	unsigned length = 0;

	TRAPD(err, prefs.reset(g_prefsManager->GetPreferencesL(in.GetSort(), length)));
	RETURN_OOM_IF_NULL(prefs.get());

	for (unsigned i = 0; i < length; ++i)
	{
		if (!in.HasSection() || uni_str_eq(prefs[i].section, in.GetSection().CStr()))
		{
			Pref *p_out = out.GetPrefListRef().AddNew();
			RETURN_OOM_IF_NULL(p_out);
			RETURN_IF_ERROR(SetPref(prefs[i], *p_out));
		}
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopePrefs::DoSetPref(const SetPrefArg &in)
{
	// Convert to OpString8:
	OpString8 section, key;
	RETURN_IF_ERROR(section.Set(in.GetSection()));
	RETURN_IF_ERROR(key.Set(in.GetKey()));

	BOOL found = FALSE;
	TRAPD(err, found = g_prefsManager->WritePreferenceL(section.CStr(), key.CStr(), in.GetValue()));

	if (OpStatus::IsError(err) || !found)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Could find pref, or pref not writable"));

	return OpStatus::OK;
}

OP_STATUS
OpScopePrefs::SetPref(const prefssetting &pref, Pref &out)
{
	switch(pref.type)
	{
	case prefssetting::string:
		out.SetType(Pref::TYPE_STRING);
		break;
	case prefssetting::integer:
		out.SetType(Pref::TYPE_INTEGER);
		break;
	case prefssetting::boolean:
		out.SetType(Pref::TYPE_BOOLEAN);
		break;
	case prefssetting::file:
		out.SetType(Pref::TYPE_FILE);
		break;
	case prefssetting::requiredfile:
		out.SetType(Pref::TYPE_REQUIRED_FILE);
		break;
	case prefssetting::directory:
		out.SetType(Pref::TYPE_DIRECTORY);
		break;
	case prefssetting::color:
		out.SetType(Pref::TYPE_COLOR);
		break;
	default:
		OP_ASSERT(!"Please add a handler for the unknown case");
		out.SetType(Pref::TYPE_STRING);
		break;
	}

	RETURN_IF_ERROR(out.SetSection(pref.section));
	RETURN_IF_ERROR(out.SetKey(pref.key));
	RETURN_IF_ERROR(out.GetValueRef().Set(pref.value.CStr()));

#ifdef PREFSFILE_CASCADE
	out.SetEnabled(pref.enabled);
#endif // PREFSFILE_CASCADE

	return OpStatus::OK;
}

#endif // SCOPE_PREFS
