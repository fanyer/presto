/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/widgetruntime/hotlist/GadgetsHotlistController.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModelItem.h"
#include "modules/inputmanager/inputaction.h"


GadgetsHotlistController::GadgetsHotlistController(OpTreeView& item_view)
	: m_item_view(&item_view)
{
}


BOOL GadgetsHotlistController::HandleAction(OpInputAction& action)
{
	BOOL handled = TRUE;

	GadgetsTreeModelItem* item = NULL;
	if (NULL != m_item_view->GetSelectedItem())
	{
		item = static_cast<GadgetsTreeModelItem*>(
				m_item_view->GetSelectedItem());
	}

	switch (action.GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction& child_action(*action.GetChildAction());
			switch (child_action.GetAction())
			{
				case OpInputAction::ACTION_OPEN_WIDGET:
					child_action.SetEnabled(NULL != item);
					break;
				case OpInputAction::ACTION_DELETE:
					child_action.SetEnabled(NULL != item && item->CanBeUninstalled());
					break;
			}
			break;
		}

		case OpInputAction::ACTION_OPEN_WIDGET:
			if (NULL != item)
			{
				item->OnOpen();
			}
			break;

		case OpInputAction::ACTION_DELETE:
			if (NULL != item)
			{
				item->OnDelete();
			}
			break;

		default:
			handled = FALSE;
	}

	return handled;
}

#endif // WIDGET_RUNTIME_SUPPORT
