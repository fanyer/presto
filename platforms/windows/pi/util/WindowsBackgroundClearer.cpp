/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/pi/util/WindowsBackgroundClearer.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/pi/WindowsOpMessageLoop.h"
#include "platforms/windows/pi/util/WindowFrame.h"
#include "adjunct/desktop_pi/DesktopOpUiInfo.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/desktop_util/widget_utils/LogoThumbnailClipper.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpButton.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/display/vis_dev.h"

#include <VSStyle.h>
#include <WinUser.h>

extern HINSTANCE hInst;

void AlphaBlt(HDC dst, INT32 dstx, INT32 dsty, INT32 dstwidth, INT32 dstheight, HDC src, INT32 srcx, INT32 srcy, INT32 srcwidth, INT32 srcheight);

#ifdef _DEBUG
extern void OutputDebugLogging(const uni_char *tmp, ...);
extern void OutputDebugLogging(const uni_char *tmp, char *);
#define CLEARER_TRACE(x, y) // OutputDebugLogging(x, y)
#else
#define CLEARER_TRACE(x, y)
#endif // _DEBUG

static inline bool isLanguageRTL()
{
	return g_languageManager &&
		g_languageManager->GetWritingDirection() == OpLanguageManager::RTL;
}

/* static */
OP_STATUS OpBorderButtons::Construct(OpBorderButtons** obj, WindowsOpWindow *window, HTHEME theme, BOOL pagebar_on_top) 
{
	// Only create the quick caption buttons when the title bar is not visible - this covers the conditions where this
	// is the case
  	if(g_skin_manager->HasPersonaSkin() && !window->m_menubar_visibility && theme && (pagebar_on_top || window->m_fullscreen))
	{
		*obj = OP_NEW(QuickBorderButtons, (window, theme)); 
		CLEARER_TRACE(UNI_L("QuickBorderButtons created\n"), "");
	}
	else
	{
		*obj = OP_NEW(NativeBorderButtons, (window, theme)); 
		CLEARER_TRACE(UNI_L("NativeBorderButtons created\n"), "");
	}
	if (*obj == NULL) 
		return OpStatus::ERR_NO_MEMORY; 

	OP_STATUS s = (*obj)->Init();
	if (OpStatus::IsError(s)) 
	{
		OP_DELETE(*obj);
		return s;
	} 
	return OpStatus::OK; 
}

/**
 *  Those classes handles the regular themed max/min/close buttons in the windows border
 */

WindowsRectOffsets NativeBorderButtons::NativeBorderButton::ButtonIdToRectId()
{
	OP_ASSERT(id != WINPART_NONE);
	switch(id)
	{
	case WINPART_CLOSE: return WIN_RECT_CLOSE_BUTTON;
	case WINPART_MAXIMIZE: return WIN_RECT_MAXIMIZE_BUTTON;
	case WINPART_MINIMIZE: return WIN_RECT_MINIMIZE_BUTTON;
	default: return WIN_RECT_CLOSE_BUTTON; //for the compiler to stop b... nagging about codepaths
	}
}

int NativeBorderButtons::NativeBorderButton::ButtonIdToWindowPart(bool is_maximized)
{
	OP_ASSERT(id != WINPART_NONE);
	switch(id)
	{
	case WINPART_CLOSE: return WP_CLOSEBUTTON;
	case WINPART_MAXIMIZE: return is_maximized ? WP_RESTOREBUTTON : WP_MAXBUTTON;
	case WINPART_MINIMIZE: return WP_MINBUTTON;
	default: return WP_CLOSEBUTTON; //for the compiler
	}
}

int NativeBorderButtons::NativeBorderButton::GetCurrentButtonState(WindowsButtonId pushed, WindowsButtonId hovered, bool /*is_maximized*/, bool is_hwnd_active)
{
	// 1. the hovered is respected no matter if windows is active or not
	// 2. if a button is pressed, however, hovered is not respected anymore;
	//    with pressed, hovered is only used to apply PUSHED [3] state
	// 3. clicking a not active window activates it on mouse down; hovered and pressed are not used
	// 4. clicking an active window sends a sys command on mouse up

	if (is_hwnd_active && pushed != WINPART_NONE)
		return pushed == id && hovered == id ? CBS_PUSHED : CBS_NORMAL;
	else
		return hovered == id ? CBS_HOT : is_hwnd_active ? CBS_NORMAL : CBS_DISABLED;
}

void NativeBorderButtons::NativeBorderButton::Paint(HTHEME theme, HDC hdc, WindowsButtonId pushed, WindowsButtonId hovered, bool is_maximized, bool is_hwnd_active)
{
	DrawThemeBackground(theme, hdc, ButtonIdToWindowPart(is_maximized), GetCurrentButtonState(pushed, hovered, is_maximized, is_hwnd_active), &btn_rect, NULL);
}

OP_STATUS NativeBorderButtons::Init()
{
	METHOD_CALL;

	SetListener(this);
	SetUpdateNeededWhen(UPDATE_NEEDED_WHEN_VISIBLE);

	// add us first so we get painted last
	m_window->GetDesktopWindow()->GetWidgetContainer()->GetRoot()->AddChild(this, FALSE, TRUE);

	return OpStatus::OK;
}

NativeBorderButtons::~NativeBorderButtons()
{
	SetListener(NULL);
	Remove();
}

WindowsButtonId NativeBorderButtons::ButtonFromPoint(const OpPoint &point)
{
	OpRect rect = GetRect();
	POINT pt = { point.x + rect.x, point.y + rect.y };

#if 0
	const char* part = "WINPART_NONE";
	if (PtInRect(&m_btn_close.GetRect(), pt))
		part = "WINPART_CLOSE";
	else if (PtInRect(&m_btn_maximize.GetRect(), pt))
		part = "WINPART_MAXIMIZE";
	else if (PtInRect(&m_btn_minimize.GetRect(), pt))
		part = "WINPART_MINIMIZE";

	char buffer[1024];
	sprintf(buffer, "(%d, %d) -> (%d, %d) -> %s\n", point.x, point.y, pt.x, pt.y, part);
	OutputDebugStringA(buffer);
#endif

	if (PtInRect(&m_btn_close.GetRect(), pt))
		return WINPART_CLOSE;
	else if (PtInRect(&m_btn_maximize.GetRect(), pt))
		return WINPART_MAXIMIZE;
	else if (PtInRect(&m_btn_minimize.GetRect(), pt))
		return WINPART_MINIMIZE;

	return WINPART_NONE;
}

