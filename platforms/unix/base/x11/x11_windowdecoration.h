/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef X11_WINDOW_DECORATION_H
#define X11_WINDOW_DECORATION_H

#include "modules/display/vis_dev.h"
#include "modules/pi/OpView.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpButton.h"

#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h" // SimpleFileImage
#include "adjunct/quick_toolkit/windows/DesktopWindow.h" // DesktopWindowSpy

#include "platforms/unix/base/x11/x11_widget.h"


class OpWindow;
class OpToolbarMenuButton;
class BackgroundWidget;


class WMButton : public OpButton
{
public:
	enum Mode
	{
		WMMinimize,
		WMMaximize,
		WMRestore,
		WMClose,
		WMDummy
	};

public:
	static OP_STATUS Create(WMButton** obj);
	void SetMode(Mode mode);
	Mode GetMode() const { return m_mode; }

protected:
	WMButton()
		:OpButton(OpButton::TYPE_TOOLBAR, OpButton::STYLE_IMAGE)
		,m_mode(WMDummy) {}

private:
	Mode m_mode;
};


class WMButtonSet : public OpWidget
{
public:
	WMButtonSet() : m_spacing(3), m_update_mode_pending(false) {}

	static OP_STATUS Create(WMButtonSet** obj);

	void UpdateMode();
	void SetMaximizedMode(bool maximized);

	// OpWidget
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void OnLayout();
	virtual void OnBeforePaint() { if (m_update_mode_pending) UpdateMode(); }

	// OpWidgetListener
	virtual void OnClick(OpWidget *widget, UINT32 id);

private:
	OP_STATUS Init();

private:
	OpAutoPtr<WidgetContainer> m_widget_container;
	WMButton* button[3];
	const int m_spacing;
	bool m_update_mode_pending;
};


class MDIButton : public OpButton
{
public:
	enum Mode
	{
		MDIMinimize,
		MDIMaximize,
		MDIRestore,
		MDIClose,
		MDIDummy
	};

public:
	static OP_STATUS Create(MDIButton** obj);
	void SetMode(Mode mode);
	Mode GetMode() const { return m_mode; }

protected:
	MDIButton()
		:OpButton(OpButton::TYPE_PUSH, OpButton::STYLE_IMAGE)
		,m_mode(MDIDummy){}

private:
	Mode m_mode;
	OpAutoPtr<SimpleFileImage> m_image;
};


class MDIButtonSet : public OpWidget, public DesktopWindowSpy
{
public:
	MDIButtonSet();

	static OP_STATUS Create(MDIButtonSet** obj);

	void UpdateMode();
	void SetMaximizedMode(bool maximized);

	// OpWidget
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void OnLayout();
	virtual void OnDeleted();

	// DesktopWindowSpy
	virtual void OnTargetDesktopWindowChanged(DesktopWindow* target_window);

	// OpWidgetListener
	virtual void OnClick(OpWidget *widget, UINT32 id);

private:
	OP_STATUS Init();

private:
	OpAutoPtr<WidgetContainer> m_widget_container;
	MDIButton* m_button[3];
	DesktopWindow* m_desktop_window;
	const int m_unit_size;
	const int m_big_spacing;
};



