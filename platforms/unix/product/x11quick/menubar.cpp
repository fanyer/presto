/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "menubar.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/menus/MenuShortcutLetterFinder.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/quick/widgets/OpToolbarMenuButton.h"

#include "modules/display/vis_dev.h"
#include "modules/pi/OpPainter.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpView.h"
#include "modules/pi/OpWindow.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/unicode/unicode.h"
#include "modules/util/OpLineParser.h"
#include "modules/widgets/OpButton.h"

#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_mdescreen.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/unix/base/x11/x11_windowwidget.h"
#include "platforms/utilix/x11_all.h"


class MenuBackgroundWidget : public OpWidget
{
public:
	static OP_STATUS Create(MenuBackgroundWidget** obj, MenuBar* menu)
	{
		*obj = OP_NEW(MenuBackgroundWidget, (menu));
		if (*obj == NULL)
			return OpStatus::ERR_NO_MEMORY;
		if (OpStatus::IsError((*obj)->init_status))
		{
			OP_DELETE(*obj);
			return OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::OK;
	}

protected:
	MenuBackgroundWidget(MenuBar* menu) : m_menu(menu) {}

	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
	{
		m_menu->OnPaint(paint_rect, m_menu->GetOpView());
	}

	virtual void OnMouseMove(const OpPoint &point)
	{
		m_menu->OnMouseMove(point, 0, m_menu->GetOpView());
	}

	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		m_menu->OnMouseDown(button, nclicks, 0, m_menu->GetOpView());
	}

	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		m_menu->OnMouseUp(button, 0, m_menu->GetOpView());
	}

	virtual void OnSetCursor(const OpPoint &point) { /* Just override. We handle this manually*/ }

private:
	MenuBar* m_menu;
};


class MenuBarButton : public OpButton, public PopupMenu::PopupMenuListener
{
public:
	static OP_STATUS Create(MenuBarButton** obj, MenuBar* menu_bar)
	{
		*obj = OP_NEW(MenuBarButton, (menu_bar));
		if (*obj == NULL)
			return OpStatus::ERR_NO_MEMORY;
		if (OpStatus::IsError((*obj)->init_status))
		{
			OP_DELETE(*obj);
			return OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::OK;
	}

protected:
	MenuBarButton(MenuBar* menu_bar)
		:m_menu_bar(menu_bar)
	{
		SetButtonTypeAndStyle(OpButton::TYPE_MENU, OpButton::STYLE_TEXT);
		string.SetConvertAmpersand(TRUE);
		string.SetDrawAmpersandUnderline(TRUE);
	}

	virtual void OnMouseMove(const OpPoint &point)
	{
		OpButton::OnMouseMove(point);
		// Menu bar expects coordinates in its space
		m_menu_bar->OnMouseMove(point + GetRect(FALSE).TopLeft(), 0, m_menu_bar->GetOpView());
	}

	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		OpButton::OnMouseDown(point, button, nclicks);
		m_menu_bar->OnMouseDown(button, nclicks, 0, m_menu_bar->GetOpView());
	}

	// Implement PopupMenu::PopupMenuListener
	virtual void OnPopupOpened()
	{
		packed2.is_pushed = packed2.is_down = TRUE;
		packed2.is_outside = FALSE;
		Update();
	}

	virtual void OnPopupClosed()
	{
		packed2.is_pushed = packed2.is_down = FALSE;
		packed2.is_outside = TRUE;
		Update();
	}

private:
	MenuBar* m_menu_bar;
};



MenuBar::MenuBar()
	:m_corner_size(25)
	,m_resize_margin(3)
	,m_active_menu(NULL)
	,m_border_color(0)
	,m_height(0)
	,m_visible(false)
	,m_has_open_menu(false)
	,m_has_pending_exec(false)
	,m_holds_focus(false)
	,m_keep_focus(false)
	,m_active(false)
	,m_opened_with_mouse(false)
	,m_captured(false)
	,m_can_move_resize(true)
	,m_draw_border(false)
{
}

MenuBar::~MenuBar()
{
	if (m_active)
		((X11DesktopGlobalApplication*)g_desktop_global_application)->RemoveGlobalKeyEventListener(this);

	m_menus.Clear();
}


