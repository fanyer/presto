/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_UPDATETOOLBAR_H
#define OP_UPDATETOOLBAR_H

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

class WidgetUpdater;

/***********************************************************************************
**
**	OpUpdateToolbar
**
**  Toolbar that asks user if he wants to update widget
**
**
***********************************************************************************/

class OpUpdateToolbar : public OpToolbar
{
public:

	OpUpdateToolbar(WidgetUpdater& updater);
	~OpUpdateToolbar(){};

	void Show();
	void Hide(BOOL focus_page = TRUE);

	// == Hooks ======================
	virtual void		OnReadContent(PrefsSection *section);

	// == OpInputContext ======================

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Update tool bar";}

private:
	WidgetUpdater* m_updater;
};

#endif // OP_UPDATETOOLBAR_H
