/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_SPLITTER_H
#define OP_SPLITTER_H

#include "OpGroup.h"
#include "adjunct/quick/widgets/QuickGhostInputView.h"

// == OpSplitter ==================================================

#define OPSPLITTER_MIN_SIZE		10

#ifdef VEGA_OPPAINTER_SUPPORT
class OpSplitter;
class OpSplitterDragArea : public OpWidget, public QuickGhostInputViewListener
{
public:
	static OP_STATUS	Construct(OpSplitterDragArea** obj);
	OpSplitterDragArea();
	~OpSplitterDragArea();

	void SetSplitter(OpSplitter* splitter){m_splitter = splitter;}

	void UpdateGhost();

	virtual void OnResize(INT32* new_w, INT32* new_h);
	virtual void OnShow(BOOL show);
	virtual void OnMove();
	void OnGhostInputViewDestroyed(QuickGhostInputView* gv){if (m_ghost == gv)m_ghost = NULL;}

	void OnMouseMove(const OpPoint &point);
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnSetCursor(const OpPoint &point);
private:
	QuickGhostInputView*	m_ghost;
	OpSplitter*				m_splitter;
};
#endif // VEGA_OPPAINTER_SUPPORT

class OpSplitter : public OpGroup
{
	protected:
							OpSplitter();
							~OpSplitter();

	public:
		static OP_STATUS	Construct(OpSplitter** obj);

		void				SetPixelDivision(bool pixel_division, bool reversed = false);
		void				SetReversed(bool reversed);
		void				SetHorizontal(bool is_horizontal);
		bool				IsHorizontal() const { return m_is_horizontal; }
		void				SetDivision(INT32 divison);
		INT32				GetDivision() const { return m_split_division; }
		void				SetDividerSkin(const char* skin);
		bool				IsFixed() const { return m_is_fixed; }

		// == Hooks ======================

		void				OnRelayout() {}
		void				OnLayout();
		void				OnMouseMove(const OpPoint &point);
		void				OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
		void				OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
		void				OnSetCursor(const OpPoint &point);
		void				OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

		virtual const char*	GetInputContextName() {return "Splitter";}

		// Implementing the OpTreeModelItem interface

		virtual Type		GetType() {return WIDGET_TYPE_SPLITTER;}

	private:

		void				ComputeSplitPosition(INT32 width, INT32 height);
		void				ComputeRealSplitPosition(INT32 available, INT32 min0, INT32 max0, INT32 min1, INT32 max1);
		void				SetFixed(bool fixed);
		bool				IsReversed() const { return (m_reversed ^ (m_is_horizontal && GetRTL())) != 0; }
		bool				IsPixelDivisionReversed() const { return (m_reversed_pixel_division ^ (m_is_horizontal && GetRTL())) != 0; }
		// similar to !r1.Equals(r2), but also compares empty rectangles
		static bool			RectsNotEqual(const OpRect& r1, const OpRect& r2) { return r1.x != r2.x || r1.y != r2.y || r1.width != r2.width || r1.height != r2.height; }

		INT32				m_split_division;
		INT32				m_split_size;
		INT32				m_split_position;
		INT32				m_child_count;
		bool				m_is_fixed;
		bool				m_is_horizontal;
		bool				m_is_dragging;
		bool				m_pixel_division;
		bool				m_reversed_pixel_division;
		bool				m_reversed;
		OpButton*           m_divider_button;           // used to draw just the skin for the part shown dividing the layouts
#ifdef VEGA_OPPAINTER_SUPPORT
		OpSplitterDragArea*	m_divider_drag_area;
#endif // VEGA_OPPAINTER_SUPPORT
};

#endif // OP_SPLITTER_H
