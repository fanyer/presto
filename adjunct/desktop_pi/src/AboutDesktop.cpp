/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/desktop_pi/AboutDesktop.h"

// We want to have one file that defines VER_BUILD_NUMBER_STR
#if defined(MSWIN)
# include "platforms/windows/windows_ui/res/#buildnr.rci"
#elif defined(UNIX)
# include "platforms/unix/product/config.h"
#elif defined(_MACINTOSH_)
# include "platforms/mac/Resources/buildnum.h"
#else
# pragma error("include the file where VER_BUILD_NUMBER is defined")
#endif

#ifdef VER_BETA
# define ABOUT_VERSION_STRING UNI_L(VER_NUM_STR) UNI_L(" ") VER_BETA_NAME
#else
# define ABOUT_VERSION_STRING VER_NUM_STR
#endif

OP_STATUS AboutDesktop::GetVersion(OpString *dest)
{
	return dest->Set(ABOUT_VERSION_STRING);
}

OP_STATUS AboutDesktop::GetBuild(OpString *dest)
{
	return dest->Set(VER_BUILD_NUMBER_STR);
}
