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

#include "adjunct/quick/controller/FeatureController.h"

/***********************************************************************************
**  FeatureController::BroadcastOnFeatureEnablingSucceeded
************************************************************************************/
void 
FeatureController::BroadcastOnFeatureEnablingSucceeded()
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFeatureEnablingSucceeded();
	}
}


/***********************************************************************************
**  FeatureController::BroadcastOnFeatureSettingsChangeSucceeded
************************************************************************************/
void
FeatureController::BroadcastOnFeatureSettingsChangeSucceeded()
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFeatureSettingsChangeSucceeded();
	}
}
