/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_globals.h"

#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_cursor.h"
#include "platforms/unix/base/x11/x11_inputmethod.h"
#include "platforms/unix/base/x11/x11_settingsclient.h"
#include "platforms/unix/base/x11/x11_tc16_colormanager.h"
#include "platforms/unix/base/x11/x11_tc32_colormanager.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/quix/environment/DesktopEnvironment.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/utilix/x11_all.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "modules/pi/OpDLL.h"
#include "modules/skin/OpSkinManager.h"

#include <sys/utsname.h>

X11Globals *g_x11 = 0;

using X11Types::Atom;

// Error handlers
namespace X11ErrorHandlers
{
	int ErrorHandler(X11Types::Display *display, XErrorEvent *ev)
	{
		if (g_startup_settings.debug_x11_error)
		{
			char text[201];
			XGetErrorText(display, ev->error_code, text, 200);
			fprintf(stderr, 
					"X request error - %s:\n"
					"\tMajor opcode: %d, Minor opcode: %d\n"
					"\tResource ID: %#x\n\n",
					text, ev->request_code, ev->minor_code, (unsigned int)ev->resourceid);
		}

		switch (ev->error_code)
		{
			case BadWindow:
				g_x11->SetX11ErrBadWindow(true);
				break;
		}

		return 0;
	}
};

//static 
OP_STATUS X11Globals::Create()
{
	OP_STATUS rc = OpStatus::OK;
	if (!g_x11)
	{
		g_x11 = OP_NEW(X11Globals,());
		if (!g_x11)
			return OpStatus::ERR_NO_MEMORY;
		rc = g_x11->Init();
		if (OpStatus::IsError(rc))
		{
			OP_DELETE(g_x11);
			g_x11=0;
		}
	}	
	return rc;
}

//static 
void X11Globals::Destroy()
{
	OP_DELETE(g_x11);
	g_x11 = 0;
}


X11Globals::X11Globals()
	:m_display(0)
	,m_xim_data(0)
	,m_screens(0)
	,m_default_screen(0)
	,m_screen_count(0)
	,m_forced_dpi(0)
	,m_xft_dpi(0)
	,m_xrandr_major(0)
	,m_xrandr_event(0)
	,m_xrandr_error(0)
	,m_app_window(None)
	,m_image_swap_endianness(false)
	,m_support_xsync(false)
	,m_x11err_bad_window(false)
	,m_widget_list(0)
{
}


X11Globals::~X11Globals()
{
	OP_DELETE(m_xim_data);
	OP_DELETE(m_widget_list);

	X11SettingsClient::Destroy();
	X11Cursor::Destroy();

	if (m_display)
	{
		if (m_screens)
		{
			for(int i=0; i<m_screen_count; i++)
			{
				m_screens[i].m_visual.FreeColormap(m_display);
				m_screens[i].m_argb_visual.FreeColormap(m_display);
			}
		}
		if (m_app_window != None)
			XDestroyWindow(m_display, m_app_window);
		XCloseDisplay(m_display);
	}

	OP_DELETEA(m_screens);
}



OP_STATUS X11Globals::Init()
{
	m_display = XOpenDisplay(g_startup_settings.display_name.CStr());
	if (!m_display)
	{
		fprintf(stderr, "opera: cannot connect to X server %s. Error: ", XDisplayName(g_startup_settings.display_name.CStr()));
		perror("");
		return OpStatus::ERR;
	}
	if (g_startup_settings.sync_event)
		XSynchronize(m_display, True);

	XSetErrorHandler(X11ErrorHandlers::ErrorHandler);

	m_default_screen = DefaultScreen(m_display);

	m_widget_list = OP_NEW(X11WidgetList, ());
	if (!m_widget_list)
		return OpStatus::ERR_NO_MEMORY;

	m_xim_data = OP_NEW(X11InputMethod, (m_display));
	if (!m_xim_data)
		return OpStatus::ERR_NO_MEMORY;

	m_screen_count = ScreenCount(m_display);
	if (m_default_screen < 0 || m_default_screen >= m_screen_count)
		return OpStatus::ERR;

	RETURN_IF_ERROR(InitScreens());
	// Depends on InitScreens()
	RETURN_IF_ERROR(InitVisuals());
	RETURN_IF_ERROR(InitColorManagers());
	RETURN_IF_ERROR(InitXSync());
	RETURN_IF_ERROR(InitXRender());
	// Depends on XRender
	RETURN_IF_ERROR(InitARGBVisuals()); 
	RETURN_IF_ERROR(InitFontSettings());
	// Depends on InitVisuals() and InitARGBVisuals(); 
	RETURN_IF_ERROR(UpdateImageParameters());
	RETURN_IF_ERROR(InitApplicationWindow());

	RETURN_IF_ERROR(X11SettingsClient::Create());
	// Connects to X11SettingsClient
	RETURN_IF_ERROR(X11Cursor::Create());

	RETURN_IF_ERROR(m_startup_id.Set(getenv("DESKTOP_STARTUP_ID")));
	unsetenv("DESKTOP_STARTUP_ID"); // Per spec

	return OpStatus::OK;
}


