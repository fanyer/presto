/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include <sys/types.h>
#include <sys/wait.h>

#include "platforms/unix/base/x11/x11_opmessageloop.h"

#include "platforms/unix/base/x11/x11_callback.h"
#include "platforms/unix/base/x11/x11_debug.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opthreadtools.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/unix/base/x11/inpututils.h"

#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/posix_ipc/posix_ipc_process_manager.h"
#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/common/unix_opsysteminfo.h"
#include "platforms/unix/product/x11quick/popupmenu.h"
#include "platforms/unix/product/x11quick/x11_global_desktop_application.h"
#include "platforms/x11api/plugins/plugin_window_tracker.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"

X11Types::Time X11OpMessageLoop::s_xtime;
unsigned long X11OpMessageLoop::s_xseqno = 0;
OpPoint X11OpMessageLoop::s_xroot;
X11OpMessageLoop *X11OpMessageLoop::s_self;
List<class X11LowLatencyCallbackElm> X11OpMessageLoop::s_pending_low_latency_callbacks;
volatile bool X11OpMessageLoop::s_sigchld_received = false;


class X11LowLatencyCallbackElm : public ListElement<X11LowLatencyCallbackElm>
{
public:
	X11LowLatencyCallbackElm(X11LowLatencyCallback * cbobj, UINTPTR data, double when)
		: obj(cbobj),
		  data(data),
		  when(when)
	{
	}

	X11LowLatencyCallback * obj;
	UINTPTR data;
	double when;

	static void ExecuteCallbacks(List<X11LowLatencyCallbackElm> * cblist)
	{
		X11LowLatencyCallbackElm * elm = cblist->First();
		if (!elm)
			return;
		double now = g_op_time_info->GetRuntimeMS();
		while (elm && elm->when <= now)
		{
			elm->Out();
			elm->obj->LowLatencyCallback(elm->data);
			OP_DELETE(elm);
			elm = cblist->First();
		}
	}
};


X11OpMessageLoop::X11OpMessageLoop(PosixIpcProcessManager& process_manager)
	: m_stop_backlog(0)
	, m_stop_current(false)
	, m_stop_all(false)
	, m_plugin_events(NULL)
	, m_process_manager(process_manager)
	, m_first_sigchld(0)
	, m_last_sigchld(0)
{
	OP_ASSERT(!s_self);
	s_self = this;
}

X11OpMessageLoop::~X11OpMessageLoop()
{
	OP_DELETEA(m_plugin_events);
	OP_ASSERT(s_self);
	s_self = 0;
}

void X11OpMessageLoop::RequestRunSlice(unsigned int limit)
{
	m_process_manager.RequestRunSlice(limit);
}

OP_STATUS X11OpMessageLoop::RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type)
{
	switch (type)
	{
	case COMPONENT_TEST:
	case COMPONENT_PLUGIN:
	case COMPONENT_PLUGIN_LINUX_IA32:
		return m_process_manager.CreateComponent(peer, requester, type);
	}

	return g_component_manager->HandlePeerRequest(requester, type);
}

OP_STATUS X11OpMessageLoop::SendMessage(OpTypedMessage* message)
{
	return m_process_manager.SendMessage(message);
}

OP_STATUS X11OpMessageLoop::ProcessEvents(unsigned timeout, EventFlags flags)
{
	if (flags == PROCESS_IPC_MESSAGES)
		return m_process_manager.ProcessEvents(timeout);

	OP_ASSERT(flags == PROCESS_ANY);
	PollPlatformEvents(timeout);
	return OpStatus::OK;
}

void X11OpMessageLoop::OnComponentCreated(OpMessageAddress address)
{
	m_process_manager.OnComponentCreated(address);
}

void X11OpMessageLoop::DispatchAllPostedMessagesNow()
{
	NestLoop(1000);
}


/*
 * Implementation specific methods
 */

OP_STATUS X11OpMessageLoop::Init()
{
	// Setup listener for X events, so that a poll() will return when X posts events to its socket
	RETURN_IF_ERROR(g_posix_selector->Watch(XConnectionNumber(g_x11->GetDisplay()), PosixSelector::READ, -1, this));

	// Setup other members
	m_plugin_events = OP_NEWA(XEvent, PLUGIN_EVENT_COUNT);
	RETURN_OOM_IF_NULL(m_plugin_events);
	op_memset(m_plugin_events, 0, sizeof(*m_plugin_events) * PLUGIN_EVENT_COUNT);

	return OpStatus::OK;
}

