/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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
#include "modules/url/url2.h"

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT

#include "modules/url/url_loading.h"

IAmLoadingThisURL::~IAmLoadingThisURL()
{
	UnsetURL();
}

OP_STATUS IAmLoadingThisURL::SetURL(URL &p_url)
{
	if(p_url == url)
		return OpStatus::OK; // No action needed

	UnsetURL();
	if(p_url.IsValid())
	{
		RETURN_IF_ERROR(p_url.IncLoading());
		url = p_url;
	}
	return OpStatus::OK;
}

void IAmLoadingThisURL::UnsetURL()
{
	if(url.IsValid())
	{
		url.DecLoading();
		url = URL();
	}
}

#endif
