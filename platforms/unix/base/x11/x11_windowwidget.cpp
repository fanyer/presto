/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "x11_windowwidget.h"

#include "inpututils.h"
#include "x11_globals.h"
#include "x11_opmessageloop.h"
#include "x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"


X11WindowWidget *X11WindowWidget::m_active_window_widget = 0;
X11Types::Window X11WindowWidget::m_active_topwindow_handle = None;


X11WindowWidget::X11WindowWidget(X11OpWindow* op_window)
	: m_op_window(op_window)
	, m_return_focus_to(None)
{
}

X11WindowWidget::~X11WindowWidget()
{
	if (m_active_window_widget == this)
	{
		X11OpWindow *window = m_active_window_widget->m_op_window;
		if (window && window->GetWindowListener())
		{
			OpWindowListener *listener = window->GetWindowListener();
			listener->OnActivate(FALSE);
		}
		m_active_window_widget = 0;
	}
}

void X11WindowWidget::Show()
{
	OpWindowListener *listener = m_op_window ? m_op_window->GetWindowListener() : 0;
	int width, height;
	GetSize(&width, &height);
	if (listener)
		listener->OnResize(width, height);
	if (listener)
		listener->OnShow(TRUE);
	X11Widget::Show();
}

void X11WindowWidget::ShowBehind()
{
	Show();
	XRaiseWindow(g_x11->GetDisplay(), X11WindowWidget::GetActiveTopWindowHandle());
	m_return_focus_to = X11WindowWidget::GetActiveTopWindowHandle();
	PostCallback(0);
}

void X11WindowWidget::HandleCallBack(INTPTR ev)
{
	if (m_return_focus_to != None)
	{
		XSetInputFocus(m_display, m_return_focus_to, RevertToParent, CurrentTime);
		m_return_focus_to = None;
	}
	X11Widget::HandleCallBack(ev);
}

void X11WindowWidget::Hide()
{
	X11Widget::Hide();
	if (m_op_window)
	{
		OpWindowListener *listener = m_op_window->GetWindowListener();
		if (listener)
			listener->OnShow(FALSE);
	}
}


BOOL X11WindowWidget::ConfirmClose()
{
	if( m_op_window )
		m_op_window->UiClosing();
	return TRUE;
}


