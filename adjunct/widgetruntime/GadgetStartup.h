/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_STARTUP_H
#define	GADGET_STARTUP_H

#ifdef WIDGET_RUNTIME_SUPPORT

/**
 * Functions related to the startup of standalone gadgets.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski@opera.com)
 */
namespace GadgetStartup
{
	/**
	 * Was the executable started with the intention to run a gadget?
	 *
	 * @return @c TRUE iff the executable was started with the intention to run
	 * 		a gadget
	 */
	BOOL IsGadgetStartupRequested();

	/**
	 * Was the executable started with the intention to run a browser?
	 *
	 * @return @c TRUE if the executable was started with the intention to run
	 * 		browser
	 */
	BOOL IsBrowserStartupRequested();

	OP_STATUS GetRequestedGadgetFilePath(OpString& path);

	/**
	 * Determines the name of the Opera profile to use for a standalone gadget
	 * or the gadget installer/manager.
	 *
	 * @param profile_name receives the profile name if a standalone gadget or
	 * 		the gadget installer/manager is to be started
	 * @return @c OpStatus::OK if the custom profile was determined successfully
	 */
	OP_STATUS GetProfileName(OpString& profile_name);

	/**
	 * Handles failure to start up properly caused by an invalid or non-existent
	 * gadget.
	 *
	 * @param gadget_path the path to the gadget file
	 * @return status
	 */
	OP_STATUS HandleStartupFailure(const OpStringC& gadget_path);
}

#endif // WIDGET_RUNTIME_SUPPORT

#endif // !GADGET_STARTUP_H
