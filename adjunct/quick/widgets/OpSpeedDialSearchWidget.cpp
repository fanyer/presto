/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "OpSpeedDialSearchWidget.h"
#include "modules/inputmanager/inputmanager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#ifdef _X11_SELECTION_POLICY_
# include "adjunct/quick/managers/DesktopClipboardManager.h"
#endif // _X11_SELECTION_POLICY_

#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinManager.h"

#include "modules/util/str.h"
#include "modules/util/OpLineParser.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/dochand/win.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick/widgets/OpSearchDropDown.h"
#include "modules/widgets/OpButton.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES

#define SEARCH_MINIMUM_WIDTH 300

/***********************************************************************************
**
**	OpSpeedDialSearchWidget -
**
***********************************************************************************/

OP_STATUS OpSpeedDialSearchWidget::Construct(OpSpeedDialSearchWidget** obj)
{
	*obj = OP_NEW(OpSpeedDialSearchWidget, ());
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpSpeedDialSearchWidget::OpSpeedDialSearchWidget()
	: m_search_field(NULL)
{
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		AccessibilitySkipsMe();
#endif
}

OpSpeedDialSearchWidget::~OpSpeedDialSearchWidget()
{
}

OP_STATUS OpSpeedDialSearchWidget::Init()
{
	SetSkinned(TRUE);

	GetBorderSkin()->SetImage("Speed Dial Search Widget Skin");
	UpdateSkinPadding();
	UpdateSkinMargin();

	RETURN_IF_ERROR(OpSearchDropDownSpecial::Construct(&m_search_field));

	m_search_field->GetBorderSkin()->SetForeground(TRUE);
	m_search_field->SetSearchInSpeedDial(TRUE);

	AddChild(m_search_field);

	return OpStatus::OK;
}

void OpSpeedDialSearchWidget::OnLayout()
{
	DoLayout();

	OpWidget::OnLayout();
}

BOOL OpSpeedDialSearchWidget::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	if(widget == m_search_field)
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Edit Go Widget Popup Menu", PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
		return TRUE;
	}
	return FALSE;
}

void OpSpeedDialSearchWidget::DoLayout()
{
	if(m_search_field)
	{
		//Get dimensions (size and padding) of this container widget:
		OpRect rect = GetBounds();
		AddPadding(rect);

		//Put the search field in its place
		m_search_field->SetRect(rect);
	}
}

void OpSpeedDialSearchWidget::GetRequiredSize(INT32& width, INT32& height)
{
	//required size is minimum size of search field + padding of this widget
	if (m_search_field)
	{
		// get required size for a minimal search field: 20 characters in 1 row
		m_search_field->GetPreferedSize(&width, &height, 1, 1);
	}
	width = SEARCH_MINIMUM_WIDTH;

	// add padding of this wrapping widget
	width += GetPaddingLeft() + GetPaddingRight();
	height += GetPaddingTop() + GetPaddingBottom();
}

void OpSpeedDialSearchWidget::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
#if defined(_X11_SELECTION_POLICY_)
	if( button == MOUSE_BUTTON_3)
	{
		OpString text;
		g_desktop_clipboard_manager->GetText(text, true);
		if( text.Length() > 0 )
		{
			g_application->OpenURL( text, MAYBE, MAYBE, MAYBE );
		}
		return;
	}
#endif

	OpWidget::OnMouseDown(point,button,nclicks);
}
