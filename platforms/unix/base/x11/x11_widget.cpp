/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "platforms/unix/base/x11/x11_widget.h"

#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_cursor.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_icon.h"
#include "platforms/unix/base/x11/x11_inputcontext.h"
#include "platforms/unix/base/x11/x11_mdebuffer.h"
#include "platforms/unix/base/x11/x11_mdescreen.h"
#include "platforms/unix/base/x11/x11_opdragmanager.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_opview.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_vegawindow.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/unix/base/common/unix_opsysteminfo.h"

#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/widgets/WidgetContainer.h"

#include <stdlib.h>
#include <ctype.h>

using X11Types::Atom;

// #define DEBUG_X11WIDGET
#ifdef DEBUG_X11WIDGET
int count = 0;
#endif

#ifdef _DEBUG
//# define DEBUG_WM_STATE
#endif

void MotifWindowManagerHints::Apply( X11Types::Display* display, X11Types::Window window)
{
	if (m_data.flags)
	{
		X11Types::Atom atom = ATOMIZE(_MOTIF_WM_HINTS);
		if (m_data.flags)
			XChangeProperty(display, window, atom, atom, 32, PropModeReplace, (unsigned char*)buf, 5);
		else
			XDeleteProperty(display, window, atom);
	}
}


void MotifWindowManagerHints::SetDecoration(bool on)
{
	m_data.flags |= HINTS_DECORATIONS;
	m_data.functions = on ? FUNCTION_ALL : 0;
	m_data.decorations = on ? DECORATION_ALL : 0;
}


void MotifWindowManagerHints::SetModal()
{	
	m_data.flags |= HINTS_INPUT_MODE; 
	m_data.input_mode = INPUT_FULL_APPLICATION_MODAL;
}


bool X11Widget::m_disable_grab = false;
X11Widget::WindowDrag X11Widget::m_window_drag;
X11Widget::RenderingAPI X11Widget::m_core_rendering_api = X11Widget::API_UNDECIDED;


X11Widget::X11Widget()
	:m_display(0)
	,m_visual(0)
	,m_colormap(None)
	,m_handle(None)
	,m_screen(0)
	,m_input_context(0)
	,m_mde_screen(NULL)
	,m_mde_buffer(NULL)
	,m_vega_window(NULL)
	,m_active_popup(NULL)
	,m_list_element(*this)
	,m_sizehints(0)
	,m_toplevel(0)
	,m_input_event_listener(0)
	,m_icon(0)
	,m_window_state(NORMAL)
	,m_active_cursor(X11Cursor::X11_CURSOR_MAX)
	,m_saved_cursor(X11Cursor::X11_CURSOR_MAX)
	,m_background_color(0)
	,m_window_flags(0)
	,m_depth(0)
	,m_expecting_buttonrelease(0)
	,m_rendering_api(API_UNDECIDED)
	,m_no_update(false)
	,m_visible(false)
	,m_grabbed(false)
	,m_destroyed(false)
	,m_modal_executing(false)
	,m_pending_resize(false)
	,m_has_background_color(false)
	,m_owns_colormap(false)
	,m_owns_mde(false)
	,m_accept_drop(false)
	,m_dragging_enabled(false)
	,m_propagate_leave_event(false)
	,m_shaded(false)
	,m_shaded_in_hide(false)
	,m_skip_taskbar(false)
	,m_exit_started(false)
	,m_closed(false)
	,m_show_on_application_show(false)
	,m_has_set_decoration(false)
	,m_recent_scrolls(0)
	,m_recent_scrolls_first_unused(0)
{
#ifdef DEBUG_X11WIDGET
	count ++;
	printf("+ %p X11Widget: %d\n", this, count);
#endif
}

X11Widget::~X11Widget()
{
	if (m_owns_mde && m_vega_window)
		m_vega_window->OnXWindowDead(this);
	m_recent_scrolls_first_unused = 0;
	while (m_recent_scrolls)
	{
		ScrollData * p = m_recent_scrolls;
		m_recent_scrolls = p->m_next;
		OP_DELETE(p);
	};
#ifdef DEBUG_X11WIDGET
	count --;
	printf("- %p X11Widget: %d\n", this, count);
#endif
	if (m_toplevel && m_toplevel->focus_proxy)
	{
		OP_DELETE(m_toplevel->focus_proxy);
		m_toplevel->focus_proxy = 0;
	};

	OP_ASSERT(!FirstChild());

	Hide();

	if (Parent())
		Parent()->DescendantRemoved(this);
	Out();

	OP_DELETE(m_input_context);
	OP_DELETE(m_icon);

	if (m_handle != None && !m_destroyed)
	{
		if (m_owns_mde && m_vega_window)
			m_vega_window->OnBeforeNativeHandleDestroyed(GetWindowHandle());
		XDestroyWindow(m_display, m_handle);
	}

	g_x11->GetWidgetList()->RemoveWidget(m_list_element);

	if (m_sizehints)
		XFree(m_sizehints);

	if (m_toplevel && m_toplevel->sync_counter_id)
		XSyncDestroyCounter(m_display, m_toplevel->sync_counter_id);

	OP_DELETE(m_toplevel);

	if (m_owns_colormap)
		XFreeColormap(m_display, m_colormap);

	if (m_owns_mde)
	{
		OP_DELETE(m_mde_screen);
		OP_DELETE(m_vega_window);
		OP_DELETE(m_mde_buffer);
	}

	for (OpListenersIterator iterator(m_x11widget_listeners); m_x11widget_listeners.HasNext(iterator);)
		m_x11widget_listeners.GetNext(iterator)->OnDeleted(this);

}

OP_STATUS X11Widget::CreateSizeHintsStruct()
{
	if (m_sizehints)
		return OpStatus::OK;

	m_sizehints = XAllocSizeHints();
	if (!m_sizehints)
		return OpStatus::ERR_NO_MEMORY;

	m_sizehints->flags = 0;
	return OpStatus::OK;
}

void X11Widget::UpdateWmData()
{
	if (!m_toplevel)
		return;

	if (!GetFrameExtents(m_toplevel->decoration))
	{
		X11Types::Window candidate = m_handle;
		X11Types::Window root;
		X11Types::Window parent;
		X11Types::Window *children;
		X11Types::Window decowin = None;
		unsigned int child_count;

		/* Search upwards in the window hierarchy and attempt to find the child
		   window of the desktop window (root / virtual root). This will
		   hopefully be the decoration window that our "top-level" window was
		   reparented into by the window manager. The horizontal and vertical
		   offset from the desktop window to the decoration window should give
		   us the correct position of the window. */
		while (XQueryTree(m_display, candidate, &root, &parent, &children,&child_count))
		{
			if (children)
				XFree(children);
			
			if (parent == None)
			{
				/* FIXME: We have now found the root window. This may not
				   necessarily be the right window to compare against.
				   There is something called virtual root windows... */
				break;
			}

			decowin = candidate;
			candidate = parent;
		}
		
		if (decowin && decowin != m_handle)
		{
			int left, top;
			X11Types::Window child;
			XTranslateCoordinates(m_display, m_handle, decowin, 0, 0, &left, &top, &child);
			m_toplevel->decoration = OpRect(left,top,left,left); // Assuming same borders at left, right, bottom
		}
		else
			m_toplevel->decoration = OpRect(0,0,0,0);
	}
}



OP_STATUS X11Widget::Init(X11Widget *parent, const char *name, int flags)
{
	return Init(parent, name, flags, -1);
}


OP_STATUS X11Widget::Init(X11Widget *parent, const char *name, int flags, int screen)
{
	if (!g_x11->GetWMClassName())
		return OpStatus::ERR;

	if (!parent || (flags & (TRANSIENT | BYPASS_WM)))
	{
		m_toplevel = OP_NEW(ToplevelData, ());
		if (!m_toplevel)
			return OpStatus::ERR_NO_MEMORY;
	}

	if ((flags&X11Widget::SYNC_REQUEST) && !g_x11->IsXSyncAvailable())
		flags &= ~X11Widget::SYNC_REQUEST;

	if (parent)
		Under(parent);

	m_window_flags = flags;

	if (parent && !m_toplevel)
	{
		m_display  = parent->GetDisplay();
		m_screen   = parent->GetScreen();
		m_visual   = parent->GetVisual();
		m_colormap = parent->GetColormap();
		m_depth    = parent->GetDepth();
	}
	else
	{
		if (parent)
		{
			m_display = parent->GetDisplay();
			m_screen  = parent->GetScreen();
		}
		else
		{
			m_display = g_x11->GetDisplay();
			m_screen  = screen < 0 || screen >= g_x11->GetScreenCount() ? g_x11->GetDefaultScreen() : screen;
		}

		X11Visual * visinfo = NULL;
		if (IsUsingOpenGL())
		{
			if ((m_window_flags & TRANSPARENT) && SupportsExternalCompositing())
			{
				visinfo = &g_x11->GetGLARGBVisual(m_screen);
				if (visinfo->visual == 0)
					visinfo = NULL;
			}
			if (!visinfo)
			{
				visinfo = &g_x11->GetGLVisual(m_screen);
				if (visinfo->visual == 0)
					visinfo = NULL;
				else
					m_window_flags &= ~TRANSPARENT;
			}
		}
		if (!visinfo && (m_window_flags & TRANSPARENT) && SupportsExternalCompositing())
		{
			visinfo = &g_x11->GetX11ARGBVisual(m_screen);
			if (visinfo->visual == 0)
				visinfo = NULL;
		}
		if (!visinfo)
		{
			visinfo = &g_x11->GetX11Visual(m_screen);
			m_window_flags &= ~TRANSPARENT;
		}

		m_visual   = visinfo->visual;
		m_depth    = visinfo->depth;
		m_colormap = visinfo->colormap;
	}


	XSetWindowAttributes attr;
	int attr_flags = 0;

	attr.bit_gravity = NorthWestGravity;
	attr.colormap = m_colormap;
	attr.border_pixel = BlackPixel(m_display, m_screen);
	attr.event_mask = ExposureMask |
		KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask |
		PointerMotionMask |
		FocusChangeMask |
		EnterWindowMask |
		LeaveWindowMask |
		StructureNotifyMask |
		PropertyChangeMask;
	attr_flags |= CWBitGravity | CWEventMask | CWColormap | CWBorderPixel;

	if (m_toplevel && (m_window_flags & BYPASS_WM))
	{
		attr.override_redirect = True;
		attr_flags |= CWOverrideRedirect;
	}

	X11Types::Window parent_win;
	if (!m_toplevel)
		parent_win = parent->GetWindowHandle();
	else
		parent_win = RootWindow(m_display, m_screen);

	int initial_width=100, initial_height=100;

	m_handle = XCreateWindow(m_display, parent_win,
						   0, 0, initial_width, initial_height, 0,
						   m_depth,
						   InputOutput,
						   m_visual,
						   attr_flags, &attr);
	if (m_handle == None)
		return OpStatus::ERR;

	RETURN_IF_ERROR(InitToplevel());

	g_x11->GetWidgetList()->AddWidget(m_list_element);

	if (parent)
		parent->DescendantAdded(this);

	SetAcceptDrop(true);

	return OpStatus::OK;
}

bool X11Widget::IsUsingOpenGL()
{
	if (m_rendering_api == API_OPENGL)
		return true;
	if (m_rendering_api != API_UNDECIDED)
		return false;
	if (m_core_rendering_api == API_UNDECIDED)
	{
		if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_NONE)
		{
			VEGARendererBackend * backend = g_vegaGlobals.CreateRasterBackend(100, 100, VEGA_DEFAULT_QUALITY, LibvegaModule::BACKEND_NONE);
			OP_DELETE(backend);
		}
		if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D ||
			g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW2D)
			m_core_rendering_api = API_OPENGL;
		else
			m_core_rendering_api = API_X11;
	}
	m_rendering_api = m_core_rendering_api;
	OP_ASSERT(m_rendering_api != API_UNDECIDED);
	return m_rendering_api == API_OPENGL;
}

OP_STATUS X11Widget::RecreateWindow()
{
	if (!m_destroyed)
		return OpStatus::ERR;

	if (m_toplevel)
	{
		OP_DELETE(m_toplevel->focus_proxy);
		m_toplevel->focus_proxy = 0;
	}

	XSetWindowAttributes attr;
	attr.background_pixmap = 0;
	attr.colormap          = m_colormap;
	attr.background_pixel  = 0;
	attr.border_pixel      = 0;

	X11Types::Window parent = RootWindow(m_display, m_screen);
	if (Parent())
		parent = Parent()->GetWindowHandle();

	X11Types::Window window = XCreateWindow(
		m_display, parent, -1, -1, 1, 1, 0, m_depth, InputOutput, m_visual,
		CWBackPixmap|CWBackPixel|CWBorderPixel|CWColormap, &attr);

	return ReplaceWindow(window);
}




