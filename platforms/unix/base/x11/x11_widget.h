/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_WIDGET_H__
#define __X11_WIDGET_H__

#include "x11_callback.h"

#include "modules/display/cursor.h" // CursorType
#include "modules/util/tree.h"
#include "modules/util/adt/oplisteners.h"

#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/utilix/x11_all.h"

class DocumentDesktopWindow;
class OpWidgetImage;
class X11MdeScreen;
class X11Icon;
class X11MdeBuffer;
class X11VegaWindow;
class X11OpWindow;
class X11InputContext;
class OpInputMethodListener;


struct MotifWindowManagerHints
{
public:
	enum 
	{
		HINTS_FUNCTIONS   = (1L << 0),

		FUNCTION_ALL      = (1L << 0),
		FUNCTION_RESIZE   = (1L << 1),
		FUNCTION_MOVE     = (1L << 2),
		FUNCTION_MINIMIZE = (1L << 3),
		FUNCTION_MAXIMIZE = (1L << 4),
		FUNCTION_CLOSE    = (1L << 5),

		HINTS_DECORATIONS = (1L << 1),

		DECORATION_ALL      = (1L << 0),
		DECORATION_BORDER   = (1L << 1),
		DECORATION_RESIZEH  = (1L << 2),
		DECORATION_TITLE    = (1L << 3),
		DECORATION_MENU     = (1L << 4),
		DECORATION_MINIMIZE = (1L << 5),
		DECORATION_MAXIMIZE = (1L << 6),

		HINTS_INPUT_MODE = (1L << 2),

		INPUT_MODELESS                  = 0L,
		INPUT_PRIMARY_APPLICATION_MODAL = 1L,
		INPUT_FULL_APPLICATION_MODAL    = 3L
	};

public:
	MotifWindowManagerHints() 
	{ 
		m_data.flags = 0L; m_data.functions = 0L; m_data.decorations = 0L; m_data.input_mode = 0L; m_data.status = 0L; 
	}
	void Apply( X11Types::Display* display, X11Types::Window window);
	void SetDecoration(bool on);
	void SetModal();

public:
	union 
	{
		struct 
		{
			unsigned long flags;
			unsigned long functions;
			unsigned long decorations;
			long input_mode;
			unsigned long status;
		} m_data;
		unsigned long buf[5];
	};
};


/** A listener class for being notified about events happening to
 * native X11 resources.
 *
 * Note that the callbacks in this listener generally have empty
 * implementations, so it is only necessary to implement those
 * callbacks you care about.
 */
class X11ResourceListener
{
public:
	virtual ~X11ResourceListener() {}

	/** Called before a native X11 window is destroyed.
	 */
	virtual void OnBeforeNativeX11WindowDestroyed(X11Types::Window win) {};
};


class X11Widget : public Tree, public X11CbObject
{
public:
	enum WindowState {
		NORMAL,
		MAXIMIZED,
		ICONIFIED,
		FULLSCREEN
	};

	enum WindowFlag
	{
		TRANSIENT     = 0x01,   // Window is a transient window
		BYPASS_WM     = 0x02,   // Enables override redirect. Use with caution
		POPUP         = 0x04,   // Window is a popup window
		MODAL         = 0x08,   // Window is a modal window
		AUTOGRAB      = 0x10,
		UTILITY       = 0x20,
		PERSONA       = 0x40,   // Window supports drawing its own window decorations in place of the window manager
		TRANSPARENT   = 0x80,
		TOOLTIP       = 0x100,  // Window is a tooltip window
		OFFSCREEN     = 0x200,
		SYNC_REQUEST  = 0x400,  // Window implements _NET_WM_SYNC_REQUEST
		NODECORATION  = 0x800   // Window should be drawn without standard WM decoration
	};

	// Window decoration modes. Window manager or Opera's own. A skinned decoration can be set
	// on all toplevel browser windows, but will look the best when a PERSONA enabled skin is
	// active.
	enum DecorationType
	{
		DECORATION_SKINNED,
		DECORATION_WM
	};

	// These enums must match the defined values specified at freedesktop.org (_NET_WM_MOVERESIZE)
	enum MoveResizeAction
	{
		MOVE_RESIZE_TOPLEFT     = 0,
		MOVE_RESIZE_TOP         = 1,
		MOVE_RESIZE_TOPRIGHT    = 2,
		MOVE_RESIZE_RIGHT       = 3,
		MOVE_RESIZE_BOTTOMRIGHT = 4,
		MOVE_RESIZE_BOTTOM      = 5,
		MOVE_RESIZE_BOTTOMLEFT  = 6,
		MOVE_RESIZE_LEFT        = 7,
		MOVE_RESIZE_MOVE        = 8,
		MOVE_RESIZE_NONE        = 100 // Not part of spec, used by Opera
	};

	class InputEventListener
	{
		public:
			virtual BOOL OnInputEvent(XEvent* e) = 0;
	};

	class X11WidgetListener
	{
		public:
			virtual void OnDeleted(X11Widget* widget) = 0;
	};

