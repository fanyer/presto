/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick/controller/FeatureSettingsContext.h"


/***********************************************************************************
**  FeatureSettingsContext::FeatureSettingsContext
**  
************************************************************************************/
FeatureSettingsContext::FeatureSettingsContext() : 
	m_enabled(FALSE)
{
}


/***********************************************************************************
**  FeatureSettingsContext::FeatureSettingsContext
**  
************************************************************************************/
BOOL FeatureSettingsContext::IsFeatureEnabled() const
{
	return m_enabled;
}


/***********************************************************************************
**  FeatureSettingsContext::SetIsFeatureEnabled
**  
************************************************************************************/
void FeatureSettingsContext::SetIsFeatureEnabled(BOOL enabled)
{
	m_enabled = enabled;
}