OP_STATUS X11Globals::InitApplicationWindow()
{
	X11Visual& v = GetX11Visual(m_default_screen);

	XSetWindowAttributes attrs;
	attrs.colormap = v.colormap;
	attrs.border_pixel = BlackPixel(m_display, m_default_screen);
	unsigned int mask = CWColormap | CWBorderPixel;

	m_app_window = XCreateWindow(m_display, RootWindow(m_display, m_default_screen), 0, 0,
								 GetScreenWidth(m_default_screen), GetScreenHeight(m_default_screen), 
								 0, v.depth, InputOutput, v.visual, mask, &attrs);

	XChangeProperty(m_display, m_app_window, ATOMIZE(WM_CLIENT_LEADER),
					XA_WINDOW, 32, PropModeReplace, (unsigned char *)&m_app_window, 1);

	return OpStatus::OK;
}


OP_STATUS X11Globals::InitScreens()
{
	BOOL first_init = m_screens == 0;

	// InitScreens() will be called when we receive XRandR reconfiguration events
	if (!m_screens)
	{
		m_screens = OP_NEWA(X11ScreenData, m_screen_count);
		if (!m_screens)
			return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		for (int s=0; s<m_screen_count; s++)
			m_screens[s].m_rect.DeleteAll();
	}

	if (!m_xrr.get())
	{
		if (XQueryExtension(m_display, "RANDR", &m_xrandr_major, &m_xrandr_event, &m_xrandr_error))
		{
			OpAutoPtr<XrandrLoader> loader(OP_NEW(XrandrLoader,()));
			if (loader.get() && OpStatus::IsSuccess(loader->Init(m_display)))
				m_xrr = loader;
		}
	}

	if (m_xrr.get())
	{
		for (int s=0; s<m_screen_count; s++)
		{
			// XRRSelectInput must only be used on program init, not when InitScreens() is called because of
			// XRandR reconfiguration events as that will trigger new events and an endless loop (DSK-346119)
			if (first_init)
				m_xrr->XRRSelectInput(m_display, RootWindow(m_display, s), RRScreenChangeNotifyMask|RRCrtcChangeNotifyMask|RROutputPropertyNotifyMask);

			XRRScreenResources* resources = m_xrr->XRRGetScreenResourcesCurrent(m_display, RootWindow(m_display, s));
			if (resources)
			{
				for (int i=0; i<resources->noutput; i++)
				{
					XRROutputInfo* output = m_xrr->XRRGetOutputInfo (m_display, resources, resources->outputs[i]);
					if (output)
					{
						if (output->connection == RR_Disconnected)
						{
							m_xrr->XRRFreeOutputInfo (output);
							continue;
						}
						if (output->crtc)
						{
							XRRCrtcInfo* crtc = m_xrr->XRRGetCrtcInfo (m_display, resources, output->crtc);
							if (crtc)
							{
								OpRect* rect = OP_NEW(OpRect,(crtc->x, crtc->y,crtc->width, crtc->height));
								if (!rect || OpStatus::IsError(m_screens[s].m_rect.Add(rect)))
								{
									OP_DELETE(rect);
									m_xrr->XRRFreeCrtcInfo (crtc);
									m_xrr->XRRFreeOutputInfo (output);
									m_xrr->XRRFreeScreenResources(resources);
									return OpStatus::ERR_NO_MEMORY;
								}
								if (g_startup_settings.debug_libraries)
									fprintf(stderr, "opera: XRandR reports monitor area [%d %d %d %d] on screen %d\n", crtc->x, crtc->y,crtc->width, crtc->height, s);
								m_xrr->XRRFreeCrtcInfo (crtc);
							}
						}
						m_xrr->XRRFreeOutputInfo (output);
					}
				}
				m_xrr->XRRFreeScreenResources(resources);
			}
		}
	}

	// Try Xinerama if we have not already found multiple physical screens using XRandR
	// Xinerama API seems only to work properly if we have one logical screen
	if (m_screen_count == 1 && m_screens[0].m_rect.GetCount() < 2)
	{
		if (!m_xin.get())
		{
			int ignore;
			if (XQueryExtension(m_display, "XINERAMA", &ignore, &ignore, &ignore))
			{
				OpAutoPtr<XineramaLoader> loader(OP_NEW(XineramaLoader,()));
				if (loader.get() && OpStatus::IsSuccess(loader->Init(m_display)))
					m_xin = loader;
			}
		}

		if (m_xin.get())
		{
			int xinerama_screen_count;
			XineramaScreenInfo* info = m_xin->XINXineramaQueryScreens(m_display, &xinerama_screen_count);
			if (info)
			{
				m_screens[0].m_rect.DeleteAll();
				for (int i=0; i<xinerama_screen_count; i++)
				{
					OpRect* rect = OP_NEW(OpRect,(info[i].x_org, info[i].y_org, info[i].width, info[i].height));
					if (!rect || OpStatus::IsError(m_screens[0].m_rect.Add(rect)))
					{
						OP_DELETE(rect);
						XFree(info);
						return OpStatus::ERR_NO_MEMORY;
					}
					if (g_startup_settings.debug_libraries)
						fprintf(stderr, "opera: Xinerama reports monitor area [%d %d %d %d] on screen %d\n", info[i].x_org, info[i].y_org, info[i].width, info[i].height, 0);
				}
				XFree(info);
			}

		}
	}

	for (int i=0; i<m_screen_count; i++)
	{
		X11ScreenData& s = m_screens[i];
		s.m_number = i;
		s.m_width  = DisplayWidth(m_display, i);
		s.m_height = DisplayHeight(m_display, i);

		int width_mm  = DisplayWidthMM(m_display, i);
		int height_mm = DisplayHeightMM(m_display, i);
		if (width_mm == 0 || height_mm == 0)
			s.m_dpi_x = s.m_dpi_y = 96;
		else
		{
			s.m_dpi_x = (s.m_width * 254 + width_mm*5) / (width_mm*10);
			s.m_dpi_y = (s.m_height * 254 + height_mm*5)/ (height_mm*10);
		}

		if (first_init)
			s.m_xrender_supported = FALSE; // Set in InitXRender()
	}
	return OpStatus::OK;
}


