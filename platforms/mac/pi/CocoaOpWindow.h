/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_OPWINDOW_H
#define COCOA_OPWINDOW_H

#include "adjunct/desktop_pi/DesktopOpWindow.h"
#include "adjunct/desktop_util/actions/delayed_action.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "modules/skin/skin_listeners.h"

#ifdef __OBJC__
#define BOOL NSBOOL
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <AppKit/NSAnimation.h>
#undef BOOL
#else
class NSWindow;
class NSView;
class NSViewAnimation;
#endif

class CocoaVEGAWindow;
class CocoaMDEScreen;

#define MAC_TITLE_BAR_HEIGHT  22

class CocoaOpWindow : public DesktopOpWindow, public SkinNotificationListener, public TransparentBackgroundListener
{
public:
	CocoaOpWindow();
	virtual ~CocoaOpWindow();
	void Invalidate(MDE_Region *update_region);
	CocoaVEGAWindow* GetCocoaVEGAWindow() { return m_vega; }
	CocoaMDEScreen* GetScreen() { return m_screen; }
	WindowRef GetCarbonWindow();
	void GetContentPos(INT32* x, INT32* y);
	NSWindow* GetNativeWindow() { return (NSWindow*)m_data; }
	NSView* GetContentView();
	void ResetRightMouseCount();
	void GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom);

	BOOL IsNativeWindow() const { return m_data ? TRUE: FALSE; }
	BOOL IsSameWindow(void* win) const { return win == m_data; }

	/** Get the Style of a window, used to style a new window with the style of opener if defined,
		return STYLE_UNKNOWN if the window doesn't retain information on style setting */
	virtual Style GetStyle();

	/** TransparentBackgroundListener */
	virtual void OnBackgroundCleared(OpPainter* painter, const OpRect& clear_rect);

	/**
	* IntersectsGrowRegion
	*
	* Gets the size of the grow box (if shown) on the window
	*/
	OpPoint IntersectsGrowRegion(const OpRect& rect);

	// Returns if the window is using the invisible titlebar hack
	BOOL HasInvisibleTitlebar();

	static BOOL CloseCurrentPopupIfNot(void* nswin);
	
	static BOOL ShouldPassMouseEvents(void *nswin);
	static BOOL ShouldPassMouseMovedEvents(void *nswin, int x, int y);
	
	static void SetLastMouseDown(int x, int y, void* evt);
	static OpPoint GetLastMouseDown() { return s_last_mouse_down; }
	static void* GetLastMouseDownEvent() { return s_last_mouse_down_event; }

	static BOOL HandleEnterLeaveFullscreen();

	void  SetAlpha(float alpha);
	static float s_alpha;
	OpWindowListener * GetWindowListener() { return windowListener; }
	void AbortInputMethodComposing();
	void ResetFirstResponder();

	OpAutoVector<OpDelayedAction> m_posted_actions;

	// SkinNotificationListener
	virtual void OnSkinLoaded(OpSkin *loaded_skin) {}
	virtual void OnPersonaLoaded(OpPersona *loaded_persona);
	static void DeminiaturizeAnyWindow();

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	virtual int GetPixelScale();
#endif //PIXEL_SCALE_RENDERING_SUPPORT

protected:
// OpWindow:

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
	virtual OP_STATUS Init(Style style = STYLE_DESKTOP, OpTypedObject::Type type = OpTypedObject::WINDOW_TYPE_UNKNOWN, OpWindow* parent_window = NULL, OpView* parent_view = NULL, void* native_handle = NULL, UINT32 effect = EFFECT_NONE);

	/** Reparent the window. */
	virtual void SetParent(OpWindow* parent_window = NULL, OpView* parent_view = NULL, BOOL behind = FALSE);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	/** Set the accessibility item associated to the window
	 *
	 * Sets the accessible item for which an adapter should be
	 * requested when the bootstrapping of the accessible
	 * herarchy is requested.
	 */
	virtual void SetAccessibleItem(OpAccessibleItem* accessible_item);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	/** Get the parent.
	 *
	 * @return The parent, or NULL if there is no parent or the parent
	 * is not an OpWindow.
	 */
	virtual OpWindow* GetParentWindow();

	/** Get the parent.
	 *
	 * @return The parent, or NULL if there is no parent or the parent
	 * is not an OpView.
	 */
	virtual OpView* GetParentView();

	/** Get the top-level window ancestor of this window.
	 *
	 * Is able to go through any OpView objects in the ancestor
	 * chain. It is needed to go from any OpWindow/OpView to the
	 * OpWindow defining the UI, where there are no parents.
	 *
	 * @return The top-level window, or NULL if unsupported.
	 */
	virtual OpWindow* GetRootWindow();

	/** Set the the listener which will receive events from this window.
	 *
	 * NOTE!!! When compiling with WINDOW_COMMANDER, the listener must
	 * be deleted by the ui.
	 */
	virtual void SetWindowListener(OpWindowListener *windowlistener);

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
	virtual void SetDesktopPlacement(const OpRect& rect, State state, BOOL inner = FALSE, BOOL behind = FALSE, BOOL center = FALSE);

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
	virtual void GetDesktopPlacement(OpRect& rect, State& state);

	/** Get the state that a window shall enter when activated from
	 * minimized state. */
	virtual State GetRestoreState() const;

	/** Set the state that a window shall enter when activated from
	 * minimized state.
	 *
	 * The window state shall not change as a direct result of this
	 * call.
	 */
	virtual void SetRestoreState(State state);

	/** Set the window state to restored. */
	virtual void Restore();

	/** Set the window state to minimized. */
	virtual void Minimize();

	/** Set the window state to maximized. */
	virtual void Maximize();

	/** Set the window state to full-screen. */
	virtual void Fullscreen();

	/** Attempt to prevent the window from being updated.
	 *
	 * Initially it is not locked.
	 *
	 * @param lock If TRUE, lock for updates. This means that the
	 * platform will attempt to ignore repaint request from both Opera
	 * itself, and from the windowing system. If FALSE, handle repaint
	 * requests normally.
	 */
	virtual void LockUpdate(BOOL lock);

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
	virtual void SetFloating(BOOL floating);

	/** Set the always-below state of this window.
	 *
	 * Initially, a window is not always-below; i.e. it is in the
	 * normal window stack.
	 *
	 * @param below if TRUE, the window is below all other windows
	 * that are in the normal window stack and floating window
	 * stack. If FALSE, the window lives in the normal window stack.
	 */
	virtual void SetAlwaysBelow(BOOL below);

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
	virtual void SetShowInWindowList(BOOL in_window_list);