void MenuBar::Reset()
{
	CancelSubMenu();

	// Just destroying MenuBarItems doesn't delete the associated
	// MenuBarButtons.  Do that manually first.
	for (MenuBarItem* item = static_cast<MenuBarItem*>(m_menus.First());
			item != NULL;
			item = static_cast<MenuBarItem*>(item->Suc()))
	{
		item->button->Delete();
	}

	m_menus.Clear();
}



OP_STATUS MenuBar::Init(void* parent_handle, OpWindow* parent_window)
{
	OpWindow* window;
	RETURN_IF_ERROR(OpWindow::Create(&window));
	m_op_window.reset(window);
	RETURN_IF_ERROR(m_op_window->Init(OpWindow::STYLE_UNKNOWN, OpTypedObject::WINDOW_TYPE_UNKNOWN, parent_window, 0, parent_handle));

	VisualDevice* vis_dev = VisualDevice::Create(m_op_window.get(), 0, VisualDevice::VD_SCROLLING_NO);
	RETURN_OOM_IF_NULL(vis_dev);
	m_vis_dev.reset(vis_dev);

	WidgetContainer* widget_container = OP_NEW(WidgetContainer, ());
	RETURN_OOM_IF_NULL(widget_container);
	m_widget_container.reset(widget_container);
	RETURN_IF_ERROR(m_widget_container->Init(OpRect(0,0,0,0), m_op_window.get()));

	Controls* controls = OP_NEW(Controls,());
	RETURN_OOM_IF_NULL(controls);
	m_controls.reset(controls);

	RETURN_IF_ERROR(OpToolbarMenuButton::Construct(&m_controls->menu));
	m_widget_container->GetRoot()->AddChild(m_controls->menu, TRUE);
	m_controls->menu->SetParentInputContext(this);
	m_controls->menu->SetSystemFont(OP_SYSTEM_FONT_UI_TOOLBAR);
	m_controls->menu->SetVisibility(false); // regular buttons are visible by default

	RETURN_IF_ERROR(WMButtonSet::Create(&m_controls->wm_buttonset));
	m_widget_container->GetRoot()->AddChild(m_controls->wm_buttonset, TRUE);
	m_controls->wm_buttonset->SetVisibility(false);

	RETURN_IF_ERROR(MDIButtonSet::Create(&m_controls->mdi_buttonset));
	m_widget_container->GetRoot()->AddChild(m_controls->mdi_buttonset, TRUE);
	m_controls->mdi_buttonset->SetVisibility(false);

	RETURN_IF_ERROR(MenuBackgroundWidget::Create(&m_controls->background, this));
	m_widget_container->GetRoot()->AddChild(m_controls->background, TRUE);

	static_cast<X11OpWindow*>(m_op_window.get())->RegisterAsMenuBar();

	OpView* view;
	RETURN_IF_ERROR(OpView::Create(&view, m_op_window.get()));
	m_op_view.reset(view);

	GetOpView()->SetMouseListener(this);
	GetOpView()->SetPaintListener(this);

	return OpStatus::OK;
}


OP_STATUS MenuBar::Populate()
{
	PrefsSection* section = g_setup_manager->GetSectionL("Browser Menu Bar", OPMENU_SETUP);
	OP_ASSERT(section);
	if (!section)
		return OpStatus::ERR;

	MenuShortcutLetterFinder letter_finder;
	for (const PrefsEntry *entry = section->Entries(); entry; entry = static_cast<const PrefsEntry *>(entry->Suc()))
	{
		OpLineParser line(entry->Key());

		OpString item_name, default_item_name;
		OpString8 sub_menu_name;
		INTPTR sub_menu_value = 0;
		INTPTR menu_value = 0;
		QuickMenu::MenuType menu_type;
		BOOL ghost_item = FALSE;
		OpWidgetImage image;

		if (QuickMenu::ParseMenuEntry(&line, item_name, default_item_name, sub_menu_name, sub_menu_value, menu_value, menu_type, ghost_item, image)
			&& menu_type == QuickMenu::MENU_SUBMENU)
		{
			OpString item_name_new;
			letter_finder.CreateMenuString(item_name.CStr(), item_name_new);
			AddPopupMenu(sub_menu_name.CStr(), item_name_new.CStr(), sub_menu_value);
		}
	}
	OP_DELETE(section);
	return OpStatus::OK;
}




