/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef NON_BROWSER_APPLICATION_H
#define NON_BROWSER_APPLICATION_H

#include "adjunct/quick/Application.h"

class OpPoint;
class OpRect;


class NonBrowserApplication
{
public:
	virtual ~NonBrowserApplication() {}

	/**
	 * @return an OpUiWindowListener object that should be used in this
	 * application.  The caller is responsible for deletion.
	 */
	virtual OpUiWindowListener* CreateUiWindowListener() = 0;

	/**
	 * @return an OpAuthenticationListener object that should be used in this
	 * application.  Caller is responsible for deletion.
	 */
	virtual DesktopOpAuthenticationListener* CreateAuthenticationListener() = 0;

	/**
	 * Indicates to the application that it's environment is now completely
	 * set up and it can start operation.
	 */
	virtual OP_STATUS Start() = 0;

	/**
	 * @return A DesktopMenuHandler that should be used in this application.
	 */
	virtual DesktopMenuHandler* GetMenuHandler() = 0;

	virtual DesktopWindow* GetActiveDesktopWindow(BOOL toplevel_only) = 0;

	virtual BOOL HasCrashed() const = 0;

	virtual Application::RunType DetermineFirstRunType() = 0;

	virtual BOOL IsExiting() const = 0;

	virtual OperaVersion GetPreviousVersion() = 0;

	/**
	 * @return this application's global input context
	 */
	virtual UiContext* GetInputContext() = 0;

	/**
	 * Establishes this application's global input context.
	 *
	 * @param input_context this application's global input context
	 */
	virtual void SetInputContext(UiContext& input_context) = 0;

	virtual BOOL OpenURL(const OpStringC &address) = 0;
};

#endif // NON_BROWSER_APPLICATION_H
