/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"


#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/applicationcache/src/application_cache_common.h"
#include "modules/applicationcache/application_cache_manager.h"

UpdateAlgorithmArguments::~UpdateAlgorithmArguments()
{
	if (InList())
		Out();

	if (m_owner)
		m_owner->SetRestartArguments(NULL);
}


#endif // APPLICATION_CACHE_SUPPORT

