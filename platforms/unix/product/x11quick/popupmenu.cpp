/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "platforms/unix/product/x11quick/popupmenu.h"
#include "platforms/unix/product/x11quick/popupmenuitem.h"
#include "platforms/unix/product/x11quick/x11_desktop_popup_menu_handler.h"

#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/display/vis_dev.h"
#include "modules/display/styl_man.h"
#include "modules/pi/OpFont.h"
#include "modules/pi/OpPainter.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/pi/OpView.h"
#include "modules/pi/OpWindow.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#undef ProtocolVersion // defined in /usr/include/X11/Xlib.h
#include "modules/scope/src/scope_manager.h"
#include "modules/skin/OpSkinManager.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opview.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_popupwidget.h"
#include "platforms/unix/base/x11/x11_windowwidget.h"

// Setting this to 0 will bypass timer
#define PENDING_MENU_TIMEOUT_MS 250

PopupMenu* PopupMenu::m_hotclick_menu = 0;
PopupMenu* PopupMenu::m_topmost_popup_menu = 0;
PopupMenu* PopupMenu::m_bookmark_context_menu = 0;
// Allows open/close sub menu by mouse (bug #DSK-296014)
bool PopupMenu::m_block_mouse_up = false;
// Action to execute
OpInputAction* PopupMenu::m_exec_action;
PopupMenu::ContextType PopupMenu::m_exec_context;
// Event blocking (bug #DSK-295927)
bool PopupMenu::m_block_after_close = false;

// static
PopupMenu* PopupMenu::GetHotClickMenu()
{
	return m_hotclick_menu;
}

// static
PopupMenu* PopupMenu::GetTopPopupMenu()
{
	return m_topmost_popup_menu;
}

PopupMenu* PopupMenu::GetBookmarkContextMenu()
{
	return m_bookmark_context_menu;
}


//static
BOOL PopupMenu::FilterEvent(XEvent* e)
{
	if (m_block_after_close)
	{
		if (e->type == MotionNotify)
		{
			m_block_after_close = FALSE;
		}
		else if (e->type == ButtonRelease)
		{
			m_block_after_close = FALSE;
			return TRUE;
		}
	}

	return FALSE;
}


//static
bool PopupMenu::SupportMouseHover()
{
	// Depends on active UI style
	bool g_style_with_mouse_hovering = true;
	return g_style_with_mouse_hovering;
}



PopupMenu::PopupMenu()
	:m_opwindow(0)
	,m_opview(0)
	,m_paint_widget(0)
	,m_normalfont(0)
	,m_boldfont(0)
	,m_parent_menu(0)
	,m_open_submenu(0)
	,m_selected_item(0)
	,m_mouse_selected_item(0)
	,m_listener(NULL)
	,m_context_type(UnknownContext)
	,m_image_width(PopupMenuItem::MaxImageSize)
	,m_image_height(PopupMenuItem::MaxImageSize)
	,m_max_text_width(0)
	,m_max_shortcut_width(0)
	,m_auto_shown_time(0.0)
	,m_has_submenus(false)
	,m_cancelled(false)
	,m_mouse_has_moved(false)
	,m_is_o_menu(false)
{
}

PopupMenu::~PopupMenu()
{
	Hide();
	m_menu_items.Clear();
	OP_DELETE(m_opview);
	OP_DELETE(m_opwindow);
	OP_DELETE(m_normalfont);
	OP_DELETE(m_boldfont);
	if (m_hotclick_menu == this)
		m_hotclick_menu = 0;
	if (m_topmost_popup_menu == this)
		m_topmost_popup_menu = 0;
	if (m_bookmark_context_menu == this)
		m_bookmark_context_menu = 0;
	if (m_paint_widget)
	{
		OP_DELETE(m_paint_widget->GetVisualDevice());
		m_paint_widget->SetVisualDevice(0);
		m_paint_widget->Delete();
	}
}

OP_STATUS PopupMenu::Init(OpView *parent)
{
	m_opwindow = OP_NEW(X11OpWindow, ());
	RETURN_OOM_IF_NULL(m_opwindow);
	RETURN_IF_ERROR(m_opwindow->Init(OpWindow::STYLE_POPUP, OpTypedObject::WINDOW_TYPE_UNKNOWN, 0, parent));
	RETURN_IF_ERROR(OpView::Create(&m_opview, m_opwindow));
	m_opwindow->GetOuterWidget()->SetPropagateLeaveEvents(TRUE);

	if (!GetNormalFont() || !GetBoldFont())
		return OpStatus::ERR;

	VisualDevice* vd = OP_NEW(VisualDevice,());
	if (!vd)
		return OpStatus::ERR_NO_MEMORY;
	m_paint_widget = OP_NEW(OpWidget,());
	if (!m_paint_widget)
	{
		OP_DELETE(vd);
		return OpStatus::ERR_NO_MEMORY;
	}
	m_paint_widget->SetVisualDevice(vd);
	m_paint_widget->SetSystemFont(OP_SYSTEM_FONT_UI_MENU);
	m_paint_widget->SetRTL(UiDirection::Get() == UiDirection::RTL);

	m_timer.SetTimerListener(this);

	return OpStatus::OK;
}


