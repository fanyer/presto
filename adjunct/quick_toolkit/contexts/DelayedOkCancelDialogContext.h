/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DELAYED_OK_CANCEL_DIALOG_CONTEXT_H
#define DELAYED_OK_CANCEL_DIALOG_CONTEXT_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"
#include "modules/hardcore/timer/optimer.h"

/**
 * An OkCancelDialogContext that enables OK/Cancel after some delay.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class DelayedOkCancelDialogContext
		: public OkCancelDialogContext
		, public DesktopWindowListener
		, public OpTimerListener
{
public:
	DelayedOkCancelDialogContext();
	virtual ~DelayedOkCancelDialogContext();

	// Overriding UiContext
	virtual BOOL DisablesAction(OpInputAction* action);

	// Overriding DesktopWindowListener
	virtual void OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);

	// Overriding OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

	const OpTimer * GetDisableTimer() const { return m_disable_timer; }
	bool AttachToWindowIfNeeded();

private:
	/** number of ms to wait before enabling OK/Cancel
	  */
	static const unsigned ENABLE_AFTER = 750;

	bool m_attached_to_window;
	OpTimer* m_disable_timer;
};

#endif // DELAYED_OK_CANCEL_DIALOG_CONTEXT_H
