/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __X11_GLOBALS_H__
#define __X11_GLOBALS_H__

#include "platforms/utilix/x11_all.h"
#include "x11_gl.h" // For GLX::FBConfig

class OpDLL;
class X11Widget;

extern class X11Globals *g_x11;

/** The (default) color manager, for converting between RGB and pixel values.
 */
//extern class X11ColorManager *g_x11_colormanager;

class X11InputMethod;
class X11InputContext;

struct X11Visual
{
public:
	X11Visual()
		:visual(0)
		,id(None)
		,colormap(None)
		,fbconfig(None)
		,depth(0)
		,image_bpp(0)
		,r_mask(0)
		,g_mask(0)
		,b_mask(0)
		,owns_colormap(false)
	{}

	void FreeColormap(X11Types::Display* display) 
	{ 
		if(owns_colormap) 
		{ 
			if( colormap != None )
				XFreeColormap(display, colormap);
			colormap = None;
			owns_colormap = false; 
		}
	}

public:
	X11Types::Visual* visual;
	X11Types::VisualID id;
	X11Types::Colormap colormap;
	GLX::FBConfig fbconfig;
	int  depth;
	int  image_bpp; // Bit per pixel 
	int  r_mask;
	int  g_mask;
	int  b_mask;
	bool owns_colormap;
};


class XrandrLoader
{
private:
	typedef X11Types::Bool (*XRRQueryExtension_t)(X11Types::Display* dpy, int *event_base_return, int *error_base_return);
	typedef X11Types::Status (*XRRQueryVersion_t)(X11Types::Display* dpy, int *major_version_return, int *minor_version_return);
	typedef void (*XRRSelectInput_t)(X11Types::Display *dpy, X11Types::Window window, int mask);
	typedef XRRScreenResources* (*XRRGetScreenResourcesCurrent_t)(X11Types::Display *dpy, X11Types::Window window);
	typedef RROutput (*XRRGetOutputPrimary_t)(X11Types::Display *dpy, X11Types::Window window);
	typedef XRROutputInfo* (*XRRGetOutputInfo_t) (X11Types::Display *dpy, XRRScreenResources *resources, RROutput output);
	typedef void (*XRRFreeScreenResources_t)(XRRScreenResources *resources);
	typedef XRRCrtcInfo* (*XRRGetCrtcInfo_t)(X11Types::Display *dpy, XRRScreenResources *resources, RRCrtc crtc);
	typedef void (*XRRFreeOutputInfo_t)(XRROutputInfo *outputInfo);
	typedef void (*XRRFreeCrtcInfo_t)(XRRCrtcInfo *crtcInfo);
	typedef int (*XRRUpdateConfiguration_t)(XEvent *event);

public:
	XrandrLoader();
	OP_STATUS Init(X11Types::Display* display);

public:
	XRRQueryExtension_t XRRQueryExtension;
	XRRQueryVersion_t XRRQueryVersion;
	XRRSelectInput_t XRRSelectInput;
	XRRGetScreenResourcesCurrent_t XRRGetScreenResourcesCurrent;
	XRRGetOutputPrimary_t XRRGetOutputPrimary;
	XRRGetOutputInfo_t XRRGetOutputInfo;
	XRRGetCrtcInfo_t XRRGetCrtcInfo;
	XRRFreeScreenResources_t XRRFreeScreenResources;
	XRRFreeOutputInfo_t XRRFreeOutputInfo;
	XRRFreeCrtcInfo_t XRRFreeCrtcInfo;
	XRRUpdateConfiguration_t XRRUpdateConfiguration;

private:
	OpDLL * m_dll;
};

class XineramaLoader
{
private:
	typedef X11Types::Bool (*XINXineramaIsActive_t)(X11Types::Display* dpy);
	typedef XineramaScreenInfo* (*XINXineramaQueryScreens_t)(X11Types::Display* dpy, int* number);

public:
	XineramaLoader();

	OP_STATUS Init(X11Types::Display* display);

public:
	XINXineramaIsActive_t XINXineramaIsActive;
	XINXineramaQueryScreens_t XINXineramaQueryScreens;

private:
	OpDLL * m_dll;
};


/**
 * Various stuff needed for X11 implementation. This class knows which X11 Display
 * to use. It also contains function pointers to the Xft and Xrender libraries. It
 * opens these libraries dynamically on creation, and closes them on destruction.
 */