UINT32 MenuBar::CalculateHeight()
{
	m_toolkit_layout.frame_thickness = 0;
	m_toolkit_layout.spacing = 0;
	m_toolkit_layout.h_margin = 0;
	m_toolkit_layout.v_margin = 0;
	m_toolkit_layout.bottom_margin = 0;
	m_toolkit_layout.separate_help = false;
	g_toolkit_library->GetMenuBarLayout(m_toolkit_layout);

	int height = 0;
	for (MenuBarItem *m=(MenuBarItem *)m_menus.First(); m; m=(MenuBarItem *)m->Suc())
	{
		int w, h;
		m->button->GetPreferedSize(&w, &h, 0, 0);
		if (height < h)
			height = h;
	}

	INT32 item_width, item_height;

	// Button set where we display mdi min/max/restore/close buttons
	m_controls->mdi_buttonset->GetPreferedSize(&item_width, &item_height, 0, 0);
	if (height < item_height)
		height = item_height;

	// Button set where we display system min/max/restore/close buttons
	m_controls->wm_buttonset->GetPreferedSize(&item_width, &item_height, 0, 0);
	if (height < item_height)
		height = item_height;

	height += 2*(m_toolkit_layout.frame_thickness+m_toolkit_layout.v_margin) + m_toolkit_layout.bottom_margin;

	return MAX(0, height);
}


void MenuBar::SetGeometry(int x, int y, int width, int height)
{
	m_op_window->SetOuterPos(x, y);
	m_op_view->SetPos(0, 0);
	SetSize(width, height);
}


void MenuBar::SetDisplayMode(DisplayMode mode)
{
	m_controls->menu->SetVisibility(mode == OPERA_MENU);

	for (MenuBarItem *m=(MenuBarItem *)m_menus.First(); m; m=(MenuBarItem *)m->Suc())
		m->button->SetVisibility(mode == ROW_MENU);
}


void MenuBar::SetSize(int width, int height)
{
	UINT32 w, h;
	m_op_window->GetOuterSize(&w, &h);
	if (width != (int)w || height != (int)h)
	{
		m_height = height;
		m_op_window->SetOuterSize(width, height);
		m_op_view->SetSize(width, height);
	}

	OpWidget* root = m_widget_container->GetRoot();
	root->SetRTL(UiDirection::Get() == UiDirection::RTL);
	root->SetRect(OpRect(0,0,width,height));

	OpRect rect;
	int item_width, item_height;

	rect.x = 3;
	for (MenuBarItem* m=(MenuBarItem *)m_menus.First(); m; m=(MenuBarItem *)m->Suc())
	{
		m->button->GetPreferedSize(&item_width, &item_height, 0, 0);
		rect.y = 1;
		rect.width  = item_width;
		rect.height = item_height;
		root->SetChildRect(m->button, rect);
		rect.x += rect.width;
	}

	m_controls->menu->GetPreferedSize(&item_width, &item_height, 0, 0);
	root->SetChildRect(m_controls->menu, OpRect(0, 0, item_width, item_height));

	int button_offset = 1;

	// Button set where we display system min/max/restore/close buttons
	m_controls->wm_buttonset->GetPreferedSize(&item_width, &item_height, 0, 0);
	rect = OpRect(width-item_width-1, (height-item_height)/2, item_width, item_height);
	if (rect.height > height)
	{
		rect.y      = 0;
		rect.height = height;
	}
	root->SetChildRect(m_controls->wm_buttonset, rect);

	if (m_controls->wm_buttonset->IsVisible())
		button_offset += rect.width + 1;

	// Button set where we display mdi min/max/restore/close buttons
	m_controls->mdi_buttonset->GetPreferedSize(&item_width, &item_height, 0, 0);
	rect = OpRect(width-button_offset-item_width, (height-item_height)/2, item_width, item_height);
	if (rect.height > height)
	{
		rect.y      = 0;
		rect.height = height;
	}
	root->SetChildRect(m_controls->mdi_buttonset, rect);

	// Background widget
	m_controls->background->SetRect(OpRect(0, 0, width, height));

	// The view itself
	m_widget_container->GetView()->SetSize(width, height);
}

void MenuBar::Show()
{
	m_visible = true;
	m_op_window->Show(TRUE);
}

void MenuBar::Hide()
{
	m_visible = false;
	m_op_window->Show(FALSE);
}


void MenuBar::Activate()
{
	if (!m_active)
	{
		m_active = TRUE;
		((X11DesktopGlobalApplication*)g_desktop_global_application)->AddGlobalKeyEventListener(this);
	}
}

