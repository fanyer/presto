/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPWINDOW_H
#define OPWINDOW_H

#include "modules/display/cursor.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/OpTypedObject.h"
#include "modules/util/opstring.h"

class OpRect;
class OpView;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
// Defined in the accessibility module
class OpAccessibleItem;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

/** Window listener. Recepient of window events. */
class OpWindowListener
{
public:
	virtual ~OpWindowListener() {};

	/** The window size has changed. */
	virtual void OnResize(UINT32 width, UINT32 height) {}

	/** The window position has changed. */
	virtual void OnMove() {}

	/** The size of the rendering buffer has changed.
	 *
	 * It is only necessary to call this method if the rendering buffer size
	 * differs from the window size.
	 *
	 * @see OpWindow::GetRenderingBufferSize()
	 */
	virtual void OnRenderingBufferSizeChanged(UINT32 width, UINT32 height) {}

	/** The window active state has changed.
	 *
	 * @param active If TRUE, the window just became active. If FALSE,
	 * the window just became inactive.
	 */
	virtual void OnActivate(BOOL active) {}

	/** Is it allowed to close this window?
	 *
	 * Called when trying to close the window.
	 *
	 * @return TRUE if the window may be closed, FALSE if not. This is
	 * typically used to prevent the user from closing windows that
	 * are not supposed to be closed (at the moment).
	 */
	virtual BOOL OnIsCloseAllowed() { return TRUE; }

	/** Is the window to be closed/destroyed asynchronously?
	 *
	 * Called when trying to close the window.
	 *
	 * @return TRUE if the window is to be closed asynchronously,
	 * meaning that a message should be posted, and the window should
	 * be closed when handling the message. Good in situations when a
	 * window is to be closed and still have itself/childstuff on the
	 * stack. FIXME: I don't see how this solves any problems
	 * properly. The event loop may be nested, and thus the delayed
	 * close message posted may be handled while the bad stuff is
	 * still on the stack. Oh, and nobody calls this method.
	 */
	virtual BOOL OnNeedPostClose() { return FALSE; }

	/** The window is being closed, by Opera itself, or by the user.
	 *
	 * Must not be called if OnIsCloseAllowed() returns FALSE.
	 *
	 * @param user_initiated TRUE if the window was closed by the
	 * user, FALSE if closed by Opera itself.
	 */
	virtual void OnClose(BOOL user_initiated) {}

	/** Visibility state of the window has changed.
	 *
	 * @param show If TRUE, the window has become visible (shown). If
	 * FALSE, the window has become invisible (hidden).
	 */
	virtual void OnShow(BOOL show) {}

	/** Files have been dropped on the window. */
	virtual void OnDropFiles(const OpVector<OpString>& file_names) {}

	/** The window has been covered by another window or is no
	 * longer covered by another window.
	 *
	 * @param vis TRUE if the window is now visible, FALSE if it is
	 * completly covered by another window.
	 */
	virtual void OnVisibilityChanged(BOOL vis) {}
};

#ifdef PI_ANIMATED_WINDOWS
/** @short State of an animated window (position, size, opacity). */
class OpAnimatedWindowState
{
public:
	OpAnimatedWindowState() : opacity(0.0) { }
	OpAnimatedWindowState(OpRect rect, float opacity) : rect(rect), opacity(opacity) { }

	/** The rectangle (position and stretch) of the window. */
	OpRect rect;

	/** The opacity of the window. 0.0 = fully transparent, 1.0 = fully opaque */
	float opacity;
};

/** @short Window animation.
 *
 * This class is used to describe an animation (transition of position, size
 * and opacity) of an OpWindow.
 */
class OpWindowAnimation
{
public:
	OpWindowAnimation() : duration_ms(0) { }
	OpWindowAnimation(const OpAnimatedWindowState& before, const OpAnimatedWindowState& after, unsigned int duration_ms)
		: before(before),
		  after(after),
		  duration_ms(duration_ms) { }

	/** The initial state of the window. */
	OpAnimatedWindowState before;

	/** The final state of the window. */
	OpAnimatedWindowState after;

	/** The duration of the animation, in milliseconds. */
	unsigned int duration_ms;
};

/** @short Listener for window animation progress. */
class OpWindowAnimationListener
{
public:
	virtual ~OpWindowAnimationListener() { }