#endif // OPWINDOW_DESKTOP_FUNCTIONS

	/** Show or hide the window.
	 *
	 * @param show Show the window if TRUE, hide it if FALSE.
	 */
	virtual void Show(BOOL show);

	/** Give or take away focus.
	 *
	 * @param focus Give focus to the window if TRUE, take it away if FALSE.
	 */
	virtual void SetFocus(BOOL focus);

	/** Close the window and unload it from the system. */
	virtual void Close(BOOL user_initiated = FALSE);

	/** Get the window outer size (including decorations). */
	virtual void GetOuterSize(UINT32* width, UINT32* height);

	/** Set the window outer size (including decorations). */
	virtual void SetOuterSize(UINT32 width, UINT32 height);

	/** Get the window inner size (excluding decorations). */
	virtual void GetInnerSize(UINT32* width, UINT32* height);

	/** Set the window inner size (excluding decorations). */
	virtual void SetInnerSize(UINT32 width, UINT32 height);

	/** Get the window outer position (including decorations). */
	virtual void GetOuterPos(INT32* x, INT32* y);

	/** Set the window outer position (including decorations). */
	virtual void SetOuterPos(INT32 x, INT32 y);

	/** Get the window inner position (excluding decorations). */
	virtual void GetInnerPos(INT32* x, INT32* y);

	/** Set the window inner position (excluding decorations). */
	virtual void SetInnerPos(INT32 x, INT32 y);

	/** Set the window outer position and size (excluding decorations) in one operation. */
	virtual void SetOuterPosAndSize(INT32 x, INT32 y, UINT32 width, UINT32 height);

	/** Set the window inner position and size (excluding decorations) in one operation (optimizing animation). */
	virtual void SetInnerPosAndSize(INT32 x, INT32 y, UINT32 width, UINT32 height);

	/** Get the main menu bar items for automated testing **/
	virtual BOOL GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out);

#ifdef VIEWPORTS_SUPPORT
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
	virtual void GetRenderingBufferSize(UINT32* width, UINT32* height);
