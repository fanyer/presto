/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DRAGSCROLLER_H
# define DRAGSCROLLER_H

# ifdef DRAG_SUPPORT
#  include "modules/hardcore/timer/optimer.h"

/** Class taking care of smooth scrolling during d'n'd */
class DragScroller : public OpTimerListener
{
	/** The point the drag was moved to the last time. In document scale, relative to the upper left corner of the view. */
	OpPoint				m_last_move_pos;
	/** The scrolling timer making sure scroll is done in set intervals (thus is smooth). */
	OpTimer				m_scrolling_timer;
	/** The flag indicating if scroller's timer is already started. */
	BOOL				m_is_started;
public:
	DragScroller();
	~DragScroller();
	/** Sets the last d'n'd move position. */
	void				SetLastMovePos(int x, int y) { m_last_move_pos.x = x; m_last_move_pos.y = y; }
	/** Start the scroller (its timer). */
	void				Start();
	/** Stops the scroller (its timer). */
	void				Stop();
	/** Returns TRUE if the scroller (its timer) is started. FALSE otherwise. */
	BOOL				IsStarted() { return m_is_started; }
	// OpTimerListener API
	virtual void		OnTimeOut(OpTimer* timer);
};

# endif // DRAG_SUPPORT

#endif // DRAGSCROLLER_H
