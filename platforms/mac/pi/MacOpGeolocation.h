/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MAC_OP_GEOLOCATION_H
#define MAC_OP_GEOLOCATION_H

#include "modules/pi/OpGeolocation.h"

class MacOpGeolocationWifiDataProvider : public OpGeolocationWifiDataProvider, public MessageObject
{
public:
	MacOpGeolocationWifiDataProvider(OpGeolocationDataListener *listener);
	virtual ~MacOpGeolocationWifiDataProvider();
	virtual OP_STATUS Poll();
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
private:
	static void *worker(void *);
	OpGeolocationDataListener *m_listener;
};

#endif // MAC_OP_GEOLOCATION_H