/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_SPATIAL_NAVIGATION_SPATIAL_NAVIGATION_MODULE_H
#define MODULES_SPATIAL_NAVIGATION_SPATIAL_NAVIGATION_MODULE_H

#ifdef _SPAT_NAV_SUPPORT_

#if 0 // this class is not needed at the moment
class SpatialNavigationModule : public OperaModule
{
public:
	SpatialNavigationModule() {}
	~SpatialNavigationModule() {}

	virtual void InitL(const OperaInitInfo& info) {}
	virtual void Destroy() {}
};

#define SPATIAL_NAVIGATION_MODULE_REQUIRED
#endif // 0
#endif // _SPAT_NAV_SUPPORT_

#endif // !MODULES_SPATIAL_NAVIGATION_SPATIAL_NAVIGATION_MODULE_H
