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

URL_InUse::~URL_InUse()
{
	UnsetURL();
}

/*
void URL_InUse::SetURL(URL_InUse &old)
{
	if(&old == this)
		return; // No action needed

	UnsetURL();
	if(old.url.IsValid())
	{
		url = old.url;
		url.IncUsed();
		TRACER_INUSE_SETTRACKER(url);
	}
}
*/

void URL_InUse::SetURL(URL &p_url)
{
	if(p_url == url)
		return; // No action needed

	UnsetURL();
	if(p_url.IsValid())
	{
		url = p_url;
		url.IncUsed();
		TRACER_INUSE_SETTRACKER(url);
	}
}

void URL_InUse::SetURL(const URL &p_url)
{
	if(p_url == url)
		return; // No action needed

	UnsetURL();
	if(p_url.IsValid())
	{
		url = p_url;
		url.IncUsed();
		TRACER_INUSE_SETTRACKER(url);
	}
}

void URL_InUse::UnsetURL()
{
	if(url.IsValid())
	{
		url.DecUsed();
		url = URL();
	}
	TRACER_RELEASE();
}