class X11Globals
{
public:
	struct X11ScreenData
	{
		int m_number;
		int m_width;
		int m_height;
		int m_dpi_x;
		int m_dpi_y;
		// The list is populated only if there is more than one monitor
		OpAutoVector<OpRect> m_rect;
		BOOL m_xrender_supported;
		X11Visual m_visual;
		X11Visual m_argb_visual;
		X11Visual m_gl_visual;
		X11Visual m_gl_argb_visual;
	};

	enum WMClassID
	{
		OperaBrowser = 0,
		OperaNextBrowser,
		OperaWidget,
		OperaNextWidget,
		OperaWidgetManager,
		OperaNextWidgetManager,
	};

	struct WMClassInfo
	{
		WMClassID id;
		OpString8 name;
		OpString8 icon;
	};

	/**
	 * Creates the one and only instance of the X11Globals object. Opens connection 
	 * to the display and probes system to determine capabilities
	 *
	 * @return OpStatus::OK on success, otherwise an error.On any error opera should exit
	 */
	static OP_STATUS Create();
	
	/**
	 * Removes the one and only instance of the X11Globals object and releases
	 * all allocated resources
	 */
	static void Destroy();

	/**
	 * Process event
	 *
	 * @param event The event to examine
	 *
	 * @return TRUE if event was recognized otherwise FALSE
	 */
	BOOL HandleEvent(XEvent* event);

	/**
	 * Returns the display handle
	 */
	X11Types::Display* GetDisplay() const { return m_display; }

	/**
	 * Returns the default screen. Will in in the range of [0..GetScreenCount()-1]
	 */
	int GetDefaultScreen() const { return m_default_screen; }

	/**
	 * Returns the number of detected screens
	 */
	int GetScreenCount() const { return m_screen_count; }

	/**
	 * Tell whether a screen contains a desktop which is formed by more than one monitors
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 * @return TRUE if there are more than one monitor otherwise FALSE
	 */
	BOOL IsVirtualDesktop(UINT32 screen) const { return m_screens[screen].m_rect.GetCount() > 0; }

	/**
	 * Return the rectangle of the specified screen. An X11 screen may contain
	 * virtual sub 'screens' if multiple monitors are used to form one larger
	 * continous desktop. The point argument must be within the rectangle to match.
	 * The returned rectangle may be sub screen rectangle. See also @ref GetScreenWidth
	 * and @ref GetScreenheight
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 * @param point A point that the retuned rectangle must contain
	 *
	 * @return The rectangle that contains the point, otherwise an empty rectangle
	 */
	OpRect GetWorkRect(UINT32 screen, const OpPoint& point) const;

	/**
	 * Returns the width of the specified screen. See also @ref GetWorkRect
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	int GetScreenWidth(UINT32 screen) const { return m_screens[screen].m_width; }

	/**
	 * Returns the height of the specified screen. See also @ref GetWorkRect
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	int GetScreenHeight(UINT32 screen) const { return m_screens[screen].m_height; }

	/**
	 * Returns horizontal DPI
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	int GetDpiX(UINT32 screen) const { return m_screens[screen].m_dpi_x; }

	/**
	 * Returns vertical DPI
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	int GetDpiY(UINT32 screen) const { return m_screens[screen].m_dpi_y; }

	/**
	 * Returns overridden horizontal DPI
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	int GetOverridableDpiX(UINT32 screen) const;

	/**
	 * Returns overridden vertical DPI
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	int GetOverridableDpiY(UINT32 screen) const;

	/**
	 * Returns the information about the visual used for "plain"
	 * windows without alpha channel.
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	X11Visual& GetX11Visual(UINT32 screen) const { return m_screens[screen].m_visual; }

	/**
	 * Returns the information about the visual used for "plain"
	 * windows with an alpha channel. It might not be
	 * initialized. X11Visual.visual will be 0 in that case.
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	X11Visual& GetX11ARGBVisual(UINT32 screen) const { return m_screens[screen].m_argb_visual; }

	/**
	 * Returns the information about the visual used for OpenGL
	 * windows without alpha channel.  It might not be initialized.
	 * X11Visual.visual will be 0 in that case.
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	X11Visual& GetGLVisual(UINT32 screen) const { return m_screens[screen].m_gl_visual; }

	/**
	 * Returns the information about the visual used for OpenGL
	 * windows with an alpha channel.  It might not be initialized.
	 * X11Visual.visual will be 0 in that case.
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	X11Visual& GetGLARGBVisual(UINT32 screen) const { return m_screens[screen].m_gl_argb_visual; }

	/**
	 * Sets the visual to be used for windows rendered to by OpenGL.
	 *
	 * This method sets the visuals returned by GetGLVisual() and
	 * GetGLARGBVisual().  Changing visuals on the fly is probably not
	 * supported, so only call this once for each combination of
	 * 'screen' and 'with_alpha'.
	 *
	 * @param screen Which screen's visual to set.
	 * @param with_alpha true to set the GLARGBVisual() and false to set GLVisual().
	 * @param vi The visual to use.
	 * @param fbconfig the OpenGL fbconfig to use.
	 * @return TRUE if successful, FALSE otherwise
	 */
	BOOL SetGLVisual(int screen, bool with_alpha, XVisualInfo * vi, GLX::FBConfig fbconfig);

