/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software ASa.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef PREFS_DELAYED_INIT_H
#define PREFS_DELAYED_INIT_H

// Check if we need the delayed init
#ifdef PREFS_VALIDATE
# define PREFS_DELAYED_INIT_NEEDED
#endif

#ifdef PREFS_DELAYED_INIT_NEEDED
# include "modules/hardcore/mh/messobj.h"

class PrefsDelayedInit : private MessageObject
{
public:
	PrefsDelayedInit();				///< Public constructor.
	virtual ~PrefsDelayedInit();	///< Public destructor.

	/**
	 * Start the delayed initialization. This will post a message to the
	 * message handler to call us later on when the Opera core is configured
	 * and ready.
	 */
	void StartL();

private:
	// Inherited interfaces from MessageObject:
	virtual void HandleCallback(OpMessage, MH_PARAM_1, MH_PARAM_2);
};

#endif // PREFS_DELAYED_INIT_NEEDED
#endif // PREFS_DELEYED_INIT_H
