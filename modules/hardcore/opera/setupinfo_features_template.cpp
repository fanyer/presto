/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifdef SETUP_INFO

#include "modules/hardcore/opera/setupinfo.h"

int OpSetupInfo::GetFeatureCount()
{
	// <GetFeatureCount>
}

OpSetupInfo::Feature OpSetupInfo::GetFeature(int index)
{
	Feature f;
	f.name = UNI_L("");
	f.enabled = FALSE;
	f.description = UNI_L("");
	f.owner = UNI_L("");
	switch (index)
	{
	// <GetFeature>
	}

	return f;
}

#endif // SETUP_INFO
