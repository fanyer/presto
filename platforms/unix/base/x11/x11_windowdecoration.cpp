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

#include "platforms/unix/base/x11/x11_windowdecoration.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/widgets/OpToolbarMenuButton.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/display/vis_dev.h"
#include "modules/pi/OpPainter.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpView.h"
#include "modules/pi/OpWindow.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/skin/OpSkinManager.h"

#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"

#include "platforms/unix/product/x11quick/iconsource.h"


class BackgroundWidget : public OpWidget
{
public:
	static OP_STATUS Create(BackgroundWidget** obj, X11WindowDecorationEdge* edge)
	{
		*obj = OP_NEW(BackgroundWidget, (edge));
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
	BackgroundWidget(X11WindowDecorationEdge* edge):m_edge(edge) { }

	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
	{
		m_edge->OnPaint(paint_rect, m_edge->GetOpView());
	}

	virtual void OnMouseMove(const OpPoint &point)
	{
		m_edge->OnMouseMove(point, 0, m_edge->GetOpView());
	}

	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		m_edge->OnMouseDown(button, nclicks, 0, m_edge->GetOpView());
	}

	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		m_edge->OnMouseUp(button, 0, m_edge->GetOpView());
	}

	virtual void OnSetCursor(const OpPoint &point) { /* Just override. We handle this manually*/ }

private:
	X11WindowDecorationEdge* m_edge;
};





X11WindowDecoration::X11WindowDecoration()
	:m_visible(false)
{
}


OP_STATUS X11WindowDecoration::Init(void *parent_handle, OpWindow* parent_window)
{
	const X11WindowDecorationEdge::Orientation orientation[4] =
		{ X11WindowDecorationEdge::TOP, X11WindowDecorationEdge::BOTTOM,
		  X11WindowDecorationEdge::LEFT, X11WindowDecorationEdge::RIGHT};

	for (int i=0; i<4; i++)
	{
		X11WindowDecorationEdge* edge = OP_NEW(X11WindowDecorationEdge,());
		RETURN_OOM_IF_NULL(edge);
		m_edge[i].reset(edge);
		RETURN_IF_ERROR(m_edge[i]->Init(parent_handle, parent_window, orientation[i]));
		m_edge[i]->SetParentInputContext(this);
	}
	return OpStatus::OK;
}


void X11WindowDecoration::Show()
{
	m_visible = true;
	for (int i=0; i<4; i++)
		m_edge[i]->Show();
}

void X11WindowDecoration::Hide()
{
	m_visible = false;
	for (int i=0; i<4; i++)
		m_edge[i]->Hide();
}

void X11WindowDecoration::Activate()
{
	for (int i=0; i<4; i++)
		m_edge[i]->SetActive(true);
}

void X11WindowDecoration::Deactivate()
{
	for (int i=0; i<4; i++)
		m_edge[i]->SetActive(false);
}

void X11WindowDecoration::SetHighlightActive(bool highlight_active)
{
	for (int i=0; i<4; i++)
		m_edge[i]->SetHighlightActive(highlight_active && i > 0);
}

void X11WindowDecoration::SetCanMoveResize(bool can_move_resize)
{
	for (int i=0; i<4; i++)
		m_edge[i]->SetCanMoveResize(can_move_resize);
}

void X11WindowDecoration::SetDrawBorder(bool draw_border)
{
	for (int i=0; i<4; i++)
		m_edge[i]->SetDrawBorder(draw_border);
}

void X11WindowDecoration::SetBorderColor(UINT32 border_color)
{
	for (int i=0; i<4; i++)
		m_edge[i]->SetBorderColor(border_color);
}


void X11WindowDecoration::SetMaximized(bool maximized)
{
	for (int i=0; i<4; i++)
		m_edge[i]->SetMaximized(maximized);
}


void X11WindowDecoration::ShowControls(bool show_menu, bool show_wm_buttons, bool show_mdi_buttons)
{
	for (int i=0; i<4; i++)
		m_edge[i]->ShowControls(show_menu, show_wm_buttons, show_mdi_buttons);
}

void X11WindowDecoration::Invalidate()
{
	for (int i=0; i<4; i++)
		m_edge[i]->Invalidate();
}



