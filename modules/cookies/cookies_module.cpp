/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/cookies/cookies_module.h"
#include "modules/cookies/cookie_common.h"

/* virtual */ void
CookiesModule::InitL(const OperaInitInfo& info)
{
	cookie_header_generation_buffer = OP_NEWA_L(char, COOKIE_BUFFER_SIZE);
}

/* virtual */ void
CookiesModule::Destroy()
{
	OP_DELETEA(cookie_header_generation_buffer);
	cookie_header_generation_buffer = NULL;
}
