/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 * 
 * @author Cezary Kulakowski (ckulakowski)
 */


#include "core/pch.h"

#include "adjunct/quick/widgets/OpIndicatorButton.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"


OP_STATUS OpIndicatorButton::Init(const char* border_skin)
{
	SetButtonTypeAndStyle(TYPE_TOOLBAR, STYLE_IMAGE);
	GetBorderSkin()->SetImage(border_skin);
	SetIgnoresMouse(TRUE);

	RETURN_IF_ERROR(OpButton::Construct(&m_camera_button, OpButton::TYPE_CUSTOM));
	m_camera_button->GetBorderSkin()->SetForeground(TRUE);
	m_camera_button->SetVisibility(FALSE);
	m_camera_button->SetIgnoresMouse(TRUE);
	m_camera_button->GetBorderSkin()->SetImage("Camera Indicator Inactive");
	m_camera_button->SetName("Camera_indicator");
	AddChild(m_camera_button);

	RETURN_IF_ERROR(OpButton::Construct(&m_geolocation_button, OpButton::TYPE_CUSTOM));
	m_geolocation_button->GetBorderSkin()->SetForeground(TRUE);
	m_geolocation_button->SetVisibility(FALSE);
	m_geolocation_button->SetIgnoresMouse(TRUE);
	m_geolocation_button->GetBorderSkin()->SetImage("Geolocation Indicator Inactive");
	m_geolocation_button->SetName("Geolocation_indicator");
	AddChild(m_geolocation_button);

	RETURN_IF_ERROR(OpButton::Construct(&m_separator_button, OpButton::TYPE_CUSTOM));
	m_separator_button->GetBorderSkin()->SetForeground(TRUE);
	m_separator_button->SetVisibility(FALSE);
	m_separator_button->SetIgnoresMouse(TRUE);
	m_separator_button->GetBorderSkin()->SetImage("Indicator Button Separator");
	AddChild(m_separator_button, FALSE, m_separator_position == LEFT ? TRUE : FALSE);

	return OpStatus::OK;
}

void OpIndicatorButton::UpdateButtons(IndicatorType type, bool set)
{
	if (((m_indicator_type & type) > 0) == set)
	{
		return;
	}

	if (set)
	{
		m_indicator_type |= type;
	}
	else
	{
		m_indicator_type &= ~type;
	}

	m_geolocation_button->SetVisibility((m_indicator_type & GEOLOCATION) ? TRUE : FALSE);
	m_camera_button->SetVisibility((m_indicator_type & CAMERA) ? TRUE : FALSE);
	m_separator_button->SetVisibility((m_indicator_type != 0) ? TRUE : FALSE);
	BOOL set_visibility = m_desktop_window ? !m_desktop_window->IsActive() : TRUE;
	SetVisibility(set_visibility && m_indicator_type);
	parent->Relayout();
}

void OpIndicatorButton::SetIndicatorType(short type)
{
	UpdateButtons(GEOLOCATION, (type & GEOLOCATION) > 0);
	UpdateButtons(CAMERA, (type & CAMERA) > 0);
}

INT32 OpIndicatorButton::GetPreferredHeight()
{
	INT32 result = 0;
	INT32 width, height;
	INT32 left, top, right, bottom;

	if (m_indicator_type && m_show_separator)
	{
		m_separator_button->GetBorderSkin()->GetSize(&width, &height);
		m_separator_button->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
		result = max(result, height + top + bottom);
	}
	if (m_indicator_type & CAMERA)
	{
		m_camera_button->GetBorderSkin()->GetSize(&width, &height);
		m_camera_button->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
		result = max(result, height + top + bottom);
	}
	if (m_indicator_type & GEOLOCATION)
	{
		m_geolocation_button->GetBorderSkin()->GetSize(&width, &height);
		m_geolocation_button->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
		result = max(result, height + top + bottom);
	}

	return result;
}

INT32 OpIndicatorButton::GetPreferredWidth()
{
	INT32 result = 0;
	INT32 width, height;

	if (m_indicator_type && m_show_separator)
	{
		m_separator_button->GetBorderSkin()->GetSize(&width, &height);
		result = width;
	}
	if (m_indicator_type & CAMERA)
	{
		m_camera_button->GetBorderSkin()->GetSize(&width, &height);
		result += width;
	}
	if (m_indicator_type & GEOLOCATION)
	{
		m_geolocation_button->GetBorderSkin()->GetSize(&width, &height);
		result += width;
	}
	INT32 left, top, right, bottom;
	GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
	result += left + right;

	return result;
}

void OpIndicatorButton::OnLayout()
{
	OpRect rect = GetBounds();
	GetBorderSkin()->AddPadding(rect);

	if (m_indicator_type && m_show_separator && m_separator_position == LEFT)
	{
		LayoutButton(m_separator_button, rect);
	}
	if (m_indicator_type & CAMERA)
	{
		LayoutButton(m_camera_button, rect);
	}
	if (m_indicator_type & GEOLOCATION)
	{
		LayoutButton(m_geolocation_button, rect);
	}
	if (m_indicator_type && m_show_separator && m_separator_position == RIGHT)
	{
		LayoutButton(m_separator_button, rect);
	}
}

void OpIndicatorButton::LayoutButton(OpButton* button, OpRect& orig_rect)
{
	OpRect rect = orig_rect;
	button->GetBorderSkin()->GetSize(&rect.width, &rect.height);
	orig_rect.x += rect.width;
	button->GetBorderSkin()->AddPadding(rect);
	SetChildRect(button, rect);
}

void OpIndicatorButton::OnShow(BOOL show)
{
	if (show && !m_indicator_type)
	{
		SetVisibility(FALSE);
	}
}

void OpIndicatorButton::SetIconState(IndicatorType type, IconState state)
{
	switch (type)
	{
		case CAMERA:
		{
			switch (state)
			{
				case ACTIVE:
				{
					m_is_camera_active = true;
					m_camera_button->GetBorderSkin()->SetImage("Camera Indicator Active");
					break;
				}
				case INACTIVE:
				{
					m_is_camera_active = false;
					m_camera_button->GetBorderSkin()->SetImage("Camera Indicator Inactive");
					break;
				}
				case INVERTED:
				{
					m_is_camera_active = false;
					m_camera_button->GetBorderSkin()->SetImage("Camera Indicator Inverted");
					break;
				}
			}
			break;
		}
		case GEOLOCATION:
		{
			switch (state)
			{
				case ACTIVE:
				{
					m_is_geolocation_active = true;
					m_geolocation_button->GetBorderSkin()->SetImage("Geolocation Indicator Active");
					break;
				}
				case INACTIVE:
				{
					m_is_geolocation_active = false;
					m_geolocation_button->GetBorderSkin()->SetImage("Geolocation Indicator Inactive");
					break;
				}
				case INVERTED:
				{
					m_is_geolocation_active = false;
					m_geolocation_button->GetBorderSkin()->SetImage("Geolocation Indicator Inverted");
					break;
				}
			}
			break;
		}
		case SEPARATOR:
		{
			switch (state)
			{
				case ACTIVE:
				case INACTIVE:
				{
					m_separator_button->GetBorderSkin()->SetImage("Indicator Button Separator");
					break;
				}
				case INVERTED:
				{
					m_separator_button->GetBorderSkin()->SetImage("Indicator Button Separator Inverted");
					break;
				}
			}
			break;
		}
		default:
		{
			OP_ASSERT(!"Unknown indicator type");
			break;
		}
	}
}