OP_STATUS X11Globals::InitFontSettings()
{
	// Xft.dpi
	char* database_string = XResourceManagerString(m_display);
	if (!database_string)
		return OpStatus::OK; // We survive without

	XrmInitialize();
	XrmDatabase database = XrmGetStringDatabase(database_string);

	char* type = 0;
	XrmValue value;
	if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &type, &value) &&
		type && !op_strcmp(type, "String"))
	{
		m_xft_dpi = op_atoi(value.addr);
	}

	XrmDestroyDatabase(database);

	return OpStatus::OK;
}

static OP_STATUS update_image_params(X11Types::Display * dpy, X11Visual & v, int * ret_byte_order=0)
{
	if (v.visual == 0)
		return OpStatus::OK;
	XImage* img = XCreateImage(dpy, v.visual, v.depth, ZPixmap, 0, 0, 1, 1, 8, 0);
	if (!img)
		return OpStatus::ERR_NO_MEMORY;

	v.image_bpp = img->bits_per_pixel;
	if (ret_byte_order)
		*ret_byte_order = img->byte_order;
	XDestroyImage(img);
	return OpStatus::OK;
}

OP_STATUS X11Globals::UpdateImageParameters()
{
	for (int i=0; i<m_screen_count; i++)
	{
		int byte_order = LSBFirst;
		RETURN_IF_ERROR(update_image_params(m_display, GetX11Visual(i), &byte_order));

		if (i==0)
		{
#ifdef OPERA_BIG_ENDIAN
			m_image_swap_endianness = byte_order == LSBFirst;
#else
			m_image_swap_endianness = byte_order == MSBFirst;
#endif
			OP_ASSERT((GetX11Visual(0).image_bpp & 7) == 0);
		}

		RETURN_IF_ERROR(update_image_params(m_display, GetX11ARGBVisual(i)));
		RETURN_IF_ERROR(update_image_params(m_display, GetGLVisual(i)));
		RETURN_IF_ERROR(update_image_params(m_display, GetGLARGBVisual(i)));
	}

	return OpStatus::OK;
}


OP_STATUS X11Globals::InitXSync()
{
	int event_base, error_base;
	if (XSyncQueryExtension(m_display, &event_base, &error_base))
	{
		// XSyncInitialize returns > 0 on success
		int major, minor;
		m_support_xsync = XSyncInitialize(m_display, &major, &minor) > 0;
		if (m_support_xsync)
		{
			// Make it possible to test and compare with and without support
			// By default we keep it off. General behavior seems best so [espen 2010-09-15]
			m_support_xsync = UnixUtils::HasEnvironmentFlag("OPERA_USE_XSYNC");
		}
	}

	return OpStatus::OK;
}


OP_STATUS X11Globals::InitXRender()
{
	int d1, d2, d3;
	if (XQueryExtension(m_display, "RENDER", &d1, &d2, &d3) == True)
	{
		for(int i=0; i<m_screen_count; i++) 
		{
			XRenderPictFormat* format = XRenderFindVisualFormat(m_display, GetX11Visual(i).visual);
			m_screens[i].m_xrender_supported = format != 0;
		}
	}

	return OpStatus::OK;
}


OP_STATUS X11Globals::InitVisuals()
{
	for (int i=0; i<m_screen_count; i++)
	{
		XVisualInfo request;
		request.screen = i;
		int count;

		XVisualInfo* info = XGetVisualInfo(m_display, VisualScreenMask, &request, &count);
		if (info)
		{
			X11ScreenData& s = m_screens[i];

			if (!s.m_visual.visual)
			{
				s.m_visual.visual = DefaultVisual(m_display, i);
			}

			for (int j=0; j<count; j++)
			{
				if (s.m_visual.visual == info[j].visual)
				{
					s.m_visual.id     = info[j].visualid;
					s.m_visual.depth  = info[j].depth;
					s.m_visual.r_mask = info[j].red_mask;
					s.m_visual.g_mask = info[j].green_mask;
					s.m_visual.b_mask = info[j].blue_mask;
					break;
				}
			}

			XFree(info);
		}
	}

	return OpStatus::OK;
}


