/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */


#include "core/pch.h"

#include "modules/url/url_docload.h"
#include "modules/locale/locale-enum.h"

BOOL URL_DocumentHandler::OnURLLoadingStarted(URL &url)
{
	return TRUE;
}


BOOL URL_DocumentHandler::OnURLRedirected(URL &url, URL &redirected_to)
{
	return TRUE;
}

BOOL URL_DocumentHandler::OnURLHeaderLoaded(URL &url)
{
	return TRUE;
}

BOOL URL_DocumentHandler::OnURLDataLoaded(URL &url, BOOL finished, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	using_default_data_loaded = TRUE;
	return (finished ? FALSE : TRUE);
}

BOOL URL_DocumentHandler::OnURLRestartSuggested(URL &url, BOOL &restart_processing, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	restart_processing = FALSE;
	return TRUE;
}

BOOL URL_DocumentHandler::OnURLNewBodyPart(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	if(using_default_data_loaded)
	{
		// Fake retrieval to make sure we progress to the last item
		URL_DataDescriptor *desc = url.GetDescriptor(NULL, URL::KFollowRedirect);
		OP_DELETE(desc);
	}
	return TRUE;
}

void URL_DocumentHandler::OnURLLoadingFailed(URL &url, Str::LocaleString error, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
}

void URL_DocumentHandler::OnURLLoadingStopped(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
}
