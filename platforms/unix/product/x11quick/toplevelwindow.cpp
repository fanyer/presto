/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "toplevelwindow.h"

#include "adjunct/desktop_util/settings/SettingsType.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/menus/MenuShortcutLetterFinder.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/skin/OpSkinManager.h"

#include "ipcmessageparser.h"
#include "menubar.h"
#include "popupmenu.h"

#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_icon.h"
#include "platforms/unix/base/x11/x11_ipcmanager.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"


class ToplevelWindowChild : public X11Widget
{
public:
	ToplevelWindowChild() :
		X11Widget() { }
};


void ToplevelWindow::DecorationSize::Init()
{
	int width, height;
	OpSkinElement* skin_element;

	skin_element = g_skin_manager->GetSkinElement("Top Decoration Skin");
	if (skin_element && OpStatus::IsSuccess(skin_element->GetSize(&width, &height, 0)) && height >= 0)
		top = height;

	skin_element = g_skin_manager->GetSkinElement("Right Decoration Skin");
	if (skin_element && OpStatus::IsSuccess(skin_element->GetSize(&width, &height, 0)) && width >= 0)
		right = width;

	skin_element = g_skin_manager->GetSkinElement("Bottom Decoration Skin");
	if (skin_element && OpStatus::IsSuccess(skin_element->GetSize(&width, &height, 0)) && height >= 0)
		bottom = height;

	skin_element = g_skin_manager->GetSkinElement("Left Decoration Skin");
	if (skin_element && OpStatus::IsSuccess(skin_element->GetSize(&width, &height, 0)) && width >= 0)
		left = width;
}



OpVector<ToplevelWindow> ToplevelWindow::m_window_list;

ToplevelWindow::ToplevelWindow(X11OpWindow* op_window)
	: X11WindowWidget(op_window)
	, m_decoration_border_color(0xC8505050)
	, m_layout_update_pending(false)
{
}

ToplevelWindow::~ToplevelWindow()
{
	m_window_list.RemoveByItem(this);
	UpdateWindowList();
}


OP_STATUS ToplevelWindow::Init(X11Widget* parent, const char* name, int flags)
{
	RETURN_IF_ERROR(X11WindowWidget::Init(parent, name, flags));
	RETURN_IF_ERROR(m_window_list.Add(this));
	RETURN_IF_ERROR(SetupSkinnedDecoration());
	RETURN_IF_ERROR(SetupMenuBar());
	RETURN_IF_ERROR(SetupWorkarea());

	int color;
	RETURN_IF_LEAVE(color = g_skin_manager->GetCurrentSkin()->ReadColorL("Options", "Window Border Color", m_decoration_border_color, FALSE));

	m_menu_bar->SetBorderColor(color);
	m_decoration->SetBorderColor(color);

	X11Icon::SetApplicationIcon( this );
	X11IPCManager::Init(GetWindowHandle());

	g_application->AddSettingsListener(this);

	UpdateWindowList();

	SetWindowRole("browser");

	// Can be overridden by quick if it wants to
	SetMinSize(100, 100);

	return OpStatus::OK;
}


void ToplevelWindow::Show()
{
	UpdateDecorationType(g_x11->HasPersonaSkin() ? DECORATION_SKINNED : DECORATION_WM);
	UpdateLayout(); // No posting here. Shows menu bar immediately if needed. Avoids flicker
	m_work_area->Show();

	X11WindowWidget::Show();


	// Set parent context. Can not be done in Init() as the desktop window is not available yet
	DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(GetOpWindow());
	if (dw)
	{
		m_menu_bar->SetParentInputContext(dw);
		m_decoration->SetParentInputContext(dw);
	}

	// This is needed on startup when we provide command line urls.
	// OpBootManager::StartBrowser() will create toplevel windows from
	// a session or a new window and it assumes QUICK can provide a non NULL
	// GetActiveBrowserDesktopWindow() synchronously. Normally the OnActivate()
	// message happens after event processing in X11WindowWidget::HandleEvent()
	// If we do not do this one url will create three windows
	if (GetOpWindow()->GetWindowListener())
	{
		GetOpWindow()->GetWindowListener()->OnActivate(TRUE);
	}
}