	/** @short The requested window animation has finished. */
	virtual void OnAnimationFinished(class OpWindow* window) = 0;
};
#endif // PI_ANIMATED_WINDOWS

/** A window.
 *
 * Parent of at most one OpWindow or OpView.
 * Child of another OpWindow or nobody (top-level).
 *
 * FIXME: This API is a mess.
 */
class OpWindow
{
public:

	/** Special effect flags. */
	enum Effect
	{
		/** No special effect. */
		EFFECT_NONE = 0x00,

		/** Draw the window with an operating system specific drop-shadow. */
		EFFECT_DROPSHADOW = 0x01,

		/** The window should have per-pixel transparency as described in
		 * STYLE_TRANSPARENT. Needs to work with borderless windows, such
		 * as STYLE_POPUP, STYLE_TOOLTIP and STYLE_NOTIFIER. */
		EFFECT_TRANSPARENT = 0x02
	};

	/** Window type/style. */
	enum Style
	{
		/** Unknown style. Don't use. */
		STYLE_UNKNOWN,

		/** A typical desktop window with borders that can be resized,
		 * maximized, minimized etc. QUICK expects to be able to
		 * create STYLE_DESKTOP windows as children of a
		 * STYLE_WORKSPACE window, as well as being top-level windows.
		 */
		STYLE_DESKTOP,

		/** A popup window without borders. Appears on top of everything.
		 *
		 * Popup windows should never be activated.
		 *
		 * When a popup window is open, the following kind of events
		 * should cause the popup to be automaticly closed and
		 * destroyed:
		 *   - Another STYLE_POPUP window is created
		 *   - User clicks somewhere outside the popup
		 */
		STYLE_POPUP,

#ifdef EXTENSION_SUPPORT
		/** A window without borders. Appears on top of everything.
		 *
		 * Extension Popup windows should never be activated.
		 *
		 */
		STYLE_EXTENSION_POPUP,
#endif // EXTENSION_SUPPORT

		/** A border-less window, very similar to STYLE_POPUP, but
		 * unlike STYLE_POPUP, it should never be closed
		 * automaticly. It should never receive input events.
		 */
		STYLE_TOOLTIP,

		/** A border-less window, very similar to STYLE_POPUP, but
		 * unlike STYLE_POPUP, it should never be closed
		 * automaticly. It should receive input events.
		 */
		STYLE_NOTIFIER,

		/** A modal dialog window. Dialog windows can be resized, but
		 * not maximized or minimized.
		 *
		 * Dialogs should preferably appear on top of their parent
		 * (i.e., if you click in the parent - or activate another
		 * application and then click on the parent window of the
		 * dialog, the dialog should still be there on top of the
		 * parent window)
		 *
		 * A modal dialog window blocks all input (mouse and keyboard)
		 * to other windows in the application, including the parent
		 * window.
		 */
		STYLE_MODAL_DIALOG,

		/** A modeless dialog window. Dialog windows can be resized,
		 * but not maximized or minimized.
		 *
		 * Dialogs should preferably appear on top of their parent
		 * (i.e., if you click in the parent - or activate another
		 * application and then click on the parent window of the
		 * dialog, the dialog should still be there on top of the
		 * parent window)
		 *
		 * A modeless dialog window allows input (mouse and keyboard)
		 * to other windows even while it is open.
		 */
		STYLE_MODELESS_DIALOG,

		/** A blocking dialog window. Dialog windows can be resized,
		 * but not maximized or minimized.
		 *
		 * Dialogs should preferably appear on top of their parent
		 * (i.e., if you click in the parent - or activate another
		 * application and then click on the parent window of the
		 * dialog, the dialog should still be there on top of the
		 * parent window)
		 *
		 * A blocking dialog window is like a modal dialog window, but
		 * in addition, calling SetDesktopPlacement() starts a nested
		 * event loop which should not return until the dialog window
		 * is closed.
		 */
		STYLE_BLOCKING_DIALOG,

		/** A console dialog window. May not be maximized.
		 *
		 * Dialogs should preferably appear on top of their parent
		 * (i.e., if you click in the parent - or activate another
		 * application and then click on the parent window of the
		 * dialog, the dialog should still be there on top of the
		 * parent window)
		 *
		 * The console dialog acts as a modeless dialog, except that
		 * it may be minimized.
		 */
		STYLE_CONSOLE_DIALOG,

