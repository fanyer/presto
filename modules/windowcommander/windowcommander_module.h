/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_WINDOWCOMMANDER_WINDOWCOMMANDER_MODULE_H
#define MODULES_WINDOWCOMMANDER_WINDOWCOMMANDER_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/windowcommander/wic_globals.h"

#if defined WIC_PROGRESS_FREQUENCY_LIMIT && WIC_PROGRESS_FREQUENCY_LIMIT > 0
# define WIC_LIMIT_PROGRESS_FREQUENCY
# define WIC_PROGRESS_DELAY (1000 / WIC_PROGRESS_FREQUENCY_LIMIT)
#endif

// "Temporary" define that enables the new asynch context menu system
// along with lots of small and major API changes that were needed.
// It will be enabled automatically as soon as all modules are
// released (windowcommander, display, widgets, dom, doc, forms, logdoc, prefs
#define NEW_CONTEXT_MENU_SYSTEM

/* When this define is set, core supports the concept of the rendering/visual/layout viewports.
   When writing code, please just assume that this is defined. There is no need to check for it anymore. */

#define VIEWPORTS_SUPPORT

class OpWindowCommanderManager;
class OpTransferManager;
class OpCookieManager;
class OpUrlFilterApi;

class WindowcommanderModule : public OperaModule
{
public:
	WindowcommanderModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	OpWindowCommanderManager* windowcommander_manager;
#ifdef WINDOW_COMMANDER_TRANSFER
	OpTransferManager* transfer_manager;
#endif // WINDOW_COMMANDER_TRANSFER
	OpCookieManager* cookie_API;
#ifdef URL_FILTER
	OpUrlFilterApi* url_filter_API;
#endif // URL_FILTER

#ifndef HAS_COMPLEX_GLOBALS
	const char* wic_charsets[wic_charsets_SIZE];
#endif // HAS_COMPLEX_GLOBALS

#ifdef RESERVED_REGIONS
	DOM_EventType reserved_region_types[5]; /* ARRAY OK 2010-08-3 terjes */
#endif // RESERVED_REGIONS
};

#define g_windowCommanderManager g_opera->windowcommander_module.windowcommander_manager

#ifndef HAS_COMPLEX_GLOBALS
#define wic_charsets g_opera->windowcommander_module.wic_charsets
#endif // HAS_COMPLEX_GLOBALS

#ifdef WINDOW_COMMANDER_TRANSFER
#define g_transferManager g_opera->windowcommander_module.transfer_manager
#endif // WINDOW_COMMANDER_TRANSFER

#define g_cookie_API g_opera->windowcommander_module.cookie_API

#ifdef RESERVED_REGIONS
# define g_reserved_region_types g_opera->windowcommander_module.reserved_region_types
#endif // RESERVED_REGIONS

#define WINDOWCOMMANDER_MODULE_REQUIRED

#endif // !MODULES_WINDOWCOMMANDER_WINDOWCOMMANDER_MODULE_H