void MenuBar::Deactivate()
{
	if (m_active)
	{
		m_active = FALSE;
		((X11DesktopGlobalApplication*)g_desktop_global_application)->RemoveGlobalKeyEventListener(this);
	}
}


bool MenuBar::IsInside(int x, int y, bool is_global_coordinates)
{
	UINT32 w, h;
	m_op_view->GetSize(&w, &h);
	OpPoint p = is_global_coordinates ? m_op_view->ConvertToScreen(OpPoint(0,0)) : OpPoint(0,0);
	return x >= p.x && y >= p.y && x < (int)(p.x+w) && y < (int)(p.y+h);
}


void MenuBar::SetLocalFocus(bool on)
{
	if (on)
	{
		if (!m_holds_focus)
		{
		}
	}
	else
	{
		if (m_holds_focus)
		{
			m_holds_focus = false;

			X11MdeScreen* mde_screen = static_cast<X11OpWindow*>(m_op_window.get())->GetMdeScreen();
			if (mde_screen)
				mde_screen->ReleaseMouseCapture();
		}
	}
}


OP_STATUS MenuBar::AddPopupMenu(const char *name, const uni_char *visible_name, INTPTR value)
{
	OpAutoPtr<MenuBarItem> menu_bar_item (OP_NEW(MenuBarItem, ()));
	if (!menu_bar_item.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(MenuBarButton::Create(&menu_bar_item->button, this));
	RETURN_IF_ERROR(menu_bar_item->button->SetText(visible_name));
	m_widget_container->GetRoot()->AddChild(menu_bar_item->button);
	menu_bar_item->button->SetZ(OpWidget::Z_TOP);
	menu_bar_item->button->SetParentInputContext(this);

	RETURN_IF_ERROR(menu_bar_item->name.Set(name));
	RETURN_IF_ERROR(menu_bar_item->visible_name.Set(visible_name));
	menu_bar_item->value = value;

	menu_bar_item->Into(&m_menus);
	menu_bar_item.release();

	return OpStatus::OK;
}



void MenuBar::Stop(bool cancel)
{
	m_active_menu = 0;

	m_has_open_menu = false;

	m_keep_focus = false;
	SetLocalFocus(false);

	Invalidate();
}


void MenuBar::CancelSubMenu()
{
	if (m_active_menu && m_active_menu->menu)
	{
		m_active_menu->menu->Cancel();
	}
	m_active_menu = 0;
	m_has_open_menu = false;

	if (m_holds_focus && !m_keep_focus)
	{
		SetLocalFocus(false);
	}


	Invalidate();
}



void MenuBar::Show(MenuBarItem* item)
{
	if (!item || item == m_active_menu && m_active_menu->menu)
	{
		return;
	}

	m_keep_focus = true;

	CancelSubMenu();
	Invalidate();

	if (m_has_pending_exec)
	{
		m_active_menu = item;
		Invalidate();
		return;
	}

	m_active_menu = item;

	m_has_pending_exec = true;
	m_pending_exec_rect = m_active_menu->button->GetRect(FALSE);

	m_opened_with_mouse = false;
	PostCallback(0);
	Invalidate();
}




void MenuBar::ShowNext(MenuElement* current, bool next)
{
	MenuBarItem* item = m_active_menu;
	MenuBarItem* old_m_active_menu = m_active_menu;

	m_keep_focus = true;

	CancelSubMenu();

	if (item)
	{
		if (next)
		{
			for (MenuBarItem* m=(MenuBarItem *)m_menus.First(); m; m=(MenuBarItem *)m->Suc())
			{
				if (item == m)
				{
					item = (MenuBarItem *)m->Suc();
					if (!item)
						item = (MenuBarItem *)m_menus.First();
					break;
				}
			}
			if (item == old_m_active_menu)
				item = 0;
		}
		else
		{
			for (MenuBarItem *m=(MenuBarItem *)m_menus.Last(); m; m=(MenuBarItem *)m->Pred())
			{
				if (item == m)
				{
					item = (MenuBarItem *)m->Pred();
					if (!item)
						item = (MenuBarItem *)m_menus.Last();
					break;
				}
			}
			if (item == old_m_active_menu)
				item = 0;
		}
	}

	if (item)
	{
		if (m_has_pending_exec)
		{
			m_active_menu = item;
			Invalidate();
			return;
		}

		m_active_menu = item;

		m_has_pending_exec = true;
		m_pending_exec_rect = m_active_menu->button->GetRect(FALSE);

		m_opened_with_mouse = false;
		PostCallback(0);
		Invalidate();

	}
}


bool MenuBar::HandleMouseEvent(int gx, int gy, MouseEvent event, bool confine_to_area)
{
	if (!IsInside(gx, gy, true))
		return false;

	switch (event)
	{
		case POINTER_MOTION:
		{
			if (PopupMenu::SupportMouseHover())
			{
				OpPoint p = m_op_view->ConvertToScreen(OpPoint(0, 0));
				OpPoint point(gx-p.x, gy-p.y);
				HandleMouseEventInternal(point, false);
			}
			break;
		}
		case BUTTON_PRESS:
		case BUTTON_RELEASE:
			break;
	}

	return true;
}


bool MenuBar::ShowSubmenu(int gx, int gy)
{
	m_keep_focus = true;

	CancelSubMenu();

	if (IsInside(gx, gy, true))
	{
		OpPoint p = m_op_view->ConvertToScreen(OpPoint(0, 0));
		OpPoint point(gx-p.x, gy-p.y);
		HandleMouseEventInternal(point, true);
	}

	if (!m_has_pending_exec)
	{
		m_keep_focus = false;
		SetLocalFocus(false);
	}

	return true;
}


bool MenuBar::IsCaptured() const
{
	X11Widget* captured = g_x11->GetWidgetList()->GetCapturedWidget();
	if (captured && m_captured)
	{
		X11Widget* widget = static_cast<X11OpWindow*>(m_op_window.get())->GetOuterWidget();
		return captured == widget;
	}
	return false;
}


X11Widget::MoveResizeAction MenuBar::GetAction(const OpPoint& point, UINT32 width, UINT32 height)
{
	X11Widget::MoveResizeAction action = X11Widget::MOVE_RESIZE_NONE;
	if (point.x < m_corner_size)
		action = X11Widget::MOVE_RESIZE_TOPLEFT;
	else if (static_cast<int>(width) - point.x < m_corner_size)
		action = X11Widget::MOVE_RESIZE_TOPRIGHT;
	else
		action = point.y > m_resize_margin ? X11Widget::MOVE_RESIZE_MOVE : X11Widget::MOVE_RESIZE_TOP;

	return action;
}


X11Widget::MoveResizeAction MenuBar::UpdateCursorShape(const OpPoint& point)
{
	UINT32 width, height;
	GetOpView()->GetSize(&width, &height);

	X11Widget::MoveResizeAction action;

	if (m_can_move_resize)
	{
		action = GetAction(point, width, height);
		switch (action)
		{
		case X11Widget::MOVE_RESIZE_LEFT:
			m_op_window->SetCursor(CURSOR_W_RESIZE);
			break;
		case X11Widget::MOVE_RESIZE_RIGHT:
			m_op_window->SetCursor(CURSOR_E_RESIZE);
			break;
		case X11Widget::MOVE_RESIZE_TOP:
			m_op_window->SetCursor(CURSOR_N_RESIZE);
			break;
		case X11Widget::MOVE_RESIZE_BOTTOM:
			m_op_window->SetCursor(CURSOR_S_RESIZE);
			break;
		case X11Widget::MOVE_RESIZE_TOPLEFT:
			m_op_window->SetCursor(CURSOR_NW_RESIZE);
			break;
		case X11Widget::MOVE_RESIZE_TOPRIGHT:
			m_op_window->SetCursor(CURSOR_NE_RESIZE);
			break;
		case X11Widget::MOVE_RESIZE_BOTTOMLEFT:
			m_op_window->SetCursor(CURSOR_SW_RESIZE);
			break;
		case X11Widget::MOVE_RESIZE_BOTTOMRIGHT:
			m_op_window->SetCursor(CURSOR_SE_RESIZE);
			break;
		case X11Widget::MOVE_RESIZE_MOVE:
			m_op_window->SetCursor(IsCaptured() ? CURSOR_MOVE : CURSOR_DEFAULT_ARROW);
			break;
		default:
			m_op_window->SetCursor(CURSOR_DEFAULT_ARROW);
			break;
		}
	}
	else
	{
		action = X11Widget::MOVE_RESIZE_NONE;
		m_op_window->SetCursor(CURSOR_DEFAULT_ARROW);
	}

	if (m_controls.get())
	{
		m_controls->background->SetCursor(m_op_window->GetCursor());
	}

	return action;
}



void MenuBar::OnPaint(const OpRect &rect, OpView* view)
{
	OpPainter* painter = view->GetPainter(rect);
	if (!painter)
		return;

	UINT32 width, height;
	view->GetSize(&width, &height);
	OpRect view_rect(0, 0, width, height);

	UINT32 color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_BACKGROUND);
	painter->SetColor(OP_GET_R_VALUE(color), OP_GET_G_VALUE(color), OP_GET_B_VALUE(color), OP_GET_A_VALUE(color));
	painter->FillRect(view_rect);

	m_vis_dev->BeginPaint(painter, view_rect, rect);

	const char* skin_element_name = g_x11->HasPersonaSkin() ? "Menu Transparent Skin" : "Menu Skin";
	g_skin_manager->DrawElement(m_vis_dev.get(), skin_element_name, view_rect);

	if (m_draw_border)
	{
		painter->SetColor(m_border_color);
		painter->DrawLine(OpPoint(0,0), width, TRUE);
	}

	m_vis_dev->EndPaint();
	view->ReleasePainter(rect);
}