		/** Child window. To be embedded inside an OpView.
		 *
		 * It should have no borders. It can never be activated. The
		 * user is provided with no means of modifying its size,
		 * position, state. This is to be handled by an ancestor
		 * window, if desirable.
		 */
		STYLE_CHILD,

		/** Window container or workspace.
		 *
		 * It is a container for a set of STYLE_DESKTOP windows.
		 *
		 * The appearance and behavior is mostly platform specific. It
		 * should provide the user with mechanisms of resizing,
		 * moving, closing, minimizing, maximizing and restoring
		 * STYLE_DESKTOP windows. It may also display icons for
		 * minimized windows.
		 */
		STYLE_WORKSPACE,

		/** FIXME: DOC */
		STYLE_EMBEDDED,

		/** Window based on a native platform window handle.
		 *
		 * The native window handle is passed as a parameter to
		 * Init(). It should simply be wrapped in an OpWindow object,
		 * essentially to allow for the native window to be a parent
		 * for other OpWindow or OpView objects.
		 *
		 * In general, no other calls than Init() makes any sense and
		 * should not be supported. Deleting an OpWindow like this
		 * does not affect the native window.
		 *
		 * The native window must exist for the entire lifetime of the
		 * class.
		 */
		STYLE_ADOPT,

		/** Window with per-pixel alpha transparency.
		 *
		 * The window has no decoration.
		 *
		 * The default background is transparent, and the window
		 * supports per-pixel alpha transparency, meaning that the
		 * window (or the desktop) behind, window-stack-wise, may be
		 * (fully or partially) visibile through such a window. Pixels
		 * with alpha=0 shall not take any input.
		 */
		STYLE_TRANSPARENT,

		/** Opera widget/gadget.
		 *
		 * This is like STYLE_TRANSPARENT, but the window is draggable
		 * in addition.
		 *
		 * Dragability can be already handled by core, but dragging of
		 * toplevel windows should be delegated to OS window manager,
		 * not handled by opera internally.
		 */
		STYLE_GADGET,

		/** Opera widget/gadget with native window decorations.
		 *
		 * Window looks like any other gadget from core or
		 * quick perspective, yet it is decorated, like any
		 * other window.
		 */
		STYLE_DECORATED_GADGET,

		/**
		 * Core requires gadgets to have OpWindow, even if it
		 * should never be displayed (e.g. when gadget is
		 * extension or unite).
		 *
		 * Window should be completely invisible, without
		 * possibility of making it visible.
		 *
		 * This style will be removed as soon as gadgets won't
		 * require OpWindow to run properly.
		 */
		STYLE_HIDDEN_GADGET,

		/** Transparent overlay window.
		 *
		 * Normally used in full screen only. It is used to visualize
		 * 'a drawer' for widgets/gadgets.
		 *
		 * Currently unused.
		 */
		STYLE_OVERLAY,

#ifdef PI_ANIMATED_WINDOWS
		/** Window suitable for transformations and animations.
		 *
		 * This window has no decoration, and it supports per-pixel alpha
		 * transparency. The default background is transparent.
		 *
		 * This style is very similar to STYLE_TRANSPARENT, but in addition it
		 * may call the SetTransformation() method. It may animate certain
		 * operations, such as Show() (i.e. fade in or out).
		 */
		STYLE_ANIMATED,
#endif // PI_ANIMATED_WINDOWS

		/** Non-platform window, with bitmap buffers as a drawind area
		* 
		* A virtual window which does not have platform window representation
		* It might use bitmap as an screen painting buffer
		*/
		STYLE_BITMAP_WINDOW,
	};

	/** Window state.
	 *
	 * Only relevant to certain window types/styles.
	 */
	enum State
	{
		RESTORED,
		MINIMIZED,
		MAXIMIZED,
		FULLSCREEN
	};

	/** Create a window */
	static OP_STATUS Create(OpWindow** new_opwindow);

	virtual ~OpWindow() {};