void X11OpMessageLoop::MainLoop()
{
#ifdef DEBUG_ENABLE_OPASSERT
	static bool running = false;
	OP_ASSERT(!running && "MainLoop should only be run once!");
	running = true;
#endif // DEBUG_ENABLE_OPASSERT

	// This is the main loop
	while (!m_stop_all)
	{
		TriggerLowLatencyCallbacks();
		const unsigned waitms = g_component_manager->RunSlice();
		PollPlatformEvents(waitms);
	}
}

void X11OpMessageLoop::NestLoop(unsigned timeout)
{
	double starttime = 0;
	double endtime = 0;
	if (timeout != UINT_MAX)
	{
		starttime = g_op_time_info->GetRuntimeMS();
		endtime = starttime;
		endtime += timeout;
	}

	if (m_stop_current)
	{
		++m_stop_backlog;
		m_stop_current = false;
	}

	while (!m_stop_current && !m_stop_all)
	{
		// RunSlice() and PollPlatformEvents() can both set m_stop_current
		TriggerLowLatencyCallbacks();
		unsigned waitms = min(g_component_manager->RunSlice(), timeout);
		PollPlatformEvents(waitms);

		if (timeout != UINT_MAX)
		{
			// Check if we're past the timeout
			double current = g_op_time_info->GetRuntimeMS();
			if (current >= endtime)
				break;

			timeout = endtime - current;
		}
	}
	TriggerLowLatencyCallbacks();

	m_stop_current = m_stop_backlog > 0;
	if (m_stop_backlog > 0)
		--m_stop_backlog;
}

void X11OpMessageLoop::PollPlatformEvents(unsigned waitms)
{
	if (s_sigchld_received)
	{
		s_sigchld_received = false;
		m_last_sigchld = g_op_time_info->GetRuntimeMS();
	}
	// Check for messages posted to main thread, should be reposted to Opera
	// return if any are found because timeout might change
	if (X11OpThreadTools::GetInstance()->DeliverMessages())
		return;

	// Check platform messages, return if any are found because timeout might change
	if (ProcessXEvents())
		return;

	// Main platform event waiting
	TriggerLowLatencyCallbacks();
	if (waitms != 0)
	{
		double now = g_op_time_info->GetRuntimeMS();
		X11LowLatencyCallbackElm * elm = s_pending_low_latency_callbacks.First();
		if (elm)
		{
			double nextcb = elm->when - now;
			if (nextcb < 0)
				nextcb = 0;
			if (waitms == UINT_MAX || nextcb < waitms)
				waitms = nextcb;
		}
	}
	if (m_last_sigchld != 0)
		ReapZombies();
	const double timeout = waitms != UINT_MAX ? waitms : -1;
	g_posix_selector->Poll(timeout);
}

bool X11OpMessageLoop::ProcessXEvents()
{
	X11Types::Display *display = g_x11->GetDisplay();
	XEvent event;
	bool processed_event = false;

	// Check for X events and handle them, if any
	while (XPending(display))
	{
		processed_event = true;

		TriggerLowLatencyCallbacks();
		XNextEvent(display, &event);

		// Filter events in case the IME wants them
		if (XFilterEvent(&event, X11Constants::XNone))
			continue;

		SavePluginEvent(event);
		HandleXEvent(&event, 0);
	}

	return processed_event;
}

XEvent X11OpMessageLoop::GetPluginEvent(PluginEvent event)
{
	return m_plugin_events[event];
}

void X11OpMessageLoop::SavePluginEvent(const XEvent& xevent)
{
	switch (xevent.type)
	{
	case KeyPress:
	case KeyRelease:
		m_plugin_events[KEY_EVENT] = xevent;
		break;
	case ButtonPress:
	case ButtonRelease:
		m_plugin_events[BUTTON_EVENT] = xevent;
		break;
	case MotionNotify:
		m_plugin_events[MOTION_EVENT] = xevent;
		break;
	case FocusIn:
	case FocusOut:
		m_plugin_events[FOCUS_EVENT] = xevent;
		break;
	case EnterNotify:
	case LeaveNotify:
		m_plugin_events[CROSSING_EVENT] = xevent;
		break;
	}
}

void X11OpMessageLoop::ReapZombies()
{
	double now = g_op_time_info->GetRuntimeMS();
	if (m_first_sigchld == 0)
		m_first_sigchld = now;
	bool constant_sigchld = now - m_first_sigchld > 6000;
	if (!constant_sigchld &&
		now - m_last_sigchld < 1000)
		/* Give other components (e.g. Qt) a chance to pick up their
		 * own zombies.  Yes, there are still race conditions here.
		 * And these timeouts are pulled out of thin air.
		 */
		return;
	m_first_sigchld = 0;
	m_last_sigchld = 0;
	while (true)
	{
		if (!constant_sigchld && s_sigchld_received)
			return;
		int status;
		pid_t pid = waitpid(-1, &status, WNOHANG);
		if (pid <= 0)
			return;
	}
}

