/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon / Arjan van Leeuwen (arjanl)
 */


#ifndef DESKTOP_GLOBAL_APPLICATION_H
#define DESKTOP_GLOBAL_APPLICATION_H

/** @brief Platform hooks for events that affect the Opera application
  */
class DesktopGlobalApplication
{
public:
	virtual ~DesktopGlobalApplication() {}

	/** Create a DesktopGlobalApplication object. */
	static OP_STATUS Create(DesktopGlobalApplication** desktop_application);

	/** Called when application starts */
	virtual void OnStart() = 0;

	/** Application has started the exit sequence, please ensure no new windows can be opened */
	virtual void ExitStarted() = 0;
	
	/**
	 * Hook for interfacing with system (OS) session manager. To be used when
	 * there is user interaction involved (a dialog) when shutting down Opera.
	 *
	 * @param exit TRUE if user selected to exit Opera, otherwise FALSE
	 *
	 * @return TRUE if system session manager decidced to manage exit or the cancellation.
	 *         Opera shall then not attempt to shut down or restore unless told so from 
	 *         the manager
	 */
	virtual BOOL OnConfirmExit(BOOL exit) = 0;

	/** Make the application exit */
	virtual void Exit() = 0;

	/** Called when application is being hidden */
	virtual void OnHide() = 0;

	/** Called when application is being shown */
	virtual void OnShow() = 0;

	/** Optional method called when the application has started and is able to handle actions */
	virtual void OnStarted() {}
};

#endif // DESKTOP_GLOBAL_APPLICATION_H
