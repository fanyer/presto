/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author bkazmierczak
 */

#ifndef OP_OVERLAY_WINDOW_H
#define OP_OVERLAY_WINDOW_H

#include "adjunct/quick/managers/AnimationManager.h"


class OpOverlayWindow : public QuickAnimationWindow
{
public:

	virtual void OnAnimationComplete();

	// Override to add animation
	virtual void Close(BOOL immediately = FALSE, BOOL user_initiated = FALSE, BOOL force = TRUE);
	virtual const char*		GetWindowName() = 0;

	// need input context to respond to esc
	virtual BOOL IsInputContextAvailable(FOCUS_REASON reason) { return TRUE; }
	virtual const char*		GetInputContextName() = 0;
	virtual BOOL			OnInputAction(OpInputAction* /*action*/);
	
	virtual bool 			UsingAnimation() = 0;
	virtual INT32			GetAnimationDistance();
};

#endif // OP_OVERLAY_WINDOW_H