	class DragHotspot
	{
	public:
		DragHotspot(): m_valid(false), m_can_start(false), m_x(0), m_y(0) {}

		void Set(int x, int y) { m_valid = true; m_can_start = true; m_x = x; m_y = y; }
		int GetX() const { return m_x; }
		int GetY() const { return m_y; }
		bool IsValid() const { return m_valid; }
		void Reset() { m_can_start = m_valid = false; }
		bool Start(int x, int y) { return m_can_start && (abs(m_x-x) > 5 || abs(m_y-y) > 5); }
		void Done() { m_can_start = false; }

	private:
		bool m_valid;
		bool m_can_start;
		int m_x;
		int m_y;
	};


private:
	enum DragState
	{
		DSIdle,
		DSPending,
		DSActive
	};

	struct WindowDrag
	{
		WindowDrag() { state = DSIdle; button = 0; }
		DragState state;
		OpPoint origin;
		int button;
	};
	
	struct ToplevelData
	{
		OpRect decoration;
		DecorationType decoration_type;
		int normal_x;
		int normal_y;
		int normal_width;
		int normal_height;
		// A window id if the WM decorates the window without reparenting it 
		X11Types::Window decoration_window; 
		X11Types::XSyncCounter sync_counter_id;
		X11Types::XSyncValue sync_counter_value;
		/* The window that should hold the X keyboard focus whenever
		 * anything inside this toplevel window wants keyboard input.
		 */
		X11Widget * focus_proxy;

		ToplevelData() :
			decoration_type(DECORATION_WM),
			normal_x(0),
			normal_y(0),
			normal_width(0),
			normal_height(0),
			decoration_window(None),
			sync_counter_id(0),
			focus_proxy(0)
			{ }
	};

public:
	X11Widget();
	virtual ~X11Widget();

	/**
	 * Initializes the widget content. 
	 *
	 * @param parent of the widget. Can be 0
	 * @param name X11 resource name of the widget
	 * @param widget flags 
	 * @param screen Only used if there is no parent. A negative value means default screen 
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	virtual OP_STATUS Init(X11Widget *parent, const char *name=0, int flags=0);
	virtual OP_STATUS Init(X11Widget* parent, const char* name, int flags, int screen);

	/**
	 * Returns true if this widget is to be rendered to with OpenGL,
	 * false otherwise.
	 *
	 * If this method is called before the choice of rendering API has
	 * been made, this method will make that choice by guessing what
	 * core will use.
	 */
	bool IsUsingOpenGL();

	OP_STATUS RecreateWindow();

	/**
	 * Replace internal window with new
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS ReplaceWindow( X11Types::Window window );

	/**
	 * Changes the visual. It means the window has to be recreated as the depth may change
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS ChangeVisual(XVisualInfo* vi);

	/**
	 * Sets the window resource name.
	 *
	 * @param name The new resource name
	 * 
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS SetResourceName(const char* name);

	/**
	 * Creates an internal back buffer that is not connected to core
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on insufficient memory
	 */
	OP_STATUS CreateMdeBuffer();

	/**
	 * Creates an internal MDE layer
	 *
	 * @param width Buffer width. Value is taken from GetWidth() if less of equal to 0
	 * @param height Buffer height. Value is taken from GetHeight() if less of equal to 0
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS CreateMdeScreen(int width=-1, int height=-1);

	/**
	 * Resizes an internal MDE layer.
	 *
	 * @width Buffer width. Value is taken from GetWidth() if less of equal to 0
	 * @height Buffer height. Value is taken from GetHeight() if less of equal to 0
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS ResizeMdeScreen(int width=-1, int height=-1);

	/**
	 * Specifies an MDE layer
	 */
	void SetMdeScreen(X11MdeScreen* scr, X11VegaWindow* window, X11MdeBuffer* buffer){ m_mde_screen = scr; m_vega_window = window, m_mde_buffer = buffer; }

	/**
	 * Returns the mde buffer of the MDE layer. Always test for NULL
	 */
	X11MdeBuffer* GetMdeBuffer() const { return m_mde_buffer; }

	/**
	 * Returns the mde screen of the  MDE layer. Always test for NULL
	 */
	X11MdeScreen* GetMdeScreen() const { return m_mde_screen; }

	/**
	 * Returns the vega window of the  MDE layer. Always test for NULL
	 */
	X11VegaWindow* GetVegaWindow() const { return m_vega_window; }

	/** 
	 * TODO: Get rid of this. Duplicates functionality elsewhere
	 */
	void SetActivePopup(X11OpWindow* pop){m_active_popup = pop;}
	
	/** 
	 * TODO: Get rid of this. Duplicates functionality elsewhere
	 */
	X11OpWindow* GetActivePopup(){return m_active_popup;}

	/**
	 * Returns the X11 window handle of this window
	 */
	X11Types::Window GetWindowHandle() const { return m_handle; }

	/**
	 * Retuns the widgets display
	 */
	X11Types::Display *GetDisplay() const { return m_display; }

