/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_NETWORK_SELFTEST_MODULE_H
// Not yet included
#ifdef SELFTEST
#define MODULES_NETWORK_SELFTEST_MODULE_H

class NetworkSelftestModule : public OperaModule
{
public:
public:
	NetworkSelftestModule(){};
	virtual ~NetworkSelftestModule(){};

	virtual void InitL(const OperaInitInfo& info){};
	virtual void Destroy(){};
};

#define NETWORK_SELFTEST_MODULE_REQUIRED

#endif // SELFTEST
#endif // !MODULES_NETWORK_SELFTEST_MODULE_H
