/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_opdragmanager.h"

#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/formats/uri_escape.h"
#include "modules/inputmanager/inputmanager.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"

#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_cursor.h"
#include "platforms/unix/base/x11/x11_dropmanager.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_dragwidget.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/unix/base/x11/x11_viewwidget.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/quix/environment/DesktopEnvironment.h"
#include "platforms/utilix/x11_all.h"


#include <X11/cursorfont.h>

using X11Types::Atom;


// The '_NETSCAPE_URL' type cause problems in Nautilus. Multi file drops fail (DSK-365626 mentions it), but we have to use it as handling of text/url-list is broken. See DSK-368636.
#define SUPPORT_NETSCAPE_URL

// For a lot of debugging statements
//#define DEBUG_DRAG

X11OpDragManager* g_x11_op_dragmanager = 0;


OP_STATUS OpDragObject::Create(OpDragObject*& object, OpTypedObject::Type type)
{
	RETURN_OOM_IF_NULL(object = OP_NEW(X11OpDragObject, (type)));
	return OpStatus::OK;
}


DropType X11OpDragObject::GetSuggestedDropType() const
{
	switch (InputUtils::GetOpModifierFlags())
	{
		case 0:
			return DesktopDragObject::GetSuggestedDropType();
		break;

		case SHIFTKEY_CTRL:
			return DROP_COPY;
		break;

		case SHIFTKEY_SHIFT:
			return DROP_MOVE;
		break;

		case SHIFTKEY_ALT:
			return DROP_UNINITIALIZED;
		break;

		case SHIFTKEY_CTRL|SHIFTKEY_SHIFT:
			return DROP_LINK;
		break;

		case SHIFTKEY_CTRL|SHIFTKEY_ALT:
			return DROP_COPY;
		break;

		case SHIFTKEY_SHIFT|SHIFTKEY_ALT:
			return DROP_MOVE;
		break;

		case SHIFTKEY_CTRL|SHIFTKEY_SHIFT|SHIFTKEY_ALT:
			return DROP_LINK;
		break;
	}
	return DROP_NONE;
}


// static
OP_STATUS OpDragManager::Create(OpDragManager*& manager)
{
	if (g_x11_op_dragmanager)
		return OpStatus::ERR;

	g_x11_op_dragmanager = OP_NEW(X11OpDragManager, ());
	if (!g_x11_op_dragmanager)
		return OpStatus::ERR_NO_MEMORY;
	manager = g_x11_op_dragmanager;
	return OpStatus::OK;
}


X11OpDragManager::X11OpDragManager()
	:m_remote_version(-1)
	,m_timer_action(Idle)
	,m_canceled_with_keypress(FALSE)
	,m_wait_for_status(FALSE)
	,m_force_send_position(FALSE)
	,m_has_ever_received_status(FALSE)
	,m_always_send_position(FALSE)
	,m_drop_sent(FALSE)
	,m_drag_accepted(FALSE)
	,m_send_drop_on_status_received(FALSE)
	,m_send_leave_on_status_received(FALSE)
	,m_can_start_drag(FALSE)
	,m_is_stopping(FALSE)
	,m_is_dragging(false)
	,m_current_target(None)
	,m_current_proxy_target(None)
	,m_current_action(None)
	,m_start_screen(0)
	,m_current_screen(0)
	,m_drag_source_window(NULL)
{
}

void X11OpDragManager::SetDragObject(OpDragObject* drag_object)
{
	if (!drag_object || drag_object != m_drag_object.get())
	{
		StopDrag(FALSE);
		m_drag_object.reset(static_cast<DesktopDragObject*>(drag_object));
	}

	m_is_dragging = !!m_drag_object.get();
	if (m_is_dragging)
		m_drag_object->SynchronizeCoreContent();
}


void X11OpDragManager::StartDrag()
{
	// DSK-365756. If mouse has been released before core responds we must prevent the drag from starting
	// DSK-369037. We must also prevent an illegal mouse combination from starting a drag
	MouseButton drag_button = InputUtils::X11ToOpButton(InputUtils::GetX11DragButton());
	if (!InputUtils::IsOpButtonPressed(drag_button, true))
	{
		// The drag source will remain valid after a mouse button has been released while
		// the captured widget will be set in situations when drag source is NULL (like when the
		// drag is started from ui and not core - DSK-369037)
		X11Widget* drag_source = g_x11->GetWidgetList()->GetDragSource();
		if (!drag_source)
			drag_source = g_x11->GetWidgetList()->GetCapturedWidget();
		if (drag_source && drag_source->GetMdeScreen())
			drag_source->GetMdeScreen()->TrigDragCancel(InputUtils::GetOpModifierFlags());
		return;
	}

	if (!m_drag_object.get() || !m_can_start_drag)
		return;

	OpAutoPtr<OpTimer> timer(OP_NEW(OpTimer,()));
	if (!timer.get())
		m_drag_object.reset();

	m_timer = timer;
	m_timer->SetTimerListener(this);

	// Core assumes StartDrag() to return before a drag starts
	m_timer_action = Start;
	m_timer->Start(0);
}

OP_STATUS X11OpDragManager::Exec()
{
	m_is_dragging = true; // should StartDrag() be called twice without calling SetDragObject()

	// 'm_can_start_drag' can be set to FALSE during the timeout
	if (m_can_start_drag)
	{
		m_drag_source_window = X11Widget::GetDragSource();
		g_application->GetDesktopWindowCollection().AddListener(this);

		m_start_screen = m_current_screen = InputUtils::GetScreenUnderPointer();
		RETURN_IF_ERROR(MakeDragWidget(m_current_screen));

		SaveCursor();
		UpdateCursor(FALSE);

		X11Types::Display* display = g_x11->GetDisplay();
		X11Types::Window window = g_x11->GetAppWindow();

		XSetSelectionOwner(display, ATOMIZE(XdndSelection), window, CurrentTime);

#ifdef DEBUG_DRAG
		printf("(Exec) Enter drag-n-drop loop\n");
#endif
		X11OpMessageLoop::Self()->NestLoop();
#ifdef DEBUG_DRAG
		printf("(Exec) Leave drag-n-drop loop\n");
#endif

		m_drag_source_window = NULL;
		g_application->GetDesktopWindowCollection().RemoveListener(this);

		RestoreCursor();

		// Core does not receive any mouse events after the drag started
		// Emulate a mouse move to update cursor shape if mouse is inside an
		// opera window.
		GenerateMouseMove();
	}

	OnStopped(m_canceled_with_keypress); // Do not delete drag object here. Core may use it

	g_widget_globals->captured_widget = NULL; // Prevent issue DSK-293919
	g_input_manager->ResetInput();
	m_is_dragging = false; // Always reset here, core will otherwise believe drag is still active

	return OpStatus::OK;
}


