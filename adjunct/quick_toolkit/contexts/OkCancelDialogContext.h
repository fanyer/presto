/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OK_CANCEL_DIALOG_CONTEXT_H
#define OK_CANCEL_DIALOG_CONTEXT_H

#include "adjunct/quick_toolkit/contexts/DialogContext.h"

/**
 * A DialogContext suitable for use with OK/Cancel-type dialogs.
 *
 * Implements a default handling of the OK and Cancel actions. Changes entered
 * in the dialog affect the model immediately, but are reverted if the user
 * discards them with Cancel.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class OkCancelDialogContext : public DialogContext
{
public:
	OkCancelDialogContext() : m_ok(false), m_hook_called(false) {}

	// Overriding UiContext
	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);
	virtual void OnUiClosing();

protected:
	virtual void OnCancel() {}
	virtual void OnOk() {}

	// Overriding DialogContext
	virtual QuickBinder* CreateBinder();	

private:
	typedef class ReversibleQuickBinder MyBinder;
	MyBinder* GetMyBinder();
	
	void CallOkCancelHook();

	bool m_ok;
	bool m_hook_called;
};

#endif // OK_CANCEL_DIALOG_CONTEXT_H
