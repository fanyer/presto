/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_OPWINDOW_H
#define MDE_OPWINDOW_H

#include "modules/pi/OpWindow.h"
#include "modules/libgogi/mde.h"
#include "modules/libgogi/pi_impl/mde_widget.h"

class MDE_Widget;
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
class MDE_ChromeWindow;
#endif // MDE_OPWINDOW_CHROME_SUPPORT

#ifndef GOGI
class OpCursorListener
{
public:
	virtual ~OpCursorListener(){}
	virtual void OnSetCursor(CursorType cursor, MDE_Screen* screen) = 0;
};
#endif

class MDE_OpWindow : public OpWindow, public MDE_WidgetVisibilityListener
{
private:
	OpWindow* m_parent_window;
	OpView* m_parent_view;
public:
	MDE_OpWindow();
	virtual ~MDE_OpWindow();
	virtual OP_STATUS Init(Style style = STYLE_DESKTOP, OpTypedObject::Type type = OpTypedObject::WINDOW_TYPE_UNKNOWN, OpWindow* parent_window = NULL, OpView* parent_view = NULL, void* native_handle = NULL, UINT32 effect = EFFECT_NONE);
	virtual void SetParent(OpWindow* parent_window = NULL, OpView* parent_view = NULL, BOOL behind = FALSE);
	virtual void SetWindowListener(OpWindowListener *windowlistener);
	virtual void SetDesktopPlacement(const OpRect& rect, State state, BOOL inner = FALSE, BOOL behind = FALSE, BOOL center = FALSE);
	virtual void GetDesktopPlacement(OpRect& rect, State& state);
	virtual State GetRestoreState() const { return m_state_before_minimize; }
	virtual void SetRestoreState(State state) { m_state_before_minimize = state; }
	virtual void Restore();
	virtual void Minimize();
	virtual void Maximize();
	virtual void Fullscreen();
	virtual void LockUpdate(BOOL lock);
	virtual void Show(BOOL show);
	virtual void SetFocus(BOOL focus);
	virtual void Close(BOOL user_initiated = FALSE);
	virtual void GetOuterSize(UINT32* width, UINT32* height);
	virtual void SetOuterSize(UINT32 width, UINT32 height);
	virtual void GetInnerSize(UINT32* width, UINT32* height);
	virtual void SetInnerSize(UINT32 width, UINT32 height);
	virtual void GetOuterPos(INT32* x, INT32* y);
	virtual void SetOuterPos(INT32 x, INT32 y);
	virtual void GetInnerPos(INT32* x, INT32* y);
	virtual void SetInnerPos(INT32 x, INT32 y);
	virtual void SetMinimumInnerSize(UINT32 width, UINT32 height);
	virtual void SetMaximumInnerSize(UINT32 width, UINT32 height);
	virtual void GetMargin(INT32* left_margin, INT32* top_margin, INT32* right_margin, INT32* bottom_margin);
	virtual void SetTransparency(INT32 transparency);
	virtual void SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility);

	/** Set if this window (including all children) should ignore mouse input. */
	void SetIgnoreMouseInput(BOOL ignore) { ignore_mouse_input = ignore; }
	BOOL GetIgnoreMouseInput() { return ignore_mouse_input; }

	virtual void SetSkin(const char* skin);
	virtual void Redraw();
	virtual Style GetStyle();
	virtual void Raise();
	virtual void Lower();
	virtual void Activate();
	virtual void Deactivate();
	virtual void Attention();

	/** Get parent at runtime.. either OpWindow or OpView that was
	passed in to Init or SetParent*/

	virtual OpWindow* GetParentWindow() { return m_parent_window; }
	virtual OpView* GetParentView() { return m_parent_view; }
	virtual OpWindow* GetRootWindow() { return GetMDEWidget()->GetRootWindow(); }

#ifndef MOUSELESS
	virtual void SetCursor(CursorType cursor);
	virtual CursorType GetCursor();
#endif // !MOUSELESS
	virtual const uni_char* GetTitle() const;
	virtual void SetTitle(const uni_char* title);
	virtual void SetIcon(class OpBitmap* bitmap);

#ifndef GOGI
	void SetCursorListener(OpCursorListener *cl);