OP_STATUS X11Widget::ReplaceWindow( X11Types::Window window )
{
	if (m_handle == None)
	{
		return OpStatus::ERR;
	}
	else if (!m_destroyed)
	{
		if (m_owns_mde && m_vega_window)
			m_vega_window->OnBeforeNativeHandleDestroyed(GetWindowHandle());
		XDestroyWindow(m_display, m_handle);
		m_handle = None;
	}

	OP_DELETE(m_input_context);
	m_input_context = 0;

	m_handle = window;
	m_destroyed = false;

	XWindowAttributes win_attr;
	XGetWindowAttributes( m_display, m_handle, &win_attr );

	// display and screen must remain the same
	m_visual   = win_attr.visual;
	m_colormap = win_attr.colormap;

	XSetWindowAttributes attr;
	unsigned long attr_flags = 0;

	attr.bit_gravity  = NorthWestGravity;
	attr.colormap     = m_colormap;
	attr.border_pixel = BlackPixel(m_display, m_screen);
	attr.event_mask   = ExposureMask | KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
		FocusChangeMask | LeaveWindowMask | EnterWindowMask | StructureNotifyMask | PropertyChangeMask;
	attr_flags |= CWBitGravity | CWEventMask | CWColormap | CWBorderPixel;

	if (m_toplevel && (m_window_flags & BYPASS_WM))
	{
		attr.override_redirect = True;
		attr_flags |= CWOverrideRedirect;
	}

	XChangeWindowAttributes( m_display, m_handle, attr_flags, &attr);
	XSetWindowBackground(m_display, m_handle, m_background_color);

	RETURN_IF_ERROR(InitToplevel());

	if (m_accept_drop)
	{
		m_accept_drop = false;
		SetAcceptDrop(true);
	}

	return OpStatus::OK;
}


OP_STATUS X11Widget::InitToplevel()
{
	if (m_toplevel)
	{
		X11Types::Window client_leader = g_x11->GetAppWindow();

		if (m_toplevel->focus_proxy == 0)
		{
			m_toplevel->focus_proxy = OP_NEW(X11Widget, ());
			if (m_toplevel->focus_proxy == 0)
				return OpStatus::ERR_NO_MEMORY;
			RETURN_IF_ERROR(m_toplevel->focus_proxy->Init(this, 0, 0));
			m_toplevel->focus_proxy->SetGeometry(-1, -1, 1, 1);
			m_toplevel->focus_proxy->Show();
		};
		if (m_toplevel && m_toplevel->focus_proxy)
			m_input_context = g_x11->CreateIC(m_toplevel->focus_proxy);

		XWMHints wm_hints;
		wm_hints.flags = InputHint | StateHint | WindowGroupHint;
		wm_hints.input = True;
		wm_hints.initial_state = NormalState;
		wm_hints.window_group = client_leader;

		XClassHint class_hint;
		char res_name[] = "opera";
		class_hint.res_class = g_x11->GetWMClassName();
		class_hint.res_name = res_name;
		XSetWMProperties(m_display, m_handle, 0, 0, 0, 0, 0, &wm_hints, &class_hint);

		SetTitle(UNI_L("Opera"));

		if (m_window_flags & TRANSIENT)
		{
			X11Widget* p = Parent();
			if( p )
				p = p->GetNearestTopLevel();
			if( p )
				XSetTransientForHint(m_display, m_handle, p->m_handle);
			else // Transient for the client leader
				XSetTransientForHint(m_display, m_handle, client_leader);
		}

		MotifWindowManagerHints mwm_hints;

		if (m_window_flags & MODAL)
		{
			mwm_hints.SetModal();
			Atom wm_modal_atom = ATOMIZE(_NET_WM_STATE_MODAL);
			XChangeProperty(m_display, m_handle, ATOMIZE(_NET_WM_STATE), XA_ATOM, 32,
							PropModeReplace, (const unsigned char *)&wm_modal_atom, 1);
		}

		if (m_window_flags & BYPASS_WM)
		{
			XSetWindowAttributes wsa;
			wsa.override_redirect = True;
			wsa.save_under = True;
			XChangeWindowAttributes(m_display, m_handle, CWOverrideRedirect | CWSaveUnder, &wsa);
		}

		long net_wm_window_types = None;
		int  num_window_types = 1; // For now
		if( m_window_flags & POPUP )
			net_wm_window_types = ATOMIZE(_NET_WM_WINDOW_TYPE_POPUP_MENU);
		else if( m_window_flags & UTILITY)
			net_wm_window_types = ATOMIZE(_NET_WM_WINDOW_TYPE_UTILITY);
		else if( m_window_flags & TOOLTIP)
			net_wm_window_types = ATOMIZE(_NET_WM_WINDOW_TYPE_TOOLTIP);
		else
		{
			if (IsDialog())
				net_wm_window_types = ATOMIZE(_NET_WM_WINDOW_TYPE_DIALOG);
			else
				net_wm_window_types = ATOMIZE(_NET_WM_WINDOW_TYPE_NORMAL);
		}

		XChangeProperty(m_display, m_handle, ATOMIZE(_NET_WM_WINDOW_TYPE), XA_ATOM, 32,
						PropModeReplace, (unsigned char *) &net_wm_window_types, num_window_types );


		if ((m_window_flags & PERSONA) && g_x11->HasPersonaSkin())
			mwm_hints.SetDecoration(false);
		else if (m_window_flags & (UTILITY|TRANSPARENT|NODECORATION))
			mwm_hints.SetDecoration(false);
		else
			mwm_hints.SetDecoration(true);


		mwm_hints.Apply(m_display, m_handle);

		// Support for _NET_WM_PID and WM_CLIENT_MACHINE
		long pid = getpid();
		XChangeProperty(m_display, m_handle, ATOMIZE(_NET_WM_PID), XA_CARDINAL, 32, 
						PropModeReplace, (unsigned char *)&pid, 1);

		// set client leader property
        XChangeProperty(m_display, m_handle, ATOMIZE(WM_CLIENT_LEADER),
                        XA_WINDOW, 32, PropModeReplace,
                        (unsigned char *)&client_leader, 1);

		Atom protocols[4];
		int num_protocols = 0;
		protocols[num_protocols++] = ATOMIZE(WM_DELETE_WINDOW);
		protocols[num_protocols++] = ATOMIZE(WM_TAKE_FOCUS);
		protocols[num_protocols++] = ATOMIZE(_NET_WM_PING);
		if (m_window_flags&SYNC_REQUEST)
			protocols[num_protocols++] = ATOMIZE(_NET_WM_SYNC_REQUEST);
		XSetWMProtocols(m_display, m_handle, protocols, num_protocols);

	}

	return OpStatus::OK;
}


OP_STATUS X11Widget::SetResourceName(const char* name)
{
	if (!name)
		return OpStatus::ERR_NULL_POINTER;

	int length = op_strlen(name);

	OpString8 resource_name;
	if (!resource_name.Reserve(length+1))
		return OpStatus::ERR_NO_MEMORY;

	// Remove whitespaces and lower any capital letters
	char* p = resource_name.CStr();
	for (int i=0; i<length; i++)
	{
		int c = name[i];
		if (!isspace(c))
		{
			if (c < 32 || c > 127)
				*p++ = '?';
			else
				*p++ = tolower(c);
		}
	}
	*p = 0;

	XClassHint class_hint;
	class_hint.res_class = g_x11->GetWMClassName();
	class_hint.res_name  = *resource_name.CStr() ? resource_name.CStr() : class_hint.res_class;
	XSetClassHint(m_display, m_handle, &class_hint);

	return OpStatus::OK;
}

OP_STATUS X11Widget::ChangeVisual(XVisualInfo* vi)
{
	if (m_visual == vi->visual)
		return OpStatus::OK;

	// Remove the proxy. It is a child of the widget and we only handle
	// widgets with no children (see test below). The proxy gets re-created
	// in InitToplevel() called from ReplaceWindow()
	if (m_toplevel)
	{
		OP_DELETE(m_toplevel->focus_proxy);
		m_toplevel->focus_proxy = 0;
	}

	if (FirstChild())
		return OpStatus::ERR; // Not supported unless we need it
	
	if (m_owns_colormap)
		XFreeColormap(m_display, m_colormap);

	m_owns_colormap = true;

	m_visual   = vi->visual;
	m_colormap = XCreateColormap(m_display, RootWindow(m_display, vi->screen), vi->visual, AllocNone);
	m_depth    = vi->depth;

	XSetWindowAttributes attr;
	attr.background_pixmap = 0;
	attr.colormap          = m_colormap;
	attr.background_pixel  = 0;
	attr.border_pixel      = 0;

	X11Types::Window parent = RootWindow(m_display, vi->screen);
	if (Parent())
		parent = Parent()->GetWindowHandle();

	X11Types::Window window = XCreateWindow(
		m_display, parent, -1, -1, 1, 1, 0, m_depth, InputOutput, m_visual,
		CWBackPixmap|CWBackPixel|CWBorderPixel|CWColormap, &attr);

	return ReplaceWindow(window);
}

