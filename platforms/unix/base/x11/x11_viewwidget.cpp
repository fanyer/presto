/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

#ifndef VEGA_OPPAINTER_SUPPORT

#include "x11_viewwidget.h"

#include "inpututils.h"
#include "x11_opview.h"
#include "x11_opwindow.h"
#include "x11_windowwidget.h"


int X11ViewWidget::last_mouse_x;
int X11ViewWidget::last_mouse_y;


X11ViewWidget::X11ViewWidget(X11OpView *opview)
	: opview(opview),
	  pending_leave(false)
{
}

X11ViewWidget::~X11ViewWidget()
{
}

void X11ViewWidget::UpdateAll(bool now)
{
	if (no_update)
		return;

	if (now)
	{
		// Hey!! X11ViewWidget objects may have child windows!

		OpPaintListener *listener = opview ? opview->GetPaintListener() : 0;
		if (listener)
		{
			int width, height;
			GetSize(&width, &height);
			XFlush(display);
			XEvent more;
			while (XCheckWindowEvent(display, handle, ExposureMask, &more))
				;
			opview->GenerateOnPaint(OpRect(0, 0, width, height), true);
			return;
		}
	}
	else
	{
		X11Widget::UpdateAll(now);
	}
}

bool X11ViewWidget::HandleEvent(XEvent *event)
{
	if (!opview)
		return false;

	bool eaten = true;

	switch (event->type)
	{
	case Expose:
	case GraphicsExpose:
	{
		HandleAllExposures(event);
		return true;
	}
	case MotionNotify:
	{
		OpMouseListener *listener = opview->GetMouseListener();
		if (listener)
		{
			XEvent *e = event;
			XEvent more;
			while (XCheckTypedWindowEvent(display, handle, MotionNotify,
										  &more))
			{
				e = &more;
			}

			int x = e->xmotion.x;
			int y = e->xmotion.y;
			FromViewCoords(&x, &y);
			last_mouse_x = e->xmotion.x_root;
			last_mouse_y = e->xmotion.y_root;
			listener->OnMouseMove(OpPoint(x, y), opview);
			return true;
		}
		break;
	}
	case EnterNotify:
		pending_leave = false;
		break;
	case LeaveNotify:
	{
		OpMouseListener *listener = opview->GetMouseListener();
		if (listener)
		{
			if (event->xcrossing.mode != NotifyNormal ||
				!InputUtils::GetButtonFlags())
			{
				listener->OnMouseLeave();
				return true;
			}
			else
			{
				/* If we get a LeaveNotify while a mouse button is pressed,
				   and the LeaveNotify isn't grab/ungrab related, we should
				   delay the OnMouseLeave() call to core, because this is what
				   core expects. */
				pending_leave = true;
			}
		}
		break;
	}
	case ButtonPress:
	case ButtonRelease:
	{
		OpMouseListener *listener = opview->GetMouseListener();
		if (listener)
		{
			OpPoint pos(event->xbutton.x, event->xbutton.y);
			last_mouse_x = pos.x;
			last_mouse_y = pos.y;
			PointToGlobal(&last_mouse_x, &last_mouse_y);
			int button = event->xbutton.button;
			if (button >= 1 && button <= 3)
			{
				MouseButton operabutton = InputUtils::TranslateButton(button);

				if (event->type == ButtonPress)
				{
					int clickcount = InputUtils::GetClickCount();
					listener->OnMouseDown(operabutton, clickcount, opview);
					return true;
				}
				else
				{
					listener->OnMouseUp(operabutton, opview);
					return true;
				}
			}
			else if (event->type == ButtonPress && (button == 4 || button == 5))
			{
				listener->OnMouseWheel(button == 4 ? -2 : 2, TRUE);
				return true;
			}

			if (opview && pending_leave && !InputUtils::GetButtonFlags())
			{
				pending_leave = false;
				listener->OnMouseLeave();
				return true;
			}
		}
		break;
	}
	default:
		eaten = false;
		break;
	}
	return X11Widget::HandleEvent(event) || eaten;
}

