/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/excepts.h"

int OpLanguageManager::GetStringL(Str::LocaleString num, OpString &s)
{
	UniString us;
	LEAVE_IF_ERROR(GetString(num, us));

	s.ReserveL(us.Length() + 1);
	us.CopyInto(s.CStr(), us.Length());
	s[us.Length()] = 0;

	return static_cast<int>(us.Length());
}

OP_STATUS OpLanguageManager::GetString(Str::LocaleString num, OpString &s)
{
	UniString us;
	RETURN_IF_ERROR(GetString(num, us));

	s.Reserve(us.Length() + 1);
	if (s.Capacity() <= static_cast<int>(us.Length()))
		return OpStatus::ERR_NO_MEMORY;

	us.CopyInto(s.CStr(), us.Length());
	s[us.Length()] = 0;
	return OpStatus::OK;
}

#ifdef LOCALE_CONTEXTS
void OpLanguageManager::SetContext(URL_CONTEXT_ID)
{
	OP_ASSERT(!"Ignoring language context");
}
#endif
