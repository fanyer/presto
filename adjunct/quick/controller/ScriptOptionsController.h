/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */

#ifndef SCRIPT_OPTIONS_CONTROLLER_H
#define SCRIPT_OPTIONS_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class ScriptOptionsController : public OkCancelDialogContext
{
protected:
	virtual void OnOk();

private: 
	virtual void InitL();
    OP_STATUS InitOptions();
	
	OpProperty<bool> m_open_js_console;
	OpProperty<OpString> m_js_filepath;
};

#endif //SCRIPT_OPTIONS_CONTROLLER_H
