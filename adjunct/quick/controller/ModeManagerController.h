/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */

#ifndef MODE_MANAGER_CONTROLLER_H
#define MODE_MANAGER_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class ModeManagerController : public OkCancelDialogContext
{
private: 
	virtual void InitL();
    OP_STATUS InitOptions();
};

#endif //MODE_MANAGER_CONTROLLER_H