void ToplevelWindow::UpdateWindowList()
{
	UINT32 num_window = m_window_list.GetCount();
	OpAutoArray<unsigned long> list (OP_NEWA(unsigned long, num_window));
	if (list.get())
	{
		for (UINT32 i=0; i<num_window; i++)
			list[i] = m_window_list.Get(i)->GetWindowHandle();
		for (UINT32 i=0; i<num_window; i++)
		{
			ToplevelWindow* item = m_window_list.Get(i);
			item->SetWindowList(list.get(), num_window);
			item->SetWindowName(i+1);
		}
	}
}

void ToplevelWindow::Resize(int w, int h)
{
	X11WindowWidget::Resize(w, h);

	int mbh = m_menu_bar->CalculateHeight();
	if (mbh < m_decoration_size.top)
		mbh = m_decoration_size.top;

	m_work_area->Resize(w-m_decoration->Width(), h-m_decoration->Height()-mbh);
	m_menu_bar->SetGeometry(m_decoration->Left(), m_decoration->Top(), w-m_decoration->Width(), mbh);
}


BOOL ToplevelWindow::ConfirmClose()
{
	return GetOpWindow()->UiClosing();
}


void ToplevelWindow::HandleResize()
{
	int width, height;
	GetSize(&width, &height);

	int w = width;
	int h = height;
	m_decoration->SetSize(w, h);

	w -= m_decoration->Width();
	h -= m_decoration->Height();

	int mbh = m_menu_bar->CalculateHeight();
	if (mbh < m_decoration_size.top)
		mbh = m_decoration_size.top;
	m_menu_bar->SetSize(w, mbh);

	h -= m_menu_bar->Height();

	m_work_area->Resize(w, h);

	UpdateButtonSets();

	// A vertical resize of the window does not change the size or position
	// of the menu bar or top edge decoration. When using a persona skin the
	// background should nevertheless be updated so we trigger an update here
	if (g_x11->HasPersonaSkin())
	{
		if (IsMenuBarVisible())
			m_menu_bar->Invalidate();
		if (IsSkinnedDecorationVisible())
			m_decoration->Invalidate();
	}

	X11WindowWidget::HandleResize();
}


void ToplevelWindow::OnDecorationChanged()
{
	m_next.is_decoration_visible = GetDecorationType() == DECORATION_SKINNED;
	PostUpdateLayout();
}


void ToplevelWindow::OnWindowStateChanged(WindowState state)
{
	PostUpdateLayout();
}


void ToplevelWindow::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_MENU_SETUP))
	{
		UpdateDecorationType(GetDecorationType());
	}
	else if (settings->IsChanged(SETTINGS_SKIN))
	{
		int color;
		OP_STATUS rc;
		TRAP_AND_RETURN_VOID_IF_ERROR(rc, color = g_skin_manager->GetCurrentSkin()->ReadColorL("Options", "Window Border Color", m_decoration_border_color, FALSE));
		m_menu_bar->SetBorderColor(color);
		m_decoration->SetBorderColor(color);
	}
	else if (settings->IsChanged(SETTINGS_TOOLBAR_ALIGNMENT))
	{
		PostUpdateLayout();
	}
}

void ToplevelWindow::UpdateDecorationType(DecorationType preferred)
{
	DecorationType type = preferred;

	if (g_x11->HasPersonaSkin())
		type = DECORATION_SKINNED;

	// Let it be possible for the user to override the title bar visibility
	int user_override = g_pcunix->GetIntegerPref(PrefsCollectionUnix::ShowWindowBorder);
	if (user_override == 1)
		type = DECORATION_WM;
	else if (user_override == 2)
		type = DECORATION_SKINNED;

	// user_override == 0 means system default. Then we shall verify the type
	SetDecorationType(type, user_override == 0 ? true : false);
}



