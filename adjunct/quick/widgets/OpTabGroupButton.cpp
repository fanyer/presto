/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/OpTabGroupButton.h"

#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	OpTabGroupButton
**
***********************************************************************************/

////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpTabGroupButton::Construct(OpTabGroupButton** obj, DesktopGroupModelItem& group)
{
	OpAutoPtr<OpTabGroupButton> button (OP_NEW(OpTabGroupButton, (group)));
	RETURN_OOM_IF_NULL(button.get());
	RETURN_IF_ERROR(button->init_status);

	*obj = button.release();
	return OpStatus::OK; 
}

////////////////////////////////////////////////////////////////////////////////////////

// Here, m_group is a pointer due to proper interaction between destructor and OnGroupDestroyed.
// However, the group parameter is pushed down as a reference all the way from OpWorkspace::Listener
// interface on OpPagebar. We can assume that here, the group is _always_ valid.

OpTabGroupButton::OpTabGroupButton(DesktopGroupModelItem& group) :
	OpPagebarItem(group.GetID(), group.GetID()),
	m_group(&group),
	m_expander_button(NULL),
	m_indicator_button(NULL)
{
	SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	SetFixedTypeAndStyle(TRUE);
	GetBorderSkin()->SetImage("Tab Group Button Skin");

	init_status = OpButton::Construct(&m_expander_button, OpButton::TYPE_CUSTOM);
	RETURN_VOID_IF_ERROR(init_status);
	m_expander_button->GetBorderSkin()->SetForeground(TRUE);
	m_expander_button->GetBorderSkin()->SetImage("Tab Group Collapse");
	m_expander_button->SetIgnoresMouse(TRUE);
	AddChild(m_expander_button);

	m_indicator_button = OP_NEW(OpIndicatorButton, (NULL, OpIndicatorButton::RIGHT));
	if (!m_indicator_button)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	AddChild(m_indicator_button);
	init_status = m_indicator_button->Init("Tabbar Tabstack Indicator Skin");
	RETURN_VOID_IF_ERROR(init_status);
	m_indicator_button->SetIconState(OpIndicatorButton::GEOLOCATION, OpIndicatorButton::INVERTED);
	m_indicator_button->SetIconState(OpIndicatorButton::CAMERA, OpIndicatorButton::INVERTED);
	m_indicator_button->SetIconState(OpIndicatorButton::SEPARATOR, OpIndicatorButton::INVERTED);

	RefreshIndicatorState();	

	// Set the default value
	OnCollapsedChanged(&group, group.IsCollapsed());
	group.AddListener(this);
	CalculateAndSetMinWidth();

}

