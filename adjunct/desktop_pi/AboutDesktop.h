/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef ABOUT_DESKTOP_H
#define ABOUT_DESKTOP_H

#include "modules/about/operaabout.h"

class AboutDesktop : public OperaAbout
{
public:
	AboutDesktop(URL &url) : OperaAbout(url) {}

protected:
	virtual OP_STATUS GetVersion(OpString *dest);
	virtual OP_STATUS GetBuild(OpString *dest);
};

#endif
