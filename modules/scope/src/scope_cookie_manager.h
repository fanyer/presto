/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_COOKIE_MANAGER_H
#define SCOPE_COOKIE_MANAGER_H

#ifdef SCOPE_COOKIE_MANAGER

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_cookie_manager_interface.h"

class OpScopeCookieManager
	: public OpScopeCookieManager_SI
{
public:
	OpScopeCookieManager();
	virtual ~OpScopeCookieManager();

	OP_STATUS SetCookieValue(Cookie& cookie_out, ::Cookie& cookie);

	// Request/Response functions
	virtual OP_STATUS DoGetCookie(const GetCookieArg &in, CookieList &out);
	virtual OP_STATUS DoRemoveCookie(const RemoveCookieArg &in);
	virtual OP_STATUS DoRemoveAllCookies();
	virtual OP_STATUS DoGetCookieSettings(CookieSettings &out);
	virtual OP_STATUS DoAddCookie(const AddCookieArg &in);
};

#endif // SCOPE_COOKIE_MANAGER
#endif // SCOPE_COOKIE_MANAGER_H
