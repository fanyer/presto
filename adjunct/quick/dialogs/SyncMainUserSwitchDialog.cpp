/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Adam Minchinton and Karianne Ekern
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/dialogs/SyncMainUserSwitchDialog.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

SyncMainUserSwitchDialog::SyncMainUserSwitchDialog(BOOL *ok)
{
	m_ok = ok;
	// Force default it to FALSE;
	*m_ok = FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

SyncMainUserSwitchDialog::~SyncMainUserSwitchDialog()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void SyncMainUserSwitchDialog::OnInit()
{
	// Make the text a label
	OpLabel* label_sync = (OpLabel*)GetWidgetByName("label_text");
	if (label_sync) 
		label_sync->SetWrap(TRUE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL SyncMainUserSwitchDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_OK:
			case OpInputAction::ACTION_CANCEL:
				{
					// I'm not really sure why this is necessary, but I need to explicitly set 
					// the action state to TRUE to make sure the OK/Cancel buttons are always enabled
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
			}
			break;
		}
	}
	return Dialog::OnInputAction(action);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

UINT32 SyncMainUserSwitchDialog::OnOk()
{
	// Set OK to true
	*m_ok = TRUE;

	return 0;
}


#endif // SUPPORT_DATA_SYNC


