/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OPWINDOW_H
#define WINDOWS_OPWINDOW_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include <Oleacc.h>
#include <Winuser.h>
#endif

#include "adjunct/desktop_pi/DesktopOpWindow.h"
#include "adjunct/desktop_util/settings/SettingsListener.h"

#include "modules/libgogi/pi_impl/mde_generic_screen.h"
#include "modules/libvega/vegawindow.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "modules/pi/OpAccessibilityAdapter.h"
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
# include "platforms/windows/pi/WindowsOpTaskbar.h"
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

#include "platforms/windows/windows_ui/hash_tables.h"

#include "platforms/windows/windows_ui/OpShObjIdl.h"
#include "platforms/windows/WindowsTimer.h"

#include "modules/skin/skin_listeners.h"
#include "modules/libgogi/mde.h"

class OUIMenuBar;
class VegaMDEScreen;
class WindowFrame;
class MDE_View;

#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
#define WM_DWMSENDICONICTHUMBNAIL           0x0323
#define WM_DWMSENDICONICLIVEPREVIEWBITMAP   0x0326
#define OPERA_WINDOW_CLASSNAME				"OperaWindowClass"
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

enum WIN32_TOUCH_EVENT 
{
	/** Touch detected */
	WIN32_TOUCH_DOWN,
	/** Touch removed */
	WIN32_TOUCH_UP,
	/** Touch moved */
	WIN32_TOUCH_MOVE
};

#ifndef MOUSEEVENTF_FROMTOUCH
#define MOUSEEVENTF_FROMTOUCH			0xFF515700
#endif // MOUSEEVENTF_FROMTOUCH

class WindowsOpWindow;
class VEGA3dWindow;

#define WM_MESSAGE_HANDLED(x) // OutputDebugStringA( x "\n" )

class WindowsVegaWindow : public VEGAWindow
{
public:
	WindowsVegaWindow();
	virtual ~WindowsVegaWindow();

	OP_STATUS Construct(unsigned int w, unsigned int h, HWND win, VegaMDEScreen *screen);

	bool IsHardwareWindow() const { return m_hwWin != NULL; }

	void setSystemMenuOn(BOOL on){m_system_menu_on_wanted = on;}
	void setTransparentMargins(int top, int bottom, int left, int right);
	void getTransparentMargins(int& top, int& bottom, int& left, int& right);

	unsigned int getWidth(){return m_width;}
	unsigned int getHeight(){return m_height;}
	VEGAPixelStore* getPixelStore(){return &m_pixelstore;}
	void* getNativeHandle();

	void present(const OpRect* update_rects, unsigned int num_rects);
	bool wm_size(unsigned int width, unsigned int height);

	// Test if the VEGA3dWindow supports getBackbufferHandle, which is the faster path
	// to present layered window
	void TestLayeredWindow();

	void SetIsLayered() { m_is_layered_window = TRUE; }
	BOOL GetIsLayered() { return m_is_layered_window; }
	
	void SetLayeredTransparency(UINT8 val) { m_layered_window_transparency = val; }

	void SetFullTransparency(BOOL full);
	BOOL HasFullTransparency() { return m_full_transparency; }
	void SetTransparencyChanged() { m_transparency_changed = TRUE; }
	void ExtendFrameInfoClientArea();

	OP_STATUS initSoftwareBackend();
	OP_STATUS initHardwareBackend(VEGA3dWindow* win);

	HDC GetBitmapDC(){return m_hBitmapDC;}
	void ChangeDCFont(HFONT fnt, UINT32 color);

	virtual bool requiresBackbufferReading() {return m_is_layered_window ? true : false;}

private:
	VEGAPixelStore m_pixelstore;
	unsigned int m_width;
	unsigned int m_height;

	HWND m_hwnd;
	HBITMAP m_bitmap;
	HDC m_hBitmapDC;
	HBITMAP m_hOldBitmap;
	BOOL m_bufferInitialized;
	VEGA3dWindow* m_hwWin;
	// whether VEGA3dWindow has a functioning getBackbufferHandle implementation, -1 means unknown
	int m_has_valid_getdc;
	HFONT m_curFont;
	HFONT m_oldFont;
	UINT32 m_curFontColor;

