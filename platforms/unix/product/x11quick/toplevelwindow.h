/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef X11_TOPLEVELWINDOW_H
#define X11_TOPLEVELWINDOW_H

#include "uiwindownotifier.h"
#include "menubar.h"

#include "adjunct/desktop_scope/src/generated/g_scope_desktop_window_manager_interface.h"
#include "adjunct/desktop_util/settings/SettingsListener.h"

#include "platforms/unix/base/x11/x11_windowdecoration.h"
#include "platforms/unix/base/x11/x11_windowwidget.h"

#include "modules/hardcore/timer/optimer.h"

class BrowserDesktopWindow;
class MenuBar;
class X11WindowDecoration;

class ToplevelWindow
	:public X11WindowWidget
	,public UiWindowNotifier
	,public SettingsListener
	,public OpTimerListener
{
public:
	enum ToplevelComponent
	{
		TL_NONE,
		TL_PAGEBAR,
		TL_MENUBAR,
		TL_DECORATION
	};


	ToplevelWindow(X11OpWindow *opwindow);
	~ToplevelWindow();

	OP_STATUS Init(X11Widget *parent, const char *name=0, int flags=0);

	X11Widget *GetChild() const { return m_work_area.get(); }

	OpPoint GetSkinOrigin() { return OpPoint(0,0); }
	OpPoint GetSkinOffset() { return OpPoint(m_decoration->Left(), m_decoration->Top()+m_menu_bar->Height()); }

	void Show();
	void Resize(int w, int h);
	void HandleResize();
	void UpdateButtonSets();

	void AdjustAvailableSize(UINT32& width, UINT32& height);

	void ShowMenuBar(bool show);

	virtual void ShowSkinnedDecoration(bool show);
	virtual void GetContentMargins(int &top, int &bottom, int &left, int &right);

	/*
	 * Implementation of UiWindowNotifier
	 */
	void SetSkinElement(const char *) {}
	void GetMargin(INT32 *left, INT32 *top, INT32 *right, INT32 *bottom);

	/**
	 * Implementation of SettingsListener
	 */
	virtual void OnSettingsChanged(DesktopSettings* settings);


	BOOL ConfirmClose();
	virtual void OnToplevelActivate();
	virtual void OnToplevelDeactivate();
	virtual void OnDecorationChanged();
	virtual void OnWindowStateChanged(WindowState state);

    BOOL GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out);

	OP_STATUS SetupMenuBar();

public:

	/**
	 * Returns the BrowserDesktopWindow object that lives in a toplevel window which
	 * is using the X11 Window handle
	 *
	 * @param window The X11 window handle
	 * 
	 * @return The BrowserDesktopWindow object or 0 on error
	 */
	static BrowserDesktopWindow* GetBrowserDesktopWindow(X11Types::Window window);

private:
	/**
	 * Returns the BrowserDesktopWindow object that the toplevel window represents
	 *
	 * @return The BrowserDesktopWindow object, or 0 on error
	 */
	BrowserDesktopWindow* GetBrowserDesktopWindow();

	OP_STATUS SetupWorkarea();
	OP_STATUS SetupSkinnedDecoration();
	void UpdateWorkArea();
	void UpdateWindowList();
	void UpdatePageBar();
	void UpdateDecorationType(DecorationType preferred);
	ToplevelComponent MDIButtonLocation();
	bool IsMenuBarVisible() const;
	bool IsOperaMenuInTopPagebar() const;
	bool IsPagebarOnTop() const;
	bool IsPagebarVisible() const;
	bool IsMDIButtonsVisible() const;
	bool IsOMenuVisible() const;
	bool IsSkinnedDecorationVisible() const;
 	inline OpWidget* GetRootOpWidget() const;
	bool NeedSkinnedDecoration() const;

	void OnTimeOut(OpTimer* timer);
	void PostUpdateLayout();
	void UpdateLayout();


private:
	struct DecorationSize
	{
	public:
		DecorationSize():top(25), bottom(3), left(3), right(3) {Init();}
		void Init();

	public:
		int top;
		int bottom;
		int left;
		int right;
	};

	struct LayoutFlags
	{
		LayoutFlags()
			:is_decoration_visible(false)
			,is_menubar_visible(false)
			,is_menubar_override(false)
			,menubar_mode(MenuBar::ROW_MENU)
			{}

		bool is_decoration_visible;
		bool is_menubar_visible;
		bool is_menubar_override;
		MenuBar::DisplayMode menubar_mode;
	};

	struct Controls
	{
		Controls() : wm_buttonset(NULL), mdi_buttonset(NULL) {}

		WMButtonSet* wm_buttonset;
		MDIButtonSet* mdi_buttonset;
	};

private:
	OpAutoPtr<X11WindowDecoration> m_decoration;
	OpAutoPtr<MenuBar> m_menu_bar;
	OpAutoPtr<X11Widget> m_work_area;
	Controls m_controls;
	const UINT32 m_decoration_border_color;
	DecorationSize m_decoration_size;
	LayoutFlags m_next;
	LayoutFlags m_state;
	bool m_layout_update_pending;
	OpTimer m_layout_timer;

private:
	static OpVector<ToplevelWindow> m_window_list;
};

#endif // X11_TOPLEVELWINDOW_H
