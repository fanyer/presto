/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Bartek Przybylski (bprzybylski)
 */

#ifndef MAC_ASYNC_LOADER_H
#define MAC_ASYNC_LOADER_H

// MacAsyncLoader is class which should be used for resources
// which can require longer loading, but which are not necessary
// for startup
class MacAsyncLoader {
public:

	// function wich should be called by child thread
	static void* LoadElements(void*);
private:

	// This function load sharing services
	// first call always takes some time, thus it's wanted to load
	// those services asynchronously
	static void LoadSharingServices();
};

#endif  // MAC_ASYNC_LOADER_H