	int m_transparent_margin_top;
	int m_transparent_margin_bottom;
	int m_transparent_margin_left;
	int m_transparent_margin_right;
	int m_extended_client_area_top;
	int m_extended_client_area_bottom;
	int m_extended_client_area_left;
	int m_extended_client_area_right;
	BOOL m_is_layered_window;
	UINT8 m_layered_window_transparency;
	BOOL m_system_menu_on_wanted;
	BOOL m_system_menu_on;
	VegaMDEScreen *m_screen;
	BOOL m_full_transparency;		// TRUE for aero in the complete window
	BOOL m_transparency_changed;	// TRUE if we need to update the aero area
};

class VegaBackgroundMDEView : public MDE_View
{
public:
	virtual ~VegaBackgroundMDEView() {}

	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
};

class VegaMDEScreen : public MDE_GenericScreen, public WindowsTimerListener
{
	friend class VegaBackgroundMDEView;

public:
	VegaMDEScreen(int bufferWidth, int bufferHeight, int bufferStride, MDE_FORMAT bufferFormat, void *bufferData, WindowsOpWindow* w);
	virtual ~VegaMDEScreen();

	virtual OP_STATUS Init();
	virtual void OnValidate();
	virtual void OnInvalid();
	virtual void ScrollPixels(const MDE_RECT &rect, int dx, int dy);
	virtual bool ScrollPixelsSupported();
	virtual OP_STATUS Resize(int bufferWidth, int bufferHeight, int bufferStride, void *bufferData);
	virtual bool IsResizing();
	virtual bool UseTransparentBackground();

	// WindowsTimerListener
	virtual void OnTimeOut(class WindowsTimer* timer);

	WindowsOpWindow* GetWindow() {return m_window;}
private:
	WindowsVegaWindow* m_vegaWin;
	WindowsOpWindow* m_window;
	VegaBackgroundMDEView* m_background_view;		// the view filling the whole screen, used for backgrounds etc.

	WindowsTimer m_timer;
	double m_last_validate;
};

/***********************************************************************************
**
**	WindowsOpWindow
**
***********************************************************************************/

class WindowsOpWindow : public DesktopOpWindow, public Link, public SettingsListener, public SkinNotificationListener
{
public:

	class AttentionTimerListener : public OpTimerListener
	{
		void OnTimeOut(OpTimer* timer);
	};

	enum OpWindowOptions
	{
		opWindowAllOptions     = 0x00000000,
		opWindowDisableAppkeys = 0x00000001,
		opWindowNoContextMenu  = 0x00000002 //allow EmBrowser to block context menus
	};
	WindowsOpWindow();
	~WindowsOpWindow();

	static OP_STATUS StaticInitialization();

	//
	// OpWindow implementation
	//

	OP_STATUS Init(Style style, OpTypedObject::Type type, OpWindow* parent_window, OpView* parent_view, void* native_handle, UINT32 effect);

	void SetParent(OpWindow* parent_window, OpView* parent_view, BOOL behind);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	void SetAccessibleItem(OpAccessibleItem* accessible_item) {m_accessible_item=accessible_item;}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	virtual OpWindow* GetParentWindow() {return m_parent_window;}
	virtual OpView* GetParentView() {return m_parent_view;}

	void SetWindowListener(OpWindowListener *windowlistener);

	void SetDesktopPlacement(const OpRect& rect, State state, BOOL inner = FALSE, BOOL behind = FALSE, BOOL center = FALSE);
	void GetDesktopPlacement(OpRect& rect, State& state);

	State GetRestoreState() const;
	void SetRestoreState(State state);

	void Restore();
	void Minimize();
	void Maximize();
	void Fullscreen();
	void LockUpdate(BOOL lock);

	void SetFloating(BOOL floating);
	void SetAlwaysBelow(BOOL below);
	void SetShowInWindowList(BOOL in_window_list);

	void Show(BOOL show = TRUE);
	void SetFocus(BOOL focus) {}
	void Close(BOOL user_initiated = FALSE);