OP_STATUS X11Widget::CreateMdeBuffer()
{
	if (m_mde_buffer && m_owns_mde)
	{
		OP_DELETE(m_mde_buffer);
		m_mde_buffer = 0;
		m_owns_mde = false;
	}

	m_owns_mde = true;
	m_mde_buffer  = OP_NEW(X11MdeBuffer, (GetDisplay(), m_screen, GetWindowHandle(), GetVisual(), 0, 0, m_depth, GetImageBPP()));
	return m_mde_buffer ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


OP_STATUS X11Widget::CreateMdeScreen(int width, int height)
{
	if (width <= 0)
		width = GetWidth();
	if (height <= 0)
		height = GetHeight();

	m_owns_mde = true;

	m_vega_window = OP_NEW(X11VegaWindow, (width, height, this));
	m_mde_screen  = OP_NEW(X11MdeScreen, (0, 0, 0, MDE_FORMAT_BGRA32, m_vega_window));
	m_mde_buffer  = OP_NEW(X11MdeBuffer, (GetDisplay(), m_screen, GetWindowHandle(), GetVisual(), width, height, m_depth, GetImageBPP()));

	if (!m_vega_window || !m_mde_screen || !m_mde_buffer)
		return OpStatus::ERR_NO_MEMORY;

	m_vega_window->setMdeBuffer(m_mde_buffer);
	m_mde_screen->setVegaWindow(m_vega_window);
	m_mde_screen->setMdeBuffer(m_mde_buffer);

	RETURN_IF_ERROR(m_mde_screen->Init());
	RETURN_IF_ERROR(m_mde_screen->PostValidateMDEScreen());

	return OpStatus::OK;
}


OP_STATUS X11Widget::ResizeMdeScreen(int width, int height)
{
	if (width <= 0 )
		width = GetWidth();
	if (height <= 0 )
		height = GetHeight();

	if (m_mde_buffer && m_mde_screen && m_vega_window)
	{
		if (m_mde_buffer->GetWidth() != (UINT32)width || m_mde_buffer->GetHeight() != (UINT32)height )
		{
			RETURN_IF_ERROR(m_vega_window->resize(width, height));
			m_mde_screen->Resize(width, height, width*4, m_vega_window);
			m_mde_screen->Invalidate(MDE_MakeRect(0,0,width,height), true);
		}
	}
	return OpStatus::OK;
}


X11Widget* X11Widget::GetNearestTopLevel()
{
	X11Widget* widget = this;
	while (widget && !widget->IsToplevel())
		widget = widget->Parent();

	return widget;
}


X11Widget* X11Widget::GetTopLevel()
{
	X11Widget* widget = this;
	while (widget->Parent())
		widget = widget->Parent();

	return widget;
}


bool X11Widget::Contains(const X11Widget* candidate)
{
	while (candidate)
	{
		if (candidate == this)
			return TRUE;
		candidate = candidate->Parent();
	}
	return FALSE;
}


bool X11Widget::GetKeyPress(XEvent* event, KeySym& keysym, OpString& text)
{
	g_x11->GetWidgetList()->SetInputWidget(this);

	if (!m_input_context)
		return false;

	return m_input_context->GetKeyPress(event, keysym, text);
}


void X11Widget::ActivateIM(OpInputMethodListener* listener)
{
	if (!m_input_context)
		return;

	m_input_context->Activate(listener);
}


void X11Widget::DeactivateIM(OpInputMethodListener* listener)
{
	if (!m_input_context)
		return;

	m_input_context->Deactivate(listener);
}


void X11Widget::AbortIM()
{
	if (!m_input_context)
		return;

	m_input_context->Abort();
}


void X11Widget::SetIMFocusWindow(X11Widget * window)
{
	if (!m_input_context)
		return;

	m_input_context->SetFocusWindow(window);
}

void X11Widget::SetIMFocusIfActive()
{
	if (!m_input_context)
		return;

	m_input_context->SetFocusIfActive();
};

void X11Widget::UnsetIMFocus()
{
	if (!m_input_context)
		return;

	m_input_context->UnsetFocus();
};


bool X11Widget::IsVisible()
{
	if (!m_visible)
		return false;
	if (Parent())
		return Parent()->IsVisible();
	return true;
}

bool X11Widget::DispatchEvent(XEvent *event)
{
	if (event->type == ButtonPress)
	{
		m_expecting_buttonrelease |= Button1Mask << (event->xbutton.button - 1);
	}
	else if (event->type == ButtonRelease)
	{
		m_expecting_buttonrelease &= ~(Button1Mask << (event->xbutton.button - 1));
	}

	// This function might trigger a 'delete this' if true is returned
	if (HandleEvent(event))
		return true;

	for (X11Widget *child = FirstChild(); child; child = child->Suc())
	{
		if (child->DispatchEvent(event))
			return true;
	}

	return false;
}

bool X11Widget::IgnoreModality(XEvent *event)
{
	if (event->type != ButtonRelease)
		return false;

	return (Button1Mask << (event->xbutton.button - 1)) & m_expecting_buttonrelease;
}

void X11Widget::Show()
{
	if (m_destroyed)
		return;

	if (m_toplevel && m_toplevel->sync_counter_id == 0 && (m_window_flags&SYNC_REQUEST) )
	{
		XSyncIntToValue (&m_toplevel->sync_counter_value, 0);

		X11Types::XSyncValue value;
		XSyncIntToValue(&value, 0);
		m_toplevel->sync_counter_id = XSyncCreateCounter(m_display, value);

		XChangeProperty(m_display, m_handle, ATOMIZE(_NET_WM_SYNC_REQUEST_COUNTER),XA_CARDINAL, 32, 
						PropModeReplace, (unsigned char *) &m_toplevel->sync_counter_id, 1);
	}

	XMapWindow(m_display, m_handle);

	if (m_toplevel)
		g_x11->NotifyStartupFinish();
	if (m_window_flags & TRANSIENT)
		Raise();
	if (m_window_flags & AUTOGRAB)
		Grab();
	if (!m_toplevel)
		m_visible = true;

	// Reset drag hotspot (pending drag) in current active widget before opening a window (menu, dropdown etc) [DSK-365066]
	if (g_x11->GetWidgetList()->GetCapturedWidget())
		g_x11->GetWidgetList()->GetCapturedWidget()->m_drag_hotspot.Reset();

	if (m_window_flags & MODAL)
	{
		Raise();
		GiveFocus();
		// We may show a hidden dialog that is already in the loop. This can happen
		// when showing all opera window by clicking the tray icon
		if (!m_modal_executing)
		{
			m_modal_executing = true;
			g_x11->GetWidgetList()->AddModalWidget(m_list_element);
			X11OpMessageLoop::Self()->NestLoop();
		}
	}
}

void X11Widget::Close()
{
	m_closed = TRUE;
	Hide();
}


void X11Widget::Hide()
{
	OP_ASSERT(m_handle != None);
	if (m_handle == None)
		return;

	// A shaded window can still be hidden (like when we hide opera from the taskbar)
	// To make this work, we unshade it here and hide the window again in HandleEvent()
	// once we receive the MapNotify event that the unshading produce. Not pretty, but 
	// seems to be a method all window managers can handle.
	if (m_shaded && !m_exit_started)
	{
		m_shaded_in_hide = true;
		SetShaded(false);
	}

	if (!m_destroyed)
	{
		XUnmapWindow(m_display, m_handle);
		XFlush(m_display); // Let server receive message right away so that grabs are removed
	}
	m_visible = false;
	m_show_on_application_show = false;

	if (m_window_flags & POPUP)
	{
		for (X11Widget *child = FirstChild(); child; child = child->Suc())
			child->Hide();
	}

	if ((m_window_flags & MODAL) && m_modal_executing)
	{
		// Only allow to leave a message loop if the window has been closed. A dialog
		// may be hidden by hiding all of opera by blicking the tray icon. We do not
		// want to exit the local loop then. (leave a fallback open if opera is being
		// closed).
		if (m_closed || m_exit_started)
		{
			m_modal_executing = false;
			g_x11->GetWidgetList()->RemoveModalWidget(m_list_element);
			X11OpMessageLoop::Self()->StopCurrent();
		}
	}
}


void X11Widget::OnVisibilityChange()
{
	if (!IsToplevel())
	  	return;

	// Sync. widget and window visibility state. Window manager requests
	// are sent to the widget and in this case the window must be updated
	X11OpWindow* window = GetOpWindow();
	if (window && m_visible != window->IsVisible())
		window->SetWidgetVisibility(m_visible);
}


void X11Widget::OnApplicationShow(bool show)
{
	if (m_toplevel && GetOpWindow())
	{
		if (show)
		{
			if (m_show_on_application_show)
			{
				Show();
				SetWMSkipTaskbar(m_skip_taskbar);
				m_show_on_application_show = false;
			}
		}
		else
		{
			if (m_visible)
			{
				Hide();

				if (!(m_window_flags & (POPUP|TOOLTIP|OFFSCREEN)))
					m_show_on_application_show = true;

				// Save taskbar setting in case it was already off
				bool skip_taskbar = m_skip_taskbar;
				SetWMSkipTaskbar(TRUE);
				m_skip_taskbar = skip_taskbar;
			}
		}
	}
}


void X11Widget::Resize(int w, int h)
{
	if (w <= 0 || h <= 0 || m_window_state != NORMAL)
		return;
	
	if (m_mapped_rect.width == w && m_mapped_rect.height == h)
		return;

	m_mapped_rect.width  = w;
	m_mapped_rect.height = h;

	if (m_toplevel)
	{
		m_toplevel->normal_width  = m_mapped_rect.width;
		m_toplevel->normal_height = m_mapped_rect.height;

		if (OpStatus::IsError(CreateSizeHintsStruct()))
		{
			// FIXME: OOM
		}
		else
		{
			m_sizehints->width  = m_mapped_rect.width;
			m_sizehints->height = m_mapped_rect.height;
			m_sizehints->flags |= PSize | USSize;	
			XSetWMNormalHints(m_display, m_handle, m_sizehints);
		}

		XWindowChanges changes;
		changes.width  = m_mapped_rect.width;
		changes.height = m_mapped_rect.height;
		XReconfigureWMWindow(m_display, m_handle, m_screen, CWWidth | CWHeight, &changes);
	}
	XResizeWindow(m_display, m_handle, m_mapped_rect.width, m_mapped_rect.height);

	if (!m_toplevel && !m_pending_resize)
	{
		m_pending_resize = true;
		PostCallback(0);
	}
}

void X11Widget::GetSize(int *w, int *h)
{
	if (w) *w = m_mapped_rect.width;
	if (h) *h = m_mapped_rect.height;
}

void X11Widget::Move(int x, int y)
{
	if (PreferWindowDragging() && m_window_drag.state != DSIdle)
	{
		if (IS_NET_SUPPORTED(_NET_WM_MOVERESIZE))
		{
			if (m_window_drag.state == DSPending)
			{
				m_mapped_rect.x = x;
				m_mapped_rect.y = y;
				MoveResizeWindow(MOVE_RESIZE_MOVE);
			}
		}
		else
		{
			m_mapped_rect.x = x;
			m_mapped_rect.y = y;
			XMoveWindow(m_display, m_handle, m_mapped_rect.x, m_mapped_rect.y);
		}
		m_window_drag.state = DSActive;
		return;
	}

	if (m_window_state != NORMAL)
		return;
	
	if (m_mapped_rect.x == x && m_mapped_rect.y == y)
		return;

	m_mapped_rect.x = x;
	m_mapped_rect.y = y;

	XMoveWindow(m_display, m_handle, m_mapped_rect.x, m_mapped_rect.y);
	
	if (m_toplevel)
	{
		m_toplevel->normal_x = m_mapped_rect.x;
		m_toplevel->normal_y = m_mapped_rect.y;

		if (OpStatus::IsError(CreateSizeHintsStruct()))
		{
			// FIXME: OOM
		}
		else
		{
			m_sizehints->x = m_mapped_rect.x;
			m_sizehints->y = m_mapped_rect.y;
			
			m_sizehints->flags |= PPosition | USPosition;
			XSetWMNormalHints(m_display, m_handle, m_sizehints);

			XWindowChanges changes;
			changes.x = m_mapped_rect.x;
			changes.y = m_mapped_rect.y;
			XReconfigureWMWindow(m_display, m_handle, m_screen, CWX | CWY, &changes);
		}
	}
}

void X11Widget::GetPos(int *x, int *y)
{
	if (x) *x = m_mapped_rect.x;
	if (y) *y = m_mapped_rect.y;
}

OP_STATUS X11Widget::SetMinSize(int w, int h)
{
	RETURN_IF_ERROR(CreateSizeHintsStruct());
	m_sizehints->min_width = w;
	m_sizehints->min_height = h;
	m_sizehints->flags |= PMinSize;
	XSetWMNormalHints(m_display, m_handle, m_sizehints);
	return OpStatus::OK;
}

OP_STATUS X11Widget::SetMaxSize(int w, int h)
{
	RETURN_IF_ERROR(CreateSizeHintsStruct());
	m_sizehints->max_width = w;
	m_sizehints->max_height = h;
	m_sizehints->flags |= PMaxSize;
	XSetWMNormalHints(m_display, m_handle, m_sizehints);
	return OpStatus::OK;
}

void X11Widget::SetGeometry(int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0 || m_window_state != NORMAL)
		return;

    m_mapped_rect.x      = x;
	m_mapped_rect.y      = y;
	m_mapped_rect.width  = w;
	m_mapped_rect.height = h;

	if (m_toplevel)
	{
		m_toplevel->normal_x      = m_mapped_rect.x;
		m_toplevel->normal_y      = m_mapped_rect.y;
		m_toplevel->normal_width  = m_mapped_rect.width;
		m_toplevel->normal_height = m_mapped_rect.height;

		if (OpStatus::IsError(CreateSizeHintsStruct()))
		{
			// FIXME: OOM
		}
		else
		{
			m_sizehints->x      = m_mapped_rect.x;
			m_sizehints->y      = m_mapped_rect.y;
			m_sizehints->width  = m_mapped_rect.width;
			m_sizehints->height = m_mapped_rect.height;
			m_sizehints->flags |= PPosition | USPosition | PSize | USSize;
			XSetWMNormalHints(m_display, m_handle, m_sizehints);
		}

		XWindowChanges changes;
		changes.x      = m_mapped_rect.x;
		changes.y      = m_mapped_rect.y;
		changes.width  = m_mapped_rect.width;
		changes.height = m_mapped_rect.height;
		XReconfigureWMWindow(m_display, m_handle, m_screen, CWX | CWY | CWWidth | CWHeight, &changes);
	}

	XMoveWindow(m_display, m_handle, m_mapped_rect.x, m_mapped_rect.y);
	XResizeWindow(m_display, m_handle, m_mapped_rect.width, m_mapped_rect.height);

	if (!m_toplevel && !m_pending_resize)
	{
		m_pending_resize = true;
		PostCallback(0);
	}
}

const uni_char* X11Widget::GetTitle()
{
	return m_title.CStr();
}

OP_STATUS X11Widget::SetTitle(const uni_char *title)
{
	if (!title)
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_title.Set(title));

	OpString8 title_utf8;
	RETURN_IF_ERROR(title_utf8.SetUTF8FromUTF16(title));
	XChangeProperty(m_display, m_handle, ATOMIZE(_NET_WM_NAME), ATOMIZE(UTF8_STRING), 8, PropModeReplace,
					reinterpret_cast<const unsigned char*>(title_utf8.CStr()), title_utf8.Length());

	XTextProperty text;
	char* t = title_utf8.CStr();
	int rc = Xutf8TextListToTextProperty(m_display, &t, 1, XStdICCTextStyle, &text);
	if (rc == 0)
	{ 
		XSetWMName(m_display, m_handle, &text);
		XSetWMIconName(m_display, m_handle, &text);
		XFree(text.value);
	}

	return OpStatus::OK;
}

void X11Widget::Raise()
{
	XRaiseWindow(m_display, m_handle);
}

void X11Widget::Lower()
{
	XLowerWindow(m_display, m_handle);
}

