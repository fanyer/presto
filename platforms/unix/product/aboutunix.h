/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef ABOUTUNIX_H
#define ABOUTUNIX_H

#include "adjunct/desktop_pi/AboutDesktop.h"

class AboutUnix : public AboutDesktop
{
public:
	AboutUnix(URL &url) : AboutDesktop(url) {}

protected:
	// Inherited interfaces
	OP_STATUS GetPlatform(OpString *platform, OpString *version);

# if defined HAVE_DISK && defined PREFS_READ
	OP_STATUS GetPreferencesFile(OpString *dest);
# endif
};

#endif // ABOUTUNIX_H