	/**
	 * Returns the screen the widget displayed
	 */
	int GetScreen() const { return m_screen; }

	/** 
	 * Returns the visual used by the X11 window of the widget
	 */
	X11Types::VisualPtr GetVisual() const { return m_visual; }

	/**
	 * Returns the colormap installed on the X11 window
	 */
	X11Types::Colormap GetColormap() const { return m_colormap; }

	/**
	 * Returns the widget height. TODO: Mention decoration
	 */
	int GetHeight() const { return m_mapped_rect.height; }

	/**
	 * Returns the widget width TODO: Mention decoration
	 */
	int GetWidth() const { return m_mapped_rect.width; }

	/**
	 * Returns the widgets left border position. TODO: Mention decoration
	 * and parent relationship.
	 */
	int GetXpos() const { return m_mapped_rect.x; }

	/**
	 * Returns the widgets top border position. TODO: Mention decoration
	 * and parent relationship.
	 */
	int GetYpos() const { return m_mapped_rect.y; }

	/**
	 * Returns the drawing depth of the window
	 */
	int GetDepth() const { return m_depth; }

	/**
	 * Returns the X11OpWindow should this widget be created by 
	 * from the X11OpWindow, otherwise NULL
	 *
	 * @return The X11OpWindow. Always test for NULL
	 */
	virtual class X11OpWindow* GetOpWindow() const { return 0; }

	/**
	 * To be used by X11OpWindow with its inner/outer widget model
	 * to modify available size for the inner widget should the inner
	 * and outer widget be separate 
	 *
	 * @param width  On input, the available inner width, on return the
	 *               modified width.
	 * @param height On input, the available inner height, on return the
	 *               modified height.
	 */
	virtual void AdjustAvailableSize(UINT32& width, UINT32& height) { }

	/**
	 * Returns nearest ancestor widget that is a toplevel widget (typically with 
	 * window decorations). Note that this toplevel widget may have a parent which
	 * again is a child (or grand child etc) of a top level window. 
	 * See also @ref GetTopLevel()
	 *
	 * @return The toplevel window. Always test for NULL before using the result
	 */
	X11Widget* GetNearestTopLevel();

	/**
	 * Returns the topmost widget of an entire widget chain. This widget will not 
	 * have a parent. Use this if you have to find the widget from within multiple
	 * layers of dialogs etc. See also @ref GetNearestTopLevel()
	 *
	 * @return The toplevel window. Always test for NULL before using the result
	 */
	X11Widget* GetTopLevel();

	/**
	 * Tests if the candidate is a decendant of this widget. Using a 
	 * canidate equal to this widget will give a positive match.
	 *
	 * @param candidate The widget to test
	 *
	 * @return Returns true if some ancestor of the candidate is this
	 *         widget, otherwise false
	 */
	bool Contains(const X11Widget* candidate);

	/** Decode the key event in 'event', using the associated input
	 * context of this widget.
	 *
	 * @param event Keypress event to decode.
	 *
	 * @param keysym Keysym of the key press, NoSymbol if the event
	 * does not represent a valid keysym.  But see return value.
	 *
	 * @param text The text generated by the key press, empty if no
	 * text is generated.  But see return value.
	 *
	 * @return true on successful decoding, false on error.  If this
	 * method returns true and produces the keysym NoSymbol and an
	 * empty 'text', the key event has been handled and should be
	 * dropped.  If this method returns false, 'keysym' and 'text' are
	 * undefined.
	 */
	bool GetKeyPress(XEvent* event, KeySym& keysym, OpString& text);

	/** Activate input method (IM) for this widget with an IM listener
	  * @param listener IM listener to send IM events to
	  */
	void ActivateIM(OpInputMethodListener* listener);

	/** Deactivate input method (IM) for this widget
	  * @param listener IM listener to stop sending IM events to
	  */
	void DeactivateIM(OpInputMethodListener* listener);

	/** Abort ongoing composing in IM
	  */
	void AbortIM();

	/** Set input window
	  * @param window Window that receives user input
	  */
	void SetIMFocusWindow(X11Widget * window);

	/** Notify the platform that this widget is now ready to receive
	 * text input, so if there is an active text element, it should
	 * reactivate the input method.  This should typically be called
	 * to undo the effect of UnsetIMFocus().
	 */
	void SetIMFocusIfActive();

	/** Notify the platform that this widget is no longer receiving
	 * text input.  This should typically be called (at least)
	 * whenever the widget loses input focus.
	 */
	void UnsetIMFocus();


	X11Widget *Parent() const { return (X11Widget *)Tree::Parent(); }
	X11Widget *Suc() const { return (X11Widget *)Tree::Suc(); }
	X11Widget *Pred() const { return (X11Widget *)Tree::Pred(); }
	X11Widget *FirstChild() const { return (X11Widget *)Tree::FirstChild(); }
	X11Widget *LastChild() const { return (X11Widget *)Tree::LastChild(); }

