/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen
 */

#include "core/pch.h"

#include "adjunct/desktop_util/resources/ResourceDefines.h"

/* static */
StartupType& StartupType::GetInstance()
{ 
	static StartupType s_startup_type; 
	return s_startup_type; 
}

/* static */
RegionInfo& RegionInfo::GetInstance()
{
	static RegionInfo s_region_info;
	return s_region_info;
}
