/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WIDGET_STATE_LISTENER_H
#define WIDGET_STATE_LISTENER_H

class WidgetState;
class WidgetStateModifier;

/***********************************************************************************
**  @class WidgetStateListener
**	@brief Abstract class, implemented by widgets that want to change their state based on
**
**  The modifier is the one responsible for setting the state of a state-listening widget
**  and to maintain the state objects.
**  NOTE: if you need a toolbar buttonthat is changing its icon and action at the same time,
**  you can use built-in OpButton functionality instead (set actions in the toolbar definition 
**  file and define icons per action in the skin file).
**
************************************************************************************/
class WidgetStateListener
{
public:
	WidgetStateListener();
	WidgetStateListener(const WidgetStateListener & state_listener);
	virtual ~WidgetStateListener();

	/* The callback for the modifier to inform about state changes. */
	virtual void OnStateChanged(WidgetState* state) = 0;

	/*
	**  Sets the modifier of this class (and removes links to the old listener).
	**  Only one modifier per listener is allowed. The modifier is not owned by the listener.
	*/
	virtual void SetModifier(WidgetStateModifier* modifier);

	/* Gets a reference to the modifier */
	virtual WidgetStateModifier* GetModifier()	{ return m_modifier; }

private:
	WidgetStateModifier*	m_modifier;
};

#endif // WIDGET_STATE_LISTENER_H