void ToplevelWindow::AdjustAvailableSize(UINT32& width, UINT32& height)
{
	UINT32 w = m_decoration->Width();
	UINT32 h = m_decoration->Height() + m_menu_bar->Height();

	width  -= w > width  ? width  : w;
	height -= h > height ? height : h;
}


void ToplevelWindow::PostUpdateLayout()
{
	if (!m_layout_update_pending)
	{
		m_layout_update_pending = true;
		m_layout_timer.SetTimerListener(this);
		m_layout_timer.Start(0);
	}
}


void ToplevelWindow::OnTimeOut(OpTimer* timer)
{
	m_layout_update_pending = false;
	UpdateLayout();
}


void ToplevelWindow::UpdateLayout()
{
	int w, h;
	GetSize(&w, &h);

	// The menu bar also displays the opera menu when needed. Cross platform code does
	// not expect this so we do the overrides here.

	m_next.is_menubar_override = false; // true means we will show the opera menu in the menu bar area

	if (GetWindowState() == NORMAL || GetWindowState() == MAXIMIZED)
	{
		if (m_next.is_menubar_visible)
			m_next.menubar_mode = MenuBar::ROW_MENU;
		else
		{
			m_next.is_menubar_override = true;
			m_next.menubar_mode = MenuBar::OPERA_MENU;
		}
	}
	else if (GetWindowState() == FULLSCREEN)
	{
		if (m_next.is_menubar_visible)
			m_next.menubar_mode = MenuBar::ROW_MENU;
	}

	if (m_next.is_menubar_override)
	{
		// No menu bar if we can draw it in the page bar
		if (IsOperaMenuInTopPagebar())
			m_next.is_menubar_override = false;
		// Always draw opera menu in page bar regardless of orientation when WM decoration is active
		else if (IsPagebarVisible() && GetDecorationType() == DECORATION_WM && !IsMDIButtonsVisible())
			m_next.is_menubar_override = false;
		// Do not show menu bar if user has tuned (via skin ini) top decoration off.
		// This is the only way to use opera without any visible toolbars and how it
		// works from 12.00 and onwards.
		else if (m_decoration_size.top == 0)
			m_next.is_menubar_override = false;
	}

	m_state.is_decoration_visible = m_next.is_decoration_visible;
	m_state.is_menubar_visible    = m_next.is_menubar_visible;
	m_state.is_menubar_override   = m_next.is_menubar_override;

	if (m_state.is_decoration_visible)
	{
		m_decoration->SetSize(w, h);
		m_decoration->Show();
	}
	else
	{
		m_decoration->Hide();
	}

	if (GetDecorationType() == DECORATION_WM)
	{
		m_decoration->SetBorders(0, 0, 0, 0, false);
	}
	else
	{
		switch (GetWindowState())
		{
			case FULLSCREEN:
				m_decoration->SetBorders(0, 0, 0, 0, false);
			break;

			default:
			{
				// No decoration on top if there is a menu bar or a page bar on top
				unsigned int top_thickness = m_next.is_menubar_visible || m_next.is_menubar_override || IsOperaMenuInTopPagebar() ? 0 : m_decoration_size.top;

				if (GetWindowState() == MAXIMIZED)
					m_decoration->SetBorders(top_thickness, 0, 0, 0, true);
				else
					m_decoration->SetBorders(
						top_thickness, m_decoration_size.bottom, m_decoration_size.left,
						m_decoration_size.right, true);
				break;
			}
		}
	}

	bool show_opera_menu  = !m_next.is_menubar_visible;
	bool show_wm_buttons  = GetDecorationType() == DECORATION_SKINNED;
	bool show_mdi_buttons = MDIButtonLocation() == TL_DECORATION;
	m_decoration->ShowControls(show_opera_menu, show_wm_buttons, show_mdi_buttons);
	m_decoration->SetCanMoveResize(GetWindowState() == NORMAL);
	m_decoration->SetMaximized(GetWindowState() == MAXIMIZED);
	m_decoration->SetDrawBorder(GetDecorationType() == DECORATION_SKINNED);
	m_decoration->SetHighlightActive(GetDecorationType() == DECORATION_SKINNED);
	m_decoration->SetSize(w, h);

	if (m_state.is_menubar_visible || m_state.is_menubar_override)
	{
		int mbh = m_menu_bar->CalculateHeight();
		if (mbh < m_decoration_size.top)
			mbh = m_decoration_size.top;
		m_menu_bar->SetGeometry(m_decoration->Left(), m_decoration->Top(), w-m_decoration->Width(), mbh);

		show_mdi_buttons = MDIButtonLocation() == TL_MENUBAR;

		m_menu_bar->SetDrawBorder(m_state.is_decoration_visible);
		m_menu_bar->ShowControls(show_wm_buttons, show_mdi_buttons);

		m_menu_bar->Show();
	}
	else
	{
		m_menu_bar->Hide();
	}

	m_state.menubar_mode = m_next.menubar_mode;
	m_menu_bar->SetDisplayMode(m_state.menubar_mode);

	UpdatePageBar();
	UpdateWorkArea();
}


