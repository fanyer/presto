/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef FAKE_FEATURE_SETTINGS_CONTEXT_H
#define FAKE_FEATURE_SETTINGS_CONTEXT_H

#include "adjunct/quick/controller/FeatureSettingsContext.h"
#include "modules/locale/locale-enum.h"

class FakeFeatureSettingsContext : public FeatureSettingsContext
{
	virtual FeatureType			GetFeatureType() const		{ return FeatureTypeSelftest; }
	virtual Str::LocaleString	GetFeatureStringID() const	{ return Str::S_OPERA_LINK; } // don't have a fake string here..
	virtual Str::LocaleString	GetFeatureLongStringID() const	{ return Str::S_OPERA_LINK; } // don't have a fake string here..
};

#endif // FAKE_FEATURE_SETTINGS_CONTEXT_H