void MenuBar::OnMouseMove(const OpPoint &point, ShiftKeyState shift_state, OpView* view)
{
	X11Widget::MoveResizeAction action = UpdateCursorShape(point);

	OpPoint gp = m_op_view->ConvertToScreen(point);
	HandleMouseEvent(gp.x, gp.y, POINTER_MOTION, true);

	if (!m_active_menu && IsCaptured())
	{
		if (m_can_move_resize)
		{
			X11Widget* widget = static_cast<X11OpWindow*>(m_op_window.get())->GetOuterWidget();
			widget->GetTopLevel()->MoveResizeWindow(action);
		}

		m_captured = false;
		UpdateCursorShape(point);
	}

}

void MenuBar::OnMouseDown(MouseButton button, UINT8 nclicks, ShiftKeyState shift_state, OpView *view)
{
	if (button == MOUSE_BUTTON_1)
	{
		if (nclicks == 1)
		{
			m_captured = true;
			OpPoint point;
			view->GetMousePos(&point.x, &point.y);

			HandleMouseEventInternal(point, true);
		}
		else if (nclicks == 2)
		{
			X11Widget* widget = static_cast<X11OpWindow*>(m_op_window.get())->GetOuterWidget();
			widget->GetTopLevel()->SetWindowState(
				widget->GetTopLevel()->GetWindowState() == X11Widget::NORMAL ?
				X11Widget::MAXIMIZED : X11Widget::NORMAL);
		}
	}
}

