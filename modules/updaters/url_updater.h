/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _URL_UPDATE_H_
#define _URL_UPDATE_H_

#if defined UPDATERS_ENABLE_URL_AUTO_FETCH 
#include "modules/url/url2.h"

#include "modules/updaters/update_man.h"

#include "modules/url/url_docload.h"

/** Basic implementation for downloading and processing a URL resource
 *
 *	Usage:
 *		- Construct the object specifying the URL top be loaded and the finished message
 *		- Hand over to manager, or start loading directly
 *		- When loaded and (optionally verified) the implementation's ResourceLoaded() function is called
 *			- The implementation then processes the received data 
 *			- When finished it must return OpStatus::OK or an error, SetFinished is called by the caller.
 *			- The function MUST complete in a single step
 */
class URL_Updater : public AutoFetch_Element
	, public URL_DocumentLoader
{
private:
	/** Update loader URL */
	URL update_loader;

	/** Update URL loader unload lock */
	URL_InUse update_url;

	/** Finished ? */
	BOOL finished;

	/** Loadpolicy */
	URL_Reload_Policy load_policy;

public:

	/** Constructor */
	URL_Updater();

	/** Destructor */
	~URL_Updater();

protected:
	/** Construct the updater object, and prepare the URL and callback message*/
	OP_STATUS Construct(URL &url, OpMessage fin_msg, MessageHandler *mh=NULL);

	/** Process the loaded file. Implemented in the application subclass.
	 *  The loaded URL is supplied as an argument.
	 *	Must complete with OpStatus::OK or an error code 
	 *	the parser member is initialized when this function is entered
	 */
	virtual OP_STATUS ResourceLoaded(URL &url)=0;

	/** Mark the updater as finished with a flag marking the succes or failure of the operation */
	virtual void SetFinished(BOOL success);

public:

	/** Start update process */
	virtual OP_STATUS StartLoading();

	/** Are we finished? */
	BOOL Finished(){return finished;};

	/** Set the reload policy for the URL */
	void SetLoadPolicy(URL_Reload_Policy pol){load_policy = pol;}

	virtual void OnAllDocumentsFinished();
};
#endif

#endif // _XML_UPDATE_H_