	int GetWindowFlags() const { return m_window_flags; }
	bool IsToplevel() const { return !!m_toplevel; }
	virtual bool IsDialog() const { return false; }
	bool IsVisible();
	bool IsGrabbed() const { return m_grabbed; }
	void SetModal(bool modal) { m_window_flags = (m_window_flags & ~MODAL) | (modal ? MODAL : 0); }
	void SetAutoGrab(bool autograb) { m_window_flags = (m_window_flags & ~AUTOGRAB) | (autograb ? AUTOGRAB : 0); }
	void SetBackgroundColor(UINT32 color) { m_background_color = color; m_has_background_color=true;}

	void SetAcceptDrop(bool accept);
	bool GetAcceptDrop() const { return m_accept_drop; }

	/**
	 * Mark the widget as closed. The closed flag can never be reverted to false again. 
	 * A modal widget can only terminate its message loop if it is closed.
	 */
	void SetClosed() { m_closed = true; }

	/**
	 * Set whether a LeaveNotify event shall be propagated to the 
	 * underlying code.
	 *
	 * @param propagate If false (default mode is false for the widget) it 
	 *        only happens when the LeaveNotify happens as a result of a 
	 *        NotifyGrab or NotifyUngrab sub mode.  If true, it will happen 
	 *        for all sub modes
	 */
	void SetPropagateLeaveEvents(bool propagate) { m_propagate_leave_event = propagate; }

	void SetIcon( OpWidgetImage* image);
	void SetIcon( X11Icon* icon );
	X11Icon* GetIcon() const { return m_icon; }

	bool DispatchEvent(XEvent *event);

	/** Check if we should ignore the fact that we are executing a modal
	 * dialog, and let a widget (this widget) that isn't a part of the modal
	 * dialog dispatch an event nevertheless.
	 * An example on why we need this:
	 * Imagine a modal dialog is opened on a button click. Modality means:
	 * don't deliver input events to widgets that aren't a part of the modal
	 * dialog. However, in this case we should make an exception. We want the
	 * widget that received the button press event to also receive the
	 * corresponding button release event, even if it's not a part of the modal
	 * dialog (that was opened between the button click and button release
	 * event).
	 */
	bool IgnoreModality(XEvent *event);

	/**
	 * Mark window as closed and hide it. Widgets with a blocking message loop
	 * will terminate the loop as well.
	 */
	virtual void Close();
	virtual void Show();
	virtual void ShowBehind() {}

	/**
	 * Hide the widget. Note that a blocking message loop is not terminated.
	 * Use @ref Close() if the message loop should be terminated as well.
	 */
	virtual void Hide();
	virtual void Resize(int w, int h);
	virtual void GetSize(int *w, int *h);
	virtual void Move(int x, int y);
	virtual void GetPos(int *x, int *y);
	virtual OP_STATUS SetMinSize(int w, int h);
	virtual OP_STATUS SetMaxSize(int w, int h);
	virtual void SetGeometry(int x, int y, int w, int h);
	virtual const uni_char* GetTitle();
	virtual OP_STATUS SetTitle(const uni_char *title);
	virtual void Raise();
	virtual void Lower();
	virtual void SetWindowState(WindowState newstate);
	virtual void Grab();
	virtual void Ungrab();
	virtual void GrabMouse();
	virtual void UngrabMouse();
	virtual void GrabKeyboard();
	virtual void UngrabKeyboard();
	virtual void OnToplevelActivate() {};
	virtual void OnToplevelDeactivate() {};
	virtual BOOL ConfirmClose() { return FALSE; }
	WindowState GetWindowState() const { return m_window_state; }

	/** Equivalent to XCopyArea() with this window as both source and
	 * destination drawable.  Always use this method instead of
	 * XCopyArea() to ensure that Expose and GraphicsExpose events are
	 * handled correctly.
	 */
	void CopyArea(GC gc, int src_x, int src_y, int width, int height, int dst_x, int dst_y);

	/**
	 * Returns TRUE if this widgets exists in an environment where an compositing
	 * manager exists and is used by Opera. Note: The same answer will be returned by 
	 * all widgets on the same screen
	 */
	BOOL SupportsExternalCompositing();

	void EmulateRelease(const OpPoint& pos);

	void GetNormalSize(int *w, int *h);
	void GetNormalPos(int *x, int *y);

	/**
	 * Maximize a window. The window must be a toplevel window for this
	 * to have any effect
	 *
	 * @param state true will set the maximized property on the window,
	 *        false will remove it.
	 */
	void SetMaximized(bool state);

	/**
	 * Put the window in fullscreen mode. The window must be a toplevel window for this
	 * to have any effect
	 *
	 * @param state true will set the fullscreen property on the window,
	 *        false will remove it.
	 */
	void SetFullscreen(bool state);

	void SetShaded(bool shaded);
	void ToggleFullscreen();
	void SetBelow( bool state );
	void SetAbove( bool state );
	void SetWMSkipTaskbar( bool state );

