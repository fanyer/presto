/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
# include "modules/dragdrop/src/scroller.h"

/* The delay of the scrolling start. It's useful in order to not scroll when a drag leaves or enters
 the scrollable fast enough. */
# define SCROLL_START_DELAY (1000)

DragScroller::DragScroller()
{
	m_is_started = FALSE;
	m_scrolling_timer.SetTimerListener(this);
}

DragScroller::~DragScroller()
{
	Stop();
}

void
DragScroller::OnTimeOut(OpTimer* timer)
{
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
	{
		if (g_drag_manager->ScrollIfNeeded(m_last_move_pos.x, m_last_move_pos.y))
			Start();
		else
			Stop();
	}
	else
		Stop();
}

void
DragScroller::Start()
{
	if (!m_is_started)
		m_scrolling_timer.Start(SCROLL_START_DELAY);
	else
		m_scrolling_timer.Start(DND_SCROLL_INTERVAL);
	m_is_started = TRUE;
}

void
DragScroller::Stop()
{
	m_scrolling_timer.Stop();
	m_is_started = FALSE;
}

#endif // DRAG_SUPPORT