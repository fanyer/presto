
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick/controller/factories/ButtonFactory.h"
#include "adjunct/quick/managers/OperaTurboManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick_toolkit/widgets/OpStateButton.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"

/***********************************************************************************
**  ButtonFactory::ConstructStateButtonByType
**  @param type
**  @param button
**  @return
************************************************************************************/
OP_STATUS ButtonFactory::ConstructStateButtonByType(const char* type, OpStateButton** button)
{
	OP_ASSERT(type);
	if (type == NULL)
		return OpStatus::ERR;

	*button = OP_NEW(OpStateButton, ());
	if (!*button)
		return OpStatus::ERR_NO_MEMORY;

#ifdef SUPPORT_DATA_SYNC
	if(op_stristr(type, "Sync") != 0 )
	{
		(*button)->SetModifier(g_sync_manager->GetWidgetStateModifier());
		return OpStatus::OK;
	}
#endif // SUPPORT_DATA_SYNC

#ifdef WEBSERVER_SUPPORT
	if(op_stristr(type, "WebServer") != 0 )
	{
		if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUnite))
		{
			(*button)->SetModifier(g_webserver_manager->GetWidgetStateModifier());
			return OpStatus::OK;
		}
	}
#endif // WEBSERVER_SUPPORT

	if(op_stristr(type, "Turbo") != 0 )
	{
		(*button)->SetModifier(g_opera_turbo_manager->GetWidgetStateModifier());
		return OpStatus::OK;
	}

	OP_DELETE(*button);
	return OpStatus::ERR;
}