	void GetOuterSize(UINT32* width, UINT32* height);
	void SetOuterSize(UINT32 width, UINT32 height);
	void GetInnerSize(UINT32* width, UINT32* height);
	void SetInnerSize(UINT32 width, UINT32 height);
	void GetOuterPos(INT32* x, INT32* y);
	void SetOuterPos(INT32 x, INT32 y);
	void GetInnerPos(INT32* x, INT32* y);
	void SetInnerPos(INT32 x, INT32 y);

	void GetRenderingBufferSize(UINT32* width, UINT32* height);
	void SetMinimumInnerSize(UINT32 width, UINT32 height);
	void SetMaximumInnerSize(UINT32 width, UINT32 height);
	void GetMargin(INT32* left_margin, INT32* top_margin, INT32* right_margin, INT32* bottom_margin) {*left_margin = m_left_margin; *top_margin = m_top_margin; *right_margin = m_right_margin; *bottom_margin = m_bottom_margin;}

	void SetTransparency(INT32 transparency);
	void SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility);
	void SetMenuBarVisibility( BOOL visible );
	BOOL GetMenuBarVisibility() {return m_menubar_visibility;} 

	/** Update the translated text in the window.
	 *
	 * Only used for special windows like the toolbar etc.
	 */
	virtual void UpdateLanguage();
	void SetSkin(const char* skin);
	void Redraw();
	Style GetStyle() { return m_style; }

	void Raise();
	void Lower();
	void Activate() {Activate(TRUE);}
	void Deactivate();
	void Attention();

	void SetCursor(CursorType cursor);
	CursorType GetCursor();

	const uni_char* GetTitle() const;
	void SetTitle(const uni_char* title);

	void SetIcon(OpWidgetImage* img);
	void SetIcon(OpBitmap* bitmap);

	BOOL IsActiveTopmostWindow();


	//
	// DesktopOpWindow implementation
	//

	virtual void OnVisibilityChanged(BOOL visibility) {if (m_window_listener) m_window_listener->OnVisibilityChanged(visibility);}
	virtual void SetLockedByUser(BOOL locked);

	virtual void OnPagebarMoved(BOOL onTop, int height);
	virtual void OnTransparentAreaChanged(int top, int bottom, int left, int right);
	virtual void SetFullTransparency(BOOL m_full_transparency);

	virtual BOOL SupportsExternalCompositing();

	BOOL GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out);


	//
	// SettingsListener implementation
	//

	virtual void OnSettingsChanged(DesktopSettings* settings);


	//
	// SkinNotificationListener
	//

	virtual void OnSkinLoaded(OpSkin *loaded_skin) {}
	virtual void OnPersonaLoaded(OpPersona *loaded_persona);


	//
	// WindowsOpWindow functions
	//

	static BOOL IsDestroyingOpWindow(HWND hwnd);
	static WindowsOpWindow* GetWindowFromHWND(HWND hwnd);
	static WindowsOpWindow* GetAdoptedWindowFromHWND(HWND hwnd);
	static void	ClosePopup(HWND hWndNextWindow); //do not close the popup, if it is to be the next active window...
	static WindowsOpWindow*	FindWindowUnderCursor(); // find the OpWindow under the cursor, if any
	static void	FinishDrag(HWND hWnd); 	// called on dnd drop

	void SetParent(HWND parenthwnd, BOOL behind);
	void SetIsThemed(BOOL is_themed) { m_themed = is_themed; }

	const char*	GetMenuName();
	OUIMenuBar* GetMenuBar();

	HICON GetIcon(int icon_size = ICON_BIG);
	class OpPagebar* GetPagebar();
	WindowFrame* GetFrame() { return m_frame; }
	HWND GetParentHWND(); // Get the HWND of this window or closest parent that has a HWND.
	HWND GetDesktopParent();
	virtual OpView* GetFirstChildView();

	void Enable();
	void Disable();
	BOOL IsEnabled() {return m_disabled_count == 0;}

	void Activate(BOOL restore_from_minimized);
	void EnsureHiddenIconicState();
	void DeactivateAndActivateTopmost(BOOL minimizing = FALSE);
	void FindAndActivateDesktopWindow(int x, int y);

	unsigned long GetOptions(){	return m_options;}
	void SetOptions(unsigned long options){ m_options = options; }
	void SetOption(OpWindowOptions option) { m_options |= option; }

	BOOL IsFocusable();
	BOOL IsDialog();
	BOOL IsModalDialog();
	BOOL IsLayeredWindow(); // Is the window layered (style WS_EX_LAYERED)
	BOOL IsThemed() { return m_themed; }
	BOOL IsLockedByUser() { return m_locked_by_user; } // used if the user locks a window/page
	bool IsCursorInChrome(); // Will return true if cursor is in chrome.
	LRESULT IsCursorOnBorder() const; // Will return > 0 if cursor is on border.

	BOOL HasFullTransparency();
	BOOL HasDropShadow() { return m_effects & OpWindow::EFFECT_DROPSHADOW; }
	BOOL HasDecorations(); // Checks if this window is decorated (has native title bar and borders).