void X11OpDragManager::OnDesktopWindowAdded(DesktopWindow* window)
{
	// The call to SendXdndLeave() is required should a new tab open.
	if (m_current_target != None)
		SendXdndLeave();
}


void X11OpDragManager::OnDesktopWindowRemoved(DesktopWindow* window)
{
	// The call to SendXdndLeave() is required a tab close.
	if (m_current_target != None)
		SendXdndLeave();
	if (m_drag_source_window == window)
		m_drag_source_window = NULL;
}


void X11OpDragManager::StopDrag(BOOL canceled)
{
	OnStopped(canceled);

	// There is only one listener. The drop manager.
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnDragObjectDeleted(GetDragObject());

	m_drag_object.reset();

	m_is_dragging = false;
}


void X11OpDragManager::OnStopped(BOOL canceled)
{
	// Protect against recursion. Calling OnDragEnded() can result in
	// a call to core which in some states call StopDrag() and then OnStopped() again
	if (m_is_stopping)
		return;

	m_is_stopping = TRUE;

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnDragEnded(m_drag_object.get(), canceled);

	m_timer.reset();
	m_drag_widget.reset();

	if (m_timer_action == WaitForStatus_OrExit || m_timer_action == WaitForFinish_OrExit)
	{
		// This condition will always happen if we drop an OpTypedObject::DRAG_TYPE_WINDOW
		// object onto an opera window (page bar). The detaching will call StopDrag() which
		// kills the timer driven timeout we wait for to break the exec loop
		X11OpMessageLoop::Self()->StopCurrent();
	}

	m_remote_version = -1;
	m_timer_action = Idle;
	m_current_target = None;
	m_current_proxy_target = None;
	m_current_action = None;
	m_nochange_rect.SetValid(FALSE);
	m_cached_pos.SetValid(FALSE);
	m_canceled_with_keypress = FALSE;
	m_wait_for_status = FALSE;
	m_force_send_position = FALSE;
	m_has_ever_received_status = FALSE;
	m_always_send_position = FALSE;
	m_drop_sent = FALSE;
	m_drag_accepted = FALSE;
	m_send_drop_on_status_received = FALSE;
	m_send_leave_on_status_received = FALSE;
	m_start_screen = 0;
	m_current_screen = 0;

	m_is_stopping = FALSE;
}


// static
void X11OpDragManager::GenerateMouseMove()
{
	int screen = InputUtils::GetScreenUnderPointer();
	if (screen < 0)
	{
		return;
	}

	X11Types::Display* display = g_x11->GetDisplay();
	X11Types::Window root = RootWindow(display, screen);

	X11Types::Window root_window, window;
	int rx, ry, wx, wy;
	unsigned int mask;

	X11Types::Window parent = root;

	while (1)
	{
		X11Types::Bool status = XQueryPointer(display, parent, &root_window, &window, &rx, &ry, &wx, &wy, &mask );
		if (status == True)
		{
			if (window == None)
			{
				break;
			}
			parent = window;
		}
		else
		{
			break;
		}
	}

	XEvent event;
	event.xmotion.type = MotionNotify;
	event.xmotion.display = display;
	event.xmotion.window = parent;
	event.xmotion.root = root;
	event.xmotion.subwindow = None;
	event.xmotion.time = X11OpMessageLoop::GetXTime();
	event.xmotion.x = wx;
	event.xmotion.y = wy;
	event.xmotion.x_root = rx;
	event.xmotion.y_root = ry;
	event.xmotion.state = 0;
	event.xmotion.is_hint = NotifyNormal;
	event.xmotion.same_screen = True;

	XSendEvent(display, parent, True, 0, &event);
}

BOOL X11OpDragManager::IsDragging()
{
	if (g_drop_manager && g_drop_manager->GetDropObject())
		return TRUE;

	return m_is_dragging;
}

DesktopDragObject* X11OpDragManager::GetDragObject()
{
	// The drop manager holds the drag object if the source is outside opera
	if (g_drop_manager && g_drop_manager->GetDropObject())
		return g_drop_manager->GetDropObject();
	return m_drag_object.get();
}


BOOL X11OpDragManager::StateMachineRunning()
{
	return m_timer.get() && m_drag_object.get() && m_timer_action != Start && !m_canceled_with_keypress;
}


void X11OpDragManager::OnTimeOut(OpTimer* timer)
{
	if (timer == m_timer.get())
	{
		switch (m_timer_action)
		{
			case Start:
				m_timer_action = Idle;
				Exec(); // Blocks. Returns after X11OpMessageLoop::Self()->StopCurrent() is called
				return;
			break;

			case WaitForStatus_OrSendLeave:
				if (m_current_target != None)
					SendXdndLeave();
			break;

			case WaitForStatus_OrExit:
				if (m_current_target != None)
					SendXdndLeave();
				X11OpMessageLoop::Self()->StopCurrent();
			break;

			case WaitForFinish_OrExit:
				X11OpMessageLoop::Self()->StopCurrent();
			break;

			default:
				return;
		}
		m_timer_action = Idle;
	}
}