void X11ViewWidget::ToViewCoords(int *x, int *y)
{
	int px, py;
	GetPos(&px, &py);
	if (x) *x -= px;
	if (y) *y -= py;
}

void X11ViewWidget::FromViewCoords(int *x, int *y)
{
	int px, py;
	GetPos(&px, &py);
	if (x) *x += px;
	if (y) *y += py;
}

void X11ViewWidget::HandleCallBack(INTPTR ev)
{
	if (opview)
		opview->PaintInvalidatedRegion();
}

void X11ViewWidget::HandleAllExposures(XEvent *event)
{
	OpPaintListener *listener = opview->GetPaintListener();
	if (!listener)
		return;

	OpRect rects[100];
	int nrects = 0;
	XEvent *e = event;
	XEvent more;

	int width, height;
	GetSize(&width, &height);

	if (!e)
	{
		if (XCheckWindowEvent(display, handle, ExposureMask, &more))
			e = &more;
	}

	while (e && nrects < 100)
	{
		if (e->type == Expose || e->type == GraphicsExpose)
		{
			int x = e->xexpose.x;
			int y = e->xexpose.y;
			int x2 = x + e->xexpose.width;
			int y2 = y + e->xexpose.height;

			if (x < 0)
				x = 0;
			if (y < 0)
				y = 0;
			if (x2 > width)
				x2 = width;
			if (y2 > height)
				y2 = height;

			rects[nrects++] = OpRect(x, y, x2, y2);

			int matchrect = -1;
			for (int j=0; j<nrects; j++)
			{
				/* note that width and height is really right and bottom here */
				int dist=0;
				if (x > rects[j].width)
					dist += x - rects[j].width;
				else if (rects[j].x > x2)
					dist += rects[j].x - x2;
				if (y > rects[j].height)
					dist += y - rects[j].height;
				else if (rects[j].y > y2)
					dist += rects[j].y - y2;

				if (dist < 100)
				{
					if (matchrect < 0)
					{
						matchrect = j;
						if (rects[j].width < x2)
							rects[j].width = x2;
						if (rects[j].height < y2)
							rects[j].height = y2;
						if (rects[j].x > x)
							rects[j].x = x;
						if (rects[j].y > y)
							rects[j].y = y;
					}
					else
					{
						/* merge oprects j and matchrect */
						if (rects[matchrect].width < rects[j].width)
							rects[matchrect].width = rects[j].width;
						if (rects[matchrect].height < rects[j].height)
							rects[matchrect].height = rects[j].height;
						if (rects[matchrect].x > rects[j].x)
							rects[matchrect].x = rects[j].x;
						if (rects[matchrect].y > rects[j].y)
							rects[matchrect].y = rects[j].y;
						rects[j] = rects[nrects-1];
						nrects --;
					}
				}
			}
			if (nrects == 100)
				break;
		}
#ifdef DEBUG
		else
		{
			if (e->type != NoExpose)
			{
				// If the event type is anything else than this, we're in trouble.
				printf("Event type: %d\n", e->type);
				OP_ASSERT(!"Unexpected event type");
			}
		}
#endif
		if (XCheckWindowEvent(display, handle, ExposureMask, &more))
			e = &more;
		else
			e = 0;
	}
	if (!nrects)
		return;

	listener->BeforePaint();
	for (int i=0; i<nrects; i++)
	{
		int x = rects[i].x;
		int y = rects[i].y;
		FromViewCoords(&x, &y);
		opview->GenerateOnPaint(OpRect(x, y, rects[i].width-rects[i].x,
									   rects[i].height-rects[i].y),
								false);
	}
}

bool X11ViewWidget::IsButtonPressed(int num)
{
	return !!(InputUtils::GetButtonFlags() & (1 << num));
}

void X11ViewWidget::GetLastMouseEventPos(int *x, int *y)
{
	*x = last_mouse_x;
	*y = last_mouse_y;
}

#endif // !VEGA_OPPAINTER_SUPPORT
