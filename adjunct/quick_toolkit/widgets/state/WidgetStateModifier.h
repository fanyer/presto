/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter
 */

#ifndef WIDGET_STATE_MODIFIER_H
#define WIDGET_STATE_MODIFIER_H

#include "modules/util/adt/oplisteners.h"

#include "adjunct/quick_toolkit/widgets/state/WidgetStateListener.h"

class WidgetState;


/***********************************************************************************
**  @class WidgetStateModifier
**	@brief Abstract class, implemented if you want to modify a widget that is listening
**     to state changes (WidgetStateListener).
**
**  The modifier is the one responsible for setting the state of a state-listening widget
**  and to maintain the state objects.
**
************************************************************************************/
class WidgetStateModifier
{
public:
#if defined(SELFTEST) || defined(_DEBUG)
	enum Type
	{
		FakeModifier,
		OperaTurboModifier,
		SyncModifier,
		WebServerModifier
	};

	virtual Type	GetModifierType() const = 0;
#endif // defined(SELFTEST) || defined(_DEBUG)

	WidgetStateModifier();
	virtual ~WidgetStateModifier();

	virtual const char* GetDescriptionString() const = 0;
	
	virtual OP_STATUS	Init();			// do everything that needs Core there, not in the constructor
	virtual void		CleanUp() {}	// do everything that needs Core there, not in the destructor

	/* Add a state listener (widget) and calls OnStateChanged on it straight away */
	virtual OP_STATUS	AddWidgetStateListener(WidgetStateListener* listener);
	/* Remove a state listener. */
	virtual OP_STATUS	RemoveWidgetStateListener(WidgetStateListener* listener);

	/* Returns the state the listeners are currently in. */
	virtual WidgetState*	GetCurrentWidgetState() const = 0;

protected:
	/* Helper-function: inform all listeners about state changes */
	void		GenerateOnStateChanged(WidgetState* state);

	BOOL		IsInited() const;
	void		SetIsInited(BOOL is_inited);

private:
	BOOL		m_is_inited;
	OpListeners<WidgetStateListener>	m_state_listeners; //< All widgets currently listening.
};


#endif // WIDGET_STATE_MODIFIER_H
