/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Petter Nilsen (pettern@opera.com)
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpToolbarMenuButton.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
**  Construct.
**
************************************************************************************/
OP_STATUS OpToolbarMenuButton::Construct(OpToolbarMenuButton** obj, OpButton::ButtonStyle button_style) 
{
	*obj = OP_NEW(OpToolbarMenuButton, (button_style)); 
	if (*obj == NULL) 
		return OpStatus::ERR_NO_MEMORY; 
	if (OpStatus::IsError((*obj)->init_status)) 
	{
		OP_DELETE(*obj); 
		return OpStatus::ERR_NO_MEMORY; 
	} 
	return OpStatus::OK; 
}

/***********************************************************************************
**  Constructor.
**
************************************************************************************/
OpToolbarMenuButton::OpToolbarMenuButton(OpButton::ButtonStyle button_style)
	: OpButton(TYPE_CUSTOM)
{
	Init();
}

OpToolbarMenuButton::OpToolbarMenuButton(const OpToolbarMenuButton & menu_button)
	: OpButton(TYPE_CUSTOM)
{
	Init();
}

void OpToolbarMenuButton::Init()
{
	SetSkinned(TRUE);
	SetFixedTypeAndStyle(TRUE);

	OpInputAction* action =
#ifdef QUICK_NEW_OPERA_MENU
		OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_MENU));
#else
		OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU));
#endif // QUICK_NEW_OPERA_MENU

	if (action)
	{
#ifndef QUICK_NEW_OPERA_MENU
		action->SetActionDataString(UNI_L("Browser Button Menu Bar"));
#endif // QUICK_NEW_OPERA_MENU

		OpString title;
		g_languageManager->GetString(Str::SI_MENU_BUTTON_TEXT, title);

		// Specify that action comes from keyboard. The menu action will then be executed in the keyboard 
		// context (DSK-287242). See IsInputContextAvailable() below. Both are needed for this to work
		action->SetActionMethod(OpInputAction::METHOD_KEYBOARD);
		action->GetActionInfo().SetStatusText(title.CStr());
		SetAction(action);
	}

	const uni_char* title = NULL;
	switch (g_desktop_product->GetProductType())
	{
		case PRODUCT_TYPE_OPERA_LABS:
			title = UNI_L("Opera Labs");
			break;

		case PRODUCT_TYPE_OPERA_NEXT:
			title = UNI_L("Opera Next");
			break;

		default:
			title = UNI_L("Opera");
			break;
	}
	SetSkin();
	SetText(title);

	SetName(WIDGET_NAME_MENU_BUTTON);
	
}

void OpToolbarMenuButton::SetSkin()
{
	switch (g_desktop_product->GetProductType())
	{
	case PRODUCT_TYPE_OPERA_LABS:
		GetBorderSkin()->SetImage("Main Menu Labs Toolbar Button Skin", "Toolbar Button Skin");
		GetForegroundSkin()->SetImage("Main Menu Labs");
		break;

	case PRODUCT_TYPE_OPERA_NEXT:
		GetBorderSkin()->SetImage("Main Menu Next Toolbar Button Skin", "Toolbar Button Skin");
		GetForegroundSkin()->SetImage("Main Menu Next");
		break;

	default:
		GetBorderSkin()->SetImage("Main Menu Toolbar Button Skin", "Toolbar Button Skin");
		GetForegroundSkin()->SetImage("Main Menu");
		break;
	}
}

void OpToolbarMenuButton::UpdateButton(OpButton::ButtonStyle button_style)
{
	SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, button_style);

	// changing the style changes the skin, so make sure we reset it here again
	SetSkin();
}
void
OpToolbarMenuButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_BUTTON, w, h, cols, rows);
}

BOOL OpToolbarMenuButton::OnContextMenu(const OpPoint &point, BOOL keyboard_invoked)
{
	return OpButton::OnContextMenu(point, NULL, keyboard_invoked);
}

BOOL OpToolbarMenuButton::IsInputContextAvailable(FOCUS_REASON reason)
{
	// Keep focus and context activation away from this button. It will interfere 
	// with the context where we want an action to take place (DSK-287242). See also
	// SetAction() above
	return FALSE;
}


BOOL OpToolbarMenuButton::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_REMOVE:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}
