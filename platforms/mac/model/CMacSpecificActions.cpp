/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#include "core/pch.h"

#include "platforms/mac/util/macutils.h"
#include "platforms/mac/pi/MacOpPrinterController.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/model/CMacSpecificActions.h"

#include "modules/display/VisDevListeners.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/dochand/win.h"
#include "modules/doc/frm_doc.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"

#include <PMErrors.h>

/*static*/
BOOL CMacSpecificActions::Handle(OpInputAction* aAction, Window* aWindow)
{
	if (aAction == NULL || aWindow == NULL)
		return FALSE;

	switch (aAction->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = aAction->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_CUT:
				child_action->SetEnabled(FALSE);
				break;
			case OpInputAction::ACTION_COPY:
			case OpInputAction::ACTION_COPY_TO_NOTE:
			{
				INT32 sel_text_len = 0;
				if (aWindow && aWindow->DocManager() && aWindow->DocManager()->GetCurrentDoc())
				{
					sel_text_len = aWindow->DocManager()->GetCurrentDoc()->GetSelectedTextLen();
				}
				child_action->SetEnabled((sel_text_len > 0) ? TRUE : FALSE);

			}
			break;

			case OpInputAction::ACTION_PASTE_MOUSE_SELECTION:
			case OpInputAction::ACTION_PASTE:
			case OpInputAction::ACTION_PASTE_TO_NOTE:
			case OpInputAction::ACTION_PASTE_AND_GO:
			{
				child_action->SetEnabled(g_desktop_clipboard_manager->HasText());
			}
			break;

			case OpInputAction::ACTION_SHOW_PRINT_SETUP:
				child_action->SetEnabled(FALSE);
				break;
			default:
				return FALSE;
			};
		}
		break;

		default:
			return FALSE;
	};

	return TRUE;
}

/*static*/
BOOL CMacSpecificActions::HandlePrint(OpInputAction* aAction)
{
	if (aAction == NULL)
		return FALSE;

	switch (aAction->GetAction())
	{
	}
	return FALSE;
}
