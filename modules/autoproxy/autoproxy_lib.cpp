/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2005
 *
 * Initialization of precompiled JS libraries
 * Lars T Hansen
 */

#include "core/pch.h"

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION

#include "modules/autoproxy/autoproxy_lib.h"
#include "modules/autoproxy/autoproxy.h"

void
GetAPCLibrarySourceCode(int *library_size, const uni_char * const **library_strings)
{
	*library_size = g_proxycfg->proxy_conf_lib_length;
	*library_strings = g_proxycfg->proxy_conf_lib;
}

void
InitAPCLibrary(ES_Runtime *rt)
{
	/* nothing */
}

#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
