/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/DelayedAction.h"

DelayedAction::DelayedAction(UINT32 timeout_ms)
	: mOpTimer(0)
{
	mOpTimer = new OpTimer();
	if (mOpTimer) {
		mOpTimer->SetTimerListener(this);
		mOpTimer->Start(timeout_ms);
	}
}

DelayedAction::~DelayedAction()
{
	delete mOpTimer;
}

void DelayedAction::OnTimeOut(OpTimer* timer)
{
	DoAction();
	delete this;
}

