/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef CLOSE_DIALOG_CONTEXT_H
#define CLOSE_DIALOG_CONTEXT_H

#include "adjunct/quick_toolkit/contexts/DialogContext.h"

class CloseDialogContext : public DialogContext
{
public:
	// Overriding UiContext
	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);
};

#endif // CLOSE_DIALOG_CONTEXT_H
