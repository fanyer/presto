/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

OP_STATUS SetLangString(Str::LocaleString str, OpString &target)
{
	if(str == 0)
	{
		target.Empty();
		return OpStatus::OK;
	}
	return g_languageManager->GetString(str, target);
}

#endif