void X11WindowDecoration::SetSize(int width, int height)
{
	// Top
	m_edge[0]->SetGeometry(m_borders.left, 0, width-m_borders.left-m_borders.right, m_borders.top);
	// Bottom
	m_edge[1]->SetGeometry(m_borders.left, height-m_borders.bottom, width-m_borders.left-m_borders.right, m_borders.bottom);
	// Left
	m_edge[2]->SetGeometry(0, 0, m_borders.left, height);
	// Right
	m_edge[3]->SetGeometry(width-m_borders.right, 0, m_borders.right, height);
}


X11WindowDecorationEdge::X11WindowDecorationEdge()
	:m_orientation(TOP)
	,m_view_state(HIDDEN)
	,m_border_color(0)
	,m_corner_size(25)
	,m_resize_margin(3)
	,m_captured(false)
	,m_active(false)
	,m_can_move_resize(true)
	,m_draw_border(false)
	,m_highlight_active(true)
{
}


OP_STATUS X11WindowDecorationEdge::Init(void* parent_handle, OpWindow* parent_window, Orientation orientation)
{
	m_orientation = orientation;

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

	if (orientation == X11WindowDecorationEdge::TOP)
	{
		Controls* controls = OP_NEW(Controls,());
		RETURN_OOM_IF_NULL(controls);
		m_controls.reset(controls);

		RETURN_IF_ERROR(OpToolbarMenuButton::Construct(&m_controls->menu));
		m_widget_container->GetRoot()->AddChild(m_controls->menu, TRUE);
		m_controls->menu->SetParentInputContext(this);
		m_controls->menu->SetSystemFont(OP_SYSTEM_FONT_UI_TOOLBAR);

		RETURN_IF_ERROR(WMButtonSet::Create(&m_controls->wm_buttonset));
		m_widget_container->GetRoot()->AddChild(m_controls->wm_buttonset, TRUE);

		RETURN_IF_ERROR(MDIButtonSet::Create(&m_controls->mdi_buttonset));
		m_widget_container->GetRoot()->AddChild(m_controls->mdi_buttonset, TRUE);

		RETURN_IF_ERROR(BackgroundWidget::Create(&m_controls->background, this));
		m_widget_container->GetRoot()->AddChild(m_controls->background, TRUE);
	}

	OpView* view;
	RETURN_IF_ERROR(OpView::Create(&view, m_op_window.get()));
	m_op_view.reset(view);

	OpStatus::Ignore(static_cast<X11OpWindow*>(m_op_window.get())->MakeBackgroundListener());

	GetOpView()->SetMouseListener(this);
	GetOpView()->SetPaintListener(this);

	return OpStatus::OK;
}


void X11WindowDecorationEdge::Show()
{
	if (!(m_view_state&EMPTY))
		m_op_window->Show(TRUE);

	// Note: EMPTY can still be set after this. Intended
	m_view_state &= ~HIDDEN;
	m_view_state |= VISIBLE;
}

void X11WindowDecorationEdge::Hide()
{
	m_op_window->Show(FALSE);

	// Note: EMPTY can still be set after this. Intended
	m_view_state &= ~VISIBLE;
	m_view_state |= HIDDEN;
}

void X11WindowDecorationEdge::SetActive(bool active)
{
	m_active = active;
	// We do not trigger an update here
}


void X11WindowDecorationEdge::SetHighlightActive(bool highlight_active)
{
	m_highlight_active = highlight_active;
	// We do not trigger an update here
}


void X11WindowDecorationEdge::SetGeometry(int x, int y, int width, int height)
{
	if (width<=0 || height<=0)
	{
		m_view_state |= EMPTY;
		m_op_window->Show(FALSE);
	}
	else
	{
		m_op_window->SetOuterPos(x, y);
		m_op_window->SetOuterSize(width, height);

		GetOpView()->SetPos(0, 0);
		GetOpView()->SetSize(width, height);

		if (m_view_state == (VISIBLE|EMPTY))
		{
			m_op_window->Show(TRUE);
			m_view_state = VISIBLE;
		}
	}

	if (m_view_state == VISIBLE)
	{
		OpWidget* root = m_widget_container->GetRoot();
		root->SetRTL(UiDirection::Get() == UiDirection::RTL);
		root->SetRect(OpRect(x,y,width,height));

		if (m_controls.get())
		{
			INT32 item_width, item_height;
			m_controls->menu->GetPreferedSize(&item_width, &item_height, 0, 0);
			OpRect rect(0, 0, item_width, item_height);
			root->SetChildRect(m_controls->menu, rect);

			int button_offset = 1;

			// Button set where we display system min/max/restore/close buttons
			m_controls->wm_buttonset->GetPreferedSize(&item_width, &item_height, 0, 0);
			rect = OpRect(width-item_width-button_offset, (height-item_height)/2, item_width, item_height);
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

			m_widget_container->GetView()->SetSize(width, height);
		}
	}
}

