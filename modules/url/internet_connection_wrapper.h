/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _URL_NET_CONN_WRAPPER_H_
#define _URL_NET_CONN_WRAPPER_H_

#ifdef PI_INTERNET_CONNECTION

#include "modules/pi/network/OpInternetConnection.h"

/**
 * A listener for OpInternetConnection, which maintains a list of listeners 
 * that should be updated when the internet connection status changes.  
 */
class InternetConnectionWrapper : public OpInternetConnectionListener
{
public:
	InternetConnectionWrapper();
	~InternetConnectionWrapper();

	/**
	 * Initialises the object.  Must be called before RequestConnection is called.
	 */
	OP_STATUS Init();

	/**
	 * Called by the OpInternetConnection implementation when the platform 
	 * notifies Opera of the network status.
	 * @param connected TRUE if the network is up, otherwise FALSE.
	 */
	void OnConnectionEvent(BOOL connected);

	/**
	 * Request that an internet connection be brought up, and a listener notified 
	 * when the network status changes. 
	 * @param listener The OpInternetConnectionListener that has requested that the 
	 * network be brought up.  
	 */
	OP_STATUS RequestConnection(OpInternetConnectionListener *listener);

	/**
	 * Returns TRUE if we think that the network is up (based on information provided 
	 * by the platform, otherwise FALSE.
	 */
	BOOL IsConnected();

private:
	void AddListener(OpInternetConnectionListener *listener);
	void RemoveListener(OpInternetConnectionListener *listener);

	OpInternetConnection *m_internet_connection;
	List<OpInternetConnectionListener> m_listeners;
	BOOL m_is_connected;
};

#endif // PI_INTERNET_CONNECTION

#endif // _URL_NET_CONN_WRAPPER_H_