void NativeBorderButtons::Paint(HDC dc)
{
	METHOD_CALL;

	if(!dc)
		return;

	if (ShouldNotLayOut())
		return;

	bool active = m_window->m_hwnd_active != FALSE;
	bool maximized = m_window->m_state == OpWindow::MAXIMIZED;

	m_btn_close.Paint(m_theme, dc, m_pushed, m_hovered, maximized, active);
	m_btn_maximize.Paint(m_theme, dc, m_pushed, m_hovered, maximized, active);
	m_btn_minimize.Paint(m_theme, dc, m_pushed, m_hovered, maximized, active);
}

void NativeBorderButtons::GetSize(INT32* w, INT32* h)
{
	*w = GetSystemMetrics(SM_CXSIZE) * WIN_RECT_MAX;
	*h = GetSystemMetrics(SM_CYSIZE);
}

struct vals
{
	template <typename T>
	static T minval(T a, T b) { return a < b ? a : b; }

	template <typename T>
	static T minval(T a, T b, T c) { return minval(c, minval(a, b)); }

	template <typename T>
	static T maxval(T a, T b) { return a > b ? a : b; }

	template <typename T>
	static T maxval(T a, T b, T c) { return maxval(c, maxval(a, b)); }
};

bool NativeBorderButtons::ShouldNotLayOut()
{
	//mimicking WindowFrame::UseCustomNativeFrame (only part of the expression is used):
	OpPagebar* pb = m_window->GetPagebar();
	bool pagebar_on_top = pb && (pb->GetAlignment() == OpBar::ALIGNMENT_TOP && m_window->m_requested_titlebar_height > 0);

	// unless the skin is transparent, we're not going to want any kind of special close buttons hit handling
	bool transparent = !!(g_skin_manager->GetOptionValue("Transparency", FALSE) || g_skin_manager->GetOptionValue("Full Transparency", FALSE));

	//Should layout on UX theme-enabled skins without menu bar and with pagebar on top
	return m_window->m_menubar_visibility || !m_theme || !pagebar_on_top || !transparent;
}

void NativeBorderButtons::OnLayout(OpWidget *widget)
{
	if (widget != this)
		return;

	// Normally, client area is placed within
	// non-client area and is separate from the frame.
	//
	// If, however, we have an UX theme and we have OMenu,
	// then the coordinates are counted as if (0,0) was in
	// top-left corner of the window, including frame.
	//
	// If we do not have any theme, or we have a standard
	// menu bar, then we do not draw inside the frame and
	// the (0,0) will be within client area and the native
	// buttons will end up covering the trash button (tab
	// restore button).
	//
	// We don't want this...

	if (ShouldNotLayOut())
	{
		SetRect(OpRect());
		return;
	}

	RECT btnrect[WIN_RECT_MAX];
	CalculateCaptionButtonRects(m_window, btnrect);

	//finding outline of all three buttons,
	//including spaces in between
	RECT out;
	out.left = vals::minval(
		btnrect[WIN_RECT_CLOSE_BUTTON].left,
		btnrect[WIN_RECT_MAXIMIZE_BUTTON].left,
		btnrect[WIN_RECT_MINIMIZE_BUTTON].left);
	out.top = vals::minval(
		btnrect[WIN_RECT_CLOSE_BUTTON].top,
		btnrect[WIN_RECT_MAXIMIZE_BUTTON].top,
		btnrect[WIN_RECT_MINIMIZE_BUTTON].top);
	out.right = vals::maxval(
		btnrect[WIN_RECT_CLOSE_BUTTON].right,
		btnrect[WIN_RECT_MAXIMIZE_BUTTON].right,
		btnrect[WIN_RECT_MINIMIZE_BUTTON].right);
	out.bottom = vals::maxval(
		btnrect[WIN_RECT_CLOSE_BUTTON].bottom,
		btnrect[WIN_RECT_MAXIMIZE_BUTTON].bottom,
		btnrect[WIN_RECT_MINIMIZE_BUTTON].bottom);

	SetRect(OpRect(&out));

	m_btn_close.SetRect(btnrect);
	m_btn_maximize.SetRect(btnrect);
	m_btn_minimize.SetRect(btnrect);
}

void NativeBorderButtons::OnMouseMove(OpWidget *widget, const OpPoint &point)
{
	WindowsButtonId hover = ButtonFromPoint(point);

	if (hover == m_hovered)
		return;

	m_hovered = hover;

	RECT inv = GetRect();
	InvalidateRect(m_window->m_hwnd, &inv, TRUE);
}

void NativeBorderButtons::OnMouseLeave(OpWidget *widget)
{
	WindowsButtonId hover = WINPART_NONE;

	if (hover == m_hovered)
		return;

	m_hovered = hover;

	RECT inv = GetRect();
	InvalidateRect(m_window->m_hwnd, &inv, TRUE);
}

void NativeBorderButtons::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1)
	{
		m_pushed = ButtonFromPoint(point);
		RECT inv = GetRect();
		InvalidateRect(m_window->m_hwnd, &inv, TRUE);
	}
}

void NativeBorderButtons::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1)
	{
		WindowsButtonId hover = ButtonFromPoint(point);
		WindowsButtonId current = m_pushed;
		m_pushed = WINPART_NONE;
		m_hovered = WINPART_NONE; //so that it does not immediately turn HOVERED

		if (hover != current || current == WINPART_NONE)
			return;

		RECT inv = GetRect();
		InvalidateRect(m_window->m_hwnd, &inv, TRUE);

		int command = 0;
		switch(hover)
		{
		case WINPART_CLOSE:
			m_window->GetDesktopWindow()->Close(FALSE, TRUE); //mimicking QuickBorderButtons::OnClick
			return;
		case WINPART_MAXIMIZE:
			command = m_window->m_state == OpWindow::MAXIMIZED ? SC_RESTORE : SC_MAXIMIZE;
			break;
		case WINPART_MINIMIZE:
			command = SC_MINIMIZE;
			break;
		}
		PostMessage(m_window->m_hwnd, WM_SYSCOMMAND, command, 0);
		return;
	}
}