bool X11WindowWidget::HandleEvent(XEvent *event)
{
	bool eaten = true;

	switch (event->type)
	{
	case FocusIn: /* fallthrough */
	case FocusOut:
#if 0
		printf("type:   ");
		if (event->type == FocusIn)
			printf("FocusIn");
		else if (event->type == FocusOut)
			printf("FocusOut");
		printf("  %08x, windowwidget: %p, %08x", (int)event->xfocus.window, this, (int)m_handle);
		puts("");

		printf("mode:   ");
		if (event->xfocus.mode==NotifyNormal)
			printf("NotifyNormal");
		else if (event->xfocus.mode==NotifyGrab)
			printf("NotifyGrab");
		else if (event->xfocus.mode==NotifyUngrab)
			printf("NotifyUngrab");
		puts("");

		printf("detail: ");
		if (event->xfocus.detail==NotifyAncestor)
			printf("NotifyAncestor");
		else if (event->xfocus.detail==NotifyDetailNone)
			printf("NotifyDetailNone");
		else if (event->xfocus.detail==NotifyInferior)
			printf("NotifyInferior");
		else if (event->xfocus.detail==NotifyNonlinear)
			printf("NotifyNonlinear");
		else if (event->xfocus.detail==NotifyNonlinearVirtual)
			printf("NotifyNonlinearVirtual");
		else if (event->xfocus.detail==NotifyPointer)
			printf("NotifyPointer");
		else if (event->xfocus.detail==NotifyPointerRoot)
			printf("NotifyPointerRoot");
		else if (event->xfocus.detail==NotifyVirtual)
			printf("NotifyVirtual");
		puts("");
#endif

		/* I'm only interested in focus events that transfer focus
		 * into or out of a top level window.  All top level windows
		 * SHALL be X11WindowWidgets, so I only need to handle focus
		 * events for my own X11 windows here.
		 */
		if (event->xfocus.window == m_handle)
		{
			/* Ignore focus changes caused by a grabbing window
			 * (popup, dropdown, ctrl+tab) opening.
			 */
			if (event->xfocus.mode == NotifyGrab ||
				event->xfocus.mode == NotifyUngrab)
			{
				/* When a grab occurs, we may lose some events.  Since
				 * core keeps track of the state of mouse buttons and
				 * keys, this may cause confusion.  See DSK-320808 for
				 * one example.
				 *
				 * (In principle, I guess we should only do this on
				 * NotifyGrab and only when we lose focus to a
				 * non-opera window.  In practice, it is probably fine
				 * to just do it in all cases.)
				 */

				/*
				 * Only do this on NotifyUngrab since we do not know a good
				 * way to determine "when we lose focus to a non-opera window"
				 * Fixes DSK-323698
				 */
				if (event->xfocus.mode == NotifyUngrab)
					g_input_manager->ResetInput();

				return true;
			};

			/* Only handle events when focus moves out of or into this
			 * window, or between children of this window (although I
			 * don't think we get events when focus moves between
			 * children).
			 */
			if (event->xfocus.detail != NotifyPointer &&
				event->xfocus.detail != NotifyPointerRoot)
			{
				/* Tell the input method that we've lost focus.
				 */
				if (event->type == FocusOut && event->xfocus.detail != NotifyInferior)
				{
					X11Widget * widget = GetActiveWindowWidget() ? GetActiveWindowWidget()->GetNearestTopLevel() : 0;
					if (widget)
						widget->UnsetIMFocus();
				};
				/* If we received focus, make sure the focus is moved
				 * to the focus proxy.
				 */
				if (event->type == FocusIn)
				{
					X11Widget * proxy = GetFocusProxy();
					OP_ASSERT(proxy != 0);
					if (proxy != 0)
					{
						X11Types::Window current_focus = None;
						int current_revert;
						XGetInputFocus(event->xfocus.display, &current_focus, &current_revert);
						// Only move focus if it is currently on this widget or a child widget
						bool move_focus = current_focus == m_handle;
						if (!move_focus &&
							current_focus != None &&
							current_focus != proxy->GetWindowHandle())
						{
							X11Widget * current_focus_widget = g_x11->GetWidgetList()->FindWidget(current_focus);
							if (current_focus_widget->GetNearestTopLevel() == this)
								move_focus = true;
						};
						if (move_focus)
						{
							/* Using CurrentTime here is wrong, but
							 * hopefully we'll never get here if the
							 * WM sends WM_TAKE_FOCUS messages.  And
							 * the XGetInputFocus() check above should
							 * limit the race condition somewhat.
							 */
							XSetInputFocus(m_display, proxy->GetWindowHandle(), RevertToParent, CurrentTime);
						};
					};
				}; // move focus to focus proxy

				/* With that out of the way, we need to notify opera
				 * that this top-level window has gained or lost
				 * focus.
				 */
				/* Only deal with events that indicates that focus has
				 * crossed into or out of this window.  NotifyInferior
				 * indicates that this window has lost/gained focus
				 * to/from a child window.  If focus moves between two
				 * child windows, this window should not get any focus
				 * events at all.
				 */
				if (event->xfocus.detail != NotifyInferior)
				{
					bool focus_in = event->type == FocusIn;
					if ((m_active_window_widget == this) ^ focus_in)
					{
						X11Widget* top_level_widget = 0;
						if (!focus_in)
							top_level_widget = GetActiveToplevelWidget();

						m_active_window_widget = focus_in ? this : 0;

						X11Widget* widget = m_active_window_widget ? m_active_window_widget->GetActiveToplevelWidget() : 0;
						m_active_topwindow_handle = widget ? widget->GetWindowHandle() : None;

						if (focus_in)
							top_level_widget = GetActiveToplevelWidget();

						if (top_level_widget && top_level_widget == GetNearestTopLevel())
						{
							if(focus_in)
								top_level_widget->OnToplevelActivate();
							else
								top_level_widget->OnToplevelDeactivate();
						}

						OpWindowListener *listener = m_op_window->GetWindowListener();
						if (listener)
							listener->OnActivate(focus_in);
					}
				}; // Notify opera about gained/lost focus

				/* Tell the input method that we've gained focus.
				 */
				if (event->type == FocusIn)
				{
					X11Widget * widget = GetActiveWindowWidget() ? GetActiveWindowWidget()->GetNearestTopLevel() : 0;
					if (widget)
					{
						widget->SetIMFocusWindow(widget->GetFocusProxy());
						widget->SetIMFocusIfActive();
					};
				};

			}; // if (focus moved to/from a relevant window)
		}; // if (focus event belongs to this window)


		break;

	default:
		eaten = false;
		break;
	}

	return X11Widget::HandleEvent(event) || eaten;
}

void X11WindowWidget::HandleResize()
{
	int w, h;
	GetSize(&w, &h);

	if( m_op_window )
	{
		int t, b, l, r;
		GetContentMargins(t, b, l, r);
		m_op_window->SetInnerSizeFromPlatform(w - l - r, h - t - b);
	}


	X11Widget::HandleResize();
}

//static 
X11WindowWidget* X11WindowWidget::GetActiveWindowWidget()
{
	return m_active_window_widget;
}

//static 
X11Widget* X11WindowWidget::GetActiveToplevelWidget()
{
	X11Widget* widget = GetActiveWindowWidget();
	return widget ? widget->GetTopLevel() : 0;
}

//static 
X11Types::Window X11WindowWidget::GetActiveTopWindowHandle()
{
	return m_active_topwindow_handle;
}