void X11OpMessageLoop::HandleXEvent(XEvent* e, X11Widget* widget)
{
	int type = e->type;
	BOOL is_input_event = FALSE;

	if (e->type != KeymapNotify &&
		e->type < 64)
	{
		/* From the X protocol specification (X11R6.9/7.0):
		 *
		 *    Every event contains an 8-bit type code.  The most
		 *    significant bit in this code is set if the event was
		 *    generated from a SendEvent request.  Event codes 64
		 *    through 127 are reserved for extensions[...].  Every
		 *    core event (with the exception of KeymapNotify) also
		 *    contains the least significant 16 bits of the sequence
		 *    number of the last request issued by the client that was
		 *    (or is currently being) processed by the server.
		 *
		 * Xlib strips the SendEvent bit from the type value and
		 * "[expands] the 16-bit least-significant bits [of the
		 * sequence number] to a full 32-bit value."
		 */
		s_xseqno = e->xany.serial;
	};

	UINT32 data = 0;
	switch (type)
	{
	case SelectionClear:
		s_xtime = e->xselectionclear.time; break;
	case SelectionRequest:
		s_xtime = e->xselectionrequest.time; break;
	case SelectionNotify:
		s_xtime = e->xselection.time; break;
	case MotionNotify:
		s_xtime = e->xmotion.time;
		s_xroot.x = e->xmotion.x_root;
		s_xroot.y = e->xmotion.y_root;
		data = e->xmotion.x_root | (e->xmotion.y_root << 16);
		InputUtils::SetX11ModifierMask(e);
		InputUtils::SetOpButtonFlags(e);
		// Release dragged widget if it exists. Must happen before any event processing
		if (g_x11->GetWidgetList()->GetDraggedWidget())
			g_x11->GetWidgetList()->GetDraggedWidget()->StopMoveResizeWindow(FALSE);
		is_input_event = TRUE;
		break;
	case ButtonPress:
	case ButtonRelease:
		// Release dragged widget if it exists. Must happen before any event processing
		if (g_x11->GetWidgetList()->GetDraggedWidget())
			g_x11->GetWidgetList()->GetDraggedWidget()->StopMoveResizeWindow(type == ButtonRelease);
		s_xtime = e->xbutton.time;
		s_xroot.x = e->xbutton.x_root;
		s_xroot.y = e->xbutton.y_root;
		InputUtils::SetX11ModifierMask(e);
		InputUtils::SetOpButtonFlags(e);
		if (g_startup_settings.debug_mouse)
			InputUtils::DumpButtonEvent(e);
		is_input_event = TRUE;
		break;
	case KeyPress:
	case KeyRelease:
	{
		s_xtime = e->xkey.time;
		data = e->xkey.keycode;
		InputUtils::SetX11ModifierMask(e);
		if (g_startup_settings.debug_keyboard)
			InputUtils::DumpKeyEvent(e);
		is_input_event = TRUE;
		break;
	}
	case EnterNotify:
	case LeaveNotify:
		s_xtime = e->xcrossing.time;
		InputUtils::HandleFocusChange(e);
		break;

	case MappingNotify:
		if (e->xmapping.request==MappingKeyboard || e->xmapping.request==MappingModifier)
			XRefreshKeyboardMapping(&e->xmapping);
		break;

	case FocusOut:
		InputUtils::HandleFocusChange(e);
		break;	
	};

	if (data)
	{
		UnixUtils::FeedSSLRandomEntropy(data);
	}


	// Highest priority. Test for modal dialogs in toolkit layer. Block input
	// events that such a dialog is modal for
	if (is_input_event)
	{
		X11Widget* w1 = g_x11->GetWidgetList()->GetModalToolkitParent();
		if (w1 && w1->Contains(g_x11->GetWidgetList()->FindWidget(e->xany.window)))
			return;
	}
	

	if( ((X11DesktopGlobalApplication*)g_desktop_global_application)->HandleEvent(e) )
	{
		if (type == ButtonRelease)
			if (g_x11->GetWidgetList()->GetCapturedWidget())
				g_x11->GetWidgetList()->GetCapturedWidget()->ReleaseCapture();
		return;
	}


	switch (type)
	{
	case MotionNotify:
	case ButtonPress:
	case ButtonRelease:
	case KeyPress:
	case KeyRelease:
	{
		if (((X11DesktopGlobalApplication*)g_desktop_global_application)->BlockInputEvent(e, widget))
			return;

		if (PluginWindowTracker::GetInstance().HandleEvent(e))
			return;

		X11Widget* w = widget;
		if (!w)
			w = g_x11->GetWidgetList()->GetGrabbedWidget();
		if (!w)
			w = g_x11->GetWidgetList()->FindWidget(e->xany.window);
		if (w && w->GetInputEventListener() && w->GetInputEventListener()->OnInputEvent(e))
			return;

		static BOOL block_mouse_release = FALSE;
		if (block_mouse_release && type == ButtonRelease)
		{
			// This is the first release after closing the hotclick menu
			block_mouse_release = FALSE;
			return;
		}

		if (PopupMenu::GetHotClickMenu() && (type == ButtonPress || type == ButtonRelease))
		{
			if (e->xbutton.button != 1)
			{
				if (type == ButtonPress)
				{
					X11Widget* popup = X11Widget::GetPopupWidget();
					if (popup)
						popup->Close();
					block_mouse_release = TRUE; // prevent mouse release to re-open popup menu
				}
				return;
			}

			if (PopupMenu::GetHotClickMenu()->Contains(e->xbutton.x_root, e->xbutton.y_root, TRUE) == 0)
			{
				w = g_x11->GetWidgetList()->GetPopupParentWidget();
				if (w)
				{
					// Send event to document instead of popup. That enables multi click selection

					X11Types::Window child;
					int x, y;
					XTranslateCoordinates(e->xbutton.display, e->xbutton.window, w->GetWindowHandle(), e->xbutton.x, e->xbutton.y, &x, &y, &child);
					e->xbutton.x = x;
					e->xbutton.y = y;
					e->xbutton.window = w->GetWindowHandle();

					InputUtils::UpdateClickCount(e);

					w->DispatchEvent(e);

					// The event may have cleared the selection or we have moved the mouse.
					// Remove popup in that case
					X11Widget* popup = X11Widget::GetPopupWidget();
					DocumentDesktopWindow* ddw = g_application->GetActiveDocumentDesktopWindow();
					if (popup && ((ddw && !ddw->GetWindowCommander()->HasSelectedText()) || InputUtils::GetClickCount() <= 1))
						popup->Close();
					return;
				}
			}
		}

		InputUtils::UpdateClickCount(e);

		if (InputUtils::HandleEvent(e))
		{
			// A mouse gesture will be dealt with in InputUtils::HandleEvent() but we
			// still have a grabbed widget. Release it (espen, #DSK-273090)
			if (type == ButtonRelease)
				if (g_x11->GetWidgetList()->GetCapturedWidget())
					g_x11->GetWidgetList()->GetCapturedWidget()->ReleaseCapture();
			return;
		}

		break;
	}
	case ClientMessage:
	{
		if (PluginWindowTracker::GetInstance().HandleEvent(e))
			return;

		break;
	}
	}

	if (!widget)
		widget = g_x11->GetWidgetList()->FindWidget(e->xany.window);
	if (widget)
		widget->DispatchEvent(e);
}

