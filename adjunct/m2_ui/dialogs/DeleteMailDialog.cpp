/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "DeleteMailDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"

#include "adjunct/m2/src/include/defs.h"

#include <limits.h>

OP_STATUS DeleteMailDialog::Init(UINT32 index_id, DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);

	SetWidgetValue("Delete_message_radio", TRUE);
	SetWidgetEnabled("Remove_message_radio", FALSE);

	if (index_id >= IndexTypes::FIRST_FOLDER && index_id < IndexTypes::LAST_FOLDER)
	{
		SetWidgetEnabled("Remove_message_radio", TRUE);
	}

	return OpStatus::OK;
}


UINT32 DeleteMailDialog::OnOk()
{
	if (GetWidgetValue("Delete_message_radio"))
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_DELETE_MAIL, -1, NULL, GetParentDesktopWindow());
	}
	else
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_CUT, 0, NULL, GetParentDesktopWindow());
	}

	if (IsDoNotShowDialogAgainChecked())
	{
		TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::ShowDeleteMailDialog, FALSE));
	}

	return 0;
}

#endif // M2_SUPPORT