// For xdnd version 4
X11Types::Window X11OpDragManager::GetProxy(X11Types::Window window)
{
	X11Types::Display* display = g_x11->GetDisplay();
	X11Types::Window proxy_window = None;

	Atom type = 0;
	int format;
	unsigned long dummy;
	unsigned char* data = NULL;

    int rc = XGetWindowProperty(display, window, ATOMIZE(XdndProxy), 0, 1, False,
								XA_WINDOW, &type, &format, &dummy, &dummy, &data);
	if (rc == Success && type == XA_WINDOW && data && format == 32)
	{
		proxy_window = *(X11Types::Window*)data;

		// Verify that proxy window exists and that its XdndProxy resource refers to itself

		g_x11->SetX11ErrBadWindow(false);

		rc = XGetWindowProperty(display, proxy_window, ATOMIZE(XdndProxy), 0, 1, False,
								XA_WINDOW, &type, &format, &dummy, &dummy, &data);

		if (g_x11->GetX11ErrBadWindow() || rc != Success || type != XA_WINDOW || !data ||
			proxy_window != *(X11Types::Window*)data)
		{
			proxy_window = None;
		}
	}

	if (data)
	{
		XFree(data);
	}

	return proxy_window;
}


BOOL X11OpDragManager::HandleOneEvent(int type)
{
	if (StateMachineRunning() || m_canceled_with_keypress)
	{
		XEvent event;
		if (XCheckTypedEvent(g_x11->GetDisplay(), type, &event) == True)
		{
			return HandleEvent(&event);
		}
	}
	return FALSE;
}


BOOL X11OpDragManager::HandleEvent(XEvent* event)
{
	// This is a filter that defines when it is possible to start a drag. The reason
	// why we need this is that xdnd and X11 in general consumes the ButtonRelease
	// event that completes a drag-n-drop handshake. Opera code often assumes that
	// that is not the case since (at least) the windows implementation does not.
	switch(event->type)
	{
		case ButtonPress:
			m_can_start_drag = m_timer_action == Idle && event->xbutton.button == InputUtils::GetX11DragButton();
		break;

		case ButtonRelease:
			if (event->xbutton.button == InputUtils::GetX11DragButton())
				m_can_start_drag = FALSE;
		break;
	}

	if (StateMachineRunning() || m_canceled_with_keypress)
	{
		if (m_canceled_with_keypress)
		{
			// Eat mouse up that is left if canceling with Escape
			if (event->type == ButtonRelease && event->xbutton.button == InputUtils::GetX11DragButton())
			{
				X11OpMessageLoop::Self()->StopCurrent();
				m_timer_action = Idle;
				return TRUE;
			}
			else if (event->type == MotionNotify || event->type == ButtonRelease ||
					 event->type == KeyPress || event->type == KeyRelease)
			{
				// Eat input events until correct mouse button has been released
				return TRUE;
			}

			if (event->type == SelectionRequest && event->xselectionrequest.selection == ATOMIZE(XdndSelection))
			{
				HandleSelectionRequest(event);
				return TRUE;
			}

			return FALSE;
		}

		switch (event->type)
		{
			case SelectionClear:
				if (event->xselectionclear.selection == ATOMIZE(XdndSelection))
				{
					return TRUE;
				}
				break;

			case SelectionRequest:
				if (event->xselectionrequest.selection == ATOMIZE(XdndSelection))
				{
					HandleSelectionRequest(event);
					return TRUE;
				}
				break;

			case SelectionNotify:
				if (event->xselection.selection == ATOMIZE(XdndSelection))
				{
					return TRUE;
				}
				break;

			case ClientMessage:
			{
				if (event->xclient.message_type == ATOMIZE(XdndStatus))
				{
					X11Types::Window window = (X11Types::Window)event->xclient.data.l[0];
					if ( window && window != m_current_target)
					{
#ifdef DEBUG_DRAG
						printf("(XdndStatus) Window mismatch 0x%08x vs (expected) 0x%08x\n", (unsigned int)window, (unsigned int)m_current_target);

#endif
						return TRUE;
					}

					long flag   = event->xclient.data.l[1];
					long pos    = event->xclient.data.l[2];
					long size   = event->xclient.data.l[3];
					long action = event->xclient.data.l[4];

					m_drag_accepted = flag&0x1 ? TRUE : FALSE;
					m_always_send_position = flag&0x2 ? TRUE : FALSE;
					m_current_action = m_drag_accepted ? action : None;

					m_nochange_rect.SetValid(TRUE);
					m_nochange_rect.x = (pos >> 16);
					m_nochange_rect.y = pos & 0xFFFF;
					m_nochange_rect.width = (size >> 16);
					m_nochange_rect.height = size & 0xFFFF;

#ifdef DEBUG_DRAG
					for (int i=0; i<5; i++)
						printf("(XdndStatus) %d: %08x\n", i, (unsigned int)event->xclient.data.l[i]);
					printf("(XdndStatus) Rect: [%d %d %d %d]\n", m_nochange_rect.x, m_nochange_rect.y,
						   m_nochange_rect.width, m_nochange_rect.height);
					printf("(XdndStatus) always send position: %d\n", m_always_send_position);
					if (action != 0)
					{
						char* name = XGetAtomName(g_x11->GetDisplay(), action);
						if (name)
						{
							printf("(XdndStatus) action: %s\n", name);
							XFree(name);
						}
					}
					else
						printf("(XdndStatus) action: %s\n", "None");
#endif

					m_timer_action = Idle;
					m_wait_for_status = FALSE;
					m_has_ever_received_status = TRUE;

					if (m_send_drop_on_status_received)
					{
						m_send_drop_on_status_received = FALSE;
						m_cached_pos.SetValid(FALSE);
						if (m_drag_accepted)
						{
							SendXdndDrop();
							m_timer_action = WaitForFinish_OrExit;
							m_timer->Start(2000);
						}
					}
					else if (m_send_leave_on_status_received)
					{
						SendXdndLeave();
						X11OpMessageLoop::Self()->StopCurrent();
						m_timer_action = Idle;
						return TRUE;
					}

					if (m_cached_pos.IsValid())
					{
						HandleMove(m_cached_pos.x, m_cached_pos.y, m_cached_pos.screen, m_force_send_position);
						m_cached_pos.SetValid(FALSE);
						m_force_send_position = FALSE; // m_force_send_position only TRUE if m_cached_pos.IsValid() is TRUE
					}

					UpdateCursor(m_drag_accepted);

					return TRUE;
				}
				else if (event->xclient.message_type == ATOMIZE(XdndFinished))
				{
#ifdef DEBUG_DRAG
					printf("(XdndFinished) received\n");
#endif
					X11OpMessageLoop::Self()->StopCurrent();
					m_timer_action = Idle;
					return TRUE;
				}
				break;
			}

			case MotionNotify:
			{
				m_current_screen = InputUtils::GetScreenUnderPointer();

				if (m_drag_widget->GetScreen() != m_current_screen)
				{
					RETURN_VALUE_IF_ERROR(MakeDragWidget(m_current_screen), TRUE);
				}

				HandleMove(event->xmotion.x_root, event->xmotion.y_root, m_current_screen, FALSE);
				return TRUE;
				break;
			}

			case ButtonRelease:
			{
				if (event->xbutton.button == InputUtils::GetX11DragButton())
				{
					m_drag_widget->Hide();
					RestoreCursor();

					if (m_wait_for_status && m_has_ever_received_status)
					{
						m_send_drop_on_status_received = TRUE;
						m_timer_action = WaitForStatus_OrExit;
						m_timer->Start(2000);
					}
					else if (m_drag_accepted)
					{
						SendXdndDrop();
						m_timer_action = WaitForFinish_OrExit;
						m_timer->Start(2000);
					}
					else
					{
						BOOL window_dropped = FALSE;

						if (m_wait_for_status)
						{
							window_dropped = HandleWindowDrop(event->xbutton.x_root, event->xbutton.y_root);
							if (!window_dropped)
							{
								m_send_leave_on_status_received = TRUE;
								m_timer_action = WaitForStatus_OrExit;
								m_timer->Start(2000);
								return TRUE;
							}
						}

						if (m_current_target != None)
						{
							SendXdndLeave();
						}

						if (!window_dropped)
							HandleWindowDrop(event->xbutton.x_root, event->xbutton.y_root);

						X11OpMessageLoop::Self()->StopCurrent();
						m_timer_action = Idle;
					}
				}
				return !InputUtils::IsX11WheelButton(event->xbutton.button);
				break;
			}

			case ButtonPress:
			{
				// DSK-358888 Only for KDE
				if (DesktopEnvironment::GetInstance().GetToolkitEnvironment() == DesktopEnvironment::ENVIRONMENT_KDE)
				{
					if (event->xbutton.button == 2)
						OnEscapePressed(); // We emulate an XK_Escape in this case
				}

				return !InputUtils::IsX11WheelButton(event->xbutton.button);
				break;
			}

			case KeyPress:
			{
				char buffer[101];
				KeySym keysym = 0;
				XLookupString(&event->xkey, buffer, 100, &keysym, 0);
				if (keysym == XK_Escape)
				{
					OnEscapePressed();
				}
				else
				{
					SetActionList();
					// Important for targets that read modifier flags
					int screen = InputUtils::GetScreenUnderPointer();
					HandleMove(event->xkey.x_root, event->xkey.y_root, screen, TRUE);

					// Let shortcuts be avalable during d-n-d
					InputUtils::HandleEvent(event);
				}

				return TRUE;
			}
			case KeyRelease:
			{
				SetActionList();
				// Important for targets that read modifier flags
				int screen = InputUtils::GetScreenUnderPointer();
				HandleMove(event->xkey.x_root, event->xkey.y_root, screen, TRUE);

				// Let shortcuts be avalable during d-n-d. See OnDesktopWindowAdded and OnDesktopWindowRemoved
				InputUtils::HandleEvent(event);

				return TRUE;
			}
		}
	}

	return FALSE;
}


