/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _UPDATE_MAN_H_
#define _UPDATE_MAN_H_

#ifdef UPDATERS_ENABLE_MANAGER

#include "modules/hardcore/mh/mh.h"

/** Basic manager for automatic updaters, which only manages the basic 
 *	end of fetch message.
 *
 *	The end of handling message is set during construction, and posted when PostFinished is called
 *
 *	The object have the following phases:
 *
 *		- Construction (with message specified)
 *			a callback must be registered in g_main_message_handler for the specified message
 *		- manager calls StartLoading() to initiate sequence (implemented by subclass)
 *		- Implementation subclass handles loading
 *		- Implementation calls PostFinished() with result after all processing is finished
 *		- Manager deletes the fetcher
 *
 *	If the message is specified the listener must register a callback for
 *	the specified message and element->Id() as par1; par2 contains the result
 */
class AutoFetch_Element : 
	public Link
{
private:
	/** The message to be posted when finished, par1 is the ID, par2 is the result */
	OpMessage finished_msg;
	BOOL finished;
	unsigned int id;

public: 
	/** Constructor */
	AutoFetch_Element():finished_msg(MSG_NO_MESSAGE), finished(FALSE){id = g_updaters_api->NewUpdaterId();}
	/** Construct with message */
	void Construct(OpMessage fin_msg){finished_msg = fin_msg;}
	/** Destructor */
	virtual ~AutoFetch_Element();

	/** Start update process */
	virtual OP_STATUS StartLoading()=0;

	/** Id */
	MH_PARAM_1 Id(){return id;}

	/** Post the finsihed message with the provided result as par2 */
	void PostFinished(MH_PARAM_2 result);

	BOOL IsFinished(){return finished;}

	AutoFetch_Element *Suc(){return (AutoFetch_Element *) Link::Suc();}
	AutoFetch_Element *Pred(){return (AutoFetch_Element *) Link::Pred();}
};

/** Basic manager for handling multiple updaters; used by application using many updaters
 *	
 *	When all pending fetchings are finished a message is posted to the owner if one is specified. 
 *	The manager may also be polled to determine if there are active downloads
 *
 *	HandleCallbacks for updaters' finished message must be registered by the implementation
 *
 *	Use:
 *		- Owner constructs the manager with an optional message, and InitL()s it, 
 *			a callback must be registered in g_main_message_handler for the specified message
 *		- New fetchers are added using using AddUpdater; this will start the updater
 *		- When all updaters are finished the specified message (if any) is posted.
 */
class AutoFetch_Manager : public MessageObject
{
private:

	/** List of active updaters */
	AutoDeleteHead updaters;
	/** Specified finished message */
	OpMessage fin_msg;
	/** ID to send with finished message */
	MH_PARAM_1 fin_id;

public:

	/** Constructor with optional finished message */
	AutoFetch_Manager(OpMessage a_msg=MSG_NO_MESSAGE, MH_PARAM_1 a_id=0):fin_msg(a_msg), fin_id(a_id){};
	/** Destructor */
	virtual ~AutoFetch_Manager(){g_main_message_handler->UnsetCallBacks(this);};

	/** Initialization operation; Must be used; Leaves if error occurs */
	virtual void InitL();

	/** Handle callbacks; default is to consider any message for an ID as a finished message */
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** Is this manager still loading updates? */
	BOOL Active() const {return !updaters.Empty();}

	/** Start loading an autoupdater, and register it if it is a fire-and-almost-forget updater */
	OP_STATUS AddUpdater(AutoFetch_Element *new_updater, BOOL is_started = FALSE);

	/** Remove all loaders */
	void Clear(){updaters.Clear();}

	/** Clean up finished loaders */
#ifdef UPDATERS_ENABLE_MANAGER_CLEANUP
	void Cleanup();
#endif

	virtual void OnAllUpdatersFinished();

	AutoFetch_Element *First(){return (AutoFetch_Element *) updaters.First();}
};

#endif

#endif //_UPDATE_MAN_H_