	/**
	 * Returns the coordinate where the skin starts with respect to the top level window
	 */
	virtual OpPoint GetSkinOrigin() { return OpPoint(0,0); }

	/**
	 * Returns the coordinate where the widget starts with respect to the skin
	 */
	virtual OpPoint GetSkinOffset() { return OpPoint(m_mapped_rect.x,m_mapped_rect.y); }

	/**
	 * To be called whenever the entire application changes visibility 
	 * state. Typically when hidden or shown from the system tray icon.
	 *
	 * @param show true when the application becomes visible, otherwise false
	 */
	void OnApplicationShow(bool show);

	/**
	 * To be called whenever the entire application is to shut down
	 */
	void OnApplicationExit() { m_exit_started = true; }

	void GetDecorationOffset(int *x, int *y);

	/**
	 * Returns TRUE if it is preferred that a window drag can be 
	 * started by pressing the mouse down inside the window. If FALSE
	 * the window can only be dragged by the window manager
	 */
	virtual BOOL PreferWindowDragging() const { return FALSE; }

	/**
	 * Start moving or resizing the window using the _NET_WM_MOVERESIZE
	 * protocol. The window must be a top level window.

	 * If successful, the window manager takes charge of the operation.
	 * Opera will not receive any mouse events until the first mouse
	 * press/move after a terminating mouse up.
	 *
	 * @param action What action to start
	 * @return TRUE if a request was sent to the window manager, FALSE if
	 *         the window manager does not support _NET_WM_MOVERESIZE or if
	 *         another widget is already being dragged
	 */
	BOOL MoveResizeWindow(MoveResizeAction action);

	/**
	 * Resets the window state of a currently moved or resized window. This function
	 * should be called at the first received mouse press/move event after
	 * @ref MoveResizeWindow was called.
	 *
	 * @param inform_wm If TRUE, a _NET_WM_MOVERESIZE message is sent to the
	 *        window manager. It should only be TRUE if Opera receives a mouse
	 *        release event before a mouse press/move event as it indicates
	 *        that the window manager failed to drag the window.
	 *
	 * @return TRUE on success, FALSE if the window manager does not support
	 *         _NET_WM_MOVERESIZE or if this widger is not dragged
	 */
	BOOL StopMoveResizeWindow(BOOL inform_wm);

	/**
	 * Sets the WM_WINDOW_ROLE name of the window
	 * 
	 * @param name The text to assign to the WM_WINDOW_ROLE property
	 */
	void SetWindowRole(const char* name);
	
	/**
	 * Sets an optional name a property (OPERA_WINDOW_NAME) that is used for IPC purposes
	 *
	 * @param name The window name 
	 */
	void SetWindowName(const uni_char* name);

	/**
	 * Sets an optional name a property (OPERA_WINDOW_NAME) that is used for IPC purposes
	 * Calls @ref SetWindowName with the number appended to the string "opera"
	 *
	 * @param indentifier The number to append to "opera" 
	 */
	void SetWindowName(UINT32 indentifier);

	/**
	 * Informs the window manager that this window needs to change visual apparence to 
	 * indicate the urgency of the window. The actual change depends on the window 
	 * manager and how it is configured.
	 *
	 * @param state. If true, the window manager is asked to set urgency indication, 
	 *               otherwise to remove it
	 */
	void SetWindowUrgency(bool state);

	/**
	 * Send startup message to root window. Implements _NET_STARTUP_INFO_BEGIN protocol
	 *
	 * @param message The message to emit
	 */
	void SendStartupMessage(OpStringC8& message);

	/**
	 * Assign window list to the property OPERA_WINDOWS on the window. The list should
	 * only include window ids of currently active toplevel browser windows and shall
	 * only be set on a toplevel browser window
	 *
	 * @param list List of window ids
	 * @paran count Number of items in list
	 */
	void SetWindowList(const unsigned long* list, UINT32 count);

	/**
	 * Set the preferred type of window decoration. It can be
	 * DECORATION_SKINNED (decoration drawn by Opera) or DECORATION_WM
	 * (decoration drawn by window manager). Note that there is no
	 * guaranty a window manager will accept a DECORATION_SKINNED request
	 *
	 * @param decoration_type How to decorate the window
	 * @param verify Verify that the setting gives the best result. A user
	 *        driven setting can set this to false to let the user be in total control
	 */
	void SetDecorationType(DecorationType decoration_type, bool verify);

	/**
	 * Return the current decoration type. Note that this is the
	 * requested type made by Opera. If Opera has requested DECORATION_SKINNED
	 * that will be returned regardless of whether the window manager has accepted it
	 */
	DecorationType GetDecorationType() const
	{
		return m_toplevel ? m_toplevel->decoration_type : DECORATION_WM;
	}

	/**
	 * Called just before the decoration is changed. A derived class can reimplement
	 * this function to overrule the transition.
	 *
	 * @param decoration_type The new decoration
	 *
	 * @return The actual decoration that will be used. By default the same as spesified in the argument
	 */
	virtual DecorationType VerifyDecorationType(DecorationType decoration_type) const { return decoration_type; }