unsigned int X11WindowDecorationEdge::GetPreferredheight()
{
	int height = 1; // Suitable default. We will only use this for the top edge that contains controls
	if (m_controls.get())
	{
		INT32 item_width, item_height;
		m_controls->menu->GetPreferedSize(&item_width, &item_height, 0, 0);
		if (height < item_height)
			height = item_height;

		// Button set where we display mdi min/max/restore/close buttons
		m_controls->mdi_buttonset->GetPreferedSize(&item_width, &item_height, 0, 0);
		if (height < item_height)
			height = item_height;

		// Button set where we display system min/max/restore/close buttons
		m_controls->wm_buttonset->GetPreferedSize(&item_width, &item_height, 0, 0);
		if (height < item_height)
			height = item_height;
	}

	return height;
}

X11Widget::MoveResizeAction X11WindowDecorationEdge::GetAction(const OpPoint& point, UINT32 width, UINT32 height)
{
	X11Widget::MoveResizeAction action = X11Widget::MOVE_RESIZE_NONE;

	if (m_orientation == LEFT || m_orientation == RIGHT)
	{
		if (point.y < m_corner_size)
			action = m_orientation == LEFT ? X11Widget::MOVE_RESIZE_TOPLEFT : X11Widget::MOVE_RESIZE_TOPRIGHT;
		else if (static_cast<int>(height) - point.y < m_corner_size)
			action = m_orientation == LEFT ? X11Widget::MOVE_RESIZE_BOTTOMLEFT : X11Widget::MOVE_RESIZE_BOTTOMRIGHT;
		else
			action = m_orientation == LEFT ? X11Widget::MOVE_RESIZE_LEFT : X11Widget::MOVE_RESIZE_RIGHT;
	}
	else if (m_orientation == TOP || m_orientation == BOTTOM)
	{
		if (point.x < m_corner_size)
			action = m_orientation == TOP ? X11Widget::MOVE_RESIZE_TOPLEFT : X11Widget::MOVE_RESIZE_BOTTOMLEFT;
		else if (static_cast<int>(width) - point.x < m_corner_size)
			action = m_orientation == TOP ? X11Widget::MOVE_RESIZE_TOPRIGHT : X11Widget::MOVE_RESIZE_BOTTOMRIGHT;
		else
			action = m_orientation == TOP ? (point.y > m_resize_margin ? X11Widget::MOVE_RESIZE_MOVE : X11Widget::MOVE_RESIZE_TOP) : X11Widget::MOVE_RESIZE_BOTTOM;
	}

	return action;
}


X11Widget::MoveResizeAction X11WindowDecorationEdge::UpdateCursorShape(const OpPoint& point)
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


bool X11WindowDecorationEdge::IsCaptured() const
{
	X11Widget* captured = g_x11->GetWidgetList()->GetCapturedWidget();
	if (captured && m_captured)
	{
		X11Widget* widget = static_cast<X11OpWindow*>(m_op_window.get())->GetOuterWidget();
		return captured == widget;
	}
	return false;
}


