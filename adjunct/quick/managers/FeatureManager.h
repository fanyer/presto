// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __FEATURE_MANAGER_H__
#define __FEATURE_MANAGER_H__

#include "adjunct/quick/managers/DesktopManager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FeatureManager : public DesktopManager<FeatureManager>
{
public:
	enum Names
	{
		Sync,
		OperaAccount
	};

	FeatureManager();
	~FeatureManager();
	virtual OP_STATUS Init() { return OpStatus::OK; }

	/**
		Checks if a feature is enabled based on some conditions
	**/
	BOOL IsEnabled(Names feature_name);
};

#endif // __FEATURE_MANAGER_H__