	/** Second-phase constructor.
	 *
	 * Must be run to initiate the object.
	 *
	 * @param style Style/type of window.
	 * @param type Window sub-type mess. Most importantly, it may
	 * differ if style is STYLE_DESKTOP: If it is
	 * OpTypedObject::WINDOW_TYPE_BROWSER, it will be a top-level
	 * window. Otherwise it will be embedded inside a window of type
	 * STYLE_WORKSPACE.
	 * @param parent_window OpWindow parent of this window, or NULL if
	 * parent isn't an OpWindow, or if there is no parent. Must be
	 * NULL if parent_view isn't.
	 * @param parent_view OpView parent of this window, or NULL if
	 * parent isn't an OpView, or if there is no parent. Must be NULL
	 * if parent_window isn't.
	 * @param native_handle Native window handle. Non-NULL if style is
	 * STYLE_ADOPT.
	 * @param effect Special effects to apply to this window - members
	 * of the 'Effect' enum OR'ed together.
	 */
	virtual OP_STATUS Init(Style style = STYLE_DESKTOP, OpTypedObject::Type type = OpTypedObject::WINDOW_TYPE_UNKNOWN, OpWindow* parent_window = NULL, OpView* parent_view = NULL, void* native_handle = NULL, UINT32 effect = EFFECT_NONE) = 0;

	/** Reparent the window. */
	virtual void SetParent(OpWindow* parent_window = NULL, OpView* parent_view = NULL, BOOL behind = FALSE) = 0;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	/** Set the accessibility item associated to the window
	 *
	 * Sets the accessible item for which an adapter should be
	 * requested when the bootstrapping of the accessible
	 * hierarchy is requested.
	 */
	virtual void SetAccessibleItem(OpAccessibleItem* accessible_item) {}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	/** Get the parent.
	 *
	 * @return The parent, or NULL if there is no parent or the parent
	 * is not an OpWindow.
	 */
	virtual OpWindow* GetParentWindow() = 0;

	/** Get the parent.
	 *
	 * @return The parent, or NULL if there is no parent or the parent
	 * is not an OpView.
	 */
	virtual OpView* GetParentView() = 0;

	/** Get the top-level window ancestor of this window.
	 *
	 * Is able to go through any OpView objects in the ancestor
	 * chain. It is needed to go from any OpWindow/OpView to the
	 * OpWindow defining the UI, where there are no parents.
	 *
	 * @return The top-level window, or NULL if unsupported.
	 */
	virtual OpWindow* GetRootWindow() { return NULL; }

	/** Set the the listener which will receive events from this window.
	 *
	 * NOTE: The listener must be deleted by the UI.
	 */
	virtual void SetWindowListener(OpWindowListener *windowlistener) = 0;

#ifdef OPWINDOW_DESKTOP_FUNCTIONS
	/** Set the desktop placement of the window (with STYLE_DESKTOP).
	 *
	 * Placement is defined to be both position, size and state. The
	 * size and position is for the RESTORED state, so calling it with
	 * state set to MAXIMIZED means that the window should be
	 * maximized but if the user restores the window, it should
	 * restore to the positions passed.
	 *
	 * In other words, this is intended to be called only once per
	 * window, to set the initial position of a desktop window, and it
	 * is therefore defined that this call should also SHOW the
	 * window.
	 *
	 * The inner flag specifies if the restored rect is the desired
	 * inner pos and size
	 *
	 * The behind flag specifies that it should open behind other
	 * windows. (for the typical "Open link in background" operation)
	 *
	 * The center flag specifies that the window should be centered to
	 * the parent (or screen if no parent) (this was previously done
	 * in QUICK itself, but there appeared some problems with that
	 * method, so now the platform should do it)
	 */
	virtual void SetDesktopPlacement(const OpRect& rect, State state, BOOL inner = FALSE, BOOL behind = FALSE, BOOL center = FALSE) = 0;

	/** Get the desktop placement of the window.
	 *
	 * Used primarily when window is closing and we want to store the state to preferences.
	 *
	 * THIS METHOD WILL BE MOVED OUT OF THIS INTERFACE!!!
	 * Any year soon. Maybe.
	 *
	 * @param rect the outer rectangle of the window
	 * @param state the state to get the placement for
	 */
	virtual void GetDesktopPlacement(OpRect& rect, State& state) = 0;

	/** Get the state that a window shall enter when activated from
	 * minimized state. */
	virtual State GetRestoreState() const = 0;

	/** Set the state that a window shall enter when activated from
	 * minimized state.
	 *
	 * The window state shall not change as a direct result of this
	 * call.
	 */
	virtual void SetRestoreState(State state) = 0;