OP_STATUS X11Globals::InitARGBVisuals()
{
	if (!g_startup_settings.no_argb)
	{
		for (int i=0; i<m_screen_count; i++) 
		{
			if (m_screens[i].m_xrender_supported)
			{
				XVisualInfo request;
				request.screen  = i;
				request.depth   = 32;
				request.c_class = TrueColor;

				int num_visual;
				XVisualInfo* info = XGetVisualInfo(m_display, VisualScreenMask|VisualDepthMask|VisualClassMask, &request, &num_visual);
				if (info)
				{
					for (int j=0; j<num_visual; j++) 
					{
						XRenderPictFormat* format = XRenderFindVisualFormat(m_display, info[j].visual);
						if( format && format->type == PictTypeDirect && format->direct.alphaMask) 
						{
							X11Visual& v = GetX11ARGBVisual(i);
							v.visual = info[j].visual;
							v.id     = info[j].visualid;
							v.colormap = XCreateColormap(m_display, RootWindow(m_display, i), v.visual, AllocNone);
							v.owns_colormap = true;
							v.depth  = info[j].depth;
							v.r_mask = info[j].red_mask;
							v.g_mask = info[j].green_mask;
							v.b_mask = info[j].blue_mask;
							break;
						}
					}
					XFree(info);
				}
			}
		}
	}
	return OpStatus::OK;
}


BOOL X11Globals::SetGLVisual(int screen, bool with_alpha, XVisualInfo * vi, GLX::FBConfig fbconfig)
{
	X11Types::Colormap cm = XCreateColormap(m_display, RootWindow(m_display, screen), vi->visual, AllocNone);
	if (!cm)
		return FALSE;

	X11Visual * visptr;
	if (with_alpha)
		visptr = &g_x11->GetGLARGBVisual(screen);
	else
		visptr = &g_x11->GetGLVisual(screen);
	X11Visual &x11vis = *visptr;
	OP_ASSERT(x11vis.visual == 0); // Replacing visuals on the fly is probably not supported
	x11vis.visual = vi->visual;
	x11vis.id = vi->visualid;
	x11vis.FreeColormap(m_display);
	x11vis.owns_colormap = true;
	x11vis.colormap = cm;
	x11vis.fbconfig = fbconfig;
	x11vis.depth = vi->depth;
	x11vis.r_mask = vi->red_mask;
	x11vis.g_mask = vi->green_mask;
	x11vis.b_mask = vi->blue_mask;

	return OpStatus::IsSuccess(update_image_params(m_display, x11vis));
}


OP_STATUS X11Globals::InitColorManagers()
{
	for(int i=0; i<m_screen_count; i++) 
	{
		X11ScreenData& s = m_screens[i];
		if( s.m_visual.colormap == None )
		{
			if( s.m_visual.visual != DefaultVisual(m_display, i) )
			{
				int count;
				XStandardColormap *stdmaps;

				if (XGetRGBColormaps(m_display, RootWindow(m_display, i), &stdmaps, &count, XA_RGB_DEFAULT_MAP))
				{
					for (int i=0; i<count; i++)
					{
						if (stdmaps[i].visualid == s.m_visual.id)
						{
							s.m_visual.colormap = stdmaps[i].colormap;
							s.m_visual.owns_colormap = false;
							break;
						}
					}
					XFree(stdmaps);
				}

				if (s.m_visual.colormap == None)
				{
					s.m_visual.colormap = XCreateColormap(m_display, RootWindow(m_display, i), s.m_visual.visual, AllocNone);
					if (s.m_visual.colormap == None)
						return OpStatus::ERR;
					s.m_visual.owns_colormap = true;
				}
			}
			else
			{
				s.m_visual.colormap = DefaultColormap(m_display, i);
				s.m_visual.owns_colormap = false;
			}
		}

		XVisualInfo vistempl;
		int count;
		vistempl.screen = i;
		XVisualInfo* visual_info_list = XGetVisualInfo(m_display, VisualScreenMask, &vistempl, &count);
		if (!visual_info_list)
			return OpStatus::ERR;


		XVisualInfo* vis_found = 0;
		for (int j=0; j<count; j++)
		{
			vis_found = &visual_info_list[j];
			if (vis_found->visual == s.m_visual.visual)
				break;
		}

		XFree(visual_info_list);

		OP_ASSERT(vis_found);
		if (!vis_found)
			return OpStatus::ERR;

	}

	return OpStatus::OK;
}




