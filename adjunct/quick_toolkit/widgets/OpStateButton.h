/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter
 */

#ifndef OP_STATE_BUTTON_H
#define OP_STATE_BUTTON_H

#include "adjunct/quick_toolkit/widgets/state/WidgetStateModifier.h"
#include "modules/widgets/OpButton.h"

class StateModifier;
class WidgetState;


/***********************************************************************************
**  @class	OpStateButton
**	@brief	A button that can change its state (text, icon, action)
**  @see	WidgetStateListener
**  @see	WidgetStateModifier
**
************************************************************************************/
class OpStateButton : public OpButton, public WidgetStateListener
{
public:
	OpStateButton();
	OpStateButton(const OpStateButton & state_button);
	virtual ~OpStateButton() {}

	BOOL		IsUsingTooltipFromState();
	void		SetIsUsingTooltipFromState(BOOL tooltip_from_state);

	// == OpTypedObject ===========================
	virtual	Type	GetType() {return WIDGET_TYPE_STATE_BUTTON;}

	// == OpWidget ================================
	virtual void	OnAdded();
	virtual void	OnDragStart(const OpPoint& point);
	virtual void	GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	// == WidgetStateListener ======================
	virtual void	OnStateChanged(WidgetState* state);

	// == OpToolTipListener ======================
	virtual BOOL	HasToolTipText(OpToolTip* tooltip);
	virtual void	GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
	virtual INT32	GetToolTipDelay(OpToolTip* tooltip);

	// == OpButton ================================
	virtual void	Click(BOOL plus_action = FALSE);

private:
	OpString		m_tooltip_text;
	BOOL			m_tooltip_from_state; // if the default button tooltip or the WidgetState tooltip text should be used
	WidgetState *	m_state;
};

#endif // OP_STATE_BUTTON_H