OP_STATUS PopupMenu::AddItem(const uni_char* name, const uni_char* shortcut, OpInputAction* action, UINT32 state, OpWidgetImage* image, PopupMenu* submenu)
{
	ToolkitLibrary::PopupMenuLayout& layout = PopupMenuItem::m_layout;

	layout.left_margin       = 3;
	layout.image_margin      = 3;
	layout.text_margin       = 6;
	layout.right_margin      = 3;
	layout.arrow_width       = 12;
	layout.frame_thickness   = 1;
	layout.arrow_overlap     = 0;
	layout.sub_menu_x_offset = 2;
	layout.sub_menu_y_offset = 3;
	layout.vertical_screen_edge_reflects = true;

	g_toolkit_library->GetPopupMenuLayout(layout);

	OpAutoPtr<PopupMenuItem> item (OP_NEW(PopupMenuItem, (state, action, submenu, m_paint_widget)));
	if (!item.get())
		return OpStatus::ERR_NO_MEMORY;

	OpFont* font = state & PopupMenuItem::MI_BOLD ? GetBoldFont() : GetNormalFont();

	if (name)
	{
		OpScreenProperties screen_properties;
		g_op_screen_info->GetProperties(&screen_properties, m_opwindow);

		int item_width = g_pcunix->GetIntegerPref(PrefsCollectionUnix::WindowMenuMaxItemWidth);
		int max_width  = (screen_properties.width * item_width) / 100;

		OpString shortened_name;
		RETURN_IF_ERROR(shortened_name.Set(name));
		RETURN_IF_ERROR(StringUtils::EllipticText(shortened_name, max_width, FALSE, font));

		RETURN_IF_ERROR(item->SetName(shortened_name));
		UINT32 width = item->GetName().GetWidth();
		if (m_max_text_width < width)
			m_max_text_width = width;
	}

	if (shortcut)
	{
		RETURN_IF_ERROR(item->SetShortcut(shortcut));
		UINT32 width = item->GetShortcut().GetWidth();
		if (submenu && layout.arrow_overlap > 0)
			width += layout.arrow_width;
		if (m_max_shortcut_width < width)
			m_max_shortcut_width = width;
	}

	if (image)
	{
		RETURN_IF_ERROR(item->SetWidgetImage(image));

		int width, height;
		image->GetSize(&width, &height);
		if (width > 0 && height > 0)
		{
			m_image_width = min(PopupMenuItem::MaxImageSize, max(m_image_width, (UINT32)width));
			m_image_height = min(PopupMenuItem::MaxImageSize, max(m_image_height, (UINT32)height));
		}
	}

	if (submenu)
	{
		m_has_submenus = true;
		submenu->m_parent_menu = this;
	}

	item.release()->Into(&m_menu_items);

	return OpStatus::OK;
}

OP_STATUS PopupMenu::AddSeparator()
{
	PopupMenuComponent* last_item = (PopupMenuComponent *)m_menu_items.Last();
	if (!last_item || (last_item && last_item->IsSeparator()))
	{
		// Silently reject multiple separators or a separator as first item in the menu
		return OpStatus::OK;
	}

	PopupMenuSeparator *item = OP_NEW(PopupMenuSeparator, ());
	if (!item)
		return OpStatus::ERR_NO_MEMORY;

	item->Into(&m_menu_items);

	return OpStatus::OK;
}


void PopupMenu::SetSelectedItem(INT32 index)
{
	if (index < 0)
	{
		m_selected_item = 0;
	}
	else
	{
		INT32 i = 0;

		m_selected_item = (PopupMenuComponent *)m_menu_items.First();
		while (m_selected_item)
		{
			if (i == index)
			{
				if (!m_selected_item->IsSelectable())
				{
					m_selected_item = 0;
				}
				break;
			}
			i++;
		}
		if (!m_selected_item)
		{
			m_selected_item = (PopupMenuComponent *)m_menu_items.First();	
			while (m_selected_item && !m_selected_item->IsSelectable() )
			{
				m_selected_item = (PopupMenuComponent *)m_selected_item->Suc();
			}
		}
	}
}