BOOL X11Globals::HandleEvent(XEvent* event)
{
	if (m_xrr.get())
	{
		if ((event->type - m_xrandr_event) == RRScreenChangeNotify ||
			(event->type - m_xrandr_event) == RRNotify)
		{
			m_xrr->XRRUpdateConfiguration(event);

			// Update screen rectangles (each rectangle is a physical monitor). We can only do this if number of screens
			// remain the same (anything else is quite unlikely)
			int screen_count = ScreenCount(m_display);
			if (screen_count == m_screen_count)
			{
				InitScreens();
			}
			return TRUE;
		}
	}

	return FALSE;
}



UINT32 X11Globals::GetMaxSelectionSize(X11Types::Display* display)
{
	// XExtendedMaxRequestSize will return 0 if BIG-REQUESTS extension is not supported
	long size_in_int32 = XExtendedMaxRequestSize(display);
	if (size_in_int32 <= 0)
	{
		size_in_int32 = XMaxRequestSize(display);
	}

	// We have tried subtracting 24 (as is done in Qt) but that was not enough.
	// Subtracting 100 should be way enough. The values are so large that it does
	// not matter if we waste some bytes.
	long size_in_bytes = (size_in_int32 * 4) - 100;

	return size_in_bytes > 0 ? size_in_bytes : 0;
}


BOOL X11Globals::IsCompositingManagerActive(UINT32 screen)
{
	// NET WM Spec: _NET_WM_CM_S0 on screen 0, _NET_WM_CM_S1 on screen 1 etc
	// xcompmgr [for ubuntu 8.10] does not support screen > 0 so we will get 
	// an error if on screen 1 or higher even if it is running.

	char buf[100];
	sprintf(buf, "_NET_WM_CM_S%d", screen);

	X11Types::Atom net_wm_state = XInternAtom(m_display, buf, False);
	X11Types::Window window_owner = XGetSelectionOwner(m_display, net_wm_state);

	return window_owner != None;
}


BOOL X11Globals::HasPersonaSkin() const
{
	return g_skin_manager->HasPersonaSkin();
}


OpRect X11Globals::GetWorkRect(UINT32 screen, const OpPoint& point) const
{
	for (UINT32 i=0; i<m_screens[screen].m_rect.GetCount(); i++)
	{
		if(m_screens[screen].m_rect.Get(i)->Contains(point))
			return *m_screens[screen].m_rect.Get(i);
	}

	OpRect r(0, 0, GetScreenWidth(screen), GetScreenHeight(screen));
	if (r.Contains(point))
		return r;

	return OpRect();
}


BOOL X11Globals::GetWorkSpaceRect(UINT32 screen, OpRect& rect)
{
	X11Types::Atom rettype;
	int format;
	unsigned long nitems;
	unsigned long bytes_remaining;
	unsigned char *data = 0;

	X11Types::Window root_window = RootWindow(m_display, screen);
	BOOL ok = FALSE;

	int current_desktop = 0;
	GetCurrentDesktop(screen, current_desktop);

	int rc = XGetWindowProperty(m_display, root_window, ATOMIZE(_NET_WORKAREA), current_desktop*4, 4,
							False, XA_CARDINAL, &rettype, &format, &nitems, &bytes_remaining, &data);
	if (rc == Success && rettype == XA_CARDINAL && format == 32 && nitems == 4)
	{
		const long* r = reinterpret_cast<const long *>(data);
		rect.x      = r[0];
		rect.y      = r[1];
		rect.width  = r[2];
		rect.height = r[3];
		ok = TRUE;
	}
	if (data)
		XFree(data);

	return ok;
}


BOOL X11Globals::GetCurrentDesktop(int screen, int& current_desktop)
{
	int format;
	X11Types::Atom rettype;
	unsigned long nitems;
	unsigned long bytes_remaining;
	unsigned char* data = 0;

	BOOL ok = FALSE;

	X11Types::Window root_window = RootWindow(m_display, screen);
	int rc = XGetWindowProperty(m_display, root_window, ATOMIZE(_NET_CURRENT_DESKTOP), 0, 1,
								False, XA_CARDINAL, &rettype, &format, &nitems, &bytes_remaining, &data);
	if (rc == Success && rettype == XA_CARDINAL && format == 32 && nitems == 1)
	{
		ok = TRUE;
		current_desktop = *(long*)data;
	}
	if (data)
	{
		XFree(data);
		data = 0;
	}

	return ok;
}


X11InputContext* X11Globals::CreateIC(X11Widget * window)
{
	if (!m_xim_data)
		return 0;

	return m_xim_data->CreateIC(window);
}

int X11Globals::GetOverridableDpiX(UINT32 screen) const
{
	return m_forced_dpi ? m_forced_dpi : (m_xft_dpi ? m_xft_dpi : GetDpiX(screen));
}

int X11Globals::GetOverridableDpiY(UINT32 screen) const
{
	return m_forced_dpi ? m_forced_dpi : (m_xft_dpi ? m_xft_dpi : GetDpiY(screen));
}

