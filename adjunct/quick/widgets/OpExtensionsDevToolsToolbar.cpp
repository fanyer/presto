/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpExtensionsDevToolsToolbar.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"

OpExtensionsDevToolsToolbar::OpExtensionsDevToolsToolbar()
{
	SetListener(this);
	SetWrapping(OpBar::WRAPPING_OFF);
	SetShrinkToFit(TRUE);
	//SetFixedHeight(FIXED_HEIGHT_BUTTON_AND_EDIT);
	SetStandardToolbar(FALSE);
	SetButtonType(OpButton::TYPE_TOOLBAR);
	SetName("Extensions Developer Tools Toolbar");
	GetBorderSkin()->SetImage("Viewbar skin");
}

OpExtensionsDevToolsToolbar::~OpExtensionsDevToolsToolbar()
{

}

void OpExtensionsDevToolsToolbar::ToggleVisibility()
{
	if (GetAlignment() == ALIGNMENT_OFF)
	{
		Show();
	}
	else
	{
		Hide();
	}

}


void OpExtensionsDevToolsToolbar::Show()
{

	SetAlignmentAnimated(OpBar::ALIGNMENT_OLD_VISIBLE);
}

void OpExtensionsDevToolsToolbar::Hide(BOOL focus_page)
{
	SetAlignmentAnimated(OpBar::ALIGNMENT_OFF);
}

void OpExtensionsDevToolsToolbar::OnAlignmentChanged()
{
	if (GetAlignment() == ALIGNMENT_OFF)
	{
			
	}
}

void OpExtensionsDevToolsToolbar::OnReadContent(PrefsSection *section)
{
	OpToolbar::OnReadContent(section);

	// We want some buttons to actually look like buttons, instead of the toolbar button style,
	// so find them and force style.

	OpButton *button;
	OpInputAction action_close(OpInputAction::ACTION_CANCEL);
	
	if (button = (OpButton*) GetWidgetByTypeAndIdAndAction(OpTypedObject::WIDGET_TYPE_BUTTON, 0, &action_close, FALSE))
	{
			button->SetButtonStyle(OpButton::STYLE_IMAGE);
			button->SetFixedTypeAndStyle(TRUE);
	}
}

BOOL OpExtensionsDevToolsToolbar::OnInputAction(OpInputAction* action)
{
	
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CANCEL:
			Hide();
			return TRUE;
	};
	
	if (!g_desktop_extensions_manager->HandleAction(action))
		return OpToolbar::OnInputAction(action);
	else 
		return TRUE;

}