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

#include "adjunct/quick_toolkit/widgets/state/WidgetStateListener.h"
#include "adjunct/quick_toolkit/widgets/state/WidgetStateModifier.h"


/***********************************************************************************
**  WidgetStateListener::WidgetStateListener
**
************************************************************************************/
WidgetStateListener::WidgetStateListener() :
	m_modifier(NULL)
{
}


/***********************************************************************************
**  WidgetStateListener::WidgetStateListener
**
************************************************************************************/
WidgetStateListener::WidgetStateListener(const WidgetStateListener & state_listener) :
	m_modifier(state_listener.m_modifier)
{
}


/***********************************************************************************
**  Destructor. Remove from listener list.
**
**  WidgetStateListener::~WidgetStateListener
**
************************************************************************************/
WidgetStateListener::~WidgetStateListener()
{
	if (m_modifier)
	{
		m_modifier->RemoveWidgetStateListener(this);
	}
}

/***********************************************************************************
**  WidgetStateListener::SetModifier
**  @param modifier	The class that is modifying the listener's state.
**  
************************************************************************************/
void WidgetStateListener::SetModifier(WidgetStateModifier* modifier)
{
	if (m_modifier)
	{
		m_modifier->RemoveWidgetStateListener(this);
	}
	m_modifier = modifier;
	if (m_modifier != NULL)
	{
		m_modifier->AddWidgetStateListener(this);
	}
}