void X11OpDragManager::OnEscapePressed()
{
	// We do not leave event loop until mouse button is relased
	m_canceled_with_keypress = TRUE;
	m_drag_widget->Hide();
	RestoreCursor();

	if (m_current_target != None)
	{
		SendXdndLeave();
	}
}


void X11OpDragManager::HandleSelectionRequest(XEvent* event)
{
	if (event->type == SelectionRequest)
	{
		XEvent e;
		e.xselection.type      = SelectionNotify;
		e.xselection.display   = event->xselectionrequest.display;
		e.xselection.requestor = event->xselectionrequest.requestor;
		e.xselection.selection = event->xselectionrequest.selection;
		e.xselection.target    = event->xselectionrequest.target;
		e.xselection.property  = event->xselectionrequest.property;
		e.xselection.time      = event->xselectionrequest.time;

		X11Types::Atom type     = event->xselectionrequest.target;
		X11Types::Atom property = event->xselectionrequest.property;

		char* name = XGetAtomName(g_x11->GetDisplay(), type);
		if (name)
		{
			DragByteArray buf;
			GetMimeData(name, buf);

			XChangeProperty(e.xselection.display, e.xselection.requestor, property, type,
							8, PropModeReplace, buf.GetData(), buf.GetSize());
			XFree(name);
		}

		e.xselection.target = type;
		XSendEvent(e.xselection.display, e.xselection.requestor, True, 0, &e);
	}
}