OP_STATUS X11OpMessageLoop::PostLowLatencyCallbackAbsoluteTime(X11LowLatencyCallback * cbobj, UINTPTR data, double when)
{
	X11LowLatencyCallbackElm * elm = OP_NEW(X11LowLatencyCallbackElm, (cbobj, data, when));
	if (!elm)
		return OpStatus::ERR_NO_MEMORY;

	X11LowLatencyCallbackElm * before = s_pending_low_latency_callbacks.First();
	while (before && before->when <= elm->when)
		before = before->Suc();
	if (before)
		elm->Precede(before);
	else
		elm->Into(&s_pending_low_latency_callbacks);
	return OpStatus::OK;
}

OP_STATUS X11OpMessageLoop::PostLowLatencyCallbackDelayed(X11LowLatencyCallback * cbobj, UINTPTR data, double delay)
{
	return PostLowLatencyCallbackAbsoluteTime(cbobj, data, g_op_time_info->GetRuntimeMS() + delay);
}

OP_STATUS X11OpMessageLoop::PostLowLatencyCallbackNoDelay(X11LowLatencyCallback * cbobj, UINTPTR data)
{
	/* Simple implementation: expire 1 second in the past to make sure
	 * any reasonable test will find it ready to execute.
	 */
	return PostLowLatencyCallbackAbsoluteTime(cbobj, data, g_op_time_info->GetRuntimeMS() - 1000);
}

void X11OpMessageLoop::CancelLowLatencyCallbacks(X11LowLatencyCallback * cbobj)
{
	X11LowLatencyCallbackElm * elm = s_pending_low_latency_callbacks.First();
	while (elm)
	{
		X11LowLatencyCallbackElm * next = elm->Suc();
		if (elm->obj == cbobj)
		{
			elm->Out();
			OP_DELETE(elm);
		}
		elm = next;
	}
}

void X11OpMessageLoop::TriggerLowLatencyCallbacks()
{
	X11LowLatencyCallbackElm::ExecuteCallbacks(&s_pending_low_latency_callbacks);
}
