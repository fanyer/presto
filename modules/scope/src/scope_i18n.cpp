/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_I18N

#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_i18n.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

/* OpScopeDocumentManager */

OpScopeI18n::OpScopeI18n()
	: OpScopeI18n_SI()
{
}

/* virtual */
OpScopeI18n::~OpScopeI18n()
{
}

OP_STATUS
OpScopeI18n::DoGetLanguage(Language &out)
{
	return out.SetLanguage(g_languageManager->GetLanguage());
}

OP_STATUS
OpScopeI18n::DoGetString(const GetStringArg &in, String &out)
{
#ifdef LOCALE_SET_FROM_STRING
	Str::LocaleString lstr(in.GetStringID().CStr());
	return g_languageManager->GetString(lstr, out.GetStrRef());
#else // LOCALE_SET_FROM_STRING
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // LOCALE_SET_FROM_STRING
}

#endif // SCOPE_I18N
