/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/windowcommander/windowcommander_module.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/windowcommander/OpTransferManager.h"
#include "modules/windowcommander/OpCookieManager.h"
#include "modules/windowcommander/src/UrlFilterApi.h"

WindowcommanderModule::WindowcommanderModule() :
	windowcommander_manager(NULL)
#ifdef WINDOW_COMMANDER_TRANSFER
	, transfer_manager(NULL)
#endif // WINDOW_COMMANDER_TRANSFER
	, cookie_API(NULL)
#ifdef URL_FILTER
	, url_filter_API(NULL)
#endif // URL_FILTER
{
}

void WindowcommanderModule::InitL(const OperaInitInfo& info)
{
	windowcommander_manager = OpWindowCommanderManager::CreateL();

#ifdef WINDOW_COMMANDER_TRANSFER
	transfer_manager = OpTransferManager::CreateL();
#endif // WINDOW_COMMANDER_TRANSFER

	cookie_API = OpCookieManager::CreateL();
#ifdef URL_FILTER
	url_filter_API = OP_NEW_L(UrlFilterApi,());
#endif // URL_FILTER
#ifdef RESERVED_REGIONS
	int i = 0;
#ifdef WIC_RESERVE_MOUSE_REGIONS
	reserved_region_types[i++] = ONMOUSEMOVE;
#endif // WIC_RESERVE_MOUSE_REGIONS
#ifdef WIC_RESERVE_TOUCH_REGIONS
	reserved_region_types[i++] = TOUCHSTART;
	reserved_region_types[i++] = TOUCHMOVE;
	reserved_region_types[i++] = TOUCHEND;
#endif // WIC_RESERVE_TOUCH_REGIONS
	reserved_region_types[i] = DOM_EVENT_NONE;
#endif // RESERVED_REGIONS
}

void WindowcommanderModule::Destroy()
{
	OP_DELETE(windowcommander_manager);
	windowcommander_manager = NULL;

#ifdef WINDOW_COMMANDER_TRANSFER
	OP_DELETE(transfer_manager);
	transfer_manager = NULL;
#endif // WINDOW_COMMANDER_TRANSFER

	OP_DELETE(cookie_API);
	cookie_API = NULL;

#ifdef URL_FILTER
	OP_DELETE(url_filter_API);
	url_filter_API = NULL;	
#endif // URL_FILTER
}
