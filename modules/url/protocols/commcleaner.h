/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef COMMCLEANER_H
#define COMMCLEANER_H

#include "modules/hardcore/mh/messages.h"

#include "modules/url/protocols/comm.h"

/**
 *	Call the Comm cleanup functions up to 10 times a second.  This class
 *	should be instansiated after the MessageHandler is ready, and
 *	destroyed before the MessageHandler is destroyed.
 *
 *	The Comm and SComm classes signal the  g_comm_cleaner object each time 
 *	they do something in the waitinglists, and a 100ms delayed message will 
 *	be posted. The message will be handled later. Only one message can be posted each cycle.
 */
class CommCleaner : public MessageObject 
{
private:
	BOOL posted_message;	
	
private:
	/**
	* Request a callback in 1/10 second.
	*/
	void postL();
public:
	CommCleaner() {	}

	/**
	 * Register the callback and request a callback.
	 * @see postL()
	 * @see ~CommCleaner()
	 */
	void ConstructL();

	/**
	 * Unregister the callback
	 */
	virtual ~CommCleaner();

	/**
	 * Call the Comm class cleanup methods and request a new callback.
	 *
	 * Error: If postL() fails, there is no way for this function to
	 * report the error!
	 *
	 * @see postL()
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** Signal from SComm and Comm that they have updated the wait lists, and that they need handling */
	void SignalWaitElementActivity();
};

#ifndef URL_MODULE_REQUIRED
extern CommCleaner *g_comm_cleaner;
#endif

#endif // COMMCLEANER_H