	/**
	 * Request window manager to show or hide the window decoration for a top
	 * level window. Note: The window manager can decide to ignore this request
	 *
	 * @param show Show the decoration if true, otherwise hide
	 */
	void ShowWMDecoration(bool show);

	/**
	 * Show or hide a decoration drawn by Opera itself. The implementation must
	 * be made in the widget that implements the top level window
	 *
	 * @param show Show the decoration if true, otherwise hide
	 */
	virtual void ShowSkinnedDecoration(bool show) {}

	/**
	 * Returns the "content margins" of this window.
	 *
	 * When this widget is an "outer" widget, this method returns how
	 * much space should be allocated between the "outer" widget and
	 * the "inner" widget.
	 *
	 * If this is not an "outer" widget, or if the "inner" and "outer"
	 * widgets are the same, this method returns 0 in all four
	 * arguments.
	 *
	 * Currently, only the top-level browser windows distinguish
	 * between "inner" and "outer" widgets.  For those windows, the
	 * "outer" widget is the application's top-level X11 window, while
	 * the "inner" widget is the window where html documents are
	 * painted.
	 *
	 * NOTE: The "outer" widget is what OpWindow refers to as
	 * InnerSize.
	 *
	 * The "content margin" (i.e. what this method returns) includes
	 * the menu bar, but not the window manager's decorations.
	 *
	 * When personas are active, we turn off the window manager's
	 * decorations and paint "fake" decorations ourselves.  These are
	 * inside the "outer" widget and thus also included in the
	 * "content margin" (unlike the "real" window decorations painted
	 * by the window manager, which are outside the "outer" widget).
	 *
	 * @param top Returns the space that should be between the top of
	 * the "outer" widget and the top of the "inner" widget.
	 *
	 * @param bottom Returns the space that should be between the
	 * bottom of the "outer" widget and the bottom of the "inner"
	 * widget.
	 *
	 * @param left Returns the space that should be between the left
	 * edge of the "outer" widget and the left edge of the "inner"
	 * widget.
	 *
	 * @param right Returns the space that should be between the right
	 * edge of the "outer" widget and the right edge of the "inner"
	 * widget.
	 */
	virtual void GetContentMargins(int &top, int &bottom, int &left, int &right);

	/**
	 * Update the window shape
	 */
	void UpdateShape();

	/** Disable update/repaint requests
	 * @param disable If true, all calls to @ref Update() or @ref UpdateAll()
	 * will have no effect.
	 */
	virtual void DisableUpdates(bool disable);

	/** Update (repaint) an area in the widget.
	 * The repaint will be handled asynchronously.
	 */
	virtual void Update(int x, int y, int w, int h);

	/** Update (repaint) the entire widget, and all its children.
	 * @bool now If true, do it synchronously. If false, do it asynchronously.
	 */
	virtual void UpdateAll(bool now=false);

	/** Get rid of unnecessary pending events.
	 * Go through the event queue, and see if there are any events we can
	 * (more or less) safely ignore.
	 */
	virtual void Sync();

	/**
	 * Release a mouse capture in this widget
	 */
	void ReleaseCapture();

	/** Give this widget a new parent.
	 * This will generate both DescendantRemoved() and DescandantAdded()
	 * notifications.
	 */
	void Reparent(X11Widget *parent);

	void GiveFocus();
	void RemoveFocus();
	void PointToGlobal(int *x, int *y);
	void PointFromGlobal(int *x, int *y);

	/** Get the focus proxy window for this window.  The focus proxy
	 * is the window that should have the X keyboard focus instead of
	 * this window.
	 */
	X11Widget * GetFocusProxy();

	/** Notification about a new child.
	 * Called after a new descendant has been added (this doesn't necessarily
	 * mean that the widget has just been created - it may have been
	 * reparented into this hierarchy, and actually created a long time ago).
	 * Default implementation is to just propagate this to the ancestor, and
	 * this should always be done (unless you really know that no ancestor is
	 * interested in this notification), no matter what extra stuff you want
	 * to do.
	 */
	virtual void DescendantAdded(X11Widget *child);

	/** Notification that a child is to be removed.
	 * Called before a descendant is removed (this doesn't necessarily mean
	 * that the widget is to be destroyed - it may be reparented out from this
	 * hierarchy).
	 * Default implementation is to just propagate this to the ancestor, and
	 * this should always be done (unless you really know that no ancestor is
	 * interested in this notification), no matter what extra stuff you want
	 * to do.
	 */
	virtual void DescendantRemoved(X11Widget *child);

	/** Called when 'w' is being destroyed.  'w' may already be
	 * partially destroyed when this method is called, so it is not
	 * safe to access 'w' within this method (or at any time later).
	 */
	virtual void OnVEGAWindowDead(VEGAWindow * w);

	/** Handle an X event.
	 * @return true if the event should be considered as handled/eaten by this
	 * call, or false otherwise
	 */
	virtual bool HandleEvent(XEvent *event);

