/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPINTERNETCONNECTION_H
#define OPINTERNETCONNECTION_H

#ifdef PI_INTERNET_CONNECTION

#include "modules/util/simset.h"

class OpInternetConnectionListener;

/** Internet Connection
 *
 * This class is used to request an Internet connection, and feed connection 
 * status messages (up/down) to its listener.  
 */
class OpInternetConnection
{
public:
	/** Create and return an OpInternetConnection object.
	 *
	 * @param[out] conn Set to the OpInternetConnection created.
	 * @param listener Listener object that will be notified of connection status changes.
	 */
	static OP_STATUS Create(OpInternetConnection** conn, OpInternetConnectionListener *listener);

	virtual ~OpInternetConnection() {}

	/** Request that an Internet connection is brought online.
	 *
	 * The platform should attempt to bring up a network connection, and subsequently call 
	 * OnConnectionEvent(TRUE) when the network is known to be up, and again whenever the 
	 * network status changes (with the correct input argument TRUE/FALSE).
	 * These calls can either be made synchronously from within RequestConnection(), or 
	 * asynchronously after RequestConnection() has returned.  
	 */
	virtual OP_STATUS RequestConnection() = 0;
};

class OpInternetConnectionListener : public ListElement<OpInternetConnectionListener>
{
public:
	/**
	 * Called by the platform whenever the network connection status changes.  
	 * The value of connected should be TRUE when the network is up, and FALSE 
	 * when it is down.
	 *
	 * Since OpInternetConnection::RequestConnection() can call this method 
	 * synchronously, it is forbidden for this method to delete the object 
	 * that called RequestConnection().  These objects should only be deleted 
	 * asynchronously. 
	 */
	virtual void OnConnectionEvent(BOOL connected) = 0;
};

#endif // PI_INTERNET_CONNECTION

#endif // OPINTERNETCONNECTION_H
