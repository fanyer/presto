/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author bkazmierczak
 */

#include "core/pch.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/widgets/OpOverlayWindow.h"
#include "adjunct/quick/animation/QuickAnimation.h"

#define ANIMATION_DISTANCE -15;

void OpOverlayWindow::OnAnimationComplete()
{
	QuickAnimationWindowObject::OnAnimationComplete();
	if (m_close_on_complete)
	{
		Show(FALSE);
		// Do not close *immediately* here, g_animation_manager->advanceAnimations() could be
		// called everywhere, possibly before paiting the window etc, deleting the window
		// will cause a crash.
		DesktopWindow::Close();
		return;
	}
}

void OpOverlayWindow::Close(BOOL immediately, BOOL user_initiated, BOOL force)
{
	if (g_application->GetPopupDesktopWindow() == this)
		g_application->ResetPopupDesktopWindow();

	if (immediately || !UsingAnimation())
	{
		DesktopWindow::Close(TRUE, user_initiated, force);
		// No code after this. The object has been destroyed
		return;
	}

	m_close_on_complete = TRUE;

	OpRect rect;
	GetRect(rect);

	OpRect end_rect = rect;
	end_rect.OffsetBy(0, GetAnimationDistance());

	animateOpacity(1, 0);
	animateIgnoreMouseInput(TRUE, TRUE);
	animateRect(rect, end_rect);
	
	g_animation_manager->startAnimation(this, ANIM_CURVE_SPEED_UP);

	// Don't call DesktopWindow::Close, will do that when the animation is done
}

BOOL OpOverlayWindow::OnInputAction(OpInputAction* action)
{
	if (!QuickAnimationWindow::OnInputAction(action))
	{
		DesktopWindow* win = g_application->GetActiveDesktopWindow();
		if (win && win != this)
		{
			return win->OnInputAction(action);
		}
	}
	return TRUE;
}

INT32 OpOverlayWindow::GetAnimationDistance() {return ANIMATION_DISTANCE;}