void PopupMenu::SelectParents()
{
	if (!m_selected_item)
		return;

	PopupMenu* child = this;
	MenuElement* elm = m_parent_menu;
	while (elm && !elm->IsMenuBar())
	{
		PopupMenu* menu = static_cast<PopupMenu*>(elm);
		if (!menu->m_selected_item)
		{
			for (PopupMenuComponent* m=(PopupMenuComponent *)menu->m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
			{
				if (m->GetSubMenu() == child)
				{
					menu->m_selected_item = m;
					menu->Invalidate();
					break;
				}
			}
		}
		child = menu;
		elm = menu->m_parent_menu;
	}
}



OP_STATUS PopupMenu::SetSubMenuData(const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start)
{
	RETURN_IF_ERROR(m_submenu.name.Set(sub_menu_name));
	m_submenu.value = sub_menu_value;
	m_submenu.start = sub_menu_start;
	return OpStatus::OK;
}


void PopupMenu::AboutToShow()
{
	if (!m_menu_items.First() && m_submenu.name.HasContent())
	{
		OpInputContext* context = m_context_type == KeyboardContext ? 
			g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext();
		g_application->GetMenuHandler()->AddMenuItems(this, m_submenu.name.CStr(), m_submenu.value, m_submenu.start, context, TRUE);
	}
}

void PopupMenu::MoveTo(AnchorType anchor_type, const OpRect& anchor_rect)
{
	const OpPoint position = CalculateMenuPosition(anchor_type, anchor_rect);
	m_opwindow->GetWidget()->Move(position.x, position.y);
}

void PopupMenu::Show(AnchorType anchor_type, const OpRect& anchor_rect)
{
	const OpPoint position = CalculateMenuPosition(anchor_type, anchor_rect);
	m_opwindow->SetOuterPos(position.x, position.y);

	const int width  = GetWidth();
	const int height = GetHeight();
	m_opwindow->SetOuterSize(width, height);
	m_opview->SetSize(width, height);

	m_opview->SetMouseListener(this);
	m_opview->SetPaintListener(this);
	m_opview->SetVisibility(TRUE);

	X11PopupWidget* widget = (X11PopupWidget *)m_opwindow->GetWidget();
	widget->SetInputEventListener(this);
	widget->GiveFocus();
	widget->DisableAutoClose(true);

	m_auto_shown_time = 0;

	if (m_listener)
		m_listener->OnPopupOpened();

	// This will end up in X11Widget::Show() which will block if modal
	m_opwindow->Show(TRUE);
	// Menu is now out of the modal loop if any
}

OpPoint PopupMenu::CalculateMenuPosition(AnchorType anchor_type, const OpRect& anchor_rect) const
{
	OpRect menu_rect(-1, -1, GetWidth(), GetHeight());

	const OpPoint& anchor_point = anchor_rect.Center();

	OpScreenProperties screen_properties;
	g_op_screen_info->GetProperties(&screen_properties, m_opwindow, &anchor_point);

	OpRect work_rect = g_x11->GetWorkRect(m_opwindow->GetOuterWidget()->GetScreen(), anchor_point);
	work_rect.IntersectWith(screen_properties.screen_rect);

	const OpRect& screen_rect = !work_rect.IsEmpty() ? work_rect : screen_properties.screen_rect;

	if (anchor_type == BELOW_OR_ABOVE)
	{
		if (UiDirection::Get() == UiDirection::LTR)
			menu_rect.x = anchor_rect.Left();
		else
			menu_rect.x = anchor_rect.Right() - menu_rect.width;

		menu_rect.x = MIN(menu_rect.x, screen_rect.Right() - menu_rect.width);

		menu_rect.y = anchor_rect.Bottom();
		if (menu_rect.Bottom() > screen_rect.Bottom())
			menu_rect.y = anchor_rect.Top() - menu_rect.height;
	}
	else
	{
		if (UiDirection::Get() == UiDirection::LTR)
			menu_rect.x = anchor_rect.Right();
		else
			menu_rect.x = anchor_rect.Left() - menu_rect.width;

		if (IsSubmenu() || PopupMenuItem::m_layout.vertical_screen_edge_reflects)
		{
			if (menu_rect.Right() > screen_rect.Right())
				menu_rect.x = anchor_rect.Left() - menu_rect.width;

			if (menu_rect.x < screen_rect.x)
				menu_rect.x = anchor_rect.Right();
		}
		else
		{
			menu_rect.x = MIN(menu_rect.x, screen_rect.Right() - menu_rect.width);
		}

		menu_rect.y = anchor_rect.Top();
		if (!IsSubmenu() && menu_rect.Bottom() > screen_rect.Bottom())
			menu_rect.y = anchor_rect.Bottom() - menu_rect.height;
		else
			menu_rect.y = MIN(menu_rect.y, screen_rect.Bottom() - menu_rect.height);
	}

	menu_rect.x = MAX(menu_rect.x, screen_rect.x);
	menu_rect.y = MAX(menu_rect.y, screen_rect.y);

	return menu_rect.TopLeft();
}

void PopupMenu::Hide()
{
	if (m_opwindow->IsVisible())
	{
		if (m_open_submenu)
		{
			SendOnMenuShown(m_open_submenu, FALSE);
			m_open_submenu->Hide();
			m_open_submenu = 0;
		}

		if (m_listener)
			m_listener->OnPopupClosed();

		// We have to mark the widget as closed to terminate the message loop when it is hidden
		((X11PopupWidget *)m_opwindow->GetWidget())->SetClosed();
		m_opwindow->Show(FALSE);
	}
}

MenuElement* PopupMenu::Hide(int gx, int gy)
{
	UINT32 w, h;
	m_opview->GetSize(&w, &h);
	OpPoint p = ConvertToScreen(OpPoint(0, 0));

	bool inside = gx >= p.x && gy >= p.y && gx < (int)(p.x+w) && gy < (int)(p.y+h);	
	if (!inside)
	{
		if (m_parent_menu)
		{
			m_parent_menu->CancelSubMenu();
			return m_parent_menu->Hide(gx, gy);
		}
		else
		{
			Hide();
			return 0;
		}
	}
	else
	{
		return this;
	}

}


BOOL PopupMenu::CanHideNow() const
{
	double then = m_auto_shown_time;
	if (then > 0)
	{
		double now  = g_op_time_info->GetWallClockMS();
		if (now - then < 1000.0)
		{
			return FALSE;
		}
	}
	
	return TRUE;
}


PopupMenu* PopupMenu::Contains(int gx, int gy, BOOL include_submenu)
{
	UINT32 w, h;
	m_opview->GetSize(&w, &h);
	OpPoint p = ConvertToScreen(OpPoint(0, 0));
 
	if (gx >= p.x && gy >= p.y && gx < (int)(p.x+w) && gy < (int)(p.y+h))
		return this;
	else if(include_submenu && m_open_submenu)
		return m_open_submenu->Contains(gx, gy, include_submenu);
	else
		return 0;
}


PopupMenuComponent* PopupMenu::ItemAt(int gx, int gy)
{
	UINT32 w, h;
	m_opview->GetSize(&w, &h);
	OpPoint p = ConvertToScreen(OpPoint(0, 0));

	bool inside = gx >= p.x && gy >= p.y && gx < (int)(p.x+w) && gy < (int)(p.y+h);
	if (!inside)
		return m_parent_menu ? m_parent_menu->ItemAt(gx,gy) : 0;
	
	OpPoint point(gx-p.x, gy-p.y);
	return GetItem(point.x, point.y);
}


void PopupMenu::Exec(AnchorType anchor_type, const OpRect& anchor_rect)
{
	m_exec_action = 0;

	// Popups are by default not modal. Set to modal here. Sub menus are not modal
	m_opwindow->GetWidget()->SetModal(true);

	Show(anchor_type, anchor_rect);

	// This happens after menu is closed, but not destoyed. Save action 
	// and context type. We have to destroy the popup menu before we can 
	// invoke the action as the action itself can change the enviroment the 
	// popup is in (see bug #DSK-285455)

	m_exec_action  = TakeInputAction(); // Ownership transferred
	m_exec_context = m_context_type;
	
	// Always reset input after closing a popup menu
	g_input_manager->ResetInput();
}


// static
OpInputAction::Action PopupMenu::Exec(PopupMenu* menu, AnchorType anchor_type, const OpRect& anchor_rect, bool delete_menu, bool invoke_action)
{
	OpInputAction::Action action = OpInputAction::ACTION_UNKNOWN;

	if (menu)
	{
		m_exec_action = 0;
		menu->Exec(anchor_type, anchor_rect);

		if (delete_menu)
		{
			OP_DELETE(menu);
		}

		if (m_exec_action)
		{
			action = m_exec_action->GetAction();

			if (invoke_action)
			{
				OpInputContext* context = m_exec_context == KeyboardContext ? 
					g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext();
				g_input_manager->InvokeAction(m_exec_action, context, 0, TRUE, OpInputAction::METHOD_MENU);
			}

			OP_DELETE(m_exec_action);
		}
	}

	return action;
}


void PopupMenu::Grab()
{
	m_opwindow->GetWidget()->Grab();
	m_timer.Stop();
}


OpPoint PopupMenu::ConvertToScreen(const OpPoint& point)
{
	return m_opview->ConvertToScreen(point);
}

void PopupMenu::SendOnMenuShown(PopupMenu* menu, BOOL show)
{
	if (!g_scope_manager->desktop_window_manager->IsEnabled() || !menu)
		return;

	OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
	if (OpStatus::IsSuccess(menu->SetQuickMenuInfo(info)))
		g_application->GetMenuHandler()->OnMenuShown(FALSE, info);
}


void PopupMenu::Stop(bool cancel)
{
	// Clear status bar of any text we may have set
	ClearStatusBar();

	if (m_open_submenu)
	{
		PopupMenu *m = m_open_submenu;

		SendOnMenuShown(m, FALSE);
		m_open_submenu = 0;
		m->Stop(cancel);
		return;
	}
	Hide();
	m_cancelled = cancel;
	if (m_parent_menu)
	{
		m_parent_menu->Stop(cancel);
		return;
	}
}


void PopupMenu::CancelSubMenu()
{
	if (m_open_submenu)
	{
		SendOnMenuShown(m_open_submenu, FALSE);
		m_open_submenu->Cancel();
		m_open_submenu = 0;
		Grab();
	}
}

void PopupMenu::Cancel()
{
	// Clear status bar of any text we may have set
	ClearStatusBar();

	SetSelectedItem(-1); // So that menu is resat if shown again
	m_cancelled = true;
	Hide();
}


bool PopupMenu::ShowSubmenu(bool activate)
{
	if (m_selected_item && m_selected_item->IsSelectable() && m_selected_item->GetSubMenu() && m_open_submenu != m_selected_item->GetSubMenu())
	{
		CancelSubMenu();

		m_open_submenu = m_selected_item->GetSubMenu();

		m_open_submenu->AboutToShow();
		m_open_submenu->SetSelectedItem(activate ? 0 : -1);

		INT32 x, y;
		GetItemPosition(m_selected_item, x, y);
		x -= PopupMenuItem::m_layout.sub_menu_x_offset;
		y -= PopupMenuItem::m_layout.sub_menu_y_offset;
		const OpPoint& item_pos = ConvertToScreen(OpPoint(x, y));

		UINT32 menu_width, menu_height;
		m_opview->GetSize(&menu_width, &menu_height);

		OpRect anchor_rect(item_pos.x, item_pos.y, menu_width, GetItemHeight());
		anchor_rect = anchor_rect.InsetBy(PopupMenuItem::m_layout.sub_menu_x_offset, 0);

		OpString name;
		name.Set(m_open_submenu->GetScopeName().CStr());
		OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
		if (OpStatus::IsSuccess(m_open_submenu->SetQuickMenuInfo(info)))
			g_application->GetMenuHandler()->OnMenuShown(TRUE, info);

		m_open_submenu->Show(RIGHT_OR_LEFT_OF, anchor_rect);
		m_open_submenu->Grab();

		return true;
	}

	return false;
}


bool PopupMenu::ShowSubmenu(int gx, int gy)
{
	UINT32 w, h;
	m_opview->GetSize(&w, &h);
	OpPoint p = ConvertToScreen(OpPoint(0, 0));
	bool inside = gx >= p.x && gy >= p.y && gx < (int)(p.x+w) && gy < (int)(p.y+h);

	if (inside)
	{
		OpPoint point(gx-p.x, gy-p.y);
		PopupMenuComponent* item = GetItem(point.x, point.y);
		if (item && item != m_selected_item)
		{
			CancelSubMenu();

			if (item->IsSelectable())
			{
				m_selected_item = item;
				ShowSubmenu(true);
				Invalidate();
			}
		}
		return true;
	}
	else
	{
		Cancel();
		if (m_parent_menu)
		{
			return m_parent_menu->ShowSubmenu(gx, gy);
		}
	}

	return false;
}


void PopupMenu::ShowNext(MenuElement* current, bool next)
{
	(void)current;
	if (m_parent_menu)
	{
		m_parent_menu->ShowNext(this, next);
	}
}


bool PopupMenu::HandleMouseEvent(int gx, int gy, MouseEvent event, bool confine_to_area)
{
	UINT32 w, h;
	m_opview->GetSize(&w, &h);
	OpPoint p = ConvertToScreen(OpPoint(0, 0));

	bool inside = gx >= p.x && gy >= p.y && gx < (int)(p.x+w) && gy < (int)(p.y+h);
	
	if (event == POINTER_MOTION)
	{
		// This test is to remove a highlighted item if mouse has (quickly) moved outside 
		// the window (with the mouse button pressed) so that the highlight remains. We 
		// want the highlight to remain only when the item have an open sub menu. Finally, 
		// this shall only happen if the mouse selected the item in the first place.

		if (SupportMouseHover() && !inside && m_mouse_selected_item && m_selected_item)
		{
			if (!m_selected_item->GetSubMenu())
				confine_to_area = false;
			else if (m_selected_item->GetSubMenu() && m_open_submenu != m_selected_item->GetSubMenu())
				confine_to_area = false;
		}

		if (SupportMouseHover() && (inside || !confine_to_area))
		{
			OpPoint point(gx-p.x, gy-p.y);
			PopupMenuComponent* item = GetItem(point.x, point.y);

			// We use the mouse selected item to prevent a random mouse move to
			// interfere with keyboard navigation
			if (item && item != m_mouse_selected_item)
			{
				// Order is important here
				m_selected_item = item;
				PostPendingMenu();
				m_mouse_selected_item = m_selected_item;

				if (m_selected_item)
					OnItemHover(m_selected_item);
				else
					OnItemHover(item); // Allow status bar message from a disbled item

				Invalidate();
			}
			else if (!item && (m_selected_item || m_mouse_selected_item))
			{
				m_mouse_selected_item = m_selected_item = 0;
				Invalidate();
			}

			SelectParents(); // Ensure all parent items are selected (DSK-324254)

			return true;
		}
	}
	else if (event == BUTTON_PRESS)
	{
		if (SupportMouseHover())
		{
			if (!inside)
			{
				PopupMenuComponent* item = ItemAt(gx,gy);
				if (!item || item->GetSubMenu() != this)
				{
					// Hide sub menu right away if we would not not open
					// the same immediately
					MenuElement* elm = Hide(gx,gy);
					if (elm)
						return elm->HandleMouseEvent(gx, gy, event, true);
				}
				else if (CanHideNow())
				{
					Hide(gx,gy);
				}
				m_auto_shown_time = 0.0;
				m_block_mouse_up = true;
			}
			else
			{
				OpPoint point(gx-p.x, gy-p.y);
				PopupMenuComponent* item = GetItem(point.x, point.y);
				if (item)
				{
					CancelSubMenu();

					if (item->IsSelectable())
					{
						m_selected_item = item;
						ShowSubmenu(true);
						Invalidate();
					}

					// Do not close menu when clikcing on an open or closed submenu entry 
					// or a disabled entry
					m_block_mouse_up = m_open_submenu || !item->IsSelectable();
				}
			}
		}
		else
		{
			if (inside)
			{
				OpPoint point(gx-p.x, gy-p.y);
				PopupMenuComponent* item = GetItem(point.x, point.y);
				if (item && item != m_selected_item)
				{
					CancelSubMenu();

					if (item->IsSelectable())
					{
						m_selected_item = item;
						ShowSubmenu(true);
						Invalidate();
					}
				}
			}
			else
			{
				ShowSubmenu(gx, gy);
			}
		}

		return true;
	}
	else if (event == BUTTON_RELEASE)
	{
		if (inside && m_mouse_has_moved)
		{
			Stop(false);
		}
		return true;	
	}

	if (m_parent_menu)
	{
		return m_parent_menu->HandleMouseEvent(gx, gy, event, true);
	}

	return false;
}


void PopupMenu::PostPendingMenu()
{
#if PENDING_MENU_TIMEOUT_MS == 0 
	OnTimeOut(&m_timer);
#else
	m_timer.Start(PENDING_MENU_TIMEOUT_MS);
#endif
}


void PopupMenu::OnItemHover(PopupMenuComponent* item)
{
	if (item && g_application->GetActiveDesktopWindow())
	{
		OpInputAction* action = item->GetInputAction();
		if (action && action->GetActionInfo().HasStatusText())
			g_application->GetActiveDesktopWindow()->SetStatusText(action->GetActionInfo().GetStatusText());
		else
			g_application->GetActiveDesktopWindow()->SetStatusText(UNI_L(""));
	}
}


void PopupMenu::ClearStatusBar()
{
	if (g_application->GetActiveDesktopWindow())
		g_application->GetActiveDesktopWindow()->SetStatusText(UNI_L(""));
}


void PopupMenu::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_timer)
	{
		if (m_selected_item)
		{
			// If we have an open sub menu, it might be that we have changed 
			// the selected item before the timeout. So, we must test if the 
			// mouse is inside the current sub menu, and if it is, keep that 
			// menu open and change the selected item to the parent item of 
			// the sub menu. Bug DSK-302682

			if (m_open_submenu)
			{
				OpPoint p;
				InputUtils::GetGlobalPointerPos(p);

				PopupMenu* match = m_open_submenu->Contains(p.x, p.y, TRUE);
				if (match)
				{
					// Roll back selected item until we reach the matched sub menu
					PopupMenu* menu = this;
					while (menu && menu != match)
					{
						for (PopupMenuComponent* m=(PopupMenuComponent *)menu->m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
						{
							if (m->GetSubMenu() == menu->m_open_submenu)
							{
								menu->m_selected_item = m;
								menu->Invalidate();
								menu = menu->m_open_submenu;
								break;
							}
						}
					}
					return;
				}
			}

			// Select new menu. If the same menu is already open
			// we will not do anything. It is important that we do not hide 
			// and show the same menu quickly after another. The UnmapNotify 
			// and MapNotify events leave a short interval where there is no 
			// defined grabbed widget when there really is one.

			if (!m_selected_item->GetSubMenu())
				CancelSubMenu();

			if (ShowSubmenu(false))
				m_open_submenu->m_auto_shown_time = g_op_time_info->GetWallClockMS();
		}
		else
		{
			if(!m_open_submenu)
				CancelSubMenu();
		}			
		m_mouse_selected_item = m_selected_item;
	}
}


