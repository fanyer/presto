/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_OPWINDOW_H__
#define __X11_OPWINDOW_H__

#include "adjunct/desktop_pi/DesktopOpWindow.h"
#include "x11_opview.h"

class X11WindowWidget;
class X11Widget;
class X11MdeBuffer;
class X11MdeScreen;
class X11OpView;
class X11OpWindow;
class X11VegaWindow;

class X11OpWindowList
{
public:
	X11OpWindowList();
	~X11OpWindowList();

	OP_STATUS add(X11OpWindow * win);
	void remove(X11OpWindow * win);

	/** Usage restriction: Do not modify anything in the list returned
	 * by this method.  Only use the returned object for scanning
	 * through the list.
	 */
	class X11OpWindowListElement * get_list() { return m_list; };

	/** As long as there has been more lock_tail() calls than
	 * unlock_tail() calls, methods of this class (in particular
	 * add(), remove() and cleanup()) has strict restrictions on what
	 * modifications they can make to the list.
	 *
	 * The only change that will be made to the object returned from
	 * get_list or any object reachable through its chain of m_next
	 * pointers is that m_window can be set to 0.
	 *
	 * However, the return value of get_list() itself may change
	 * arbitrarily.
	 */
	void lock_tail()
		{
			m_freeze_tail_structure += 1;
			OP_ASSERT(m_freeze_tail_structure > 0);
		};

	/** Cancel one call to lock_tail().
	 */
	void unlock_tail()
		{
			OP_ASSERT(m_freeze_tail_structure > 0);
			m_freeze_tail_structure -= 1;
			cleanup();
		};

private:
	/** Clean up any mess that may have been caused by
	 * m_freeze_tail_structure being non-zero.
	 */
	void cleanup();

	class X11OpWindowListElement * m_list;
	unsigned int m_freeze_tail_structure;
};

class X11OpWindow : public DesktopOpWindow
{
private:
	enum ThreeState
	{
		STATE_OFF=0,
		STATE_ON,
		STATE_UNKNOWN
	};

public:
	X11OpWindow();
	~X11OpWindow();

	OP_STATUS Init(Style style=STYLE_DESKTOP,
				   OpTypedObject::Type type=OpTypedObject::WINDOW_TYPE_UNKNOWN,
				   OpWindow *parent_window=NULL, OpView *parent_view=NULL,
				   void* native_handle=NULL, UINT32 effect=EFFECT_NONE);
	void SetParent(OpWindow *parent_window=NULL, OpView *parent_view=NULL,
				   BOOL behind=FALSE);
	OpWindow *GetParentWindow() { return m_parent_window; }
	OpView *GetParentView() { return m_parent_view; }
	OpWindow *GetRootWindow();
	void SetWindowListener(OpWindowListener *windowlistener);
	void SetDesktopPlacement(const OpRect &rect, State state,
							 BOOL inner=FALSE, BOOL behind=FALSE,
							 BOOL center=FALSE);
	void GetDesktopPlacement(OpRect &rect, State &state);
	State GetRestoreState() const;
	void SetRestoreState(State state);
	void Restore();
	void Minimize();
	void Maximize();
	void Fullscreen();
	void LockUpdate(BOOL lock);
	void Show(BOOL show);
	void SetFocus(BOOL focus);
	void Close(BOOL user_initiated=FALSE);
	void GetOuterSize(UINT32 *width, UINT32 *height);
	void SetOuterSize(UINT32 width, UINT32 height);
	void GetInnerSize(UINT32 *width, UINT32 *height);
	void SetInnerSize(UINT32 width, UINT32 height);
	void GetOuterPos(INT32 *x, INT32 *y);
	void SetOuterPos(INT32 x, INT32 y);
	void GetInnerPos(INT32 *x, INT32 *y);
	void SetInnerPos(INT32 x, INT32 y);
	void GetRenderingBufferSize(UINT32* width, UINT32* height);
	void SetMinimumInnerSize(UINT32 width, UINT32 height);
	void SetMaximumInnerSize(UINT32 width, UINT32 height);
	void GetMargin(INT32 *left_margin, INT32 *top_margin,
				   INT32 *right_margin, INT32 *bottom_margin);
	void SetTransparency(INT32 transparency);
	void SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility);
	void SetFloating(BOOL floating);
	void SetAlwaysBelow(BOOL below);
	void SetShowInWindowList(BOOL in_window_list);
	void UpdateFont();
	void UpdateLanguage();
	void UpdateMenuBar();
	void SetSkin(const char *skin);
	void Redraw();
	Style GetStyle();
	void Raise();
	void Lower();
	void Activate();
	void Deactivate();
	void Attention();
	void SetCursor(CursorType cursor);
	CursorType GetCursor();
	const uni_char *GetTitle() const;
	void SetTitle(const uni_char *title);
	void SetIcon(OpWidgetImage *image);
	void SetResourceName(const char *name);
	BOOL IsActiveTopmostWindow();

	/**
	 * Register a background painter listener that will update the background
	 * of the window when a Persona enabled skin requests it
	 */
	OP_STATUS MakeBackgroundListener();

	/**
	 * Update the background of a window that has installed a background painter listener
	 */
	void UpdateBackgroundListener();

	void SetMenuBarVisibility(BOOL visible);
	BOOL GetMenuBarVisibility();

	BOOL SupportsExternalCompositing();

	// From DesktopOpWindow
	virtual void OnVisibilityChanged(BOOL visibility);
	virtual void SetLockedByUser(BOOL locked);

	/**
	 * Request window manager to show or hide the window decoration for a top
	 * level window. Note: The window manager can decide to ignore this request
	 *
	 * @param show Show the decoration if true, otherwise hide
	 */
	virtual void SetShowWMDecoration(BOOL show_decoration);

	/**
	 * Returns TRUE if the window manager shows the decoration
	 */
	virtual BOOL HasWMDecoration();

	// For scope
	BOOL GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out);


	// Implementation specific methods:

	/**
	 * Sets if window shal be activated when shown. Default is TRUE. The setting
	 * has no effect on modal windows
	 */
	void SetActivateOnShow(BOOL activate) { m_packed.is_activate_on_show = activate; }

	/**
	 * Prepares the window to be a menu bar
	 */
	void RegisterAsMenuBar();

