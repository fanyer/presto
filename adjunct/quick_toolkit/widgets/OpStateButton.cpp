/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpStateButton.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetState.h"
#include "adjunct/quick/Application.h"
#include "modules/dragdrop/dragdrop_manager.h"

/***********************************************************************************
**  Constructor.
**  OpStateButton::OpStateButton
**
************************************************************************************/
OpStateButton::OpStateButton() :
	OpButton(TYPE_CUSTOM),
	WidgetStateListener(),
	m_tooltip_text(),
	m_tooltip_from_state(TRUE),
	m_state(NULL)
{
	SetSkinned(TRUE);
}


/***********************************************************************************
**  Copy Constructor
**
**  OpStateButton::OpStateButton
************************************************************************************/
OpStateButton::OpStateButton(const OpStateButton & state_button) :
	OpButton(TYPE_CUSTOM),
	WidgetStateListener(state_button),
	m_tooltip_from_state(state_button.m_tooltip_from_state)
{
	//OOM unsafe
	OpStatus::Ignore(m_tooltip_text.Set(state_button.m_tooltip_text));
	SetSkinned(TRUE);
}


/***********************************************************************************
**  OpStateButton::IsUsingTooltipFromState
**  @return
**
************************************************************************************/
BOOL OpStateButton::IsUsingTooltipFromState()
{
	return m_tooltip_from_state;
}


/***********************************************************************************
**  OpStateButton::SetIsUsingTooltipFromState
**  @param tooltip_from_state
**
************************************************************************************/
void OpStateButton::SetIsUsingTooltipFromState(BOOL tooltip_from_state)
{
	m_tooltip_from_state = tooltip_from_state;
}

/***********************************************************************************
**  OpStateButton::OnAdded
************************************************************************************/
void OpStateButton::OnAdded()
{
	OnStateChanged(GetModifier()->GetCurrentWidgetState());
}

/***********************************************************************************
**  OpStateButton::OnDragStart
************************************************************************************/
void OpStateButton::OnDragStart(const OpPoint& point)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(WIDGET_TYPE_STATE_BUTTON, point.x, point.y);

	if (drag_object)
	{
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

/***********************************************************************************
**  OpStateButton::GetPreferedSize
************************************************************************************/
void
OpStateButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_BUTTON, w, h, cols, rows);
}

/***********************************************************************************
**  Sets state changed properties on the button. The state is owned by the modifier,
**  so we need to make sure that state properties are copied before being applied.
**
**  OpStateButton::OpStateButton
**  @param state
**
************************************************************************************/
void OpStateButton::OnStateChanged(WidgetState * state)
{
	if (state)
	{
		m_state = state;
		SetText(state->GetText());

		OpInputAction * action = OpInputAction::CopyInputActions(const_cast<OpInputAction*>(state->GetAction()));
		if (state->GetForegroundImage())
		{
			action->SetActionImage(state->GetForegroundImage());
			SetAction(action);
		}
		m_tooltip_text.Set(state->GetTooltipText());
	}
}


/***********************************************************************************
**  OpStateButton::HasToolTipText
**  @param tooltip
**  @return
**
************************************************************************************/
BOOL OpStateButton::HasToolTipText(OpToolTip* tooltip)
{
	// needed for the debugging tooltip
	if (IsDebugToolTipActive())
		return TRUE;

	return (IsUsingTooltipFromState() && !m_tooltip_text.IsEmpty()) || OpButton::HasToolTipText(tooltip);
}


/***********************************************************************************
**  OpStateButton::GetToolTipText
**  @param tooltip
**  @param text
**
************************************************************************************/
void OpStateButton::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	// needed for the debugging tooltip first
	if (IsDebugToolTipActive())
		QuickOpWidgetBase::GetToolTipText(tooltip, text);
	else if (IsUsingTooltipFromState())
		text.SetTooltipText(m_tooltip_text.CStr());
	else
		OpButton::GetToolTipText(tooltip, text);
}


/***********************************************************************************
**  OpStateButton::GetToolTipDelay
**  @param tooltip
**  @return 
**
************************************************************************************/
INT32 OpStateButton::GetToolTipDelay(OpToolTip* tooltip)
{
	return OpButton::GetToolTipDelay(tooltip);
}

/***********************************************************************************
**  OpStateButton::Click
**
************************************************************************************/
/*virtual*/ void
OpStateButton::Click(BOOL plus_action)
{
	if (m_state && m_state->HasAttention())
	{
		m_state->SetAttention(FALSE);
		OnStateChanged(m_state);
	}

	OpButton::Click(plus_action);
}
