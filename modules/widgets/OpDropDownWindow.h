/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_DROP_DOWN_WINDOW_H
#define OP_DROP_DOWN_WINDOW_H

class OpWidget;
class OpDropDown;
class OpInputAction;

class OpDropDownWindow
{
public:
	static OP_STATUS Create(OpDropDownWindow **w, OpDropDown* dropdown);
	virtual ~OpDropDownWindow() {}
	virtual OpRect CalculateRect() = 0;
	/**
	 * Called when there was some change that can influence
	 * this drop down window.
	 * @param items_changed TRUE when there was some addition or removal of
	 *		  the owner's items/options, FALSE in other cases.
	 */
	virtual void UpdateMenu(BOOL items_changed) = 0;
	virtual BOOL HasClosed() const = 0;
	virtual void SetClosed(BOOL closed) = 0;
	virtual void ClosePopup() = 0;
	virtual void SetVisible(BOOL vis, BOOL default_size = FALSE) = 0;
	virtual void SelectItem(INT32 nr, BOOL selected) = 0;
	virtual INT32 GetFocusedItem() = 0;
#ifndef MOUSELESS
	virtual void ResetMouseOverItem() = 0;
#endif // MOUSELESS
	virtual BOOL HasHotTracking() = 0;
	virtual BOOL IsListWidget(OpWidget* widget) = 0;
	virtual void SetHotTracking(BOOL hot_track) = 0;
	virtual OpPoint GetMousePos() = 0;
#ifndef MOUSELESS
	virtual void OnMouseDown(OpPoint pt, MouseButton button, INT32 nclicks) = 0;
	virtual void OnMouseUp(OpPoint pt, MouseButton button, INT32 nclicks) = 0;
	virtual void OnMouseMove(OpPoint pt) = 0;
	virtual BOOL OnMouseWheel(INT32 delta, BOOL vertical) = 0;
#endif // MOUSELESS
	virtual void ScrollIfNeeded() = 0;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual OP_STATUS GetAbsolutePositionOfList(OpRect &rect) = 0;
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
	virtual BOOL SendInputAction(OpInputAction* action) = 0;
	virtual void HighlightFocusedItem() = 0;
};

#endif // OP_DROP_DOWN_WINDOW_H