OpTabGroupButton::~OpTabGroupButton()
{
	if (m_group)
		m_group->RemoveListener(this);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	string.NeedUpdate();
	UpdateActionStateIfNeeded();

	GetInfo()->GetPreferedSize(this, GetType(), w, h, cols, rows);

	m_expander_button->GetBorderSkin()->GetSize(w, h);
	INT32 l, t, r, b;
	m_expander_button->GetBorderSkin()->GetPadding(&l, &t, &r, &b);
	*w += l + r;
	*h += t + b;
	if (m_indicator_button->IsVisible())
	{
		*w += m_indicator_button->GetPreferredWidth();
		*h = max(*h, m_indicator_button->GetPreferredHeight());
	}
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	OpString tooltip_text;
	if (GetValue())
	{
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_BUTTON_TAB_GROUP_EXPAND, tooltip_text));
	}
	else 
	{
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_BUTTON_TAB_GROUP_COLLAPSE, tooltip_text));
	}
	OpStatus::Ignore(text.SetTooltipText(tooltip_text.CStr()));
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::CalculateAndSetMinWidth()
{
	INT32 min_width, dummy;
	GetPreferedSize(&min_width, &dummy, 0, 0);
	SetMinWidth(min_width);
	SetFixedMinMaxWidth(TRUE);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::OnLayout()
{
	UpdateButtonsSkinType();

	OpRect rect = GetBounds();
	GetBorderSkin()->AddPadding(rect);

	INT32 width, height;
	INT32 indicator_width = 0;

	if (m_indicator_button->IsVisible())
	{
		OpRect indicator_rect = rect;
		indicator_width = m_indicator_button->GetPreferredWidth();
		indicator_rect.width = indicator_width;
		SetChildRect(m_indicator_button, indicator_rect);
	}

	m_expander_button->GetBorderSkin()->GetSize(&width, &height);
	SkinType type = parent->GetBorderSkin()->GetType();
	if (type == SKINTYPE_LEFT || type == SKINTYPE_RIGHT)
	{
		rect.x = (rect.width - width) / 2;
	}
	else
	{
		rect.x += indicator_width;
	}

	OpRect expander_rect = rect;
	expander_rect.width = width;
	expander_rect.height = height;
	expander_rect.y += (rect.height - expander_rect.height) / 2;
	m_expander_button->GetBorderSkin()->AddPadding(expander_rect);
	SetChildRect(m_expander_button, expander_rect);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::UpdateButtonsSkinType()
{
	SkinType type = parent->GetBorderSkin()->GetType();
	m_indicator_button->GetBorderSkin()->SetType(type);
	m_expander_button->GetBorderSkin()->SetType(type);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OpButton::OnMouseDown(point, button, nclicks);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::Click(BOOL plus_action)
{
	OpWidgetListener* listener = NULL;

	// Kill the toolbar listener
	if (GetListener())
	{
		listener = GetListener();
		SetListener(NULL, TRUE);
	}

	OpButton::Click(plus_action);

	// Toggle the group
	if (m_group)
		m_group->SetCollapsed(GetValue() == 0);

	// Restore the toolbar listener
	SetListener(listener, TRUE);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::SetSkin()
{
	// Set the expanded ot collapsed skin (Value=1 is compressed)
	if (GetValue())
	{
		m_expander_button->GetBorderSkin()->SetImage("Tab Group Collapse");
	}
	else
	{
		m_expander_button->GetBorderSkin()->SetImage("Tab Group Expand");
	}
}

////////////////////////////////////////////////////////////////////////////////////////

void OpTabGroupButton::OnCollapsedChanged(DesktopGroupModelItem* group, bool collapsed)
{
	// Change the state
	SetValue(collapsed);

	// Update the skin
	SetSkin();

	m_indicator_button->SetVisibility(collapsed ? TRUE : FALSE);

	CalculateAndSetMinWidth();
}

void OpTabGroupButton::AddIndicator(OpIndicatorButton::IndicatorType type)
{
	m_indicator_button->AddIndicator(type);
	CalculateAndSetMinWidth();
}

void OpTabGroupButton::RemoveIndicator(OpIndicatorButton::IndicatorType type)
{
	m_indicator_button->RemoveIndicator(type);
	CalculateAndSetMinWidth();
}

void OpTabGroupButton::RefreshIndicatorState()
{
	short indicator_state = 0;
	if (!m_group)
	{
		return;
	}
	for (INT32 i = m_group->GetChildIndex(); i <= m_group->GetLastChildIndex(); i++)
	{
		DesktopWindowCollectionItem* item = m_group->GetModel()->GetItemByIndex(i);
		DesktopWindow* window = item->GetDesktopWindow();
		PagebarButton* button = window->GetPagebarButton();
		/* It can happen that DesktopWindow still exists but PagebarButton related
		 * to it is already destroyed when Opera is closing so we have to check if
		 *  button is not NULL.
		 */
		if (button)
		{
			OpIndicatorButton* indicator = button->GetIndicatorButton();
			if (indicator->IsVisible() == TRUE && window != m_group->GetActiveDesktopWindow())
			{
				indicator_state |= indicator->GetIndicatorType();
			}
		}
	}
	m_indicator_button->SetIndicatorType(indicator_state);
	if (!m_group->IsCollapsed())
	{
		m_indicator_button->SetVisibility(FALSE);
	}
	CalculateAndSetMinWidth();
}