void X11Widget::SetWindowState(WindowState newstate)
{
#ifdef DEBUG_WM_STATE
	const char *win_state[4] = {
		"NORMAL",
		"MAXIMIZED",
		"ICONIFIED",
		"FULLSCREEN"
	};
	printf("switch state: %s -> %s\n",
			win_state[m_window_state], win_state[newstate]);
#endif

	if (newstate == m_window_state)
		return; // Nothing to do.

	if (!m_toplevel)
		return;

	if (m_window_state == FULLSCREEN)
	{
		// Window manager shall restore the state by its own. The result gets written to
		// the _NET_WM_STATE property which triggers a PropertyNotify to this window. We
		// update the state there. TODO: This means there is a timing issue related to
		// the state change which we should examine/deal with later [espen 2010-06-18]
		ToggleFullscreen();
	}
	else
	{
		switch (newstate)
		{
			case MAXIMIZED:
				SetMaximized(true);
				break;

			case ICONIFIED:
				XIconifyWindow(m_display, m_handle, m_screen);
				break;

			case FULLSCREEN:
				SetFullscreen(true);
				break;

			case NORMAL:
				if (m_window_state == MAXIMIZED)
					SetMaximized(false);
				SetGeometry(m_toplevel->normal_x, m_toplevel->normal_y,
							m_toplevel->normal_width, m_toplevel->normal_height);
				break;
		}
	}
	m_window_state = newstate;
	OnWindowStateChanged(m_window_state);
}


void X11Widget::SetAcceptDrop(bool accept)
{
	if (m_accept_drop != accept)
	{
		m_accept_drop = accept;
		if (m_accept_drop)
		{
			X11Types::Atom version_atom = (X11Types::Atom)X11OpDragManager::GetProtocolVersion();
			XChangeProperty(m_display, m_handle, ATOMIZE(XdndAware),
							XA_ATOM, 32, PropModeReplace, (unsigned char *)&version_atom, 1);
		}
		else
			XDeleteProperty(m_display, m_handle, ATOMIZE(XdndAware));
	}
}


void X11Widget::SetIcon(OpWidgetImage* image)
{
	OP_DELETE(m_icon);

	m_icon = OP_NEW(X11Icon,());
	if (m_icon)
	{
		m_icon->SetBuffer(image);
		m_icon->SetPixmap(image, TRUE, 24, g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON));
		m_icon->Apply(m_handle);
	}
}



void X11Widget::SetIcon( X11Icon* icon )
{
	OP_DELETE(m_icon);

	m_icon = icon;

	if (m_icon)
		m_icon->Apply(m_handle);
}


void X11Widget::Grab()
{
	GrabMouse();
	GrabKeyboard();
}


void X11Widget::Ungrab()
{
	UngrabMouse();
	UngrabKeyboard();
}


void X11Widget::GrabMouse()
{
	//printf("%p X11Widget::GrabMouse(): window: %08x\n", this, m_handle );

	if (!m_disable_grab)
	{
		int rc = XGrabPointer(m_display, m_handle, False,
					 PointerMotionMask | ButtonPressMask | ButtonReleaseMask |
					 EnterWindowMask | LeaveWindowMask, GrabModeAsync,
					 GrabModeAsync, None, None, X11OpMessageLoop::GetXTime());
		if( rc != GrabSuccess )
		{
			//printf("%p Grab mouse failed: %d\n", this, rc );
		}
		g_x11->GetWidgetList()->AddGrabbedWidget(this);

	}
	m_grabbed = true;

}

void X11Widget::UngrabMouse()
{
	//printf("%p X11Widget::UngrabMouse()\n", this);

	int rc = XUngrabPointer(m_display, X11OpMessageLoop::GetXTime());
	if( rc != 0 )
	{
		//printf("%p Ungrab mouse failed: %d\n", this, rc );
	}
	OpStatus::Ignore(g_x11->GetWidgetList()->RemoveGrabbedWidget(this));
	m_grabbed = false;
}


void X11Widget::GrabKeyboard()
{
	//printf("%p X11Widget::GrabKeyboard()\n", this );

	if (!m_disable_grab)
	{
		X11Types::Window handle = m_handle;
		X11Widget * proxy = GetFocusProxy();
		OP_ASSERT(proxy);
		if (proxy)
			handle = proxy->GetWindowHandle();

		int rc = XGrabKeyboard(m_display, handle, True, GrabModeAsync, GrabModeAsync, X11OpMessageLoop::GetXTime());
		if( rc != GrabSuccess )
		{
			//printf("%p Grab keyboard failed: %d\n", this, rc );
		}
	}
}


void X11Widget::UngrabKeyboard()
{
	//printf("%p X11Widget::UngrabKeyboard()\n", this );

	int rc = XUngrabKeyboard(m_display, X11OpMessageLoop::GetXTime());
	if( rc != 0 )
	{
		//printf("%p Ungrab keyboard failed: %d\n", this, rc );
	}
}


void X11Widget::CopyArea(GC gc, int src_x, int src_y, int width, int height, int dst_x, int dst_y)
{
	OP_ASSERT(m_recent_scrolls_first_unused != 0 || m_recent_scrolls == 0);
	/* Remove pending scroll requests that we know have been handled
	 * by the server (to ensure the list does not grow unbounded).
	 */
	unsigned long done_seqno = X11OpMessageLoop::GetXRecentlyProcessedRequestNumber();
	while (m_recent_scrolls != m_recent_scrolls_first_unused &&
		   (m_recent_scrolls->m_serial_after <= done_seqno ||
			NextRequest(m_display) < m_recent_scrolls->m_serial_before))
	{
		ScrollData * p = m_recent_scrolls;
		m_recent_scrolls = p->m_next;
		p->m_next = m_recent_scrolls_first_unused->m_next;
		m_recent_scrolls_first_unused->m_next = p;
	};

	OP_ASSERT(m_recent_scrolls_first_unused != 0 || m_recent_scrolls == 0);
	if (m_recent_scrolls == m_recent_scrolls_first_unused &&
		m_recent_scrolls != 0)
	{
		/* Delete one "unused" entry each time the list is empty.  But
		 * always leave a few available.
		 */
		ScrollData * n = m_recent_scrolls_first_unused->m_next;
		int how_many_to_keep = 3;
		while (how_many_to_keep > 0 &&
			   n != 0)
		{
			n = n->m_next;
			how_many_to_keep -= 1;
		};
		if (n != 0)
		{
			ScrollData * p = m_recent_scrolls_first_unused->m_next;
			m_recent_scrolls_first_unused->m_next = p->m_next;
			OP_DELETE(p);
		};
	};

	/* Perform the XCopyArea call.
	 */
	unsigned long seqno_before = NextRequest(m_display);
	XCopyArea(GetDisplay(), GetWindowHandle(), GetWindowHandle(), gc,
			  src_x, src_y, width, height, dst_x, dst_y);

	/* Append this scroll request to the 'pending scrolls' list.
	 */
	if (m_recent_scrolls == 0)
	{
		m_recent_scrolls = OP_NEW(ScrollData, ());
		m_recent_scrolls_first_unused = m_recent_scrolls;
	};
	OP_ASSERT(m_recent_scrolls_first_unused != 0 || m_recent_scrolls == 0);
	if (m_recent_scrolls_first_unused != 0 && m_recent_scrolls_first_unused->m_next == 0)
		m_recent_scrolls_first_unused->m_next = OP_NEW(ScrollData, ());

	if (m_recent_scrolls_first_unused != 0 && m_recent_scrolls_first_unused->m_next != 0)
	{
		/* All is well.  If all is not well here, we will forget about
		 * this scroll, and will probably have missing redraws and
		 * ensuing display corruption.  But that's acceptable under
		 * these circumstances.
		 */
		m_recent_scrolls_first_unused->Set(src_x, src_y, width, height,
										   dst_x, dst_y, seqno_before, NextRequest(m_display));
		m_recent_scrolls_first_unused = m_recent_scrolls_first_unused->m_next;
	};
	OP_ASSERT(m_recent_scrolls_first_unused != 0 || m_recent_scrolls == 0);
};



void X11Widget::GetNormalSize(int *w, int *h)
{
	if (w) *w = m_toplevel ? m_toplevel->normal_width : m_mapped_rect.width;
	if (h) *h = m_toplevel ? m_toplevel->normal_height : m_mapped_rect.height;
}

void X11Widget::GetNormalPos(int *x, int *y)
{
	if (x) *x = m_toplevel ? m_toplevel->normal_x : m_mapped_rect.x;
	if (y) *y = m_toplevel ? m_toplevel->normal_y : m_mapped_rect.y;
}

void X11Widget::DisableUpdates(bool disable)
{
	m_no_update = disable;
	for (X11Widget *child = FirstChild(); child; child = child->Suc())
		child->DisableUpdates(m_no_update);
}

void X11Widget::Update(int x, int y, int w, int h)
{
	if (m_no_update)
		return;

	XEvent event;
	event.type = Expose;
	event.xexpose.x = x;
	event.xexpose.y = y;
	event.xexpose.width = w + 1;
	event.xexpose.height = h + 1;
	event.xexpose.display = m_display;
	event.xexpose.window = m_handle;
	XSendEvent(m_display, m_handle, False, 0, &event);
}

void X11Widget::UpdateAll(bool now)
{
	if (m_no_update)
		return;

	// 'now' is ignored. We have no means of doing it 'now' anyway...

	int width, height;
	GetSize(&width, &height);
	Update(0, 0, width, height);
	for (X11Widget *child = FirstChild(); child; child = child->Suc())
		child->UpdateAll(now);
}

void X11Widget::Sync()
{
	XEvent* e=0;
	CompressEvents(e, MotionNotify);
	if (e)
		X11OpMessageLoop::Self()->HandleXEvent(e, this);
}

void X11Widget::ReleaseCapture()
{
	if (m_mde_screen)
		m_mde_screen->ReleaseMouseCapture();
	g_x11->GetWidgetList()->SetCapturedWidget(0);
}


void X11Widget::Reparent(X11Widget *parent)
{
	if (Parent())
		Parent()->DescendantRemoved(this);
	X11Types::Window phandle;
	if (parent)
		phandle = parent->GetWindowHandle();
	else
		phandle = RootWindow(m_display, m_screen);
	XReparentWindow(m_display, m_handle, phandle, m_mapped_rect.x, m_mapped_rect.y);

	Out();
	if (parent)
	{
		Under(parent);
		parent->DescendantAdded(this);
	}
}

void X11Widget::GiveFocus()
{
	if (!IsVisible())
		return;

	XSetInputFocus(m_display, m_handle, RevertToPointerRoot, CurrentTime);
}

