/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * George Refseth, rfz@opera.com
*/
#ifndef REMOTEURLHANDLER_H_
#define REMOTEURLHANDLER_H_

# ifdef DU_REMOTE_URL_HANDLER
#  include "adjunct/quick/data_types/OpenURLSetting.h"

namespace RemoteURLHandler
{
	// Method to add a URL (string) over resources that we want to keep tabs on
	// in order to detect call loops which could turn into neverending loops
	// and ultimately crash opera as we run out of resources
	void AddURLtoList(const OpStringC url, time_t timeout = 0);
	
	// Method to test if a URL (string in setting) are registred as a resource
	// that we execute an application to run
	BOOL CheckLoop(OpenURLSetting &setting);

	// explisitly clean up list of urls...
	void FlushURLList();
};

# endif // DU_REMOTE_URL_HANDLER
#endif /*REMOTEURLHANDLER_H_*/