void X11OpDragManager::HandleMove(int x, int y, int screen, BOOL force)
{
	if (m_drop_sent)
	{
		// Prevent sending new messages to target after we have released mouse
		// and sent an XdndDrop
		return;
	}

	m_drag_widget->Move(x, y);
	// Fixes missing border rectangles
	m_drag_widget->Update(0, 0, m_drag_widget->GetWidth(), m_drag_widget->GetHeight());

	X11Types::Display* display = g_x11->GetDisplay();
	X11Types::Window root = RootWindow(display, screen);

	X11Types::Window target = None;
	int tx, ty;

	X11Types::Bool ok = XTranslateCoordinates(display, root, root, x, y, &tx, &ty, &target);
	if (!ok)
	{
		// Should not happen
		return;
	}

	// Find topmost window with XdndAware set. Do not care about version yet
	if (target != root)
	{
		if (target != None)
		{
			X11Types::Window src = root;
			while (target != None)
			{
				X11Types::Window tmp;
				int tmpx, tmpy;

				ok = XTranslateCoordinates(display, src, target, tx, ty, &tmpx, &tmpy, &tmp);
				if (!ok)
				{
					target = None;
					break;
				}

				tx = tmpx;
				ty = tmpy;
				src = target;

				if (GetXdndVersion(target) != -1)
				{
					break;
				}

				ok = XTranslateCoordinates(display, src, src, tx, ty, &tmpx, &tmpy, &target);
				if (!ok)
				{
					target = None;
					break;
				}
			}
		}
	}

	if (target == None)
	{
		UpdateCursor(FALSE);
		target = root;
	}

	if (m_wait_for_status)
	{
		m_cached_pos.Set(x, y, screen);
		m_cached_pos.SetValid(TRUE);
		m_force_send_position = force;

		if (m_current_target != target && m_current_target != None)
		{
			// Spec: While wating for XdndStatus, send XdndLeave immediately if no
			// XdndStatus has ever been received from current window or start a timer
			// and send XdndLeave if timer expires before XdndStatus has been received
			if (!m_has_ever_received_status)
			{
				SendXdndLeave();
			}
			else
			{
				m_timer_action = WaitForStatus_OrSendLeave;
				m_timer->Start(2000);
			}
		}
		return;
	}

	if (m_current_target != target)
	{
		if (m_current_target != None)
		{
			SendXdndLeave();
		}

		m_current_target = target;
		if (m_current_target != None)
		{
			// Locate proxy if any and determine xdnd protocol version
			m_current_proxy_target = GetProxy(m_current_target);
			if (m_current_proxy_target == None)
			{
				m_current_proxy_target = m_current_target;
			}
			m_remote_version = GetXdndVersion(m_current_proxy_target);

			SendXdndEnter(OpPoint(x,y));
			SendXdndPosition(OpPoint(x,y));
			m_wait_for_status = TRUE;
		}
	}
	else if (m_nochange_rect.IsValid())
	{
		// Spec says an empty rect is allowed and should be treated as "always send"
		BOOL inside = m_nochange_rect.Contains(OpPoint(x,y));

		if (m_nochange_rect.IsEmpty() || !inside || (m_always_send_position && inside))
		{
			SendXdndPosition(OpPoint(x,y));
			m_wait_for_status = TRUE;
		}
	}
	else if (force)
	{
		SendXdndPosition(OpPoint(x,y));
		m_wait_for_status = TRUE;
	}
}


BOOL X11OpDragManager::HandleWindowDrop(int x, int y)
{

	if (g_x11->GetWidgetList()->GetDropTarget() && g_x11->GetWidgetList()->GetDropTarget()->GetAcceptDrop())
		return FALSE;

	// We can currently only drop on the start screen. Opera does not support multiple screens
	if (m_current_screen == m_start_screen && m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_WINDOW)
	{
		// Make local list. The m_drag_object will be destroyed on the first detach
		OpINT32Vector list;
		for (INT32 i=0; i<m_drag_object->GetIDCount(); i++)
			RETURN_VALUE_IF_ERROR(list.Add(m_drag_object->GetID(i)), FALSE);

		for (UINT32 i=0; i<list.GetCount(); i++)
		{
			DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(list.Get(i));
			if (dw)
			{
				dw->SetParentWorkspace(NULL);
				dw->GetToplevelDesktopWindow()->SetOuterPos(x, y);
				x += 20;
				y += 20;
			}
		}
		return TRUE;
	}
	return FALSE;
}


/**
 * Note: Setting the XdndActionList is something one (per spec) does when the
 * requested action is XdndActionAsk in the XdndPosition message.
 * However, we set it before sending XdndEnter regardless of action and whenever
 * a key is pressed or relased while dragging. This is to behave the same way as
 * Gtk (Firefox) when dragging links to the Nautilus file manager or the Gnome
 * desktop. Those receivers will then automatically treat the incoming dropped
 * object as something that should be linked (not copied).
 */
void X11OpDragManager::SetActionList()
{
	X11Types::Atom list[3];
	int i=0;

	unsigned int mask = InputUtils::GetX11ModifierMask() & (ShiftMask|ControlMask);
	if (mask == ControlMask)
		list[i++] = ATOMIZE(XdndActionCopy);
	else if (mask == ShiftMask)
		list[i++] = ATOMIZE(XdndActionMove);
	else if (mask == (ControlMask|ShiftMask))
		list[i++] = ATOMIZE(XdndActionLink);
	else
	{
		list[i++] = ATOMIZE(XdndActionCopy);
		list[i++] = ATOMIZE(XdndActionMove);
		list[i++] = ATOMIZE(XdndActionLink);
	}

	XChangeProperty(g_x11->GetDisplay(), g_x11->GetAppWindow(), ATOMIZE(XdndActionList),
					XA_ATOM, 32, PropModeReplace, (unsigned char *)list, i);
}



void X11OpDragManager::SendXdndEnter(const OpPoint& pos)
{
#ifdef DEBUG_DRAG
	printf("SendXdndEnter(): Target: 0x%08x Version: %d\n", (unsigned int)m_current_target, m_remote_version);
#endif

	int version = MIN(GetProtocolVersion(), m_remote_version);
	if (version < 0)
	{
		return;
	}

	SetActionList();

	XClientMessageEvent message;
	X11Types::Display* display = g_x11->GetDisplay();

	OpAutoVector<OpString8> mime_types;
	if (OpStatus::IsError(GetProvidedMimeTypes(mime_types)))
	{
		return;
	}

    long flag = 0;

	if (mime_types.GetCount() > 3)
	{
		// Save mime list as window property

		X11Types::Atom* list = OP_NEWA(X11Types::Atom, mime_types.GetCount());
		if (!list)
		{
			return;
		}
		for (UINT32 i=0; i<mime_types.GetCount(); i++)
		{
			list[i] =  XInternAtom(display, mime_types.Get(i)->CStr(), False);
		}

		XChangeProperty(display, g_x11->GetAppWindow(), ATOMIZE(XdndTypeList),
						XA_ATOM, 32, PropModeReplace,
						(unsigned char *)list, mime_types.GetCount());

		OP_DELETEA(list);

		flag |= 0x0001;
	}

	message.type = ClientMessage;
	message.window = m_current_target;
	message.format = 32;
	message.message_type = ATOMIZE(XdndEnter);
	message.data.l[0] = g_x11->GetAppWindow();
	message.data.l[1] = version << 24 | flag;
	message.data.l[2] = message.data.l[3] = message.data.l[4] = 0;
	for (UINT32 i=0; i<mime_types.GetCount() && i<3; i++)
	{
		message.data.l[i+2] = XInternAtom(display, mime_types.Get(i)->CStr(), False);
	}

	XSendEvent(display, m_current_proxy_target, False, NoEventMask, (XEvent*)&message);

	// Initial rectangle. Target will change this on first reply
	m_nochange_rect.SetValid(TRUE);
	m_nochange_rect.x = pos.x - 2;
	m_nochange_rect.y = pos.y - 2;
	m_nochange_rect.width  = 5;
	m_nochange_rect.height = 5;
}



