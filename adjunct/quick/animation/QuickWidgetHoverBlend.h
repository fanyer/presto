/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Petter Nilsen
 */

#ifndef QUICK_WIDGET_HOVER_BLEND_H
#define QUICK_WIDGET_HOVER_BLEND_H

#include "modules/hardcore/timer/optimer.h"

/** QuickWidgetHoverBlend is for doing animated blends between hover and non-hover skin states for classes that need 
 ** it for multiple synchronized widgets or for widgets that doesn't inherit from OpButton and gets it automatically
 */
class QuickWidgetHoverBlend : public OpTimerListener
{
public:
	QuickWidgetHoverBlend();
	~QuickWidgetHoverBlend();

	// OpTimerListener
	void OnTimeOut(OpTimer* timer);

	// Which widget(s) to blend 
	OP_STATUS AddWidgetToBlend(OpWidget *widget);

	// Which widget(s) to blend 
	void RemoveWidgetToBlend(OpWidget *widget);

	// blend from non-hover to hover state. Call this from OnMouseMove if the mouse it just entered the widget
	void HoverBlendIn();

	// blend from hover to non-hover state. Call this from OnMouseLeave
	void HoverBlendOut();

	UINT32	GetTimerDelay() const { return m_timer_delay; }
	bool	IsTimerRunning() const { return m_timer_delay != 0; }

private:
	void StartTimer(UINT32 millisec);
	void StopTimer();
	void UpdateHover();

private:
	OpVector<OpWidget> m_blend_widgets;
	int                m_hover_counter;    // the updated hover value to use for the SetState call
	double             m_hover_start_time; // when the hover started
	OpTimer            m_timer;
	UINT32             m_timer_delay;
	bool               m_is_hovering;
};


#endif // QUICK_WIDGET_HOVER_BLEND_H