void X11Widget::RemoveFocus()
{
	if (!IsVisible())
		return;

	XSetInputFocus(m_display, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void X11Widget::PointToGlobal(int *x, int *y)
{
	// Slow!

	int dx, dy;
	X11Types::Window child;
	if (XTranslateCoordinates(m_display, m_handle, RootWindow(m_display, m_screen),
							  *x, *y, &dx, &dy, &child))
	{
		*x = dx;
		*y = dy;
	}
}

void X11Widget::PointFromGlobal(int *x, int *y)
{
	// Slow!

	int dx, dy;
	X11Types::Window child;
	if (XTranslateCoordinates(m_display, RootWindow(m_display, m_screen), m_handle,
							  *x, *y, &dx, &dy, &child))
	{
		*x = dx;
		*y = dy;
	}
}

X11Widget * X11Widget::GetFocusProxy()
{
	if (m_toplevel && m_toplevel->focus_proxy)
		return m_toplevel->focus_proxy;
	if (IsToplevel())
		return 0;
	if (Parent())
		return Parent()->GetFocusProxy();
	return 0;
};

void X11Widget::DescendantAdded(X11Widget *child)
{
	if (Parent())
		Parent()->DescendantAdded(child);
}

void X11Widget::DescendantRemoved(X11Widget *child)
{
	if (Parent())
		Parent()->DescendantRemoved(child);
}

void X11Widget::OnVEGAWindowDead(VEGAWindow * w)
{
	if (m_vega_window == w)
		m_vega_window = NULL;
}

void X11Widget::FindAndActivateDesktopWindow(int x, int y)
{
	if (!m_mde_screen->m_captured_input)
	{
		// Find the first DesktopWindow above or equal to the view
		MDE_View *view = m_mde_screen->GetViewAt(x, y, true);
		while (view)
		{
			if (view->IsType("MDE_Widget"))
			{
				X11OpWindow *win = (X11OpWindow*) ((MDE_Widget*)view)->GetOpWindow();
				if (win && win->GetDesktopWindow())
				{
					if (!win->GetDesktopWindow()->IsActive())
						win->Activate();
					return;
				}
			}
			view = view->m_parent;
		}
	}
}


bool X11Widget::HandleEvent(XEvent *event)
{
	switch (event->type)
	{
	case MapNotify:
		m_visible = true;
		UpdateWmData();
		SetToplevelRect(m_unmapped_rect);
		UpdateShape();

		if (m_shaded_in_hide)
		{
			// The widget was shaded when we called Hide(). We had to unshade it first
			// as no window manager seem to allow hiding ("hiding" is not the same as 
			// "minimizing", we hide a window by clicking on the task bar icon) a shaded 
			// window. The unshading produces the MapNotify we catch here. This allows 
			// us to hide all windows, even the shaded ones, by clicking in the taskbar button.
			m_shaded_in_hide = false;
			Hide();
		}

		OnVisibilityChange();

		return true;

	case UnmapNotify:
		m_visible = false;
		m_grabbed = false;
		m_shaded_in_hide = false;
		m_unmapped_rect = m_mapped_rect;
		if (m_toplevel)
		{
			// The unmapped rect is always without decorations
			m_unmapped_rect.OffsetBy(m_toplevel->decoration.TopLeft());
		}

		OpStatus::Ignore(g_x11->GetWidgetList()->RemoveGrabbedWidget(this));

		OnVisibilityChange();

		return true;

	case DestroyNotify:
		m_destroyed = true;
		break;

	case ConfigureNotify:
	{
		if (!m_toplevel)
			return true;

		XEvent* e = event;
		CompressEvents(e, ConfigureNotify);
		OpRect rect = OpRect(e->xconfigure.x, e->xconfigure.y, e->xconfigure.width, e->xconfigure.height);

		// The rectangle is the size and position of our window, not the wm window
		// which includes the decoration. The wm window is normally bigger and its
		// origin is up-left of our window.

		// metacity and other non-composited window managers
		// return different values in x,y for window moving and
		// window resizing - this breakes m_mapped_rect, and causes
		// all sorts of troubles for window positioning
		// see DSK-285955 [pobara 16-04-2010]
		if (!SupportsExternalCompositing())
			GetToplevelRect(rect);

		SetToplevelRect(rect);

		if (GetOpWindow() && GetOpWindow()->GetWindowListener())
			GetOpWindow()->GetWindowListener()->OnMove();

		UpdateShape();

		if (m_toplevel->sync_counter_id && !XSyncValueIsZero(m_toplevel->sync_counter_value))
		{
			XSyncSetCounter(m_display, m_toplevel->sync_counter_id, m_toplevel->sync_counter_value);
			XSyncIntToValue(&m_toplevel->sync_counter_value, 0);
		}

		return true;
	}
	case PropertyNotify:
	{
		X11Types::Atom rettype;
		int format;
		unsigned long nitems;
		unsigned long bytes_remaining;
		unsigned char *data;

		X11Types::Atom prop = event->xproperty.atom;
		if (prop == ATOMIZE(WM_STATE))
		{
			if (event->xproperty.state == PropertyDelete)
			{
				// ICCCM 4.1.3.1 Not supported yet. WM has removed the 
				// WM State property and window is in  withdrawn state
			}
			else if (GetWindowHandle() != RootWindow(m_display,m_screen))
			{
				WindowState old_window_state = m_window_state;

				int rc = XGetWindowProperty(m_display, m_handle, event->xproperty.atom, 0,
											2, False, AnyPropertyType, &rettype,
											&format, &nitems, &bytes_remaining, &data);
				if (rc == Success && rettype == ATOMIZE(WM_STATE) && format == 32 && nitems > 0)
				{
					long* state = (long*) data;
					switch (state[0]) 
					{
						case WithdrawnState:
							// Not supported yet
							break;
						case IconicState:
							m_window_state = ICONIFIED;
							break;
						case NormalState:
							// ICCM did not define a MAXIMIXED or FULLSCREEN state. NormalState
							// means anything that is not MINIMIZED. The only thing we can do is 
							// to change from a minimized state if that is the current state. 
							if (m_window_state == ICONIFIED)
							{
								m_window_state = NORMAL;
							}
							break;
					}
				}

				if (data)
					XFree(data);

				if (old_window_state != m_window_state)
					OnWindowStateChanged(m_window_state);
			}
		}
		else if (prop == ATOMIZE(_NET_WM_STATE))
		{
			int rc = XGetWindowProperty(m_display, m_handle, event->xproperty.atom, 0,
										1024, False, AnyPropertyType, &rettype,
										&format, &nitems, &bytes_remaining, &data);
#ifdef DEBUG_WM_STATE
			printf("_NET_WM_STATE: %d %lu %d %lu %lu\n", rc, rettype, format, nitems, bytes_remaining);
#endif
			if (rc == Success)
			{
				WindowState old_window_state = m_window_state;

				if (data && rettype == XA_ATOM && format == 32)
				{
					X11Types::Atom* state = (X11Types::Atom *)data;
					BOOL max  = FALSE;
					BOOL min  = FALSE;
					BOOL full = FALSE;
					BOOL shaded = FALSE;
					BOOL skip_task_bar = FALSE;

					for (unsigned long i=0; i < nitems; i++)
					{
						if (state[i] == ATOMIZE(_NET_WM_STATE_MAXIMIZED_VERT) || state[i] == ATOMIZE(_NET_WM_STATE_MAXIMIZED_HORZ))
							max = TRUE;
						else if (state[i] == ATOMIZE(_NET_WM_STATE_FULLSCREEN))
							full = TRUE;
						else if (state[i] == ATOMIZE(_NET_WM_STATE_HIDDEN))
							min = TRUE;
						else if (state[i] == ATOMIZE(_NET_WM_STATE_SHADED))
							shaded = TRUE;
						else if (state[i] == ATOMIZE(_NET_WM_STATE_SKIP_TASKBAR))
							skip_task_bar = TRUE;

					}

#ifdef DEBUG_WM_STATE
					printf("max=%d, min=%d, full=%d, shaded=%d\n", max, min, full, shaded);
#endif
					// Test full first. There might be a combination of states.
					if (full)
						m_window_state = FULLSCREEN;
					else if (max)
						m_window_state = MAXIMIZED;
					else if (min)
						m_window_state = ICONIFIED;
					else
						m_window_state = NORMAL;

					m_shaded = shaded;
					// Minimized + no taskbar means a window has been hidden. Maybe from a minimized state
					// When shown it shall not be restored to a shaded state (to seems all WMs and programs 
					// behave this way)
					if (min && skip_task_bar)
						m_shaded = m_shaded_in_hide = false;
				}
				else
				{
					// Fallback. Some window managers will land here when the window gets withdrawn by clicking
					// on the task bar button. It happens because the rettype atom is 0. Kwin behaves properly 
					// in that case with all states set to 0
					m_window_state = NORMAL;
					m_shaded = false;
				}
				if (old_window_state != m_window_state)
					OnWindowStateChanged(m_window_state);
			}

			if (data)
				XFree(data);

			UpdateShape();
		}
		else if (prop == ATOMIZE(_NET_FRAME_EXTENTS))
		{
			if (m_toplevel)
			{
				switch(event->xproperty.state)
				{
					case PropertyNewValue:
						if (!GetFrameExtents(m_toplevel->decoration))
							m_toplevel->decoration = OpRect(0,0,0,0);
					break;

					case PropertyDelete:
						m_toplevel->decoration = OpRect(0,0,0,0);
					break;
				}
			}
		}
		else if (prop == ATOMIZE(_NET_FRAME_WINDOW))
		{
			if (m_toplevel)
			{
				switch(event->xproperty.state)
				{
					case PropertyNewValue:
						if (!GetFrameWindow(m_toplevel->decoration_window))
							m_toplevel->decoration_window = None;
					break;

					case PropertyDelete:
						m_toplevel->decoration_window = None;
					break;
				}
			}
		}
		return true;
	}

	case ClientMessage:
	{
		if (event->xclient.format == 32 && event->xclient.message_type == ATOMIZE(WM_PROTOCOLS)) 
		{
			X11Types::Atom atom = event->xclient.data.l[0];
			if (atom == ATOMIZE(WM_DELETE_WINDOW))
			{
				// 1 Check if a modal window is a decendant. Then we can not close
				X11Widget* widget = g_x11->GetWidgetList()->GetModalWidget();
				if (widget)
					widget = widget->Parent();		   
				while(widget)
				{
					if(widget == this)
						break;
					else
						widget = widget->Parent();
				}
				if (widget == this)
					return true;
				
				// 2 Check that there are no open toolkit dialog. Then we can not close
				widget = g_x11->GetWidgetList()->GetModalToolkitParent();
				if (widget && widget->Contains(this))
				{
					return true;
				}
				
				// 3 Let the window decide itself if it wants to be closed
				ConfirmClose();
				return true;
			}
			else if (atom == ATOMIZE(_NET_WM_SYNC_REQUEST) && (m_window_flags&SYNC_REQUEST) ) 
			{
				unsigned long timestamp = event->xclient.data.l[1];
				if (timestamp > X11OpMessageLoop::GetXTime())
				{
					// A general ClientMessage event does not inlcude a time member, but 
					// this particular type does in the data section
					X11OpMessageLoop::SetXTime(timestamp);
				}
				if (m_toplevel)
					XSyncIntsToValue(&m_toplevel->sync_counter_value, event->xclient.data.l[2], event->xclient.data.l[3]);
				return true;
			}
			else if( atom == ATOMIZE(WM_TAKE_FOCUS) )
			{
				X11Widget * proxy = GetFocusProxy();
				if (proxy != 0)
				{
					//printf("Set input focus to %08x\n", (int)proxy->GetWindowHandle());
					XSetInputFocus(m_display, proxy->GetWindowHandle(), RevertToParent, event->xclient.data.l[1]);
				};
				return true;
			}
		}
		break;
	}

	case Expose:
	case GraphicsExpose:
		return HandleExposeEvent(&event->xexpose);

	case MotionNotify:
	{
		if (m_mde_screen)
		{
			XEvent *e = event;
			CompressEvents(e, MotionNotify);

			int x = e->xmotion.x;
			int y = e->xmotion.y;

			// DSK-293111 This happens because there is a captured widget in MDE
			// that is assumed to receive events. This will fail in X if there
			// is a window with a keyboard grab as well (as there is for dropdown
			// widgets) because X will send input events to the grabbed window. Redirect
			// events that are sent to the grabbed window to the captured widget.

			// The popup menu code has worked around this. I am not going to change that right now.
			X11Widget* widget = g_x11->GetWidgetList()->GetCapturedWidget();
			if(widget && widget != this && widget->m_mde_screen && !GetPopupWidget())
			{
				// Only send move events when mouse is pressed and/or inside, Ideally I would
				// like to have done that in Quick, but it is rather platform specific
				if (InputUtils::GetOpButtonFlags() || (x >= 0 && y >= 0 && x < GetWidth() && y < GetHeight()))
				{
					x = e->xmotion.x_root;
					y = e->xmotion.y_root;
					widget->PointFromGlobal(&x, &y);
					widget->m_mde_screen->TrigMouseMove(x, y, InputUtils::GetOpModifierFlags());
				}
			}
			else
			{
				if (!m_mde_screen->m_captured_input && !m_mde_screen->GetHitStatus(x, y))
				{
					// If a mouse event does not hit a view, just send it to the view at 0,0. Fixes some popup problems
					MDE_View* v = m_mde_screen->GetViewAt(0,0, true);
					if (v)
						v->OnMouseMove(x, y, InputUtils::GetOpModifierFlags());
				}
				else
					m_mde_screen->TrigMouseMove(x, y, InputUtils::GetOpModifierFlags());

				if (m_drag_hotspot.Start(x,y))
				{
					if ((e->xmotion.state & (Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask)) == Button1Mask)
					{
						g_x11->GetWidgetList()->SetDragSource(this);
						m_mde_screen->TrigDragStart(m_drag_hotspot.GetX(), m_drag_hotspot.GetY(), InputUtils::GetOpModifierFlags(), x, y);
					}
					m_drag_hotspot.Done();
				}
			}

			return true;
		}
		break;
	}

	case LeaveNotify:
	{
		if (m_mde_screen)
		{
			// Popups menus and menu bar set the 'm_propagate_leave_event' flag
			if (m_propagate_leave_event || event->xcrossing.mode != NotifyNormal)
			{
				if (!m_mde_screen->m_captured_input && !m_mde_screen->GetHitStatus(event->xcrossing.x, event->xcrossing.y))
				{
					// If a mouse event does not hit a view, just send it to the view at 0,0. Fixes some popup problems
					MDE_View* v = m_mde_screen->GetViewAt(0,0, true);
					if (v)
					{
						v->OnMouseLeave();
					}
				}
				return true;
			}
		}
		break;
	}
	
	case ButtonPress:
	case ButtonRelease:
	{
		g_x11->GetWidgetList()->SetInputWidget(this);

		if (PreferWindowDragging())
		{
			if (event->type == ButtonPress)
			{
				MouseButton operabutton = InputUtils::X11ToOpButton(event->xbutton.button);
				if (operabutton == MOUSE_BUTTON_1)
				{
					m_window_drag.state = DSPending;
				}
			}
			else
				m_window_drag.state = DSIdle;
		}

		if (m_mde_screen)
		{
			unsigned int button = event->xbutton.button;
			if (button >= 1 && button <= 3)
			{
				OpPoint pos(event->xbutton.x, event->xbutton.y);
				int operabutton = 1;
				switch (button)
				{
				case 2:
					operabutton = 3;
					break;
				case 3:
					operabutton = 2;
					break;
				}

				if (event->type == ButtonPress)
				{
					if (button == 1)
						m_drag_hotspot.Set(pos.x, pos.y);
					else
						m_drag_hotspot.Reset();

					// In some cases a new widget may be created while the button is down
					// (like first time gesture dialog). There can only be one captured widget so
					// release old one first if any.

					if (g_x11->GetWidgetList()->GetCapturedWidget() )
						g_x11->GetWidgetList()->GetCapturedWidget()->ReleaseCapture();

					// Set new capture
					g_x11->GetWidgetList()->SetCapturedWidget(this);

					int clickcount = InputUtils::GetClickCount();
					if (m_active_popup)
					{
						MDE_View* v = m_mde_screen->GetViewAt(pos.x, pos.y, true);
						bool is_hit = false;
						while (v && !is_hit)
						{
							if (v == m_active_popup->GetMDEWidget())
								is_hit = true;
							v = v->m_parent;
						}
						if (!is_hit)
						{
							// Show(FALSE) sets m_active_popup to NULL
							X11OpWindow* popup = m_active_popup;
							// Call Show(FALSE) first to trigger DesktopWindowListener::OnShow()
							popup->Show(FALSE);
							popup->Close(TRUE);
							return true;
						}
					}
					if (!m_mde_screen->m_captured_input && !m_mde_screen->GetHitStatus(pos.x, pos.y))
					{
						// If a mouse event does not hit a view, just send it to the view at 0,0. Fixes some popup problems
						MDE_View* v = m_mde_screen->GetViewAt(0,0, true);
						if (v)
						{
							v->OnMouseDown(pos.x, pos.y, operabutton, clickcount, InputUtils::GetOpModifierFlags());
						}
					}
					else
					{
						FindAndActivateDesktopWindow(pos.x, pos.y);
						m_mde_screen->TrigMouseDown(pos.x, pos.y, operabutton, clickcount, InputUtils::GetOpModifierFlags());
					}

					return true;
				}
				else
				{
					m_drag_hotspot.Reset();

					if (!m_mde_screen->m_captured_input && !m_mde_screen->GetHitStatus(pos.x, pos.y))
					{
						// If a mouse event does not hit a view, just send it to the view at 0,0. Fixes some popup problems
						MDE_View* v = m_mde_screen->GetViewAt(0,0, true);
						if (v)
						{
							v->OnMouseUp(pos.x, pos.y, operabutton, InputUtils::GetOpModifierFlags());
						}
					}
					else
					{
						m_mde_screen->TrigMouseUp(pos.x, pos.y, operabutton, InputUtils::GetOpModifierFlags());
					}

					// MDE capture should be fine at this stage, but we have to release
					// the captured X11Widget as well
					if (g_x11->GetWidgetList()->GetCapturedWidget())
						g_x11->GetWidgetList()->GetCapturedWidget()->ReleaseCapture();

					return true;
				}
			}
			else if (event->type == ButtonPress && (button == 4 || button == 5 || button == 6 || button == 7))
			{
				XEvent *final_event = event;
				XEvent more;
				while (XCheckTypedWindowEvent(m_display, m_handle, ButtonPress, &more))
				{
					if (button != more.xbutton.button)
					{
						XPutBackEvent(m_display, &more);
						break;
					}

					final_event = &more;
				}

				OpPoint pos(final_event->xbutton.x, final_event->xbutton.y);

				bool back = button == 4 || button == 6;
				bool vertical = button == 4 || button == 5;
				m_mde_screen->TrigMouseWheel(pos.x, pos.y, back ? -2 : 2, vertical, InputUtils::GetOpModifierFlags());
				return true;
			}
		}
		break;
	}
	}

	return false;
}


/* This function assumes the source scroll rectangle (src_x, src_y,
 * width, height) partially overlaps 'rect', such that parts of the
 * contents of rect is moved and parts is not.
 *
 * This function modifies 'rect' so that both the part of rect that is
 * moved by the scrolling and the part that is not are inside 'rect'.
 * (And preferably, 'rect' should be as small as possible, but that's
 * not guaranteed).
 */
static void enlarge_rect_by_scroll(MDE_RECT * rect, int src_x, int src_y, int width, int height, int dst_x, int dst_y)
{
	int dx = dst_x - src_x;
	if (dx < 0)
	{
		int x = rect->x + dx;
		if (x < dst_x)
			x = dst_x;
		if (x < rect->x)
		{
			rect->w += rect->x - x;
			rect->x = x;
		};
	}
	else
	{
		int x2 = rect->x + rect->w + dx;
		if (x2 > dst_x + width)
			x2 = dst_x + width;
		if (x2 > rect->x + rect->w)
			rect->w = x2 - rect->x;
	};
	int dy = dst_y - src_y;
	if (dy < 0)
	{
		int y = rect->y + dy;
		if (y < dst_y)
			y = dst_y;
		if (y < rect->y)
		{
			rect->h += rect->y - y;
			rect->y = y;
		};
	}
	else
	{
		int y2 = rect->y + rect->h + dy;
		if (y2 > dst_y + height)
			y2 = dst_y + height;
		if (y2 > rect->y + rect->h)
			rect->h = y2 - rect->y;
	};
};

//#define TEST_ENLARGE_RECT_BY_SCROLL

#if defined TEST_ENLARGE_RECT_BY_SCROLL
void expect_rectangle(const char * msg, const MDE_RECT & rect, int x, int y, int w, int h)
{
	if (rect.x != x || rect.y != y || rect.w != w || rect.h != h)
		printf("test failed: %s: %d %d %d %d != %d %d %d %d\n", msg, rect.x, rect.y, rect.w, rect.h, x, y, w, h);
};
#endif

#ifdef TEST_ENLARGE_RECT_BY_SCROLL
void test_enlarge_rect_by_scroll()
{
	printf("test_enlarge_rect_by_scroll\n");
	MDE_RECT rect = MDE_MakeRect(200, 200, 200, 200);
	enlarge_rect_by_scroll(&rect, 100, 100, 200, 200, 200, 200);
	expect_rectangle("NOP 1", rect, 200, 200, 200, 200);

	rect = MDE_MakeRect(200, 200, 200, 200);
	enlarge_rect_by_scroll(&rect, 300, 300, 200, 200, 200, 200);
	expect_rectangle("NOP 2", rect, 200, 200, 200, 200);

	rect = MDE_MakeRect(200, 200, 200, 200);
	enlarge_rect_by_scroll(&rect, 100, 100, 200, 200, 150, 150);
	expect_rectangle("NOP 3", rect, 200, 200, 200, 200);

	rect = MDE_MakeRect(200, 200, 200, 200);
	enlarge_rect_by_scroll(&rect, 300, 300, 200, 200, 250, 250);
	expect_rectangle("NOP 4", rect, 200, 200, 200, 200);

	rect = MDE_MakeRect(200, 200, 200, 200);
	enlarge_rect_by_scroll(&rect, 300, 300, 200, 200, 300, 400);
	expect_rectangle("Enlarge down", rect, 200, 200, 200, 300);

	rect = MDE_MakeRect(200, 200, 200, 200);
	enlarge_rect_by_scroll(&rect, 300, 300, 200, 200, 400, 300);
	expect_rectangle("Enlarge right", rect, 200, 200, 300, 200);

	rect = MDE_MakeRect(200, 200, 200, 200);
	enlarge_rect_by_scroll(&rect, 100, 100, 200, 200, 100, 0);
	expect_rectangle("Enlarge up", rect, 200, 100, 200, 300);

	rect = MDE_MakeRect(200, 200, 200, 200);
	enlarge_rect_by_scroll(&rect, 100, 100, 200, 200, 0, 100);
	expect_rectangle("Enlarge left", rect, 100, 200, 300, 200);
};
#endif // TEST_ENLARGE_RECT_BY_SCROLL


bool X11Widget::HandleExposeEvent(XExposeEvent * ev)
{
#ifdef TEST_ENLARGE_RECT_BY_SCROLL
	test_enlarge_rect_by_scroll();
#endif
	/* Some windows (e.g. the focus proxies) should never be visible.
	 * If we get an Expose event, move the window out of sight.
	 */
	if (ev->window == GetWindowHandle() &&
		GetFocusProxy() == this)
	{
		/* Shrink the window to 1x1 pixel.  Hopefully that won't be
		 * too noticeable.  And hopefully it will be moved out of
		 * sight again when there's no active edit field...
		 */
		/* Shrink first, to avoid rendering artifacts */
		int x = GetXpos();
		int y = GetYpos() + GetHeight();
		Resize(1, 1);
		Move(x, y);
	};

	if (m_mde_screen)
	{
		/* As the code stands, there will typically be generated no
		 * more than a single extra rectangle (when there is a scroll
		 * request we are not sure is accounted for in the event).
		 * The other scroll events will just move or enlarge the
		 * existing rectangles.
		 */
		const int MAX_RECTS = 8;
		MDE_RECT rects[MAX_RECTS];
		int rect_count = 0;
		rects[rect_count] = MDE_MakeRect(ev->x, ev->y, ev->width, ev->height);
		rect_count += 1;


		/* First, all elements in the "pending scrolls" list that we
		 * know have been completed on the X server should be removed.
		 */
		OP_ASSERT(m_recent_scrolls_first_unused != 0 || m_recent_scrolls == 0);
		while (m_recent_scrolls != m_recent_scrolls_first_unused &&
			   (m_recent_scrolls->m_serial_after <= ev->serial ||
				NextRequest(m_display) < m_recent_scrolls->m_serial_before))
		{
			ScrollData * p = m_recent_scrolls;
			m_recent_scrolls = p->m_next;
			p->m_next = m_recent_scrolls_first_unused->m_next;
			m_recent_scrolls_first_unused->m_next = p;
		};
		OP_ASSERT(m_recent_scrolls_first_unused != 0 || m_recent_scrolls == 0);

		/* And then we have to account for all the scrolling that
		 * wasn't completed on the X server before the expose event
		 * was generated.
		 */
		ScrollData * scroll = m_recent_scrolls;
		while (scroll != m_recent_scrolls_first_unused)
		{
			int first_rect_no = 0;
			if (scroll->m_serial_before <= ev->serial)
			{
				/* This piece of code should be executed for all
				 * scrolls that we don't know whether are properly
				 * reflected in the event.
				 *
				 * The tests are set up to run this code for the
				 * request with request number equal to ev->serial.
				 * This is probably unnecessarily paranoid.  The
				 * intention of the X specification is surely that all
				 * requests with sequence numbers equal to or earlier
				 * than ev->serial are accounted for in ev (and thus
				 * should be deleted from the list in the while loop
				 * above).
				 */
				while (rect_count * 2 > MAX_RECTS)
				{
					// This almost deserves:
					// OP_ASSERT(!"Potential performance issue: more rectangles than expected are produced");
					rects[rect_count - 2] = MDE_RectUnion(rects[rect_count-2], rects[rect_count-1]);
					rect_count -= 1;
				};
				first_rect_no = rect_count;
				int copycount = rect_count;
				for (int tocopy = 0; tocopy < copycount; tocopy ++)
				{
					rects[rect_count] = rects[tocopy];
					rect_count += 1;
				};
				OP_ASSERT(rect_count <= MAX_RECTS);
			};

			/* Move all invalidation rectangles according to the
			 * not-yet-executed scroll requests.
			 */
			for (int next_rect_no = first_rect_no; next_rect_no < rect_count; next_rect_no += 1)
			{
				MDE_RECT * next_rect = rects + next_rect_no;
				if (scroll->m_src_x <= next_rect->x &&
					scroll->m_src_y <= next_rect->y &&
					next_rect->x + next_rect->w <= scroll->m_src_x + scroll->m_width &&
					next_rect->y + next_rect->h <= scroll->m_src_y + scroll->m_height)
				{
					// The rectangle is wholly inside the scroll area.
					// I assume this is the common case.
					next_rect->x += scroll->m_dst_x - scroll->m_src_x;
					next_rect->y += scroll->m_dst_y - scroll->m_src_y;
				}
				else if (next_rect->x + next_rect->w <= scroll->m_src_x ||
						 next_rect->y + next_rect->h <= scroll->m_src_y ||
						 scroll->m_src_x + scroll->m_width <= next_rect->x ||
						 scroll->m_src_y + scroll->m_height <= next_rect->y)
				{
					// The rectangle is wholly outside the scroll area.
				}
				else
				{
					// The rectangle is split.  Ouch!

					/* Simplified solution, just enlarge the rectangle
					 * to cover the whole possible invalid area.
					 */
					enlarge_rect_by_scroll(next_rect,
										   scroll->m_src_x, scroll->m_src_y,
										   scroll->m_width, scroll->m_height,
										   scroll->m_dst_x, scroll->m_dst_y);
				};
			};
			scroll = scroll->m_next;
		};

		for (int rectno = 0; rectno < rect_count; rectno += 1)
			m_mde_screen->Invalidate(rects[rectno], true);
	}
	return true;
};


BOOL X11Widget::SupportsExternalCompositing()
{
	return g_x11->IsCompositingManagerActive(m_screen) && !g_startup_settings.no_argb;
}



void X11Widget::EmulateRelease(const OpPoint& pos)
{
	int operabutton = 1;

	if (m_mde_screen)
	{
		if (!m_mde_screen->m_captured_input && !m_mde_screen->GetHitStatus(pos.x, pos.y))
		{
			// If a mouse event does not hit a view, just send it to the view at 0,0. Fixes some popup problems
			MDE_View* v = m_mde_screen->GetViewAt(0,0, true);
			if (v)
				v->OnMouseUp(pos.x, pos.y, operabutton, InputUtils::GetOpModifierFlags());
		}
		else
			m_mde_screen->TrigMouseUp(pos.x, pos.y, operabutton, InputUtils::GetOpModifierFlags());
	}
}


BOOL X11Widget::GetFrameExtents(OpRect& rect)
{
	if (IS_NET_SUPPORTED(_NET_FRAME_EXTENTS))
	{
		Atom type;
		int format;
		unsigned long nitems, after;
		unsigned char *data = 0;

		int rc = XGetWindowProperty( 
			m_display, m_handle, ATOMIZE(_NET_FRAME_EXTENTS), 0, 4,
			False, XA_CARDINAL, &type, &format, &nitems, &after, &data);
		if (rc == Success && type == XA_CARDINAL && format == 32)
		{
			int left   = ((long*)data)[0];
			int right  = ((long*)data)[1];
			int top    = ((long*)data)[2];
			int bottom = ((long*)data)[3];
			rect = OpRect(left, top, right, bottom);
			XFree(data);
			return TRUE;
		}

		if (data)
			XFree(data);
	}

	return FALSE;
}


BOOL X11Widget::GetFrameWindow(X11Types::Window& window)
{
	if (IS_NET_SUPPORTED(_NET_FRAME_WINDOW))
	{
		Atom type;
		int format;
		unsigned long nitems, after;
		unsigned char *data = 0;

		int rc = XGetWindowProperty( 
			m_display, m_handle, ATOMIZE(_NET_FRAME_WINDOW), 0, 4,
			False, XA_WINDOW, &type, &format, &nitems, &after, &data);
		if (rc == Success && type == XA_WINDOW && format == 32)
		{
			window = ((long*)data)[0];
			XFree(data);
			return TRUE;
		}

		if (data)
			XFree(data);
	}

	return FALSE;
}



void X11Widget::SetMaximized(bool state)
{
	if (IS_NET_SUPPORTED(_NET_WM_STATE_MAXIMIZED_VERT))
	{
		if (m_visible) // Same as mapped
		{
			XEvent event;

			event.xclient.type = ClientMessage;
			event.xclient.display = m_display;
			event.xclient.window = m_handle;
			event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
			event.xclient.format = 32;
			event.xclient.data.l[0] = state ? 1 : 0;
			event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_MAXIMIZED_HORZ);
			event.xclient.data.l[2] = ATOMIZE(_NET_WM_STATE_MAXIMIZED_VERT);
			event.xclient.data.l[3] = 0;
			event.xclient.data.l[4] = 0;

			X11Types::Window root = RootWindow(m_display, m_screen);
			XSendEvent(m_display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
		}
		else
		{
			// We do not remove a property is this mode
			long net_maximized[2] = {
				ATOMIZE(_NET_WM_STATE_MAXIMIZED_HORZ),
				ATOMIZE(_NET_WM_STATE_MAXIMIZED_VERT)
			};
			XChangeProperty(m_display, m_handle,
							ATOMIZE(_NET_WM_STATE), XA_ATOM, 32, PropModeReplace,
							(unsigned char *)&net_maximized, 2);
		}
	}
}


void X11Widget::SetShaded(bool state)
{
	if (IS_NET_SUPPORTED(_NET_WM_STATE_SHADED) && state != m_shaded)
	{
		XEvent event;

		event.xclient.type = ClientMessage;
		event.xclient.display = m_display;
		event.xclient.window = m_handle;
		event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
		event.xclient.format = 32;
		event.xclient.data.l[0] = state ? 1 : 0;
		event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_SHADED);
		event.xclient.data.l[2] = 0;
		event.xclient.data.l[3] = 0;
		event.xclient.data.l[4] = 0;

		X11Types::Window root = RootWindow(m_display, m_screen);
		XSendEvent(m_display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
	}
}



void X11Widget::SetFullscreen(bool state)
{
	if (IS_NET_SUPPORTED(_NET_WM_STATE_FULLSCREEN))
	{
		if (m_visible) // Same as mapped
		{
			XEvent event;

			event.xclient.type = ClientMessage;
			event.xclient.display = m_display;
			event.xclient.window = m_handle;
			event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
			event.xclient.format = 32;
			event.xclient.data.l[0] = state ? 1 : 0;
			event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_FULLSCREEN);
			event.xclient.data.l[2] = 0;
			event.xclient.data.l[3] = 0;
			event.xclient.data.l[4] = 0;

			X11Types::Window root = RootWindow(m_display, m_screen);
			XSendEvent(m_display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
		}
		else
		{
			long fullscreen = ATOMIZE(_NET_WM_STATE_FULLSCREEN);
			XChangeProperty(m_display, m_handle,
							ATOMIZE(_NET_WM_STATE), XA_ATOM, 32, PropModeReplace,
							(unsigned char *)&fullscreen, 1);
		}
	}
}


void X11Widget::ToggleFullscreen()
{
	if (IS_NET_SUPPORTED(_NET_WM_STATE_FULLSCREEN))
	{
		XEvent event;

		event.xclient.type = ClientMessage;
		event.xclient.display = m_display;
		event.xclient.window = m_handle;
		event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
		event.xclient.format = 32;
		event.xclient.data.l[0] = 2; // toggle
		event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_FULLSCREEN);
		event.xclient.data.l[2] = 0;
		event.xclient.data.l[3] = 0;
		event.xclient.data.l[4] = 0;

		X11Types::Window root = RootWindow(m_display, m_screen);
		XSendEvent(m_display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
	}
}



void X11Widget::SetBelow( bool state )
{
	if (IS_NET_SUPPORTED(_NET_WM_STATE) && IS_NET_SUPPORTED(_NET_WM_STATE_BELOW))
	{
		XEvent event;

		event.xclient.type = ClientMessage;
		event.xclient.display = m_display;
		event.xclient.window = m_handle;
		event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
		event.xclient.format = 32;
		event.xclient.data.l[0] = state ? 1 : 0;
		event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_BELOW);
		event.xclient.data.l[2] = 0;
		event.xclient.data.l[3] = 0;
		event.xclient.data.l[4] = 0;

		X11Types::Window root = RootWindow(m_display, m_screen);
		XSendEvent(m_display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
	}
	else
	{
		Lower();
	}
}


void X11Widget::SetAbove( bool state )
{	
	if (IS_NET_SUPPORTED(_NET_WM_STATE))
	{
		XEvent event;

		event.xclient.type = ClientMessage;
		event.xclient.display = m_display;
		event.xclient.window = m_handle;
		event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
		event.xclient.format = 32;
		event.xclient.data.l[0] = state ? 1 : 0;
		event.xclient.data.l[2] = 0;
		event.xclient.data.l[3] = 0;
		event.xclient.data.l[4] = 0;
		
		if( IS_NET_SUPPORTED(_NET_WM_STATE_ABOVE) )
			event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_ABOVE);
		else if( IS_NET_SUPPORTED(_NET_WM_STATE_STAYS_ON_TOP) )
			event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_STAYS_ON_TOP);
		else
		{
			Raise();
			return;
		}
		
		X11Types::Window root = RootWindow(m_display, m_screen);
		XSendEvent(m_display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
	}
	else
	{
		Raise();
	}
}


void X11Widget::SetWMSkipTaskbar( bool state )
{
	if (IS_NET_SUPPORTED(_NET_WM_STATE_SKIP_TASKBAR))
	{
		XEvent event;

		event.xclient.type = ClientMessage;
		event.xclient.display = m_display;
		event.xclient.window = m_handle;
		event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
		event.xclient.format = 32;
		event.xclient.data.l[0] = state ? 1 : 0;
		event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_SKIP_TASKBAR);
		event.xclient.data.l[2] = 0;
		event.xclient.data.l[3] = 0;
		event.xclient.data.l[4] = 0;

		X11Types::Window root = RootWindow(m_display, m_screen);
		XSendEvent(m_display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
		m_skip_taskbar = state;
	}
}


void X11Widget::SetWindowRole(const char* name)
{
	if (name && *name)
		XChangeProperty(m_display, m_handle, ATOMIZE(WM_WINDOW_ROLE), XA_STRING, 8, PropModeReplace, (unsigned char *)name, strlen(name));
}


void X11Widget::SetWindowName(const uni_char* name)
{
	if (name && *name)
		XChangeProperty(m_display, m_handle, ATOMIZE(OPERA_WINDOW_NAME), XA_STRING, 8, PropModeReplace, (unsigned char *)name, uni_strlen(name)*2);
}


void X11Widget::SetWindowName(UINT32 indentifier)
{
	OpString name;
	if (OpStatus::IsSuccess(name.AppendFormat(UNI_L("opera%d"), indentifier)))
		SetWindowName(name);
}


void X11Widget::SetWindowUrgency(bool state)
{
	if (IS_NET_SUPPORTED(_NET_WM_STATE_DEMANDS_ATTENTION))
	{
		XEvent event;

		event.xclient.type = ClientMessage;
		event.xclient.display = m_display;
		event.xclient.window = m_handle;
		event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
		event.xclient.format = 32;
		event.xclient.data.l[0] = state ? 1 : 0;
		event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_DEMANDS_ATTENTION);
		event.xclient.data.l[2] = 0;
		event.xclient.data.l[3] = 0;
		event.xclient.data.l[4] = 0;

		X11Types::Window root = RootWindow(m_display, m_screen);
		XSendEvent(m_display, root, False, SubstructureNotifyMask|SubstructureRedirectMask, &event);
	}
	else
	{	
		XWMHints* hints = XGetWMHints(m_display, m_handle);
		if (hints)
		{
			// XGetWMHints returns NULL if no XA_WM_HINTS property was set on window w
			// and returns a pointer to an XWMHints structure if it succeeds. The NULL
			// pointer typically happens if we set the urgency very early in the startup
			// before the window manager has had a chance to modify the settings.
			if (state)
				hints->flags |= XUrgencyHint;
			else
				hints->flags &= ~XUrgencyHint;

			XSetWMHints(m_display, m_handle, hints);
			XFree(hints);
		}
	}
}