	/** Handle Expose and GraphicsExpose events.
	 */
	bool HandleExposeEvent(XExposeEvent * ev);

	virtual void HandleResize() { m_pending_resize = false; }

	/**
	 * Register input event listener. If set and listener consumes event
	 * no input event is sent to the widget or core. We only allow one 
	 * listener pr widget
	 *
	 * @param listener The input listener
	 */
	void SetInputEventListener(InputEventListener* listener) { m_input_event_listener = listener; }

	/**
	 * Returns the current input event listener.
	 */
	InputEventListener* GetInputEventListener() const { return m_input_event_listener; }

	/**
	 * Adds general widget listener. 
	 *
	 * @param listener The listener
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS AddX11WidgetListener(X11WidgetListener* listener) { return m_x11widget_listeners.Add(listener); }

	/**
	 * Removes general widget listener
	 *
	 * @param listener The listener
	 */
	void RemoveX11WidgetListener(X11WidgetListener* listener) {m_x11widget_listeners.Remove(listener);}

	/**
	 * Sets displayed mouse cursor shape
	 */
	void SetCursor(unsigned int cursor);
	
	/**
	 * Saves the active mouse cursoe shape as set by @ref SetCursor
	 */
	void SaveCursor();

	/**
	 * Sets the displayed mouse cursor shape by using the shape saved
	 * in @ref SaveCursor
	 */
	void RestoreCursor();

	/**
	 * Updates current cursor shape
	 */
	void UpdateCursor();

	/**
	 * Sets the drop offset
	 * 
	 * @param point The drop offset
	 */
	void SetDropOffset(const OpPoint& point) { m_drop_offset.Set(point.x,point.y); }

	/**
	 * Returns the drop offset
	 */
	const OpPoint& GetDropOffset() const { return m_drop_offset; }

	/**
	 * Returns the OpView located a the specific location.
	 */
	class OpView* GetOpView(int ix, int iy, int& ox, int& oy);

	bool IsModal() const { return m_modal_executing; }

	/** Return active modal widget.
	 * In other words: the topmost element on the modal widgets stack)
	 */
	static X11Widget* GetModalWidget();

	/**
	 * Returns the active grabbed widget if any. This is typically 
	 * a popup menu or a dropdown window 
	 */
	static X11Widget* GetGrabbedWidget();

	/**
	 * Returns the active popup widget if any.
	 */
	static X11Widget* GetPopupWidget();

	/**
	 * Return the topmost (z-order) document desktop window that holds the given coordinates
	 * The coordinates should be relative to the mde view
	 *
	 * @param view The mde view that contains other views (which again contain the DocumentDesktopWindow)
	 * @param x X coordinate
	 * @param y Y coordinate
	 */
	static DocumentDesktopWindow* GetDocumentDesktopWindow(MDE_View* v, int x, int y);

	/**
	 * Return the document desktop window (if any) where the current drag started. NULL
	 * is always returned after the mouse button that activated the drag is released.
	 */
	static DocumentDesktopWindow* GetDragSource();

	/**
	 * Debug option. Not for regular code!
	 */
	static void DisableGrab(bool disable_grab) { m_disable_grab = disable_grab; }

	// Implementation of X11CbObject:
	virtual void HandleCallBack(INTPTR ev);

protected:
	/**
	 * Removes all events of the specified type sent to the widget's
	 * window, and returns the last event found.
	 *
	 * @param e Returns the last event removed.  If no suitable event
	 *     is found, e will be left untouched.  Note that if an event
	 *     is found, e will be set to point to statically allocated,
	 *     global data.  So don't delete it and don't use it beyond
	 *     the next time Compress() is called on any X11widget.
	 *
	 *     It is valid to pass a null pointer as this parameter.
	 *
	 * @param type The event type to look for.
	 */
	void CompressEvents(XEvent*& e, int type);

private:
	OP_STATUS InitToplevel();
	OP_STATUS CreateSizeHintsStruct();
	void UpdateWmData();

	/**
	 * When creating an XImage to be used with this window
	 * (i.e. compatible with this window's visual), the value returned
	 * from this method is a suitable number of bits to allocate for
	 * each pixel.  (The number of bits of actual data in each pixel
	 * is GetDepth(), this is how much space each pixel occupies in
	 * memory.)
	 */
	int GetImageBPP();

	/**
	 * Queries system for widget rect using global coordinates.
	 * Works only for toplevel widgets.
	 *
	 * @param rect size and position of the widget
	 */
	void GetToplevelRect(OpRect & rect);

	/**
	 * Saves the size and position of the widget as reported by the window
	 * manager. The values are not available to ref @GetHeight() ref @GetWidth()
	 * ref @GetXpos and ref @GetYpos() until the widget becomes visible (gets mapped)
	 *
	 * @param rect The new size and position of the widget
	 */
	void SetToplevelRect(const OpRect& rect);

