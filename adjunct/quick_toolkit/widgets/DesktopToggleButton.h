/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef DESKTOP_TOGGLE_BUTTON_H
#define DESKTOP_TOGGLE_BUTTON_H

#include "modules/widgets/OpButton.h"

class OpInputAction;

/**
 * @brief A button with one action per state
 */
class DesktopToggleButton : public OpButton
{
public:
	class Listener
	{
	public:
		virtual ~Listener() {}
		virtual void OnToggle() = 0;
	};

	static OP_STATUS	Construct(DesktopToggleButton** obj, ButtonType button_type = TYPE_PUSH);

	OP_STATUS			AddToggleState(OpInputAction * action, const OpStringC & text = NULL, const OpStringC8 & widget_image = NULL, int state_id = -1);

	OP_STATUS           SetToggleStateText(const OpStringC & text, int state_id);

	OP_STATUS AddListener(Listener* listener)    { return m_listeners.Add(listener);    }
	OP_STATUS RemoveListener(Listener* listener) { return m_listeners.Remove(listener); }

	void Toggle();

	// OpButton
	virtual void        SetAction(OpInputAction * action);
	virtual void		Click(BOOL plus_action = false);
	virtual INT32		GetValue() { return m_current_state; }

protected:
	DesktopToggleButton(ButtonType button_type = TYPE_PUSH);

private:
	// OpButton
	virtual OpInputAction* GetNextClickAction(BOOL plus_action);
	virtual void           GetActionState(OpInputAction*& action_to_use, INT32& state_to_use, BOOL& next_operator_used);

	void OnToggle();

	OpAutoVector<OpInputAction>		m_toggle_actions; //< The actions stored for each toggle state
	INT32							m_current_state;

	OpListeners<Listener>			m_listeners;
};

#endif // DESKTOP_TOGGLE_BUTTON_H
