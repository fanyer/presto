/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifdef SETUP_INFO

#include "modules/hardcore/opera/setupinfo.h"

#define TOSTRING(x) UNI_L(#x)

int OpSetupInfo::GetTweakCount()
{
	// <GetTweakCount>
}

OpSetupInfo::Tweak OpSetupInfo::GetTweak(int index)
{
	Tweak t;
	t.name = UNI_L("");
	t.tweaked = FALSE;
	t.description = UNI_L("");
	t.owner = UNI_L("");
	t.module = UNI_L("");
	t.value = UNI_L("");
	t.default_value = UNI_L("");
	switch (index)
	{
	// <GetTweak>
	}

	return t;
}

#endif // SETUP_INFO
