/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter
 */


#ifndef FAKE_WIDGET_STATE_MODIFIER_H
#define FAKE_WIDGET_STATE_MODIFIER_H

#ifdef SELFTEST

#include "adjunct/quick_toolkit/widgets/state/WidgetState.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetStateListener.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetStateModifier.h"

/***********************************************************************************
**  @class FakeEmptyWidgetState
**	@brief Empty widget state, used in selftests
************************************************************************************/
class FakeEmptyWidgetState : public WidgetState
{
public:
	virtual const uni_char*      GetText() const            { return NULL; }
	virtual const OpInputAction* GetAction() const          { return NULL; }
	virtual const char*          GetForegroundImage() const { return NULL; }
	virtual const uni_char*      GetTooltipText()           { return NULL; }
	virtual const char*          GetStatusImage() const     { return NULL; }
};


/***********************************************************************************
**  @class FakeWidgetStateListener
**	@brief Fake widget state listener, used in selftests
************************************************************************************/
class FakeWidgetStateListener : public WidgetStateListener
{
public:
	FakeWidgetStateListener() : m_current_state(NULL) {}

	virtual void	OnStateChanged(WidgetState* state)
					{ m_current_state = state; }

	WidgetState*	GetCurrentState()
					{ return m_current_state; }

	void			SetCurrentState(WidgetState* state)
					{ m_current_state = state; } // state is owned by testcase

private:
	WidgetState* m_current_state;
};


/***********************************************************************************
**  @class FakeWidgetStateModifier
**	@brief Fake widget state modifier, used in selftests
************************************************************************************/
class FakeWidgetStateModifier : public WidgetStateModifier
{
public:
	FakeWidgetStateModifier() :
	  m_listener_count(0),
	  m_current_state(NULL)
	{}

  	virtual const char* GetDescriptionString() const { return "Fake"; }

	/* workaround to get the number of listeners */
	virtual OP_STATUS	AddWidgetStateListener(WidgetStateListener* listener)
	{
		OP_STATUS ret_status = WidgetStateModifier::AddWidgetStateListener(listener);
		if (OpStatus::IsSuccess(ret_status))
		{
			m_listener_count++;
		}
		return ret_status;
	}

	/* workaround to get the number of listeners */
	virtual OP_STATUS	RemoveWidgetStateListener(WidgetStateListener* listener)
	{
		OP_STATUS ret_status = WidgetStateModifier::RemoveWidgetStateListener(listener);
		if (OpStatus::IsSuccess(ret_status))
		{
			m_listener_count--;
		}
		return ret_status;
	}

	virtual WidgetState*	GetCurrentWidgetState() const
							{ return m_current_state; }

	void					SetCurrentWidgetState(WidgetState* state)
							{ m_current_state = state; } // state is owned by testcase

	UINT32					GetListenerCount() const
							{ return m_listener_count; }

	virtual WidgetStateModifier::Type	GetModifierType() const
							{ return FakeModifier; }

private:
	/* number of listeners. workaround, as listeners::GetCount() is protected */
	UINT32			m_listener_count;
	WidgetState*	m_current_state;
};

#endif // SELFTEST

#endif // FAKE_WIDGET_STATE_MODIFIER_H
