/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if !defined NO_URL_OPERA && defined ABOUT_OPERA_CACHE
#include "modules/about/operacache.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/url/url_man.h"
#include "modules/cache/cache_man.h"
#include "modules/locale/locale-enum.h"

OP_STATUS OperaCache::GenerateData()
{
	// FIXME: Move generation of actual document from cache module
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::SI_CACHE_TITLE_TEXT, PrefsCollectionFiles::StyleCacheFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::SI_CACHE_TITLE_TEXT));
#endif
	RETURN_IF_ERROR(OpenBody(Str::SI_CACHE_TITLE_TEXT));
	RETURN_IF_ERROR(urlManager->GenerateCacheList(m_url));
	return CloseDocument();
}

#endif
