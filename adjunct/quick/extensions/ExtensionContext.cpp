/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/extensions/ExtensionContext.h"

#include "adjunct/quick/managers/DesktopExtensionsManager.h"

BOOL ExtensionContext::OnInputAction(OpInputAction* action)
{
	if (!g_desktop_extensions_manager)
		return FALSE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
			switch (action->GetChildAction()->GetAction())
			{
				case OpInputAction::ACTION_EXTENSION_ACTION:
				case OpInputAction::ACTION_SHOW_SPEEDDIAL_EXTENSION_OPTIONS:
				case OpInputAction::ACTION_DISABLE_SPEEDDIAL_EXTENSION:
					return g_desktop_extensions_manager->HandleAction(action);
			}
			break;

		case OpInputAction::ACTION_EXTENSION_ACTION:
		case OpInputAction::ACTION_SHOW_SPEEDDIAL_EXTENSION_OPTIONS:
		case OpInputAction::ACTION_DISABLE_SPEEDDIAL_EXTENSION:
			return g_desktop_extensions_manager->HandleAction(action);
	}

	return FALSE;
}