	/**
	 * Return the current desktop as reported by _NET_CURRENT_DESKTOP
	 *
	 * @param screen The screen to examine. A value in [0...GetScreenCount()-1] is allowed
	 * @param current_desktop On success the active desktop, otherwise not set
	 *
	 * @return TRUE of successful lookup, otherwise FALSE
	 */
	BOOL GetCurrentDesktop(int screen, int& current_desktop);

	/**
	 * Return the usable rectangle of a workspace on the current desktop. This is
	 * typically the rectangle of a maximized window.
	 *
	 * @param The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 * @param rect On successful return the workspace rectangle
	 *
	 * @return TRUE if a rectangle could be determined, otherwise FALSE
	 */
	BOOL GetWorkSpaceRect(UINT32 screen, OpRect& rect);

	/**
	 * Return the maximum size in bytes that can be used when setting data in a
	 * property (eg, using XChangeProperty()) or selection. The function can
	 * return a value of 0 and bigger. Well behaved code should test for 0
	 *
	 * @param display Display of interrest
	 */
	UINT32 GetMaxSelectionSize(X11Types::Display* display);

	/**
	 * Returns TRUE if a active compositing window manager is detected
	 * on the screen, otherwise FALSE.
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	BOOL IsCompositingManagerActive(UINT32 screen);
	/**
	 * Returns TRUE if the X Render extension can be used. Otherwise FALSE
	 *
	 * @param screen The screen. No range check is made on this parameter
	 *        Use @ref GetScreenCount(). A value in [0...GetScreenCount()-1] is
	 *        allowed
	 */
	BOOL IsXRenderAvailable(UINT32 screen) const { return m_screens[screen].m_xrender_supported; }

	/**
	 * Returns TRUE if the XSync extension can be used. Otherwise FALSE
	 */
	BOOL IsXSyncAvailable() const { return m_support_xsync; }

	/**
	 * Returns TRUE if Opera is using a persona activated skin
	 */
	BOOL HasPersonaSkin() const;

	void SetForcedDpi(int dpi) { m_forced_dpi = dpi; }
	void SetX11ErrBadWindow(bool state) { m_x11err_bad_window = state; }
	bool GetX11ErrBadWindow() const { return m_x11err_bad_window; }

	/**
	 * Returns a window that will be available as long as opera is running
	 */
	X11Types::Window GetAppWindow() const { return m_app_window; }

	/**
	 * Return wmclass information
	 */
	const WMClassInfo& GetWMClassInfo() const { return m_wmclass; }

	/**
	 * Simplified access to the wmclass name. Not const due to limitations
	 * in code where it will be used. Do not change content of returned value
	 *
	 * @return The wmclass name
	 */
	 char* GetWMClassName() { return m_wmclass.name.CStr(); }

	/**
	 * Returns an object that maintains a list of all existing X11Widgets
	 */
	class X11WidgetList *GetWidgetList() const { return m_widget_list; }

	// TODO: These two are probably not needed at all
	OP_STATUS SetWindowBackgroundSkinElement(const char *skin) { return m_background_skin_elm.Set(skin); }
	const char *GetWindowBackgroundSkinElement() { return m_background_skin_elm.CStr(); }

	/**
	 * Register information of window class name and application icon name.
	 *
	 * @param wmclass Container for transferred data
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on insufficient memory
	 */
	OP_STATUS SetWMClassInfo(const WMClassInfo& wmclass)
	{
		m_wmclass.id = wmclass.id;
		RETURN_IF_ERROR(m_wmclass.name.Set(wmclass.name));
		return m_wmclass.icon.Set(wmclass.icon);
	}

	/**
	 * Creates an input context for the window. The caller takes ownership.
	 *
	 * @param window The window
	 *
	 * @return The input context. Will be 0 if allocation failed
	 */
	X11InputContext* CreateIC(X11Widget * window);