class X11WindowDecorationEdge
	:public OpInputContext
	,public OpPaintListener
	,public OpMouseListener
{
public:
	enum Orientation
	{
		TOP,
		BOTTOM,
		LEFT,
		RIGHT
	};

private:
	enum ViewState
	{
		VISIBLE = 0x01, // The edge is visible
		HIDDEN  = 0x02, // The edge is hidden
		EMPTY   = 0x04  // The edge is empty (width and/or height is 0)
	};

	struct Controls
	{
		BackgroundWidget* background;
		OpToolbarMenuButton* menu;
		WMButtonSet* wm_buttonset;
		MDIButtonSet* mdi_buttonset;
	};

public:
	X11WindowDecorationEdge();

	OP_STATUS Init(void *parent_handle, OpWindow* parent_window, Orientation orientation);

	void Show();
	void Hide();
	void SetActive(bool active);
	void SetHighlightActive(bool highlight_active);
	void SetCanMoveResize(bool can_move_resize) { m_can_move_resize = can_move_resize; }
	void SetDrawBorder(bool draw_border) { m_draw_border = draw_border; }
	void SetBorderColor(UINT32 border_color) { m_border_color = border_color; }
	void SetMaximized(bool maximized);
	void ShowControls(bool show_menu, bool show_wm_buttons, bool show_mdi_buttons);
	void Invalidate();
	void SetGeometry(int x, int y, int width, int height);
	unsigned int GetPreferredheight();

	X11Widget::MoveResizeAction GetAction(const OpPoint& point, UINT32 width, UINT32 height);
	X11Widget::MoveResizeAction UpdateCursorShape(const OpPoint& point);
	bool IsCaptured() const;

	// OpPaintListener
	virtual void OnPaint(const OpRect &rect, OpView* view);
	// OpMouseListener
	virtual void OnMouseMove(const OpPoint &point, ShiftKeyState shift_state, OpView* view);
	virtual void OnMouseDown(MouseButton button, UINT8 nclicks, ShiftKeyState shift_state, OpView* view);
	virtual void OnMouseUp(MouseButton button, ShiftKeyState shift_state, OpView* view);
	virtual void OnMouseLeave() {}
	virtual BOOL OnMouseWheel(INT32 delta, BOOL vertical, ShiftKeyState shift_state) { return FALSE; }
	virtual void OnSetCursor() {}
	virtual void OnMouseCaptureRelease() {}

public:
	OpView* GetOpView();

private:
	Orientation m_orientation;
	int m_view_state;
	OpAutoPtr<OpWindow> m_op_window;
	OpAutoPtr<OpView> m_op_view;
	OpAutoPtr<VisualDevice> m_vis_dev;
	OpAutoPtr<WidgetContainer> m_widget_container;
	OpAutoPtr<Controls> m_controls;
	UINT32 m_border_color; // Format 0xAABBGGRR
	const int m_corner_size;
	const int m_resize_margin;
	bool m_captured;
	bool m_active;
	bool m_can_move_resize;
	bool m_draw_border;
	bool m_highlight_active;
};

class X11WindowDecoration
	:public OpInputContext
{
public:
	struct Borders
	{
	public:
		Borders() { top = bottom = left = right = 0; }
		UINT32 top;
		UINT32 bottom;
		UINT32 left;
		UINT32 right;
	};

public:
	X11WindowDecoration();

	OP_STATUS Init(void *parent_handle, OpWindow* parent_window);

	void Show();
	void Hide();
	void Activate();
	void Deactivate();
	void SetHighlightActive(bool highlight_active);
	void SetCanMoveResize(bool can_move_resize);
	void SetDrawBorder(bool draw_border);
	void SetBorderColor(UINT32 border_color);
	void Invalidate();
	void SetMaximized(bool maximized);
	void ShowControls(bool show_menu, bool show_wm_buttons, bool show_mdi_buttons);

	void SetBorders(UINT32 top, UINT32 bottom, UINT32 left, UINT32 right, bool can_override)
	{
		// We can not override top size of it has been set to 0
		if (can_override && top > 0)
		{
			unsigned int h = m_edge[0]->GetPreferredheight();
			if (top < h)
				top = h;
		}

		m_borders.top = top;
		m_borders.bottom = bottom;
		m_borders.left = left;
		m_borders.right = right;
	}
	void SetSize(int width, int height);

	bool IsVisible() const { return m_visible; }

	UINT32 Left() const { return m_visible ? m_borders.left : 0; }
	UINT32 Right() const { return m_visible ? m_borders.right : 0; }
	UINT32 Top() const { return m_visible ? m_borders.top : 0; }
	UINT32 Bottom() const { return m_visible ? m_borders.bottom : 0; }
	UINT32 Width() const { return Left() + Right(); }
	UINT32 Height() const { return Top() + Bottom(); }

private:
	bool m_visible;
	Borders m_borders;
	OpAutoPtr<X11WindowDecorationEdge> m_edge[4];
};


#endif // X11_WINDOW_DECORATION_H
