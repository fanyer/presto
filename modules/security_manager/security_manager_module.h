/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SECURITY_MANAGER_MODULE_H
#define SECURITY_MANAGER_MODULE_H

#define SECURITY_MANAGER_MODULE_REQUIRED

#include "modules/hardcore/opera/module.h"

class OpSecurityManager;

class SecurityManagerModule : public OperaModule
{
public:
	SecurityManagerModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	OpSecurityManager *secman_instance;
};

#define g_secman_instance g_opera->security_manager_module.secman_instance

#endif // !SECURITY_MANAGER_MODULE_H