	/** Set the window state to restored. */
	virtual void Restore() = 0;

	/** Set the window state to minimized. */
	virtual void Minimize() = 0;

	/** Set the window state to maximized. */
	virtual void Maximize() = 0;

	/** Set the window state to full-screen. */
	virtual void Fullscreen() = 0;

	/** Attempt to prevent the window from being updated.
	 *
	 * Initially it is not locked.
	 *
	 * @param lock If TRUE, lock for updates. This means that the
	 * platform will attempt to ignore repaint request from both Opera
	 * itself, and from the windowing system. If FALSE, handle repaint
	 * requests normally.
	 */
	virtual void LockUpdate(BOOL lock) = 0;

	/** Set the floating state (always-on-top) of this window.
	 *
	 * Initially, a window is not floating; i.e. it is in the normal
	 * window stack.
	 *
	 * @param floating if TRUE, the window becomes floating, meaning
	 * that it is always on top of all other windows that are in the
	 * normal window stack and always-below window stack. If FALSE,
	 * the window lives in the normal window stack.
	 */
	virtual void SetFloating(BOOL floating) = 0;

	/** Set the always-below state of this window.
	 *
	 * Initially, a window is not always-below; i.e. it is in the
	 * normal window stack.
	 *
	 * @param below if TRUE, the window is below all other windows
	 * that are in the normal window stack and floating window
	 * stack. If FALSE, the window lives in the normal window stack.
	 */
	virtual void SetAlwaysBelow(BOOL below) = 0;

	/** Specify if this window is to be visible in the window list.
	 *
	 * Most operating systems or window managers have some mechanism
	 * of presenting the user with the currently open windows
	 * (taskbar, Alt-Tab window cycler, etc.). This method is used to
	 * toggle the presence of this window in such a list.
	 *
	 * @param in_window_list Included in the window list if TRUE,
	 * exluded if FALSE.
	 */
	virtual void SetShowInWindowList(BOOL in_window_list) = 0;
#endif // OPWINDOW_DESKTOP_FUNCTIONS

	/** Show or hide the window.
	 *
	 * @param show Show the window if TRUE, hide it if FALSE.
	 */
	virtual void Show(BOOL show) = 0;

	/** Give or take away focus.
	 *
	 * @param focus Give focus to the window if TRUE, take it away if FALSE.
	 */
	virtual void SetFocus(BOOL focus) = 0;

	/** Close the window and unload it from the system. */
	virtual void Close(BOOL user_initiated = FALSE) = 0;

	/** Get the window outer size (including decorations). */
	virtual void GetOuterSize(UINT32* width, UINT32* height) = 0;

	/** Set the window outer size (including decorations). */
	virtual void SetOuterSize(UINT32 width, UINT32 height) = 0;

	/** Get the window inner size (excluding decorations). */
	virtual void GetInnerSize(UINT32* width, UINT32* height) = 0;

	/** Set the window inner size (excluding decorations). */
	virtual void SetInnerSize(UINT32 width, UINT32 height) = 0;

	/** Get the window outer position (including decorations). */
	virtual void GetOuterPos(INT32* x, INT32* y) = 0;

	/** Set the window outer position (including decorations). */
	virtual void SetOuterPos(INT32 x, INT32 y) = 0;

	/** Get the window inner position (excluding decorations). */
	virtual void GetInnerPos(INT32* x, INT32* y) = 0;

	/** Set the window inner position (excluding decorations). */
	virtual void SetInnerPos(INT32 x, INT32 y) = 0;

	/** Get the size of the rendering buffer.
	 *
	 * This is what core bases the size of the rendering viewport on (i.e. the
	 * OpView child of this OpWindow). Note that this method works on so-called
	 * screen coordinates, while the rendering viewport and other viewports in
	 * OpViewportController and OpViewportListener work on so-called document
	 * coordinates (where the difference is that document coordinates are
	 * affected by the internal zoom level in core (controlled by
	 * OpWindowCommander::SetScale() or
	 * OpViewportController::SetRenderingScale()), while screen coordinates
	 * don't). Coordinates and dimensions used in pi classes work on screen
	 * coordinates. Coordinates and dimensions used in windowcommander work on
	 * document coordinates.
	 *
	 * This size may be different to GetInnerSize() (typically larger) when using
	 * the viewport API in windowcommander (OpViewportController and
	 * OpViewportListener), so that the rendering viewport may be zoomed and
	 * panned on the platform side.
	 *
	 * What size to use is entirely up to the platform. It typically depends on
	 * how much memory the device has, and characteristics of the graphics
	 * system on the device. If the size changes,
	 * OpWindowListener::OnRenderingBufferSizeChanged() is to be called.
	 *
	 * If the platform is not going to use the windowcommander viewport API,
	 * this method should return the same size as for GetInnerSize().
	 */
	virtual void GetRenderingBufferSize(UINT32* width, UINT32* height) = 0;

