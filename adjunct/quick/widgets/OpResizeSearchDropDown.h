/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef OP_RESIZE_SEARCH_DROPDOWN_H
#define OP_RESIZE_SEARCH_DROPDOWN_H

#include "modules/util/gen_math.h"
#include "modules/widgets/OpWidget.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "adjunct/quick/animation/QuickAnimation.h"
#endif

class OpSearchDropDownSpecial;

///////////////////////////////////////////////////////////////

class OpResizeSearchDropDown : public OpWidget
#ifdef VEGA_OPPAINTER_SUPPORT
	, public QuickAnimation
#endif
{
protected:
	OpResizeSearchDropDown();
	virtual ~OpResizeSearchDropDown();

public:
	static OP_STATUS		Construct(OpResizeSearchDropDown** obj);

	virtual Type			GetType() {return WIDGET_TYPE_RESIZE_SEARCH_DROPDOWN;}
	virtual void			GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	virtual void			OnLayout();
	virtual void			OnAdded();
	virtual void			OnDeleted();

    virtual void			OnContentSizeChanged();
	virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void			OnSetCursor(const OpPoint &point);
	virtual void			OnMouseMove(const OpPoint &point);
	virtual void			OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void			OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
	{
		OpWidgetListener* listener = GetParent() ? GetParent() : this;
		return listener->OnContextMenu(this, child_index, menu_point, avoid_rect, keyboard_invoked);
	}

	virtual OP_STATUS		GetText(OpString& text);

	INT32					GetCurrentWeightedWidth() { return m_weighted_width; }
	void					SetCurrentWeightedWidth(INT32 width) { m_weighted_width = width; }

	virtual void			SetAvailableParentSize(INT32 parent_width, INT32) { m_parent_available_width = parent_width; ResetRequiredSize(); }

	// == OpInputContext ======================
	virtual BOOL			OnInputAction(OpInputAction* action);

#ifdef VEGA_OPPAINTER_SUPPORT
	// == QuickAnimation ======================
	virtual					void OnAnimationStart();
	virtual					void OnAnimationUpdate(double position);
	virtual					void OnAnimationComplete();
#endif


private:
	class SiblingIterator;

	void					DetectGrowableSiblings();
	void					DetectLeftRightDraggingBorders();

	OpWidget				*m_knob;
	OpSearchDropDownSpecial	*m_search_drop_down;
	OpPoint					m_drag_start_point;
	OpRect					m_drag_start_rect;
	BOOL					m_dragging;
	INT32					m_max_size;
	INT32					m_weighted_width;
	INT32					m_width_auto_resize;
	INT32					m_width_auto_resize_old;
	INT32					m_requested_width;
	double					m_animation_position;
	BOOL					m_knob_on_left;
	// grow to left/right indicate, that this widget is resizable
	// on left or right edge; it can resizable only on one edge
	BOOL					m_grow_to_left;
	BOOL					m_grow_to_right;
	OpWidget*				m_growable_left_sibling;
	OpWidget*				m_growable_right_sibling;
	BOOL					m_delayed_layout_in_progress;
	INT32					m_parent_available_width;

	INT32					m_drag_left_border;
	INT32					m_drag_right_border;
};

inline int CALC_WIDTH_FROM_WEIGHTED_WIDTH(double weighted_width, double parent_width)
{
	return OpRound((weighted_width / 10000.) * parent_width);
}
inline int CALC_WEIGHTED_WIDTH_FROM_WIDTH(double width, double parent_width)
{
	return OpRound((width / parent_width) * 10000.);
}

#endif // OP_RESIZE_SEARCH_DROPDOWN_H
