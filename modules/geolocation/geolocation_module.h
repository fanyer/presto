/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GEOLOCATION_MODULE_H
#define GEOLOCATION_MODULE_H

#ifdef GEOLOCATION_SUPPORT

#include "modules/hardcore/opera/module.h"

class GeolocationImplementation;
class OpGeolocation;

class GeolocationModule : public OperaModule
{
	GeolocationImplementation *m_pimpl;
public:
	GeolocationModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	OpGeolocation *GetGeolocationSingleton();
};

#define GEOLOCATION_MODULE_REQUIRED

#define g_geolocation g_opera->geolocation_module.GetGeolocationSingleton()

#endif // GEOLOCATION_SUPPORT

#endif // !GEOLOCATION_MODULE_H