/**
 *  This class handles the persona max/min/close buttons in the windows border
 */
QuickBorderButtons::~QuickBorderButtons()
{
	// make sure the regular buttons are enabled
	for(int x = 0; x < WINICON_COUNT; x++)
	{
		if(!m_window_icons[x].IsEmpty())
		{
			// decrease ref count on images
			m_window_icons[x].DecVisible(null_image_listener);
		}
		OP_DELETE(m_content_providers[x]);
	}
	SetListener(NULL);
}

OP_STATUS QuickBorderButtons::Init()
{
	METHOD_CALL;

	if(!m_window->GetDesktopWindow())
		return OpStatus::ERR_NULL_POINTER;

	static const struct {
		WindowBorderIcons id;
		const uni_char* name;
	}
	indices[] =
	{
		{ WINICON_CLOSE, UNI_L("ZWINCLOSE") },
		{ WINICON_MAXIMIZE, UNI_L("ZWINMAXIMIZE") },
		{ WINICON_MINIMIZE, UNI_L("ZWINMINIMIZE") },
		{ WINICON_RESTORE, UNI_L("ZWINRESTORE") }
	};

	static const struct {
		const uni_char* appendix;
		int off;
	}
	states[] = {
		{ UNI_L(""), 0 },
		{ UNI_L("_HOVER"), WINICON_CLOSE_HOVER - WINICON_CLOSE },
		{ UNI_L("_PRESSED"), WINICON_CLOSE_PRESSED - WINICON_CLOSE },
		{ UNI_L("_RTL"), WINICON_RTL_OFFSET },
		{ UNI_L("_HOVER_RTL"), WINICON_RTL_OFFSET + WINICON_CLOSE_HOVER - WINICON_CLOSE },
		{ UNI_L("_PRESSED_RTL"), WINICON_RTL_OFFSET + WINICON_CLOSE_PRESSED - WINICON_CLOSE },
	};

	// the longest <name, appendix> pair is 24 chars long
	uni_char resid[30]; // ARRAY OK 2012-03-23 mzdun

	for (int i = 0; i < ARRAY_SIZE(indices); ++i)
	{
		uni_strcpy(resid, indices[i].name);
		uni_char* resid_apdx = resid + uni_strlen(resid);
		for (int j = 0; j < ARRAY_SIZE(states); ++j)
		{
			uni_strcpy(resid_apdx, states[j].appendix);
			WindowBorderIcons id = static_cast<WindowBorderIcons>(indices[i].id + states[j].off);
			m_window_icons[id] = CreateReplacementWindowButton(this, id, resid);
		}
	}

	OpStatus::Ignore(OpButton::Construct(&m_btn_minimize, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	OpStatus::Ignore(OpButton::Construct(&m_btn_restore, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	OpStatus::Ignore(OpButton::Construct(&m_btn_close, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));

	OpAutoPtr<OpButton> btn_minimize_p(m_btn_minimize);
	OpAutoPtr<OpButton> btn_close_p(m_btn_close);
	OpAutoPtr<OpButton> btn_restore_p(m_btn_restore);

	// not the end of the world if we OOM above
	if(!(m_btn_close && m_btn_minimize && m_btn_restore))
		return OpStatus::ERR_NO_MEMORY;

	AddChild(btn_minimize_p.release());
	AddChild(btn_restore_p.release());
	AddChild(btn_close_p.release());

	m_language_is_RTL = isLanguageRTL();
	int rtl_diff = m_language_is_RTL ? WINICON_RTL_OFFSET : 0;

	m_btn_minimize->GetBorderSkin()->SetBitmapImage(m_window_icons[WINICON_MINIMIZE+rtl_diff]);
	m_btn_minimize->GetBorderSkin()->SetForeground(TRUE);
	m_btn_minimize->SetName("Window Minimize Button");

	m_btn_restore->GetBorderSkin()->SetBitmapImage(m_window->m_state == OpWindow::MAXIMIZED ? m_window_icons[WINICON_RESTORE+rtl_diff] : m_window_icons[WINICON_MAXIMIZE+rtl_diff]);
	m_btn_restore->GetBorderSkin()->SetForeground(TRUE);
	m_btn_restore->SetName("Window Close Button");

	m_btn_close->GetBorderSkin()->SetBitmapImage(m_window_icons[WINICON_CLOSE+rtl_diff]);
	m_btn_close->GetBorderSkin()->SetForeground(TRUE);
	m_btn_close->SetName("Window Restore Button");

	SetListener(this);

	// avoid that the parent element (this) clears the background when painting the children
	SetSkinIsBackground(TRUE);

	// add us first so we get painted last
	m_window->GetDesktopWindow()->GetWidgetContainer()->GetRoot()->AddChild(this, FALSE, TRUE);

	return OpStatus::OK;
}

void QuickBorderButtons::Layout()
{
	METHOD_CALL;

	INT32 w, h;
	RECT btnrect[WIN_RECT_MAX];
	RECT native_frame = m_window->GetFrame()->GetNativeFrameSizes();

	NativeBorderButtons::CalculateCaptionButtonRects(m_window, btnrect);

	GetSize(&w, &h);

	int right_offset = WINDOW_DEFAULT_BORDER_SIZE + (m_window->m_state == MAXIMIZED ? 0 : WINDOW_DEFAULT_BORDER_SIZE);

	// we know the size of our native frame so use that instead when maximized
	btnrect[0].top = native_frame.top;

	OpRect rect(m_window->m_width - w - right_offset, m_window->m_state == MAXIMIZED ? btnrect[0].top : 0, w, h);

	if (isLanguageRTL())
		rect.x = right_offset;

	SetRect(rect);
}

void QuickBorderButtons::LayoutCustomWindowButtons()
{
	METHOD_CALL;

	bool rtl = isLanguageRTL();
	bool reset_images = rtl != m_language_is_RTL;

	m_language_is_RTL = rtl;

	OpRect bounds = GetBounds();

	struct {
		WindowBorderIcons icon;
		OpButton* button;
	}
	buttons[] = {
		WINICON_CLOSE, m_btn_close,
		WINICON_RESTORE, m_btn_restore,
		WINICON_MINIMIZE, m_btn_minimize
	};

	int height = m_window_icons[WINICON_CLOSE].Height();	// use the height from the close button

	int y = m_window->m_state == MAXIMIZED ? -1 : 0;
	int x = bounds.Right();

	if (rtl)
	{
		op_swap(buttons[0], buttons[2]);
		x = bounds.Left()
			+ m_window_icons[WINICON_CLOSE].Width()
			+ m_window_icons[WINICON_RESTORE].Width()
			+ m_window_icons[WINICON_MINIMIZE].Width();
	}

	OpRect rect(x, y, 0, height);

	for (int i = 0; i < ARRAY_SIZE(buttons); ++i)
	{
		rect.width = m_window_icons[buttons[i].icon].Width();
		rect.x -= rect.width;
		buttons[i].button->SetRect(rect);
	}

	if (reset_images)
	{
		for (int i = 0; i < ARRAY_SIZE(buttons); ++i)
			SetButtonImage(buttons[i].button, FALSE);
	}
}

void QuickBorderButtons::GetSize(INT32* w, INT32* h)
{
	*h = m_window_icons[WINICON_CLOSE].Height();	// use the height from the close button
	*w = m_window_icons[WINICON_CLOSE].Width() + m_window_icons[WINICON_RESTORE].Width() + m_window_icons[WINICON_MINIMIZE].Width();
}

void QuickBorderButtons::OnClick(OpWidget *widget, UINT32 id) 
{
	if(widget == m_btn_close)
	{
		m_window->GetDesktopWindow()->Close(FALSE, TRUE);
	}
	else if(widget == m_btn_minimize)
	{
		PostMessage(m_window->m_hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
	}
	else if(widget == m_btn_restore)
	{
		if(m_window->m_state == OpWindow::MAXIMIZED)
		{
			PostMessage(m_window->m_hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
		}
		else
		{
			SendMessage(m_window->m_hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
		}
	}
}

void QuickBorderButtons::SetButtonImage(OpWidget *widget, BOOL hover, BOOL pressed)
{
	WindowBorderIcons icon = WINICON_COUNT;

	if (widget == m_btn_close)
		icon = WINICON_CLOSE;
	else if (widget == m_btn_minimize)
		icon = WINICON_MINIMIZE;
	else if (widget == m_btn_restore && m_window->m_state == OpWindow::MAXIMIZED)
		icon = WINICON_RESTORE;
	else if (widget == m_btn_restore)
		icon = WINICON_MAXIMIZE;

	if (icon == WINICON_COUNT)
		return;

	if (isLanguageRTL())
		icon = static_cast<WindowBorderIcons>(icon + WINICON_RTL_OFFSET);

	if (pressed)
		icon = static_cast<WindowBorderIcons>(icon + WINICON_CLOSE_PRESSED - WINICON_CLOSE);
	else if (hover)
		icon = static_cast<WindowBorderIcons>(icon + WINICON_CLOSE_HOVER - WINICON_CLOSE);

	widget->GetBorderSkin()->SetBitmapImage(m_window_icons[icon]);
}

// assumes the resource is in the resource section as RCDATA in PNG format
/* static */
Image QuickBorderButtons::CreateReplacementWindowButton(QuickBorderButtons* btn_class, WindowBorderIcons icon_offset, const uni_char *resource_name)
{
	Image image;
	HRSRC res = FindResource(hInst, resource_name, RT_RCDATA);
	RETURN_VALUE_IF_NULL(res, image);

	HGLOBAL data = LoadResource(hInst, res);
	OP_ASSERT(data);
	RETURN_VALUE_IF_NULL(data, image);

	DWORD size = SizeofResource(hInst, res);
	OP_ASSERT(size);
	if(!size)
		return image;

	LPVOID res_data = LockResource(data);	// documented to not need any unlock call (and none exists)
	OP_ASSERT(res_data);
	RETURN_VALUE_IF_NULL(res_data, image);

	// we load it with an new content provider. Each content provider must be unique for the lifetime of
	// the data
	btn_class->m_content_providers[icon_offset] = OP_NEW(SkinImageContentProvider, ((char *)res_data, size));
	RETURN_VALUE_IF_NULL(btn_class->m_content_providers[icon_offset], image);

	image = imgManager->GetImage(btn_class->m_content_providers[icon_offset]);

	image.IncVisible(null_image_listener); 
	image.OnLoadAll(btn_class->m_content_providers[icon_offset]); 

	if(!image.ImageDecoded())
		image.DecVisible(null_image_listener);

	return image;
}

/* static */
void OpBorderButtons::CalculateCaptionButtonRects(WindowsOpWindow* window, RECT* btnrect)
{
	if (IsSystemWinVista())
	{
		TITLEBARINFOEX tinf;
		tinf.cbSize = sizeof(TITLEBARINFOEX);
		SendMessage(window->m_hwnd, WM_GETTITLEBARINFOEX, NULL, (LPARAM)&tinf);
		POINT p;
		p.x = 0;
		p.y = 0;
		ClientToScreen(window->m_hwnd, &p);
		btnrect[WIN_RECT_CLOSE_BUTTON] = tinf.rgrect[5];
		OffsetRect(&btnrect[WIN_RECT_CLOSE_BUTTON], -p.x, -p.y);
		btnrect[WIN_RECT_MAXIMIZE_BUTTON] = tinf.rgrect[3];
		OffsetRect(&btnrect[WIN_RECT_MAXIMIZE_BUTTON], -p.x, -p.y);
		btnrect[WIN_RECT_MINIMIZE_BUTTON] = tinf.rgrect[2];
		OffsetRect(&btnrect[WIN_RECT_MINIMIZE_BUTTON], -p.x, -p.y);
	}
	else
	{
		UINT32 winw, winh;
		window->GetInnerSize(&winw, &winh);
		// FIXME: get the correct rects for xp
		SIZE s;
		s.cx = GetSystemMetrics(SM_CXSIZE)-4;
		s.cy = GetSystemMetrics(SM_CYSIZE)-4;
		btnrect[WIN_RECT_CLOSE_BUTTON].top = GetSystemMetrics(SM_CYSIZEFRAME)+(GetSystemMetrics(SM_CYCAPTION)-s.cy)/2;
		btnrect[WIN_RECT_CLOSE_BUTTON].bottom = btnrect[WIN_RECT_CLOSE_BUTTON].top+s.cy;
		btnrect[WIN_RECT_CLOSE_BUTTON].right = winw-2;
		btnrect[WIN_RECT_CLOSE_BUTTON].left = btnrect[WIN_RECT_CLOSE_BUTTON].right - s.cx;

		btnrect[WIN_RECT_MAXIMIZE_BUTTON] = btnrect[WIN_RECT_CLOSE_BUTTON];
		OffsetRect(&btnrect[WIN_RECT_MAXIMIZE_BUTTON], -s.cx-2, 0);

		btnrect[WIN_RECT_MINIMIZE_BUTTON] = btnrect[WIN_RECT_MAXIMIZE_BUTTON];
		OffsetRect(&btnrect[WIN_RECT_MINIMIZE_BUTTON], -s.cx-2, 0);
	}

	if (isLanguageRTL())
	{
		RECT cli;
		GetClientRect(window->m_hwnd, &cli);
		LONG leftandright = cli.left + cli.right;

		for (int i = 0; i < WIN_RECT_MAX; i++)
		{
			LONG width = btnrect[i].right - btnrect[i].left;
			btnrect[i].left = leftandright - btnrect[i].right;
			btnrect[i].right = btnrect[i].left + width;
		}
	}
}

/*************************************************************************
*
* The background clearing hooks trigger by the skin module during paint
* for elements that clearbackground=1
*
**************************************************************************/
WindowsBackgroundClearer::WindowsBackgroundClearer() :
	m_window(NULL), 
	m_theme(NULL),
	m_composition(FALSE),
	m_clear_background(FALSE),
	m_use_transparent_state(FALSE),
	m_menubar_visibility(FALSE),
	m_pagebar_on_top(TRUE),
	m_border_buttons(NULL),
	m_hwBmpDC(NULL),
	m_hwBmp(NULL),
	m_hwOldBmp(NULL),
	m_hwBmpData(NULL),
	m_hwOpBitmap(NULL),
	m_has_painted_personas(FALSE)
{} 

WindowsBackgroundClearer::WindowsBackgroundClearer(OpWindow* win, BOOL use_composition, BOOL use_transparent_state, BOOL menubar_visibility, BOOL pagebar_on_top) : 
	m_window(static_cast<WindowsOpWindow *>(win)), 
	m_theme(NULL),
	m_composition(use_composition),
	m_use_transparent_state(use_transparent_state),
	m_menubar_visibility(menubar_visibility),
	m_pagebar_on_top(pagebar_on_top),
	m_border_buttons(NULL),
	m_hwBmpDC(NULL),
	m_hwBmp(NULL),
	m_hwOldBmp(NULL),
	m_hwBmpData(NULL),
	m_hwOpBitmap(NULL),
	m_has_painted_personas(FALSE)
{
	METHOD_CALL;

	m_clear_background = !m_composition;

	m_theme = OpenThemeData(m_window->m_hwnd, UNI_L("window"));
	if (HASDwmSetWindowAttribute())
	{
		// we don't need to draw into the frame if the pagebar is not the top most element
		if(pagebar_on_top && !m_menubar_visibility)
		{
			DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
			HRESULT hr = OPDwmSetWindowAttribute(m_window->m_hwnd, DWMWA_NCRENDERING_POLICY, (void*)&policy, sizeof(DWMNCRENDERINGPOLICY));
			OP_ASSERT(SUCCEEDED(hr) && "Something failed while changing non-client rendering");
			hr = hr;	// silence warning
		}
	}
	if(m_window->GetDesktopWindow())
	{
		if(OpStatus::IsSuccess(OpBorderButtons::Construct(&m_border_buttons, m_window, m_theme, pagebar_on_top)))
		{
			m_border_buttons->Layout();
		}
	}
}
WindowsBackgroundClearer::~WindowsBackgroundClearer()
{
	METHOD_CALL;

	if (m_hwBmp)
	{
		if (m_hwBmpDC)
		{
			SelectObject(m_hwBmpDC, m_hwOldBmp);
			DeleteDC(m_hwBmpDC);
		}
		DeleteObject(m_hwBmp);
	}
	OP_DELETE(m_hwOpBitmap);

	if(m_border_buttons)
	{
		m_border_buttons->DeleteButtons();
	}
	if(m_theme)
	{
		CloseThemeData(m_theme);
	}
	if (HASDwmSetWindowAttribute() && m_window)
	{
		DWMNCRENDERINGPOLICY policy = DWMNCRP_USEWINDOWSTYLE;
		HRESULT hr = OPDwmSetWindowAttribute(m_window->m_hwnd, DWMWA_NCRENDERING_POLICY, (void*)&policy, sizeof(DWMNCRENDERINGPOLICY));
		OP_ASSERT(SUCCEEDED(hr) && "Something failed while changing non-client rendering");
		hr = hr; // get rid of warning
	}
	CLEARER_TRACE(UNI_L("WindowsBackgroundClearer deleted: 0x%08x\n"), this);
}

void WindowsBackgroundClearer::OnPersonaChanged(OpPersona *persona)
{
}

VEGAPixelStore* WindowsBackgroundClearer::SetupBackBuffers(HDC& dc, UINT32 winw, UINT32 winh)
{
	VEGAPixelStore *ps = NULL;
	if (!dc)
	{
		// Hardware rendering path
		if (!m_hwOpBitmap || m_hwOpBitmap->Width() != winw || m_hwOpBitmap->Height() != winh)
		{
			if (m_hwBmp)
			{
				if (m_hwBmpDC)
				{
					SelectObject(m_hwBmpDC, m_hwOldBmp);
					DeleteDC(m_hwBmpDC);
				}
				DeleteObject(m_hwBmp);
				m_hwBmp = NULL;
				m_hwBmpDC = NULL;
			}
			OP_DELETE(m_hwOpBitmap);
			m_hwOpBitmap = NULL;

			BITMAPINFO bi;
			bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bi.bmiHeader.biWidth = winw;
			bi.bmiHeader.biHeight = -(int)winh;
			bi.bmiHeader.biPlanes = 1;
			bi.bmiHeader.biBitCount = 32;
			bi.bmiHeader.biCompression = BI_RGB;
			bi.bmiHeader.biSizeImage = 0;
			bi.bmiHeader.biXPelsPerMeter = 0;
			bi.bmiHeader.biYPelsPerMeter = 0;
			bi.bmiHeader.biClrUsed = 0;
			bi.bmiHeader.biClrImportant = 0;

			m_hwBmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&m_hwBmpData, NULL, 0);
			if (!m_hwBmp)
				return NULL;
			HDC hdc = GetDC(reinterpret_cast<HWND>(m_window->m_vega_window->getNativeHandle()));
			m_hwBmpDC = CreateCompatibleDC(hdc);
			m_hwOldBmp = (HBITMAP)SelectObject(m_hwBmpDC, m_hwBmp);
			ReleaseDC(reinterpret_cast<HWND>(m_window->m_vega_window->getNativeHandle()), hdc);
			if (!m_hwBmpDC || OpStatus::IsError(OpBitmap::Create(&m_hwOpBitmap, winw, winh)))
				return NULL;
		}
		ps = &m_hwPs;
		m_hwPs.format = VPSF_BGRA8888;
		m_hwPs.height = winh;
		m_hwPs.width = winw;
		m_hwPs.stride = m_hwPs.width*4;
		m_hwPs.buffer = m_hwBmpData;
		dc = m_hwBmpDC;
	}
	else
	{
		// software rendering path
		if (m_hwBmp)
		{
			if (m_hwBmpDC)
			{
				SelectObject(m_hwBmpDC, m_hwOldBmp);
				DeleteDC(m_hwBmpDC);
			}
			DeleteObject(m_hwBmp);
			m_hwBmp = NULL;
			m_hwBmpDC = NULL;
		}
		OP_DELETE(m_hwOpBitmap);
		m_hwOpBitmap = NULL;
		ps = ((WindowsOpWindow*)m_window)->m_vega_window->getPixelStore();
	}
	return ps;
}

void WindowsBackgroundClearer::OnBackgroundCleared(OpPainter *op_painter, const OpRect& clear_rect)
{
	METHOD_CALL;

	OP_ASSERT(op_painter && "If this happens, your paint call of a skin element is oh so wrong");
	if(!op_painter)
		return;

	// op_painter can be either a top level window painter or an off screen painter to draw menubar
	bool is_window_painter = m_window->m_mde_screen->GetVegaPainter() == static_cast<VEGAOpPainter*>(op_painter);

	RECT frame_sizes = m_window->GetFrame()->GetNativeFrameSizes();
#ifdef PERSONA_SKIN_SUPPORT
	BOOL has_persona_skin = g_skin_manager->HasPersonaSkin();
#else
	BOOL has_persona_skin = FALSE;
#endif // PERSONA_SKIN_SUPPORT

	HDC dc = m_window->GetRenderTargetDC();
	OpRect r(clear_rect);

	// clear_rect is client coordinates, which means the y coordinate doesn't start
	// from 0 for window painter if there is a menu bar, but window painter expects
	// coordinates starting from 0. This is only relevant for hardware backend though,
	// for software we don't use painter.
	if (is_window_painter && !dc)
		r.y -= m_window->m_top_margin;
	else if (!is_window_painter && dc)
		return; // some extra work has to be done to clear menu bar with software backend, for now just leave it grey

	OP_ASSERT(r.y >= 0);

	RECT rect;
	UINT32 winw, winh;
	winw = m_window->m_vega_window->getWidth();
	winh = m_window->m_vega_window->getHeight();

	int vertical_border = frame_sizes.left;
	rect.left = -vertical_border;
	rect.right = winw+vertical_border;
	rect.top = 0;
	rect.bottom = winh;

	VEGAOpPainter* painter = static_cast<VEGAOpPainter*>(op_painter);
	VEGARenderer* rend = painter->GetRenderer();
	VEGARenderTarget* rt = painter->GetRenderTarget();
	VEGAPixelStore* ps = SetupBackBuffers(dc, winw, winh);
	if(!ps)
		return;

	VegaBufferClipRect clip(dc, m_window, r, ps);

	BOOL active = m_window->m_hwnd_active;
	// always clear menu bar, but only clear window when personas is off
	if ((!is_window_painter || !has_persona_skin) && m_clear_background)
	{
		BOOL drawTitlebar = FALSE;
		UINT32 color = 0;
		int ry = r.y;
		int rh = r.height;
		if (m_window->m_titlebar_height)
		{
			rect.top = frame_sizes.top + GetSystemMetrics(SM_CYCAPTION);
			if (r.y < rect.top)
			{
				if (!IsSystemWinVista())
				{
					RECT cr;
					cr.left = 1+r.x-vertical_border;
					cr.right = r.x+1;
					cr.top = r.y;
					cr.bottom = r.y+1;
					DrawThemeBackground(m_theme, dc, WP_FRAMELEFT, active?FS_ACTIVE:FS_INACTIVE, &cr, NULL);

					color = ((UINT32*)ps->buffer)[r.x+r.y*ps->stride/4] | (255<<24);
				}

				rh -= rect.top-r.y;
				ry = rect.top;
				RECT caprect;
				caprect.top = 0;
				caprect.left = -vertical_border;
				caprect.bottom = rect.top;
				caprect.right = winw+vertical_border;
				DrawThemeBackground(m_theme, dc, WP_CAPTION, active?CS_ACTIVE:CS_INACTIVE, &caprect, NULL);

				if (!IsSystemWinVista() && !m_hwOpBitmap)
				{
					rend->setRenderTarget(rt);
					rend->setClipRect(r.x, r.y, r.width, r.height);
					drawGradient(rend, color, winw, rect.top);
				}

				// buttons
				m_border_buttons->Paint(dc);

				drawTitlebar = TRUE;
			}
		}
		rect.left = 1+r.x-vertical_border;
		rect.right = r.x+1;
		DrawThemeBackground(m_theme, dc, WP_FRAMELEFT, active?FS_ACTIVE:FS_INACTIVE, &rect, NULL);
		for (int yp = 0; yp < rh; ++yp)
		{
			UINT32* src = ((UINT32*)ps->buffer)+(ry+yp)*ps->stride/4+r.x;
			*src |= 255<<24;
			UINT32* dst = src+1;
			for (int xp = 1; xp < r.width; ++xp)
				*(dst++) = *src;
		}

		if (m_hwOpBitmap)
		{
			// Update the OpBitmap used for hardware rendering
			UINT32* obd = (UINT32*)m_hwOpBitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);
			if (drawTitlebar)
			{
				for (int yp = 0; yp < rect.top; ++yp)
				{
					UINT32* src = ((UINT32*)ps->buffer)+yp*ps->stride/4;
					UINT32* dst = obd+yp*m_hwOpBitmap->GetBytesPerLine()/4;
					for (UINT32 xp = 0; xp < winw; ++xp)
						*(dst++) = *(src++) | (255<<24);
				}
			}
			for (int yp = 0; yp < rh; ++yp)
				op_memcpy(obd+(ry+yp)*m_hwOpBitmap->GetBytesPerLine()/4+r.x, ((UINT8*)ps->buffer)+ps->stride*(ry+yp)+r.x*4, r.width*4);
			m_hwOpBitmap->ReleasePointer();

			if (drawTitlebar)
			{
				painter->DrawBitmapClipped(m_hwOpBitmap, OpRect(0, 0, winw, rect.top), OpPoint(0,0));
				if (!IsSystemWinVista())
				{
					rend->setRenderTarget(rt);
					rend->setClipRect(r.x, r.y, r.width, r.height);
					drawGradient(rend, color, winw, rect.top);
				}
			}
			if (rh > 0 && r.width > 0)
			{
				painter->DrawBitmapClipped(m_hwOpBitmap, OpRect(r.x, ry, r.width, rh), OpPoint(r.x, ry));
			}
		}
	}

#ifdef PERSONA_SKIN_SUPPORT
	if(has_persona_skin && is_window_painter)
	{
		VisualDevice vd;

		vd.BeginPaint(painter, clear_rect, clear_rect);

		WindowsBackgroundClearer::PaintBackground(m_window->m_vega_window->getWidth(), m_window->m_vega_window->getHeight(), m_window, painter, &vd, clear_rect, ::GetFocus() != m_window->m_hwnd);
		m_has_painted_personas = TRUE;

		vd.EndPaint();
#endif // PERSONA_SKIN_SUPPORT
	}
	if((m_clear_background || has_persona_skin) && m_border_buttons)
	{
		m_border_buttons->Layout();
		m_border_buttons->Paint(dc);
	}
}
/* static */
void WindowsBackgroundClearer::PaintBackground(unsigned int width, unsigned int height, WindowsOpWindow *window, VEGAOpPainter *painter, VisualDevice *vd, const OpRect& paint_rect, BOOL inactive)
{
	METHOD_CALL;

#ifdef PERSONA_SKIN_SUPPORT
	OpSkinElement *elm = g_skin_manager->GetPersona()->GetBackgroundImageSkinElement();
#else
	OpSkinElement *elm = NULL;
#endif // PERSONA_SKIN_SUPPORT
	if(!elm)
		return;

	OpRect rect(0, 0, width, height);

	vd->Translate(-painter->GetTranslationX(), -painter->GetTranslationY());

	OpSkinElement::DrawArguments args;

	// set a clip rect, needed when painting the background view using this method
	if(!paint_rect.IsEmpty())
	{
		vd->BeginClipping(paint_rect);
		args.clip_rect = &paint_rect;
	}
	elm->Draw(vd, rect, 0, args);

	// top border line
	// white with 30% opacity
	if(window->GetFrame() && window->GetFrame()->UseCustomUIFrame())
	{
		int color;
		TRAPD(status, color = g_skin_manager->GetCurrentSkin()->ReadColorL("Options", "Window Border Color", OP_RGBA(255, 255, 255, 76), FALSE));
		if (OpStatus::IsSuccess(status))
			vd->SetColor32(color);

		vd->DrawLine(OpPoint(5, 0), OpPoint(width - 5, 0));
	}

	// paint the overlay, if needed
	if(inactive)
	{
		UINT32 color = g_desktop_op_ui_info->GetPersonaDimColor();
		vd->SetColor32(color);
		vd->FillRect(rect);
	}

	if(!paint_rect.IsEmpty())
	{
		vd->EndClipping();
	}
	vd->Translate(-vd->GetTranslationX(), -vd->GetTranslationY());
}

void WindowsBackgroundClearer::GetBorderButtonsSize(INT32 *w, INT32 *h)
{
	*w = *h = 0;

	if(m_border_buttons)
	{
		m_border_buttons->GetSize(w, h);
	}
	else
	{
		// just fallback to the system calculations
		RECT btnrect[WIN_RECT_MAX];
		OpBorderButtons::CalculateCaptionButtonRects(m_window, btnrect);

		for(int i = 0; i < WIN_RECT_MAX; i++)
		{
			*w += btnrect[i].right - btnrect[i].left;
		}
		*h = btnrect[WIN_RECT_CLOSE_BUTTON].bottom - btnrect[WIN_RECT_CLOSE_BUTTON].top;
	}
}

void WindowsBackgroundClearer::drawGradient(VEGARenderer* rend, UINT32 color, unsigned int width, unsigned int height)
{
	int num_stops = 2;
	VEGA_FIX stops[2] = {0.2f, 0.8f};
	UINT32 cols[2] = {color&0xffffff, color};
	VEGAFill* grad;
	int caption_height = GetSystemMetrics(SM_CYCAPTION);
	int frame_width = GetSystemMetrics(SM_CYSIZEFRAME);

	RECT btnrect[WIN_RECT_MAX];
	NativeBorderButtons::CalculateCaptionButtonRects(m_window, btnrect);

	// width of rect to apply radial gradient
	int radial_width = caption_height;
	// x coordinate of radial gradient
	int radial_x = btnrect[WIN_RECT_MINIMIZE_BUTTON].left - radial_width + caption_height/3;

	// center
	if (OpStatus::IsSuccess(rend->createLinearGradient(&grad, 0, frame_width, 0, frame_width + caption_height, num_stops, stops, cols)))
	{
		rend->setFill(grad);
		rend->fillRect(0, 0, radial_x, height);
		rend->setFill(NULL);
		OP_DELETE(grad);
	}

	// Right
	UINT32 rad_cols[2] = {color, color&0xffffff};
	if (OpStatus::IsSuccess(rend->createRadialGradient(&grad,
		VEGA_INTTOFIX(radial_x), VEGA_INTTOFIX(caption_height + frame_width), 0,
		VEGA_INTTOFIX(radial_x), VEGA_INTTOFIX(caption_height + frame_width), VEGA_INTTOFIX(radial_width),
		num_stops, stops, rad_cols)))
	{
		rend->setFill(grad);
		rend->fillRect(radial_x, 0, radial_width, height);
		rend->setFill(NULL);
		OP_DELETE(grad);
	}
}

/******************************************************************************************
*
* Implementation of WindowsBackgroundClearer that works for classic ui (non-themed) looks 
*
*******************************************************************************************/
WindowsClassicBackgroundClearer::WindowsClassicBackgroundClearer(OpWindow* win, BOOL use_composition, BOOL use_transparent_state, BOOL menubar_visibility, BOOL pagebar_on_top) :
									WindowsBackgroundClearer(win, use_composition, use_transparent_state, menubar_visibility, pagebar_on_top)
{
}

void WindowsClassicBackgroundClearer::OnBackgroundCleared(OpPainter *op_painter, const OpRect& clear_rect)
{
	OP_ASSERT(op_painter && "If this happens, your paint call of a skin element is oh so wrong");
	if(!op_painter)
		return;

#ifdef PERSONA_SKIN_SUPPORT
	BOOL has_persona_skin = g_skin_manager->HasPersonaSkin();
#else
	BOOL has_persona_skin = FALSE;
#endif // PERSONA_SKIN_SUPPORT

	OpRect r(clear_rect);

	if(has_persona_skin)
	{
#ifdef PERSONA_SKIN_SUPPORT
		// ensure we're called with the top level painter
		if (m_window->m_mde_screen->GetVegaPainter() == op_painter)
		{
			VisualDevice vd;

			vd.BeginPaint(op_painter, r, r);

			WindowsBackgroundClearer::PaintBackground(m_window->m_vega_window->getWidth(), m_window->m_vega_window->getHeight(), m_window, static_cast<VEGAOpPainter*>(op_painter), &vd, clear_rect, ::GetFocus() != m_window->m_hwnd);
			m_has_painted_personas = TRUE;

			vd.EndPaint();
		}
#endif // PERSONA_SKIN_SUPPORT
	}
	else
	{
		if (m_window->m_mde_screen->GetVegaPainter() == op_painter)
		{
			// The window

			// clear_rect is client coordinates, which means the y coordinate doesn't start
			// from 0 if there is a menu bar, but when painting to the painter we need view 
			// coordinates, transform here.
			// Maybe we should fix the caller instead
			r.y -= m_window->m_top_margin;
			OP_ASSERT(r.y >= 0);
		}
		else
		{
			// Some off screen bitmap(menubar for example)
		}

		UINT32 color = g_skin_manager->GetCurrentSkin()->GetDefaultColorScheme();
		op_painter->SetColor(OP_GET_R_VALUE(color), OP_GET_G_VALUE(color), OP_GET_B_VALUE(color), OP_GET_A_VALUE(color));
		op_painter->FillRect(r);
	}
}

/* static */
WindowsBackgroundClearer* WindowsBackgroundClearer::Create(WindowsOpWindow* win, BOOL use_transparent_state, BOOL use_composition, BOOL pagebar_on_top)
{
	WinClearerType create_hook_type = WIN_CLEARER_TYPE_NONE;
	WindowsBackgroundClearer* clearer = win->m_background_clearer;
	use_transparent_state |= g_skin_manager->HasPersonaSkin();

	// The only case background clearer is not needed is when aero is enabled and no personas
	if (g_skin_manager->HasPersonaSkin() || !use_composition)
	{
		// When there is no custom title bar WIN_CLEARER_TYPE_CLASSIC clears background
		// very quickly with a simple FillRect operation using the color
		// g_skin_manager->GetCurrentSkin()->GetDefaultColorScheme, it works fine for
		// classic Windows theme and XP, however for Vista/7 we want to use same color
		// as the native title bar and border.
		if (!win->IsThemed() || GetWinType() == WINXP && (win->m_menubar_visibility || !pagebar_on_top))
			create_hook_type = WIN_CLEARER_TYPE_CLASSIC;
		else
			create_hook_type = WIN_CLEARER_TYPE_NORMAL;
	}

	// let's check if we need to recreate it
	if(!clearer || (clearer && (clearer->GetClearerType() != create_hook_type || 
			clearer->HasComposition() != use_composition || 
			clearer->GetMenubarVisibility() != win->m_menubar_visibility ||
			clearer->IsPagebarOnTop() != pagebar_on_top ||
			clearer->HasTheme() != win->IsThemed() ||
			clearer->HasPersonas() != g_skin_manager->HasPersonaSkin())))
	{
		if(clearer)
		{
			CLEARER_TRACE(UNI_L("WindowsBackgroundClearer deleted: 0x%08x\n"), win->m_background_clearer);
			g_skin_manager->RemoveTransparentBackgroundListener(win->m_background_clearer);
			OP_DELETE(win->m_background_clearer);
			win->m_background_clearer = NULL;
			clearer = NULL;
		}
		switch(create_hook_type)
		{
		case WIN_CLEARER_TYPE_NORMAL:
			{
				clearer = OP_NEW(WindowsBackgroundClearer, (win, use_composition, use_transparent_state, win->m_menubar_visibility, pagebar_on_top));
				CLEARER_TRACE(UNI_L("WindowsBackgroundClearer created: 0x%08x\n"), clearer);
				break;
			}
		case WIN_CLEARER_TYPE_CLASSIC:
			{
				clearer = OP_NEW(WindowsClassicBackgroundClearer, (win, use_composition, use_transparent_state, win->m_menubar_visibility, pagebar_on_top));
				CLEARER_TRACE(UNI_L("WindowsClassicBackgroundClearer created: 0x%08x\n"), clearer);
				break;
			}
		case WIN_CLEARER_TYPE_DUMMY:
			{
				clearer = OP_NEW(DummyBackgroundClearer, (win, use_composition, use_transparent_state, win->m_menubar_visibility, pagebar_on_top));
				break;
			}
		}
	}
	// enable transparent skins if we're using a persona
	if (win->GetDesktopWindow())
	{
		win->GetDesktopWindow()->EnableTransparentSkins(use_transparent_state, FALSE);
	}
	if(clearer && clearer != win->m_background_clearer)
	{
		g_skin_manager->AddTransparentBackgroundListener(clearer);
	}
	return clearer;
}
