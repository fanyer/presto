/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter
 */

#ifndef WIDGET_STATE_H
#define WIDGET_STATE_H

class OpInputAction;

/***********************************************************************************
**  @class WidgetState
**	@brief Interface representing the state that a widget is currently in.
**
**  WidgetStates are used by the WidgetStateModifier to inform widgets about the fact
**  that their state has changed. Currently, widget state implementations assume that core has been
**  initialized before they are used (actions are created, strings initialized during creation).
**
**  @see WidgetStateModifier
**
************************************************************************************/

class WidgetState
{
public:
	WidgetState() : m_attention(false) {}
	virtual ~WidgetState()             {}

	virtual const uni_char*      GetText()            const = 0;
	virtual const OpInputAction* GetAction()          const = 0;
	virtual const char*          GetForegroundImage() const = 0;
	virtual const uni_char*      GetTooltipText()           = 0;
	virtual const char*          GetStatusImage()     const = 0;

	virtual bool                 HasAttention()       const   { return m_attention; }
	virtual void                 SetAttention(bool attention) { m_attention = attention; }

private:
	bool m_attention;
};

#endif // WIDGET_STATE_H