#ifdef VEGA_OPPAINTER_SUPPORT
	OP_STATUS ResizeWindow(int width, int height);
	X11MdeBuffer* GetMdeBuffer() const;
	X11MdeScreen* GetMdeScreen() const;
	BOOL IsToplevel() const { return m_packed.is_toplevel; }
#endif // VEGA_OPPAINTER_SUPPORT

	bool IsVisible() const { return mdeWidget->m_is_visible; }

	X11Widget* GetWidget() const { return m_widget; }
	X11Widget* GetOuterWidget() const { return m_outer_widget; }
	OpWindowListener* GetWindowListener() const { return m_window_listener; }
	void SetInnerSizeFromPlatform(UINT32 width, UINT32 height);

	/**
	 * Updates the size to core. The local window is not resized
	 *
	 * @param width  Width in pixels
	 * @param height Height in pixels
	 */
	void SetContentSize(UINT32 width, UINT32 height);

	BOOL UiClosing();
	class MdiManager *GetMdiManager();
	X11OpWindow* GetNativeWindow();

	class X11OpWindowBackgroundListener* GetBackgroundListener() const { return m_background_listener.get(); }

private:
	/**
	 * Creates the toplevel widget window depending on style and type
	 *
	 * @param parent_widget Parent of new widget
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS InitToplevelWidgets(X11Widget* parent_widget);

	void InnerOuterSizeDiff(int &w, int &h);
	void InnerLeftTopOffset(int &dx, int &dy);
#ifdef VEGA_OPPAINTER_SUPPORT
	void ConvertFromScreen(int &x, int &y);
	void ConvertToScreen(int &x, int &y);
#endif // VEGA_OPPAINTER_SUPPORT


private:
	X11Widget* m_widget;
	X11Widget* m_outer_widget;
	class UiWindowNotifier* m_notifier;
	Style m_style;
	UINT32 m_effect;
	OpTypedObject::Type m_type;
	/* m_parent_view and m_parent_window should probably be removed.
	 * MDE_OpWindow has the same attributes with the same names,
	 * presumably holding the same values (though slightly different
	 * types).  We should probably just use those instead (see comment
	 * in SetParent()).
	 */
	X11OpView* m_parent_view;
	X11OpWindow* m_parent_window;
	OpWindowListener* m_window_listener;
	CursorType m_cursor;
	ThreeState m_stored_menu_visible;
	OpAutoPtr<class X11OpWindowBackgroundListener> m_background_listener;
	union
	{
		struct
		{
			unsigned int is_toplevel:1;                       // True is window native non-mde window (parent of mde windows)
			unsigned int is_closed:1;                         // True after the window has been closed
			unsigned int is_visible:1;                        // True when the window is visible
			unsigned int is_active:1;                         // True when window is active
			unsigned int is_tool_window:1;                    // True when window is a tool window.
			unsigned int is_menubar:1;                        // True when window is a menu bar
			unsigned int is_menubar_visible:1;                // True when the menubar of the window is visible
			unsigned int is_floating:1;                       // True for widgets that are stay on top
			unsigned int is_set_floating_on_show:1;           // Function that must be called after widget is shown
			unsigned int is_set_show_in_windowlist_on_show:1; // Function that must be called after widget is shown
			unsigned int is_activate_on_show:1;               // Call Activate() when showing the window
			unsigned int is_use_screen_coordinates:1;         // Use screen coordinates for embedded effect widgets
			unsigned int is_embedded_toplevel_window:1;       // The window is a toplevel OpWindow but the X11 Window is not toplevel
			unsigned int is_move_behind_on_show:1;
		} m_packed;
		unsigned int m_packed_init;
	};

private:
	/**
	 * The maximum width or height of the window.
	 *
	 * The value is chosen arbitrarily as one that is big enough to make the
	 * window behave like its size is unlimited when needed, and small enough
	 * to not overwhelm some window managers (like ion3).  Note that Dialog
	 * uses the same value.
	 */
	static const int MAXIMUM_SIZE = 10000;

	static bool s_in_visibility_update;
	static X11OpWindowList s_embedded_toplevel_windows;
};

#endif // __X11_OPWINDOW_H__