void MenuBar::OnMouseUp(MouseButton button, ShiftKeyState shift_state, OpView* view)
{
	if (button == MOUSE_BUTTON_2)
	{
		if (!m_active_menu)
		{
			// Can't do this in Init as the desktop window is not fully set up then
			DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(m_op_window.get()->GetRootWindow());
			g_input_manager->SetMouseInputContext(dw);

			g_application->GetMenuHandler()->ShowPopupMenu("Menubar Menu", PopupPlacement::AnchorAtCursor());
		}
	}
}

void MenuBar::OnMouseLeave()
{
	if (!m_has_open_menu && m_active_menu)
	{
		m_active_menu = 0;
		Invalidate();
	}
}

void MenuBar::Invalidate()
{
	m_controls->background->InvalidateAll();
}

OP_STATUS MenuBar::PopulateMenu(MenuBarItem* m)
{
	if (m->menu)
		return OpStatus::OK;

	m->menu = OP_NEW(PopupMenu, ());
	if (!m->menu)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS result = m->menu->Init(m_op_view.get());
	if (OpStatus::IsError(result))
	{
		OP_DELETE(m->menu);
		m->menu = 0;
		return result;
	}
	m->menu->SetParent(this);

	// Use keyboard context. Otherwise we will not be able to paste text into form fields etc 
	OpInputContext* ctx = g_input_manager->GetKeyboardInputContext();
	g_application->GetMenuHandler()->AddMenuItems(m->menu, m->name.CStr(), 0, 0, ctx);
	m->menu->SetExecContext(PopupMenu::KeyboardContext);

	return OpStatus::OK;
}