void X11OpDragManager::SendXdndPosition(const OpPoint& pos)
{
#ifdef DEBUG_DRAG
	printf("SendXdndPosition(): Target: 0x%08x\n", (unsigned int)m_current_target);
#endif

	XClientMessageEvent message;
	X11Types::Display* display = g_x11->GetDisplay();

	unsigned int mask = InputUtils::GetX11ModifierMask() & (ShiftMask|ControlMask);
	X11Types::Atom action;
	if (mask == ControlMask)
		action = ATOMIZE(XdndActionCopy);
	else if (mask == ShiftMask)
		action = ATOMIZE(XdndActionMove);
	else if (mask == (ControlMask|ShiftMask))
		action = ATOMIZE(XdndActionLink);
	else
		action = ATOMIZE(XdndActionCopy);

	message.type   = ClientMessage;
	message.window = m_current_target;
	message.format = 32;
	message.message_type = ATOMIZE(XdndPosition);
	message.data.l[0] = g_x11->GetAppWindow();
	message.data.l[1] = (0xFF & InputUtils::GetX11ModifierMask());
	message.data.l[2] = pos.x << 16 | pos.y;
	message.data.l[3] = X11OpMessageLoop::GetXTime();
	message.data.l[4] = action;

	XSendEvent(display, m_current_proxy_target, False, NoEventMask, (XEvent*)&message);
}


void X11OpDragManager::SendXdndLeave()
{
#ifdef DEBUG_DRAG
	printf("SendXdndLeave(): Target: 0x%08x\n", (unsigned int)m_current_target);
#endif

	XClientMessageEvent message;
	X11Types::Display* display = g_x11->GetDisplay();

    message.type = ClientMessage;
    message.window = m_current_target;
    message.format = 32;
    message.message_type = ATOMIZE(XdndLeave);
    message.data.l[0] = g_x11->GetAppWindow();
    message.data.l[1] = 0;
    message.data.l[2] = 0;
    message.data.l[3] = 0;
    message.data.l[4] = 0;

	XSendEvent(display, m_current_proxy_target, False, NoEventMask, (XEvent*)&message);

	m_current_target = None;
	m_current_proxy_target = None;
	m_nochange_rect.SetValid(FALSE);
	m_cached_pos.SetValid(FALSE);
	m_wait_for_status = FALSE;
	m_has_ever_received_status = FALSE;
	m_always_send_position = FALSE;
	m_drag_accepted = FALSE;
	m_send_drop_on_status_received = FALSE;
}


void X11OpDragManager::SendXdndDrop()
{
#ifdef DEBUG_DRAG
	printf("SendXdndDrop(): Target: 0x%08x\n", (unsigned int)m_current_target);
#endif

	XClientMessageEvent message;
	X11Types::Display* display = g_x11->GetDisplay();

	message.type = ClientMessage;
    message.window = m_current_target;
    message.format = 32;
    message.message_type = ATOMIZE(XdndDrop);
    message.data.l[0] = g_x11->GetAppWindow();
    message.data.l[1] = 0;
    message.data.l[2] = X11OpMessageLoop::GetXTime();
    message.data.l[3] = 0;
    message.data.l[4] = 0;

	XSendEvent(display, m_current_proxy_target, False, NoEventMask, (XEvent*)&message);

	m_drop_sent = TRUE;
	m_current_target = None;
	m_current_proxy_target = None;
	m_nochange_rect.SetValid(FALSE);
	m_cached_pos.SetValid(FALSE);
}




int X11OpDragManager::GetXdndVersion(X11Types::Window window)
{
	X11Types::Display* display = g_x11->GetDisplay();

	int version = -1;

	X11Types::Atom rettype;
	int format;
	unsigned long nitems;
	unsigned long bytes_remaining;
	unsigned char *data;

	int rc = XGetWindowProperty(display, window, ATOMIZE(XdndAware), 0, 1, False, AnyPropertyType, &rettype, &format, &nitems, &bytes_remaining, &data);
	if (rc == Success && data)
	{
		version = *(int *)data;
	}

	if (data)
	{
		XFree(data);
	}

	return version;
}


OP_STATUS X11OpDragManager::MakeDragWidget(int screen)
{
	m_drag_widget.reset();

	OpAutoPtr<X11DragWidget> drag_widget(OP_NEW(X11DragWidget,(this, m_drag_object.get(), OpPoint(12,18))));
	if (!drag_widget.get())
	{
		OnStopped(FALSE);
		return OpStatus::ERR_NO_MEMORY;
	}
	m_drag_widget = drag_widget;

	OP_STATUS res = m_drag_widget->Init(0, 0, X11Widget::TRANSIENT | X11Widget::BYPASS_WM | X11Widget::TRANSPARENT, screen);
	if (OpStatus::IsError(res))
	{
		OnStopped(FALSE);
		return res;
	}

	m_drag_widget->InstallIcon();

	OpPoint pos;
	if (InputUtils::GetGlobalPointerPos(pos) )
		m_drag_widget->Move(pos.x, pos.y);

	m_drag_widget->Show();

	return OpStatus::OK;
}