void ToplevelWindow::ShowMenuBar(bool show)
{
	m_next.is_menubar_visible = show;
	PostUpdateLayout();
}


ToplevelWindow::ToplevelComponent ToplevelWindow::MDIButtonLocation()
{
	if (IsMDIButtonsVisible())
	{
		if (GetDecorationType() == DECORATION_WM)
		{
			if (m_state.is_menubar_visible || m_state.is_menubar_override)
				return TL_MENUBAR;
			else if (IsPagebarOnTop())
				return TL_PAGEBAR;
		}
		else
		{
			if (IsPagebarOnTop())
				return TL_PAGEBAR;
			else if (m_state.is_menubar_visible || m_state.is_menubar_override)
				return TL_MENUBAR;
			else if (GetDecorationType() == DECORATION_SKINNED)
				return TL_DECORATION;
		}
	}
	return TL_NONE;
}


void ToplevelWindow::UpdatePageBar()
{
	DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(GetOpWindow());
	if (dw)
	{
		OpPagebar* pagebar = static_cast<OpPagebar*>(dw->GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR));
		if (pagebar)
		{
			bool show_wm_controls  = IsPagebarOnTop() && !IsMenuBarVisible() && GetDecorationType() == DECORATION_SKINNED;
			bool show_o_menu       = IsOperaMenuInTopPagebar() && !IsMenuBarVisible() || (IsPagebarVisible() && !IsMenuBarVisible() && !IsSkinnedDecorationVisible());
			bool show_mdi_controls = MDIButtonLocation() == TL_PAGEBAR;

			if (!m_controls.mdi_buttonset && show_mdi_controls)
			{
				RETURN_VOID_IF_ERROR(MDIButtonSet::Create(&m_controls.mdi_buttonset));
				m_controls.mdi_buttonset->SetSkinIsBackground(TRUE);
				GetRootOpWidget()->AddChild(m_controls.mdi_buttonset, FALSE, TRUE);
			}

			if (!m_controls.wm_buttonset && show_wm_controls)
			{
				RETURN_VOID_IF_ERROR(WMButtonSet::Create(&m_controls.wm_buttonset));
				m_controls.wm_buttonset->SetSkinIsBackground(TRUE);
				GetRootOpWidget()->AddChild(m_controls.wm_buttonset, FALSE, TRUE);
			}

			if (m_controls.mdi_buttonset)
				m_controls.mdi_buttonset->SetVisibility(show_mdi_controls);
			if (m_controls.wm_buttonset)
				m_controls.wm_buttonset->SetVisibility(show_wm_controls);

			if (m_controls.mdi_buttonset || m_controls.wm_buttonset)
				UpdateButtonSets();

			pagebar->SetAllowMenuButton(show_o_menu);
			pagebar->EnsureMenuButton(TRUE);
		}
	}
}


