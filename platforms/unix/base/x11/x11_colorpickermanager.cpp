/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */


#include "core/pch.h"
#include "platforms/unix/base/x11/x11_colorpickermanager.h"

#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/unix/base/x11/x11_dragwidget.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/unix/base/x11/x11_windowwidget.h"

ColorPickerManager* g_color_picker_manager = 0;

// static
OP_STATUS ColorPickerManager::GetColor(UINT32& color)
{
	X11Widget* widget = X11WindowWidget::GetActiveWindowWidget();
	if (!widget)
		return OpStatus::ERR;

	if (g_color_picker_manager)
		return OpStatus::ERR;

	g_color_picker_manager = OP_NEW(ColorPickerManager,());
	if (!g_color_picker_manager)
		return OpStatus::ERR_NO_MEMORY;

	widget->SaveCursor();
	widget->SetCursor(CURSOR_CROSSHAIR);
	widget->Grab();

	OP_STATUS rc = g_color_picker_manager->Run(color);

	widget->Ungrab();
	widget->RestoreCursor();

	OP_DELETE(g_color_picker_manager);
	g_color_picker_manager = 0;

	return rc;
}


//static 
OP_STATUS ColorPickerManager::GetWidget(X11Widget*& target, OpPoint& point, INT32 color)
{
	X11Widget* widget = X11WindowWidget::GetActiveWindowWidget();
	if (!widget)
		return OpStatus::ERR;

	if (g_color_picker_manager)
		return OpStatus::ERR;

	g_color_picker_manager = OP_NEW(ColorPickerManager,());
	if (!g_color_picker_manager)
		return OpStatus::ERR_NO_MEMORY;

	widget->SaveCursor();
	widget->SetCursor(CURSOR_NONE);
	widget->Grab();

	OP_STATUS rc = g_color_picker_manager->Run(target, point, color);

	g_color_picker_manager->RestoreCursor();

	widget = g_x11->GetWidgetList()->GetGrabbedWidget();
	if (widget)
		widget->Ungrab();

	OP_DELETE(g_color_picker_manager);
	g_color_picker_manager = 0;

	return rc;
}




ColorPickerManager::ColorPickerManager()
	:m_mode(GET_COLOR)
	,m_drag_widget(0)
	,m_drag_color(0)
	,m_active(FALSE)
	,m_color(0)
	,m_widget(0)	
	,m_status(OpStatus::OK)
{
}


ColorPickerManager::~ColorPickerManager()
{
	OP_DELETE(m_drag_widget);
}


OP_STATUS ColorPickerManager::Run(UINT32& color)
{
	m_mode   = GET_COLOR;
	m_active = TRUE;
	m_status = OpStatus::ERR; // Set to OK on success

	X11OpMessageLoop::Self()->NestLoop();

	m_active = FALSE;

	if(m_status == OpStatus::OK)
		color = m_color;

	return m_status;
}


OP_STATUS ColorPickerManager::Run(X11Widget*& target, OpPoint& point, INT32 color)
{
	m_mode   = GET_WIDGET;
	m_active = TRUE;
	m_status = OpStatus::ERR; // Set to OK on success

	m_drag_color = color;
	RETURN_IF_ERROR(MakeDragWidget(InputUtils::GetScreenUnderPointer()));

	X11OpMessageLoop::Self()->NestLoop();

	m_active = FALSE;
	
	if (m_status == OpStatus::OK)
	{
		target = m_widget;
		point  = m_point;
	}

	return m_status;
}


void ColorPickerManager::RestoreCursor()
{
	if (m_drag_widget)
		m_drag_widget->Hide();
	
	X11Widget* widget = g_x11->GetWidgetList()->GetGrabbedWidget();
	if (widget)
		widget->RestoreCursor();
}


