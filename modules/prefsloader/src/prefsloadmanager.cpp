/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Tord Akerbæk
*/

#include "core/pch.h"

#ifdef PREFS_DOWNLOAD

#include "modules/prefsloader/prefsloadmanager.h"
#include "modules/prefsloader/prefsloader.h"
#include "modules/prefsloader/prefsfileloader.h"

OP_STATUS PrefsLoadManager::InitLoader(const URL url, OpEndChecker * end)
{
	OP_STATUS rc = OpStatus::OK;
	
	PrefsLoader * loader = OP_NEW(PrefsLoader, (end));
	if (!loader)
	{
		OP_DELETE(end);
		rc = OpStatus::ERR_NO_MEMORY;
	}
	else if (OpStatus::IsSuccess(rc = loader->Construct(url)) &&
			 OpStatus::IsSuccess(rc = loader->StartLoading()))
	{
		Cleanup(FALSE);
		loader->Into(&m_activeLoaders);
	}
	else
	{
		OP_DELETE(loader);
	}

	return rc;
}

OP_STATUS PrefsLoadManager::InitLoader(const OpStringC &host, OpEndChecker * end)
{
	OP_STATUS rc = OpStatus::OK;

	PrefsLoader * loader = OP_NEW(PrefsLoader, (end));
	if (!loader)
	{
		OP_DELETE(end);
		rc = OpStatus::ERR_NO_MEMORY;
	}
	else if (OpStatus::IsSuccess(rc = loader->Construct(host)) &&
			 OpStatus::IsSuccess(rc = loader->StartLoading()))
	{
		Cleanup(FALSE);
		loader->Into(&m_activeLoaders);
	}
	else
	{
		OP_DELETE(loader);
	}
	
	return rc;
}

#ifdef PREFS_FILE_DOWNLOAD
OP_STATUS PrefsLoadManager::InitFileLoader(const OpStringC &host, const OpStringC8 &section, const OpStringC8 &pref, const OpStringC &from, const OpStringC &to)
{
	OP_STATUS rc;
	PrefsFileLoader * loader = OP_NEW(PrefsFileLoader, ());
	if(!loader)
	{
		rc = OpStatus::ERR_NO_MEMORY;
	}
	else if(OpStatus::IsSuccess(rc = loader->Construct(host, section, pref, from, to)) &&
			OpStatus::IsSuccess(rc = loader->StartLoading()))
	{
		loader->Into(&m_activeLoaders);
	}
	else
	{
		OP_DELETE(loader);
	}
	return rc;
}
#endif // PREFS_FILE_DOWNLOAD

void PrefsLoadManager::Cleanup(BOOL force)
{
	PLoader *q;
	Link *p = m_activeLoaders.First();
	while(p)
	{
		q = static_cast<PLoader *>(p);
		p = p->Suc();
		if(q->IsDead() || force)
		{
			q->Out();
			OP_DELETE(q);
		}
	}
}

#endif // PREFS_DOWNLOAD
