/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/DesktopToggleButton.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "modules/display/vis_dev.h"

OP_STATUS
DesktopToggleButton::Construct(DesktopToggleButton** obj, OpButton::ButtonType button_type)
{
	*obj = OP_NEW(DesktopToggleButton, (button_type));
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}


DesktopToggleButton::DesktopToggleButton(OpButton::ButtonType button_type)
	: OpButton(button_type)
	, m_current_state(-1)
{
	SetFixedImage(FALSE);
}

/*virtual*/ OP_STATUS
DesktopToggleButton::AddToggleState(OpInputAction * action, const OpStringC & text, const OpStringC8 & widget_image, int state_id)
{
	if (!action)
	{
		return OpStatus::ERR;
	}

	if (text.HasContent())
	{
		action->SetActionText(text.CStr());
	}
	if (widget_image.HasContent())
	{
		RETURN_IF_ERROR(action->SetActionImage(widget_image.CStr()));
	}

	if (state_id == -1)
	{
		RETURN_IF_ERROR(m_toggle_actions.Add(action));
	}
	else
	{
		RETURN_IF_ERROR(m_toggle_actions.Insert(state_id, action));
	}

	if (m_toggle_actions.GetCount() == 1) // first element added
	{
		m_current_state = 0;
		OnUpdateActionState();
	}

	return OpStatus::OK;
}

/*virtual*/ void
DesktopToggleButton::SetAction(OpInputAction* action)
{
	OpWidget::SetAction(action);
	m_action_to_use = action;
}

OP_STATUS DesktopToggleButton::SetToggleStateText(const OpStringC & text, int state_id)
{
	OpInputAction *state_action = m_toggle_actions.Get(state_id);
	if (!state_action)
		return OpStatus::ERR;

	state_action->SetActionText(text);
	return OpStatus::OK;
}

void DesktopToggleButton::Toggle()
{
	GetNextClickAction(FALSE);
	OnToggle();
}

void DesktopToggleButton::OnToggle()
{
	OnUpdateActionState();
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator); )
		m_listeners.GetNext(iterator)->OnToggle();
}

/*virtual*/ OpInputAction*
DesktopToggleButton::GetNextClickAction(BOOL plus_action)
{
	OpInputAction* action = m_toggle_actions.Get(m_current_state);
	m_current_state = (m_current_state + 1) % m_toggle_actions.GetCount();
	return action;
}

/*virtual*/ void
DesktopToggleButton::Click(BOOL plus_action)
{
	OpButton::Click(plus_action);
	OnToggle();
}

/*virtual*/ void
DesktopToggleButton::GetActionState(OpInputAction*& action_to_use,
		INT32& state_to_use, BOOL& next_operator_used)
{
	action_to_use = m_toggle_actions.Get(m_current_state);
	if (action_to_use)
		state_to_use = g_input_manager->GetActionState(action_to_use, GetParentInputContext());
	next_operator_used = FALSE;
}
