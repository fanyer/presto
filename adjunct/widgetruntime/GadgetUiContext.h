/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGET_UI_CONTEXT_H
#define	GADGET_UI_CONTEXT_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick_toolkit/contexts/UiContext.h"

class Application;
class ClassicGlobalUiContext;
class GadgetHelpLoader;
class GadgetRemoteDebugHandler;


/**
 * Feature specific UI context for gadgets.
 *
 * FIXME: Refactor once the feature context framework is integrated.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetUiContext : public UiContext
{
public:
	explicit GadgetUiContext(Application& application);
	~GadgetUiContext();

	/**
	 * Tells the input context to start or stop handling actions.
	 */
	void SetEnabled(BOOL enabled);

	virtual const char*	GetInputContextName()
		{ return "Gadget Application"; }

	virtual BOOL OnInputAction(OpInputAction* action);

private:
	/**
	 * FIXME: These are the real gadget-specific actions.  The goal is that
	 * and only these actions are handled in this context.
	 */
	BOOL MaybeHandleGadgetAction(OpInputAction* action);

	/**
	 * FIXME: These are basically browser-specific actions.  Instead of
	 * filtering, they should be seperated into a browser UI context.
	 */
	BOOL MaybeFilterBrowserAction(OpInputAction* action);

	ClassicGlobalUiContext* m_global_context;
	GadgetHelpLoader* m_help_loader;
	GadgetRemoteDebugHandler* m_debug_handler;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_UI_CONTEXT_H