void X11Widget::SetWindowList(const unsigned long* list, UINT32 count)
{
	XChangeProperty(m_display, m_handle, ATOMIZE(OPERA_WINDOWS), XA_WINDOW, 32,
					PropModeReplace, (unsigned char *) list, count);
}



BOOL X11Widget::MoveResizeWindow(MoveResizeAction action)
{
	if (action == MOVE_RESIZE_NONE)
		return FALSE;
	if (!IS_NET_SUPPORTED(_NET_WM_MOVERESIZE))
		return FALSE;
	if (g_x11->GetWidgetList()->GetDraggedWidget())
		return FALSE;

	// Let the window manager receive mouse events from now on
	XUngrabPointer(m_display, CurrentTime);

	g_x11->GetWidgetList()->SetDraggedWidget(this);

	X11Types::Window window = GetWindowHandle();

	OpPoint pos;
	InputUtils::GetGlobalPointerPos(pos);

	int button = 1;
	if (InputUtils::IsOpButtonPressed(MOUSE_BUTTON_2, true))
		button = 3;
	else if (InputUtils::IsOpButtonPressed(MOUSE_BUTTON_3, true))
		button = 2;

	XEvent event;
	event.xclient.type = ClientMessage;
	event.xclient.message_type = ATOMIZE(_NET_WM_MOVERESIZE);
	event.xclient.display = m_display;
	event.xclient.window = window;
	event.xclient.format = 32;
	event.xclient.data.l[0] = pos.x;
	event.xclient.data.l[1] = pos.y;
	event.xclient.data.l[2] = action;
	event.xclient.data.l[3] = button;
	event.xclient.data.l[4] = 0;

	long mask = SubstructureRedirectMask|SubstructureNotifyMask;
	X11Types::Window root = RootWindow(m_display, m_screen);
	XSendEvent(m_display, root, False, mask, &event);

	// Save local mouse pos. Will be used when drag has completed
	PointFromGlobal(&pos.x, &pos.y);
	m_window_drag.origin = pos;
	m_window_drag.button = button;
	return TRUE;
}


