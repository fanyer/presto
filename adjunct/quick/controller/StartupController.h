/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef STARTUP_CONTROLLER_H
#define STARTUP_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class StartupController : public OkCancelDialogContext
{
public:
	StartupController();

protected:
	virtual void OnOk();	
	virtual void OnCancel();

private:
	virtual void InitL();

	OP_STATUS InitOptions();
	OP_STATUS InitTreeView();
	
	const TypedObjectCollection* m_widgets;

	OpProperty<bool> m_restart_gadgets;
	OpProperty<bool> m_restart_gadgets_enabled;
	OpProperty<bool> m_have_sessions;
	OpProperty<bool> m_have_autosave;
	OpProperty<bool> m_dont_show_again;
	
	SimpleTreeModel m_sessions_model;
};

#endif
