/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DROP_DOWN_WINDOW_H
#define DROP_DOWN_WINDOW_H

#include "modules/widgets/OpDropDownWindow.h"
#include "modules/widgets/WidgetWindow.h"
#include "modules/display/coreview/coreviewlisteners.h"

class OpListBox;
class OpDropDown;

/***********************************************************************************
 **
 **	DropDownWindow - a WidgetWindow subclass for implementing the dropdown window
 **
 ***********************************************************************************/

class DropDownWindow : public WidgetWindow, public OpDropDownWindow, public CoreViewScrollListener
{
public:
	DropDownWindow();

	OP_STATUS Construct(OpDropDown* dropdown);

	virtual ~DropDownWindow();

	virtual BOOL OnNeedPostClose();

	virtual void OnResize(UINT32 width, UINT32 height);

	OpRect CalculateRect();

	void UpdateMenu(BOOL items_changed);

	BOOL HasClosed() const { return m_is_closed; }
	void SetClosed(BOOL closed) { m_is_closed = closed; }

	virtual void OnScroll(CoreView* view, INT32 dx, INT32 dy, SCROLL_REASON reason);
	virtual void OnParentScroll(CoreView* view, INT32 dx, INT32 dy, SCROLL_REASON reason) { OnScroll(view, dx, dy, reason); }

	virtual void ClosePopup() { Close(TRUE); }
	virtual void SetVisible(BOOL vis, BOOL default_size); 
	virtual void SelectItem(INT32 nr, BOOL selected);
	virtual INT32 GetFocusedItem();
#ifndef MOUSELESS
	virtual void ResetMouseOverItem();
#endif // MOUSELESS
	virtual BOOL HasHotTracking();
	virtual void SetHotTracking(BOOL hot_track);
	virtual BOOL IsListWidget(OpWidget* widget);
	virtual OpPoint GetMousePos();
#ifndef MOUSELESS
	virtual void OnMouseDown(OpPoint pt, MouseButton button, INT32 nclicks);
	virtual void OnMouseUp(OpPoint pt, MouseButton button, INT32 nclicks);
	virtual void OnMouseMove(OpPoint pt);
	virtual BOOL OnMouseWheel(INT32 delta, BOOL vertical);
#endif // MOUSELESS
	virtual void ScrollIfNeeded();
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual OP_STATUS GetAbsolutePositionOfList(OpRect &rect);
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
	virtual BOOL SendInputAction(OpInputAction* action);
	virtual void HighlightFocusedItem();

	BOOL			m_hot_tracking;	// if listbox has started hottracking
	OpDropDown*		m_dropdown;		// pointer back to dropdown that owns me
	OpListBox*		m_listbox;		// listbox in this dropdown
	INT32			m_scale;
	BOOL			m_is_closed;    // Set to TRUE when window is to be closed

private:
	UINT32 GetScreenHeight();

	// We need to save the unscaled width and height to ensure that the
	// contained OpListBox gets the same dimensions as the DropDownWindow, as
	// ScaleToScreen is not necessarily invertible.
	INT32 unscaled_width;
	INT32 unscaled_height;
};

#endif // DROP_DOWN_WINDOW_H