MenuBar::MenuBarItem* MenuBar::GetMenuBarItemAt(const OpPoint& p, OpRect* item_dimensions)
{
	for (MenuBarItem *m=(MenuBarItem *)m_menus.First(); m; m=(MenuBarItem *)m->Suc())
	{
		if (m->button->IsVisible())
		{
			OpRect rect = m->button->GetRect(FALSE);
			if (rect.Contains(p))
			{
				if (item_dimensions)
					*item_dimensions = rect;
				return m;
			}
		}
	}
	return NULL;
}


void MenuBar::HandleMouseEventInternal(const OpPoint& point, bool was_just_pressed)
{
	OpRect rect;
	MenuBarItem* hovered_menu_item = GetMenuBarItemAt(point, &rect);

	if (m_has_pending_exec)
	{
		if (hovered_menu_item && hovered_menu_item != m_active_menu)
		{
			m_active_menu = hovered_menu_item;
			m_pending_exec_rect = rect;
			Invalidate();
		}
		return;
	}

	if (m_active_menu && m_active_menu->menu && hovered_menu_item && hovered_menu_item != m_active_menu)
	{
		m_active_menu->menu->Cancel();
	}

	MenuBarItem *old_active = m_active_menu;
	if (hovered_menu_item || !m_has_open_menu)
		m_active_menu = hovered_menu_item;

	if (old_active != m_active_menu || was_just_pressed)
	{
		if (m_active_menu && (m_has_open_menu || InputUtils::IsOpButtonPressed(MOUSE_BUTTON_1, true)))
		{
			m_pending_exec_rect = rect;
			if (!m_has_pending_exec)
			{
				m_has_pending_exec = true;
				m_opened_with_mouse = true;
				PostCallback(0);
			}
		}
		Invalidate();
	}
}


void MenuBar::HandleCallBack(INTPTR ev)
{
	m_has_pending_exec = false;
	if (!m_active_menu)
		return;

	// This menu and the O-menu are both opened after a timeout 
	// Testing here will prevent showing both.
	if (g_application->GetMenuHandler()->IsShowingMenu())
		return;

	m_has_open_menu = true;
	PopulateMenu(m_active_menu);
	if (!m_active_menu->menu)
		return;

	Invalidate();

	// So that new popup menu will have menubar as parent context (prevents focus flicker when
	// switching menus when focus is in a text field)
	SetLocalFocus(true);
	m_keep_focus = false;

	m_active_menu->menu->SetSelectedItem(m_opened_with_mouse ? -1 : 0);

	const OpPoint& point_on_screen = m_op_view->ConvertToScreen(m_pending_exec_rect.TopLeft());
	const OpRect bar_item_rect(
			point_on_screen.x, point_on_screen.y, m_pending_exec_rect.width, m_pending_exec_rect.height);

	OpScopeDesktopWindowManager_SI::QuickMenuInfo info;

	OpString name;
	name.Set(m_active_menu->name); 

	if (m_active_menu->menu->GetScopeName().IsEmpty())
		m_active_menu->menu->SetScopeName(m_active_menu->name);
	m_active_menu->menu->SetQuickMenuInfo(info);
	m_active_menu->menu->SetIsTopPopupMenu(); 
	g_application->GetMenuHandler()->OnMenuShown(TRUE, info);

	m_active_menu->menu->SetPopupMenuListener(m_active_menu->button);

	PopupMenu::Exec(m_active_menu->menu, PopupMenu::BELOW_OR_ABOVE, bar_item_rect, false, true);

	g_application->GetMenuHandler()->OnMenuShown(FALSE, info);

	m_captured = false;

	// Remove menu from memory. 'm_active_menu' is NULL if menu was closed, but will
	// be NON-NULL if we opened a sibling menu from the first. In the latter case
	// 'm_active_menu' is already the sibling menu
	for (MenuBarItem* m = (MenuBarItem*)m_menus.First(); m; m = (MenuBarItem *)m->Suc())
	{
		if (m != m_active_menu)
		{
			OP_DELETE(m->menu);
			m->menu = 0;
		}
	}
}


