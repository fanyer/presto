/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#ifndef MODULES_GEOLOCATION_SELFTEST_HELPERS_H
#define MODULES_GEOLOCATION_SELFTEST_HELPERS_H

#ifdef GEOLOCATION_SUPPORT

#include "modules/pi/OpGeolocation.h"

class OpGeolocationSelftestDataProvider : public OpGeolocationDataProviderBase
{
public:
	virtual ~OpGeolocationSelftestDataProvider() {}

	static OP_STATUS Create(OpGeolocationDataProviderBase *& new_obj, OpGeolocationDataListener *listener);

	OP_STATUS Poll();

	OpGeolocationDataListener *m_listener;
};

#endif // GEOLOCATION_SUPPORT

#endif // MODULES_GEOLOCATION_SELFTEST_HELPERS_H
