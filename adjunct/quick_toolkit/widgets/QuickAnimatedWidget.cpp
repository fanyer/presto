/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Chris Pine (chrispi)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickAnimatedWidget.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "modules/widgets/OpWidget.h"


OP_STATUS QuickAnimatedWidget_Layout(const OpRect& new_rect, QuickAnimationListener* listener, OpWidget* opwidget)
{
	OpRect old_rect;

	if (opwidget->IsFloating())
		old_rect = opwidget->GetOriginalRect();
	else
		old_rect = opwidget->GetRect();

	if (old_rect.x      == -10000 &&
	    old_rect.y      == -10000 &&
	    old_rect.width  ==      0 &&
	    old_rect.height ==      0)
	{
		// first time being layed out, so don't animate it
		if (opwidget->IsFloating())
			opwidget->SetOriginalRect(new_rect);
		else
			opwidget->SetRect(new_rect);
	}
	else if (!new_rect.Equals(old_rect))
	{
		if (opwidget->IsFloating() && opwidget->GetAnimation() == NULL)
		{
			// If the widget is floating, but *not* animating, then
			// we assume it is being dragged, in which case we
			// should *not* animate it.
		}
		else
		{
			QuickAnimationParams params(opwidget);

			params.duration   = 0.2;
			params.curve      = ANIM_CURVE_SLOW_DOWN;
			params.move_type  = ANIM_MOVE_RECT_TO_ORIGINAL;
			params.listener   = listener;

			g_animation_manager->startAnimation(params);
		}

		opwidget->SetOriginalRect(new_rect);
	}

	return OpStatus::OK;
}

void QuickAnimatedWidget_SetZ(OpWidget::Z z, OpWidget* opwidget)
{
	if (!opwidget)
		return;

	if (opwidget->IsFloating() && opwidget->GetAnimation() == NULL)
		opwidget->SetZ(OpWidget::Z_TOP); // assuming that this is being dragged, so put it on top
	else
		opwidget->SetZ(z);
}