#endif // VIEWPORTS_SUPPORT

	/** Set the minimum inner size of the window (excluding decorations).
	 *
	 * The window may not get a smaller size than this.
	 */
	virtual void SetMinimumInnerSize(UINT32 width, UINT32 height);

	/** Sets the maximum inner size of the window (excluding decorations).
	 *
	 * The window may not get a larger size than this.
	 */
	virtual void SetMaximumInnerSize(UINT32 width, UINT32 height);

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
	virtual void GetMargin(INT32* left_margin, INT32* top_margin, INT32* right_margin, INT32* bottom_margin);

	/** Request a transparency level for the whole window.
	 *
	 * It is not required that this be implemented.
	 *
	 * @param transparency Transparency level, a value between 0
	 * (fully transparent) and 255 (fully opaque).
	 */
	virtual void SetTransparency(INT32 transparency);

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
	virtual void SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility);

	/** Set menubar visibility (if any in the window). */
	virtual void SetMenuBarVisibility(BOOL visible);

	/** Get menubar visibility.
	 *
	 * @return TRUE if visible. FALSE if hidden or none.
	 */
	virtual BOOL GetMenuBarVisibility();

	/** Update the font in the window.
	 *
	 * Only used for updating special fonts like the toolbar font etc.
	 */
	virtual void UpdateFont();

	/** Update the translated text in the window.
	 *
	 * Only used for special windows like the toolbar etc.
	 */
	virtual void UpdateLanguage();

	/** Update the menu bar in the window.
	 *
	 * Only used for special windows.
	 */
	virtual void UpdateMenuBar();

	/** Set the skin.
	 *
	 * Tell OpWindow what kind of skin DesktopWindow will use through
	 * OpSkinManager on this window.
	 */
	virtual void SetSkin(const char* skin);

	/** Redraw window. Needed because color scheme might have changed */

	virtual void Redraw();

	/** Raise the window.
	 *
	 * Raise the window to the top of the window stack to which it
	 * currently belongs.
	 */
	virtual void Raise();

	/** Lower the window.
	 *
	 * Lower the window to the bottom of the window stack to which it
	 * currently belongs.
	 */
	virtual void Lower();

	/** Raise and activate the window. */
	virtual void Activate();

	/** Deactivate the window if it was active.
	 *
	 * This activates no other windows, but sends proper
	 * OpWindowListener::OnActivate(FALSE) to window being
	 * deactivated. This is used by QUICK to deactivate an active
	 * STYLE_DESKTOP window inside a STYLE_WORKSPACE so that all
	 * windows in the workspace appear deactivated.
	 */
	virtual void Deactivate();

	/** The window wants user attention.
	 *
	 * Typically do nothing if this is the active window, or else
	 * flash its taskbar button or something.
	 */
	virtual void Attention();

#ifndef MOUSELESS
	/** Set the current cursor */
	virtual void SetCursor(CursorType cursor);

	/** Get the current cursor */
	virtual CursorType GetCursor();
#endif // !MOUSELESS

	/** Get the window title. */
	virtual const uni_char* GetTitle() const;

	/** Set the window title. */
	virtual void SetTitle(const uni_char* title);

	/** Set the window icon. */
	virtual void SetIcon(class OpBitmap* bitmap);

	/** Instruct this window to be opened in the background.
	 *
	 * Special function for workspace windows to allow opening a
	 * childwindow in the background. You will probably not need this
	 * one unless you deal with Qt. [espen 2003-01-27]
	 */
	virtual void SetOpenInBackground(BOOL state);

	/** Set a resource name on the window.
	 *
	 * In X-windows the resource name is used by some window managers
	 * to track what windows that belong together. You can probably
	 * ignore to reimplement this function unless you use X-windows
	 * [espen 2003-02-25]
	 *
	 * @param name The resource name
	 */
	virtual void SetResourceName(const char *name);

#ifdef OPWINDOW_TOPMOST_WINDOW
	/** Is this window, or its ancestor, topmost and active? */
	virtual BOOL IsActiveTopmostWindow();
#endif // OPWINDOW_TOPMOST_WINDOW

#ifdef PI_ANIMATED_WINDOWS
	/** @short Set listener for window animation events. */
	virtual void SetWindowAnimationListener(OpWindowAnimationListener *listener);

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
	virtual void AnimateWindow(const OpWindowAnimation& animation);

	/** @short Cancel an animation previously started with AnimateWindow().
	 *
	 * This does not trigger a call to
	 * OpWindowAnimationListener::OnAnimationFinished().
	 */
	virtual void CancelAnimation();
#endif // PI_ANIMATED_WINDOWS

// DesktopOpWindow:
	virtual void OnVisibilityChanged(BOOL visibility);
	virtual void SetLockedByUser(BOOL locked_by_user);
	virtual void OnTransparentAreaChanged(int top, int bottom, int left, int right);

	static OpPointerHashTable<void, CocoaOpWindow> s_adopted_windows;

private:
	void SetFullscreenMode(BOOL fullscreen);
	void AddTooltipWindow(CocoaOpWindow* tooltip);
	void RemoveTooltipWindow(CocoaOpWindow* tooltip);
	void TooltipParentDeleted();
	
	static CocoaOpWindow* s_current_popup;
	static CocoaOpWindow* s_current_extension_popup;
	CocoaMDEScreen* m_screen;
	CocoaVEGAWindow* m_vega;
	OpVector<CocoaOpWindow> m_tooltips;
	OpView* m_parent_view;
	OpWindow* m_parent_window;
	void* m_data;
	void* m_temp_data;					// Holds the real window when we are in full screen mode
	Style m_style;
	OpTypedObject::Type m_type;
	UINT32 m_effect;
	BOOL m_auto_resize_window;
	OpPoint m_grow_size;
	NSViewAnimation *m_animated_setframe;
	static CocoaOpWindow* s_fullscreen;
	static OpPoint s_last_mouse_down;
	static void* s_last_mouse_down_event;
	static BOOL s_force_first_responder;		// Set to avoid problems with Cocoa not setting the parent window correctly 
};

#endif // COCOA_OPWINDOW_H

