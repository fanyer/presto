/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef __X11_WINDOWWIDGET_H__
#define __X11_WINDOWWIDGET_H__

#include "x11_widget.h"

class X11WindowWidget : public X11Widget
{
public:
	enum EventType {
		DELAYED_RESIZE,
		DELAYED_SHOW,
		VALIDATE_MDE_SCREEN,
	};

	X11WindowWidget(class X11OpWindow* op_window);
	~X11WindowWidget();

	virtual void Show();
	virtual void ShowBehind();
	virtual void Hide();
	virtual bool HandleEvent(XEvent *event);
	virtual void HandleResize();
	virtual void HandleCallBack(INTPTR ev);

	BOOL ConfirmClose();

	class X11OpWindow *GetOpWindow() const { return m_op_window; }

	// Implementation specific methods:
	/** @return The currently focused window widget */
	static X11WindowWidget* GetActiveWindowWidget();

	/** @return The topmost widget of an entire widget chain (without a parent). 
	  * See also X11Widget::GetTopLevel().
	  */
	static X11Widget* GetActiveToplevelWidget();

	/** @return X11 window handle of the topmost widget
	  */
	static X11Types::Window GetActiveTopWindowHandle();

protected:
	class X11OpWindow* m_op_window;
	X11Types::Window m_return_focus_to;
	static X11WindowWidget* m_active_window_widget;	
	static X11Types::Window m_active_topwindow_handle;
};

#endif // __X11_WINDOWWIDGET_H__
