/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef WIZARD_CONTEXT_H
#define WIZARD_CONTEXT_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class QuickPagingLayout;

/**
 * A DialogContext suitable for use with wizards.
 *
 * Implements a default handling of the Previous, Next, and Cancel actions. Changes entered
 * in the dialog affect the model immediately, but are reverted if the user
 * discards them with Cancel.
 *
 * @author Manuela Hutter (manuelah)
 */
class WizardContext : public OkCancelDialogContext
{
public:
	WizardContext() : m_pages(NULL) {}

	// Overriding UiContext
	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual BOOL DisablesAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);

protected:
	OP_STATUS ShowWidget(const OpStringC8& name);
	OP_STATUS HideWidget(const OpStringC8& name);

private:
	bool HasPreviousPage();
	bool HasNextPage();
	void GoToPreviousPage();
	void GoToNextPage();

	QuickPagingLayout* GetPages();

	QuickPagingLayout* m_pages;
};

#endif // WIZARD_CONTEXT_H