BOOL X11Widget::StopMoveResizeWindow(BOOL inform_wm)
{
	if (!IS_NET_SUPPORTED(_NET_WM_MOVERESIZE))
		return FALSE;

	if (!g_x11->GetWidgetList()->GetDraggedWidget() || g_x11->GetWidgetList()->GetDraggedWidget() != this)
		return FALSE;
	g_x11->GetWidgetList()->SetDraggedWidget(0);

	ReleaseCapture();

	OpPoint pos;
	InputUtils::GetGlobalPointerPos(pos);

	if (inform_wm)
	{
		X11Types::Window window = GetWindowHandle();

		int button = m_window_drag.button;

		XEvent event;
		event.xclient.type = ClientMessage;
		event.xclient.message_type = ATOMIZE(_NET_WM_MOVERESIZE);
		event.xclient.display = m_display;
		event.xclient.window = window;
		event.xclient.format = 32;
		event.xclient.data.l[0] = pos.x;
		event.xclient.data.l[1] = pos.y;
		event.xclient.data.l[2] = 11; // _NET_WM_MOVERESIZE_CANCEL 
		event.xclient.data.l[3] = button;
		event.xclient.data.l[4] = 0;

		long mask = SubstructureRedirectMask|SubstructureNotifyMask;
		X11Types::Window root = RootWindow(m_display, m_screen);
		XSendEvent(m_display, root, False, mask, &event);
	}

	// Prevents input manager from eating right mouse clicks after dragging a window.
	g_input_manager->ResetInput();
	// Allow popup menus to be shown a correct place if opened before moving the mouse 
	g_input_manager->SetMousePosition(pos, 0);
	EmulateRelease(m_window_drag.origin);

	return TRUE;
}

