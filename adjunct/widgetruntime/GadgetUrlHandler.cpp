/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "GadgetUrlHandler.h"

#include "modules/url/url_man.h"
#include "modules/util/opfile/opfile.h"


namespace
{
	const uni_char* FILTER_INI_FILENAME = UNI_L("widget_urlfilter.ini");
}


OP_STATUS GadgetUrlHandler::Init()
{
	OpFile filter_ini_file;
	RETURN_IF_ERROR(
			filter_ini_file.Construct(FILTER_INI_FILENAME, OPFILE_HOME_FOLDER));
	
	BOOL ini_file_exists = FALSE;
	if (OpStatus::IsError(filter_ini_file.Exists(ini_file_exists)
			|| !ini_file_exists))
	{
		// TODO: debug around this code
		// dbg_printf("Filter INI file %s doesn't exist\n",
				filter_ini_file.GetFullPath();
		//		);
		// TODO:  Create default filter INI here.  Or?
	}

	OP_STATUS status = OpStatus::OK;
	TRAP(status,
			url_filter.InitL(OpString().SetL(filter_ini_file.GetFullPath())));
	return status;
}


BOOL GadgetUrlHandler::IsExternalUrl(const uni_char* url_str)
{
	URL url = urlManager->GetURL(url_str);

	BOOL is_internal = TRUE;
	OP_STATUS status = url_filter.CheckURL(&url, is_internal);
	if (OpStatus::IsError(status))
	{
		// dbg_printf("Error checking URL"); //TODO: debug around
		is_internal = FALSE;
	}

#ifdef _DEBUG
	OpString8 url8;
	url8.SetL(url_str);
	dbg_printf("%s: %s\n", url8.CStr(), is_internal ? "internal" : "external");
#endif // _DEBUG

	return !is_internal;
}


#endif  // WIDGET_RUNTIME_SUPPORT
