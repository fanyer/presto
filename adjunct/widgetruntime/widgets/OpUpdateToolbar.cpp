/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGETS_UPDATE_SUPPORT

#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/widgetruntime/widgets/OpUpdateToolbar.h"

#include "modules/locale/oplanguagemanager.h"

OpUpdateToolbar::OpUpdateToolbar(WidgetUpdater& updater)
	: m_updater(&updater)
{
	SetWrapping(OpBar::WRAPPING_OFF);
	SetShrinkToFit(TRUE);
	SetStandardToolbar(FALSE);
	SetButtonType(OpButton::TYPE_TOOLBAR);
	SetName("Autoupdate Toolbar");
	GetBorderSkin()->SetImage("Viewbar skin");
}

void OpUpdateToolbar::Show()
{
	SetAlignmentAnimated(OpBar::ALIGNMENT_OLD_VISIBLE);
}

void OpUpdateToolbar::Hide(BOOL focus_page)
{
	SetAlignmentAnimated(OpBar::ALIGNMENT_OFF);
}


void OpUpdateToolbar::OnReadContent(PrefsSection *section)
{
	OpToolbar::OnReadContent(section);

	OpButton *button;
	OpInputAction action_close(OpInputAction::ACTION_CANCEL);
	
	if (button = (OpButton*) GetWidgetByTypeAndIdAndAction(OpTypedObject::WIDGET_TYPE_BUTTON, 0, &action_close, FALSE))
	{
		button->SetButtonStyle(OpButton::STYLE_IMAGE);
		button->SetFixedTypeAndStyle(TRUE);
	}

	OpInputAction action_show(OpInputAction::ACTION_SHOW_GADGET_UPDATE);
	if (button = (OpButton*) GetWidgetByTypeAndIdAndAction(OpTypedObject::WIDGET_TYPE_BUTTON, 0, &action_show, FALSE))
	{
		button->SetButtonType(OpButton::TYPE_PUSH_DEFAULT);
		button->SetFixedTypeAndStyle(TRUE);
	}

	OpInputAction action_update(OpInputAction::ACTION_CONFIRM_GADGET_UPDATE);
	if (button = (OpButton*) GetWidgetByTypeAndIdAndAction(OpTypedObject::WIDGET_TYPE_BUTTON, 0, &action_update, FALSE))
	{
		button->SetButtonType(OpButton::TYPE_PUSH_DEFAULT);
		button->SetFixedTypeAndStyle(TRUE);
	}
}

BOOL OpUpdateToolbar::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CANCEL:
			m_updater->CancelGadgetUpdate();				
			break;
			
		case OpInputAction::ACTION_SHOW_GADGET_UPDATE:
			m_updater->ShowUpdateDialog();
			break;

		case OpInputAction::ACTION_CONFIRM_GADGET_UPDATE:
			m_updater->ShowUpdateDialog(TRUE);
			break;

	};
	return OpToolbar::OnInputAction(action);
}

#endif //WIDGETS_UPDATE_SUPPORT