/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "OpTabs.h"
#include "modules/util/adt/opvector.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinManager.h"
#include "adjunct/quick/Application.h"

/***********************************************************************************
**
**	OpTabs
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpTabs)

OpTabs::OpTabs()
	: m_tab_cycle_context(*this)
{
	SetSupportsReadAndWrite(FALSE);
	SetStandardToolbar(FALSE);
#ifdef QUICK_TOOLKIT_CENTER_TABS_BY_DEFAULT
	SetCentered(g_skin_manager->GetOptionValue("Center Tabs", TRUE));
#else
	SetCentered(g_skin_manager->GetOptionValue("Center Tabs"));
#endif
	SetWrapping(OpBar::WRAPPING_OFF);
	SetSelector(TRUE);
	SetButtonType(OpButton::TYPE_TAB);
	SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	GetBorderSkin()->SetImage("Tabs Skin");
	UpdateSkinPadding();
	g_application->AddSettingsListener(this);
}

/***********************************************************************************
**
**	~OpTabs
**
***********************************************************************************/

OpTabs::~OpTabs()
{
	if (g_application)
		g_application->RemoveSettingsListener(this);
}

/***********************************************************************************
**
**	AddTab
**
***********************************************************************************/

OpButton* OpTabs::AddTab(const uni_char* title, void* user_data)
{
	OpButton* button = AddButton(title, NULL, NULL, user_data);
#if defined(HAS_TAB_BUTTON_POSITION)
	UpdateTabStates(-1);
#endif
	return button;
}

/***********************************************************************************
**
**	RemoveTab
**
***********************************************************************************/

void OpTabs::RemoveTab(INT32 pos)
{
	RemoveWidget(pos);
#if defined(HAS_TAB_BUTTON_POSITION)
	UpdateTabStates(-1);
#endif
}

/***********************************************************************************
**
**	FindTab
**
***********************************************************************************/

INT32 OpTabs::FindTab(void* user_data)
{
	return FindWidgetByUserData(user_data);
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void OpTabs::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_SKIN))
	{
#ifdef QUICK_TOOLKIT_CENTER_TABS_BY_DEFAULT
		SetCentered(g_skin_manager->GetOptionValue("Center Tabs", TRUE));
#else
		SetCentered(g_skin_manager->GetOptionValue("Center Tabs"));
#endif
	}
}


/***********************************************************************************
**
**	OnAdded
**
***********************************************************************************/

void OpTabs::OnAdded()
{
	OpToolbar::OnAdded();
	m_tab_cycle_context.Insert();
}

/***********************************************************************************
**
**	OnRemoving
**
***********************************************************************************/

void OpTabs::OnRemoving()
{
	m_tab_cycle_context.Remove();
	OpToolbar::OnRemoving();
}

/***********************************************************************************
**
**	TabCycleContext::Insert
**
***********************************************************************************/

void OpTabs::TabCycleContext::Insert()
{
	// Insert m_tab_cycle_context as child of dialog context.
	m_child = NULL;
	for (OpInputContext* dialog = m_tabs->GetParentInputContext();
			dialog != NULL; dialog = dialog->GetParentInputContext())
	{
		if (OpStringC8(dialog->GetInputContextName()) == "Dialog")
		{
			if (m_child != NULL)
			{
				m_child->SetParentInputContext(this);
			}
			SetParentInputContext(dialog);
			break;
		}
		m_child = dialog;
	}
}

/***********************************************************************************
**
**	TabCycleContext::Remove
**
***********************************************************************************/

void OpTabs::TabCycleContext::Remove()
{
	if (m_child != NULL)
	{
		m_child->SetParentInputContext(GetParentInputContext());
	}
	SetParentInputContext(NULL);
}

/***********************************************************************************
**
**	TabCycleContext::OnInputAction
**
***********************************************************************************/

BOOL OpTabs::TabCycleContext::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
			switch (action->GetChildAction()->GetAction())
			{
				case OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE:
				case OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE:
					action->GetChildAction()->SetEnabled(m_tabs->IsEnabled());
					return TRUE;
			}
			return FALSE;

		case OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE:
			m_tabs->SelectNext();
			return TRUE;

		case OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE:
			m_tabs->SelectPrevious();
			return TRUE;
	}
	return FALSE;
}

#if defined(HAS_TAB_BUTTON_POSITION)
void OpTabs::UpdateTabStates(int selected_tab)
{
	INT32 count = GetWidgetCount();
	for (INT32 i=0; i<count; i++)
	{
		if (GetWidget(i)->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
		{
			OpButton* button = ((OpButton*)GetWidget(i));
			if (button->GetButtonType() == OpButton::TYPE_TAB)
			{
				int state = 0;
				if (i==0)
					state |= OpButton::TAB_FIRST;
				if (i==count-1)
					state |= OpButton::TAB_LAST;
				if (i==selected_tab)
					state |= OpButton::TAB_SELECTED;
				if (i>0 && i-1 == selected_tab)
					state |= OpButton::TAB_PREV_SELECTED;
				if (i+1<count && i+1 == selected_tab)
					state |= OpButton::TAB_NEXT_SELECTED;
				if (state != button->GetTabState())
				{
					button->SetTabState(state);
					button->InvalidateAll();
				}
			}
		}
	}
}
#endif // HAS_TAB_BUTTON_POSITION

BOOL OpTabs::SetSelected(INT32 index, BOOL invoke_listeners, BOOL changed_by_mouse)
{
#if defined(HAS_TAB_BUTTON_POSITION)
	UpdateTabStates(index);
#endif
	return OpToolbar::SetSelected(index, invoke_listeners);
}

#if defined ACCESSIBILITY_EXTENSION_SUPPORT && defined MSWIN
OpAccessibleItem* OpTabs::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpAccessibleItem* child = OpWidget::GetAccessibleChildOrSelfAt(x, y);
	if (child)
		return child;
	else
	{
		int count = GetWidgetCount();
		for (int i = 0; i < count; i++)
		{
			if (GetWidget(i)->GetAccessibleChildOrSelfAt(x, y))
				return GetWidget(i);
		}
	}
	return NULL;
}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT && MSWIN
