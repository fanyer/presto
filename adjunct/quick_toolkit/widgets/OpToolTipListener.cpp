/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpToolTipListener.h"
#include "adjunct/quick_toolkit/widgets/OpToolTip.h"
#include "adjunct/quick/Application.h"

OpToolTipListener::~OpToolTipListener()
{
	if (g_application && this == g_application->GetToolTipListener())
	{
		g_application->SetToolTipListener(NULL);
	}
}

INT32 OpToolTipListener::GetToolTipDelay(OpToolTip* tooltip)
{
	if(tooltip && tooltip->HasImage())
	{
		return 200;
	}
	else
	{
		return 600;
	}
}