	/**
	 * Ask Window Manager for width of decoration borders.
	 * This will work even for managers, that don't reparentify
	 * toplevel windows (e.g. compiz < 0.9).
	 *
	 * @param rect On return the reported sizes as a rectangle where
	 *             OpRect::x      - left border size
	 *             OpRect::y      - top border size
	 *             OpRect::width  - right border size
	 *             OpRect::height - bottom border size
	 *
	 * @return TRUE on success, otherwise FALSE. Rectangle contains random
	 *         data on FALSE
	 */
	BOOL GetFrameExtents(OpRect& rect);

	/**
	 * Ask window manager for the window id of the decoration window that 
	 * exists if the window manager decorates the client window without 
	 * reparenting it. Composite enabled window managers can do it this way.
	 *
	 * @param window On return, the window id
	 *
	 * @return TRUE on success, otherwise FALSE. The window contains random
	 *         data on FALSE
	 */
	BOOL GetFrameWindow(X11Types::Window& window);

	/**
	 * Activate the DesktopWindow at the given coordinate (in the window)
	 * if it is not already active.
	 *
	 * @param x Local x coordinate
	 * @param y Local y coordinate
	 */
	void FindAndActivateDesktopWindow(int x, int y);

	/**
	 * Set or remove the top level window shape. The shape consists of the full area
	 * of the window except for rounded corners when set, otherwise all is visible
	 *
	 * @param set Use shape if true, otherwise false
	 */
	void SetShape(bool set);

	/**
	 * Called on MapNotify and UnmapNotify. Ensures widget and window share
	 * the same visibility state.
	 */
	void OnVisibilityChange();

	/**
	 * Called whenever @ref SetDecorationType changed the decoration type
	 */
	virtual void OnDecorationChanged() {}

	/**
	 * Called whenever the window state has changed
	 *
	 * @param state The new window state
	 */
	virtual void OnWindowStateChanged(WindowState state) {}

protected:
	X11Types::Display* m_display;
	X11Types::VisualPtr m_visual;
	X11Types::Colormap m_colormap;
	X11Types::Window m_handle;
	int m_screen;

private:
	X11InputContext* m_input_context;
	X11MdeScreen* m_mde_screen;
	X11MdeBuffer* m_mde_buffer;
	X11VegaWindow* m_vega_window;
	X11OpWindow* m_active_popup;
	X11WidgetList::Element m_list_element;
	OpString m_title;
	XSizeHints* m_sizehints;
	ToplevelData* m_toplevel;
	InputEventListener* m_input_event_listener;
	OpListeners<X11WidgetListener> m_x11widget_listeners;
	X11Icon* m_icon;
	WindowState m_window_state;
	unsigned int m_active_cursor;
	unsigned int m_saved_cursor;
	INT32 m_background_color;
	int m_window_flags; // WindowFlag values OR'ed together
	OpRect m_mapped_rect;
	OpRect m_unmapped_rect;
	OpPoint m_drop_offset;
	DragHotspot m_drag_hotspot;
	int m_depth;
	int m_expecting_buttonrelease;
	enum RenderingAPI
	{
		API_UNDECIDED,
		API_X11,
		API_OPENGL
	};
	RenderingAPI m_rendering_api;
	bool m_no_update:1;
	bool m_visible:1;
	bool m_grabbed:1;
	bool m_destroyed:1;
	bool m_modal_executing:1;
	bool m_pending_resize:1;
	bool m_has_background_color:1;
	bool m_owns_colormap:1;
	bool m_owns_mde:1;
	bool m_accept_drop:1;
	bool m_dragging_enabled:1;
	bool m_propagate_leave_event:1;
	bool m_shaded:1;
	bool m_shaded_in_hide:1;
	bool m_skip_taskbar:1;
	bool m_exit_started:1;
	bool m_closed:1;
	bool m_show_on_application_show:1;
	bool m_has_set_decoration:1;

private:
	static bool m_disable_grab;
	static WindowDrag m_window_drag;
	static RenderingAPI m_core_rendering_api;

	class ScrollData
	{
	public:
		ScrollData()
			: m_next(0)
			{
			};

		void Set(int src_x, int src_y,
				 int width, int height,
				 int dst_x, int dst_y,
				 unsigned long before,
				 unsigned long after)
			{
				m_src_x = src_x;
				m_src_y = src_y;
				m_width = width;
				m_height = height;
				m_dst_x = dst_x;
				m_dst_y = dst_y;
				m_serial_before = before;
				m_serial_after = after;
			};

		int m_src_x;
		int m_src_y;
		int m_width;
		int m_height;
		int m_dst_x;
		int m_dst_y;
		unsigned long m_serial_before;
		unsigned long m_serial_after;
		ScrollData * m_next;
	};

	ScrollData * m_recent_scrolls;
	/* This one must always point to a valid object (as long as
	 * m_recent_scrolls is not 0).  Unfortunately, this mean that the
	 * last element in the list can never be in use.
	 */
	ScrollData * m_recent_scrolls_first_unused;
};

#endif // __X11_WIDGET_H__