#endif
	// non-interface functions
	
	MDE_Widget *GetMDEWidget(){return mdeWidget;}
	virtual MDE_View* GetScreenView(const MDE_View *parentWidget, const OpWindow* parent_window, const OpView* parent_view) const;
	virtual MDE_View* GetParentWidget(const OpWindow* parent_window, const OpView* parent_view) const;
	virtual BOOL SetWidgetRect(const MDE_RECT& mr);
	void SetWidgetVisibility(bool vis);

	// FIX: Turn into a API!
	OpBitmap *CreateBitmapFromWindow();
	bool PaintOnBitmap(OpBitmap *bitmap);

#ifdef MDE_DS_MODE
	virtual OP_STATUS SetDSMode(BOOL enable);
	virtual BOOL GetDSMode();
#endif // MDE_DS_MODE

	virtual void OnVisibilityChanged(bool vis);
	virtual void OnRectChanged(const MDE_RECT &old_rect);

	INT32 GetTransparency() { return transparency; }
	UINT32 GetEffects() { return effect; }

	void SetAllowAsActive(BOOL allow) { m_allow_as_active = allow; }

	void SetCached(BOOL cached);
	OpBitmap *GetCacheBitmap();

	void SetActive(BOOL active) { m_active = active; }
	BOOL IsActive() const { return m_active; }

	/**
	   Returns the bottom-most (in z-order) window that represents
	   actual content (e.g. not chrome windows).
	 */
	MDE_OpWindow* GetBottomMostWindow();

	/**
	   Returns the next (above in z-order) window that represents
	   actual content (e.g. not chrome windows).
	*/
	MDE_OpWindow* NextWindow();

	/**
	   lowers the mdeWidget, along with any shadow, down one step.

	   if MDE_OPWINDOW_CHROME_SUPPORT is enabled, also lowers any
	   window chrome, and takes any chrome of the raised window into
	   consideration.
	 */
	void LowerWindow();

	/**
	   Lowers the mdeWidget, along with any shadow, to the bottom of
	   the stack of visible windows.

	   This method will not lower the window past minimized windows or
	   any "background" element.

	   if MDE_OPWINDOW_CHROME_SUPPORT is enabled, also lowers any
	   window chrome, and takes any chrome of the raised window into
	   consideration.
	 */
	void MoveToBottomOfWindowStack();

#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	/**
	   returns TRUE if view the window has chrome
	*/
	BOOL HasChrome() { return chromeWin != 0; }

	/**
	   returns TRUE if this window is the chrome of another window
	*/
	BOOL IsChromeWindow();
#endif // MDE_OPWINDOW_CHROME_SUPPORT

	/**
	   Called from ~MDE_Widget - unregisters the widget and destroys
	   the window.
	 */
	void WidgetDeleted();

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	int GetPixelScale() { return 100; }
#endif // PIXEL_SCALE_RENDERING_SUPPORT

protected:

	MDE_Widget *mdeWidget;
	MDE_Widget *mdeWidgetShadow;

#ifdef MDE_DS_MODE
	class MDE_DSMode* dsMode;
#endif // MDE_DS_MODE

	OpWindowListener *windowListener;

	Style style;
	UINT32 effect;
	INT32 transparency;
	BOOL ignore_mouse_input;

	OpBitmap *cache_bitmap;

private:
	void CloseInternal();
	void UpdateTransparency();
	void DeactivateAndActivateTopmost(BOOL minimizing = FALSE);

	void NotifyOnActivate(BOOL activate, MDE_OpWindow* ow);

#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	/**
	   lowers the mdeWidget, along with any shadow and chrome, down
	   one step. does not take any chrome of the raised window into
	   consideration - see LowerWindow.
	 */
	void LowerWindowOneStep();
#endif // MDE_OPWINDOW_CHROME_SUPPORT

	UINT32 minInnerWidth, minInnerHeight;
	UINT32 maxInnerWidth, maxInnerHeight;

	CursorType cursor;
#ifndef GOGI
	OpCursorListener *cursorListener;
#endif
	State m_state;
	State m_state_before_minimize;
	BOOL m_allow_as_active;
	OpRect m_restored_rect;

	uni_char* m_title;
	OpBitmap* m_icon;

#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	MDE_ChromeWindow* chromeWin;
#endif // MDE_OPWINDOW_CHROME_SUPPORT
	BOOL m_active;
};

#endif // MDE_OPWINDOW_H