void X11OpDragManager::UpdateCursor(BOOL accept)
{
	// TODO: Save pointer to captured widget as member and use X11WidgetListener::OnDeleted to
	// zero it. Cleaner but not necessary

	X11Widget* captured_widget = g_x11->GetWidgetList()->GetCapturedWidget();
	if (captured_widget)
	{
		if (accept)
		{
			if (m_current_action == ATOMIZE(XdndActionCopy) )
				captured_widget->SetCursor(CURSOR_DROP_COPY);
			else if (m_current_action == ATOMIZE(XdndActionLink) )
				captured_widget->SetCursor(CURSOR_DROP_LINK);
			else if (m_current_action == ATOMIZE(XdndActionMove) )
				captured_widget->SetCursor(CURSOR_DROP_MOVE);
			else
				captured_widget->SetCursor(CURSOR_DEFAULT_ARROW);
		}
		else if (m_drag_object.get() && m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_WINDOW)
		{
			// We do not support window reparenting onto a new screen
			if (!accept || m_current_screen != m_start_screen)
				captured_widget->SetCursor(CURSOR_NO_DROP);
			else
				captured_widget->SetCursor(CURSOR_DEFAULT_ARROW);
		}
		else
		{
			captured_widget->SetCursor(CURSOR_NO_DROP);
		}
	}
}


void X11OpDragManager::SaveCursor()
{
	X11Widget* captured_widget = g_x11->GetWidgetList()->GetCapturedWidget();
	if (captured_widget)
		captured_widget->SaveCursor();
}


void X11OpDragManager::RestoreCursor()
{
	X11Widget* captured_widget = g_x11->GetWidgetList()->GetCapturedWidget();
	if (captured_widget)
		captured_widget->RestoreCursor();
}


OP_STATUS X11OpDragManager::GetProvidedMimeTypes(OpVector<OpString8>& mime_types)
{
	if (!m_drag_object.get())
		return OpStatus::ERR;

	OpString8* s;

	if (m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_WINDOW)
	{
		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("application/x-opera-tab"));
		RETURN_IF_ERROR(mime_types.Add(s));
	}
	else if (DragDrop_Data_Utils::HasURL(m_drag_object.get()))
	{
		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("text/uri-list"));
		RETURN_IF_ERROR(mime_types.Add(s));

		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("text/x-moz-url"));
		RETURN_IF_ERROR(mime_types.Add(s));

		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("text/x-moz-url-data"));
		RETURN_IF_ERROR(mime_types.Add(s));

		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("text/plain"));
		RETURN_IF_ERROR(mime_types.Add(s));

#if defined(SUPPORT_NETSCAPE_URL)
		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("_NETSCAPE_URL"));
		RETURN_IF_ERROR(mime_types.Add(s));
#endif

		if (m_drag_object->GetTitle())
		{
			s = OP_NEW(OpString8,());
			if (!s) return OpStatus::ERR_NO_MEMORY;
			RETURN_IF_ERROR(s->Set("text/x-moz-url-desc"));
			RETURN_IF_ERROR(mime_types.Add(s));

			s = OP_NEW(OpString8,());
			if (!s) return OpStatus::ERR_NO_MEMORY;
			RETURN_IF_ERROR(s->Set("application/x-opera-title"));
			RETURN_IF_ERROR(mime_types.Add(s));
		}
	}
	else if (DragDrop_Data_Utils::HasText(m_drag_object.get()))
	{
		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("text/unicode"));
		RETURN_IF_ERROR(mime_types.Add(s));

		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("UTF8_STRING"));
		RETURN_IF_ERROR(mime_types.Add(s));

		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("STRING"));
		RETURN_IF_ERROR(mime_types.Add(s));

		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("text/plain;charset=utf-8"));
		RETURN_IF_ERROR(mime_types.Add(s));

		s = OP_NEW(OpString8,());
		if (!s) return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(s->Set("text/plain"));
		RETURN_IF_ERROR(mime_types.Add(s));
	}

	return OpStatus::OK;
}


static OP_STATUS GetTextFromDragObject(OpString& target, bool prefer_url, DesktopDragObject* drag_object)
{
	if (prefer_url && DragDrop_Data_Utils::HasURL(drag_object))
	{
		TempBuffer buffer;
		RETURN_IF_ERROR(DragDrop_Data_Utils::GetURL(drag_object, &buffer, FALSE));
		return target.Set(buffer.GetStorage());
	}
	else if (DragDrop_Data_Utils::HasText(drag_object))
		return target.Set(DragDrop_Data_Utils::GetText(drag_object));

	return OpStatus::ERR;
}


static OP_STATUS GetUrlsFromDragObject(DragByteArray& target, OpStringC8 sep, bool append_title, DesktopDragObject* drag_object)
{
	if (drag_object->GetURLs().GetCount() == 0)
		return OpStatus::ERR;

	OpString8 all_urls;
	for (unsigned int i=0; i<drag_object->GetURLs().GetCount(); i++)
	{
		OpFile file;
		BOOL exists;
		OpStringC s = drag_object->GetURLs().Get(i)->CStr();
		bool add_localhost = OpStatus::IsSuccess(file.Construct(s)) && OpStatus::IsSuccess(file.Exists(exists)) && exists && s.Find(UNI_L("file://localhost")) != 0;

		int size = s.UTF8(0);
		if (size > 0)
		{
			OpString8 encoded;
			RETURN_OOM_IF_NULL(encoded.Reserve(size));
			s.UTF8(encoded.CStr(), size);

			// Remove everything after (and incuding) first '\r' or '\n' before we escape [DSK-365626]
			int pos = encoded.FindFirstOf("\r\n"); // strpbrk behavior
			if (pos != KNotFound)
				encoded.Delete(pos);

			if (add_localhost)
			{
				// Prepend with 'file://localhost' Some targets (like gnome desktop) need this to work
				RETURN_IF_ERROR(encoded.Insert(0, "file://localhost"));
			}
			size = encoded.Length();
			if (size > 0)
			{
				OpString8 escaped;
				RETURN_OOM_IF_NULL(escaped.Reserve(size*3));
				UriEscape::Escape(escaped.CStr(), encoded.CStr(), UriEscape::StandardUnsafe);
				RETURN_IF_ERROR(escaped.Append(sep));
				RETURN_IF_ERROR(all_urls.Append(escaped));
			}
		}
	}

	// Add title if possible. Used at least by Gnome desktop links
	if (append_title && drag_object->GetURLs().GetCount() == 1)
	{
		OpStringC tmp = DragDrop_Data_Utils::GetDescription(drag_object);
		if (tmp.HasContent())
		{
			int size = tmp.UTF8(0);
			if (size > 0)
			{
				OpString8 encoded;
				RETURN_OOM_IF_NULL(encoded.Reserve(size));
				tmp.UTF8(encoded.CStr(), size);
				RETURN_IF_ERROR(all_urls.Append(encoded));
			}
		}
	}

	return target.SetWithTerminate(reinterpret_cast<unsigned char*>(all_urls.CStr()), all_urls.Length());
}


