/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/sync/view/SyncPasswordDeletionWarningDialog.h"
#include "adjunct/quick/sync/view/SyncConstants.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"

#include "modules/locale/locale-enum.h"
#include "modules/widgets/OpButton.h"

BOOL SyncPasswordDeletionWarningDialog::OnInputAction(OpInputAction* action)
{
	return Dialog::OnInputAction(action);
}

void SyncPasswordDeletionWarningDialog::OnInit()
{
	OpIcon* warning_icon = static_cast<OpIcon*>(GetWidgetByName(SyncConstant::WARNING_ICON));
	OP_ASSERT(warning_icon);
	if (warning_icon)
	{
		warning_icon->SetImage("Dialog Warning");
	}

	OpRadioButton* only_on_this_radio =
			static_cast<OpRadioButton*>(GetWidgetByName(SyncConstant::ONLY_ON_THIS_RADIO));
	OP_ASSERT(only_on_this_radio);
	if (only_on_this_radio)
	{
		only_on_this_radio->SetValue(1);
	}
}

UINT32 SyncPasswordDeletionWarningDialog::OnOk()
{
	if (GetWidgetValue(SyncConstant::ONLY_ON_THIS_RADIO))
		return DELETE_ONLY_ON_THIS_COMPUTER;
	else /* if (GetWidgetValue(ON_ALL_RADIO)) */
		return DELETE_ON_ALL_COMPUTERS;
}

Str::LocaleString SyncPasswordDeletionWarningDialog::GetOkTextID()
{
	return Str::D_WM_DELETE_PASSWORDS;
}