void X11WindowDecorationEdge::OnPaint(const OpRect &rect, OpView* view)
{
	OpPainter* painter = view->GetPainter(rect);
	if (!painter)
		return;

	UINT32 width, height;
	view->GetSize(&width, &height);
	OpRect view_rect(0, 0, width, height);

	UINT32 color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_BACKGROUND);
	if (m_active && m_highlight_active)
	{
		// We only darken the active border a bit. The border will, if all goes well
		// and we use a persona skin, be redrawn by the skin manager so this serves
		// as a fallback
		float dim_factor = 0.85;
		UINT8 r = (UINT8)(OP_GET_R_VALUE(color) * dim_factor);
		UINT8 g = (UINT8)(OP_GET_G_VALUE(color) * dim_factor);
		UINT8 b = (UINT8)(OP_GET_B_VALUE(color) * dim_factor);
		painter->SetColor(r, g, b, OP_GET_A_VALUE(color));
	}
	else
		painter->SetColor(OP_GET_R_VALUE(color), OP_GET_G_VALUE(color), OP_GET_B_VALUE(color), OP_GET_A_VALUE(color));
	painter->FillRect(view_rect);

	m_vis_dev->BeginPaint(painter, view_rect, rect);

	if (g_x11->HasPersonaSkin())
	{
		const char* name = "";
		switch (m_orientation)
		{
		case TOP:
			name = "Top Decoration Skin";
			break;
		case BOTTOM:
			name = "Bottom Decoration Skin";
			break;
		case LEFT:
			name = "Left Decoration Skin";
			break;
		case RIGHT:
			name = "Right Decoration Skin";
			break;
		}
		OpSkinElement* skin_element = g_skin_manager->GetSkinElement(name);
		if (skin_element)
			g_skin_manager->DrawElement(m_vis_dev.get(), name, view_rect);
	}

	if (m_draw_border)
	{
		// The skin can not draw borders well since we shape the top window
		painter->SetColor(m_border_color);
		if (m_orientation == TOP)
			painter->DrawLine(OpPoint(0,0), width, TRUE);
		if (m_orientation == BOTTOM)
			painter->DrawLine(OpPoint(0,height-1), width, TRUE);
		if (m_orientation == LEFT)
		{
			painter->DrawLine(OpPoint(0,0), height, FALSE);
			// Upper corner
			painter->DrawLine(OpPoint(1,0), 3, FALSE);
			painter->DrawLine(OpPoint(1,1), 3, TRUE);
			// Lower corner
			painter->DrawLine(OpPoint(1,height-3), 3, FALSE);
			painter->DrawLine(OpPoint(1,height-2), 3, TRUE);
		}
		if (m_orientation == RIGHT)
		{
			painter->DrawLine(OpPoint(width-1,0), height, FALSE);
			// Upper corner
			painter->DrawLine(OpPoint(width-2,0), 3, FALSE);
			painter->DrawLine(OpPoint(width-3,1), 3, TRUE);
			// Lower corner
			painter->DrawLine(OpPoint(width-2,height-3), 3, FALSE);
			painter->DrawLine(OpPoint(width-3,height-2), 3, TRUE);
		}
	}

	m_vis_dev->EndPaint();

	view->ReleasePainter(rect);
}