	/**
	 * Send startup message to root window using _NET_STARTUP_INFO and
	 * _NET_STARTUP_INFO_BEGIN atoms
	 *
	 * @param message The message to emit
	 */
	void SendStartupXMessage(const OpString8& message);

	/**
	 * Prepare and send startup notification message. Should be called as early as possible and
	 * at least before the first window is mapped. @ref SetWMClassInfo must be called before this function.
	 * See http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt
	 *
	 * @param screen The screen at which the startup occurs
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR if @ref SetWMClassInfo has not been called
	 *         with a class name or OpStatus::ERR_NO_MEMORY on insufficient memory
	 */
	OP_STATUS NotifyStartupBegin(UINT32 screen);

	/**
	 * Send startup notification finished message. Should happend as first window is mapped. The function
	 * will do nothing (and return OpStatus::OK) if @ref NotifyStartupBegin was not already called or failed.
	 * See http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on insufficient memory
	 */
	OP_STATUS NotifyStartupFinish();

	/**
	 * Print Opera version and additional information to the terminal and
	 * optionally a message box
	 *
	 * @param full_version Controls amount of information. TRUE will print out 
	 *        information about enviroment (compositor, toolkit etc) in addition to
	 *        normal version information. Note that full_version equal to TRUE will
	 *        only have effect if core has been initialized
	 * @param show_in_messagebox If TRUE, show information in a message box as well as
	 *        in the terminal. The dialog will only be shown if core has been initialized.
	 * @param parent Parent of message box. It can be 0
	 */
	static void ShowOperaVersion(BOOL full_version, BOOL show_in_messagebox, class DesktopWindow* parent);

	/**
	 * Reads Window Manager name using Extended Window Manager Hints spec.
	 *
	 * @param[out] wm_name
	 */
	OP_STATUS GetWindowManagerName(OpString8 &wm_name);

    /**
     * Register an OpDLL to be released after closing the connection.
     * Takes ownership of the object.
     *
     * @param dll The library to be registered
     */
    OP_STATUS UnloadOnShutdown(OpDLL *dll);

private:
	X11Globals();
	~X11Globals();

	/**
	 * Opens connection to the display and probes system to determine capabilities
	 * 
	 * @return OpStatus::OK on success, otherwise an error.On any error opera should exit
	 */
	OP_STATUS Init();

	/**
	 * Creates an X11 window using the default visual and default depth on the default 
	 * screen. The window is never mapped, but will live as long as Opera is running.
	 */
	OP_STATUS InitApplicationWindow();

	/**
	 * Check for DESKTOP_STARTUP_ID (environment variable), save content and erase it
	 *
	 * @return OpStatus::OK on success (a missing DESKTOP_STARTUP_ID is not an error)
	 *         or OpStatus::ERR_NO_MEMORY on insufficient memory available
	 */
	OP_STATUS InitStartupId();

	/**
	 * Various initialization functions. A non-successful return from these should be 
	 * treated fatal
	 */
	OP_STATUS InitScreens();
	OP_STATUS InitFontSettings();
	OP_STATUS UpdateImageParameters();
	OP_STATUS InitXSync();
	OP_STATUS InitColorManagers();
	OP_STATUS InitXRender();
	OP_STATUS InitVisuals();
	OP_STATUS InitARGBVisuals();

	/**
	 * Escape a string for use inside quotes in X startup notification
	 * messages. Does not add quotes.
	 *
	 * @param source The string to be escaped
	 * @param[out] dest Where to put the escaped string
	 */
	static OP_STATUS EscapeStartupNotificationString(OpStringC8 source, OpString8& dest);

private:
	X11Types::Display* m_display;
	X11InputMethod* m_xim_data;
	X11ScreenData* m_screens;
	OpAutoVector<OpDLL> m_libraries;
	int m_default_screen;
	int m_screen_count;
	int m_forced_dpi;
	int m_xft_dpi;
	int m_xrandr_major;
	int m_xrandr_event;
	int m_xrandr_error;
	OpAutoPtr<XrandrLoader> m_xrr;
	OpAutoPtr<XineramaLoader> m_xin;
	X11Types::Window m_app_window; // Lives through the lifetime of an application
	bool m_image_swap_endianness;
	bool m_support_xsync; // The XSync extension, not related to XSync() function call
	bool m_x11err_bad_window;
	class X11WidgetList *m_widget_list;
	OpString8 m_background_skin_elm;
	OpString8 m_startup_id;
	WMClassInfo m_wmclass;
};

#endif // __X11_GLOBALS_H__