void ToplevelWindow::UpdateButtonSets()
{
	DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(GetOpWindow());
	if (dw)
	{
		OpPagebar* pagebar = static_cast<OpPagebar*>(dw->GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR));
		if (pagebar)
		{
			int padw = 0;
			int padh = 0;
			int offset = 0;

			int width, height;
			GetSize(&width, &height);

			int ph = pagebar->GetHeight();

			if (m_controls.wm_buttonset && m_controls.wm_buttonset->IsVisible())
			{
				int y, w, h;
				m_controls.wm_buttonset->GetPreferedSize(&w, &h, 0, 0);
				y = (ph-h)/2;
				GetRootOpWidget()->SetChildRect(m_controls.wm_buttonset, OpRect(width-w-offset-m_decoration->Width(), y, w, h));
				offset += w;
				padw += w;
				padh = MAX(padh, h);
			}
			if (m_controls.mdi_buttonset && m_controls.mdi_buttonset->IsVisible())
			{
				int y, w, h;
				m_controls.mdi_buttonset->GetPreferedSize(&w, &h, 0, 0);
				y = (ph-h)/2;
				GetRootOpWidget()->SetChildRect(m_controls.mdi_buttonset, OpRect(width-w-offset-m_decoration->Width(), y, w, h));
				padw += w;
				padh = MAX(padh, h);
			}

			pagebar->SetExtraTopPaddings(0, 0, padh, padw);
		}
	}
}


bool ToplevelWindow::IsOperaMenuInTopPagebar() const
{
	if (IsPagebarOnTop())
	{
		if (GetWindowState() == MAXIMIZED || GetWindowState() == NORMAL)
			return true;
		else if (GetDecorationType() == DECORATION_WM && IsMDIButtonsVisible())
			return true;
	}

	return false;
}


bool ToplevelWindow::IsPagebarOnTop() const
{
	DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(GetOpWindow());
	if (dw)
	{
		OpPagebar* pagebar = static_cast<OpPagebar*>(dw->GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR));
		if (pagebar)
		{
			return pagebar->GetAlignment() == OpBar::ALIGNMENT_TOP;
		}
	}
	return false;
}


bool ToplevelWindow::IsPagebarVisible() const
{
	DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(GetOpWindow());
	if (dw)
	{
		OpPagebar* pagebar = static_cast<OpPagebar*>(dw->GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR));
		if (pagebar)
		{
			return pagebar->GetAlignment() == OpBar::ALIGNMENT_TOP || pagebar->GetAlignment() == OpBar::ALIGNMENT_RIGHT || pagebar->GetAlignment() == OpBar::ALIGNMENT_LEFT || pagebar->GetAlignment() == OpBar::ALIGNMENT_BOTTOM;
		}
	}
	return false;
}

bool ToplevelWindow::IsMenuBarVisible() const
{
	return m_menu_bar->IsVisible();
}

bool ToplevelWindow::IsOMenuVisible() const
{
	return !m_menu_bar->IsVisible();
}

bool ToplevelWindow::IsMDIButtonsVisible() const
{
	return !g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCloseButtons);
}

bool ToplevelWindow::IsSkinnedDecorationVisible() const
{
	return m_decoration->IsVisible();
}

bool ToplevelWindow::NeedSkinnedDecoration() const
{
	// MDI buttons are painted in the menu bar if present. Otherwise we
	// have to make room by using the top decoration area. This is possible
	// even in DECORATION_WM mode
	if (IsMDIButtonsVisible() && (!IsMenuBarVisible() || !IsPagebarOnTop()) && GetWindowState() != FULLSCREEN)
		return true;

	return false;
}


