/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if !defined NO_URL_OPERA && defined _OPERA_DEBUG_DOC_
#include "modules/about/operadebugnet.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/url/url_man.h"
#include "modules/cache/cache_man.h"

OP_STATUS OperaDebugNet::GenerateData()
{
	/* This is an document used for internal debugging, and is not advertised
	 * in the UI. It is thus not localized. */
#ifdef _LOCALHOST_SUPPORT_
	OpString styleurl;
	TRAP_AND_RETURN(rc, g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleCacheFile, &styleurl));
	RETURN_IF_ERROR(OpenDocument(UNI_L("Debugging Information"), styleurl.CStr()));
#else
	RETURN_IF_ERROR(OpenDocument(UNI_L("Debugging Information")));
#endif
	RETURN_IF_ERROR(OpenBody());
	RETURN_IF_ERROR(urlManager->GenerateDebugPage(m_url));
	return CloseDocument();
}

#endif