	/** Set the minimum inner size of the window (excluding decorations).
	 *
	 * The window may not get a smaller size than this.
	 */
	virtual void SetMinimumInnerSize(UINT32 width, UINT32 height) = 0;

	/** Sets the maximum inner size of the window (excluding decorations).
	 *
	 * The window may not get a larger size than this.
	 */
	virtual void SetMaximumInnerSize(UINT32 width, UINT32 height) = 0;

	/** Get the window margins.
	 *
	 * The only purpose of this call is really to be able have
	 * seamless skin all over the window, including a menubar, which
	 * is created by the platform implemenation. DesktopWindow needs
	 * to know about such margins the window may have and expand the
	 * skin area of the root widget to "include it". This will only
	 * work if the OpWindow implmentation also obeys the SetSkin()
	 * call; otherwise it can just return 0,0,0,0
	 */
	virtual void GetMargin(INT32* left_margin, INT32* top_margin, INT32* right_margin, INT32* bottom_margin) = 0;

	/** Request a transparency level for the whole window.
	 *
	 * It is not required that this be implemented.
	 *
	 * @param transparency Transparency level, a value between 0
	 * (fully transparent) and 255 (fully opaque).
	 */
	virtual void SetTransparency(INT32 transparency) = 0;

	/** Specify if child desktop windows should be visible when minimized.
	 *
	 * This option is only used for STYLE_WORKSPACE windows. The idea
	 * is that if the pagebar is turned off, one should see minimized
	 * windows
	 *
	 * @param workspace_minimized_visibility If TRUE, minimized
	 * windows should be visible (as icons, typically) in the
	 * workspace window.
	 */
	virtual void SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility) = 0;

	/** Set menubar visibility (if any in the window). */
	virtual void SetMenuBarVisibility(BOOL visible) {}

	/** Get menubar visibility.
	 *
	 * @return TRUE if visible. FALSE if hidden or none.
	 */
	virtual BOOL GetMenuBarVisibility() { return TRUE; }

	/** Update the font in the window.
	 *
	 * Only used for updating special fonts like the toolbar font etc.
	 */
	virtual void UpdateFont() {}

	/** Update the translated text in the window.
	 *
	 * Only used for special windows like the toolbar etc.
	 */
	virtual void UpdateLanguage() {}

	/** Update the menu bar in the window.
	 *
	 * Only used for special windows.
	 */
	virtual void UpdateMenuBar() {}

	/** Set the skin.
	 *
	 * Tell OpWindow what kind of skin DesktopWindow will use through
	 * OpSkinManager on this window.
	 */
	virtual void SetSkin(const char* skin) = 0;

	/** Redraw window. Needed because color scheme might have changed */

	virtual void Redraw() = 0;

	/** Get the Style of a window, used to style a new window with the style of opener if defined,
		return STYLE_UNKNOWN if the window doesn't retain information on style setting */
	virtual Style GetStyle() = 0;

	/** Raise the window.
	 *
	 * Raise the window to the top of the window stack to which it
	 * currently belongs.
	 */
	virtual void Raise() = 0;

	/** Lower the window.
	 *
	 * Lower the window to the bottom of the window stack to which it
	 * currently belongs.
	 */
	virtual void Lower() = 0;

	/** Raise and activate the window. */
	virtual void Activate() = 0;

	/** Deactivate the window if it was active.
	 *
	 * This activates no other windows, but sends proper
	 * OpWindowListener::OnActivate(FALSE) to window being
	 * deactivated. This is used by QUICK to deactivate an active
	 * STYLE_DESKTOP window inside a STYLE_WORKSPACE so that all
	 * windows in the workspace appear deactivated.
	 */
	virtual void Deactivate() = 0;

	/** The window wants user attention.
	 *
	 * Typically do nothing if this is the active window, or else
	 * flash its taskbar button or something.
	 */
	virtual void Attention() = 0;