#ifdef GADGET_SUPPORT
	BOOL IsThisAGadget() const;
    void SetGadgetWinOPViewChild(OpView* view) { m_gadget_opview_child = view; }
    OpView* GetBottomGadgetOPView();
#endif // GADGET_SUPPORT

	void SetWantTitlebar(BOOL want_title_bar) { m_want_title_bar = want_title_bar; }
	BOOL GetWantTitlebar() { return m_want_title_bar; }
	void UpdateDwmFrameWritingDirection();

	BOOL HasDirtyThumbnail() { return m_dirty_thumbnail; }
	void SetDirtyThumbnail(BOOL dirty) { m_dirty_thumbnail = dirty; }

	/** Get a DC for the windows render target. This will only work for the 
	 * software backend as direct3d only supports dc for non alpha software 
	 * surfaces which we cannot use for rendering.
	 * Remember to set the current clipping since that isn't done automatically on the hdc! */
	HDC GetRenderTargetDC();

	void Validate(bool clear_pending = false);
	void Resize(BOOL force = FALSE);
	void UpdateBufferSize(unsigned int buffer_width, unsigned int buffer_height);

	/** Handle horizontal and vertical mouse wheel events. */
	long HandleMouseWheelEvent(bool vertical, WPARAM wparam, LPARAM lparam, POINT client_point, ShiftKeyState key_state);

	/* In case we receive WM_CLOSE message from plugin, it will first send
	   message to WindowsOpPluginWindow which will mark this WindowsOpWindow
	   instance to ignore next WM_CLOSE message. */
	BOOL						GetIgnoreNextCloseMsg() { return m_ignore_next_close_msg; }
	void						SetIgnoreNextCloseMsg(BOOL ignore) { m_ignore_next_close_msg = ignore; }

#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
	void SetTaskbarMessageListener(WindowsSevenTaskbarMessageListener *listener) {	m_taskbar_message_listener = listener; }
	WindowsSevenTaskbarMessageListener* GetTaskbarMessageListener() { return m_taskbar_message_listener; }
	HWND GetTaskbarProxyWindow(BOOL create_if_missing = TRUE);
	void GetDocumentWidgetCoordinates(int& x, int& y);
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS


	//
	// Window message dispatch table
	//
	// We have a dispatch table of function pointers for window messages. Each message we handle
	// should have a function that returns long, and has the WPARAM and LPARAM parameters.
	//
	// Currently only a few messages are converted to this new form, but all of them should be
	// converted. We have reserved an array big enough for all the 1024 Windows reserved messages.
	// User defined messages still have to be handled the old way.
	//

	typedef long (WindowsOpWindow::*WmMessageFunction)(WPARAM, LPARAM);
	static WmMessageFunction wm_messages[WM_USER];

	long WmPaint(WPARAM, LPARAM);
	long WmWindowposchanged(WPARAM, LPARAM);

