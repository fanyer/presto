/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef NAME_COMPLETION_CONTROLLER_H
#define NAME_COMPLETION_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class NameCompletionController : public OkCancelDialogContext
{
public:
	void InitL();
	
private:
	OP_STATUS InitOptions();
};

#endif // NAME_COMPLETION_CONTROLLER_H
