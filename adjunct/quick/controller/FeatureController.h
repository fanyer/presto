/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef FEATURE_CONTROLLER_H
#define FEATURE_CONTROLLER_H

#include "modules/util/adt/oplisteners.h"

class FeatureSettingsContext;


/***********************************************************************************
**  @class	FeatureControllerListener
**	@brief	Listener for UI elements to listen to feature settings changes success/failure.
************************************************************************************/
class FeatureControllerListener
{
public:
	virtual			~FeatureControllerListener() {}

	virtual void	OnFeatureEnablingSucceeded() = 0;
	virtual void	OnFeatureSettingsChangeSucceeded() = 0;
};

/***********************************************************************************
**  @class	FeatureController
**	@brief	Facade, access point for feature UI (sync, webserver) to account
**          and feature setup/settings related functions. Must be sub-classed
**          per feature.
************************************************************************************/
class FeatureController
{
public:
	virtual			~FeatureController() {}

	// Listener functions for this classes internal listener
	OP_STATUS			AddListener(FeatureControllerListener* listener)	{ return m_listeners.Add(listener); }
	OP_STATUS			RemoveListener(FeatureControllerListener* listener)	{ return m_listeners.Remove(listener); }

	virtual BOOL	IsFeatureEnabled() const = 0;
	virtual void	EnableFeature(const FeatureSettingsContext* settings) = 0;
	virtual void	DisableFeature() = 0;
	virtual void	SetFeatureSettings(const FeatureSettingsContext* settings) = 0;

	// forward common messages to the appropriate place
	virtual void	InvokeMessage(OpMessage msg, MH_PARAM_1 param_1, MH_PARAM_2 param_2) = 0;

protected:
	OpListeners<FeatureControllerListener>&	GetListeners() { return m_listeners; }

	// helper functions to broadcast to UI
	void			BroadcastOnFeatureEnablingSucceeded();
	void			BroadcastOnFeatureSettingsChangeSucceeded();

private:
	OpListeners<FeatureControllerListener>	m_listeners;
};

#endif // FEATURE_CONTROLLER_H
