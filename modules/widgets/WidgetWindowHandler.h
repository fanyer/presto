/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WIDGETWINDOWHANDLER_H
#define WIDGETWINDOWHANDLER_H

class WidgetWindow;
class OpWidget;
class OpWindow;

/** Handles ownership of a WidgetWindow. */
class WidgetWindowHandler
{
public:
	WidgetWindowHandler();
	~WidgetWindowHandler();

	/** Call if the owner is about to be deleted, so the window can be closed as soon as possible instead
		of waiting for the destructor. */
	void OnOwnerDeleted();

	/** Take ownership of a WidgetWindow. */
	void SetWindow(WidgetWindow *window);

	/** Return window or NULL */
	WidgetWindow *GetWindow() { return m_widget_window; }

	/** Will return TRUE if this window handler has just had awindow and it has sent the asynchronous request to close it. */
	BOOL IsClosingWindow() { return m_closed_widget_window ? TRUE : FALSE; }

	void Show();

	void Close();
private:
	friend class WidgetWindow;
	WidgetWindow *m_widget_window;
	WidgetWindow *m_closed_widget_window;
};

#endif // WIDGETWINDOWHANDLER_H