OP_STATUS X11Globals::GetWindowManagerName(OpString8 &wm_name)
{
	Atom type;
	int format;
	unsigned long nitems, after;
	unsigned char *data = 0;
	unsigned char *name = 0;
	OP_STATUS status = OpStatus::ERR;

	X11Types::Window root = XDefaultRootWindow(m_display);
	int rc = XGetWindowProperty(
			m_display, root,
			ATOMIZE(_NET_SUPPORTING_WM_CHECK),
			0, 1, False, XA_WINDOW,
			&type, &format, &nitems, &after, &data);
	if (rc == Success && type == XA_WINDOW && format == 32 && nitems == 1)
	{
		X11Types::Window info_win = *reinterpret_cast<X11Types::Window*>(data);
  		rc = XGetWindowProperty(
				m_display, info_win,
				ATOMIZE(_NET_WM_NAME),
				0, 128, False, ATOMIZE(UTF8_STRING),
				&type, &format, &nitems, &after, &name);
		if (rc == Success && type == ATOMIZE(UTF8_STRING) && format == 8)
		{
			status = wm_name.Set(reinterpret_cast<const char*>(name));
			if (name)
				XFree(name);
		}
		if (data)
			XFree(data);
	}
	return status;
}

void X11Globals::SendStartupXMessage(const OpString8& message)
{
	X11Types::Window root = RootWindow(m_display, m_default_screen);

	// Spec says one should create a window (that does not have to be mapped) on the
	// default screen and send the message using its handle. We already have the hidden
	// application window on this screen and it seems to work just fine.
	// See broadcast_xmessage() in GTK for an example should we have to use a separate window

	XEvent xevent;
	xevent.xclient.type = ClientMessage;
	xevent.xclient.message_type = ATOMIZE(_NET_STARTUP_INFO_BEGIN);
	xevent.xclient.display = m_display;
	xevent.xclient.window = m_app_window;
	xevent.xclient.format = 8;

	int sent = 0;
	int length = message.Length() + 1; // Spec: Must include terminating 0
	do
	{
		if (sent == 20)
			xevent.xclient.message_type = ATOMIZE(_NET_STARTUP_INFO);

		// Buffer can hold no more than 20 bytes
		for (int i = 0; i < 20 && sent < length; i++)
		{
			xevent.xclient.data.b[i] = message[sent++];
		}
		XSendEvent(m_display, root, False, PropertyChangeMask, &xevent);
	}
	while (sent < length);

	XFlush (m_display);
}


OP_STATUS X11Globals::NotifyStartupBegin(UINT32 screen)
{

	if (m_startup_id.HasContent())
		return OpStatus::OK; // A startup sequence is already in progress

	OpString8 hostname;
	if (!hostname.Reserve(101))
		return OpStatus::ERR_NO_MEMORY;
	gethostname(hostname.CStr(), 100);
	hostname.CStr()[100]=0;

	const char* program_name = "opera";
	const char* binary_name  = g_startup_settings.our_binary.CStr();
	if (!binary_name)
		binary_name = program_name;

	pid_t pid = getpid();

	// This ID is made up to be unique to the system
	RETURN_IF_ERROR(m_startup_id.AppendFormat("%s-%lu-%s-%s_TIME%lu", program_name, pid, hostname.CStr(), binary_name, 0));

	OpString8 escaped_id;
	RETURN_IF_ERROR(EscapeStartupNotificationString(m_startup_id, escaped_id));

	// See http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt
	// for supported KEY/VALUE pairs
	OpString8 message;
	OP_STATUS rc = message.Set("new: ");
	if (OpStatus::IsSuccess(rc))
		rc = message.AppendFormat("ID=\"%s\" ", escaped_id.CStr());
	if (OpStatus::IsSuccess(rc))
		rc = message.AppendFormat("NAME=\"%s\" ", m_wmclass.name.CStr());
	if (OpStatus::IsSuccess(rc))
		rc = message.AppendFormat("SCREEN=%d ", screen);
	if (OpStatus::IsSuccess(rc) && m_wmclass.icon.HasContent())
		rc = message.AppendFormat("ICON=%s ", m_wmclass.icon.CStr());

	if (OpStatus::IsError(rc))
	{
		m_startup_id.Empty(); // Prevent NotifyStartupFinish() if we can not send the begin message
		return rc;
	}

	SendStartupXMessage(message);
	return OpStatus::OK;
}


OP_STATUS X11Globals::NotifyStartupFinish()
{
	if (m_startup_id.HasContent())
	{
		OpString8 escaped_id;
		RETURN_IF_ERROR(EscapeStartupNotificationString(m_startup_id, escaped_id));

		OpString8 message;
		RETURN_IF_ERROR(message.AppendFormat("remove: ID=\"%s\"", escaped_id.CStr()));
		SendStartupXMessage(message);

		m_startup_id.Empty(); // This function can only be run once
	}
	return OpStatus::OK;
}


OP_STATUS X11Globals::EscapeStartupNotificationString(OpStringC8 source, OpString8& dest)
{
	RETURN_IF_ERROR(dest.Set(source));
	RETURN_IF_ERROR(dest.ReplaceAll("\\", "\\\\"));
	RETURN_IF_ERROR(dest.ReplaceAll("\"", "\\\""));
	return OpStatus::OK;
}


