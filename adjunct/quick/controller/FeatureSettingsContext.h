/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef FEATURE_SETTINGS_CONTEXT_H
#define FEATURE_SETTINGS_CONTEXT_H

/***********************************************************************************
**  @enum	FeatureType
**	@brief	Type of the feature.
************************************************************************************/
enum FeatureType
{
	FeatureTypeSelftest,
	FeatureTypeWebserver,
	FeatureTypeSync
};


/***********************************************************************************
**  @class	FeatureSettingsContext
**	@brief	Interface for specific feature settings contexts.
************************************************************************************/
class FeatureSettingsContext
{
public:
	FeatureSettingsContext();
	virtual ~FeatureSettingsContext() {}

	virtual FeatureType			GetFeatureType() const = 0;
	virtual Str::LocaleString	GetFeatureStringID() const = 0;
	virtual Str::LocaleString	GetFeatureLongStringID() const = 0;

	BOOL						IsFeatureEnabled() const;
	void						SetIsFeatureEnabled(BOOL enabled);

private:
	BOOL	m_enabled;
};

#endif // FEATURE_SETTINGS_CONTEXT_H
