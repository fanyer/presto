/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/dochand/dochand_module.h"
#include "modules/dochand/winman.h"
#include "modules/url/url_lop_api.h" // OperaURL_Generator, so that we can delete them

DochandModule::DochandModule() :
	window_manager(NULL)
#ifdef WEBSERVER_SUPPORT
	, unitewarningpage_generator(NULL)
#endif // WEBSERVER_SUPPORT
{
}

void
DochandModule::InitL(const OperaInitInfo& info)
{
	window_manager = OP_NEW_L(WindowManager, ());
	window_manager->ConstructL();
}

void
DochandModule::Destroy()
{
#ifdef WEBSERVER_SUPPORT
	OP_DELETE(unitewarningpage_generator);
	unitewarningpage_generator = NULL;
#endif // WEBSERVER_SUPPORT
	if (window_manager)
	{
		window_manager->Clear();
		WindowManager* wmSave = window_manager;
		window_manager = NULL;
		OP_DELETE(wmSave);
	}

	if (imgManager)
		// Make sure all UrlImageContentProviders are destroyed by clearing the image cache.
		imgManager->SetCacheSize(0, IMAGE_CACHE_POLICY_STRICT);
}
