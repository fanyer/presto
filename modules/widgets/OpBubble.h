/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPBUBBLE_H
#define OPBUBBLE_H

#include "modules/widgets/WidgetWindow.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpBubbleHandler.h"
#include "modules/hardcore/timer/optimer.h"

/** OpBubble shows a bubble-like popup close to something else. It can contain text or be subclassed to contain other ui as well. */

class OpBubble : public WidgetWindow, public OpWidgetListener, public OpTimerListener
{
public:
	OpBubble(OpBubbleHandler *handler);
	virtual ~OpBubble();

	OP_STATUS Init(OpWindow *parent_window);

	/** Update the placement of the bubble.
		The placement will be dependant on the required sized, position of target_screen_rect,
		the available screen and other things.
		
		@param target_screen_rect The rectangle representing the object the bubble relates to, in screen coordinated.
		*/
	void UpdatePlacement(const OpRect &target_screen_rect);

	/** Copy the font and color properties from the source widget */
	void UpdateFontAndColors(OpWidget *source);

	/** Show bubble */
	void Show();

	/** Hide bubble */
	void Hide();

	/** Set text to be displayed in this bubble. */
	OP_STATUS SetText(const OpStringC& text);

	/** Along which edge it should be placed */
	enum PLACEMENT {
		PLACEMENT_LEFT,
		PLACEMENT_RIGHT,
		PLACEMENT_TOP,
		PLACEMENT_BOTTOM
	};

	/** Where along the edge it should be placed */
	enum ALIGNMENT {
		ALIGNMENT_BEGINNING,
		ALIGNMENT_MIDDLE,
		ALIGNMENT_END
	};

	/** Typ of animation for the appearance of the bubble when it's shown */
	enum ANIMATION {
		ANIMATION_NORMAL,
		ANIMATION_BOUNCE,
		ANIMATION_SHAKE
	};

	/** Return the window style that should be used for the bubble. */
	virtual OpWindow::Style GetWindowStyle() { return OpWindow::STYLE_POPUP; }
	virtual PLACEMENT GetDefaultPlacement() { return PLACEMENT_BOTTOM; }
	virtual ALIGNMENT GetDefaultAlignment() { return ALIGNMENT_MIDDLE; }
	virtual ANIMATION GetAnimation() { return ANIMATION_SHAKE; }
	virtual void GetRequiredSize(INT32 &width, INT32 &height);

	/** Get a good placement in close connection to ref_screen_rect according to the given placement and alignment.
		The margins should be the window margins which will also affect the end result. */
	OpRect GetPlacement(INT32 width, INT32 height,
								INT32 margin_left, INT32 margin_top, INT32 margin_right, INT32 margin_bottom,
								const OpRect &ref_screen_rect, PLACEMENT &placement, ALIGNMENT alignment);

private:
	friend class OpBubbleHandler;
	virtual BOOL			OnIsCloseAllowed();
	virtual void			OnResize(UINT32 width, UINT32 height);
	OpBubbleHandler*		m_handler;
	OpMultilineEdit*		m_edit;
	PLACEMENT 				m_current_placement;
	OpRect					m_placement_rect;
	BOOL					m_visible;
	OpTimer					m_timer;
	double					m_timer_start_time;
	int						m_extra_distance;
	int						m_extra_shift;

	// OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);
};

/** Login bubble next to fields with want */
//class OpWandBubble : public OpBubble
//{
//public:
//};

/** Bubble with language options */
//class OpLanguageBubble : public OpBubble
//{
//public:
//};

#endif // OPBUBBLE_H