OP_STATUS X11OpDragManager::GetMimeData(const char* mime_type, DragByteArray& buf)
{
	if (!mime_type)
		return OpStatus::ERR;

	// See X11DropManager::MakeDropObject() for notes on formats

	if (op_strcmp(mime_type, "text/x-moz-url") == 0)
	{
		if (DragDrop_Data_Utils::HasURL(m_drag_object.get()))
		{
			TempBuffer buffer;
			RETURN_IF_ERROR(DragDrop_Data_Utils::GetURL(m_drag_object.get(), &buffer, TRUE));
			OpStringC tmp1 = buffer.GetStorage();
			OpStringC tmp2 = DragDrop_Data_Utils::GetDescription(m_drag_object.get());

			int size = tmp1.UTF8(0);
			if (size > 0)
			{
				OpString8 encoded;
				encoded.Reserve(size+1);
				tmp1.UTF8(encoded.CStr(),size);
				size = encoded.Length();
				if (size > 0)
				{
					OpString8 escaped;
					escaped.Reserve(size*3+3);
					UriEscape::Escape(escaped.CStr(), encoded.CStr(), UriEscape::StandardUnsafe);
					op_strcat((char*)escaped.CStr(),"\n");

					OpString tmp;
					RETURN_IF_ERROR(tmp.Set(escaped));
					RETURN_IF_ERROR(buf.Set(0, tmp.Length()*2 + tmp2.Length()*2 + 1));

					if (tmp.CStr())
						uni_strcpy((uni_char*)buf.GetData(), tmp.CStr());
					if (tmp2.CStr())
						uni_strcat((uni_char*)buf.GetData(), tmp2.CStr());
				}
			}
			return OpStatus::OK;
		}
	}
	else if (op_strcmp(mime_type, "text/uri-list") == 0)
	{
		if (OpStatus::IsSuccess(GetUrlsFromDragObject(buf, "\r\n", false, m_drag_object.get())))
			return OpStatus::OK;
	}
	else if (op_strcmp(mime_type, "text/x-moz-url-data") == 0)
	{
		if (DragDrop_Data_Utils::HasURL(m_drag_object.get()))
		{
			TempBuffer buffer;
			RETURN_IF_ERROR(DragDrop_Data_Utils::GetURL(m_drag_object.get(), &buffer, TRUE));
			OpStringC tmp = buffer.GetStorage();

			if (tmp.HasContent())
				RETURN_IF_ERROR(buf.Set((unsigned char*)tmp.CStr(), tmp.Length()*2));

			return OpStatus::OK;
		}
	}
#if defined(SUPPORT_NETSCAPE_URL)
	else if (op_strcmp(mime_type, "_NETSCAPE_URL") == 0)
	{
		if (OpStatus::IsSuccess(GetUrlsFromDragObject(buf, "\n", true, m_drag_object.get())))
			return OpStatus::OK;
	}
#endif
	else if (op_strcmp(mime_type, "application/x-opera-title") == 0)
	{
		OpStringC tmp = DragDrop_Data_Utils::GetDescription(m_drag_object.get());
		if (tmp.HasContent())
			return buf.Set((unsigned char*)tmp.CStr(), tmp.Length()*2);
	}
	else if (op_strcmp(mime_type, "text/x-moz-url-desc") == 0)
	{
		OpStringC tmp = DragDrop_Data_Utils::GetDescription(m_drag_object.get());
		if (tmp.HasContent())
			return buf.Set((unsigned char*)tmp.CStr(), tmp.Length()*2);
	}
	else if (op_strcmp(mime_type, "text/unicode") == 0)
	{
		if (DragDrop_Data_Utils::HasText(m_drag_object.get()))
		{
			OpStringC text = DragDrop_Data_Utils::GetText(m_drag_object.get());
			if (text.HasContent())
				RETURN_IF_ERROR(buf.Set((unsigned char*)text.CStr(), text.Length()*2));
			return OpStatus::OK;
		}
	}
	else if (op_strcmp(mime_type, "UTF8_STRING") == 0 ||
			 op_strcmp(mime_type, "text/plain;charset=utf-8") == 0)
	{
		// In utf8
		OpString text;
		if (OpStatus::IsSuccess(GetTextFromDragObject(text, true, m_drag_object.get())))
		{
			int size = text.UTF8(0)-1;
			if (size > 0)
			{
				RETURN_IF_ERROR(buf.Set(0,size));
				text.UTF8((char*)buf.GetData(),size);
			}
		}
	}
	else if (op_strcmp(mime_type, "STRING") == 0)
	{
		// In 8859-1
		OpString text;
		if (OpStatus::IsSuccess(GetTextFromDragObject(text, true, m_drag_object.get())))
		{
			// Convert to 8-bit
			OpString8 tmp;
			RETURN_IF_ERROR(tmp.Set(text));

			if (tmp.HasContent())
				RETURN_IF_ERROR(buf.Set((unsigned char*)tmp.CStr(), tmp.Length()));
		}
	}
	else if (op_strcmp(mime_type, "text/plain") == 0)
	{
		// In native encoding
		OpString text;
		if (OpStatus::IsSuccess(GetTextFromDragObject(text, true, m_drag_object.get())))
		{
			OpString8 tmp;
			RETURN_IF_ERROR(PosixNativeUtil::ToNative(text.CStr(), &tmp));
			if (tmp.HasContent())
				RETURN_IF_ERROR(buf.Set((unsigned char*)tmp.CStr(), tmp.Length()));
		}
	}

	return OpStatus::OK;
}

