/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpExpand.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"

/***********************************************************************************
**
**	OpExpand
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpExpand)

////////////////////////////////////////////////////////////////////////////////////////

OpExpand::OpExpand()
{
	SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);

	// Set the control so it can be tabbed to
	BOOL button_can_be_tab_stop = TRUE;
	button_can_be_tab_stop = g_op_ui_info->IsFullKeyboardAccessActive();
	SetTabStop(button_can_be_tab_stop);

	// Set the Skin for the Control and it's image (i.e. disclosure triangle)
	GetBorderSkin()->SetImage("Expand Skin");
	GetForegroundSkin()->SetImage("Disclosure Triangle Skin");
	
	// Make the font the same as any dialog
	SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
	
	// Make sure the main text will show ellipsis if it's too long!
	SetEllipsis(g_pcui->GetIntegerPref(PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpExpand::SetShowHoverEffect(BOOL show)
{
	if (show)
		GetBorderSkin()->SetImage("Expand Hover Skin");
	else
		GetBorderSkin()->SetImage("Expand Skin");
}

////////////////////////////////////////////////////////////////////////////////////////

BOOL OpExpand::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OPEN_EXPAND:
		{
			if (!GetValue())
			{
				Click();
				return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_CLOSE_EXPAND:
		{
			if (GetValue())
			{
				Click();
				return TRUE;
			}
			break;
		}
	}
	
	return OpButton::OnInputAction(action);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpExpand::OnLayout()
{
	OpRect button_rect, text_rect, icon_rect;
	INT32  right_offset = 0, left, top, right, bottom, spacing;
	
	button_rect = GetBounds();

	// Have the same spacing between the icon and text as between the disclosure triangle and text
	GetSpacing(&spacing);

	// Adjust the bounds based on the padding
	GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
	button_rect.x += left;
	button_rect.y = top;
	button_rect.height -= (top + bottom);
	button_rect.width -= (left + right);

	if (right_offset)
	{
		// Add the same spacing as above for less clutter
		right_offset += spacing;
		
		SetRightOffset(right_offset);
	}

	OpButton::OnLayout();
}
