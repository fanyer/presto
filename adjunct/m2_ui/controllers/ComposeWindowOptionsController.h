/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 */

#ifndef COMPOSE_WINDOW_OPTIONS_CONTROLLER_H
#define COMPOSE_WINDOW_OPTIONS_CONTROLLER_H

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"

class ComposeWindowOptionsController : public PopupDialogContext
{
public:
	ComposeWindowOptionsController(OpWidget* anchor_widget): PopupDialogContext(anchor_widget) {}
	virtual ~ComposeWindowOptionsController() {}

	// UiContext
	virtual BOOL			CanHandleAction(OpInputAction* action) { return action->GetAction() == OpInputAction::ACTION_OPEN_SIGNATURE_DIALOG; }
	virtual OP_STATUS		HandleAction(OpInputAction* action);

private:
	// DialogContext
	virtual void			InitL();

	void	UpdateShowFlags(bool);
	void	UpdateExpandedDraft(bool expanded);

	OpProperty<bool>		m_expanded_draft;
	OpProperty<bool>		m_show_flags[AccountTypes::HEADER_DISPLAY_LAST];
};


#endif // M2_SUPPORT
#endif // COMPOSE_WINDOW_OPTIONS_CONTROLLER_H
