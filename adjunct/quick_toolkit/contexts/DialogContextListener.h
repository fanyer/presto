/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DIALOG_CONTEXT_LISTENER_H
#define DIALOG_CONTEXT_LISTENER_H

class DialogContext;

/**
 * Listens to events related to a DialogContext.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski@opera.com)
 */
class DialogContextListener
{
public:
	virtual ~DialogContextListener() {}

	/**
	 * The dialog window is about to close.
	 *
	 * @param context the DialogContext whose dialog is closing
	 */
	virtual void OnDialogClosing(DialogContext* context) = 0;
};

#endif // DIALOG_CONTEXT_LISTENER_H
