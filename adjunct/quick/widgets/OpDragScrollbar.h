/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_DRAGSCROLLBAR_H
#define OP_DRAGSCROLLBAR_H

#include "modules/widgets/OpScrollbar.h"
#include "adjunct/quick_toolkit/widgets/OpBar.h"
#include "modules/pi/OpSystemInfo.h"
#include "adjunct/quick/widgets/QuickGhostInputView.h"

class OpDragbarChecker
{
public:

	OpDragbarChecker(OpBar::Alignment alignment, OpBar::Wrapping wrapping, BOOL use_thumbnails) :
		m_alignment(alignment),
		m_wrapping(wrapping),
		m_use_thumbnails(use_thumbnails)
		{}

	BOOL IsDragbarAllowed()
	{
		return IsAlignmentAllowed(m_alignment) && IsWrappingAllowed(m_wrapping);
	}

protected:

	BOOL IsAlignmentAllowed(OpBar::Alignment alignment)
	{
		if(m_use_thumbnails && (alignment == OpBar::ALIGNMENT_TOP || alignment == OpBar::ALIGNMENT_BOTTOM))
		{
			return TRUE;
		}
		else
		{
			return (alignment == OpBar::ALIGNMENT_LEFT || alignment == OpBar::ALIGNMENT_RIGHT);
		}
	}

	BOOL IsWrappingAllowed(OpBar::Wrapping wrapping)
	{
		return (wrapping == OpBar::WRAPPING_OFF || wrapping == OpBar::WRAPPING_EXTENDER);
	}

private:

	const OpBar::Alignment m_alignment;
	const OpBar::Wrapping m_wrapping;
	const BOOL m_use_thumbnails;
};

class OpDragbarDelayChecker
{
public:

	OpDragbarDelayChecker(UINT32 delay_in_ms) : m_previous(g_op_time_info->GetRuntimeMS()), m_ms(delay_in_ms)
	{}

	BOOL CanNotifyAboutDrag(const OpPoint& point)
	{
		UINT32 current = g_op_time_info->GetRuntimeMS();

		if(m_previous + m_ms < current)
		{
			m_previous = current;
			if(!point.Equals(m_previous_point))
			{
				m_previous_point.Set(point.x, point.y);
				return TRUE;
			}
		}
		return FALSE;
	}
private:
	UINT32 m_previous;
	UINT32 m_ms;
	OpPoint m_previous_point;
};

class OpDragScrollbarTarget
{
public:

	virtual ~OpDragScrollbarTarget() {}

	/** @return The current size of the target widget
	  */
	virtual OpRect GetWidgetSize() = 0;

	/** Called when the drag bar has been dragged to a different position
	  * @param size New size with dragged bar
	  */
	virtual void SizeChanged(const OpRect& size) = 0;

	/** @return Whether the scrollbar should be shown */
	virtual BOOL IsDragScrollbarEnabled() = 0;

	/** @return the min value that should be allowed after dragging stopped.
	  * If the value is less than this when dragging stopped, it will snap to min.
	  */
	virtual INT32 GetMinHeightSnap() = 0;

	virtual void GetMinMaxHeight(INT32& min_height, INT32& max_height) = 0;

	virtual void GetMinMaxWidth(INT32& min_height, INT32& max_height) = 0;

	// called when dragging has been complete - allows the target to defer layout until this call is made
	virtual void EndDragging() = 0;
};

/***********************************************************************************
**
**	OpDragScrollbar - used as both a separator between the pagebar and the web pages
**					and as a scrollbar to scroll the pagebar
**
***********************************************************************************/

class OpDragScrollbar : public OpScrollbar
#ifdef VEGA_OPPAINTER_SUPPORT
	, public QuickGhostInputViewListener
#endif // VEGA_OPPAINTER_SUPPORT
{
public:
								OpDragScrollbar();
		virtual					~OpDragScrollbar();

		static					OP_STATUS Construct(OpDragScrollbar** obj);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return WIDGET_TYPE_DRAGSCROLLBAR;}

		virtual const char*	GetInputContextName() {return "Drag Scrollbar";}

		// == Hooks ======================

		void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
		void OnMouseMove(const OpPoint &point);
		void OnMouseLeave();
		void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
		void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
		void OnSetCursor(const OpPoint &point);
//		BOOL OnScroll(INT32 delta, BOOL vertical, BOOL smooth = TRUE);
//		BOOL OnMouseWheel(INT32 delta,BOOL vertical);

		void SetDragScrollbarTarget(OpDragScrollbarTarget* target) 
		{ 
			m_target = target; 
		}

		void GetRequiredSize(INT32& width, INT32& height);

		OpRect LayoutToAvailableRect(const OpRect& rect, BOOL compute_rect_only = FALSE);

		// called when the other toolbars are laid out
		void SetAlignment(OpBar::Alignment alignment);
		OpBar::Alignment GetAlignment() { return m_alignment; }

#ifdef VEGA_OPPAINTER_SUPPORT
		virtual void OnResize(INT32* new_w, INT32* new_h);
		virtual void OnShow(BOOL show);
		virtual void OnMove();
		void OnGhostInputViewDestroyed(QuickGhostInputView* gv){if (m_ghost == gv)m_ghost = NULL;}
#endif // VEGA_OPPAINTER_SUPPORT

		void EnableTransparentSkin(BOOL enable);
protected:
		void UpdateMinMaxValues();
		void EndDragging();

private:
		OpDragScrollbarTarget*	m_target;
		BOOL					m_is_dragging;
		INT32					m_max_height;
		INT32					m_min_height;
		INT32					m_max_width;
		INT32					m_min_width;
		OpBar::Alignment		m_alignment;
		OpDragbarDelayChecker	m_drag_checker;
		OpPoint					m_mouse_down_point;
#ifdef VEGA_OPPAINTER_SUPPORT
		QuickGhostInputView*	m_ghost;
#endif // VEGA_OPPAINTER_SUPPORT
};

#endif // OP_DRAGSCROLLBAR_H
