/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/Application.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpDragManager.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "OpStatusField.h"

/***********************************************************************************
 **
 **	OpStatusField
 **
 ***********************************************************************************/

DEFINE_CONSTRUCT(OpStatusField);

OpStatusField::OpStatusField(DesktopWindowStatusType type) :
	m_global_widget(FALSE),
	m_status_type(type),
	m_desktop_window(0)
{
	SetEllipsis(g_pcui->GetIntegerPref(PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END);

    m_underlying_status_text.Set(UNI_L(""));

	switch (type)
	{
		case STATUS_TYPE_ALL:
			SetGrowValue(2);
			GetBorderSkin()->SetImage("Status Skin");
			break;

		case STATUS_TYPE_TITLE:
			GetBorderSkin()->SetImage("Status Title Skin");
			break;

		case STATUS_TYPE_TOOLBAR:
		case STATUS_TYPE_INFO:
			GetBorderSkin()->SetImage("Status Info Skin");
			break;
	}

	SetWrap(FALSE);

}

/***********************************************************************************
 **
 **	GetPreferedSize
 **
 ***********************************************************************************/

void OpStatusField::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	OpLabel::GetPreferedSize(w, h, cols, rows);
	// Hack fix to make the Status field appear correct in the Customise Appearance dialog
	*w += 8;
}

/***********************************************************************************
 **
 **	OnAdded
 **
 ***********************************************************************************/

void OpStatusField::OnAdded()
{
	DesktopWindow* parent_window = GetParentDesktopWindow();
	DesktopWindow* top_window = parent_window->GetToplevelDesktopWindow();

	if (top_window)
	{
		top_window->AddListener(this);
		m_desktop_window = top_window;
	}

	// only mail & irc window need status field to display tab specific information(Room title, etc)
	OpTypedObject::Type parent_type = parent_window->GetType();

	if (parent_type == WINDOW_TYPE_CHAT_ROOM || parent_type == WINDOW_TYPE_CHAT || parent_type == WINDOW_TYPE_MAIL_VIEW)
		m_global_widget = FALSE;
	else
		m_global_widget = TRUE;

	if (parent_type == DIALOG_TYPE_CUSTOMIZE_TOOLBAR)
	{
		OpString title;

		g_languageManager->GetString(Str::S_STATUS_FIELD_TITLE, title);
		SetText(title.CStr());
	}
	else if (top_window)
	{
		SetText(top_window->GetStatusText(m_status_type));
	}
}

/***********************************************************************************
 **
 **	OnRemoving
 **
 ***********************************************************************************/

void OpStatusField::OnRemoving()
{
 	if (m_desktop_window)
 		m_desktop_window->RemoveListener(this);
}

/***********************************************************************************
 **
 **	OnDragStart
 **
 ***********************************************************************************/

void OpStatusField::OnDragStart(const OpPoint& point)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(WIDGET_TYPE_STATUS_FIELD, point.x, point.y);

	if (drag_object)
	{
		drag_object->SetObject(this);
		drag_object->SetID(m_status_type);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

/***********************************************************************************
 **
 **	OnDesktopWindowStatusChanged
 **
 ***********************************************************************************/

void OpStatusField::OnDesktopWindowStatusChanged(DesktopWindow* desktop_window, 
												 DesktopWindowStatusType type)
{
	if (m_status_type == STATUS_TYPE_TOOLBAR || type != m_status_type || GetParentDesktopWindowType() == DIALOG_TYPE_CUSTOMIZE_TOOLBAR)
	{
		return;
	}

	if( m_global_widget )
	{
 		if (desktop_window->GetStatusText(m_status_type) == NULL)
 			SetText( desktop_window->GetUnderlyingStatusText(m_status_type));
 		else
			SetText( desktop_window->GetStatusText(m_status_type) );
	}

	if(!m_global_widget)
	{
		DesktopWindow* parent = GetParentDesktopWindow();
		if (parent->GetStatusText(m_status_type) == NULL)
 			SetText( parent->GetUnderlyingStatusText(m_status_type));
 		else
			SetText( parent->GetStatusText(m_status_type) );
	}

	InvalidateAll();

	ResetRequiredSize();
	GetParent()->OnContentSizeChanged();
}


/***********************************************************************************
 **
 **	OnDesktopWindowClosing
 **
 ***********************************************************************************/
void OpStatusField::OnDesktopWindowClosing(DesktopWindow* desktop_window, 
										   BOOL user_initiated)
{
	if (m_desktop_window == desktop_window)
	{
		m_desktop_window = 0;
	}
}

////////////////////////////////////////////////////////////////

void OpStatusField::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
#ifdef WINDOW_MOVE_FROM_TOOLBAR
	if (!g_application->IsCustomizingToolbars())
	{
		if (GetParent()->GetType() == WIDGET_TYPE_TOOLBAR)
		{
			GetParent()->OnMouseDown(point, button, nclicks);
			return;
		}
	}
#endif // WINDOW_MOVE_FROM_TOOLBAR

	OpLabel::OnMouseDown(point, button, nclicks);
}

////////////////////////////////////////////////////////////////////////////////////

void OpStatusField::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
#ifdef WINDOW_MOVE_FROM_TOOLBAR
	if (!g_application->IsCustomizingToolbars())
	{
		if (GetParent() && GetParent()->GetType() == WIDGET_TYPE_TOOLBAR)
		{
			GetParent()->OnMouseUp(point, button, nclicks);
			return;
		}
	}
#endif // WINDOW_MOVE_FROM_TOOLBAR

	OpLabel::OnMouseUp(point, button, nclicks);
}

////////////////////////////////////////////////////////////////////////////////////

void OpStatusField::OnMouseMove(const OpPoint &point)
{
#ifdef WINDOW_MOVE_FROM_TOOLBAR
	if (!g_application->IsCustomizingToolbars())
	{
		if (GetParent() && GetParent()->GetType() == WIDGET_TYPE_TOOLBAR)
		{
			GetParent()->OnMouseMove(point);
			return;
		}
	}
#endif // WINDOW_MOVE_FROM_TOOLBAR

	OpLabel::OnMouseMove(point);
}

////////////////////////////////////////////////////////////////////////////////////