OpInputAction *PopupMenu::TakeInputAction()
{
	if (m_cancelled || !m_selected_item)
		return 0;

	return m_selected_item->TakeInputAction();
}


BOOL PopupMenu::SoonTooTall()
{
	UINT32 full_height = GetHeight();
	UINT32 item_height = GetItemHeight();

	OpScreenProperties screen_properties;
	g_op_screen_info->GetProperties(&screen_properties, m_opwindow);

	return full_height >= screen_properties.height - (3*item_height);
}


UINT32 PopupMenu::GetHeight() const
{
	if (!HasContent())
		return 5;

	const ToolkitLibrary::PopupMenuLayout& layout = PopupMenuItem::m_layout;

	UINT32 num_items = 0;
	UINT32 num_separators = 0;
	bool last_was_separator = false;

	for (PopupMenuComponent *m=(PopupMenuComponent *)m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
	{
		if (m->IsSeparator())
		{
			num_separators ++;
			last_was_separator = true;
		}
		else
		{
			num_items ++;
			last_was_separator = false;
		}
	}

	if (last_was_separator)
		num_separators --;

	UINT32 height = (num_items * GetItemHeight()) + (num_separators * GetSeparatorHeight()) + (layout.frame_thickness*2);

	return height;
}


UINT32 PopupMenu::GetWidth() const
{
	if (!HasContent())
		return 50;

	const ToolkitLibrary::PopupMenuLayout& layout = PopupMenuItem::m_layout;

	int width = layout.left_margin;

	if (m_image_width)
		width += m_image_width + layout.image_margin;

	if (m_max_text_width)
		width += m_max_text_width + layout.text_margin;

	width += m_max_shortcut_width;

	width += layout.arrow_width - layout.arrow_overlap;

	width += layout.right_margin;

	width += layout.frame_thickness*2;

	return width;
}

UINT32 PopupMenu::GetItemHeight() const
{
	UINT32 height = PopupMenuItem::MaxImageSize;
	if (m_normalfont)
		height = max(m_normalfont->Height(), height);
	if (m_boldfont)
		height = max(m_boldfont->Height(), height);

	return height + 2;
}

UINT32 PopupMenu::GetSeparatorHeight() const
{
	return PopupMenuSeparator::GetItemHeight();
}

bool PopupMenu::GetItemPosition(PopupMenuComponent* item, INT32& x, INT32& y)
{
	x=0;
	y=0;

	UINT32 item_height = GetItemHeight();
	y=3;

	for (PopupMenuComponent* m=(PopupMenuComponent *)m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
	{
		if (m == item)
		{
			x = 2; 
			return TRUE;
		}

		if (m->IsSeparator())
		{
			y += GetSeparatorHeight();
		}
		else
		{	   
			y += item_height;
		}
	}

	return FALSE;
}


PopupMenuComponent* PopupMenu::GetItem(INT32 x, INT32 y)
{   
	UINT32 width, height;
	m_opview->GetSize(&width, &height);

	if (x < 0 || (UINT32)x > width || y < 0 || (UINT32)y > height)
		return 0;

	UINT32 item_height = GetItemHeight();
	UINT32 y_offset = 3;

	for (PopupMenuComponent* m=(PopupMenuComponent *)m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
	{
		UINT32 cell_height = m->IsSeparator() ? GetSeparatorHeight() : item_height;
		if ((UINT32)y >= y_offset && (UINT32)y < (y_offset+cell_height))
			return m;
		
		y_offset += cell_height;
	}

	return 0;
}


OpFont* PopupMenu::GetBoldFont()
{
	if (!m_boldfont)
	{
		FontAtt font_details;
		g_pcfontscolors->GetFont(OP_SYSTEM_FONT_UI_MENU, font_details);
		OpFontManager *fontman = styleManager->GetFontManager();
		m_boldfont = fontman->CreateFont(font_details.GetFaceName(), font_details.GetSize(), 7, font_details.GetItalic(), font_details.GetOverline(), 0);
	}
	return m_boldfont;
}


OpFont* PopupMenu::GetNormalFont()
{
	if (!m_normalfont)
	{
		FontAtt font_details;
		g_pcfontscolors->GetFont(OP_SYSTEM_FONT_UI_MENU, font_details);
		OpFontManager *fontman = styleManager->GetFontManager();
		m_normalfont = fontman->CreateFont(font_details.GetFaceName(), font_details.GetSize(), font_details.GetWeight(), font_details.GetItalic(), font_details.GetOverline(), 0);
	}

	return m_normalfont;
}


void PopupMenu::Invalidate()
{
	UINT32 width, height;
	m_opview->GetSize(&width, &height);
	m_opview->Invalidate(OpRect(0, 0, width, height));
}



void PopupMenu::OnPaint(const OpRect &rect, OpView *view)
{
	OpPainter* painter = view->GetPainter(rect);
	if (!painter)
		return;

	const ToolkitLibrary::PopupMenuLayout& layout = PopupMenuItem::m_layout;

	OpFont* curfont = GetNormalFont();
	painter->SetFont(curfont);

	UINT32 width, height;
	view->GetSize(&width, &height);
	OpRect view_rect(0, 0, width, height);

	PaintMenuFrame(painter, view_rect, rect);

	int y = layout.frame_thickness;
	UINT32 item_height = GetItemHeight();
	UINT32 separator_height = GetSeparatorHeight();

	PopupMenuComponent::PaintProperties properties;
	properties.normal_font = GetNormalFont();
	properties.bold_font = GetBoldFont();
	FontAtt font_att;
	g_op_ui_info->GetFont(OP_SYSTEM_FONT_UI_MENU, font_att, FALSE);
	properties.font_number = font_att.GetFontNumber();
	properties.max_text_width = m_max_shortcut_width;
	if (m_max_text_width)
		properties.max_text_width += m_max_text_width + layout.text_margin;
	properties.max_image_width = m_image_width;
	properties.max_image_height = m_image_height;

	for (PopupMenuComponent *m=(PopupMenuComponent *)m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
	{
		int h = m->IsSeparator() ? separator_height : item_height;
		y += m->Paint(painter, OpRect(layout.frame_thickness, y, width-layout.frame_thickness*2, h), m == m_selected_item, properties);
	}

	view->ReleasePainter(rect);
}


void PopupMenu::PaintMenuFrame(OpPainter* painter, const OpRect& view_rect, const OpRect& rect)
{
	VisualDevice vd;
	vd.BeginPaint(painter, view_rect, rect);
	g_skin_manager->DrawElement(&vd, "Popup Menu Skin", view_rect);
	vd.EndPaint();
}

void PopupMenu::OnMouseMove(const OpPoint &point, ShiftKeyState shift_state, OpView *view)
{
	m_mouse_has_moved = true;
	OpPoint gp = ConvertToScreen(point);
	HandleMouseEvent(gp.x, gp.y, POINTER_MOTION, true);
}

void PopupMenu::OnMouseDown(MouseButton button, UINT8 nclicks, ShiftKeyState shift_state, OpView *view)
{
	if (button == MOUSE_BUTTON_2)
	{
		OpInputAction* action =  m_selected_item ? m_selected_item->GetInputAction() : 0;
		if (action && action->GetAction() == OpInputAction::ACTION_OPEN_LINK)
			return;
	}

	if (button != MOUSE_BUTTON_1)
	{
		m_block_after_close = TRUE;
		Stop(true);
		return;
	}

	INT32 x, y;
 	view->GetMousePos(&x, &y);
	OpPoint gp = ConvertToScreen(OpPoint(x, y));
	if (!HandleMouseEvent(gp.x, gp.y, BUTTON_PRESS, false))
		Stop(true);
}


void PopupMenu::OnMouseUp(MouseButton button, ShiftKeyState shift_state, OpView *view)
{
	INT32 x, y;
	view->GetMousePos(&x, &y);
	OpPoint gp = ConvertToScreen(OpPoint(x, y));

	if (button == MOUSE_BUTTON_2)
	{
		OpInputAction *action =  m_selected_item ? m_selected_item->GetInputAction() : 0;
		if (action && action->GetAction() == OpInputAction::ACTION_OPEN_LINK)
		{
			X11DesktopPopupMenuHandler::ShowBookmarkContextMenu(gp, this, action);
			return;
		}
	}

	if (m_block_mouse_up)
	{
		m_block_mouse_up = false;
		return;
	}

	if (button != MOUSE_BUTTON_1)
	{
		Stop(true);
		return;
	}

	if (!HandleMouseEvent(gp.x, gp.y, BUTTON_RELEASE, false))
		Stop(true);
}

void PopupMenu::OnMouseLeave()
{
	// Clear status bar of any text we may have set
	ClearStatusBar();

	// Do not clear selected item if selected item has an open submenu

	if (SupportMouseHover() && m_selected_item && m_opwindow->IsVisible())
	{
		if (!(m_selected_item->GetSubMenu() && m_open_submenu == m_selected_item->GetSubMenu()))
		{
			m_selected_item = m_mouse_selected_item = 0;
			Invalidate();
		}
	}
}

void PopupMenu::OnSetCursor()
{
}

void PopupMenu::OnMouseCaptureRelease()
{
}


BOOL PopupMenu::OnInputEvent(XEvent* e)
{
	// Ignore all events other than key press and key release (even we do not use the latter)
	if (e->type != KeyPress)
		return e->type == KeyRelease;

	InputUtils::KeyContent content;
	InputUtils::TranslateKeyEvent(e, content);

	OpVirtualKey opera_key = content.GetVirtualKey();

	if (UiDirection::Get() == UiDirection::RTL)
	{
		switch (opera_key)
		{
			case OP_KEY_LEFT:	opera_key = OP_KEY_RIGHT; break;
			case OP_KEY_RIGHT:	opera_key = OP_KEY_LEFT; break;
		}
	}

	switch (opera_key)
	{
		// Close popups on Alt-down. Allows direct Alt+TAB navigation in WM from a state where
		// we show a grabbed popup in opera. Dropdowns are closed in InputUtils::HandleEvent()
		case OP_KEY_ALT:
		{
			Stop(TRUE);
		}
		break;

		case OP_KEY_ESCAPE:
		{
			if (m_parent_menu)
				m_parent_menu->CancelSubMenu();
			else
				Cancel();
		}
		break;

		case OP_KEY_UP:
		{
			PopupMenuComponent* item = 0;
			bool was_selectable = true;
			for (PopupMenuComponent *m=(PopupMenuComponent *)m_menu_items.Last(); m; m=(PopupMenuComponent *)m->Pred())
			{
				if (m == m_selected_item || !was_selectable)
				{
					item = (PopupMenuComponent *)m->Pred();
					if (item && !item->IsSelectable())
					{
						was_selectable = false;
						item = 0;
					}
					else
					{
						break;
					}
				}
			}

			if (!item)
			{
				item = (PopupMenuComponent *)m_menu_items.Last();
				while (item && !item->IsSelectable())
				{
					item = (PopupMenuComponent *)item->Pred();
				}
			}
			if (item && m_selected_item != item)
			{
				m_selected_item = item;
				Invalidate();
				SelectParents(); // Ensure all parent items are selected (DSK-324254)
			}
		}
		break;

		case OP_KEY_DOWN:
		{
			PopupMenuComponent* item = 0;
			bool was_selectable = true;
			for (PopupMenuComponent *m=(PopupMenuComponent *)m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
			{
				if (m == m_selected_item || !was_selectable)
				{
					item = (PopupMenuComponent *)m->Suc();
					if (item && !item->IsSelectable())
					{
						was_selectable = false;
						item = 0;
					}
					else
					{
						break;
					}
				}
			}

			if (!item)
			{
				item = (PopupMenuComponent *)m_menu_items.First();
				while (item && !item->IsSelectable())
				{
					item = (PopupMenuComponent *)item->Suc();
				}
			}
			if (item && m_selected_item != item)
			{
				m_selected_item = item;
				Invalidate();
				SelectParents(); // Ensure all parent items are selected (DSK-324254)
			}
		}	
		break;

		case OP_KEY_RIGHT:
		{
			if (!m_selected_item)
			{
				// This happens if mouse hovering has opened a submenu (and made it active)
				// but not selected an item in the menu because the mouse has not hovered 
				// the menu itself.
				SetSelectedItem(0);
				if (m_selected_item)
				{
					Invalidate();
					return TRUE;
				}
			}

			if (!ShowSubmenu(true))
			{
				if (m_parent_menu)
				{
					m_parent_menu->ShowNext(this, true);
				}
			}
		}
		break;

		case OP_KEY_LEFT:
		{
			if (m_parent_menu)
			{
				if (m_parent_menu->IsMenuBar())
					m_parent_menu->ShowNext(this, false);
				else
					m_parent_menu->CancelSubMenu();
			}
		}
		break;

		case OP_KEY_ENTER:
		{
			if (!ShowSubmenu(true))
				Stop(false);
		}
		break;

		default:
		{
			if (!InputUtils::GetOpModifierFlags())
			{
				UINT32 value = 0;
				if (content.GetUtf8Text().HasContent())
				{
					OpString text;
					if (OpStatus::IsSuccess(text.SetFromUTF8(content.GetUtf8Text())))
					{
						int keytext_used = 0;
						value = Unicode::GetUnicodePoint(text.CStr(), text.Length(), keytext_used);
						if (keytext_used != text.Length())
						{
							// Only allow single-character accelerators
							value = 0;
						}
						else
							value = Unicode::ToLower(value);
					}
				}


				PopupMenuComponent* first_match = 0;
				PopupMenuComponent* first_after_current = 0;
				BOOL multiple_match = FALSE;
				BOOL after_current = FALSE;
				for (PopupMenuComponent* m=(PopupMenuComponent *)m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
				{
					if (m->GetAcceleratorKey() == value)
					{
						if (first_match)
							multiple_match = TRUE;

						if (!first_match)
						{
							first_match = m;
#ifndef ALLOW_MENU_SHARE_SHORTCUT
							break;
#endif
						}
						if (after_current && !first_after_current)
							first_after_current = m;
					}

					if (m == m_selected_item)
						after_current = TRUE;

				}
				if (first_after_current || first_match)
				{
					m_selected_item = first_after_current ? first_after_current : first_match;
					if (multiple_match)
						Invalidate();
					else if (ShowSubmenu(true))
						Invalidate();
					else
						Stop(false);
				}
				break;
			}

			if (m_is_o_menu && InputUtils::IsOMenuToggleKey(e))
			{
				if (m_parent_menu)
					m_parent_menu->CancelSubMenu();
				else
					Cancel();
			}
		}
		break;
	} // switch (opera_key)

	return TRUE;
}

OP_STATUS PopupMenu::SetQuickMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuInfo& info)
{

	if (!g_scope_manager->desktop_window_manager->IsEnabled())
		return OpStatus::ERR;

	RETURN_IF_ERROR(info.GetMenuIdRef().SetMenuName(m_scope_name));
	
	INT32 x, y;
	m_opwindow->GetOuterPos(&x, &y);

	UINT32 width, height;
	m_opwindow->GetOuterSize(&width, &height);

	// Set the rect
	info.GetRectRef().SetX(x);
	info.GetRectRef().SetY(y);
	info.GetRectRef().SetWidth(width);
	info.GetRectRef().SetHeight(height);

	// List the items
	const ToolkitLibrary::PopupMenuLayout& layout = PopupMenuItem::m_layout;
	int item_y = layout.frame_thickness; // start value
	uint i = 0;

	for (PopupMenuComponent* m=(PopupMenuComponent *)m_menu_items.First(); m; m=(PopupMenuComponent *)m->Suc())
	{
		if (!m->IsSeparator())
		{
			PopupMenuItem* mi = (PopupMenuItem*)m;

			OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuItemInfo> item(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuItemInfo, ()));
			if (item.get() == NULL)
				return OpStatus::ERR;

			item.get()->SetSeparator(FALSE);
			RETURN_IF_ERROR(item.get()->SetText(mi->GetName().Get())); 
			item.get()->SetEnabled(mi->IsEnabled());
			item.get()->SetChecked(mi->IsChecked() | mi->IsSelected());
			item.get()->SetBold(mi->IsBold());
			uni_char sc = mi->GetAcceleratorKey();
			OpString accKey;
			accKey.Set(&sc, 1);
			RETURN_IF_ERROR(item.get()->SetShortcut(mi->GetShortcut().Get()));
			RETURN_IF_ERROR(item.get()->SetShortcutLetter(accKey));

			if (mi->GetInputAction())
			{
				OpInputAction* action = mi->GetInputAction();
				const char* action_string = OpInputAction::GetStringFromAction(mi->GetInputAction()->GetAction());
				OpString action_name;
				action_name.Set(action_string);
				RETURN_IF_ERROR(item.get()->SetAction(action_name));

				const uni_char* param = action->GetActionDataString();
				if (param && *param)
				{
					RETURN_IF_ERROR(item.get()->SetActionParam(param));
				}
			}

			item.get()->SetRow(i); i++;
			// Set the rect
			item->GetRectRef().SetX(x + layout.frame_thickness);
			item->GetRectRef().SetY(y + item_y); // hvorfor ikke y + GetItemHeight?
			item->GetRectRef().SetWidth(width-layout.frame_thickness*2);
			item->GetRectRef().SetHeight(GetItemHeight());

			OpString str;
			item->SetSubmenu(mi->GetSubMenu() ? mi->GetSubMenu()->GetScopeName() : str); 

			if (OpStatus::IsSuccess(info.GetMenuItemListRef().Add(item.get())))
				item.release();

			// increase y value for next iteration
			item_y += GetItemHeight();
		}
		else // separator
		{
			OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuItemInfo> item(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuItemInfo, ()));
			if (item.get() == NULL)
				return OpStatus::ERR;

			item.get()->SetSeparator(TRUE);
			OpString s;
			RETURN_IF_ERROR(item.get()->SetText(s)); 
			item.get()->SetEnabled(TRUE);
			item.get()->SetChecked(FALSE);
			item.get()->SetBold(TRUE);
			OpString accKey;
			RETURN_IF_ERROR(item.get()->SetShortcutLetter(accKey));
			item.get()->SetRow(i); i++;

			// Set the rect
			item->GetRectRef().SetX(x + layout.frame_thickness);
			item->GetRectRef().SetY(y + item_y); 
			item->GetRectRef().SetWidth(width-layout.frame_thickness*2);
			item->GetRectRef().SetHeight(GetSeparatorHeight());

			if (OpStatus::IsSuccess(info.GetMenuItemListRef().Add(item.get())))
				item.release();

		    item_y += GetSeparatorHeight();
		}
	}
	return OpStatus::OK;
}
