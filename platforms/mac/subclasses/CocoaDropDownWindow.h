/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_DROPDOWN_WINDOW_H
#define COCOA_DROPDOWN_WINDOW_H

#ifdef WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
#include "modules/widgets/OpDropDownWindow.h"
#else
#include "modules/widgets/OpDropDown.h"
#endif // WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
#include "platforms/mac/subclasses/CocoaTreeViewWindow.h"
class CocoaInternalQuickMenu;

class CocoaDropDownWindow : public OpDropDownWindow, OpTimerListener
{
public:
	CocoaDropDownWindow(OpDropDown* dropdown);
	virtual ~CocoaDropDownWindow();
	virtual OpRect CalculateRect();
	virtual void UpdateMenu(BOOL items_changed);
	virtual BOOL HasClosed() const;
	virtual void SetClosed(BOOL closed);
	virtual void ClosePopup();
	virtual void SetVisible(BOOL vis, BOOL default_size = FALSE);
	virtual void SelectItem(INT32 nr, BOOL selected);
	virtual INT32 GetFocusedItem();
	virtual void ResetMouseOverItem();
	virtual BOOL HasHotTracking();
	virtual BOOL IsListWidget(OpWidget* widget);
	virtual void SetHotTracking(BOOL hot_track);
	virtual OpPoint GetMousePos();
	virtual void OnMouseDown(OpPoint pt, MouseButton button, INT32 nclicks);
	virtual void OnMouseUp(OpPoint pt, MouseButton button, INT32 nclicks);
	virtual void OnMouseMove(OpPoint pt);
	virtual BOOL OnMouseWheel(INT32 delta, BOOL vertical);
	virtual void ScrollIfNeeded();
	virtual OP_STATUS GetItemAbsoluteVisualBounds(INT32 index, OpRect &rect);
	virtual BOOL SendInputAction(OpInputAction* action);
	virtual void HighlightFocusedItem();
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual OP_STATUS GetAbsolutePositionOfList(OpRect &rect);
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

	// OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

	void SetActiveItem(int item);
	void PopupClosed();
private:
	OpDropDown* m_dropdown;
	OperaNSPopUpButton *m_button;
	int m_selected;
	id m_delegate;
	BOOL m_is_closed;
	OpTimer* m_show_timer;
	OpWidget *m_dummy_listwidget;
	static BOOL s_was_deleted;
};

#endif // COCOA_DROPDOWN_WINDOW_H
