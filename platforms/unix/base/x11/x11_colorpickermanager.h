/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __X11_COLORPICKERMANAGER_H__
#define __X11_COLORPICKERMANAGER_H__

#include "platforms/utilix/x11_all.h"

class X11DragWidget;
class X11Widget;

class ColorPickerManager
{
public:
	static OP_STATUS GetColor(UINT32& color);
	static OP_STATUS GetWidget(X11Widget*& target, OpPoint& point, INT32 color);

	BOOL HandleEvent(XEvent* event);

private:
	enum Mode
	{
		GET_COLOR,
		GET_WIDGET
	};

private:
	ColorPickerManager();
	~ColorPickerManager();
	OP_STATUS Run(UINT32 &color);
	OP_STATUS Run(X11Widget*& target, OpPoint& point, INT32 color);

	void RestoreCursor();
	X11Types::Window FindWindow(int x, int y);
	BOOL GetPixel(X11Types::Window, int x, int y, UINT32& pixel);
	OP_STATUS MakeDragWidget(int screen);

private:
	Mode m_mode;
	X11DragWidget* m_drag_widget;
	UINT32 m_drag_color;
	BOOL m_active;
	UINT32 m_color;
	X11Widget* m_widget;
	OpPoint m_point;
	OP_STATUS m_status;
};

extern ColorPickerManager* g_color_picker_manager;

#endif