void X11WindowDecorationEdge::OnMouseMove(const OpPoint &point, ShiftKeyState shift_state, OpView* view)
{
	X11Widget::MoveResizeAction action = UpdateCursorShape(point);
	if (IsCaptured())
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


void X11WindowDecorationEdge::OnMouseDown(MouseButton button, UINT8 nclicks, ShiftKeyState shift_state, OpView* view)
{
	if (button == MOUSE_BUTTON_1)
	{
		if (nclicks == 1)
		{
			m_captured = true;

			OpPoint point;
			view->GetMousePos(&point.x, &point.y);

			UpdateCursorShape(point);
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


void X11WindowDecorationEdge::OnMouseUp(MouseButton button, ShiftKeyState shift_state, OpView* view)
{
	if (button == MOUSE_BUTTON_1)
	{
		m_captured = false;

		OpPoint point;
		view->GetMousePos(&point.x, &point.y);

		UpdateCursorShape(point);
	}
	else if (button == MOUSE_BUTTON_2)
	{
		// Can't do this in Init as the desktop window is not fully set up then
		DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(m_op_window.get()->GetRootWindow());
		g_input_manager->SetMouseInputContext(dw);

		g_application->GetMenuHandler()->ShowPopupMenu("Menubar Menu", PopupPlacement::AnchorAtCursor());
	}
}


void X11WindowDecorationEdge::SetMaximized(bool maximized)
{
	if (m_controls.get())
		m_controls->wm_buttonset->UpdateMode();
}


void X11WindowDecorationEdge::ShowControls(bool show_menu, bool show_wm_buttons, bool show_mdi_buttons)
{
	if (m_controls.get())
	{
		m_controls->menu->SetVisibility(show_menu);
		m_controls->wm_buttonset->SetVisibility(show_wm_buttons);
		m_controls->mdi_buttonset->SetVisibility(show_mdi_buttons);
	}
}


void X11WindowDecorationEdge::Invalidate()
{
	if (m_controls.get())
		m_controls->background->InvalidateAll();
}


OpView* X11WindowDecorationEdge::GetOpView()
{
	return m_op_view.get();
}


OP_STATUS WMButton::Create(WMButton** obj)
{
	*obj = OP_NEW(WMButton, ());
	if (*obj == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}


void WMButton::SetMode(Mode mode)
{
	if (m_mode != mode)
	{
		m_mode = mode;
		switch (m_mode)
		{
		case WMMinimize:
			GetForegroundSkin()->SetImage("Window Manager Minimize");
			break;
		case WMMaximize:
			GetForegroundSkin()->SetImage("Window Manager Maximize");
			break;
		case WMRestore:
			GetForegroundSkin()->SetImage("Window Manager Restore");
			break;
		case WMClose:
			GetForegroundSkin()->SetImage("Window Manager Close");
			break;
		}
	}
}


// static
OP_STATUS WMButtonSet::Create(WMButtonSet** obj)
{
	*obj = OP_NEW(WMButtonSet, ());
	if (*obj == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return (*obj)->Init();
}


OP_STATUS WMButtonSet::Init()
{
	for (int i=0; i<3; i++)
	{
		RETURN_IF_ERROR(WMButton::Create(&button[i]));
		button[i]->SetParentInputContext(0);
		button[i]->SetSystemFont(OP_SYSTEM_FONT_UI_TOOLBAR);
		button[i]->SetListener(this);
		AddChild(button[i], TRUE);
	}
	button[0]->SetMode(WMButton::WMMinimize);
	button[1]->SetMode(WMButton::WMMaximize);
	button[2]->SetMode(WMButton::WMClose);

	return OpStatus::OK;
}


void WMButtonSet::UpdateMode()
{
	OpWindow* window = GetParentOpWindow();
	if (window)
		window = window->GetRootWindow();
	if (window)
	{
		OpRect dummy;
		OpWindow::State state;
		window->GetDesktopPlacement(dummy, state);
		SetMaximizedMode(state == OpWindow::MAXIMIZED);
	}
	m_update_mode_pending = false;
}


void WMButtonSet::SetMaximizedMode(bool maximized)
{
	button[1]->SetMode(maximized ? WMButton::WMRestore : WMButton::WMMaximize);
}


void WMButtonSet::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	*h = *w = 0;
	for (int i=0; i<3; i++)
	{
		int ww, hh;
		button[i]->GetPreferedSize(&ww, &hh, 0, 0);
		if (*h < hh)
			*h = hh;
	}
	if (*h <= 0 || *h > 25)
		*h = 25;

	*w = *h*3 + m_spacing*2;
}


void WMButtonSet::OnLayout()
{
	int w = GetWidth();
	int h = GetHeight();
	int unit = 25;
	if (unit > h)
		unit = h;

	OpRect rect(w-unit, (h-unit)/2, unit, unit);
	for (int i=2; i>=0; i--)
	{
		SetChildRect(button[i], rect);
		rect.x -= unit+m_spacing;
	}
	// Ensure that we use the correct max/restore action. Ideally
	// we should listen to the desktop window for state changes but
	// there is no hook for that. This workaround is trivial so no big deal
	m_update_mode_pending = true;
}


void WMButtonSet::OnClick(OpWidget* widget, UINT32 id)
{
	for (int i=0; i<3; i++)
	{
		if (widget == button[i])
		{
			OpWindow* window = GetParentOpWindow();
			if (window)
				window = window->GetRootWindow();
			if (window)
			{
				switch (button[i]->GetMode())
				{
					case WMButton::WMMinimize:
						window->Minimize();
					break;

					case WMButton::WMMaximize:
						window->Maximize();
						UpdateMode();
					break;

					case WMButton::WMRestore:
						window->Restore();
						UpdateMode();
					break;

					case WMButton::WMClose:
					{
						X11Widget* widget = static_cast<X11OpWindow*>(window)->GetOuterWidget();
						if (widget)
							widget->ConfirmClose();
					}
					break;
				}
			}
			break;
		}
	}
}


MDIButtonSet::MDIButtonSet()
	:DesktopWindowSpy(FALSE)
	,m_desktop_window(NULL)
	,m_unit_size(17)
	,m_big_spacing(2)
{
}


OP_STATUS MDIButton::Create(MDIButton** obj)
{
	*obj = OP_NEW(MDIButton, ());
	if (*obj == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}

	(*obj)->SetButtonTypeAndStyle(OpButton::TYPE_PUSH, OpButton::STYLE_IMAGE, TRUE);

	return OpStatus::OK;
}


void MDIButton::SetMode(Mode mode)
{
	if (m_mode != mode)
	{
		UINT8* data;
		UINT32 size;

		switch (mode)
		{
		case MDIMinimize:
			IconSource::GetMenuMinimize(data, size);
			break;
		case MDIRestore:
			IconSource::GetMenuRestore(data, size);
			break;
		case MDIClose:
			IconSource::GetMenuClose(data, size);
			break;
		}

		SimpleFileImage* image = OP_NEW(SimpleFileImage,(data, size));
		if (image)
		{
			m_image.reset(image);
			OpWidgetImage widget_image;
			Image img = m_image->GetImage();
			widget_image.SetBitmapImage(img);
			GetForegroundSkin()->SetWidgetImage(&widget_image);
			m_mode = mode; // Only change mode when we know it is in sync with the displayed icon
		}
	}
}

// static
OP_STATUS MDIButtonSet::Create(MDIButtonSet** obj)
{
	*obj = OP_NEW(MDIButtonSet, ());
	if (*obj == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return (*obj)->Init();
}


OP_STATUS MDIButtonSet::Init()
{
	for (int i=0; i<3; i++)
	{
		RETURN_IF_ERROR(MDIButton::Create(&m_button[i]));
		m_button[i]->SetParentInputContext(0);
		m_button[i]->SetSystemFont(OP_SYSTEM_FONT_UI_TOOLBAR);
		m_button[i]->SetListener(this);
		AddChild(m_button[i], TRUE);
	}
	m_button[0]->SetMode(MDIButton::MDIMinimize);
	m_button[1]->SetMode(MDIButton::MDIRestore);
	m_button[2]->SetMode(MDIButton::MDIClose);

	return OpStatus::OK;
}


void MDIButtonSet::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	*h = m_unit_size;
	*w = m_unit_size*3 + m_big_spacing;
}


void MDIButtonSet::OnLayout()
{
	if (!GetSpyInputContext())
	{
		// We can not set the context in OnShow() (too early because of the window access)
		OpWindow* window = GetParentOpWindow();
		if (window)
			window = window->GetRootWindow();
		DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(window);
		if (dw)
		{
			SetSpyInputContext(dw);
			UpdateTargetDesktopWindow();
		}
	}

	int h = GetHeight();
	int unit = m_unit_size;
	if (unit > h)
		unit = h;

	OpRect rect;
	for (int i=0; i<3; i++)
	{
		if (i==0)
		{
			rect = OpRect(0, (h - unit)/2, unit, unit);
		}
		else
		{
			rect.x += unit + (i==2 ? m_big_spacing : 0);
		}

		SetChildRect(m_button[i], rect);
	}
}


void MDIButtonSet::OnDeleted()
{
	SetSpyInputContext(NULL, FALSE);
}



void MDIButtonSet::OnTargetDesktopWindowChanged(DesktopWindow* target_window)
{
	if (m_desktop_window != target_window)
	{
		m_desktop_window = target_window;
	}
}


void MDIButtonSet::OnClick(OpWidget* widget, UINT32 id)
{
	for (int i=0; i<3; i++)
	{
		if (widget == m_button[i])
		{
			if (m_desktop_window)
			{
				switch (m_button[i]->GetMode())
				{
					case MDIButton::MDIMinimize:
						m_desktop_window->Minimize();
					break;

					case MDIButton::MDIMaximize:
						m_desktop_window->Maximize();
					break;

					case MDIButton::MDIRestore:
						m_desktop_window->Restore();
					break;

					case MDIButton::MDIClose:
						m_desktop_window->Close(FALSE, TRUE, FALSE);
					break;
				}
			}
			break;
		}
	}
}