#ifndef MOUSELESS
	/** Set the current cursor */
	virtual void SetCursor(CursorType cursor) = 0;

	/** Get the current cursor */
	virtual CursorType GetCursor() = 0;
#endif // !MOUSELESS

	/** Get the window title. */
	virtual const uni_char* GetTitle() const = 0;

	/** Set the window title. */
	virtual void SetTitle(const uni_char* title) = 0;

	DEPRECATED(virtual void SetIcon(class OpWidgetImage*));

	/** Set the window icon. */
	virtual void SetIcon(class OpBitmap* bitmap) { }

	/** Instruct this window to be opened in the background.
	 *
	 * Special function for workspace windows to allow opening a
	 * childwindow in the background. You will probably not need this
	 * one unless you deal with Qt. [espen 2003-01-27]
	 */
	virtual void SetOpenInBackground(BOOL state) { (void)state; }

	/** Set a resource name on the window.
	 *
	 * In X-windows the resource name is used by some window managers
	 * to track what windows that belong together. You can probably
	 * ignore to reimplement this function unless you use X-windows
	 * [espen 2003-02-25]
	 *
	 * @param name The resource name
	 */
	virtual void SetResourceName(const char *name) { (void)name; }

#ifdef OPWINDOW_TOPMOST_WINDOW
	/** Is this window, or its ancestor, topmost and active? */
	virtual BOOL IsActiveTopmostWindow() = 0;
#endif // OPWINDOW_TOPMOST_WINDOW

#ifdef PI_ANIMATED_WINDOWS
	/** @short Set listener for window animation events. */
	virtual void SetWindowAnimationListener(OpWindowAnimationListener *listener) = 0;

	/** @short Animate the window from one state to another, over a specified amount of time.
	 *
	 * This will change the position, stretch and opacity of the window.
	 *
	 * The position change is similar to a "regular" OpWindow positioning
	 * operation (SetInnerSize(), only that it is animated), which means that
	 * calling GetInnerPos() when the animation is over will reflect the values
	 * of 'x' and 'y' in 'animation.after.rect'. Stretch and opacity, on the
	 * other hand, are PI_ANIMATED_WINDOWS-specific concepts, and cannot be
	 * manipulated or accessed with other "regular" OpWindow methods. In other
	 * words, stretch does not affect the actual size of the window (the one
	 * associated with {Set,Get}{Inner,Outer}Size()), only its zoom
	 * factor. Stretch is specified by 'width' and 'height' in
	 * animation.before.rect and animation.after.rect. These values will indeed
	 * be the actual size of the window as seen on the screen, but as far as
	 * core is concerned, the window size is the one obtained by
	 * e.g. GetInnerSize(). This means that the platform should scale mouse
	 * events and paint operations. Scale factor is
	 * animation.{before,after}.rect.{width,height} / inner size. Aspect ratio
	 * is allowed to be different from 1.0.
	 *
	 * Only one animation at a time is allowed per window. If this method is
	 * called while another animation is in progress, the previous animation
	 * will be canceled (exactly as if CancelAnimation() were called).
	 *
	 * If an OpWindowAnimationListener has been set with
	 * SetWindowAnimationListener(), the implementation must call its method
	 * OnAnimationFinished() when the animation has finished.
	 *
	 * @param animation Description of the animation. Describes the position,
	 * stretch and opacity, initially and finally (before and after). The
	 * duration of the animation is also specified.
	 */
	virtual void AnimateWindow(const OpWindowAnimation& animation) = 0;

	/** @short Cancel an animation previously started with AnimateWindow().
	 *
	 * This does not trigger a call to
	 * OpWindowAnimationListener::OnAnimationFinished().
	 */
	virtual void CancelAnimation() = 0;
#endif // PI_ANIMATED_WINDOWS

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	/** @short Get ratio of device pixel to window size
	 *
	 * @returns the ratio of device pixel to window size, in percentage
	 */
	virtual int GetPixelScale() = 0;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
};

#ifndef DOXYGEN_DOCUMENTATION
inline void OpWindow::SetIcon(class OpWidgetImage*) { }
#endif // !DOXYGEN_DOCUMENTATION

#endif // OPWINDOW_H
