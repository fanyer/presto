/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpWidget.h"
#include "TipsNotificationBar.h"

TipsNotificationBar::TipsNotificationBar()
{
	SetWrapping(OpBar::WRAPPING_OFF);
	SetStandardToolbar(FALSE);
	GetBorderSkin()->SetImage("Tips Toolbar Skin", "Toolbar Skin");
	SetName("Tips Toolbar");
	SetAlignment(OpBar::ALIGNMENT_OFF);
}

TipsNotificationBar::~TipsNotificationBar()
{

}

OP_STATUS
TipsNotificationBar::Construct(TipsNotificationBar** obj, OpWidget *parent)
{
	*obj = OP_NEW(TipsNotificationBar, ());
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (parent)
		parent->AddChild(*obj);

	return OpStatus::OK;
}

OP_STATUS
TipsNotificationBar::SetTips(const OpString &tip)
{
	if (tip.IsEmpty())
		return OpStatus::ERR;

	OpWidget *wgt = GetWidgetByName("tbb_tips");
	if (!wgt)
		return OpStatus::ERR;

	return wgt->SetText(tip.CStr());
}

OP_STATUS
TipsNotificationBar::SetInfoImage(const char *image_name)
{
	if ( !(image_name && *image_name))
		return OpStatus::ERR;

	OpButton *wgt = static_cast<OpButton*>(GetWidgetByName("tbb_info_image"));
	if (!wgt)
		return OpStatus::ERR;

	wgt->SetButtonStyle(OpButton::STYLE_IMAGE);
	wgt->SetRestrictImageSize(FALSE);
	wgt->SetDead(TRUE);
	wgt->GetForegroundSkin()->SetImage(image_name);

	return OpStatus::OK;
}

BOOL
TipsNotificationBar::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CANCEL:
		{
			SetAlignmentAnimated(ALIGNMENT_OFF);
			break;
		}
		default:;
	}

	return OpToolbar::OnInputAction(action);
}