BOOL ColorPickerManager::HandleEvent(XEvent* event)
{
	if (m_active)
	{
		switch(event->type)
		{
			case MotionNotify:
			{
				if (m_drag_widget)
				{
					int screen = InputUtils::GetScreenUnderPointer();
					if (m_drag_widget->GetScreen() != screen)
						RETURN_VALUE_IF_ERROR(MakeDragWidget(screen), TRUE);					
					m_drag_widget->Move(event->xbutton.x_root,event->xbutton.y_root);
					m_drag_widget->Update(0, 0, m_drag_widget->GetWidth(), m_drag_widget->GetHeight());
				}
			}

			case KeyPress:
			{
				char buffer[101];
				KeySym keysym = 0;
				XLookupString(&event->xkey, buffer, 100, &keysym, 0);
				if (keysym == XK_Escape)
				{
					m_status = OpStatus::ERR;
					X11OpMessageLoop::Self()->StopCurrent();
				}
				return TRUE;
			}	

			case ButtonRelease:
			{
				X11Types::Window window = FindWindow(event->xbutton.x_root,event->xbutton.y_root);

				if (m_mode == GET_COLOR)
				{
					if (GetPixel(window, event->xbutton.x_root,event->xbutton.y_root, m_color))
						m_status = OpStatus::OK;
					else
						m_status = OpStatus::ERR;

				}
				else if (m_mode == GET_WIDGET)
				{
					X11Widget* widget = g_x11->GetWidgetList()->FindWidget(window);
					if (widget)
					{
						m_widget = widget;
						m_point  = OpPoint(event->xbutton.x,event->xbutton.y);
						m_status = OpStatus::OK;
					}
					else
						m_status = OpStatus::ERR;
				}

				X11OpMessageLoop::Self()->StopCurrent();
				return TRUE;
			}
			break;

			case ButtonPress:
			case KeyRelease:
				return TRUE;
			break;

		}
	}

	return FALSE;
}


X11Types::Window ColorPickerManager::FindWindow(int x, int y)
{
	X11Types::Display* display = g_x11->GetDisplay();

	XWindowAttributes wa;
	X11Types::Window findW = DefaultRootWindow(display), stopW, childW;

	/* Setup for first window find */
	stopW = findW;

	while (stopW) 
	{
		XTranslateCoordinates(display, findW, stopW, x, y, &x, &y, &childW);
		findW = stopW;
		/* If child is not InputOutput (for example, InputOnly) */
		/* then don't continue, return the present findW which */
		/* can be the root, or a root child of class InputOutput */
		if (childW && XGetWindowAttributes(display, childW, &wa) && wa.c_class != InputOutput)
			break;
		stopW = childW;
	}

	return findW;
}


BOOL ColorPickerManager::GetPixel(X11Types::Window, int x, int y, UINT32& pixel)
{
	X11Types::Display* display = g_x11->GetDisplay();
	int screen = InputUtils::GetScreenUnderPointer();
	
	XImage* image = XGetImage(display, RootWindow(display, screen), x, y, 1, 1, AllPlanes, ZPixmap);
	if (image)
	{
		pixel = XGetPixel(image, 0, 0);
		XDestroyImage(image);
		return TRUE;
	}

	return FALSE;
}


OP_STATUS ColorPickerManager::MakeDragWidget(int screen)
{
	OP_DELETE(m_drag_widget);

	m_drag_widget = OP_NEW(X11DragWidget, (m_drag_color, OpPoint(0,0)));
	if (!m_drag_widget)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS res = m_drag_widget->Init(0, 0, X11Widget::TRANSIENT | X11Widget::BYPASS_WM | X11Widget::TRANSPARENT, screen);
	if (OpStatus::IsError(res))
	{
		return res;
	}

	m_drag_widget->InstallIcon();

	OpPoint pos;
	if (InputUtils::GetGlobalPointerPos(pos) )
		m_drag_widget->Move(pos.x, pos.y);

	m_drag_widget->Show();

	return OpStatus::OK;
}