bool MenuBar::OnGlobalKeyEvent(XEvent *event)
{
	if (event->type == KeyPress)
	{
		int mask = ShiftMask|ControlMask|Mod1Mask;
		if ((event->xkey.state&mask) == Mod1Mask)
		{
			if (g_startup_settings.debug_keyboard)
				printf("Key press: Test menu bar: ");

			KeySym keysym = NoSymbol;
			OpString keytext;
			InputUtils::GetKey(event, keysym, keytext);
			UINT32 value = 0;
			if (keytext.HasContent())
			{
				int valuelen = 0;
				value = Unicode::GetUnicodePoint(keytext.CStr(), keytext.Length(), valuelen);
				if (valuelen < keytext.Length())
				{
					// Only allow single-character shortcuts
					value = 0;
				};
			};
			if (value != 0)
			{
				value = Unicode::ToLower(value);

				MenuBarItem* item = 0;
				for (MenuBarItem *m=(MenuBarItem *)m_menus.First(); m; m=(MenuBarItem *)m->Suc())
				{
					int pos = m->visible_name.FindFirstOf('&');
					if (pos != KNotFound)
					{
						if (Unicode::ToLower(m->visible_name.CStr()[pos+1]) == value)
						{
							item = m;
							break;
						}
					}
				}
				if (item)
				{
					if (g_startup_settings.debug_keyboard)
						printf("match\n");
					Show(item);
					return true;
				}
			}

			if (g_startup_settings.debug_keyboard)
				printf("no match\n");
		}
	}
	return false;
}


OP_STATUS MenuBar::SetQuickMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuInfo& info)
{
	info.GetMenuIdRef().SetMenuName(UNI_L(MENU_NAME_MAIN_MENU));

	INT32 x, y;
	m_op_view->GetPos(&x, &y);
	UINT32 width, height;
	m_op_view->GetSize(&width, &height);

	// Set the rect
	info.GetRectRef().SetX(x);
	info.GetRectRef().SetY(y);
	info.GetRectRef().SetWidth(width);
	info.GetRectRef().SetHeight(height);

	// List the items
	int i = 0;
	for (MenuBarItem* mi=(MenuBarItem *)m_menus.First(); mi; mi=(MenuBarItem *)mi->Suc())
	{
		OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuItemInfo> item(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuItemInfo, ()));
		if (item.get() == NULL)
			return OpStatus::ERR;

		// Visible name -> item text
		OpString name;
		name.Set(mi->visible_name);
		item.get()->SetText(name.CStr());
		item.get()->SetEnabled(TRUE);
		item.get()->SetChecked(FALSE);
		item.get()->SetBold(FALSE);

		int pos = mi->visible_name.FindFirstOf('&');
		if (pos != KNotFound)
		{
			OpString letter;
			letter.Set(mi->visible_name.SubString(pos+1, 1));
			item.get()->SetShortcutLetter(letter);
		}

		item->SetRow(i); i++;

		OpRect rect = mi->button->GetRect(FALSE);
		OpPoint p = m_op_view->ConvertToScreen(OpPoint(x+rect.x,y+rect.y));
		// Set the rect
		item.get()->GetRectRef().SetX(p.x);
		item.get()->GetRectRef().SetY(p.y);
		item.get()->GetRectRef().SetWidth(rect.width);
		item.get()->GetRectRef().SetHeight(rect.height);

		// name -> Submenu name
		OpString str;
		str.Set(mi->name);
		item.get()->SetSubmenu(str.CStr());

		info.GetMenuItemListRef().Add(item.get());
		item.release();
	}

	return OpStatus::OK;
}


BOOL MenuBar::GetMenubarButtons(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out)
{
	if (OpStatus::IsSuccess(SetQuickMenuInfo(out)))
		return TRUE;
	return FALSE;
}


void MenuBar::ShowControls(bool show_wm_buttons, bool show_mdi_buttons)
{
	if (m_controls.get())
	{
		bool changed = false;

		if (m_controls->wm_buttonset->IsVisible() != show_wm_buttons)
		{
			m_controls->wm_buttonset->SetVisibility(show_wm_buttons);
			changed = true;
		}

		if (m_controls->mdi_buttonset->IsVisible() != show_mdi_buttons)
		{
			m_controls->mdi_buttonset->SetVisibility(show_mdi_buttons);
			changed = true;
		}

		if (changed)
		{
			// Apply current size again. Updates geometry of button sets
			UINT32 w, h;
			m_op_window->GetOuterSize(&w, &h);
			SetSize(w, h);
		}
	}
}


OpView* MenuBar::GetOpView()
{
	return m_op_view.get();
}
