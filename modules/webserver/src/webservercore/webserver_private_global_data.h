/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995 - 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */


#ifndef WEBSERVER_PRIVATE_GLOBAL_DATA_H_
#define WEBSERVER_PRIVATE_GLOBAL_DATA_H_
/* Put in authentication and other private global stuff here*/


/* Should we put the sub server lists in here ? */

#include "modules/webserver/src/resources/webserver_auth.h"

class WebserverPrivateGlobalData
{
public:
	WebserverPrivateGlobalData();
	virtual ~WebserverPrivateGlobalData();

	void SignalOOM();	

	UINT CountRequests() { return m_requestCount++; }	
	
private:
	UINT m_requestCount;

};

#endif /*WEBSERVER_PRIVATE_GLOBAL_DATA_H_*/