// static
void X11Globals::ShowOperaVersion(BOOL full_version, BOOL show_in_dialog, DesktopWindow* parent)
{
	OpString8 str;

	str.Append("Opera " BROWSER_FULL_VERSION
#ifdef BROWSER_SUBVER_LONG
			" " BROWSER_SUBVER_LONG "."
#endif
	);

	str.AppendFormat(" Build %d"
#if defined(__linux__)
			" for Linux"
#elif defined(__FreeBSD__)
			" for FreeBSD"
#endif
#if defined(__x86_64__)
			" x86_64"
#elif defined(__i386__)
			" i386"
#endif
			".\n", BROWSER_BUILD_NUMBER_INT);

	if (full_version)
	{
		struct utsname utsName;
		if (uname(&utsName) < 0)
			str.Append("OS: Unidentified Unix\n");
		else
		{
			str.AppendFormat("OS: %s %s\n", utsName.sysname, utsName.release);
			str.AppendFormat("Architecture: %s\n", utsName.machine);
		}

		BOOL compositor = g_x11->IsCompositingManagerActive(g_x11->GetDefaultScreen()) && !g_startup_settings.no_argb;
		str.AppendFormat("Compositor active: %s\n", compositor ? "Yes" : "No");

		OpString8 toolkit_name;
		const char* toolkit = g_toolkit_library ? g_toolkit_library->ToolkitInformation() : 0;
		toolkit_name.Set(toolkit);
		str.AppendFormat("Toolkit: %s\n", toolkit ? toolkit_name.CStr() : "X11");

		OpString8 de_name;
		DesktopEnvironment::GetInstance().GetDesktopEnvironmentName(de_name);
		str.AppendFormat("Desktop environment: %s\n", de_name.CStr());

		OpString8 wm_name;
		BOOL wm_has_name = OpStatus::IsSuccess(g_x11->GetWindowManagerName(wm_name)) && wm_name.Strip().HasContent();
		str.AppendFormat("Window manager: %s\n", wm_has_name ? wm_name.CStr() : "Unknown");

		str.AppendFormat("Screens:\n", g_x11->GetScreenCount());
		for(int i=0; i < g_x11->GetScreenCount(); i++)
		{
			BOOL def = g_x11->GetDefaultScreen() == i;
			X11Visual v =  g_x11->GetX11Visual(i);
			X11Visual va = g_x11->GetX11ARGBVisual(i);
			X11Visual vgl = g_x11->GetGLVisual(i);
			X11Visual vgla = g_x11->GetGLARGBVisual(i);

			str.AppendFormat("%d: %dx%d", i,
					g_x11->GetScreenWidth(i),
					g_x11->GetScreenHeight(i));
			str.AppendFormat(" depth %d", v.depth);
			if (va.visual)
				str.AppendFormat(",%d", va.depth);
			if (vgl.visual)
				str.AppendFormat(",%d", vgl.depth);
			if (vgla.visual)
				str.AppendFormat(",%d", vgla.depth);
			str.AppendFormat("%s\n", def ? " (default)" : "");
		}
	}

	printf("%s", str.CStr());

	if (show_in_dialog)
	{
		OpString msg;
		msg.Set(str);
		SimpleDialog::ShowDialog(WINDOW_NAME_VERSION_INFO, parent, msg.CStr(), UNI_L("Opera"), Dialog::TYPE_OK, Dialog::IMAGE_INFO);
	}
}

OP_STATUS X11Globals::UnloadOnShutdown(OpDLL *dll)
{
    return m_libraries.Insert(0, dll);
}

XrandrLoader::XrandrLoader()
	: XRRQueryExtension(0)
	, XRRQueryVersion(0)
	, XRRSelectInput(0)
	, XRRGetScreenResourcesCurrent(0)
	, XRRGetOutputPrimary(0)
	, XRRGetOutputInfo(0)
	, XRRGetCrtcInfo(0)
	, XRRFreeScreenResources(0)
	, XRRFreeOutputInfo(0)
	, XRRFreeCrtcInfo(0)
	, XRRUpdateConfiguration(0)
	, m_dll(0)
{
}

