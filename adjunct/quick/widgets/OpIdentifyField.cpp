// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#include "core/pch.h"

#include "OpIdentifyField.h"

#include "modules/url/uamanager/ua.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
//#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/util/gen_str.h"
#include "adjunct/quick/Application.h"
#include "modules/pi/OpDragManager.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/dragdrop/dragdrop_manager.h"

OpIdentifyField::OpIdentifyField(OpWidgetListener *listener)
	: m_listener(listener)
{
	SetSpyInputContext(this, FALSE);
	SetJustify(JUSTIFY_CENTER, FALSE);
	GetBorderSkin()->SetImage("Identify As Skin");
}

void OpIdentifyField::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpLabel::OnPaint(widget_painter, paint_rect);
}


void OpIdentifyField::OnAdded()
{
	if (GetParentDesktopWindowType() == DIALOG_TYPE_CUSTOMIZE_TOOLBAR)
	{
		SetSpyInputContext(NULL);
	}
	UpdateIdentity();
}


void OpIdentifyField::OnDragStart(const OpPoint& point)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(WIDGET_TYPE_IDENTIFY_FIELD, point.x, point.y);
	if (drag_object)
	{
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}


void OpIdentifyField::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	*w = GetTextWidth() + 8;
	*h = GetTextHeight() + 2;
}


void OpIdentifyField::OnSettingsChanged(DesktopSettings* settings)
{
	OpLabel::OnSettingsChanged(settings);
	if(settings->IsChanged(SETTINGS_IDENTIFY_AS))
	{
		UpdateIdentity();
	}
}


void OpIdentifyField::UpdateIdentity()
{
	if( GetParentDesktopWindow() )
	{
		OpString tmp;
		if (GetParentDesktopWindowType() == DIALOG_TYPE_CUSTOMIZE_TOOLBAR)
		{
			g_languageManager->GetString(Str::S_IDENTIFY_AS_LABEL, tmp);
		}
		else
		{
			int uaid = (int)g_uaManager->GetUABaseId();

			switch (uaid)
			{
			case UA_Opera:
				{
					g_languageManager->GetString(Str::MI_IDM_CONTROL_MENU_IDENTIFY_OPERA, tmp);
					break;
				}
			case UA_Mozilla:
				{
					g_languageManager->GetString(Str::M_IDAS_MENU_MOZILLA, tmp);
					break;
				}
			case UA_MSIE:
			default:
				{
					g_languageManager->GetString(Str::M_IDAS_MENU_INTERNET_EXPLORER, tmp);
					break;
				}
			}
		}

		SetText(tmp.CStr());

		Relayout();
	}
}


void OpIdentifyField::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if( button == MOUSE_BUTTON_1 )
	{
		if( GetParentDesktopWindowType() != DIALOG_TYPE_CUSTOMIZE_TOOLBAR )
		{
			int uaid = (int)g_uaManager->GetUABaseId();
			switch (uaid)
			{
			case UA_Opera:
				{
					g_input_manager->InvokeAction(OpInputAction::ACTION_IDENTIFY_AS,1);
					break;
				}
			case UA_Mozilla:
				{
					g_input_manager->InvokeAction(OpInputAction::ACTION_IDENTIFY_AS,4);
					break;
				}
			case UA_MSIE:
				{
					g_input_manager->InvokeAction(OpInputAction::ACTION_IDENTIFY_AS,0);
					break;
				}
			}
		}
	}
	OpLabel::OnMouseUp(point, button, nclicks);
}