void ToplevelWindow::UpdateWorkArea()
{
	int w, h;
	GetSize(&w, &h);

	w -= m_decoration->Width();
	h -= (m_decoration->Height() + m_menu_bar->Height());

	if (w > 0 && h > 0)
	{
		m_work_area->Move(m_decoration->Left(), m_decoration->Top()+m_menu_bar->Height());
		m_work_area->Resize(w, h);
		GetOpWindow()->SetContentSize(w, h);
		SetDropOffset(OpPoint(m_decoration->Left(),m_decoration->Top()+m_menu_bar->Height()));
	}
}


OP_STATUS ToplevelWindow::SetupMenuBar()
{
	if (m_menu_bar.get())
	{
		m_menu_bar->Reset();
	}
	else
	{
		MenuBar* menu_bar = OP_NEW(MenuBar,());
		RETURN_OOM_IF_NULL(menu_bar);
		m_menu_bar.reset(menu_bar);
		RETURN_IF_ERROR(m_menu_bar->Init(this, GetOpWindow()));
	}

	return m_menu_bar->Populate();
}


OP_STATUS ToplevelWindow::SetupWorkarea()
{
	if (!m_work_area.get())
	{
		ToplevelWindowChild* work_area = OP_NEW(ToplevelWindowChild, ());
		RETURN_OOM_IF_NULL(work_area);
		m_work_area.reset(work_area);
		RETURN_IF_ERROR(m_work_area->Init(this));
	}
	return OpStatus::OK;
}


OP_STATUS ToplevelWindow::SetupSkinnedDecoration()
{
	if (!m_decoration.get())
	{
		X11WindowDecoration* decoration = OP_NEW(X11WindowDecoration,());
		RETURN_OOM_IF_NULL(decoration);
		m_decoration.reset(decoration);
		return m_decoration->Init(this, GetOpWindow());
	}
	return OpStatus::OK;
}


void ToplevelWindow::ShowSkinnedDecoration(bool show)
{
	m_next.is_decoration_visible = show || NeedSkinnedDecoration();
	PostUpdateLayout();
}


void ToplevelWindow::GetContentMargins(int &top, int &bottom, int &left, int &right)
{
	top = m_decoration->Top() + m_menu_bar->Height();
	bottom = m_decoration->Bottom();
	left = m_decoration->Left();
	right = m_decoration->Right();
}


void ToplevelWindow::GetMargin(INT32 *left, INT32 *top, INT32 *right, INT32 *bottom)
{
	*top    = m_decoration->Top() + m_menu_bar->Height();
	*left   = m_decoration->Left();
	*right  = m_decoration->Right();
	*bottom = m_decoration->Bottom();
}

void ToplevelWindow::OnToplevelActivate()
{
	GetOpWindow()->UpdateBackgroundListener();
	m_menu_bar->Activate();
	m_decoration->Activate();
}

void ToplevelWindow::OnToplevelDeactivate()
{
	GetOpWindow()->UpdateBackgroundListener();
	m_menu_bar->Deactivate();
	m_decoration->Deactivate();
}

BrowserDesktopWindow* ToplevelWindow::GetBrowserDesktopWindow()
{
	DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(GetOpWindow());
	if (dw && dw->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		return (BrowserDesktopWindow*)dw;
	}

	return 0;
}


//static 
BrowserDesktopWindow* ToplevelWindow::GetBrowserDesktopWindow(X11Types::Window window)
{
	for (UINT32 i=0; i<m_window_list.GetCount(); i++)
	{
		if (m_window_list.Get(i)->GetWindowHandle() == window)
		{
			return m_window_list.Get(i)->GetBrowserDesktopWindow();
		}
	}
	return 0;
}


BOOL ToplevelWindow::GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out)
{
	return m_menu_bar->GetMenubarButtons(out);
}

OpWidget* ToplevelWindow::GetRootOpWidget() const
{
	return GetOpWindow()->GetDesktopWindow()->GetWidgetContainer()->GetRoot();
}