OP_STATUS XrandrLoader::Init(X11Types::Display* display)
{
	if (m_dll)
		return OpStatus::OK;

	RETURN_IF_ERROR(OpDLL::Create(&m_dll));

	const uni_char* library_name = UNI_L("libXrandr.so.2");
	OP_STATUS rc = m_dll->Load(library_name);
	if (OpStatus::IsError(rc))
	{
		if (g_startup_settings.debug_libraries)
		{
			OpString8 tmp;
			tmp.Set(library_name);
			fprintf(stderr, "opera: Failed to load %s\n", tmp.CStr() ? tmp.CStr() : "");
		}
		OP_DELETE(m_dll);
		m_dll = 0;
		return rc;
	}

	OpStatus::Ignore(g_x11->UnloadOnShutdown(m_dll));

	XRRQueryExtension = reinterpret_cast<XRRQueryExtension_t>(m_dll->GetSymbolAddress("XRRQueryExtension"));
	XRRQueryVersion = reinterpret_cast<XRRQueryVersion_t>(m_dll->GetSymbolAddress("XRRQueryVersion"));
	XRRSelectInput = reinterpret_cast<XRRSelectInput_t>(m_dll->GetSymbolAddress("XRRSelectInput"));
	XRRGetScreenResourcesCurrent = reinterpret_cast<XRRGetScreenResourcesCurrent_t>(m_dll->GetSymbolAddress("XRRGetScreenResourcesCurrent"));
	XRRGetOutputPrimary = reinterpret_cast<XRRGetOutputPrimary_t>(m_dll->GetSymbolAddress("XRRGetOutputPrimary"));
	XRRGetOutputInfo = reinterpret_cast<XRRGetOutputInfo_t>(m_dll->GetSymbolAddress("XRRGetOutputInfo"));
	XRRGetCrtcInfo = reinterpret_cast<XRRGetCrtcInfo_t>(m_dll->GetSymbolAddress("XRRGetCrtcInfo"));
	XRRFreeScreenResources = reinterpret_cast<XRRFreeScreenResources_t>(m_dll->GetSymbolAddress("XRRFreeScreenResources"));
	XRRFreeOutputInfo = reinterpret_cast<XRRFreeOutputInfo_t>(m_dll->GetSymbolAddress("XRRFreeOutputInfo"));
	XRRFreeCrtcInfo = reinterpret_cast<XRRFreeCrtcInfo_t>(m_dll->GetSymbolAddress("XRRFreeCrtcInfo"));
	XRRUpdateConfiguration = reinterpret_cast<XRRUpdateConfiguration_t>(m_dll->GetSymbolAddress("XRRUpdateConfiguration"));

	if (!XRRQueryExtension || !XRRQueryVersion || !XRRSelectInput || !XRRGetScreenResourcesCurrent ||
		!XRRGetOutputPrimary || !XRRGetOutputInfo || !XRRGetCrtcInfo || !XRRFreeScreenResources ||
		!XRRFreeOutputInfo || !XRRFreeCrtcInfo || !XRRUpdateConfiguration)
	{
		if (g_startup_settings.debug_libraries)
		{
			OpString8 tmp;
			tmp.Set(library_name);
			fprintf(stderr, "opera: Failed to load %s (missing symbols)\n", tmp.CStr() ? tmp.CStr() : "");
		}
		m_dll = 0;
		return OpStatus::ERR;
	}

	int major, minor;
	XRRQueryVersion (display, &major, &minor);
	if ((major == 1 && minor >= 3) || major > 1)
	{
		if (g_startup_settings.debug_libraries)
			fprintf(stderr, "opera: XRandR version %d.%d installed\n", major, minor);
		return OpStatus::OK;
	}
	else
	{
		if (g_startup_settings.debug_libraries)
			fprintf(stderr, "opera: Failed to load XRandR (version %d.%d while 1.3 or newer required)\n", major, minor);
		m_dll = 0;
		return OpStatus::ERR_NOT_SUPPORTED;
	}
}


XineramaLoader::XineramaLoader()
	: XINXineramaIsActive(0)
	, XINXineramaQueryScreens(0)
	, m_dll(0)
{
}


OP_STATUS XineramaLoader::Init(X11Types::Display* display)
{
	if (m_dll)
		return OpStatus::OK;

	RETURN_IF_ERROR(OpDLL::Create(&m_dll));

	const uni_char* library_name = UNI_L("libXinerama.so.1");
	OP_STATUS rc = m_dll->Load(library_name);
	if (OpStatus::IsError(rc))
	{
		if (g_startup_settings.debug_libraries)
		{
			OpString8 tmp;
			tmp.Set(library_name);
			fprintf(stderr, "opera: Failed to load %s\n", tmp.CStr() ? tmp.CStr() : "");
		}
		OP_DELETE(m_dll);
		m_dll = 0;
		return rc;
	}

	OpStatus::Ignore(g_x11->UnloadOnShutdown(m_dll));

	XINXineramaIsActive = reinterpret_cast<XINXineramaIsActive_t>(m_dll->GetSymbolAddress("XineramaIsActive"));
	XINXineramaQueryScreens = reinterpret_cast<XINXineramaQueryScreens_t>(m_dll->GetSymbolAddress("XineramaQueryScreens"));

	if (!XINXineramaIsActive || !XINXineramaQueryScreens)
	{
		if (g_startup_settings.debug_libraries)
		{
			OpString8 tmp;
			tmp.Set(library_name);
			fprintf(stderr, "opera: Failed to load %s (missing symbols)\n", tmp.CStr() ? tmp.CStr() : "");
		}
		m_dll = 0;
		return OpStatus::ERR;
	}

	if (XINXineramaIsActive(display))
	{
		if (g_startup_settings.debug_libraries)
			fprintf(stderr, "opera: Xinerama installed\n");
		return OpStatus::OK;
	}
	else
	{
		if (g_startup_settings.debug_libraries)
			fprintf(stderr, "opera: Xinerama not activated\n");
		m_dll = 0;
		return OpStatus::ERR;
	}
}
