/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef FAKE_FEATURE_CONTROLLER_H
#define FAKE_FEATURE_CONTROLLER_H

#include "adjunct/quick/controller/FeatureController.h"

class FakeFeatureController : public FeatureController
{
public:
	FakeFeatureController() : m_enabled(FALSE) {}

	virtual BOOL	IsFeatureEnabled() const
					{ return m_enabled; }

	virtual void	EnableFeature(const FeatureSettingsContext* context)
					{ m_enabled = TRUE; }

	virtual void	DisableFeature()
					{ m_enabled = FALSE; }

	virtual void	SetFeatureSettings(const FeatureSettingsContext* context)
					{ /*stub*/ }

	virtual void	InvokeMessage(OpMessage msg, MH_PARAM_1 param_1, MH_PARAM_2 param_2) { /* stub */}

private:
	BOOL					m_enabled;
};


#endif // FAKE_FEATURE_CONTROLLER_H
