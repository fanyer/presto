/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Tord Akerbæk
*/

#include "core/pch.h"

#ifdef PREFS_DOWNLOAD
#include "modules/prefsloader/ploader.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/hardcore/mem/mem_man.h"

void PLoader::Commit()
{
	if (m_has_changes)
	{
		TRAPD(rc, g_prefsManager->CommitL());
		if (OpStatus::IsError(rc))
		{
			if (OpStatus::IsRaisable(rc))
			{
				g_memory_manager->RaiseCondition(rc);
			}
		}
		else
		{
			m_has_changes = FALSE;
		}
	}
}

#endif