public:

	static bool							s_touch_gesture_active;		// A touch gesture has started
	static BOOL							s_attention_inverted;
	static OpTimer*						s_attention_timer;
	static Head							s_attention_list;
	static AttentionTimerListener		s_attention_timer_listener;
	static WindowsOpWindow*				s_popup_window;
	static WindowsOpWindow*				s_current_popup_window;
	static UINT32						s_expecting_dialog_close;
	static DWORD						s_atom;
	static DWORD						s_atom_ds;
	static HICON						s_custom_icon;
	static HICON						s_custom_icon_small;
	static HICON						s_winstaller_icon;
	static HICON						s_winstaller_icon_small;

	INT32								m_width;
	INT32								m_height;

	class WindowsVegaWindow*			m_vega_window;
	class VegaMDEScreen*				m_mde_screen;

	HWND								m_hwnd;
	BOOL								m_hwnd_active;
	WindowsOpWindow*					m_parent_window;
	OpView*								m_parent_view;

	OUIMenuBar*							m_menubar;
	BOOL								m_menubar_visibility;

	OpWindowListener*					m_window_listener;

	int									m_titlebar_height;
	int									m_requested_titlebar_height;
	int									m_extended_chrome_height;
	int									m_requested_extended_chrome_height;

	OpTypedObject::Type					m_type;
	State								m_state;
	BOOL								m_fullscreen;
	BOOL								m_sizemove; // window is being resized
	DWORD								m_win_style;
	DWORD								m_win_style_ex;

	INT32								m_left_margin;
	INT32								m_top_margin;
	INT32								m_right_margin;
	INT32								m_bottom_margin;

	UINT32								m_minimum_width;
	UINT32								m_minimum_height;
	UINT32								m_maximum_width;
	UINT32								m_maximum_height;

	class WindowsBackgroundClearer*		m_background_clearer;

	BOOL								m_active;
	BOOL								m_is_closing;
	BOOL								m_user_initiated_close;
	BOOL								m_inverted;
	BOOL								m_flash_count;
	BOOL								m_parent_has_been_disabled;
	BOOL								m_disable_cursor_change;
	BOOL								m_workspace_minimized_visibility;
	BOOL								m_keep_below;

	BOOL								m_pending_mde_render;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OpAccessibleItem*					m_accessible_item;
	BOOL								m_accessibility_inited;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

private:

	static OpWindowsPointerHashTable<const HWND, WindowsOpWindow *> s_adopted_windows;

	int									m_extended_chrome_top;
	int									m_extended_chrome_bottom;
	int									m_extended_chrome_left;
	int									m_extended_chrome_right;

#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
	WindowsSevenTaskbarMessageListener*	m_taskbar_message_listener;
	HWND								m_taskbar_proxy_wnd; // top-level proxy window that will receive the Windows 7 DWM taskbar messages
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

	BOOL								m_themed;

	BOOL								m_is_hidden;
	Style								m_style;
	INT32								m_disabled_count;

	OpRect								m_restored_rect;
	State								m_old_state;
	const char*							m_skin;
	HWND								m_active_window_when_created;
	
	// This is only relevant for the window types that are supposed to be 
	// platform window but for some reason can't. In which case the coordinates
	// used to position the window has to be translated before use.(DSK-347376)
	BOOL								m_translate_coordinate;

	UINT32								m_effects;
	BOOL								m_dirty_thumbnail;		// used in combination with Windows 7 integration.
																// If set, changes have occurred and a new thumbnail needs to be generated for Win 7
	HPOWERNOTIFY						m_power_notify;			// handle used for power change notifications
	BOOL								m_want_title_bar;		// set to TRUE if we need to show the title bar (pagebar is not the topmost toolbar)
	WindowFrame*						m_frame;				// The frame specific code for the top level window
	unsigned long						m_options;
    OpView*		                        m_gadget_opview_child;
	BOOL								m_locked_by_user;
	BOOL								m_ignore_next_close_msg;

public:

	// used to keep WM_GESTURE state information
	struct OpGestureInfo
	{
		GESTUREINFO gi;
		float arguments;
		POINT first_point;
		POINT second_point;
		POINT finger_distance;
		MDE_View *captured_view;		// the view receiving the touch input
		MDE_RECT view_rect;				// size of the view receiving touch input
		int overflow_y;					// how much is the overflow Y for touch
	
	} m_gesture_info;
};

#endif // WINDOWS_OPWINDOW_H