// static
X11Widget *X11Widget::GetModalWidget()
{
	return g_x11->GetWidgetList()->GetModalWidget();
}

// static
X11Widget *X11Widget::GetGrabbedWidget()
{
	return g_x11->GetWidgetList()->GetGrabbedWidget();
}

// static
X11Widget *X11Widget::GetPopupWidget()
{
	X11Widget* widget = X11Widget::GetModalWidget();
	return widget && (widget->GetWindowFlags() & POPUP) ? widget : 0;
}

// static
DocumentDesktopWindow* X11Widget::GetDocumentDesktopWindow(MDE_View* v, int x, int y)
{
	while (v)
	{
		if (v->IsType("MDE_Widget"))
		{
			OpWindow* op_window = static_cast<MDE_Widget*>(v)->GetOpWindow();
			if (op_window)
			{
				DesktopWindow* desktop_window = static_cast<X11OpWindow*>(op_window)->GetDesktopWindow();
				if (desktop_window && desktop_window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
				{
					OpWidget* root_widget = desktop_window->GetWidgetContainer()->GetRoot();
					if (root_widget)
					{
						OpWidget* widget = root_widget->GetWidget(OpPoint(x,y), TRUE);
						if (widget && widget->GetType() == OpTypedObject::WIDGET_TYPE_BROWSERVIEW)
							return static_cast<DocumentDesktopWindow*>(desktop_window);
					}
					return NULL;
				}
			}
		}

		MDE_View* child = v->GetViewAt(x,y,false);
		if (child)
		{
			x -= child->m_rect.x;
			y -= child->m_rect.y;
			DocumentDesktopWindow* dw = GetDocumentDesktopWindow(child, x, y);
			if (dw)
			{
				return dw;
			}
		}
		v = v->m_next;
	}
	return NULL;
}

// static
DocumentDesktopWindow* X11Widget::GetDragSource()
{
	X11Widget* widget = g_x11->GetWidgetList()->GetCapturedWidget();
	if (widget && widget->m_drag_hotspot.IsValid())
		return X11Widget::GetDocumentDesktopWindow(widget->GetMdeScreen(), widget->m_drag_hotspot.GetX(), widget->m_drag_hotspot.GetY());
	return NULL;
}


void X11Widget::HandleCallBack(INTPTR ev)
{
	if (m_pending_resize)
		HandleResize();
}

void X11Widget::SetCursor(unsigned int cursor)
{
	if( m_active_cursor != cursor )
	{
		X11Types::Cursor hd = X11Cursor::Get(cursor);
		if (hd != None)
		{
			m_active_cursor = cursor;

			if (g_pcunix->GetIntegerPref(PrefsCollectionUnix::HideMouseCursor))
			{
				hd = X11Cursor::Get(CURSOR_NONE);
				if (hd == None)
					return;
			}

			XDefineCursor(m_display, m_handle, hd);
		}
	}
}

void X11Widget::SaveCursor()
{
	m_saved_cursor = m_active_cursor;
}

void X11Widget::RestoreCursor()
{
	SetCursor(m_saved_cursor);
	m_saved_cursor = CURSOR_NUM_CURSORS;
}

void X11Widget::UpdateCursor()
{
	unsigned int cursor = m_active_cursor;
	m_active_cursor = X11Cursor::X11_CURSOR_MAX;
	SetCursor(cursor);
}


void X11Widget::CompressEvents(XEvent*& e, int type)
{
	static XEvent more;
	while (XCheckTypedWindowEvent(m_display, m_handle, type, &more))
		e = &more;
}


void X11Widget::SetToplevelRect(const OpRect& rect)
{
	if (m_toplevel)
	{
		if (m_visible)
		{
			if (m_mapped_rect.width != rect.width || m_mapped_rect.height != rect.height)
			{
				m_mapped_rect.width  = rect.width;
				m_mapped_rect.height = rect.height;
				if (m_window_state == NORMAL)
				{
					m_toplevel->normal_width  = m_mapped_rect.width;
					m_toplevel->normal_height = m_mapped_rect.height;
				}
				HandleResize();
			}

			m_mapped_rect.x = rect.x - m_toplevel->decoration.Left();
			m_mapped_rect.y = rect.y - m_toplevel->decoration.Top();
			
			if (m_window_state == NORMAL)
			{
				m_toplevel->normal_x = m_mapped_rect.x;
				m_toplevel->normal_y = m_mapped_rect.y;
			}
		}
		else
		{
			m_unmapped_rect = rect;
		}
	}
}


OpView* X11Widget::GetOpView(int ix, int iy, int& ox, int& oy)
{
	if(m_mde_screen)
	{
		MDE_View* mde_view = m_mde_screen->GetViewAt(ix, iy, true);
		if( mde_view && mde_view->IsType("MDE_Widget") )
		{
			ox = ix;
			oy = iy;
			mde_view->ConvertFromScreen(ox, oy);
			return ((MDE_Widget*)mde_view)->GetOpView();
		}
	}
	return 0;
}


int X11Widget::GetImageBPP()
{
	X11Types::Visual * v = GetVisual();
	if (v == g_x11->GetX11ARGBVisual(m_screen).visual)
		return g_x11->GetX11ARGBVisual(m_screen).image_bpp;
	if (v == g_x11->GetGLARGBVisual(m_screen).visual)
		return g_x11->GetGLARGBVisual(m_screen).image_bpp;
	if (v == g_x11->GetGLVisual(m_screen).visual)
		return g_x11->GetGLVisual(m_screen).image_bpp;
	OP_ASSERT(v == g_x11->GetX11Visual(m_screen).visual);
	return g_x11->GetX11Visual(m_screen).image_bpp;
}

void X11Widget::GetToplevelRect(OpRect & rect)
{
	OP_ASSERT( m_toplevel && "this query makes sense only for toplevel windows" );
	if (m_toplevel)
	{
		X11Types::Window root, child;
		int x, y, ret_x, ret_y;
		unsigned int width, height, border_width, depth;
		XGetGeometry(m_display, m_handle,
				&root, &x, &y, &width, &height, &border_width, &depth);
		XTranslateCoordinates(m_display, m_handle, root, x, y, 
				&ret_x, &ret_y, &child);
		rect.x = ret_x - x;
		rect.y = ret_y - y;
		rect.width  = width;
		rect.height = height;
	}
}


void X11Widget::GetDecorationOffset(int* x, int* y)
{
	if (m_toplevel)
	{
		if (m_toplevel->decoration_window != None)
			*x = *y = 0;
		else
		{
			*x = m_toplevel->decoration.Left();
			*y = m_toplevel->decoration.Top();
		}
	}
	else
		*x = *y = 0;
}


void X11Widget::SetDecorationType(DecorationType decoration_type, bool verify)
{
	if (m_toplevel && (m_toplevel->decoration_type != decoration_type || !m_has_set_decoration))
	{
		if (verify)
			decoration_type = VerifyDecorationType(decoration_type);

		if (m_toplevel->decoration_type != decoration_type || !m_has_set_decoration)
		{
			m_toplevel->decoration_type = decoration_type;
			m_has_set_decoration = true;

			switch (m_toplevel->decoration_type)
			{
			case DECORATION_SKINNED:
				ShowWMDecoration(false);
				ShowSkinnedDecoration(true);
				break;
			case DECORATION_WM:
				ShowSkinnedDecoration(false);
				ShowWMDecoration(true);
				break;
			}
			UpdateShape();
			OnDecorationChanged();
		}
	}
}


void X11Widget::ShowWMDecoration(bool show)
{
	if (m_toplevel)
	{
		MotifWindowManagerHints mwm_hints;
		mwm_hints.SetDecoration(show);
		mwm_hints.Apply(m_display, m_handle);
	}
}

void X11Widget::UpdateShape()
{
	if (m_toplevel && (m_window_flags & PERSONA))
	{
		// Ideally we only want to set the shape when m_window_state is NORMAL, but
		// KDE (kwin) sets a shape on its toplevel window when we set it on our window
		// and fail to remove it when we remove ours. That will break going to maximized mode
		// and fullscreen. Observed on KDE 4.4.5
		SetShape(m_visible && m_toplevel->decoration_type == DECORATION_SKINNED && m_window_state != ICONIFIED);
	}
}

void X11Widget::GetContentMargins(int &top, int &bottom, int &left, int &right)
{
	top = 0;
	bottom = 0;
	left = 0;
	right = 0;
}

void X11Widget::SetShape(bool set)
{
	if (m_toplevel)
	{
		if (set && GetWidth() > 10 && GetHeight() > 10)
		{
			INT32  p      = (GetWidth()-1)%8 + 1;   // Same as p%8, except that p is set to 8 instead of 0
			UINT32 bpl    = (GetWidth() + 8 - p)/8; // Bytes per line
			UINT32 height = GetHeight();
			UINT32 size   = bpl * height;
			UINT8* buffer = OP_NEWA(UINT8, size);
			if (buffer)
			{
				op_memset(buffer, 0xFF, size);

				if (m_window_state == NORMAL)
				{
					// Runded corners
					// Up left
					buffer[0] = 0xF8;
					buffer[bpl] = 0xFE;
					buffer[bpl*2] = 0xFE;
					// Bottom left
					buffer[(bpl*(height-3))] = 0xFE;
					buffer[(bpl*(height-2))] = 0xFE;
					buffer[(bpl*(height-1))] = 0xF8;
					// Upper right
					unsigned short flag = 0x1FFF;
					buffer[bpl-2] = (flag >> (8-p));
					buffer[bpl-1] = (flag >> (16-p));
					flag = 0x7FFF;
					buffer[bpl*2-2] = (flag >> (8-p));
					buffer[bpl*2-1] = (flag >> (16-p));
					buffer[bpl*3-2] = (flag >> (8-p));
					buffer[bpl*3-1] = (flag >> (16-p));
					// Lower right
					flag = 0x7FFF;
					buffer[bpl*(height-2)-2] = (flag >> (8-p));
					buffer[bpl*(height-2)-1] = (flag >> (16-p));
					buffer[bpl*(height-1)-2] = (flag >> (8-p));
					buffer[bpl*(height-1)-1] = (flag >> (16-p));
					flag = 0x1FFF;
					buffer[bpl*(height-0)-2] = (flag >> (8-p));
					buffer[bpl*(height-0)-1] = (flag >> (16-p));
				}

				X11Types::Pixmap pixmap = XCreatePixmapFromBitmapData(
					m_display, m_handle, reinterpret_cast<char*>(buffer), bpl*8, height,
					WhitePixel(m_display,m_screen), BlackPixel(m_display,m_screen), 1);
				XShapeCombineMask(m_display, m_handle, ShapeBounding, 0, 0, pixmap, ShapeSet);
				XFreePixmap(m_display, pixmap);
				OP_DELETEA(buffer);
			}
		}
		else
		{
			XShapeCombineMask(m_display, m_handle, ShapeBounding, 0, 0, None, ShapeSet);
		}
	}
}


#ifdef _DEBUG
# undef DEBUG_WM_STATE
#endif

