/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef X11_DRAGWIDGET_H
#define X11_DRAGWIDGET_H

#include "x11_widget.h"


class DesktopDragObject;
class X11OpDragManager;

class X11DragWidget : public X11Widget
{
public:
	/**
	 * Prepares the drag widget with a drag object
	 *
	 * @param drag_manager The drag manager. Not used at the moment
	 * @param drag_object The dragged object. It may contain bitmaps that we show
	 * @param hotspot The window hotspot
	 */
	X11DragWidget(X11OpDragManager* drag_manager, DesktopDragObject* drag_object, const OpPoint& hotspot);

	/**
	 * Prepares the drag widget in color mode. A 12x12 window will be dragged
	 *
	 * @param color Solid color to be used in the window
	 * @param hotspot The window hotspot
	 */
	X11DragWidget(UINT32 color, const OpPoint& hotspot);

	/**
	 * Repaints the window content
	 */
	void Repaint(const OpRect& rect);

	/**
	 * Prepares the image that will be draw to the window
	 */
	void InstallIcon();

	virtual void Move(int x, int y);
	virtual bool HandleEvent(XEvent* event);

private:
	X11OpDragManager* m_drag_manager;
	DesktopDragObject* m_drag_object;
	OpPoint m_hotspot;
	BOOL m_drag_color;
	UINT32 m_color;
};

#endif // __X11_VIEWWIDGET_H__
