/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/pi/WindowsOpWindow.h"

#include "platforms/windows/pi/WindowsOpDragManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"

#include "modules/display/vis_dev.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/skin/opskinmanager.h"
#include "modules/skin/OpSkin.h"
#include "modules/widgets/WidgetContainer.h"

#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/user.h"
#include "platforms/windows/win_handy.h"
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "platforms/windows/pi/WindowsOpAccessibilityAdapter.h"
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
#ifdef MANUAL_PLUGIN_ACTIVATION
# include "platforms/windows/pi/WindowsOpPluginWindow.h"
#endif // MANUAL_PLUGIN_ACTIVATION
#include "platforms/windows/pi/WindowsOpMessageLoop.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/pi/WindowsOpView.h"
#include "platforms/windows/WindowsLaunch.h"
#include "platforms/windows/installer/HandleInfo.h"
#include "platforms/windows/installer/ProcessItem.h"
#include "platforms/windows/windows_ui/docwin_mousewheel.h"
#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows/windows_ui/menubar.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "platforms/windows/utils/win_icon.h"
#include "adjunct/desktop_util/widget_utils/LogoThumbnailClipper.h"
#include "platforms/windows/pi/util/WindowsBackgroundClearer.h"
#include "platforms/windows/pi/util/WindowFrame.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "modules/url/url_api.h"
#include "modules/logdoc/urlimgcontprov.h"

#include <VSStyle.h>
#include <Windowsx.h>

static BOOL only_top_most;

#define MAX_WINDOW_BORDER_TOP			8
#define WM_APPCOMMAND					  0x0319

#if !defined (WM_XBUTTONDOWN)
// Imported these from vc7 winuser.h:
#define WM_NCXBUTTONDBLCLK              173
#define WM_NCXBUTTONDOWN                0x00AB
#define WM_NCXBUTTONUP                  0x00AC
#define WM_XBUTTONDBLCLK                525
#define WM_XBUTTONDOWN                  0x020B
#define WM_XBUTTONUP                    0x020C
/* XButton values are WORD flags */
#define GET_XBUTTON_WPARAM(wParam)      (HIWORD(wParam))
#define XBUTTON1      0x0001
#define XBUTTON2      0x0002
#endif

#if !defined(WM_DWMCOMPOSITIONCHANGED)
#define WM_DWMCOMPOSITIONCHANGED 0x031E
#endif
#if !defined(WM_DWMNCRENDERINGCHANGED)
#define WM_DWMNCRENDERINGCHANGED        0x031F
#endif
#define WM_THEMECHANGED                 0x031A

#if !defined(WS_EX_NOACTIVATE)
#define WS_EX_NOACTIVATE        0x08000000L
#endif

#define WM_NCUAHDRAWCAPTION 0xAE
#define WM_NCUAHDRAWFRAME	0xAF

#ifndef PBT_POWERSETTINGCHANGE
#define PBT_POWERSETTINGCHANGE          0x8013
typedef struct {
	GUID PowerSetting;
	DWORD DataLength;
	UCHAR Data[1];
} POWERBROADCAST_SETTING, *PPOWERBROADCAST_SETTING;
#endif // PBT_POWERSETTINGCHANGE

extern int IntelliMouse_GetLinesToScroll(HWND hwnd, short zDelta, BOOL* scroll_as_page);
extern HKL current_input_language;
extern OpInputMethodString imstring;
extern IM_WIDGETINFO widgetinfo;
extern BOOL compose_started;
extern BOOL compose_started_by_message;
extern BOOL result_was_commited;
extern OpString g_spawnPath;
INT32 UpdateIMString(HIMC hIMC, UINT32 flag);

BOOL MonitorRectFromWindow(RECT& hMonitorRect, HWND wnd, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY);
BOOL MonitorRectFromRect(RECT& hMonitorRect, LPCRECT rect, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY);
BOOL MonitorRectFromPoint(RECT& hMonitorRect, POINT point, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY);

#if !defined (WS_EX_LAYERED)
// Imported these from vc8 winuser.h:
#define WS_EX_LAYERED           0x00080000
#endif // !defined (WS_EX_LAYERED)

#if !defined (CS_DROPSHADOW)
#define CS_DROPSHADOW       0x00020000
#endif // !defined (CS_DROPSHADOW)

#ifdef TOUCH_EVENTS_SUPPORT // This is #if WINVER >= 0x0601 in Windows header files so we don't get it
#if(WINVER < 0x0601)
#define WM_GESTURE                      0x0119
#define WM_GESTURENOTIFY                0x011A
#endif // (WINVER < 0x0601)
#endif // TOUCH_EVENTS_SUPPORT

// These are defined in user.cpp
extern HCURSOR hWaitCursor, hArrowCursor, hArrowWaitCursor, hLinkCursor, hSplitterCursor, hVSplitterCursor;

extern HINSTANCE hInst;
extern int DefShowWindow;
extern UINT	idMsgLostCursor;

// Used by GdiOpFont to draw theme text
HTHEME g_window_theme_handle;

extern WindowsOpDragManager* g_windows_drag_manager;
extern struct WindowsDragTemporaryData* g_drag_data;

bool										WindowsOpWindow::s_touch_gesture_active = false;
BOOL										WindowsOpWindow::s_attention_inverted = FALSE;
OpTimer*									WindowsOpWindow::s_attention_timer = NULL;
Head										WindowsOpWindow::s_attention_list;
WindowsOpWindow::AttentionTimerListener		WindowsOpWindow::s_attention_timer_listener;
WindowsOpWindow*							WindowsOpWindow::s_popup_window = NULL;
WindowsOpWindow*							WindowsOpWindow::s_current_popup_window = NULL;
UINT32										WindowsOpWindow::s_expecting_dialog_close = 0;
DWORD										WindowsOpWindow::s_atom = 0;
DWORD										WindowsOpWindow::s_atom_ds = 0;
HICON										WindowsOpWindow::s_custom_icon = NULL;
HICON										WindowsOpWindow::s_custom_icon_small = NULL;
HICON										WindowsOpWindow::s_winstaller_icon = NULL;
HICON										WindowsOpWindow::s_winstaller_icon_small = NULL;
OpWindowsPointerHashTable<const HWND, WindowsOpWindow *>	WindowsOpWindow::s_adopted_windows;
WindowsOpWindow::WmMessageFunction			WindowsOpWindow::wm_messages[WM_USER];

#define MAX_RECTS 16

enum WINDOWTHEMEATTRIBUTETYPE
{
    WTA_NONCLIENT = 1
};
typedef struct _WTA_OPTIONS
{
    DWORD dwFlags;          // values for each style option specified in the bitmask
    DWORD dwMask;           // bitmask for flags that are changing
                            // valid options are: WTNCA_NODRAWCAPTION, WTNCA_NODRAWICON, WTNCA_NOSYSMENU
} WTA_OPTIONS, *PWTA_OPTIONS;

#define WTNCA_NODRAWCAPTION       0x00000001    // don't draw the window caption
#define WTNCA_NODRAWICON          0x00000002    // don't draw the system icon
#define WTNCA_NOSYSMENU           0x00000004    // don't expose the system menu icon functionality
#define WTNCA_NOMIRRORHELP        0x00000008    // don't mirror the question mark, even in RTL layout
#define WTNCA_VALIDBITS           (WTNCA_NODRAWCAPTION | \
                                   WTNCA_NODRAWICON | \
                                   WTNCA_NOSYSMENU | \
                                   WTNCA_NOMIRRORHELP)

#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
long FAR PASCAL Windows7TaskbarWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
#include "platforms/windows/pi/WindowsOpTaskbar.h"

#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS



WindowsVegaWindow::WindowsVegaWindow() :
			  m_width(0)
			, m_height(0)
			, m_hwnd(NULL)
			, m_bitmap(NULL)
			, m_hBitmapDC(NULL)
			, m_hOldBitmap(NULL)
			, m_bufferInitialized(FALSE)
			, m_hwWin(NULL)
			, m_has_valid_getdc(-1)
			, m_curFont(NULL)
			, m_oldFont(NULL)
			, m_curFontColor(0)
			, m_transparent_margin_top(0)
			, m_transparent_margin_bottom(0)
			, m_transparent_margin_left(0)
			, m_transparent_margin_right(0)
			, m_extended_client_area_top(0)
			, m_extended_client_area_bottom(0)
			, m_extended_client_area_left(0)
			, m_extended_client_area_right(0)
			, m_is_layered_window(FALSE)
			, m_layered_window_transparency(255)
			, m_full_transparency(FALSE)
			, m_transparency_changed(FALSE)
			, m_system_menu_on_wanted(FALSE)
			, m_system_menu_on(FALSE)
			, m_screen(NULL)
{
	op_memset(&m_pixelstore, 0, sizeof(VEGAPixelStore));
}
WindowsVegaWindow::~WindowsVegaWindow()
{
	if (m_hBitmapDC)
	{
		if (m_oldFont)
			SelectObject(m_hBitmapDC, m_oldFont);
		SelectObject(m_hBitmapDC, m_hOldBitmap);
		DeleteDC(m_hBitmapDC);
	}
	if (m_bitmap)
		DeleteObject(m_bitmap);
}

OP_STATUS WindowsVegaWindow::Construct(unsigned int w, unsigned int h, HWND win, VegaMDEScreen *screen)
{
	m_screen = screen;
	m_hwnd = win;

	return OpStatus::OK;
}

void WindowsVegaWindow::setTransparentMargins(int top, int bottom, int left, int right)
{
	if (m_transparent_margin_top != top
		|| m_transparent_margin_bottom != bottom
		|| m_transparent_margin_left != left
		|| m_transparent_margin_right != right)
	{
		m_transparency_changed = TRUE;
		m_transparent_margin_top = top;
		m_transparent_margin_bottom = bottom;
		m_transparent_margin_left = left;
		m_transparent_margin_right = right;
	}
}

void WindowsVegaWindow::getTransparentMargins(int& top, int& bottom, int& left, int& right)
{
	if (m_full_transparency)
	{
		top = m_height;
		bottom = 0;
		left = 0;
		right = 0;
	}
	else
	{
		top = m_transparent_margin_top;
		bottom = m_transparent_margin_bottom;
		left = m_transparent_margin_left;
		right = m_transparent_margin_right;
	}
}


void PPTBlt (HWND hWnd, HDC dst, HDC src, BYTE transparency = 0xff);

void WindowsVegaWindow::TestLayeredWindow()
{
	if (m_is_layered_window && m_hwnd && m_hwWin && m_has_valid_getdc == -1)
	{
		m_has_valid_getdc = FALSE;
		UINT color = 0xaabbcc;
		VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
		dev->setRenderTarget(m_hwWin, true);
		dev->setRenderState(dev->getDefault2dNoBlendNoScissorRenderState());
		dev->clear(true, false, false, color, 1.f, 0);
		HDC hdc = (HDC)m_hwWin->getBackbufferHandle();
		if (hdc)
		{
			if(GetPixel(hdc,0,0) == 0xccbbaa)
			{
				m_has_valid_getdc = TRUE;
			}
			m_hwWin->releaseBackbufferHandle(hdc);
		}

		if (!m_has_valid_getdc)
		{
			wm_size(m_width, m_height); // create pixel store
		}
	}
}

void WindowsVegaWindow::ExtendFrameInfoClientArea()
{
	if(HASDwmExtendFrameIntoClientArea())
	{
		if(m_full_transparency)
		{
			if(m_transparency_changed)
			{
				// special value -1 means the whole window
				MARGINS marg = {-1, -1, -1, -1 };

				OPDwmExtendFrameIntoClientArea(m_hwnd, &marg);

				m_transparency_changed = FALSE;
			}
		}
		else
		{
			if (m_transparency_changed || (m_extended_client_area_top != m_transparent_margin_top ||
				m_extended_client_area_bottom != m_transparent_margin_bottom || 
				m_extended_client_area_left != m_transparent_margin_left || 
				m_extended_client_area_right != m_transparent_margin_right))
			{
				m_extended_client_area_top = m_transparent_margin_top;
				m_extended_client_area_bottom = m_transparent_margin_bottom;
				m_extended_client_area_left = m_transparent_margin_left;
				m_extended_client_area_right = m_transparent_margin_right;

				INT32 inset_by = g_skin_manager->GetCurrentSkin()->GetOptionValue("Window Border Inset", 0);
				if(inset_by < 0)
					inset_by = 0;
				else if(inset_by > 4)
					inset_by = 4;

				MARGINS marg = {m_extended_client_area_left + inset_by, 
					m_extended_client_area_right + inset_by, 
					m_extended_client_area_top + inset_by, 
					m_extended_client_area_bottom + inset_by};

				OPDwmExtendFrameIntoClientArea(m_hwnd, &marg);

				m_transparency_changed = FALSE;
			}
		}
	}
}

void WindowsVegaWindow::present(const OpRect* update_rects, unsigned int num_rects)
{
	if (m_hwnd)
	{
		if (m_hBitmapDC)
		{
			HDC hdc = GetDC(m_hwnd);

			if (m_is_layered_window)
			{
				if(m_hwWin)
					m_hwWin->readBackbuffer(&m_pixelstore);

				PPTBlt(m_hwnd, hdc, m_hBitmapDC, m_layered_window_transparency);
			}
			else
			for (unsigned int i=0; i < num_rects; ++i)
			{
				BitBlt(hdc, update_rects[i].x, update_rects[i].y, update_rects[i].width, update_rects[i].height, m_hBitmapDC, update_rects[i].x, update_rects[i].y, SRCCOPY);
			}
	
			ReleaseDC(m_hwnd, hdc);
		}
		else
		if (m_hwWin)
		{
			if(m_is_layered_window)
			{
				HDC srcHdc = (HDC)m_hwWin->getBackbufferHandle();
				if (srcHdc)
				{
					PPTBlt(m_hwnd, GetDC(m_hwnd), srcHdc, m_layered_window_transparency);
					m_hwWin->releaseBackbufferHandle(srcHdc);
				}
			}
			else
			{
				m_hwWin->present(update_rects, num_rects);
			}
		}
		if (m_system_menu_on_wanted != m_system_menu_on && HASSetWindowThemeAttribute())
		{
			m_system_menu_on = m_system_menu_on_wanted;

			WTA_OPTIONS options;
			options.dwMask = WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON | WTNCA_NOSYSMENU;
			options.dwFlags = options.dwMask;
			if (m_system_menu_on)
				options.dwFlags = 0;

			if (OPSetWindowThemeAttribute(m_hwnd, WTA_NONCLIENT, &options, sizeof(WTA_OPTIONS)) != S_OK)
			{
				// Fail
			}
		}
		ExtendFrameInfoClientArea();
	}
}

bool WindowsVegaWindow::wm_size(unsigned int width, unsigned int height)
{
	if (m_bitmap && (m_width != width || m_height != height))
	{
		if (m_hBitmapDC)
		{
			if (m_oldFont)
				SelectObject(m_hBitmapDC, m_oldFont);
			m_oldFont = NULL;
			m_curFont = NULL;
			m_curFontColor = 0;
			DeleteDC(m_hBitmapDC);
			m_hBitmapDC = NULL;
		}
		DeleteObject(m_bitmap);
		m_bitmap = NULL;
	}
	m_width = width;
	m_height = height;
	bool recreated = false;

	if (m_hwWin)
	{
		if (width != m_hwWin->getWidth() || height != m_hwWin->getHeight())
		{
			m_hwWin->resizeBackbuffer(width, height);
			recreated = true;
		}
	}

	if (!m_hwWin || (m_is_layered_window && m_has_valid_getdc == FALSE))
	{
		if (!m_bitmap && width && height)
		{
			BITMAPINFO bi;
			bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bi.bmiHeader.biWidth = width;
			bi.bmiHeader.biHeight = -(int)height;
			bi.bmiHeader.biPlanes = 1;
			bi.bmiHeader.biBitCount = 32;
			bi.bmiHeader.biCompression = BI_RGB;
			bi.bmiHeader.biSizeImage = 0;
			bi.bmiHeader.biXPelsPerMeter = 0;
			bi.bmiHeader.biYPelsPerMeter = 0;
			bi.bmiHeader.biClrUsed = 0;
			bi.bmiHeader.biClrImportant = 0;

			m_bitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&m_pixelstore.buffer, NULL, 0);
		
			if (m_bitmap)
			{
				HDC hdc = GetDC(m_hwnd);
				m_hBitmapDC = CreateCompatibleDC(hdc);
				m_hOldBitmap = (HBITMAP)SelectObject(m_hBitmapDC, m_bitmap);
				ReleaseDC(m_hwnd, hdc);
				SetBkMode(m_hBitmapDC, TRANSPARENT);
			}
			else
			{
				m_width = 0;
				m_height = 0;
			}
			recreated = true;
		}

		m_pixelstore.width = m_width;
		m_pixelstore.height = m_height;
		m_pixelstore.stride = m_width * 4;
		m_pixelstore.format = VPSF_BGRA8888;
	}

	if (m_screen)
		m_screen->Resize(m_width, m_height, m_width * 4, this);

	return recreated;
}

void WindowsVegaWindow::SetFullTransparency(BOOL full)
{	
	if(m_full_transparency != full)
	{
		m_transparency_changed = TRUE;
		m_full_transparency = full;
	}
}


void WindowsVegaWindow::ChangeDCFont(HFONT fnt, UINT32 color)
{
	if (m_curFont != fnt)
	{
		HFONT tmpfnt = (HFONT)SelectObject(m_hBitmapDC, fnt);
		if(!m_oldFont)
			m_oldFont = tmpfnt;
	}
	if (color != m_curFontColor)
	{
		SetTextColor(m_hBitmapDC, RGB((color>>16)&0xff, (color>>8)&0xff, color&0xff));
		m_curFontColor = color;
	}
}

OP_STATUS WindowsVegaWindow::initSoftwareBackend()
{
	m_bufferInitialized = TRUE;
	return OpStatus::OK;
}

OP_STATUS WindowsVegaWindow::initHardwareBackend(VEGA3dWindow* win)
{
	m_hwWin = win;
	return OpStatus::OK;
}

void* WindowsVegaWindow::getNativeHandle()
{
	return m_hwnd;
}

VegaMDEScreen::VegaMDEScreen(int bufferWidth, int bufferHeight, int bufferStride, MDE_FORMAT bufferFormat, void *bufferData, WindowsOpWindow* w)
	: MDE_GenericScreen(bufferWidth, bufferHeight, bufferStride, bufferFormat, bufferData)
	, m_window(w)
	, m_background_view(NULL)
	, m_timer(this)
{
	m_vegaWin = (WindowsVegaWindow*)bufferData;
}
VegaMDEScreen::~VegaMDEScreen()
{
}

OP_STATUS VegaMDEScreen::Init()
{
	OP_STATUS s = MDE_GenericScreen::Init();
	if(OpStatus::IsSuccess(s) && !m_vegaWin->GetIsLayered())
	{
		m_background_view = OP_NEW(VegaBackgroundMDEView, ());
		RETURN_OOM_IF_NULL(m_background_view);

		AddChild(m_background_view);

		m_background_view->SetZ(MDE_Z_BOTTOM);
	}
	return s;
}

void VegaMDEScreen::OnValidate()
{
	m_last_validate = g_op_time_info->GetRuntimeMS();

	// Advance animations here. We are going to paint anyway.
	if (g_animation_manager)
		g_animation_manager->advanceAnimations();
}
void VegaMDEScreen::OnInvalid()
{
	if (!m_window->m_pending_mde_render)
	{
		m_window->m_pending_mde_render = TRUE;

		int throttling_interval = g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::PaintThrottling);
		if (throttling_interval == 0)
		{
			::PostMessage(m_window->m_hwnd, WM_OPERA_MDE_RENDER, 0, 0);
			return;
		}

		// We delay the update with 15ms to not update the screen too often.
		double delay = throttling_interval - (g_op_time_info->GetRuntimeMS() - m_last_validate);
		if (delay > INCREASED_TIMER_RESOLUTION)
		{
			if (OpStatus::IsError(m_timer.Start(MIN(delay, throttling_interval))))
				::PostMessage(m_window->m_hwnd, WM_OPERA_MDE_RENDER, 0, 0);
		}
		else
			::PostMessage(m_window->m_hwnd, WM_OPERA_MDE_RENDER, 0, 0);
	}
}

OP_STATUS VegaMDEScreen::Resize(int bufferWidth, int bufferHeight, int bufferStride, void *bufferData)
{
	OP_STATUS s = MDE_GenericScreen::Resize(bufferWidth, bufferHeight, bufferStride, bufferData);
	if(OpStatus::IsSuccess(s) && bufferWidth && bufferHeight)
	{
		MDE_RECT mr = { 0, 0, bufferWidth, bufferHeight };

		// if we use a custom frame, we need to shrink the inner view to allow for the background
		// view to be painted
		if(m_window->GetFrame())
		{
			RECT inset = m_window->GetFrame()->GetUIFrameSizes();

			mr.x = inset.left;
			mr.w = bufferWidth - inset.left - inset.right;
			mr.h = bufferHeight - inset.top - inset.bottom;
		}
		GetOperaView()->SetRect(mr);

		mr.x = 0;
		mr.y = 0;
		mr.w = bufferWidth;
		mr.h = bufferHeight;

		if (m_background_view)
			m_background_view->SetRect(mr);
	}
	return s;
}

void VegaBackgroundMDEView::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	VEGAOpPainter* painter = m_screen->GetVegaPainter();
	if(!painter)
		return;

	WindowsOpWindow *window = static_cast<VegaMDEScreen *>(m_screen)->m_window;
	BOOL has_persona = g_skin_manager->HasPersonaSkin();
	OpRect draw_rect(rect.x, rect.y, rect.w, rect.h);
	VisualDevice vd;

	vd.BeginPaint(painter, draw_rect, draw_rect);

	if(has_persona)
	{
		WindowsBackgroundClearer::PaintBackground(m_screen->GetWidth(), m_screen->GetHeight(), window, painter, &vd, draw_rect, ::GetFocus() != window->m_hwnd);
	}
	// translate to our coordinates
	vd.Translate(-painter->GetTranslationX(), -painter->GetTranslationY());
	vd.BeginClipping(draw_rect);
	// now figure out where we are so we can draw the window border bevel, if needed
	if(window->GetFrame() && window->GetFrame()->UseCustomNativeFrame())
	{
		// white with 30% opacity
		int color;
		TRAPD(status, color = g_skin_manager->GetCurrentSkin()->ReadColorL("Options", "Window Border Color", OP_RGBA(255, 255, 255, 76), FALSE));
		if (OpStatus::IsSuccess(status))
			vd.SetColor32(color);

		// left border line
		vd.DrawLine(OpPoint(0, 0), OpPoint(0, m_screen->GetHeight()));

		// right border line
		vd.DrawLine(OpPoint(m_screen->GetWidth() - 1, 0), OpPoint(m_screen->GetWidth() - 1, m_screen->GetHeight()));

		// bottom border line
		vd.DrawLine(OpPoint(0, m_screen->GetHeight() - 1), OpPoint(m_screen->GetWidth(), m_screen->GetHeight() - 1));
	}
#if 0	// disable for the time being
	if(window->GetFrame() && window->GetFrame()->UseCustomUIFrame())
	{
		RECT ui_border = window->GetFrame()->GetUIFrameSizes();

		// ensure we don't draw above tabs by using the Y translation set in the painter
		int translation_y = painter->GetTranslationY();
		int top = translation_y;
		int bottom = m_screen->GetHeight() - ui_border.bottom;
		int width = m_screen->GetWidth() - ui_border.left;
		int height = m_screen->GetHeight() - ui_border.bottom + 1;
		int left = ui_border.left - 1;

		// black with 28% opacity
		int color;
		TRAPD(status, color = g_skin_manager->GetCurrentSkin()->ReadColorL("Options", "Window Inner Border Color", OP_RGBA(0, 0, 0, 71), FALSE));
		if (OpStatus::IsSuccess(status))
			vd.SetColor32(color);
		//	vd.SetColor32(OP_RGB(255, 255, 0));

		// left border line
		vd.DrawLine(OpPoint(left, top), OpPoint(left, height));

		// bottom border line
		vd.DrawLine(OpPoint(left+1, bottom), OpPoint(width, bottom));

		// right border line
		vd.DrawLine(OpPoint(width, top - 1), OpPoint(width, height));
	}
#endif
	vd.EndClipping();
	vd.EndPaint();
}

void VegaMDEScreen::OnTimeOut(WindowsTimer*)
{
	PostMessage(m_window->m_hwnd, WM_OPERA_MDE_RENDER, 0, 0);
}

class EnumWindowsInfo
{
public:
	EnumWindowsInfo() {}
	void HandleOverlappingWindow(MDE_RECT win_r);
public:
	MDE_RECT scroll_rect;
	int scroll_screen_x, scroll_screen_y;
	HWND scroll_hwnd;
	int dx, dy;
	MDE_Screen *screen;
};

void EnumWindowsInfo::HandleOverlappingWindow(MDE_RECT win_r)
{
	win_r.x -= scroll_screen_x;
	win_r.y -= scroll_screen_y;
	if (!MDE_RectIsEmpty(win_r) && MDE_RectIntersects(win_r, scroll_rect))
	{
		MDE_RECT r = MDE_RectClip(win_r, scroll_rect);
		if (!MDE_RectIsEmpty(r) && !MDE_RectIsIdentical(r, scroll_rect))
		{
			// Handy debug code
			/*uni_char string[100];
			GetWindowText(hwnd, (LPWSTR)&string, 100);
			OutputDebugString(string);
			OutputDebugStringA("\n");*/

			MDE_Region reg;
			reg.Set(r);
			reg.Offset(dx, dy);

			if (reg.num_rects &&
				MDE_RectIntersects(reg.rects[0], r) &&
				reg.ExcludeRect(reg.rects[0], r))
			{
				reg.RemoveRect(0);
			}
			for(int i = 0; i < reg.num_rects; i++)
				screen->Invalidate(reg.rects[i], true);
		}
	}
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	EnumWindowsInfo *info = (EnumWindowsInfo *) lParam;

	// All windows from now on have lower Z except there might 
	// still be some top-most windows
	if (hwnd == info->scroll_hwnd)
		only_top_most = TRUE;

	RECT rect;
	if ((!only_top_most || GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) &&
		IsWindow(hwnd) &&
		!IsIconic(hwnd) &&
		GetDesktopWindow() != hwnd &&
		!(GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) &&
		IsWindowVisible(hwnd) &&
		GetWindowRect(hwnd, &rect))
	{
		info->HandleOverlappingWindow(MDE_MakeRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top));
	}
	return TRUE;
}

bool VegaMDEScreen::IsResizing()
{
	return !!m_window->m_sizemove;
}

bool VegaMDEScreen::UseTransparentBackground()
{
	return !!m_vegaWin->GetIsLayered();
}

bool VegaMDEScreen::ScrollPixelsSupported()
{
	if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D)
		return false;

	return !m_vegaWin->GetIsLayered();
}
void VegaMDEScreen::ScrollPixels(const MDE_RECT &rect, int dx, int dy)
{
	if (!IsWindowVisible(m_window->m_hwnd) || IsIconic(m_window->m_hwnd))
		return;

	RECT r = { rect.x, rect.y, rect.x + rect.w, rect.y + rect.h };
	// Extend RECT as necessary. ScrollWindowEx expects union of source and dest area.
	if (dy > 0)
		r.top -= dy;
	if (dy < 0)
		r.bottom -= dy;
	if (dx > 0)
		r.left -= dx;
	if (dx < 0)
		r.right -= dx;

	MDE_RECT union_scroll_rect = MDE_MakeRect(r.left, r.top, r.right - r.left, r.bottom - r.top);

	BOOL composition = FALSE;
	OPDwmIsCompositionEnabled(&composition);

	POINT client_point = {0, 0};
	ClientToScreen(m_window->m_hwnd, &client_point);

	if (!composition)
	{
		RECT monitor_rect;
		MonitorRectFromWindow(monitor_rect, m_window->m_hwnd, FALSE);

		if (!OpRect(monitor_rect.left, monitor_rect.top, monitor_rect.right - monitor_rect.left, monitor_rect.bottom - monitor_rect.top).Contains(
			OpRect(union_scroll_rect.x + client_point.x, union_scroll_rect.y + client_point.y, union_scroll_rect.w, union_scroll_rect.h)))
		{
			// Special case. When we're partly outside one monitor just invalidate all.
			// Would ideally just invalidate the right parts but multiple monitor support makes that messy.
			Invalidate(rect, true);
			return;
		}
	}

	::ScrollWindowEx(m_window->m_hwnd, dx, dy, &r, &r, NULL, NULL, 0);

	// ScrollWindowEx must scroll without SW_INVALIDATE because we might have windowed plugins
	// in the scrolled area. Those would (with SW_INVALIDATE) caus us to repaint a large area
	// resulting in slow lagging scroll.
	//
	// We might still have other overlapped windows we must care about though (such as other applications).
	// We must enumerate all those and invalidate "new" areas of intersecting windows with higher Z level
	// than the scrolled window.
	//
	// If composition is enabled we don't have to do this because overlapping applications isn't a problem.

	if (composition)
		return;

	EnumWindowsInfo info;
	info.scroll_hwnd = m_window->m_hwnd;
	info.scroll_rect = union_scroll_rect;
	info.scroll_screen_x = client_point.x;
	info.scroll_screen_y = client_point.y;
	info.dx = dx;
	info.dy = dy;
	info.screen = this;
	
	only_top_most = FALSE;
	EnumWindows(EnumWindowsProc, (LPARAM)&info);
}

void UpdateIMEWindows(HWND hWnd, HIMC hIMC)
{
	// Update the position of the candidate window
	CANDIDATEFORM CandForm;
	CandForm.dwIndex = 0;
	CandForm.dwStyle = CFS_EXCLUDE;
	POINT client_point = {0, 0};
	ClientToScreen(hWnd, &client_point);
	CandForm.rcArea.left = widgetinfo.rect.x - client_point.x;
	CandForm.rcArea.top = widgetinfo.rect.y - client_point.y;

	CandForm.rcArea.right = CandForm.rcArea.left + widgetinfo.rect.width;
	CandForm.rcArea.bottom = CandForm.rcArea.top + widgetinfo.rect.height;
	CandForm.ptCurrentPos.x = CandForm.rcArea.left;
	CandForm.ptCurrentPos.y = CandForm.rcArea.top;

	// Update the position of the compose window.
	// (We are not using it but the IME might get its position to show tooltips etc..)
/*	COMPOSITIONFORM CompForm;
	CompForm.dwStyle = CFS_RECT;
	CompForm.rcArea.left = CandForm.ptCurrentPos.x;
	CompForm.rcArea.top = CandForm.ptCurrentPos.y;
	CompForm.rcArea.right = CompForm.rcArea.left + widgetinfo.rect.width;
	CompForm.rcArea.bottom = CompForm.rcArea.top + widgetinfo.rect.height;
	CompForm.ptCurrentPos.x = CandForm.ptCurrentPos.x;
	CompForm.ptCurrentPos.y = CandForm.ptCurrentPos.y;*/

	if (WindowsOpView::GetGlobalCaretHWND())
	{
		// Some IMEs put the popupwindows at the global caretposition
		SetCaretPos(CandForm.rcArea.left, CandForm.rcArea.top);
	}

//	ImmSetCompositionWindow(hIMC, &CompForm);
	ImmSetCandidateWindow(hIMC, &CandForm);
}

static inline bool IsDwmCompositionEnabled()
{
	BOOL compositionEnabled;
	return
		SUCCEEDED(OPDwmIsCompositionEnabled(&compositionEnabled)) &&
		!!compositionEnabled;
}

/**********************************************************************************/

BOOL MonitorRectFromMonitor(RECT& hMonitorRect, void* monitor, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY)
{
	MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (monitor && GetMonitorInfo((HMONITOR)monitor, (MONITORINFO*)&mi))
    {
        CopyRect(&hMonitorRect, onlyWorkArea?&mi.rcWork:&mi.rcMonitor);
        return TRUE;
    }
    else
    {
        hMonitorRect.top=0;
        hMonitorRect.left=0;
        hMonitorRect.bottom=GetSystemMetrics(SM_CYSCREEN);
        hMonitorRect.right=GetSystemMetrics(SM_CXSCREEN);
        return FALSE;
    }
}

/**********************************************************************************/

BOOL MonitorRectFromWindow(RECT& hMonitorRect, HWND wnd, BOOL onlyWorkArea, DWORD dwFlags)
{
	HMONITOR hMonitor = MonitorFromWindow(wnd, dwFlags);
    return MonitorRectFromMonitor(hMonitorRect, hMonitor, onlyWorkArea, dwFlags);
}

/**********************************************************************************/

BOOL MonitorRectFromRect(RECT& hMonitorRect, LPCRECT rect, BOOL onlyWorkArea, DWORD dwFlags)
{
	HMONITOR hMonitor = MonitorFromRect(rect, dwFlags);
    return MonitorRectFromMonitor(hMonitorRect, hMonitor, onlyWorkArea, dwFlags);
}

/**********************************************************************************/

BOOL MonitorRectFromPoint(RECT& hMonitorRect, POINT point, BOOL onlyWorkArea, DWORD dwFlags)
{
	HMONITOR hMonitor = MonitorFromPoint(point, dwFlags);
    return MonitorRectFromMonitor(hMonitorRect, hMonitor, onlyWorkArea, dwFlags);
}

//*****************************************************************************

#if defined(MANUAL_PLUGIN_ACTIVATION) && !defined(NS4P_COMPONENT_PLUGINS)

BOOL IsPluginMessageHandled(HWND hWnd, UINT message)
{
	// handle plugin activation, if needed
	//
	// Plugins that are static in the document will need to be manually clicked on to
	// be interactive.
	//
	// Plugins that are generated from an external script will function as normal
	//
	WindowsOpPluginWindow *window = WindowsOpPluginWindow::GetPluginWindowFromHWND(hWnd);
	if (window)
	{
		switch (message)
		{
			case WM_LBUTTONDOWN:
				if(window->GetListener())
					window->GetListener()->OnMouseDown();

				return window->IsBlocked();

			case WM_LBUTTONUP:
				if(window->GetListener())
					window->GetListener()->OnMouseUp();

				if (window->IsBlocked())
				{
					window->BlockMouseInput(FALSE);
					// Still ignore this event, even if the plugin was just activated.
					return TRUE;
				}

				return window->IsBlocked();

			// ignore these messages until the plugin has been activated
			case WM_MOUSEMOVE:
			case WM_NCMOUSEMOVE:
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONDBLCLK:
			case WM_RBUTTONUP:
			case WM_NCRBUTTONDOWN:
			case WM_NCRBUTTONDBLCLK:
			case WM_NCRBUTTONUP:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONDBLCLK:
			case WM_MBUTTONUP:
				if (message == WM_MOUSEMOVE || message == WM_NCMOUSEMOVE)
					if (window->GetListener())
						window->GetListener()->OnMouseHover();

				return window->IsBlocked();
		}
	}

	return FALSE;
}

#endif // MANUAL_PLUGIN_ACTIVATION && !NS4P_COMPONENT_PLUGINS

BOOL HandleMouseEvents(HWND hWnd, UINT message, const OpPoint& point)
{
	if (!g_input_manager)
		return FALSE;

	g_input_manager->SetMousePosition(point);
	
	ShiftKeyState shift_keys = g_op_system_info->GetShiftKeyState();

	// do not open menu if mousedown event fires after pressing and before releasing alt key
	if (g_windows_menu_manager && g_windows_menu_manager->m_opening_menu)
	{
		switch(message)
		{
			case WM_LBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_RBUTTONDOWN:
				g_windows_menu_manager->m_opening_menu = FALSE;
				break;
			}
	}

#if defined(MANUAL_PLUGIN_ACTIVATION) && !defined(NS4P_COMPONENT_PLUGINS)
	if (IsPluginMessageHandled(hWnd, message))
		return TRUE;
#endif // MANUAL_PLUGIN_ACTIVATION && !NS4P_COMPONENT_PLUGINS

	switch (message)
	{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		{
			if((GetMessageExtraInfo() & MOUSEEVENTF_FROMTOUCH) == MOUSEEVENTF_FROMTOUCH)
				g_input_manager->SetTouchInputState(TOUCH_INPUT_ACTIVE);
			else
				g_input_manager->SetTouchInputState(0);
		
			WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWnd);
			if (window && g_input_manager->InvokeKeyDown(OP_KEY_MOUSE_BUTTON_1, NULL, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE))
			{
				// Window might have been deleted by WindowsOpDragManager::StopDrag().
				window = WindowsOpWindow::GetWindowFromHWND(hWnd);
				if (window && window->m_mde_screen->m_captured_input)
				{
					window->m_mde_screen->m_captured_input->OnMouseLeave();
					window->m_mde_screen->ReleaseMouseCapture();
				}
				return TRUE;
			}
			break;
		}

		case WM_LBUTTONUP:
			if((GetMessageExtraInfo() & MOUSEEVENTF_FROMTOUCH) == MOUSEEVENTF_FROMTOUCH)
				g_input_manager->SetTouchInputState(TOUCH_INPUT_ACTIVE);
			else
				g_input_manager->SetTouchInputState(0);

			if (g_input_manager->InvokeKeyUp(OP_KEY_MOUSE_BUTTON_1, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE))
			{
				ReleaseCapture();
				return TRUE;
			}
			break;
			
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		{
			if((GetMessageExtraInfo() & MOUSEEVENTF_FROMTOUCH) == MOUSEEVENTF_FROMTOUCH)
				g_input_manager->SetTouchInputState(TOUCH_INPUT_ACTIVE);
			else
				g_input_manager->SetTouchInputState(0);

			WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWnd);
			if (window && g_input_manager->InvokeKeyDown(OP_KEY_MOUSE_BUTTON_2, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE))
			{
				// Window might have been deleted by WindowsOpDragManager::StopDrag().
				window = WindowsOpWindow::GetWindowFromHWND(hWnd);
				if (window && window->m_mde_screen->m_captured_input)
				{
					window->m_mde_screen->m_captured_input->OnMouseLeave();
					window->m_mde_screen->ReleaseMouseCapture();
				}
				return TRUE;
			}
			break;
		}

		case WM_RBUTTONUP:
			if((GetMessageExtraInfo() & MOUSEEVENTF_FROMTOUCH) == MOUSEEVENTF_FROMTOUCH)
				g_input_manager->SetTouchInputState(TOUCH_INPUT_ACTIVE);
			else
				g_input_manager->SetTouchInputState(0);

			if (g_input_manager->InvokeKeyUp(OP_KEY_MOUSE_BUTTON_2, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE))
			{
				ReleaseCapture();
				return TRUE;
			}
			break;

		case WM_NCRBUTTONDOWN:
		case WM_NCRBUTTONDBLCLK:
		{
			if((GetMessageExtraInfo() & MOUSEEVENTF_FROMTOUCH) == MOUSEEVENTF_FROMTOUCH)
				g_input_manager->SetTouchInputState(TOUCH_INPUT_ACTIVE);
			else
				g_input_manager->SetTouchInputState(0);

			WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWnd);
			if (window && GetParent(hWnd) && g_input_manager->InvokeKeyDown(OP_KEY_MOUSE_BUTTON_2, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE))
			{
				// Window might have been deleted by WindowsOpDragManager::StopDrag().
				window = WindowsOpWindow::GetWindowFromHWND(hWnd);
				if (window && window->m_mde_screen->m_captured_input)
				{
					window->m_mde_screen->m_captured_input->OnMouseLeave();
					window->m_mde_screen->ReleaseMouseCapture();
				}
				return TRUE;
			}
			break;
		}

		case WM_NCRBUTTONUP:
			if((GetMessageExtraInfo() & MOUSEEVENTF_FROMTOUCH) == MOUSEEVENTF_FROMTOUCH)
				g_input_manager->SetTouchInputState(TOUCH_INPUT_ACTIVE);
			else
				g_input_manager->SetTouchInputState(0);

			if (GetParent(hWnd) && g_input_manager->InvokeKeyUp(OP_KEY_MOUSE_BUTTON_2, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE))
			{
				ReleaseCapture();
				return TRUE;
			}
			break;
			
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
		{
			WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWnd);
			if (window && g_input_manager->InvokeKeyDown(OP_KEY_MOUSE_BUTTON_3, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE))
			{
				// Window might have been deleted by WindowsOpDragManager::StopDrag().
				window = WindowsOpWindow::GetWindowFromHWND(hWnd);
				if (window && window->m_mde_screen->m_captured_input)
				{
					window->m_mde_screen->m_captured_input->OnMouseLeave();
					window->m_mde_screen->ReleaseMouseCapture();
				}
				return TRUE;
			}
			break;
		}

		case WM_MBUTTONUP:
			if (g_input_manager->InvokeKeyUp(OP_KEY_MOUSE_BUTTON_3, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE))
			{
				ReleaseCapture();
				return TRUE;
			}
			break;
	}

	return FALSE;
}

BOOL HandleMouseEvents(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	OpPoint point;

	if (message >= WM_NCMOUSEMOVE && message <= WM_NCMBUTTONDBLCLK)
	{
		point.x = GET_X_LPARAM(lParam);
		point.y = GET_Y_LPARAM(lParam);
	}
	else if (message >= WM_MOUSEMOVE && message <= WM_MBUTTONDBLCLK)
	{
		POINT client_point = {0, 0};
		ClientToScreen(hWnd, &client_point);

		point.x = client_point.x + GET_X_LPARAM(lParam);
		point.y = client_point.y + GET_Y_LPARAM(lParam);
	}
	else
	{
		return FALSE;
	}

	return HandleMouseEvents(hWnd, message, point);
}

/***********************************************************************************
**
**	FindWindowUnderCursor
**
***********************************************************************************/
/* static */
WindowsOpWindow* WindowsOpWindow::FindWindowUnderCursor()
{
	POINT point;

	GetCursorPos(&point);

	HWND hwnd = WindowFromPoint(point);

	return WindowsOpWindow::GetWindowFromHWND(hwnd);
}
/* static */
void WindowsOpWindow::FinishDrag(HWND hWnd)
{
	if(g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
	{
		WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWnd);
		DesktopDragObject *drag_object = static_cast<DesktopDragObject *>(g_drag_manager->GetDragObject());
		if (window && window->m_hwnd)
		{
			// must be synchronous to avoid accessing a deleted drag object in the calling code
			SendMessage(window->m_hwnd, WM_OPERA_DRAG_DROP, 0, 0);
		}
		else if (drag_object->IsDropAvailable())
		{
			if (drag_object->GetType() == OpTypedObject::DRAG_TYPE_WINDOW)
			{
				// store the ids as the focus loss when opening new windows will stop the drag, making m_drag_object NULL
				OpINT32Vector ids;
				for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
				{
					ids.Add(drag_object->GetID(i));
				}
				for (UINT32 i = 0; i < ids.GetCount(); i++)
				{
					DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(ids.Get(i));

					if (desktop_window)
					{
						desktop_window->SetParentWorkspace(NULL);
					}
				}
			}
		}
		if(g_drag_data)
			g_drag_data->drag_acknowledged = false;
	}
}

bool WindowsOpWindow::IsCursorInChrome()
{
	bool cursor_in_chrome = false;

	if (m_titlebar_height)
	{
		POINT point;
		GetCursorPos(&point);
		ScreenToClient(m_hwnd, &point);

		OpPagebar* pb = GetPagebar();
		BOOL in_pagebar = FALSE;
		INT32 pbh = 0;

		if (pb)
		{
			int wx = point.x;
			int wy = point.y;
			if (pb->GetWidgetContainer() && pb->GetWidgetContainer()->GetOpView())
				((MDE_OpView*)pb->GetWidgetContainer()->GetOpView())->GetMDEWidget()->ConvertFromScreen(wx, wy);
			OpWidget* w = pb->GetWidgetContainer()->GetRoot()->GetWidget(OpPoint(wx, wy), TRUE);
			in_pagebar = (w == NULL || w->IsOfType(OpTypedObject::WIDGET_TYPE_TOOLBAR) || w->GetType() == OpTypedObject::WIDGET_TYPE_GROUP);
			pbh = pb->GetHeight();
		}

		if ((point.y < m_titlebar_height + pbh && in_pagebar))
		{
			cursor_in_chrome = true;
			MDE_View* v = m_mde_screen->m_captured_input ? m_mde_screen->m_captured_input : m_mde_screen->GetViewAt(point.x, point.y, true);
			if (v && v->IsType("QuickGhostInputView"))
				cursor_in_chrome = false;
		}
	}
	return cursor_in_chrome;
}

LRESULT WindowsOpWindow::IsCursorOnBorder() const
{
	LRESULT on_border = HTNOWHERE;
	// If the window is maximized or in fullscreen, the mouse can't be on the border, as the window
	// fills the whole screen. So just fail it right away.
	if (m_state != MAXIMIZED && m_state != FULLSCREEN)
	{
		RECT frame = {WINDOW_DEFAULT_BORDER_SIZE, WINDOW_DEFAULT_BORDER_SIZE, WINDOW_DEFAULT_BORDER_SIZE, WINDOW_DEFAULT_BORDER_SIZE};
		// The area where cursor turns into a corner resize should be
		// be bigger than the frame size to make resizing easier.
		INT32 double_frame = WINDOW_DEFAULT_BORDER_SIZE * 2;

		POINT point;
		GetCursorPos(&point);
		ScreenToClient(m_hwnd, &point);

		if(m_frame)
		{
			frame = m_frame->GetUIFrameSizes();
		}

		if (point.x >= (m_width - frame.right)) // Right side.
		{
			if (point.y >= (m_height - double_frame))
				on_border = HTBOTTOMRIGHT;
			else if (point.y <= double_frame)
				on_border = HTTOPRIGHT;
			else
				on_border = HTRIGHT;
		}
		else if (point.x <= frame.left) // Left side.
		{
			if (point.y >= (m_height - double_frame))
				on_border = HTBOTTOMLEFT;
			else if (point.y <= double_frame)
				on_border = HTTOPLEFT;
			else
				on_border = HTLEFT;
		}
		else if (point.y <= frame.top) // Top side.
		{
			// Handled by IsCursorInChrome branch in WM_NCHITTEST handling.
		}
		else if (point.y >= (m_height - frame.bottom)) // Bottom side.
		{
			if (point.x <= double_frame)
				on_border = HTBOTTOMLEFT;
			else if (point.x >= (m_width - double_frame))
				on_border = HTBOTTOMRIGHT;
			else
				on_border = HTBOTTOM;
		}
	}

	return on_border;
}

long WindowsOpWindow::WmPaint(WPARAM wParam, LPARAM lParam)
{
	// wParam This parameter is not used.
	// lParam This parameter is not used.

	// No need to draw if we are minimized.
	if (IsIconic(m_hwnd))
	{
		ValidateRect(m_hwnd, NULL);
		return 0; // Handled
	}

	// Lazy resize of the buffer. We do it here instead of when the window is resized, to deffer multiple resizes.
	if (m_vega_window->getHeight() != (UINT)m_height || m_vega_window->getWidth() != (UINT)m_width)
	{
		if (m_vega_window->wm_size(m_width, m_height))
		{
			m_mde_screen->Validate(true);
			ValidateRect(m_hwnd, NULL);
			return 0; // Handled
		}
	}

	// Hardware window
	if (m_vega_window->IsHardwareWindow())
	{
		BOOL composition = FALSE;
		OPDwmIsCompositionEnabled(&composition);

		// If we have no composition, we have to repaint ourself for example when an other window is moving above the Opera window.
		if (!composition)
		{
			RECT r;
			if (GetUpdateRect(m_hwnd, &r, false))
			{
				m_mde_screen->Invalidate(MDE_MakeRect(r.left, r.top, r.right - r.left, r.bottom - r.top), true);
			}
		}

		return DefWindowProc(m_hwnd, WM_PAINT, wParam, lParam);
	}

	if (m_hwnd)
	{
		if (m_mde_screen->ScrollPixelsSupported())
		{
			// When we use hwd scroll, we must NOT blit the backbuffer as it may contain invalid parts. Invalidate through core to repaint area instead.
			RECT r;
			if (GetUpdateRect(m_hwnd, &r, false))
			{
				m_mde_screen->Invalidate(MDE_MakeRect(r.left, r.top, r.right - r.left, r.bottom - r.top), true);
				ValidateRect(m_hwnd, &r);
			}
			return DefWindowProc(m_hwnd, WM_PAINT, wParam, lParam);
		}
		
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(m_hwnd, &ps);
		RECT rect = ps.rcPaint;

		BitBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, m_vega_window->GetBitmapDC(), rect.left, rect.top, SRCCOPY);
		EndPaint(m_hwnd, &ps);
	}

	return 0; // Handled
}

long WindowsOpWindow::WmWindowposchanged(WPARAM wParam, LPARAM lParam)
{
	// wParam is not used.
	// lParam contains a pointer to a WINDOWPOS structure that contains information about the window's new size and position.

	WINDOWPLACEMENT placement;
	placement.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(m_hwnd, &placement);

	if (m_style == STYLE_DESKTOP)
	{
		WINDOWPOS* window_pos = reinterpret_cast<WINDOWPOS*>(lParam);
		if (window_pos && window_pos->flags & SWP_FRAMECHANGED && IsDwmCompositionEnabled())
		{
			/* Make sure DwmExtendFrameIntoClientArea triggers as it's necessary
			   to avoid black title bar effect when resuming from hibernation. */
			m_vega_window->SetTransparencyChanged();
			m_vega_window->ExtendFrameInfoClientArea();
		}
	}

	if (!m_fullscreen && m_state == RESTORED)
	{
		m_restored_rect.x = placement.rcNormalPosition.left;
		m_restored_rect.y = placement.rcNormalPosition.top;
		m_restored_rect.width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
		m_restored_rect.height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
	}

	if (m_state != OpWindow::MINIMIZED)
		m_old_state = m_state;

	EnsureHiddenIconicState();

	bool is_minimizing = placement.showCmd == SW_SHOWMINIMIZED && m_state != OpWindow::MINIMIZED;
	bool from_minimized = placement.showCmd != SW_SHOWMINIMIZED && m_state == OpWindow::MINIMIZED;
	bool is_maximizing = placement.showCmd == SW_SHOWMAXIMIZED && m_state != OpWindow::MAXIMIZED;
	bool from_maximized = placement.showCmd != SW_SHOWMAXIMIZED && m_state == OpWindow::MAXIMIZED;

	if (placement.showCmd == SW_SHOWMINIMIZED)
		m_state = OpWindow::MINIMIZED;
	else if (placement.showCmd == SW_SHOWMAXIMIZED)
		m_state = OpWindow::MAXIMIZED;
	else if (m_fullscreen)
		m_state = OpWindow::FULLSCREEN;
	else
		m_state = OpWindow::RESTORED;

	if (is_minimizing)
	{
		DeactivateAndActivateTopmost(TRUE);
		UpdateBufferSize(1, 1); // Throw away (most of the) buffer to free up the memory while minimized
	}
	else if (from_minimized)
	{
		SetWindowPos(m_hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOSENDCHANGING | SWP_DEFERERASE);
		m_mde_screen->Validate(true);
	}
	else if (is_maximizing || from_maximized)
	{
		OnPagebarMoved(m_requested_titlebar_height>0, m_requested_extended_chrome_height);
	}
	
	return DefWindowProc(m_hwnd, WM_WINDOWPOSCHANGED, wParam, lParam);;
}

WindowsOpView *FindWindowsOpViewAtPosition(WindowsOpWindow *window, int x, int y)
{
	WindowsOpView* view = NULL;
	MDE_View* v = window->m_mde_screen->m_captured_input;
	if (!v)
		v = window->m_mde_screen->GetViewAt(x, y, true);
	while (v)
	{
		if (v->IsType("MDE_Widget"))
		{
			view = (WindowsOpView*) ((MDE_Widget*)v)->GetOpView();
			break;
		}
		v = v->m_parent;
	}
	return view;
}

//*****************************************************************************
long FAR PASCAL WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static BOOL	s_is_power_suspend_state = FALSE;
    static BOOL s_releasing_own_capture = FALSE;
    static BOOL s_left_down = FALSE;
    static BOOL s_middle_down = FALSE;
    static BOOL s_right_down = FALSE;
	static POINT last_mouse_pos;

	//We are receiving a sent (and thus probably blocking) message, probably from a child process.
	//Reply to it before handling it
	if (InSendMessage() && (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST || message >= WM_KEYFIRST && message <= WM_KEYLAST || message == WM_ACTIVATE))
		ReplyMessage(0);

	WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWnd);

	if (window == NULL)
	{
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	if(message == WM_CREATE)
	{
		char clsName[32];
		GetClassNameA(hWnd,clsName,sizeof(clsName));
		if(op_strcmp(clsName,OPERA_WINDOW_CLASSNAME) == 0)
		{
			ImmAssociateContext(hWnd,0);
		}
	}

	POINT client_point = {0};
	ShiftKeyState keystate = static_cast<ShiftKeyState>(g_op_system_info->GetShiftKeyState());
	BOOL lookForView = FALSE;
	if (message >= WM_NCMOUSEMOVE && message <= WM_NCMBUTTONDBLCLK)
	{
		client_point.x = GET_X_LPARAM(lParam);
		client_point.y = GET_Y_LPARAM(lParam);
		ScreenToClient(hWnd, &client_point);
		lookForView = TRUE;
	}
	else if (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
	{
		client_point.x = GET_X_LPARAM(lParam);
		client_point.y = GET_Y_LPARAM(lParam);
		if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
			ScreenToClient(hWnd, &client_point);
		lookForView = TRUE;
	}
	else if (message == WM_CAPTURECHANGED && window->m_mde_screen->m_captured_input)
	{
		client_point.x = window->m_mde_screen->m_mouse_x;
		client_point.y = window->m_mde_screen->m_mouse_y;
		lookForView = TRUE;
	}
	else if (message == idMsgLostCursor)
	{
		client_point.x = window->m_mde_screen->m_mouse_x;
		client_point.y = window->m_mde_screen->m_mouse_y;
		lookForView = TRUE;
	}
	WindowsOpView* view = NULL;
	if (lookForView)
	{
		view = FindWindowsOpViewAtPosition(window, client_point.x, client_point.y);
	}
	WindowsOpView::s_safe_view_pointer = view;

	// Cancel drag when pressing right mouse button while dragging.
	if (s_left_down && (message == WM_RBUTTONDOWN || message == WM_NCRBUTTONDOWN))
	{
		// Even when not dragging, reset left_down status to FALSE as we don't
		// want drag triggering when holding both left and right buttons.
		s_left_down = FALSE;
		if (g_drag_manager->IsDragging())
		{
			window->m_mde_screen->TrigDragCancel(g_op_system_info->GetShiftKeyState());
			return 0;
		}
	}

	if (HandleMouseEvents(hWnd, message, wParam, lParam))
	{
		if (WindowsOpView::s_safe_view_pointer && WindowsOpView::s_safe_view_pointer->mouseListener)
			WindowsOpView::s_safe_view_pointer->mouseListener->OnMouseCaptureRelease();
		return 0;
	}

	if (message == WM_NCRBUTTONUP && client_point.y < window->m_titlebar_height && window->m_titlebar_height)
	{
		int cmd = TrackPopupMenu(GetSystemMenu(window->m_hwnd, FALSE), TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON,
		                         GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, window->m_hwnd, NULL);
		PostMessage(window->m_hwnd, WM_SYSCOMMAND, cmd, lParam);
		return 0;
	}

	BOOL user_stopped = DocWin_HandleIntelliMouseMessages(hWnd, message, wParam, lParam);
	if(user_stopped && (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP))
		return DefWindowProc(hWnd,  message,  wParam,  lParam);

	// WM_MOUSEWHEEL is sent to the focused window, which might not be the real target.
	// so whether a mde_view was found doesn't really matter, this message rely on
	// WindowFromPoint() to find the target window
	if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
		return window->HandleMouseWheelEvent(message == WM_MOUSEWHEEL, wParam, lParam, client_point, keystate);


	//
	// Dispatch table for windows messages (actual message handling).
	//

	if (message < WM_USER && WindowsOpWindow::wm_messages[message])
	{
		WindowsOpWindow::WmMessageFunction msg_function = WindowsOpWindow::wm_messages[message];
		return (window->*msg_function)(wParam, lParam);
	}

	//
	//
	//

	if (view)
	{
		if (message == idMsgLostCursor)
		{
			// don't reset the capture if we're in the middle of a gesture
			if(!WindowsOpWindow::s_touch_gesture_active)
			{
				/* If Opera is foreground window, fire mouseout event
				   on last hovered element. If Opera is not active, we
				   don't want to care about that as it will make tooltip
				   appear in front of other applications. */
				if (GetForegroundWindow() == hWnd)
					if (view->mouseListener)
						view->mouseListener->OnMouseLeave();

				OpWidget::ZeroCapturedWidget();
				g_application->SetToolTipListener(0);
			}
			return TRUE;
		}
		switch (message)
		{
			case WM_LBUTTONDOWN:
			{
				s_left_down = TRUE;
				SetCapture(window->m_hwnd);
				if (view->IsFocusable())
					SetFocus(window->m_hwnd);

				window->FindAndActivateDesktopWindow(client_point.x, client_point.y);
				window->m_mde_screen->TrigMouseDown(client_point.x, client_point.y, 1, 1, keystate);

				WindowsOpDragManager::PrepareDragOperation(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				break;
			}
			case WM_LBUTTONDBLCLK:
				window->m_mde_screen->TrigMouseDown(client_point.x, client_point.y, 1, 2, keystate);
				s_left_down = TRUE;
				break;

			case WM_LBUTTONUP:
				WindowsOpWindow::FinishDrag(hWnd);
				s_releasing_own_capture = GetCapture() == hWnd;
				ReleaseCapture();
				if (s_left_down)
				{			
					window->m_mde_screen->TrigMouseUp(client_point.x, client_point.y, 1, keystate);
				}
				s_left_down = FALSE;
				break;
				
			case WM_RBUTTONDOWN:
				{
				s_right_down = TRUE;
				SetCapture(window->m_hwnd);
				if (view->IsFocusable())
					SetFocus(window->m_hwnd);

				window->FindAndActivateDesktopWindow(client_point.x, client_point.y);
				window->m_mde_screen->TrigMouseDown(client_point.x, client_point.y, 2, 1, keystate);
				break;
				}
			case WM_RBUTTONDBLCLK:
				s_right_down = TRUE;
				window->m_mde_screen->TrigMouseDown(client_point.x, client_point.y, 2, 2, keystate);
				break;			
			case WM_RBUTTONUP:
				WindowsOpWindow::FinishDrag(hWnd);
				s_releasing_own_capture = GetCapture() == hWnd;
				ReleaseCapture();
				if (s_right_down)
					window->m_mde_screen->TrigMouseUp(client_point.x, client_point.y, 2, keystate);
				s_right_down = FALSE;
				break;

			case WM_MBUTTONDOWN:
				if (!IsPanning())
				{
					s_middle_down = TRUE;
					SetCapture(window->m_hwnd);
					if (view->IsFocusable())
						SetFocus(window->m_hwnd);

					window->FindAndActivateDesktopWindow(client_point.x, client_point.y);
					window->m_mde_screen->TrigMouseDown(client_point.x, client_point.y, 3, 1, keystate);
				}
				break;
			case WM_MBUTTONDBLCLK:
				s_middle_down = TRUE;
				window->m_mde_screen->TrigMouseDown(client_point.x, client_point.y, 3, 2, keystate);
				break;
				
			case WM_MBUTTONUP:
				if (!IsPanning())
				{
					s_releasing_own_capture = GetCapture() == hWnd;
					ReleaseCapture();
				}
				if (s_middle_down)
					window->m_mde_screen->TrigMouseUp(client_point.x, client_point.y, 3, keystate);
				s_middle_down = FALSE;
				break;

			case WM_NCMOUSEMOVE:
				WM_MESSAGE_HANDLED("WM_NCMOUSEMOVE");
				if (window->IsCursorInChrome())
				{
					window->m_disable_cursor_change = TRUE;
				 
					window->m_mde_screen->TrigMouseMove(client_point.x, client_point.y, keystate);
				 
					window->m_disable_cursor_change = FALSE;

					POINT screen_point = client_point;
					ClientToScreen(hWnd, &screen_point);
					last_mouse_pos = screen_point;
				}
				return DefWindowProc(hWnd,  message,  wParam,  lParam);

			case WM_MOUSEMOVE:
				{
					// We should only handle the mousemove if it changed relative to the screen.
					// The reason is that windows trigs mousemove in some conditions when it's not moved.
					// F.ex for windows that have just been showed and we don't want to mouse-over items in dropdowns automatically.
					POINT screen_point = client_point;
					ClientToScreen(hWnd, &screen_point);
					if(last_mouse_pos.x != screen_point.x || last_mouse_pos.y != screen_point.y)
					{
						int x = GET_X_LPARAM(lParam);
						int y = GET_Y_LPARAM(lParam);

						if(g_drag_manager->IsDragging())
						{
							g_windows_drag_manager->UpdateDrag(window);
						}
						// the mouse move might have caused a drag to be started
						else if (s_left_down && g_drag_data && g_drag_data->drag_x != -1 && g_drag_data->drag_y != -1 &&
							(labs(g_drag_data->drag_x - x) > ::GetSystemMetrics(SM_CXDRAG) || labs(g_drag_data->drag_y - y) > ::GetSystemMetrics(SM_CYDRAG) ))
						{
							if (!g_drag_data->drag_acknowledged)
							{
								window->m_mde_screen->TrigDragStart(g_drag_data->drag_x, g_drag_data->drag_y, g_op_system_info->GetShiftKeyState(), x, y);
							}
							else 
							{
								break;
							}
						}
						window->m_mde_screen->TrigMouseMove(x, y, g_op_system_info->GetShiftKeyState());
						last_mouse_pos = screen_point;
					}
				}
				break;

			case WM_CAPTURECHANGED:

				// Capture doesn't really change,we are calling SetCapture
				// when we already have the capture
				if (lParam == (LPARAM)hWnd)
					break;

				if(!s_releasing_own_capture && view)
				{
					if (window->m_mde_screen->m_captured_input)
					{
						window->m_mde_screen->m_captured_input->OnMouseLeave();
						window->m_mde_screen->ReleaseMouseCapture();
					}
					s_left_down = FALSE;
					s_right_down = FALSE;
					s_middle_down = FALSE;
				}
				s_releasing_own_capture = FALSE;
				break;
			case WM_GETDLGCODE:
				return DLGC_WANTALLKEYS;

			case WM_NCLBUTTONDBLCLK:
				if (client_point.y >= window->m_titlebar_height)
				{
					PostMessage(hWnd, WM_LBUTTONDOWN, 0, MAKELONG(client_point.x, client_point.y));
					PostMessage(hWnd, WM_LBUTTONUP, 0, MAKELONG(client_point.x, client_point.y));
					PostMessage(hWnd, WM_LBUTTONDBLCLK, 0, MAKELONG(client_point.x, client_point.y));
					PostMessage(hWnd, WM_LBUTTONUP, 0, MAKELONG(client_point.x, client_point.y));
					return 0;
				}
				return DefWindowProc(hWnd,  message,  wParam,  lParam);
			case WM_NCRBUTTONDOWN:
				if (client_point.y >= window->m_titlebar_height)
				{
					PostMessage(hWnd, WM_RBUTTONDOWN, 0, MAKELONG(client_point.x, client_point.y));
					return 0;
				}
				return DefWindowProc(hWnd,  message,  wParam,  lParam);
			case WM_NCRBUTTONUP:
				if (client_point.y >= window->m_titlebar_height)
				{
					PostMessage(hWnd, WM_RBUTTONUP, 0, MAKELONG(client_point.x, client_point.y));
					return 0;
				}
				return DefWindowProc(hWnd,  message,  wParam,  lParam);
			case WM_NCMBUTTONDOWN:
				if (client_point.y >= window->m_titlebar_height)
				{
					PostMessage(hWnd, WM_MBUTTONDOWN, 0, MAKELONG(client_point.x, client_point.y));
					return 0;
				}
				return DefWindowProc(hWnd,  message,  wParam,  lParam);
			case WM_NCMBUTTONUP:
				if (client_point.y >= window->m_titlebar_height)
				{
					PostMessage(hWnd, WM_MBUTTONUP, 0, MAKELONG(client_point.x, client_point.y));
					return 0;
				}
				return DefWindowProc(hWnd,  message,  wParam,  lParam);

			default:
				return DefWindowProc(hWnd,  message,  wParam,  lParam);
		}
		return FALSE;
	}
	switch(message)
	{
#ifdef TOUCH_EVENTS_SUPPORT
		case WM_GESTURE:
		{
			GESTUREINFO gi = {};
			gi.cbSize = sizeof(GESTUREINFO);

			HGESTUREINFO gesture_handle = (HGESTUREINFO)lParam;

			if(OPGetGestureInfo(gesture_handle, &gi))
			{
				switch(gi.dwID)
				{
					case GID_BEGIN:
					{
						WindowsOpWindow::s_touch_gesture_active = true;
						window->m_gesture_info.arguments = (float)gi.ullArguments;
						window->m_gesture_info.first_point.x = gi.ptsLocation.x;
						window->m_gesture_info.first_point.y = gi.ptsLocation.y;

						ScreenToClient(window->m_hwnd, &window->m_gesture_info.first_point);
						break;
					}
					case GID_END:
					{
						WindowsOpWindow::s_touch_gesture_active = false;
						break;
					}
					case GID_ZOOM:
					{
						if(!(gi.dwFlags & GF_END))
						{
							window->m_gesture_info.second_point.x = gi.ptsLocation.x;
							window->m_gesture_info.second_point.y = gi.ptsLocation.y;

							POINT new_finger_distance;

							new_finger_distance.x = (window->m_gesture_info.first_point.x + window->m_gesture_info.second_point.x) / 2;
							new_finger_distance.y = (window->m_gesture_info.first_point.y + window->m_gesture_info.second_point.y) / 2;

							// has the fingers moved? Windows sends zoom messages all the time, no need to act until
							// the fingers actually move
							if(new_finger_distance.x != window->m_gesture_info.finger_distance.x ||
								new_finger_distance.y != window->m_gesture_info.finger_distance.y)
							{
								// the zoom factor is the ratio between the new and old distances
								float zoom_ratio = (float)((float)gi.ullArguments / window->m_gesture_info.arguments) * 100;

								DesktopWindow *win = g_application->GetActiveDocumentDesktopWindow();
								if(win && win->GetBrowserView())
								{
									float curr_scale = win->GetBrowserView()->GetWindowCommander()->GetScale();
									INT32 new_scale = curr_scale;
									if(zoom_ratio < 100)
										new_scale -= 8;
									else if(zoom_ratio > 100)
										new_scale += 8;

									if(new_scale != curr_scale)
									{
										win->GetBrowserView()->GetWindowCommander()->SetScale(new_scale);
									}
								}
								window->m_gesture_info.arguments = LODWORD(gi.ullArguments);
								window->m_gesture_info.finger_distance = new_finger_distance;
								window->m_gesture_info.first_point = window->m_gesture_info.second_point;
							}
						}
						OPCloseGestureInfoHandle(gesture_handle);
						return 0;
						break;
					}

					case GID_PAN:
					{
						window->m_gesture_info.second_point.x = gi.ptsLocation.x;
						window->m_gesture_info.second_point.y = gi.ptsLocation.y;

						ScreenToClient(window->m_hwnd, &window->m_gesture_info.second_point);

						g_input_manager->SetTouchInputState(TOUCH_INPUT_ACTIVE);

						if(gi.dwFlags & GF_INERTIA)
						{
							// the gesture has triggered inertia
							g_input_manager->SetTouchInputState(TOUCH_INPUT_ACTIVE | TOUCH_INPUT_INERTIA);
						}
						if(gi.dwFlags & GF_BEGIN)
						{
							// OPBeginPanningFeedback(window->m_hwnd);
							window->m_mde_screen->TrigTouchDown(0, window->m_gesture_info.first_point.x, window->m_gesture_info.first_point.y, 5, SHIFTKEY_NONE, 0);

							window->m_gesture_info.captured_view = window->m_mde_screen->GetViewAt(window->m_gesture_info.first_point.x, window->m_gesture_info.first_point.y, true);
							window->m_gesture_info.view_rect = window->m_gesture_info.captured_view->m_rect;
							window->m_gesture_info.captured_view->ConvertToScreen(window->m_gesture_info.view_rect.x, window->m_gesture_info.view_rect.y);

							window->m_gesture_info.overflow_y = 0;

							return 0;
						}
						else if(gi.dwFlags & GF_END)
						{
							// OPEndPanningFeedback(window->m_hwnd, TRUE);

							window->m_mde_screen->TrigTouchUp(0, window->m_gesture_info.second_point.x, window->m_gesture_info.second_point.y, 5, SHIFTKEY_NONE, 0);

							UINT32 touch_state = g_input_manager->GetTouchInputState() & ~TOUCH_INPUT_INERTIA;

							g_input_manager->SetTouchInputState(touch_state);

							return 0;
						}
						else
						{
							int delta_x = window->m_gesture_info.second_point.x - window->m_gesture_info.first_point.x;
							int delta_y = window->m_gesture_info.second_point.y - window->m_gesture_info.first_point.y;

							window->m_gesture_info.overflow_y = (window->m_gesture_info.view_rect.y + window->m_gesture_info.view_rect.h) - window->m_gesture_info.second_point.y;

							// tell core to pan with the delta movement difference
							window->m_mde_screen->TrigTouchMove(0, window->m_gesture_info.first_point.x + delta_x, window->m_gesture_info.first_point.y + delta_y, 5, SHIFTKEY_NONE, NULL);

							// OPUpdatePanningFeedback(window->m_hwnd, 0, window->m_gesture_info.overflow_y, gi.dwFlags & GF_INERTIA);
						}
						OPCloseGestureInfoHandle(gesture_handle);
						return 0;
						break;
					}
					default:
						break;
				}
			}
			break;
		}
#endif // TOUCH_EVENTS_SUPPORT

		// These fire when submenus of main menu menus open and close
		// other cases: ?
		case WM_INITMENUPOPUP:
			g_windows_menu_manager->OnMenuShown(wParam, LOWORD(lParam)); // Pos (lParam) is including separators
			return 0;

		case WM_UNINITMENUPOPUP:
			g_windows_menu_manager->OnMenuClosed(wParam);
			return 0;

		case WM_NCCALCSIZE:
			WM_MESSAGE_HANDLED("WM_NCCALCSIZE");
			if (wParam)
			{
				NCCALCSIZE_PARAMS* para = (NCCALCSIZE_PARAMS*)lParam;

				// We only override the non-client border calculations if we actually do not want a title bar
				window->m_mde_screen->Invalidate(MDE_MakeRect(0,0,window->m_mde_screen->m_rect.w, window->m_titlebar_height+window->m_extended_chrome_height), true);
				if(window->m_state != OpWindow::MINIMIZED && window->IsThemed() && !window->GetWantTitlebar() && window->GetFrame())
				{
					WM_MESSAGE_HANDLED("WM_NCCALCSIZE 2");

					RECT frame_sizes = window->GetFrame()->GetNativeFrameSizes();
					BOOL has_persona = g_skin_manager->HasPersonaSkin();
					if(window->GetStyle() == OpWindow::STYLE_DESKTOP && has_persona &&
						!window->m_menubar_visibility && window->m_state != OpWindow::MAXIMIZED)
					{
						para->rgrc[0].left   += frame_sizes.left;
						para->rgrc[0].right  -= frame_sizes.right;
						para->rgrc[0].bottom	-= frame_sizes.bottom;

						RECT r = {0,0,window->m_width, window->m_height};
						InvalidateRect(hWnd, &r, TRUE);
						return 0;
					}
					else if (window->m_titlebar_height)
					{
						WM_MESSAGE_HANDLED("WM_NCCALCSIZE 3");

						para->rgrc[0].left   += frame_sizes.left;
						para->rgrc[0].right  -= frame_sizes.right;
						para->rgrc[0].bottom -= frame_sizes.bottom;

						if( window->m_state == OpWindow::MAXIMIZED && para->rgrc[0].bottom >= GetSystemMetrics(SM_CYSCREEN))
						{
							para->rgrc[0].bottom -= 1;
						}

						RECT r = {0,0,window->m_width, window->m_titlebar_height};
						InvalidateRect(hWnd, &r, TRUE);
						return 0;
					}
				}
			}
			return DefWindowProc(hWnd, message, wParam, lParam);

#ifdef WIDGETS_IME_SUPPORT
		// == Input Methods ========================
		// Vista send WM_INPUTLANGCHANGE instead of WM_INPUTLANGCHANGEREQUEST
		case WM_INPUTLANGCHANGE:
			{
				current_input_language = (HKL) lParam;

				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		case WM_INPUTLANGCHANGEREQUEST:			
				current_input_language = (HKL) lParam;

//				if (wParam & INPUTLANGCHANGE_BACKWARD)
//					g_input_manager->InvokeAction(OpInputAction::ACTION_CHANGE_DIRECTION_TO_LTR);
//				else if (wParam & INPUTLANGCHANGE_FORWARD)
//					g_input_manager->InvokeAction(OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL);

				return DefWindowProc(hWnd, message, wParam, lParam);
		case WM_IME_STARTCOMPOSITION:
			compose_started_by_message = TRUE;
/*			if(compose_started == FALSE)
			{
				if(view->imlistener)
					widgetinfo = view->imlistener->OnStartComposing(&imstring);
				compose_started = TRUE;
				result_was_commited = FALSE;
			}*/
			
			WindowsOpView::OpCreateCaret(hWnd, 1, 15);
			return TRUE;
		case WM_IME_ENDCOMPOSITION:
			{
				if (compose_started == TRUE)
					if(g_im_listener)
						g_im_listener->OnStopComposing(!result_was_commited);
				compose_started = FALSE;
				compose_started_by_message = FALSE;
			}
			WindowsOpView::OpDestroyCaret(hWnd);
			return TRUE;
		case WM_IME_COMPOSITION:
			{
				// result_was_commited may be set to TRUE in a earlier GCS_RESULTSTR which will affect the current state
				// so better set result_was_commited to FALSE here to avoid this situation
				result_was_commited = FALSE;

				HIMC hIMC = ImmGetContext(hWnd);
				if (!hIMC)
					break;

				BOOL compose_interrupted = FALSE;
				if (compose_started_by_message && !compose_started)
				{
					if(g_im_listener)
						widgetinfo = g_im_listener->OnStartComposing(&imstring, IM_COMPOSE_NEW);
					compose_started = TRUE;
					result_was_commited = FALSE;
				}

				// Sometimes some IMEs want to compose something without starting and stopping first.
				BOOL need_temp_start_stop = (compose_started == FALSE);
				static BOOL last_need_temp_start_stop = FALSE;

				if(need_temp_start_stop)
				{
					if(g_im_listener)
						widgetinfo = g_im_listener->OnStartComposing(&imstring, IM_COMPOSE_NEW);
				}

				if (lParam & GCS_RESULTSTR)
				{
					OpString oldstr;
					oldstr.Set(imstring.Get());
					INT32 len = UpdateIMString(hIMC, GCS_RESULTSTR);

                   // NOT NECESSARY ANYMORE! What the korean ime expects is that the current character being removed in WM_IME_ENDCOMPOSITION 
                   // The ime will send a GCS_RESULTSTR message which consist of the removed character and user input character(generally punctuation). 
					//if (need_temp_start_stop && len > 0 && result_was_commited)
					//{
					//	// Some crazy hacks to handle the korean ime (It does things in quite messed up order)
					//	OpString newstr;
					//	newstr.Set(imstring.Get());

					//	if (!last_need_temp_start_stop && oldstr.Length() && newstr.Find(oldstr.CStr()) == 0)
					//	{
					//		newstr.Delete(0, oldstr.Length());
					//		imstring.Set(newstr.CStr(), newstr.Length());
					//	}
					//}

					imstring.SetCaretPos(0);
					imstring.SetCandidate(0, 0);

					if(len > 0 && g_im_listener)
						g_im_listener->OnCommitResult();

					result_was_commited = TRUE;
				}
				if (lParam & GCS_COMPSTR)
				{
					// Get the size of the result string.
					INT32 len = UpdateIMString(hIMC, GCS_COMPSTR);
					
					// Size of previous imstring
					INT32 pre_len = wcslen(imstring.Get());

					if (len > 0)
					{
						// Get compose string
						UINT32 caret_pos = ImmGetCompositionString(hIMC, GCS_CURSORPOS, NULL, 0);
						caret_pos = caret_pos & 0xFFFF;
						imstring.SetCaretPos(caret_pos);

						// Get the position and length of candidate (if active)
						UINT32 attrsize = ImmGetCompositionString(hIMC, GCS_COMPATTR, NULL, 0);
						char* attrbuf = OP_NEWA(char, attrsize);
						if (attrbuf == NULL)
							 break;
						ImmGetCompositionString(hIMC, GCS_COMPATTR, attrbuf, attrsize);
						UINT32 can_pos = 0, can_len = 0;

						// Locate the start of target clause(selection)
						for(; can_pos < attrsize; can_pos++)
						{
							if (attrbuf[can_pos] == ATTR_TARGET_CONVERTED
								|| attrbuf[can_pos] == ATTR_TARGET_NOTCONVERTED)
								break;
						}

						if (can_pos == attrsize)
						{
							// No selection
							can_pos = 0;
							can_len = 0;
						}
						else
						{
							// Locate the end of the selection
							UINT32 can_stop;
							for(can_stop = can_pos; can_stop < attrsize; can_stop++)
							{
								if (attrbuf[can_stop] != ATTR_TARGET_CONVERTED
									&& attrbuf[can_stop] != ATTR_TARGET_NOTCONVERTED)
									break;
							}
							can_len = can_stop - can_pos;
						}

						OP_DELETEA(attrbuf);

						// hack of korean inputs.
						if ( PRIMARYLANGID(current_input_language) == LANG_KOREAN && len == 1)
						{
							// Korean IME should have highlighted character.
							imstring.SetCaretPos(len);
							can_pos = 0;
							can_len = len;
						}

						imstring.SetCandidate(can_pos, can_len);
					}
					else
					{
						imstring.SetCaretPos(0);
						imstring.SetCandidate(0, 0);

						// Hack for chinese inputmethods on win98. It has compositionmode still open when switching
						// to another IME then continuing it when switching back.
						compose_interrupted = TRUE;
					}

					// Inform listener
					// some IMEs keep sending WM_IME_COMPOSITION with empty IMString,those IME are composing character themselves but don't want the application 
					// to know the details.e.g. Chinese IME jidianwubi. This behaviour will cause the selected text to be unselected instead of deleted in 
					// OnCompose. So just holding this message will simply fix this.
					if ( pre_len !=0 || len != 0 )
					{
						if(g_im_listener)
							widgetinfo = g_im_listener->OnCompose();
					}
				}

				last_need_temp_start_stop = need_temp_start_stop;
				if (need_temp_start_stop || compose_interrupted)
				{
					if(g_im_listener)
						g_im_listener->OnStopComposing(compose_interrupted);
					if (compose_interrupted)
					{
						compose_started = FALSE;
					}
				}
				// Update the position of the candidate window
				// Doing it in WM_IME_NOTIFY when wParam == IMN_OPENCANDIDATE wasn't enough for the chineese IME.
				UpdateIMEWindows(hWnd, hIMC);

				ImmReleaseContext(hWnd, hIMC);
			}
			// Shouldn't be called if we want to display the composestring by ourselves.
			// return DefWindowProc(hWnd,  message,  wParam,  lParam);
			return FALSE;

			case WM_POWERBROADCAST:
			{
				switch(wParam)
				{
					case PBT_POWERSETTINGCHANGE: // we got a notification for change in the power state
						if (WindowsTimerResolutionManager::GetInstance())
							WindowsTimerResolutionManager::GetInstance()->UpdatePowerStatus();
						return TRUE;

					case PBT_APMSUSPEND:
						s_is_power_suspend_state = TRUE;
						break;

					case PBT_APMRESUMEAUTOMATIC:
						s_is_power_suspend_state = FALSE;
						break;
				}
			}
			break;

		case WM_IME_CHAR:
			return FALSE;
		case WM_IME_CONTROL:
			return FALSE;

		case WM_IME_SETCONTEXT:
			if (compose_started)
			{
				WindowsOpView::AbortInputMethodComposing(hWnd);
				return FALSE;
			}
			else
				return DefWindowProc(hWnd,  message,  wParam,  lParam);

		case WM_IME_NOTIFY:
			if (wParam==IMN_OPENCANDIDATE)
			{
				if (g_im_listener)
					g_im_listener->OnCandidateShow(TRUE);
			}

			if (wParam==IMN_CLOSECANDIDATE)
			{
				if (g_im_listener)
					g_im_listener->OnCandidateShow(FALSE);
			}

			return DefWindowProc(hWnd,  message,  wParam,  lParam);
		
#endif //WIDGETS_IME_SUPPORT

		case WM_DWMCOMPOSITIONCHANGED:
			WM_MESSAGE_HANDLED("WM_DWMCOMPOSITIONCHANGED");
			if (!s_is_power_suspend_state)
				window->OnPagebarMoved(window->m_requested_titlebar_height>0, window->m_requested_extended_chrome_height);
			break;

		case WM_THEMECHANGED:
		{
			WM_MESSAGE_HANDLED("WM_THEMECHANGED");
			if(window->m_background_clearer)
				g_skin_manager->RemoveTransparentBackgroundListener(window->m_background_clearer);
			OP_DELETE(window->m_background_clearer);
			window->m_background_clearer = NULL;
			window->SetIsThemed(FALSE);
			HTHEME th = OpenThemeData(window->m_hwnd, UNI_L("window"));
			if (th)
			{
				window->SetIsThemed(TRUE);
				CloseThemeData(th);
			}
			CloseThemeData(g_window_theme_handle);
			g_window_theme_handle = NULL; // Will be initialized when required

			window->OnPagebarMoved(window->m_requested_titlebar_height>0, window->m_requested_extended_chrome_height);
			g_main_message_handler->RemoveDelayedMessage(MSG_QUICK_RELOAD_SKIN,0,0);
			g_main_message_handler->PostDelayedMessage(MSG_QUICK_RELOAD_SKIN, 0, 0, 1000);
			return 0;
		}
		case WM_NCHITTEST:
		{
			LRESULT res = HTNOWHERE;

			if (window->GetStyle() == OpWindow::STYLE_TOOLTIP)
				return HTTRANSPARENT;
			if (window->IsDialog()
				&& window->m_maximum_width > window->m_minimum_width
				&& window->m_maximum_height > window->m_minimum_height)
			{
				RECT rect;
				GetWindowRect(hWnd, &rect);

				INT32 x = GET_X_LPARAM(lParam);
				INT32 y = GET_Y_LPARAM(lParam);

				if (x >= rect.right - 10 && y >= rect.bottom - 10)
					return HTBOTTOMRIGHT;
			}

			LRESULT on_border = window->IsCursorOnBorder();
			if (on_border != HTNOWHERE && (window->GetStyle() == OpWindow::STYLE_DESKTOP || window->IsDialog()))
				return on_border;
			else if (window->IsCursorInChrome())
			{
				// If the DWM is rendering the caption border controls, we need to give it a chance
				// to handle the hit testing first
				if (!OPDwmDefWindowProc(hWnd, WM_NCHITTEST, wParam, lParam, &res))
				{
					RECT frame_sizes = window->GetFrame()->GetNativeFrameSizes();
					BOOL composition = FALSE;
					OPDwmIsCompositionEnabled(&composition);

					res = DefWindowProc(hWnd, WM_NCHITTEST, wParam, lParam);
					if (res == HTCLIENT)
					{
						res = HTCAPTION;
						POINT point;
						GetCursorPos(&point);
						ScreenToClient(hWnd, &point);
						if (point.y < min(frame_sizes.top, MAX_WINDOW_BORDER_TOP))
							res = HTTOP;
					}
					else if (composition && (res == HTCLOSE || res == HTMINBUTTON || res == HTMAXBUTTON))
						res = HTCAPTION;

					if (res != HTCLIENT && window->m_background_clearer)
					{
						// The presence of background clearer means we either have
						// Personas or we have XP engine on, but no Aero/DWM available.
						//
						// In both cases we want to have the buttons handled by the Quick
						// code, not the OS, inside OpBorderButtons code.
						//
						switch (res)
						{
						case HTMINBUTTON:
						case HTMAXBUTTON:
						case HTCLOSE:
							res = HTCLIENT;
						};
					}
				}
				return res;
			}

			RECT crect;
			POINT cpoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (GetClientRect(hWnd, &crect) && ScreenToClient(hWnd, &cpoint))
			{
				if (cpoint.x >= crect.left &&
					cpoint.y >= crect.top &&
					cpoint.x <= crect.right &&
					cpoint.y <= crect.bottom)
					return HTCLIENT;
			}
			break;
		}
		case WM_OPERA_DRAG_GET_SUGGESTED_FILENAME:
		{
			DNDGetSuggestedFilenameData *data = reinterpret_cast<DNDGetSuggestedFilenameData *>(wParam);

			// let's create a valid filename
			URL url = g_url_api->GetURL(data->file_url);
			OpString tmp;
			TRAPD(s, url.GetAttributeL(URL::KSuggestedFileName_L, tmp, TRUE));
			if(OpStatus::IsSuccess(s))
			{
				// strip illegal characters
				if(!g_desktop_op_system_info->RemoveIllegalFilenameCharacters(tmp, TRUE))
					s = OpStatus::ERR;
				else
					s = data->destination.AppendFormat("%s.url", tmp.CStr());
			}
			if(OpStatus::IsError(s))
			{
				OpStatus::Ignore(data->destination.Set("unknown.url"));	// we give up
			}
			return 1;
			break;
		}
		case WM_OPERA_DRAG_COPY_FILE_TO_LOCATION:
		{
			CopyImageFileToLocationData *data = reinterpret_cast<CopyImageFileToLocationData *>(wParam);
			OpString suggested_filename;
			URL url = g_url_api->GetURL(data->file_url);
			if(!url.IsEmpty())
			{
				data->image = UrlImageContentProvider::GetImageFromUrl(url);

				TRAPD(err, url.GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));
				if(OpStatus::IsError(err))
				{
					return 0;
				}
			}
			// copy it from the cache filename to a temporary directory
			OpFile file;
			if(OpStatus::IsError(file.Construct(suggested_filename, OPFILE_TEMPDOWNLOAD_FOLDER)) ||
				OpStatus::IsError(data->destination.Set(file.GetFullPath())) ||
				OpStatus::IsError(file.Delete(FALSE)) ||
				OpStatus::IsError(url.SaveAsFile(data->destination)))
			{
				return 0;
			}
			return 1;
			break;
		}

		case WM_OPERA_DRAG_CAME_BACK:
		{
			OP_ASSERT(g_drag_manager->IsDragging());

			// drag came back from having been outside Opera
			WindowsOpWindow* win = WindowsOpWindow::FindWindowUnderCursor();
			if(win)
			{
				win->m_mde_screen->TrigDragEnter((int)wParam, (int)lParam, g_op_system_info->GetShiftKeyState());
				g_drag_data->dnd_thread.Finish();
			}
			return TRUE;
		}
		case WM_OPERA_DRAG_UPDATE:
		{
			g_windows_drag_manager->UpdateDrag(window);
			return TRUE;
		}
		case WM_OPERA_DRAG_DROP:
		{
			WindowsOpWindow* win = WindowsOpWindow::FindWindowUnderCursor();
			DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(g_drag_manager->GetDragObject());
			if (win && drag_object)
			{
				DesktopWindow* target_win = win->GetDesktopWindow();
				DocumentDesktopWindow* target_tab = target_win && target_win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER ? 
							static_cast<BrowserDesktopWindow *>(target_win)->GetActiveDocumentDesktopWindow() : NULL;
				OpWidget *widget = NULL;
				if(target_tab && !drag_object->IsDropAvailable())
				{
					widget = target_tab->GetWidgetByType(OpTypedObject::WIDGET_TYPE_BROWSERVIEW);
					if(!widget)
						widget = target_tab->GetWidgetByType(OpTypedObject::WIDGET_TYPE_BOOKMARKS_VIEW);

				}
				if(target_win && widget)
				{
					// get the source widget so we can let the checks inside OpDragDrop work
					DesktopWindow *source = g_windows_drag_manager->GetSourceDesktopWindow();
					if(source)
					{
						OpWidget* src_widget = source->GetWidgetByType(OpTypedObject::WIDGET_TYPE_BROWSERVIEW);
						if(!src_widget)
							src_widget = source->GetWidgetByType(OpTypedObject::WIDGET_TYPE_BOOKMARKS_VIEW);
						if(src_widget)
							drag_object->SetSource(src_widget);
					}
					// call the desktop window's drop method directly instead
					target_win->OnDragDrop(widget, drag_object, 0, 0, 0);
				}
				// poor mans fallback just in case
				else if (target_win && (target_win != g_windows_drag_manager->GetSourceToplevelWindow() || 
					drag_object->GetType() == OpTypedObject::DRAG_TYPE_BOOKMARK || drag_object->GetType() == OpTypedObject::UNKNOWN_TYPE))
				{
					if (!drag_object->IsDropAvailable() && (drag_object->GetType() == OpTypedObject::DRAG_TYPE_BOOKMARK || 
						drag_object->GetType() == OpTypedObject::DRAG_TYPE_LINK || drag_object->GetType() == OpTypedObject::UNKNOWN_TYPE) &&
						DragDrop_Data_Utils::HasURL(drag_object))
					{
						if(drag_object->GetURLs().GetCount() > 0 )
						{
							const OpVector<OpString>& url_list = drag_object->GetURLs();
							for (UINT32 i = 0; i < url_list.GetCount(); i++)
							{
								g_application->GoToPage(url_list.Get(i)->CStr(), FALSE, FALSE, FALSE, target_win);
							}
						}
					}
				}
				POINT point;
				GetCursorPos(&point);

				ScreenToClient(win->m_hwnd, &point);

				// hide the drag window so it's not still visible if core decides 
				// to show a dialog or something similar before the drop has ended
				g_windows_drag_manager->HideDragWindow();

				win->m_mde_screen->TrigDragDrop(point.x, point.y, g_op_system_info->GetShiftKeyState());
			}
			return TRUE;
		}
		case WM_OPERA_DRAG_ENTER:
		{
			POINT point;
			GetCursorPos(&point);

			WindowsOpWindow* win = WindowsOpWindow::FindWindowUnderCursor();
			if(win)
			{
				ScreenToClient(win->m_hwnd, &point);

				win->m_mde_screen->TrigDragEnter(point.x, point.y, g_op_system_info->GetShiftKeyState());
			}
			return TRUE;
		}
		case WM_OPERA_DRAG_LEAVE:
		{
			// we can get this after an external drop so make sure we're actually dragging
			// before calling core
			if(g_drag_manager->IsDragging())
			{
				WindowsOpWindow* win = WindowsOpWindow::FindWindowUnderCursor();
				if(win)
				{
					win->m_mde_screen->TrigDragLeave(g_op_system_info->GetShiftKeyState());
				}
			}
			return TRUE;
		}
		case WM_OPERA_DRAG_MOVED:
		{
			WindowsOpWindow* win = WindowsOpWindow::FindWindowUnderCursor();
			if (g_drag_data && !g_drag_data->dnd_thread.IsRunning() && !win)
			{
				// Outside Opera window start system d'n'd handling.  Treat window decoration as external.
				OpString origin_url;
				DesktopWindow *win = g_application->GetActiveDocumentDesktopWindow();
				if(win && win->GetWindowCommander())
				{
					OpStatus::Ignore(origin_url.Set(win->GetWindowCommander()->GetCurrentURL(FALSE)));
				}
				g_drag_data->dnd_thread.Start(window->m_mde_screen, origin_url);
			}
			else if(win && win->m_hwnd && win->m_mde_screen)
			{
				POINT screen_point;
				::GetCursorPos(&screen_point);
				ScreenToClient(win->m_hwnd, &screen_point);

				win->m_mde_screen->TrigDragMove(screen_point.x, screen_point.y, g_op_system_info->GetShiftKeyState());
			}
			return TRUE;
		}
		case WM_OPERA_DRAG_ENDED:
		// Fall through.

		case WM_OPERA_DRAG_CANCELLED:
		{
			POINT screen_point;

			GetCursorPos(&screen_point);
			POINT client_point = screen_point;
			ScreenToClient(window->m_hwnd, &client_point);

			if(g_drag_data)
			{
				g_drag_data->drag_x = -1;
				g_drag_data->drag_y = -1;
				g_drag_data->drag_acknowledged = false;
			}
			if(g_drag_manager->IsDragging())
			{
				g_windows_drag_manager->HideDragWindow();

				if(message == WM_OPERA_DRAG_ENDED)
				{
					DesktopDragObject* obj = static_cast<DesktopDragObject*>(g_drag_manager->GetDragObject());
					if (obj->GetType() == OpTypedObject::DRAG_TYPE_WINDOW && !obj->IsDropAvailable())
					{
						for (INT32 i = 0; i < obj->GetIDCount(); i++)
						{
							DesktopWindow* dw = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(obj->GetID(i));
							if (dw)
							{
								DesktopWindow *toplevel = dw->GetToplevelDesktopWindow();

								OpRect top_level_rect;
								toplevel->GetAbsoluteRect(top_level_rect);
								top_level_rect.x += 20;
								top_level_rect.y += 20;

								dw->SetParentWorkspace(NULL);
								dw->GetToplevelDesktopWindow()->SetOuterPos(top_level_rect.x, top_level_rect.y);
							}
						}
					}
					window->m_mde_screen->TrigDragEnd(client_point.x, client_point.y, g_op_system_info->GetShiftKeyState());
				}
				else
					window->m_mde_screen->TrigDragCancel(g_op_system_info->GetShiftKeyState());
			}
			return TRUE;
		}
		case WM_OPERA_DRAG_EXTERNAL_ENTER:
		{
			if(!g_drag_manager->IsDragging())
			{
				// we don't yet have a Opera drag object, create one now
				OpDragObject* op_drag_object;
				if (OpStatus::IsError(OpDragObject::Create(op_drag_object, OpTypedObject::UNKNOWN_TYPE)))
					return NULL;

				g_drag_manager->StartDrag(op_drag_object, NULL, FALSE);

				WindowsOpWindow* win = WindowsOpWindow::FindWindowUnderCursor();
				if(win)
				{
					POINT point;
					GetCursorPos(&point);

					ScreenToClient(win->m_hwnd, &point);

					win->m_mde_screen->TrigDragEnter(point.x, point.y, g_op_system_info->GetShiftKeyState());
				}
			}
			return TRUE;
		}
		case WM_OPERA_DRAG_EXTERNAL_CANCEL:
		case WM_OPERA_DRAG_EXTERNAL_DROPPED:
		{
			WindowsOpWindow* win = WindowsOpWindow::FindWindowUnderCursor();
			if(!win)
			{
				// we need some window to send the event to
				BrowserDesktopWindow* bw = g_application->GetActiveBrowserDesktopWindow();  
				if(bw)
				{
					win = static_cast<WindowsOpWindow*>(bw->GetOpWindow());
				}
			}
			g_drag_data->drag_x = -1;
			g_drag_data->drag_y = -1;
			g_drag_data->drag_acknowledged = false;

			POINT point;

			g_windows_drag_manager->HideDragWindow();

			GetCursorPos(&point);
			if(win)
			{
				ScreenToClient(win->m_hwnd, &point);
				if(message == WM_OPERA_DRAG_EXTERNAL_CANCEL)
				{
					win->m_mde_screen->TrigDragCancel(g_op_system_info->GetShiftKeyState());
				}
				else
				{
					win->m_mde_screen->TrigDragEnd(point.x, point.y, g_op_system_info->GetShiftKeyState());
				}

				if (s_left_down)
				{
					win->m_mde_screen->TrigMouseUp(point.x, point.y, 1, g_op_system_info->GetShiftKeyState());
				}
			}
			else
			{
				// last resort
				g_drag_manager->StopDrag(message == WM_OPERA_DRAG_EXTERNAL_CANCEL);
			}
			g_drag_data->dnd_thread.Finish();
			s_left_down = FALSE;
			return TRUE;
		}

		case WM_MOUSEACTIVATE:
		{
			if (window->GetStyle() == OpWindow::STYLE_POPUP || window->GetStyle() == OpWindow::STYLE_TOOLTIP || window->GetStyle() == OpWindow::STYLE_NOTIFIER /*|| window->m_style == OpWindow::STYLE_CHILD*/)
				return MA_NOACTIVATE;

			if (window->m_parent_window && window->m_parent_window->GetStyle() == OpWindow::STYLE_WORKSPACE)
			{
				window->Activate(!window->m_parent_window->m_workspace_minimized_visibility);
				return MA_ACTIVATE;
			}

			break;
		}

		case WM_MOUSEMOVE:
		{
			if (window->m_parent_view && window->GetStyle() == OpWindow::STYLE_WORKSPACE)
			{
				POINT point;

				point.x = GET_X_LPARAM(lParam);
				point.y = GET_Y_LPARAM(lParam);
				return 0;
			}
			break;
		}

		case WM_DRAWITEM:
			return ucWMDrawItem(hWnd,message,wParam,lParam);

		case WM_MEASUREITEM:
			return ucMeasureItem(hWnd,message,wParam,lParam);

		case WM_MENUCHAR:
			return g_windows_menu_manager ? g_windows_menu_manager->OnMenuChar(LOWORD(wParam), (HMENU) lParam) : 0;

		case WM_GETMINMAXINFO:
		{
			MINMAXINFO* info = (MINMAXINFO*) lParam;

			RECT r;

			r.left = 0;
			r.top = 0;
			r.right = window->m_minimum_width;
			r.bottom = window->m_minimum_height;

			AdjustWindowRectEx(&r,window->m_win_style,0,window->m_win_style_ex);

			int border_x = (r.right - r.left) - window->m_minimum_width + window->m_left_margin + window->m_right_margin;
			int border_y = (r.bottom - r.top) - window->m_minimum_height + window->m_top_margin + window->m_bottom_margin;

			info->ptMinTrackSize.x = window->m_minimum_width + border_x;
			info->ptMinTrackSize.y = window->m_minimum_height + border_y;

			if (window->m_fullscreen)
			{
				info->ptMaxTrackSize.x += border_x;
				info->ptMaxTrackSize.y += border_y;
			}
			else
			{
				if (window->m_maximum_width != -1)
				{
					info->ptMaxTrackSize.x = window->m_maximum_width + border_x;
				}
		
				if (window->m_maximum_height != -1)
				{
					info->ptMaxTrackSize.y = window->m_maximum_height + border_y;
				}
			}

			if (window->m_parent_window && window->m_parent_window->GetStyle() == OpWindow::STYLE_WORKSPACE)
			{
				RECT rect;

				GetClientRect(window->m_parent_window->m_hwnd, &rect);
				AdjustWindowRectEx(&rect, window->m_win_style, 0, window->m_win_style_ex);

				info->ptMaxSize.x = rect.right - rect.left;
				info->ptMaxSize.y = rect.bottom - rect.top;
				info->ptMaxPosition.x = rect.left;
				info->ptMaxPosition.y = rect.top;

				if (info->ptMaxSize.x > info->ptMaxTrackSize.x)
					info->ptMaxTrackSize.x = info->ptMaxSize.x;

				if (info->ptMaxSize.y > info->ptMaxTrackSize.y)
					info->ptMaxTrackSize.y = info->ptMaxSize.y;
			}

			break;
		}

		case WM_SETFOCUS:
		{
			break;
		}

		case WM_ENTERSIZEMOVE:
		{
			window->m_sizemove = TRUE;
			break;
		}

		case WM_EXITSIZEMOVE:
		{
			window->m_sizemove = FALSE;
			g_input_manager->ResetInput();
			// When resizing is in progress the window is painted in low quality for performance,
			// so redraw the window in high quality when exiting resizing
			::InvalidateRect(hWnd, NULL, FALSE);
			break;
		}

		case WM_SIZE:
		{
			window->Resize();
			break;
		}

		case WM_MOVE:
		{
			if(window->m_window_listener)
				window->m_window_listener->OnMove();
			break;
		}

		case WM_COMMAND:
		{
			return g_windows_menu_manager && g_windows_menu_manager->OnMenuCommand(LOWORD(wParam));
		}
		case WM_NCACTIVATE:
		{
			WM_MESSAGE_HANDLED("WM_NCACTIVATE");

			if (window->GetMenuBar())
				window->GetMenuBar()->InvalidateWindow();

			// FIXME: might not have to invalidate everything
			window->m_hwnd_active = (BOOL)wParam;
			if (window->m_background_clearer)
			{
				// ignore any default processing and drawing of the title bar done by Windows when a persona is active
				BOOL ignore_default_processing = g_skin_manager->HasPersonaSkin();
				LRESULT res = ignore_default_processing ? TRUE : DefWindowProc(hWnd, message, wParam, lParam);
				if (window->m_titlebar_height)
				{
					RECT frame_sizes = window->GetFrame()->GetNativeFrameSizes();
					BOOL title_valid = TRUE;
					int title_height = frame_sizes.top + GetSystemMetrics(SM_CYCAPTION);
					for (int i = 0; i < window->m_mde_screen->m_invalid_region.num_rects; ++i)
					{
						if (window->m_mde_screen->m_invalid_region.rects[i].y < title_height)
							title_valid = FALSE;
					}
					if (title_valid || window->m_sizemove)
					{
						window->m_mde_screen->Validate(false);
					}
				}

				window->m_mde_screen->Invalidate(MDE_MakeRect(0,0,window->m_mde_screen->m_rect.w, window->m_mde_screen->m_rect.h), true);
				return res;
			}
			break;
		}
		case WM_ACTIVATEAPP:
		{
			if(g_drag_manager->IsDragging())
			{
				BOOL activate = (BOOL)wParam;
				if (!activate && g_drag_data && !g_drag_data->dnd_thread.IsRunning())
				{
					// Outside Opera window start system d'n'd handling.  Treat window decoration as external.
					OpString origin_url;
					DesktopWindow *win = g_application->GetActiveDocumentDesktopWindow();
					if(win && win->GetWindowCommander())
					{
						OpStatus::Ignore(origin_url.Set(win->GetWindowCommander()->GetCurrentURL(FALSE)));
					}
					g_drag_data->dnd_thread.Start(window->m_mde_screen, origin_url);
				}
			}
			return 0;
			break;
		}
		case WM_ACTIVATE:
		{
			// When minimizing a window, Windows send 2 messages
			// 1 WM_ACTIVATE with active(LOWORD(wParam))=0 and minimized(HIWORD(wParam))=0
			// 2 WM_ACTIVATE with active = 1 and minimized = 1
			// We treated the 2nd message as a activation message 
			// but it's not, just ignore.
			if (LOWORD(wParam) && HIWORD(wParam))
				return 0;

			//which window is being activated now?
			//if we get this because we are activated, it's this window,
			//otherwise, it's the handle carried in lParam
			WindowsOpWindow::ClosePopup(LOWORD(wParam) == WA_INACTIVE ? (HWND)lParam : hWnd);

			window->Out();
			window->m_flash_count = 0;
			window->m_inverted = FALSE;

			if(window->m_window_listener && !window->m_is_closing)
			{
				BOOL activate = LOWORD(wParam) != WA_INACTIVE;

				window->m_window_listener->OnActivate(activate);

				// Must repaint the windowborders of the active childwindow.
				BrowserDesktopWindow* desktop = g_application ? g_application->GetActiveBrowserDesktopWindow() : NULL;
				if (!window->m_parent_window && desktop && desktop->GetWorkspace() && desktop->GetWorkspace()->GetOpWindow())
				{
					HWND workspace = static_cast<WindowsOpWindow*>(desktop->GetWorkspace()->GetOpWindow())->m_hwnd;
					HWND child = GetWindow(workspace, GW_CHILD);
					while (child)
					{
						WindowsOpWindow *child_window = WindowsOpWindow::GetWindowFromHWND(child);

						if (child_window && child_window->m_active)
						{
							SendMessage(child_window->m_hwnd, WM_NCACTIVATE, activate, 0);
							break;
						}

						child = GetWindow(child, GW_HWNDNEXT);
					}
				}
			}
			return 0;
		}
		case WM_ERASEBKGND:
		{
			return TRUE;
		}
		case WM_CLOSE:
		{
			if (window->GetIgnoreNextCloseMsg())
			{
				window->SetIgnoreNextCloseMsg(FALSE);
				return 0;
			}

			if (window->m_user_initiated_close && window->m_window_listener && !window->m_window_listener->OnIsCloseAllowed())
			{
				return 0;
			}

			if (window->m_parent_has_been_disabled && window->m_parent_window && window->IsModalDialog())
			{
				window->m_parent_window->Enable();
				window->m_parent_has_been_disabled = FALSE;
			}
			break;
		}
		case WM_WINDOWPOSCHANGING:
		{
			if (window->m_keep_below && IsSystemWinVista())
			{
				WINDOWPOS *new_pos = (WINDOWPOS*)lParam;
				new_pos->hwndInsertAfter = HWND_BOTTOM;
			}
			break;
		}
		case WM_APPCOMMAND:
			{
				OpWindow* optwin = window;

				//we know that the parent we are looking for has style adopt, ref EmbeddedDocDesktopWindow
				while(optwin)
				{
					if(optwin->GetStyle() == OpWindow::STYLE_ADOPT)
						break;
					if(((WindowsOpWindow*)optwin)->m_parent_view)
						optwin = (OpWindow*) ((MDE_OpView*)((WindowsOpWindow*)optwin)->m_parent_view)->GetParentWindow();
					else
						optwin = (OpWindow*) ((WindowsOpWindow*)optwin)->m_parent_window;
				}

				// if style is OpWindow::STYLE_ADOPT all children of this use this Option set
				if(optwin && (((WindowsOpWindow*)optwin)->GetOptions() & WindowsOpWindow::opWindowDisableAppkeys) )
				{
					return TRUE;
				}

				if (g_windows_message_loop->OnAppCommand(lParam))
					return TRUE;
				else
					return DefWindowProc(hWnd,  message,  wParam,  lParam);
			}


		case WM_NCPAINT:
		{
			WM_MESSAGE_HANDLED("WM_NCPAINT 1");

			window->EnsureHiddenIconicState();

			// Only do non-client painting if we're using a non-native frame as this
			// stops native painting artifacts from showing up when resizing the window
			if (window->m_background_clearer && window->m_titlebar_height)
			{
				WM_MESSAGE_HANDLED("WM_NCPAINT 2");

				RECT wr;
				GetWindowRect(window->m_hwnd, &wr);
				RECT frame_sizes = window->GetFrame()->GetNativeFrameSizes();
				int bottom_border = frame_sizes.bottom;
				int vborder = frame_sizes.left;
				if (!bottom_border && !vborder)
					return 0; // nothing to draw, no non-client area
				HRGN cliprgn = CreateRectRgn(wr.left+vborder, wr.top, wr.right-vborder, wr.bottom-bottom_border);
				HRGN oldclip = (HRGN)wParam;
				// Special case, wParam == 1 means the entire non-client area
				if (wParam == 1)
				{
					oldclip = CreateRectRgn(wr.left, wr.top, wr.right, wr.bottom);
				}
				RECT dirty;
				GetRgnBox(oldclip, &dirty);
				CombineRgn(cliprgn, oldclip, cliprgn, RGN_DIFF);
				if (oldclip != (HRGN)wParam)
					DeleteObject(oldclip);
				LRESULT res = DefWindowProc(hWnd, message, (WPARAM)cliprgn, lParam);
				DeleteObject(cliprgn);

				BOOL title_valid = TRUE;
				int title_height = frame_sizes.top + GetSystemMetrics(SM_CYCAPTION);
				// Only repaint title if required
				if (dirty.top-wr.top < title_height)
				{
					for (int i = 0; i < window->m_mde_screen->m_invalid_region.num_rects; ++i)
					{
						if (window->m_mde_screen->m_invalid_region.rects[i].y < title_height)
							title_valid = FALSE;
					}
					if (title_valid || window->m_sizemove)
					{
						window->m_mde_screen->Validate(false);
					}
				}
				return res;
			}
			break;
		}
		case WM_NCUAHDRAWCAPTION:
		case WM_NCUAHDRAWFRAME:
			WM_MESSAGE_HANDLED("WM_NCUAHDRAWCAPTION or WM_NCUAHDRAWFRAME");
			if (window->m_background_clearer && window->m_titlebar_height && window->m_background_clearer->IsClearingBackground())
      			return 0;
			break;

        case WM_DESTROY:
		{
			HWND focus = GetFocus();

			if ((/*!focus || */focus == hWnd) && GetParent(hWnd))
			{
				SetFocus(GetParent(hWnd));
			}

			window->m_is_closing = TRUE;

			if (window->GetMDEWidget() && window->GetMDEWidget()->m_parent)
			{
				window->GetMDEWidget()->m_parent->RemoveChild(window->GetMDEWidget());
			}
			if(g_windows_menu_manager && window->m_menubar)
				g_windows_menu_manager->DestroyMenuBar(window);

			SetWindowLongPtr(hWnd, GWLP_USERDATA, NULL);

			if(window->m_window_listener)
				window->m_window_listener->OnClose(window->m_user_initiated_close);

			g_windows_message_loop->EndDialog(window);

            return 0L;
		}
		// display resolution has changed, we need to notify the application
		case WM_DISPLAYCHANGE:
			g_application->SettingsChanged(SETTINGS_DISPLAY);
			break;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		//an application requested to get access to the MSAA objects
		case WM_GETOBJECT:
			if(!g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::EnableAccessibilitySupport))
				break;

			//requesting the accessible object corresponding client area when asked
			if (lParam == OBJID_CLIENT && window->m_accessible_item && !window->m_is_closing)
			{
				if (!window->m_accessibility_inited)
				{
					if (g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::UseScreenReaderMenus) && g_windows_menu_manager)
						g_windows_menu_manager->UseScreenReaderMenus(TRUE);

					((WindowsOpSystemInfo*)g_op_system_info)->SetScreenReaderRunning();

					// prefs might override this
					if(!((WindowsOpSystemInfo*)g_op_system_info)->IsScreenReaderRunning())
						break;

					window->m_accessibility_inited = TRUE;
				}

				IAccessible2* client_acc = (WindowsOpAccessibilityAdapter*)AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(window->m_accessible_item);
				if (client_acc)
					return LresultFromObject(IID_IAccessible, wParam, (LPUNKNOWN)(client_acc));
			}
			//requesting an accessible object having sent an event
			if (lParam > 0 && !window->m_is_closing)
			{
#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
				OutputDebugString(UNI_L("Request for an object that sent an event: "));
#endif
				IAccessible2* event_acc = (WindowsOpAccessibilityAdapter*)AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem((OpAccessibleItem*)lParam, 0);
				if (event_acc)
				{
#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
					OpString dbg_info;
					evt_acc->GetDebugInfo(dbg_info);
					OutputDebugString(dbg_info.CStr());
					OutputDebugString(UNI_L("\n"));
#endif
					return LresultFromObject(IID_IAccessible, wParam, (LPUNKNOWN)(event_acc));
				}
#if defined _DEBUG && defined ACC_DEBUG_OUTPUT
				else
					OutputDebugString(UNI_L("Not found\n"));
#endif
			}
			break; 
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
		case WM_OPERA_MDE_RENDER:
			window->Validate(true);
			break;

		case WM_OPERA_RESIZE_FRAME:
			WM_MESSAGE_HANDLED("WM_OPERA_RESIZE_FRAME");
			SetWindowPos(window->m_hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
			break;

		case WM_SETCURSOR:
			{
				POINT point;
				GetCursorPos(&point);
				ScreenToClient(hWnd, &point);
				MDE_View* v = window->m_mde_screen->GetViewAt(point.x, point.y, true);
				WindowsOpView* opview = NULL;
				while (v)
				{
					if (v->IsType("MDE_Widget"))
					{
						opview = (WindowsOpView*)((MDE_Widget*)v)->GetOpView();
						break;
					}
					v = v->m_parent;
				}
				if (opview && opview->mouseListener && window->IsEnabled() && !window->IsCursorInChrome())
				{
					return TRUE;
				}
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

OpView* WindowsOpWindow::GetFirstChildView()
{
#ifdef GADGET_SUPPORT
	if( IsThisAGadget() )
    {
        return GetBottomGadgetOPView();
    }
#endif // GADGET_SUPPORT
	return NULL;
}

void WindowsOpWindow::Validate(bool clear_pending)
{
	if (clear_pending)
		m_pending_mde_render = FALSE;

	if (!m_width || !m_height)
		return;

	m_vega_window->TestLayeredWindow();

	if (!m_is_hidden && m_state != OpWindow::MINIMIZED
		// when showing desktop(Win+D), we post and receive validate message earlier than 
		// WM_WINDOWPOSCHANGED arrives, avoid painting in this case.DSK-345434
		&& !IsIconic(m_hwnd))
	{
		if (m_mde_screen->m_is_validating)
		{
			OP_ASSERT(!"You should never get a call to Validate when we are already validating!");
			return;
		}
		m_mde_screen->Validate(true);
	}
}

OP_STATUS DesktopOpWindow::Create(DesktopOpWindow** new_opwindow)
{
	OP_ASSERT(new_opwindow != NULL);
	*new_opwindow = OP_NEW(WindowsOpWindow, ());
	if(*new_opwindow == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

OP_STATUS WindowsOpWindow::Init(Style style, OpTypedObject::Type type, OpWindow* parent_window, OpView* parent_view, void* native_handle, UINT32 effect)
{
	RETURN_IF_ERROR(WindowsOpWindow::StaticInitialization());

	m_style				= style;
	m_type				= type;
	m_effects			= effect;
	m_parent_window		= (WindowsOpWindow*) parent_window;
	m_parent_view		= parent_view;

	m_window_listener	= 0;
	m_is_closing		= FALSE;
	m_locked_by_user	= FALSE;
	m_ignore_next_close_msg = FALSE;
#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
	m_taskbar_message_listener = NULL;
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

#ifndef VEGAOPPAINTER_MDI
	if (!native_handle && (parent_view || parent_window) && 
		(style == STYLE_DESKTOP || style == STYLE_CHILD || style == STYLE_EMBEDDED || style == STYLE_WORKSPACE || style == STYLE_TRANSPARENT))
	{
		return MDE_OpWindow::Init(style, type, parent_window, parent_view, NULL, effect);
	}
#endif // VEGAOPPAINTER_MDI

	// whatever styles that need to have ws_ex_layered are created as mde window
	if (!SupportsExternalCompositing() &&
		!native_handle &&
		(parent_view || parent_window) &&
		m_effects & EFFECT_TRANSPARENT &&
		(m_style == STYLE_POPUP || 
		m_style == STYLE_EXTENSION_POPUP ||
		m_style == STYLE_TOOLTIP ||
		m_style == STYLE_NOTIFIER))
	{
		m_translate_coordinate = TRUE;
		return MDE_OpWindow::Init(style, type, parent_window, parent_view, NULL, effect);
	}

	if (!native_handle)
	{
		m_vega_window = OP_NEW(WindowsVegaWindow, ());
	}

	if (native_handle)
	{
		m_hwnd = (HWND) native_handle;
		if(style == STYLE_ADOPT)
		{
			OP_STATUS retval = s_adopted_windows.Add(m_hwnd, this);
			OP_ASSERT(retval == OpStatus::OK);
			return retval;
		}
		return OpStatus::OK;
	}

	if (g_application)
	{
		g_application->AddSettingsListener(this);
	}

	switch (style)
	{
		case STYLE_DESKTOP:
			m_win_style_ex = WS_EX_ACCEPTFILES;
			m_win_style = WS_OVERLAPPEDWINDOW;

			if (m_parent_view || m_parent_window)
				m_win_style |= WS_CHILD;
			else if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NoMinMaxButtons))
				m_win_style &= ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX);

			g_skin_manager->AddSkinNotficationListener(this);

			m_frame = OP_NEW(WindowFrame, (this));
			break;

		case STYLE_TOOLTIP:
		case STYLE_NOTIFIER:
			m_win_style_ex = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
			m_win_style = WS_OVERLAPPED | WS_POPUP;
			break;

#ifdef GADGET_SUPPORT
        case STYLE_GADGET:
        case STYLE_HIDDEN_GADGET:
			m_win_style_ex = WS_EX_LAYERED;
            m_win_style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX;
			break;
		
#ifdef DECORATED_GADGET_SUPPORT
		case STYLE_DECORATED_GADGET:
			m_win_style = WS_OVERLAPPEDWINDOW & ~(WS_HSCROLL | WS_VSCROLL);
			if (m_parent_view || m_parent_window)
			{
				m_win_style |= WS_CHILD;
			}
			
			m_win_style_ex = 0;

			break;
#endif // DECORATED_GADGET_SUPPORT

        case STYLE_OVERLAY:
            m_win_style_ex = WS_EX_TOOLWINDOW;
            m_win_style = WS_POPUP;
            break;
#endif // GADGET_SUPPORT

		case STYLE_EXTENSION_POPUP:
			m_win_style_ex =  WS_EX_TOOLWINDOW; 
			m_win_style = WS_OVERLAPPED | WS_POPUP;

			if (s_current_popup_window)
			{				
				PostMessage(s_current_popup_window->m_hwnd, WM_CLOSE, 0, 0);
				s_current_popup_window= NULL;	
			}
			s_current_popup_window = this;

			break;

		case STYLE_POPUP:
			m_win_style_ex = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
			m_win_style = WS_OVERLAPPED | WS_POPUP;

			ClosePopup(NULL);

			if (style == STYLE_POPUP)
				s_popup_window = this;
			break;

		case STYLE_MODAL_DIALOG:
		case STYLE_MODELESS_DIALOG:
		case STYLE_BLOCKING_DIALOG:
			m_win_style_ex = WS_EX_DLGMODALFRAME;
			m_win_style = WS_POPUPWINDOW | WS_OVERLAPPED | WS_DLGFRAME | WS_THICKFRAME;
			break;

		case STYLE_CONSOLE_DIALOG:
			m_win_style_ex = 0;
			m_win_style = WS_OVERLAPPEDWINDOW;
			break;

		case STYLE_CHILD:
		case STYLE_EMBEDDED:
			m_win_style_ex = 0;
			m_win_style = WS_CHILD;
			break;

		case STYLE_WORKSPACE:
			m_win_style_ex = 0;
			m_win_style = WS_CHILD;
			break;
	}

	if (m_effects & EFFECT_TRANSPARENT)
	{
		m_win_style_ex |= WS_EX_LAYERED;
	}

	if (m_win_style_ex & WS_EX_LAYERED)
	{
		m_vega_window->SetIsLayered();
	}

	HWND parenthwnd = NULL;

	if (m_parent_window && m_style != STYLE_POPUP && m_style != STYLE_TOOLTIP && m_style != STYLE_NOTIFIER)
	{
		parenthwnd = m_parent_window->GetParentHWND();

		if (parenthwnd == NULL)
		{
			parenthwnd = g_main_hwnd;
		}
	}
	else if (m_parent_view)
	{
		MDE_OpView* pv = (MDE_OpView*)m_parent_view;
		while (pv->GetParentView())
			pv = pv->GetParentView();
		parenthwnd = pv->GetParentWindow()?(((WindowsOpWindow*)pv->GetParentWindow())->m_hwnd):NULL;
	}

	if (0 == m_win_style && 0 == m_win_style_ex)
		return OpStatus::ERR; // windows styles not set, leave now

	m_active_window_when_created = GetActiveWindow();

	m_win_style |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	m_hwnd = CreateWindowEx(m_win_style_ex, HasDropShadow() ? UNI_L("OpWindowDS") : UNI_L(OPERA_WINDOW_CLASSNAME), NULL, m_win_style, 0, 0, 0, 0, parenthwnd, NULL, hInst, NULL);

	if(m_hwnd == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	if ((m_win_style & (WS_CHILD| WS_POPUP)) == 0 && IsDwmCompositionEnabled())
	{
		UpdateDwmFrameWritingDirection();
	}

	m_mde_screen = OP_NEW(VegaMDEScreen, (0, 0, 0, MDE_FORMAT_BGRA32, m_vega_window, this));
	m_vega_window->Construct(0, 0, m_hwnd, m_mde_screen);
	if (m_mde_screen)
		RETURN_IF_ERROR(m_mde_screen->Init());
	m_vega_window->wm_size(0, 0);
	RETURN_IF_ERROR(MDE_OpWindow::Init(style, type, NULL, NULL, m_mde_screen));
	if (!m_pending_mde_render)
	{
		m_pending_mde_render = TRUE;
		PostMessage(m_hwnd, WM_OPERA_MDE_RENDER, 0, 0);
	}
	HTHEME th = OpenThemeData(m_hwnd, UNI_L("window"));
	if (th)
	{
		m_themed = TRUE;
		CloseThemeData(th);
	}
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

#ifdef TOUCH_EVENTS_SUPPORT
	// We want WM_GESTURE events
	// set up our want / block settings
	DWORD dwPanWant  = GC_PAN_WITH_INERTIA | GC_PAN_WITH_SINGLE_FINGER_VERTICALLY | GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;
	DWORD dwPanBlock = GC_PAN_WITH_GUTTER;

	// set the settings in the gesture configuration
	GESTURECONFIG gc[] = 
		{{ GID_ZOOM, GC_ZOOM, 0 },
		{ GID_PAN, dwPanWant , dwPanBlock}
	};
	OPSetGestureConfig(m_hwnd, 0, 2, gc, sizeof(GESTURECONFIG));
#endif // TOUCH_EVENTS_SUPPORT
	if(m_style == STYLE_DESKTOP)
	{
		g_windows_drag_manager->RegisterDropTarget(m_hwnd);
	}
	if (!s_atom && !HasDropShadow())
	{
		s_atom = GetClassLong(m_hwnd, GCW_ATOM);
	}
	if (!s_atom_ds && HasDropShadow())
	{
		s_atom_ds = GetClassLong(m_hwnd, GCW_ATOM);
	}

	if (style == STYLE_DESKTOP || style == STYLE_CONSOLE_DIALOG || style == STYLE_MODELESS_DIALOG)
	{
		HICON icon = GetIcon(ICON_BIG);

		if(icon)
		{
			SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM) icon);
		}
		icon = GetIcon(ICON_SMALL);
		if(icon)
		{
			SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon);
		}
	}

	// register the top-level window for receiving WM_POWERBROADCAST messages for any change 
	// in the power supply (battery or ac change)
	if(m_style == STYLE_DESKTOP && m_parent_window == NULL && m_parent_view == NULL)
	{
		m_power_notify = OPRegisterPowerSettingNotification(m_hwnd, &GUID_ACDC_POWER_SOURCE, DEVICE_NOTIFY_WINDOW_HANDLE);
	}
	return OpStatus::OK;
}

void WindowsOpWindow::FindAndActivateDesktopWindow(int x, int y)
{
	if (!m_mde_screen->m_captured_input)
	{
		// Find and activate the topmost parent DesktopWindow for
		// the child found at the given coordinate.
		OpWindow *desktop_window = NULL;
		MDE_View *view = m_mde_screen->GetViewAt(x, y, true);

		while (view)
		{
			if (view->IsType("MDE_Widget"))
			{
				WindowsOpWindow *win = (WindowsOpWindow*) ((MDE_Widget*)view)->GetOpWindow();
				if (win && win != this && win->GetDesktopWindow())
					desktop_window = win;
			}
			view = view->m_parent;
		}
		if (desktop_window)
			desktop_window->Activate();
	}
}

HWND WindowsOpWindow::GetParentHWND()
{
	if (m_hwnd)
		return m_hwnd;
	if (m_parent_view && ((WindowsOpView*)m_parent_view)->GetNativeWindow())
		return ((WindowsOpView*)m_parent_view)->GetNativeWindow()->m_hwnd;
	if (m_parent_window)
		return m_parent_window->GetParentHWND();
	return NULL;
}

void WindowsOpWindow::OnPagebarMoved(BOOL onTop, int height)
{
	WindowsOpWindow* win = this;
	if (m_style != STYLE_DESKTOP && GetParentView() && static_cast<WindowsOpView*>(GetParentView())->GetNativeWindow())
		win = (static_cast<WindowsOpView*>(GetParentView())->GetNativeWindow());
	if (win->m_style == STYLE_DESKTOP)
	{
		BrowserDesktopWindow* bdwin = NULL;
		if (win->GetDesktopWindow() && win->GetDesktopWindow()->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
		{
			bdwin = static_cast<BrowserDesktopWindow*>(win->GetDesktopWindow());
		}
		bool composition = IsDwmCompositionEnabled();
		BOOL themed = win->m_themed;

		if (composition)
			UpdateDwmFrameWritingDirection();

		win->GetFrame()->CalculateSizes();

		RECT frame_sizes = win->GetFrame()->GetNativeFrameSizes();

		// if the pagebar is the topmost toolbar, we don't want a titlebar
		win->SetWantTitlebar(onTop == FALSE);

		win->m_titlebar_height = 0;
		win->m_requested_titlebar_height = 0;
		int border_height = 0;
		if (onTop)
		{
			win->m_requested_titlebar_height = frame_sizes.top;
			int drag_height = 0;
			if (win->m_state != MAXIMIZED)
			{
				// Limit the maximum border on top
				win->m_requested_titlebar_height = min(win->m_requested_titlebar_height, MAX_WINDOW_BORDER_TOP);
				drag_height = g_pcui->GetIntegerPref(PrefsCollectionUI::ChromeIntegrationDragArea);
			}
			else
			{
				border_height = win->m_requested_titlebar_height;	// the OS adds an artificial border, get rid of it here
				drag_height = g_pcui->GetIntegerPref(PrefsCollectionUI::ChromeIntegrationDragAreaMaximized);
			}
			if (drag_height > 0)
			{
				win->m_requested_titlebar_height += min(drag_height, GetSystemMetrics(SM_CYCAPTION));
			}
		}
		BOOL update_pagebar = FALSE;
		// Vista and later always wants transparent states when possible 
		BOOL use_transparent_state = IsSystemWinVista() && (composition || themed);
		if (g_skin_manager->GetOptionValue("Transparency", FALSE) || g_skin_manager->GetOptionValue("Full Transparency", FALSE))
		{
			// Also wants transparent states as long as skin allows
			use_transparent_state = TRUE;

			if (onTop && (composition || themed) && !win->m_menubar_visibility)
			{
				win->m_titlebar_height = win->m_requested_titlebar_height;
				update_pagebar = TRUE;
			}
		}

		if (!win->m_is_hidden)
			PostMessage(win->m_hwnd, WM_OPERA_RESIZE_FRAME, 0, 0);
		if (bdwin)
		{
			win->m_background_clearer =  WindowsBackgroundClearer::Create(win, use_transparent_state, composition, onTop);
		}
		OpPagebar* pb = win->GetPagebar();
		if (pb)
		{
			if (update_pagebar)
			{
				// get the caption button sizes
				INT32 btn_w = GetSystemMetrics(SM_CXSIZE) * WIN_RECT_MAX;
				INT32 btn_h = GetSystemMetrics(SM_CYSIZE);
				if(win->m_background_clearer)
				{
					win->m_background_clearer->GetBorderButtonsSize(&btn_w, &btn_h);
				}
				int close_min_max_height = composition ? btn_h : GetSystemMetrics(SM_CYCAPTION);
				if (!composition || win->m_state == MAXIMIZED)
				{
					close_min_max_height += frame_sizes.top;
				}
				pb->SetExtraTopPaddings(border_height, win->m_titlebar_height, close_min_max_height, btn_w + frame_sizes.left);
			}
			else
			{
				pb->SetExtraTopPaddings(0, 0, 0, 0);
			}
		}
		g_skin_manager->SetTransparentBackgroundColor(composition?0:0xffc0c0c0);
		OnTransparentAreaChanged(win->m_extended_chrome_top, win->m_extended_chrome_bottom, win->m_extended_chrome_left, win->m_extended_chrome_right);
	}
}
OpPagebar* WindowsOpWindow::GetPagebar()
{
	OpString8 pbn;
	if (OpStatus::IsError((pbn.Set("Pagebar"))) || !GetDesktopWindow())
		return NULL;
	return (OpPagebar*)GetDesktopWindow()->GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR);
}

void WindowsOpWindow::OnTransparentAreaChanged(int top, int bottom, int left, int right)
{
	WindowsOpWindow* win = this;
	if (m_style != STYLE_DESKTOP && GetParentView() && ((WindowsOpView*)GetParentView())->GetNativeWindow())
		win = ((WindowsOpView*)GetParentView())->GetNativeWindow();

	if (win->m_style == STYLE_DESKTOP)
	{
		BOOL composition = FALSE;
		BOOL themed = m_themed;

		OPDwmIsCompositionEnabled(&composition);
		MARGINS marg = {left,right,top,bottom};
		win->m_extended_chrome_height = 0;
		win->m_requested_extended_chrome_height = top;
		if (win->m_menubar_visibility && win->m_menubar)
		{
			SIZE sz = win->m_menubar->GetSize(NULL);
			win->m_requested_extended_chrome_height += sz.cy;
			marg.cyTopHeight += sz.cy;
		}

		// Enable or disable the system menu if the extended glass is on/off at the top.
		// Otherwise that will interfere with our client stuff (like the opera menu button)
		BOOL sys_menu_on = TRUE;

		if ((composition || themed) && win->m_titlebar_height)
		{
			win->m_extended_chrome_height = win->m_requested_extended_chrome_height;
			sys_menu_on = FALSE;
		}
		win->m_extended_chrome_top = top;
		win->m_extended_chrome_bottom = bottom;
		win->m_extended_chrome_left = left;
		win->m_extended_chrome_right = right;
		if (composition)
			win->m_vega_window->setTransparentMargins(marg.cyTopHeight, bottom, left, right);
		else
			win->m_vega_window->setTransparentMargins(0, 0, 0, 0);
		win->m_vega_window->setSystemMenuOn(sys_menu_on);
	}
}

void WindowsOpWindow::SetFullTransparency(BOOL full_transparency) 
{	
	WindowsOpWindow* win = this;
	if (m_style != STYLE_DESKTOP && GetParentView() && ((WindowsOpView*)GetParentView())->GetNativeWindow())
		win = ((WindowsOpView*)GetParentView())->GetNativeWindow();

	if (win->m_style == STYLE_DESKTOP)
	{
		win->m_vega_window->SetFullTransparency(full_transparency);
	}
}

BOOL WindowsOpWindow::HasFullTransparency() 
{	
	WindowsOpWindow* win = this;
	if (m_style != STYLE_DESKTOP && GetParentView() && ((WindowsOpView*)GetParentView())->GetNativeWindow())
		win = ((WindowsOpView*)GetParentView())->GetNativeWindow();

	if (win->m_style == STYLE_DESKTOP)
	{
		return win->m_vega_window->HasFullTransparency();
	}
	return FALSE;
}

void WindowsOpWindow::UpdateDwmFrameWritingDirection()
{
	BOOL languageIsRTL =
		g_languageManager != NULL &&
		g_languageManager->GetWritingDirection() == OpLanguageManager::RTL;

	OPDwmSetWindowAttribute(m_hwnd, DWMWA_NONCLIENT_RTL_LAYOUT, &languageIsRTL, sizeof(languageIsRTL));
}

HDC WindowsOpWindow::GetRenderTargetDC()
{
	return m_vega_window->GetBitmapDC();
}

BOOL WindowsOpWindow::SupportsExternalCompositing()
{
#ifdef VEGA_BACKEND_OPENGL
	// Layered window has serious issue with opengl ones (DSK-347376)
	if (GetWinType() <= WINXP && 
		g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D &&
		g_vegaGlobals.vega3dDevice->getDeviceType() == PrefsCollectionCore::OpenGL)
		return FALSE;
#endif // VEGA_BACKEND_OPENGL
	return TRUE;
}

void WindowsOpWindow::UpdateLanguage()
{
	if (IsDwmCompositionEnabled())
		UpdateDwmFrameWritingDirection();
	else if (m_background_clearer)
		m_background_clearer->UpdateFrameLanguageDirection();

	DesktopOpWindow::UpdateLanguage();
}

long WindowsOpWindow::HandleMouseWheelEvent(bool vertical, WPARAM wparam, LPARAM lparam, POINT client_point, ShiftKeyState key_state)
{
	if (g_application)
		g_application->RegisterActivity();

	short keys = GET_KEYSTATE_WPARAM(wparam);
	short delta = GET_WHEEL_DELTA_WPARAM(wparam);

	BOOL scroll_as_page = FALSE;
	int linecount = IntelliMouse_GetLinesToScroll(m_hwnd, delta, &scroll_as_page);

	// Our API has vertical scrolling inverted.
	if (vertical)
		linecount = -linecount;

	POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
	HWND hwnd_below_cursor = WindowFromPoint(pt);

	if (m_hwnd != hwnd_below_cursor && hwnd_below_cursor)
	{
		if (WindowsOpWindow* window_below_cursor = GetWindowFromHWND(hwnd_below_cursor))
		{
			ScreenToClient(hwnd_below_cursor, &pt);
			window_below_cursor->m_mde_screen->TrigMouseWheel(pt.x, pt.y, linecount, vertical, key_state);
			return 0;
		}
	}

	if (scroll_as_page && keys == 0)
	{
		OpInputAction::Action scroll_code;

		if (vertical)
			scroll_code = linecount < 0 ? OpInputAction::ACTION_PAGE_UP : OpInputAction::ACTION_PAGE_DOWN;
		else
			scroll_code = linecount < 0 ? OpInputAction::ACTION_PAGE_LEFT : OpInputAction::ACTION_PAGE_RIGHT;

		linecount = abs(linecount);

		for (int i = 0; i < linecount; i++)
			g_input_manager->InvokeAction(scroll_code);
	}
	else
		m_mde_screen->TrigMouseWheel(client_point.x, client_point.y, linecount, vertical, key_state);

	// Fix for bug DSK-314234. The removal of the tooltip should probably be fixed in core rather than here?
	if (g_application)
		g_application->SetToolTipListener(NULL);

	return 0;
}

void ReparentPluginWindows(MDE_View *view, HWND new_parent)
{
	if (view->IsType("MDENativeWindow"))
	{
		WindowsPluginNativeWindow *hwnd_win = (WindowsPluginNativeWindow *) view;
		hwnd_win->SetParent(new_parent);
	}
	MDE_View *child = view->m_first_child;
	while (child)
	{
		ReparentPluginWindows(child, new_parent);
		child = child->m_next;
	}
}

void WindowsOpWindow::SetParent(OpWindow* parent_window, OpView* parent_view, BOOL behind)
{
	if (m_parent_has_been_disabled && m_parent_window && IsModalDialog())
	{
		m_parent_window->Enable();
		m_parent_has_been_disabled = FALSE;
	}

	m_parent_window		= (WindowsOpWindow*) parent_window;
	m_parent_view		= parent_view;

	if (!m_hwnd)
	{
		MDE_OpWindow::SetParent(parent_window, parent_view, behind);
		if (parent_window)
			ReparentPluginWindows(GetMDEWidget(), ((WindowsOpWindow*)parent_window->GetRootWindow())->m_hwnd);
		else if (parent_view)
			ReparentPluginWindows(GetMDEWidget(), ((WindowsOpWindow*)parent_view->GetRootWindow())->m_hwnd);
		return;
	}
	HWND parenthwnd = NULL;

	if (m_parent_window && m_style != STYLE_POPUP && m_style != STYLE_TOOLTIP && m_style != STYLE_NOTIFIER)
	{
		parenthwnd = m_parent_window->GetParentHWND();

		if (parenthwnd == NULL)
		{
			parenthwnd = g_main_hwnd;
		}

		if (!m_is_hidden && !m_parent_has_been_disabled && m_parent_window && IsModalDialog())
		{
			m_parent_window->Disable();
			m_parent_has_been_disabled = TRUE;
		}
	}
	else if (m_parent_view)
	{
		MDE_OpView* pv = (MDE_OpView*)m_parent_view;
		while (pv->GetParentView())
			pv = pv->GetParentView();
		parenthwnd = pv->GetParentWindow()?(((WindowsOpWindow*)pv->GetParentWindow())->m_hwnd):NULL;
	}

	SetParent(parenthwnd, behind);
}

void WindowsOpWindow::SetParent(HWND parenthwnd, BOOL behind)
{
	if (m_style == STYLE_DESKTOP)
	{
		SetWindowLongPtr(m_hwnd, GWL_STYLE, (GetWindowLongPtr(m_hwnd, GWL_STYLE) & ~WS_CHILD) | (parenthwnd ? WS_CHILD : 0));

		Deactivate();		

		State state = m_state;

		::SetParent(m_hwnd, parenthwnd);

		SetDesktopPlacement(m_restored_rect, state);

		if (parenthwnd && behind)
		{
			SetWindowPos(m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);
		}
	}
	else
	{
		::SetParent(m_hwnd, parenthwnd);
	}
}

OP_STATUS WindowsOpWindow::StaticInitialization()
{
	static bool initialized = false;

	if (initialized)
		return OpStatus::OK;


	//
	// Dispatch table for windows messages (initialization).
	//

	op_memset(WindowsOpWindow::wm_messages, 0, sizeof(WindowsOpWindow::wm_messages));
	WindowsOpWindow::wm_messages[WM_PAINT] = &WindowsOpWindow::WmPaint;
	WindowsOpWindow::wm_messages[WM_WINDOWPOSCHANGED] = &WindowsOpWindow::WmWindowposchanged;
	

	//
	// Register Windows classes
	//

	WNDCLASSEX wnd_class;

	wnd_class.style         = CS_DBLCLKS;
	wnd_class.cbSize        = sizeof(WNDCLASSEX);
	wnd_class.lpfnWndProc   = (WNDPROC)WindowProc;
	wnd_class.cbClsExtra    = 0;
	wnd_class.cbWndExtra    = 0;
	wnd_class.hInstance     = hInst;
	wnd_class.hIcon         = NULL;
	wnd_class.hIconSm       = NULL;
	wnd_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wnd_class.hbrBackground = NULL;
	wnd_class.lpszMenuName  = NULL;
	wnd_class.lpszClassName = UNI_L(OPERA_WINDOW_CLASSNAME);

	if (RegisterClassEx(&wnd_class) == 0)
		return OpStatus::ERR_NO_MEMORY;

	WNDCLASSEX wnd_shadow_class;

	wnd_shadow_class.style         = CS_DBLCLKS;
	// No drop shadow when transparency cause problems
	if (!(GetWinType() == WINXP &&
		g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D &&
		g_vegaGlobals.vega3dDevice->getDeviceType() == PrefsCollectionCore::OpenGL))
	{
		wnd_shadow_class.style     |= CS_DROPSHADOW;
	}
	wnd_shadow_class.cbSize        = sizeof(WNDCLASSEX);
	wnd_shadow_class.lpfnWndProc   = (WNDPROC)WindowProc;
	wnd_shadow_class.cbClsExtra    = 0;
	wnd_shadow_class.cbWndExtra    = 0;
	wnd_shadow_class.hInstance     = hInst;
	wnd_shadow_class.hIcon         = NULL;
	wnd_shadow_class.hIconSm       = NULL;
	wnd_shadow_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wnd_shadow_class.hbrBackground = NULL;
	wnd_shadow_class.lpszMenuName  = NULL;
	wnd_shadow_class.lpszClassName = UNI_L("OpWindowDS");

	if (RegisterClassEx(&wnd_shadow_class) == 0)
		return OpStatus::ERR_NO_MEMORY;

#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

	WNDCLASSEX wnd_taskbar_class;

	// Invisible window class..
	wnd_taskbar_class.cbSize = sizeof(WNDCLASSEX);
	wnd_taskbar_class.style = CS_DBLCLKS;
	wnd_taskbar_class.lpfnWndProc = (WNDPROC)Windows7TaskbarWindowProc;
	wnd_taskbar_class.cbClsExtra = 0;
	wnd_taskbar_class.cbWndExtra = 0;
	wnd_taskbar_class.hInstance = hInst;
	wnd_taskbar_class.hIcon = NULL;
	wnd_taskbar_class.hIconSm = NULL;
	wnd_taskbar_class.hCursor = NULL;
	wnd_taskbar_class.hbrBackground = NULL;
	wnd_taskbar_class.lpszMenuName  = NULL;
	wnd_taskbar_class.lpszClassName = UNI_L("OpTaskbarProxyWnd");

	RegisterClassEx(&wnd_taskbar_class);

#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

	initialized = true;
	return OpStatus::OK;
}

WindowsOpWindow::WindowsOpWindow() :
	  m_flash_count(0)
	, m_inverted(FALSE)
	, m_minimum_width(0)
	, m_minimum_height(0)
	, m_maximum_width(UINT_MAX)
	, m_maximum_height(UINT_MAX)
	, m_menubar(NULL)
	, m_style(STYLE_DESKTOP)
	, m_type(OpTypedObject::WINDOW_TYPE_UNKNOWN)
	, m_left_margin(0)
	, m_top_margin(0)
	, m_right_margin(0)
	, m_bottom_margin(0)
	, m_disabled_count(0)
	, m_parent_window(NULL)
	, m_parent_view(NULL)
	, m_active(FALSE)
	, m_width(0)
	, m_height(0)
	, m_fullscreen(FALSE)
	, m_state(RESTORED)
	, m_old_state(RESTORED)
	, m_skin(NULL)
	, m_user_initiated_close(TRUE)
	, m_active_window_when_created(NULL)
	, m_is_hidden(TRUE)
	, m_workspace_minimized_visibility(TRUE)
	, m_translate_coordinate(FALSE)
	, m_menubar_visibility(FALSE)
	, m_options(WindowsOpWindow::opWindowAllOptions)
	, m_effects(OpWindow::EFFECT_NONE)
	, m_parent_has_been_disabled(FALSE)
	, m_hwnd(NULL)
	, m_win_style_ex(0)
	, m_win_style(0)
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, m_accessible_item(NULL)
	, m_accessibility_inited(FALSE)
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
#ifdef GADGET_SUPPORT
	, m_gadget_opview_child(NULL)
#endif // GADGET_SUPPORT
	, m_dirty_thumbnail(TRUE)
	, m_power_notify(NULL)
	, m_want_title_bar(FALSE)
	, m_locked_by_user(FALSE)
	, m_keep_below(FALSE)
	, m_vega_window(NULL)
	, m_mde_screen(NULL)
	, m_pending_mde_render(FALSE)
	, m_titlebar_height(0)
	, m_extended_chrome_height(0)
	, m_requested_titlebar_height(0)
	, m_requested_extended_chrome_height(0)
	, m_extended_chrome_top(0)
	, m_extended_chrome_bottom(0)
	, m_extended_chrome_left(0)
	, m_extended_chrome_right(0)
	, m_disable_cursor_change(FALSE)
#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
	, m_taskbar_proxy_wnd(NULL)
	, m_taskbar_message_listener(NULL)
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
	, m_sizemove(FALSE)
	, m_background_clearer(NULL)
	, m_themed(FALSE)
	, m_hwnd_active(FALSE)
	, m_frame(NULL)
{
	if (!current_input_language)
		current_input_language = GetKeyboardLayout(0);
}

WindowsOpWindow::~WindowsOpWindow()
{
	OP_DELETE(m_frame);
	if(m_style == STYLE_DESKTOP)
	{
		g_skin_manager->RemoveSkinNotficationListener(this);
	}
	// unregister for power notifications, if we have registered
	if(m_power_notify)
	{
		OPUnregisterPowerSettingNotification(m_power_notify);
	}
	if(m_background_clearer)
		g_skin_manager->RemoveTransparentBackgroundListener(m_background_clearer);
	OP_DELETE(m_background_clearer);

	if (g_application)
	{
		g_application->RemoveSettingsListener(this);
	}

	Out();

	if (this == s_popup_window)
		s_popup_window = NULL;

	if (this == s_current_popup_window)
		s_current_popup_window = NULL;

	Close();

	OP_DELETE(m_mde_screen);
	OP_DELETE(m_vega_window);
}

void WindowsOpWindow::SetLockedByUser(BOOL locked)
{ 
	m_locked_by_user = locked; 

	OUIMenuBar* menu_bar = GetMenuBar();

	if(menu_bar && menu_bar->m_mdi_close != NULL)
	{
		EnableWindow(menu_bar->m_mdi_close, locked ? FALSE : TRUE);
		g_input_manager->UpdateAllInputStates();
	}
}

void WindowsOpWindow::SetWindowListener(OpWindowListener *window_listener)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::SetWindowListener(window_listener);
	}
	m_window_listener = window_listener;
}

void WindowsOpWindow::SetDesktopPlacement(const OpRect& desired_restored_rect, State state, BOOL inner, BOOL behind, BOOL center)
{
	if (!m_hwnd)
	{
		m_fullscreen = (state == FULLSCREEN);
		MDE_OpWindow::SetDesktopPlacement(desired_restored_rect, state, inner, behind, center);
		return;
	}
	if (m_style != OpWindow::STYLE_DESKTOP && !IsDialog() && m_style != OpWindow::STYLE_GADGET && m_style != OpWindow::STYLE_POPUP)
		return;

	OpRect rect = desired_restored_rect;

	if (center)
	{
		RECT window_rect;

		WindowsOpWindow* parent_window = m_parent_window;
		MDE_OpView* parent_view = (MDE_OpView*)m_parent_view;

		while (parent_view)
		{
			parent_window = (WindowsOpWindow*)parent_view->GetParentWindow();
			parent_view = parent_view->GetParentView();
		}

		HWND toplevel_hwnd = NULL;

		if (parent_window)
		{
			toplevel_hwnd = parent_window->m_hwnd;

			while (GetWindowLongPtr(toplevel_hwnd, GWL_STYLE) & WS_CHILD)
				toplevel_hwnd = GetParent(toplevel_hwnd);
		}

		if (!toplevel_hwnd || IsIconic(toplevel_hwnd))
		{
			POINT point;
			GetCursorPos(&point);
			MonitorRectFromPoint(window_rect, point, TRUE);
		}
		else if (parent_window->m_state == RESTORED)
		{
			GetWindowRect(parent_window->m_hwnd, &window_rect);
		}
		else
		{
			GetWindowRect(toplevel_hwnd, &window_rect);
		}


		OpRect parent_rect(&window_rect);
		OpPoint screen_point(parent_rect.x, parent_rect.y);

		OpScreenProperties properties;
		g_op_screen_info->GetProperties(&properties, this, &screen_point);

		parent_rect.x -= (properties.workspace_rect.x - properties.screen_rect.x);
		parent_rect.y -= (properties.workspace_rect.y - properties.screen_rect.y);

		rect.x = parent_rect.x + (parent_rect.width - rect.width) / 2; 
		rect.y = parent_rect.y + (parent_rect.height - rect.height) / 2; 
	}

	RECT r;
	State savedstate = state;

	switch (DefShowWindow)
	{
		case SW_SHOWMINNOACTIVE:
			state = MINIMIZED;
			DefShowWindow = 0;	// only apply shortcut default window state to the main window
			break;

		case SW_SHOWMAXIMIZED:
			state = MAXIMIZED;
			DefShowWindow = 0;
	}

	BOOL reset_style = FALSE;
	LONG fullscreen_style = WS_OVERLAPPED | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_VISIBLE;

	if (state == FULLSCREEN)
	{
		m_restored_rect = rect;
		inner = TRUE;

		if (m_parent_window)
		{
			GetClientRect(m_parent_window->m_hwnd, &r);
		}
		else
		{
			SetWindowLongPtr(m_hwnd, GWL_STYLE, fullscreen_style);
			MonitorRectFromWindow(r, m_hwnd, FALSE);

			RECT workspace;
			MonitorRectFromWindow(workspace, m_hwnd, TRUE);

			r.right += r.left - workspace.left;
			r.bottom += r.top - workspace.top;
			r.left += r.left - workspace.left;
			r.top += r.top - workspace.top;
		}
	}
	else
	{
		if (m_fullscreen && !m_parent_window)
			reset_style = TRUE;
			
		r.left		= rect.x;
		r.top		= rect.y;
		r.right		= rect.x + rect.width;
		r.bottom	= rect.y + rect.height;
	}

	if (inner)
	{
		r.left		-= m_left_margin;
		r.top		-= m_top_margin;
		r.right		+= m_right_margin;
		r.bottom	+= m_bottom_margin;
		AdjustWindowRectEx(&r, (state == FULLSCREEN && !m_parent_window)?fullscreen_style:m_win_style,0, m_win_style_ex);
	}

	if (state == RESTORED && m_parent_window && m_parent_window->m_style == OpWindow::STYLE_WORKSPACE)
	{
		HWND child = GetWindow(m_parent_window->m_hwnd, GW_CHILD);

		child = GetWindow(child, GW_HWNDLAST);

		while (child)
		{
			if (!IsZoomed(child) && !IsIconic(child))
			{
				RECT rect;

				GetWindowRect(child, &rect);
				ScreenToClientRect(m_parent_window->m_hwnd, &rect);

				if (rect.left == r.left && rect.top == r.top)
				{
					OffsetRect(&r, GetSystemMetrics(SM_CYCAPTION), GetSystemMetrics(SM_CYCAPTION));

					RECT parent_rect;

					GetClientRect(m_parent_window->m_hwnd, &parent_rect);

					if (r.right > parent_rect.right || r.bottom > parent_rect.bottom)
					{
						OffsetRect(&r, -r.left + GetSystemMetrics(SM_CYCAPTION), -r.top + GetSystemMetrics(SM_CYCAPTION));
					}
				}
			}

			child = GetWindow(child, GW_HWNDPREV);
		}
	}

	WINDOWPLACEMENT wp;

	wp.length = sizeof(WINDOWPLACEMENT);

	::GetWindowPlacement(m_hwnd, &wp);

	if (m_parent_view)
	{
		int xofs = 0, yofs = 0;
		((MDE_OpView*)m_parent_view)->GetMDEWidget()->ConvertFromScreen(xofs, yofs);
		r.left += xofs;
		r.right += xofs;
		r.top += yofs;
		r.bottom += yofs;
	}
	wp.rcNormalPosition = r;
	m_fullscreen = (state == FULLSCREEN);

	switch (state)
	{
		case MINIMIZED:
			wp.showCmd = SW_SHOWMINIMIZED;
			if (savedstate == MAXIMIZED) wp.flags = WPF_RESTORETOMAXIMIZED;
			break;

		case MAXIMIZED:
			if (behind)
				wp.showCmd = m_parent_window && m_parent_window->m_style == OpWindow::STYLE_WORKSPACE ? SW_SHOWNOACTIVATE : SW_MAXIMIZE;
			else
				wp.showCmd = m_parent_window && m_parent_window->m_style == OpWindow::STYLE_WORKSPACE ? SW_SHOWNORMAL : SW_SHOWMAXIMIZED;

			break;

		case FULLSCREEN:
		case RESTORED:
			wp.showCmd = (m_style == OpWindow::STYLE_POPUP ? SW_SHOWNOACTIVATE : SW_SHOWNORMAL);
			break;
	}

	if (reset_style)
		SetWindowLongPtr(m_hwnd, GWL_STYLE, m_win_style | WS_VISIBLE );

	// Fix for DSK-281110 Open in background window opens link in foreground window
	// Set the zorder of the background window. We used to show the
	// window and move it to background, which cause flicker for restored
	// window on Win7(or vista as well) when the window animation is turned on
	if (behind && m_style == OpWindow::STYLE_DESKTOP)
	{
		// Don't activate the background window in the first place.
		// Maximized window seem has to be activated anyway.
		// (No option to prevent it from being activated?)
		if (state == RESTORED)
			wp.showCmd = SW_SHOWNOACTIVATE;

		SetWindowPos(m_hwnd, m_active_window_when_created, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
	}

	UINT desired = wp.showCmd;			// workaround so that Windoze doesn't maximize/minimize
	::SetWindowPlacement(m_hwnd, &wp);	// the startup dialog because of the shortcut setting
	::GetWindowPlacement(m_hwnd, &wp);	// which will always silently affect the first window
	if (desired != wp.showCmd)
	{
		wp.showCmd = desired;
		::SetWindowPlacement(m_hwnd, &wp);
	}

	if (m_parent_window && m_parent_window->m_style == OpWindow::STYLE_WORKSPACE)
	{
		if (m_is_hidden && state != MINIMIZED && !behind)
		{
			SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
		}
		else if (behind && state == MAXIMIZED)
		{
			RECT rect;

			GetClientRect(GetParent(m_hwnd), &rect);
			AdjustWindowRectEx(&rect, m_win_style, 0, m_win_style_ex);

			SetWindowLongPtr(m_hwnd, GWL_STYLE, GetWindowLongPtr(m_hwnd, GWL_STYLE) | WS_MAXIMIZE);

			SetWindowPos(m_hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_NOZORDER);
		}
	}
	else if (behind && !m_parent_window && m_active_window_when_created)
	{
		SetForegroundWindow(m_active_window_when_created);
	}
	
	//if parent is OpWorkspace the rect of this is not to be trusted do not try to keep inside of this, but do if the parent is NULL to keep inside of monitor
	if (state == RESTORED && (!m_parent_window || (m_parent_window && m_parent_window->m_style != OpWindow::STYLE_WORKSPACE)))
	{
		EnsureWindowVisible(m_hwnd, IsDialog());
	}

	if (m_is_hidden)
	{
		SetWidgetVisibility(TRUE);

		if (m_window_listener)
		{
			m_window_listener->OnShow(TRUE);
		}
		m_is_hidden = FALSE;
		if (m_titlebar_height)
		{
			// We now have enough information to determine the frames
			if (GetFrame())
				GetFrame()->CalculateSizes();
			SetWindowPos(m_hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
		}

		if (!behind && state != MINIMIZED && m_style != OpWindow::STYLE_POPUP)
		{
			Activate();
		}

		EnsureHiddenIconicState();

		if (!m_parent_has_been_disabled && m_parent_window && IsModalDialog())
		{
			m_parent_window->Disable();
			m_parent_has_been_disabled = TRUE;
		}

		if (!m_pending_mde_render)
		{
			m_pending_mde_render = TRUE;
			PostMessage(m_hwnd, WM_OPERA_MDE_RENDER, 0, 0);
		}
	}

	if (m_style == OpWindow::STYLE_BLOCKING_DIALOG)
	{
		//(julienp) Fix for bug 134227. Sending WM_CANCELMODE ensures that the menu is
		//			terminated before we call a modal dialog (so they don't block each other)
		//			This does not however terminate the message loop started by the popup menu
		//			and it cannot (easily) be terminated before launching this message loop.
		//			This makes impossible to start a popup menu from a modal dialog that
		//			interrupted another popup menu (still better than freezing)
		//			The message loop of the popup menu will be terminated once the modal
		//			dialog is closed, making it possible to use menus normally again.
		if(g_windows_menu_manager)
		{
			HWND owner = g_windows_menu_manager->GetActiveMenuOwner();
			if (owner)
			{
				PostMessage(owner, WM_CANCELMODE, 0, 0);	// can't use EndMenu(), not supported on W95
			}
		}

		g_windows_message_loop->Run(this);
	}
}

void WindowsOpWindow::GetDesktopPlacement(OpRect& rect, State& state)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::GetDesktopPlacement(rect, state);
		return;
	}
	rect = 	m_restored_rect;
	state = m_state;
}

WindowsOpWindow::State WindowsOpWindow::GetRestoreState() const
{
	if (!m_hwnd)
		return MDE_OpWindow::GetRestoreState();

	return m_old_state;
}

void WindowsOpWindow::SetRestoreState(State state)
{
	if (!m_hwnd)
		MDE_OpWindow::SetRestoreState(state);

	m_old_state = state;
}

void WindowsOpWindow::Show(BOOL show)
{
	if (!m_hwnd)
	{
		BOOL invalidate_and_bypass_lock = show && !GetMDEWidget()->m_is_visible;

		MDE_OpWindow::Show(show);

		// When we show it, core will lock updating when loading is started before we have painted it the first time.
		// Send true for the bypass_lock parameter so the first display will paint anyway. The page will become cleared
		// since core will most probably not have got any content to paint yet.
		//only if changed!
		if (invalidate_and_bypass_lock)
			GetMDEWidget()->Invalidate(MDE_MakeRect(0, 0, GetMDEWidget()->m_rect.w, GetMDEWidget()->m_rect.h), true, false, false, true);
		return;
	}
	if (show == !m_is_hidden)
		return;

	SetWidgetVisibility(!!show);

	if(show)
	{
		if (m_style == STYLE_POPUP ||  m_style == STYLE_TOOLTIP ||  m_style == STYLE_NOTIFIER)
		{
			// if a non opera window is running fullscreen, don't show toolips or popups over it

			RECT rect;
			MonitorRectFromWindow(rect, m_hwnd, FALSE);

			HWND insert_after = NULL;
			RECT after_rect = rect;		//(julienp)Warning fix: need to initialize to anything
			HWND window = GetWindow(m_hwnd, GW_HWNDLAST);

			// Starting from the current window, loop through the windows until we're on top, and check if
			// we find a fullscreen window that should not be covered by the tooltip/popup.
			// This is not 100% reliable, but this seem to be the only way to detect fullscreen mode in MSWIN
			while (window)
			{
				if (IsWindowVisible(window))
				{
					RECT window_rect;

					GetWindowRect(window, &window_rect);

					if (window_rect.left <= rect.left && window_rect.top <= rect.top && window_rect.right >= rect.right && window_rect.bottom >= rect.bottom)
					{
						// This window covers the whole screen, may be fullscreen
						insert_after = window;
						after_rect = window_rect;
					}
					else if (insert_after && GetParent(window) != insert_after)
					{
						// If the fullscreen window is covered by another window (but not a childwindow)
						// insert_after is appearantly not really full screen.
						if (window_rect.left < after_rect.right && window_rect.right > after_rect.left &&
							window_rect.top < after_rect.bottom && window_rect.bottom > after_rect.top)
							insert_after = NULL;
					}

				}
				window = GetWindow(window, GW_HWNDPREV);
			}

			if (insert_after && IsOperaWindow(insert_after))
				insert_after = NULL;

			// Position the tooltip before the fullscreen window, if none just on top of everything
			::SetWindowPos(m_hwnd, insert_after ? insert_after : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
		}
		else
			ShowWindow(m_hwnd,SW_SHOW);

		if (!m_pending_mde_render)
		{
			m_pending_mde_render = TRUE;
			PostMessage(m_hwnd, WM_OPERA_MDE_RENDER, 0, 0);
		}
	}
	else
	{
		ShowWindow(m_hwnd,SW_HIDE);
	}

	m_is_hidden = !show;

	if (show && m_titlebar_height)
		SetWindowPos(m_hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

	if (m_window_listener)
	{
		m_window_listener->OnShow(show);
	}
}

static BOOL NextWindowIsThePopupOrADialog(HWND hWndNextWindow, HWND hPopup)
{
	if (!hWndNextWindow) return FALSE;
	if (hPopup == hWndNextWindow) return TRUE;
	if (IsChild(hPopup, hWndNextWindow)) return TRUE;
	DWORD atom = GetClassLongPtr(hWndNextWindow, GCW_ATOM);
	if (atom == 32770) //Window Class for System Dialog Windows (as in CreateDialog/DialogBox function families)
		return TRUE;

	return FALSE;
}

BOOL WindowsOpWindow::IsFocusable()
{
#ifdef DECORATED_GADGET_SUPPORT
	return m_style == STYLE_DECORATED_GADGET || IsDialog() || m_style == STYLE_DESKTOP;
#else
	return IsDialog() || m_style == STYLE_DESKTOP;
#endif // DECORATED_GADGET_SUPPORT
}

BOOL WindowsOpWindow::IsDialog()
{
	if (m_style == STYLE_MODELESS_DIALOG
		|| m_style == STYLE_MODAL_DIALOG
		|| m_style == STYLE_BLOCKING_DIALOG
		|| m_style == STYLE_CONSOLE_DIALOG
		|| m_type == OpTypedObject::DIALOG_TYPE
		|| m_type == OpTypedObject::DIALOG_TYPE_DOWNLOAD_SETUP)
		return TRUE;

	return FALSE;
}

BOOL WindowsOpWindow::IsModalDialog()
{
	return m_style == STYLE_MODAL_DIALOG || m_style == STYLE_BLOCKING_DIALOG;
}

BOOL WindowsOpWindow::HasDecorations()
{
#ifdef DECORATED_GADGET_SUPPORT
	return m_style == STYLE_DECORATED_GADGET || m_style == STYLE_DESKTOP || IsDialog();
#else
	return m_style == STYLE_DESKTOP || IsDialog();
#endif // DECORATED_GADGET_SUPPORT
}

#ifdef GADGET_SUPPORT
BOOL WindowsOpWindow::IsThisAGadget() const
{
	return m_style == STYLE_GADGET || m_style == STYLE_DECORATED_GADGET || m_style == STYLE_HIDDEN_GADGET;
}
#endif // GADGET_SUPPORT


void WindowsOpWindow::ClosePopup(HWND hWndNextWindow)
{
	//close the "standard" popup (mostly used by tooltips)
	if (WindowsOpWindow::s_popup_window)
	{
		if (NextWindowIsThePopupOrADialog(hWndNextWindow, s_popup_window->m_hwnd)) return;

		PostMessage(WindowsOpWindow::s_popup_window->m_hwnd, WM_CLOSE, 0, 0);
		WindowsOpWindow::s_popup_window = NULL;
	}

	//close extension popup
	if (WindowsOpWindow::s_current_popup_window)
	{
		if ((s_current_popup_window->m_hwnd != hWndNextWindow) ||
			s_popup_window && s_popup_window->m_hwnd != hWndNextWindow)
		{
			if (NextWindowIsThePopupOrADialog(hWndNextWindow, s_current_popup_window->m_hwnd)) return;
			if (!hWndNextWindow) return;

			WindowsOpWindow *window = WindowsOpWindow::GetWindowFromHWND(hWndNextWindow);
			if (window)
			{
				if (window->GetStyle() == OpWindow::STYLE_GADGET ||
					window->GetStyle() == OpWindow::STYLE_HIDDEN_GADGET ||
					window->GetStyle() == OpWindow::STYLE_NOTIFIER)
					return;
				if (window->GetStyle() == OpWindow::STYLE_MODAL_DIALOG)
				{
						s_expecting_dialog_close = 1;
						return;
				}
			}

			if (s_expecting_dialog_close > 0)
			{
				s_expecting_dialog_close--;
				if (s_expecting_dialog_close == 0)
					PostMessage(s_current_popup_window->m_hwnd,WM_ACTIVATE,1,0);
				return;
			}

			PostMessage(s_current_popup_window->m_hwnd, WM_CLOSE, 0, 0);
			s_current_popup_window= NULL;
		}
	}
}

void WindowsOpWindow::Close(BOOL user_initiated)
{
	if(m_hwnd && m_style == STYLE_DESKTOP)
	{
		// when we get here, m_is_closing is already set or m_hwnd is NULL, so make sure to do this before the checks below
		g_windows_drag_manager->UnregisterDropTarget(m_hwnd);
	}
	if (m_is_closing || (user_initiated && m_window_listener && !m_window_listener->OnIsCloseAllowed()))
	{
		return;
	}

	m_is_closing = TRUE;	// Don't send the inactivate message to the listener.

	DocWin_HandleIntelliMouseMessages(0, 0, 0, 0); // End any potential panning now

#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
	if(m_taskbar_proxy_wnd)
	{
		// destroy all icons associated with the window
		HICON old_icon = (HICON)SendMessage(m_taskbar_proxy_wnd, WM_SETICON, ICON_SMALL, (LPARAM) NULL);
		if(old_icon)
		{
			DestroyIcon(old_icon);
		}
		old_icon = (HICON)SendMessage(m_taskbar_proxy_wnd, WM_SETICON, ICON_BIG, (LPARAM) NULL);
		if(old_icon)
		{
			DestroyIcon(old_icon);
		}
		DestroyWindow(m_taskbar_proxy_wnd);
		m_taskbar_proxy_wnd = NULL;
	}
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

	if (!m_hwnd && mdeWidget)
	{
		MDE_OpWindow::Close(user_initiated);
		return;
	}
	else if (mdeWidget && mdeWidget->m_parent)
	{
		mdeWidget->m_parent->RemoveChild(mdeWidget);
	}
	if (m_hwnd && m_style != STYLE_ADOPT)
	{
		if (m_parent_has_been_disabled && m_parent_window && IsModalDialog())
		{
			m_parent_window->Enable();
			m_parent_has_been_disabled = FALSE;
		}

		m_user_initiated_close = user_initiated;

		DestroyWindow(m_hwnd);
	}
	else
	{
		if(m_hwnd)
		{
			WindowsOpWindow *opwin;
			OP_STATUS retval = s_adopted_windows.Remove(m_hwnd, &opwin);
			OP_ASSERT(retval == OpStatus::OK);
			OpStatus::Ignore(retval);
		}
	}
}

/***********************************************************************************
**
**	Size / Position functions
**
***********************************************************************************/

void WindowsOpWindow::GetRenderingBufferSize(UINT32* width, UINT32* height)
{
	GetInnerSize(width, height);
}

void WindowsOpWindow::GetOuterSize(UINT32* width, UINT32* height)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::GetOuterSize(width, height);
		return;
	}
	RECT rect;

	GetWindowRect(m_hwnd, &rect);

	*width = rect.right - rect.left;
	*height = rect.bottom - rect.top;
}

void WindowsOpWindow::SetOuterSize(UINT32 width, UINT32 height)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::SetOuterSize(width, height);
		return;
	}
	if (m_state != RESTORED)
		return;

	SetWindowPos(m_hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	m_vega_window->wm_size(m_width, m_height);
	
	MDE_OpWindow::SetInnerSize(width, height);
}

void WindowsOpWindow::GetInnerSize(UINT32* width, UINT32* height)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::GetInnerSize(width, height);
		return;
	}
	*width = m_width - m_left_margin - m_right_margin;
	*height = m_height - m_top_margin - m_bottom_margin;
}

void WindowsOpWindow::SetInnerSize(UINT32 width, UINT32 height)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::SetInnerSize(width, height);
		return;
	}
	UINT32 outer_width;
	UINT32 outer_height;

	GetOuterSize(&outer_width, &outer_height);

	UINT32 inner_width;
	UINT32 inner_height;

	GetInnerSize(&inner_width, &inner_height);

	width += outer_width - inner_width;
	height += outer_height - inner_height;

	SetOuterSize(width, height);
}

void WindowsOpWindow::GetOuterPos(INT32* x, INT32* y)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::GetOuterPos(x, y);
		return;
	}
	RECT rect;

	GetWindowRect(m_hwnd, &rect);

	if (GetWindowLongPtr(m_hwnd, GWL_STYLE) & WS_CHILD)
	{
		ScreenToClientRect(GetParent(m_hwnd), &rect);

		if (m_parent_window)
		{
			rect.left -= m_parent_window->m_left_margin;
			rect.top -= m_parent_window->m_top_margin;
		}
	}

	*x = rect.left;
	*y = rect.top;
}

void WindowsOpWindow::SetOuterPos(INT32 x, INT32 y)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::SetOuterPos(x, y);
		return;
	}
	if (m_state != RESTORED)
		return;

	if (GetWindowLongPtr(m_hwnd, GWL_STYLE) & WS_CHILD)
	{
		if (m_parent_window)
		{
			x += m_parent_window->m_left_margin;
			y += m_parent_window->m_top_margin;
		}
	}
	if (m_parent_view)
		((MDE_OpView*)m_parent_view)->GetMDEWidget()->ConvertToScreen(x, y);

	SetWindowPos(m_hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowsOpWindow::GetInnerPos(INT32* x, INT32* y)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::GetInnerPos(x, y);

		if (m_translate_coordinate)
		{
			WindowsOpWindow* root = ((VegaMDEScreen*)GetMDEWidget()->m_screen)->GetWindow();
			POINT pt = {*x, *y};

			// cooridinate of STYLE_POPUP window is relative to the vega screen, margin is irrelevant
			if (m_style != STYLE_POPUP)
			{
				pt.x += root->m_left_margin;
				pt.y += root->m_top_margin;
			}

			ClientToScreen(root->m_hwnd, &pt);
			*x = pt.x;
			*y = pt.y;
		}
		return;
	}
	POINT point = {0,0};

	ClientToScreen(m_hwnd, &point);

	if (GetWindowLongPtr(m_hwnd, GWL_STYLE) & WS_CHILD)
	{
		ScreenToClient(GetParent(m_hwnd), &point);

		if (m_parent_window)
		{
			point.x -= m_parent_window->m_left_margin;
			point.y -= m_parent_window->m_top_margin;
		}
	}

	*x = point.x;
	*y = point.y;
}

void WindowsOpWindow::SetInnerPos(INT32 x, INT32 y)
{
	if (!m_hwnd)
	{
		if (m_translate_coordinate)
		{
			WindowsOpWindow* root = ((VegaMDEScreen*)GetMDEWidget()->m_screen)->GetWindow();
			POINT pt = {x, y};
			ScreenToClient(root->m_hwnd, &pt);
			x = pt.x;
			y = pt.y;

			// cooridinate of STYLE_POPUP window is relative to the vega screen, margin is irrelevant
			if (m_style != STYLE_POPUP)
			{
				x -= root->m_left_margin;
				y -= root->m_top_margin;
			}
		}

		MDE_OpWindow::SetInnerPos(x, y);
		return;
	}
	INT32 outer_x;
	INT32 outer_y;

	GetOuterPos(&outer_x, &outer_y);

	INT32 inner_x;
	INT32 inner_y;

	GetInnerPos(&inner_x, &inner_y);

	x += outer_x - inner_x;
	y += outer_y - inner_y;

	SetOuterPos(x, y);
}

void WindowsOpWindow::SetMinimumInnerSize(UINT32 width, UINT32 height)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::SetMinimumInnerSize(width, height);
		return;
	}
	m_minimum_width = width;
	m_minimum_height= height;
}

void WindowsOpWindow::SetMaximumInnerSize(UINT32 width, UINT32 height)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::SetMaximumInnerSize(width, height);
		return;
	}
	m_maximum_width = width;
	m_maximum_height= height;
}

void WindowsOpWindow::Activate(BOOL restore_from_minimized)
{
	if (!m_hwnd)
	{
		m_active = TRUE;
		MDE_OpWindow::Activate();
		return;
	}
	ReleaseCapture();

	HWND desktop_parent = GetDesktopParent();

	if (IsWindowVisible(desktop_parent))
	{
		if (IsIconic(desktop_parent))
			ShowWindow(desktop_parent, SW_RESTORE);

		SetActiveWindow(desktop_parent);
	}
	
	SetLockedByUser(m_locked_by_user);

	if (m_parent_window && m_parent_window->m_style == OpWindow::STYLE_WORKSPACE)
	{
		if (m_active)
			return;

		// deactivate other

		HWND child = GetWindow(m_parent_window->m_hwnd, GW_CHILD);

		while (child)
		{
			WindowsOpWindow *child_window = GetWindowFromHWND(child);

			if (child_window && child_window->m_active)
			{
				child_window->m_active = FALSE;

				SendMessage(child_window->m_hwnd, WM_NCACTIVATE, FALSE, 0);

				if (child_window->m_window_listener)
					child_window->m_window_listener->OnActivate(FALSE);
				break;
			}

			child = GetWindow(child, GW_HWNDNEXT);
		}

		m_active = TRUE;

		if (m_state == MINIMIZED && restore_from_minimized)
		{
			SetDesktopPlacement(m_restored_rect, m_old_state != RESTORED ? MAXIMIZED : RESTORED);
		}

		SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

		// only do this if we're really the foreground window

		if (GetDesktopParent() == GetForegroundWindow())
		{
			SendMessage(m_hwnd, WM_NCACTIVATE, TRUE, 0);

			::SetFocus(m_hwnd);
		}

		if (m_window_listener)
			m_window_listener->OnActivate(TRUE);
	}
}

void WindowsOpWindow::Deactivate()
{
	if (!m_hwnd)
	{
		m_active = FALSE;
		MDE_OpWindow::Deactivate();
		return;
	}
	if (!m_active || !m_parent_window || m_parent_window->m_style != OpWindow::STYLE_WORKSPACE)
		return;

	m_active = FALSE;
	SendMessage(m_hwnd, WM_NCACTIVATE, FALSE, 0);

	SetLockedByUser(m_locked_by_user);

	if (m_window_listener)
		m_window_listener->OnActivate(FALSE);
}

void WindowsOpWindow::Attention()
{
	if (GetForegroundWindow() == m_hwnd
		|| m_style != STYLE_DESKTOP
#ifdef WEBSERVER_SUPPORT
		&& (m_style != STYLE_GADGET || m_style != STYLE_HIDDEN_GADGET)
#endif
		|| m_parent_window)
	{
		return;
	}

	if (s_attention_timer == NULL)
	{
		s_attention_timer = OP_NEW(OpTimer, ());
		if (!s_attention_timer)
		{
			return;
		}

		s_attention_timer->SetTimerListener(&s_attention_timer_listener);
	}

	if (s_attention_list.First() == NULL)
	{
		s_attention_timer->Start(500);
	}

	Out();
	Into(&s_attention_list);

	m_flash_count = 5;
}

void WindowsOpWindow::AttentionTimerListener::OnTimeOut(OpTimer* timer)
{
	WindowsOpWindow* window = (WindowsOpWindow*) s_attention_list.First();

	while (window)
	{
		WindowsOpWindow* next = (WindowsOpWindow*) window->Suc();

		if (s_attention_inverted == window->m_inverted)
		{
			FlashWindow(window->m_hwnd, TRUE);
			window->m_inverted = !s_attention_inverted;
			window->m_flash_count--;

			if (window->m_flash_count <= 0)
			{
				window->Out();
			}
		}

		window = next;
	}

	s_attention_inverted = !s_attention_inverted;

	if (s_attention_list.First())
	{
		s_attention_timer->Start(500);
	}
}

void WindowsOpWindow::Raise()
{
	if (!m_hwnd)
	{
		MDE_OpWindow::Raise();
		return;
	}
	if (m_style == STYLE_CHILD || m_style == STYLE_NOTIFIER || m_style == STYLE_TOOLTIP || m_style == STYLE_POPUP || m_style == STYLE_GADGET || m_style == STYLE_HIDDEN_GADGET || m_style == STYLE_OVERLAY)
	{
		SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
	}
	else
	{
		Activate();
	}
}

void WindowsOpWindow::Lower()
{
	if (!m_hwnd)
	{
		MDE_OpWindow::Lower();
		return;
	}
	if (m_style == STYLE_CHILD || m_style == STYLE_NOTIFIER || m_style == STYLE_TOOLTIP || m_style == STYLE_POPUP || m_style == STYLE_GADGET || m_style == STYLE_HIDDEN_GADGET)
	{		
		SetWindowPos(m_hwnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		SetWindowPos(m_hwnd, HWND_BOTTOM, 0,0,0,0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	}
	DeactivateAndActivateTopmost();
}

void WindowsOpWindow::SetCursor(CursorType cursor)
{
	if (m_disable_cursor_change)
		return;

	HCURSOR hcursor = NULL;

	switch(cursor)
	{
	case CURSOR_AUTO:
	case CURSOR_DEFAULT_ARROW:
		hcursor = hArrowCursor;
		break;

	case CURSOR_URI:
	case CURSOR_CUR_POINTER:
	case CURSOR_DROP_LINK:
		hcursor = hLinkCursor;
		break;

	case CURSOR_WAIT:
		hcursor = hWaitCursor;
		break;

	case CURSOR_ARROW_WAIT:
		hcursor = hArrowWaitCursor;
		break;

	case CURSOR_HOR_SPLITTER:
		hcursor = hSplitterCursor;
		break;

	case CURSOR_VERT_SPLITTER:
		hcursor = hVSplitterCursor;
		break;

	case CURSOR_TEXT:
		hcursor = LoadCursor(NULL, IDC_IBEAM);
		break;

	case CURSOR_CROSSHAIR:
		hcursor = LoadCursor(NULL, IDC_CROSS);
        break;

	case CURSOR_ZOOM_IN:
		hcursor = LoadCursorA(hInst, "ZOOM_IN_CURSOR");
        break;

	case CURSOR_ZOOM_OUT:
		hcursor = LoadCursorA(hInst, "ZOOM_OUT_CURSOR");
        break;

	case CURSOR_MOVE:
		hcursor = LoadCursor(NULL, IDC_SIZEALL);
        break;

	case CURSOR_HELP:
		hcursor = LoadCursor(NULL, IDC_HELP);
        break;

	case CURSOR_NE_RESIZE:
	case CURSOR_SW_RESIZE:
	case CURSOR_NESW_RESIZE:
		hcursor = LoadCursor(NULL, IDC_SIZENESW);
        break;

	case CURSOR_NW_RESIZE:
	case CURSOR_SE_RESIZE:
	case CURSOR_NWSE_RESIZE:
		hcursor = LoadCursor(NULL, IDC_SIZENWSE);
		break;

	case CURSOR_CONTEXT_MENU:
		hcursor = LoadCursorA(hInst, "MENU_CURSOR");
		break;

	case CURSOR_NS_RESIZE:
	case CURSOR_N_RESIZE:
	case CURSOR_S_RESIZE:
		hcursor = hSplitterCursor;
        break;

	case CURSOR_W_RESIZE:
	case CURSOR_E_RESIZE:
	case CURSOR_EW_RESIZE:
		hcursor = hVSplitterCursor;
        break;

	case CURSOR_PROGRESS:
		hcursor = LoadCursor(NULL, IDC_APPSTARTING);
		break;

	case CURSOR_ALL_SCROLL:
		hcursor = LoadCursorA(hInst, "PAN_SUD");
		break;

	case CURSOR_VERTICAL_TEXT:
		hcursor = LoadCursorA(hInst, "VERT_CURSOR");
		break;

	case CURSOR_ALIAS:
		hcursor = LoadCursorA(hInst, "ALIAS_CURSOR");
		break;

	case CURSOR_COPY:
		hcursor = LoadCursorA(hInst, "COPY_CURSOR");
		break;

	case CURSOR_DROP_MOVE:
		hcursor = LoadCursorA(hInst, "DRAGMOVE");
		break;

	case CURSOR_DROP_COPY:
		hcursor = LoadCursorA(hInst, "DRAGCOPY");
		break;

	case CURSOR_NO_DROP:
		hcursor = LoadCursorA(hInst, "NODROP");
		break;

	case CURSOR_NOT_ALLOWED:
		hcursor = LoadCursor(NULL, IDC_NO);
		break;

	case CURSOR_CELL:
		hcursor = LoadCursorA(hInst, "CELL_CURSOR");
		break;

	case CURSOR_COL_RESIZE:
		hcursor = LoadCursorA(hInst, "COLRESIZE_CURSOR");
		break;

	case CURSOR_ROW_RESIZE:
		hcursor = LoadCursorA(hInst, "ROWRESIZE_CURSOR");
		break;

	case CURSOR_NONE:	// we don't implement this one, too annoying if widespread used
		return;

	default:
		OP_ASSERT(!"Cursor not supported, but it probably should!");
		return;
	}

	::SetCursor(hcursor);
}

CursorType WindowsOpWindow::GetCursor()
{
	return CURSOR_DEFAULT_ARROW;
}

const uni_char* WindowsOpWindow::GetTitle() const
{
	if (!m_hwnd)
	{
		return MDE_OpWindow::GetTitle();
	}
	static uni_char title[200];
	GetWindowText(m_hwnd, title, 200);
	return title;
}

void WindowsOpWindow::SetTitle(const uni_char* title)
{
	if (!m_hwnd)
	{
#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
		if(m_taskbar_proxy_wnd)
		{
			SetWindowText(m_taskbar_proxy_wnd, title);
		}
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
		MDE_OpWindow::SetTitle(title);
		return;
	}
	SetWindowText(m_hwnd, title);

}

HWND WindowsOpWindow::GetDesktopParent()
{
	HWND desktop_parent = GetParentHWND();

	if (desktop_parent)
	{
		while (GetParent(desktop_parent) && GetWindowLongPtr(desktop_parent, GWL_STYLE) & WS_CHILD)
		{
			desktop_parent = GetParent(desktop_parent);
		}
	}

	return desktop_parent;
}

void WindowsOpWindow::Enable()
{
	if (m_disabled_count > 0)
	{
		if (--m_disabled_count == 0)
		{
			EnableWindow(GetDesktopParent(), TRUE);
		}
	}
}

void WindowsOpWindow::Disable()
{
	if (m_disabled_count == 0)
	{
		EnableWindow(GetDesktopParent(), FALSE);
	}

	m_disabled_count++;
}

OUIMenuBar* WindowsOpWindow::GetMenuBar()
{
	if (m_menubar)
		return m_menubar;

	if (m_parent_window)
		return m_parent_window->GetMenuBar();

	MDE_OpView* parent_view = (MDE_OpView*)m_parent_view;

	while (parent_view)
	{
		if (parent_view->GetParentWindow())
			return ((WindowsOpWindow*)parent_view->GetParentWindow())->GetMenuBar();

		parent_view = parent_view->GetParentView();
	}

	return NULL;
}

void WindowsOpWindow::Restore()
{
	if (!m_hwnd)
	{
		MDE_OpWindow::Restore();
		return;
	}
	SetDesktopPlacement(m_restored_rect, RESTORED);
}

void WindowsOpWindow::Minimize()
{
	if (!m_hwnd)
	{
		MDE_OpWindow::Minimize();
		return;
	}
	SetDesktopPlacement(m_restored_rect, MINIMIZED);
}

void WindowsOpWindow::Maximize()
{
	if (!m_hwnd)
	{
		MDE_OpWindow::Maximize();
		return;
	}
	SetDesktopPlacement(m_restored_rect, MAXIMIZED, FALSE, TRUE);
}

void WindowsOpWindow::Fullscreen()
{
	if (!m_hwnd)
	{
		// default implementation in MDE_OpWindow asserts, so don't call it
		return;
	}
	SetDesktopPlacement(m_restored_rect, FULLSCREEN);
}

void WindowsOpWindow::LockUpdate(BOOL lock)
{
	if (!m_is_hidden && m_state != OpWindow::MINIMIZED)
		LockWindowUpdate(lock ? m_hwnd : NULL);
}

void WindowsOpWindow::SetSkin(const char* skin)
{
	m_skin = skin;
 	
	if (m_menubar)
	{
		m_menubar->SetSkin(m_skin);
	}
}

void WindowsOpWindow::Redraw()
{
	if (!m_hwnd)
	{
		MDE_OpWindow::Redraw();
		return;
	}
#ifdef GADGET_SUPPORT
    if (IsThisAGadget() && !HasDecorations())
    {
    }
    else
#endif // GADGET_SUPPORT
    {
        Resize(TRUE);
        RedrawWindow(m_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

void WindowsOpWindow::Resize(BOOL force)
{
	if (IsIconic(m_hwnd))
		return;

	RECT rect;

	GetClientRect(m_hwnd, &rect);

	INT32 new_width = rect.right;
	INT32 new_height = rect.bottom;

	if ((new_width == m_width && new_height == m_height) && !force)
		return;

	m_left_margin = m_right_margin = 0;
	if(GetFrame())
	{
		RECT ui_border = GetFrame()->GetUIFrameSizes();
		m_right_margin = ui_border.left + ui_border.right;
		m_bottom_margin = ui_border.bottom;
	}
	if (m_menubar)
	{
		m_menubar->ArrangeMenubar(&rect);

		m_top_margin = rect.top;
	}
	else
	{
		m_top_margin = 0;
	}

	if (m_style == OpWindow::STYLE_WORKSPACE)
	{
		HWND child = GetWindow(m_hwnd, GW_CHILD);

		while (child)
		{
			WindowsOpWindow *child_window = GetWindowFromHWND(child);

			if (child_window && (IsZoomed(child) || child_window->m_fullscreen))
			{
				RECT rect;

				GetClientRect(m_hwnd, &rect);
				AdjustWindowRectEx(&rect, child_window->m_win_style, 0, child_window->m_win_style_ex);

				MoveWindow(child, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
			}

			child = GetWindow(child, GW_HWNDNEXT);
		}


		RECT rect;

		GetClientRect(m_hwnd, &rect);
		rect.left = rect.right - (abs(new_width - m_width) + 2);
		InvalidateRect(m_hwnd, &rect, TRUE);

		GetClientRect(m_hwnd, &rect);
		rect.top = rect.bottom - (abs(new_height - m_height) + 2);
		InvalidateRect(m_hwnd, &rect, TRUE);
	}

	m_width = new_width;
	m_height = new_height;

	new_width -= m_left_margin+m_right_margin;
	if (new_width <= 0)
		new_width = 1;

	new_height -= m_top_margin+m_bottom_margin;
	if (new_height <= 0)
		new_height = 1;

	bool bReCreated = m_vega_window->wm_size(m_width, m_height);
	
	MDE_OpWindow::SetInnerSize(new_width, new_height);
	MDE_OpWindow::SetInnerPos(m_left_margin, m_top_margin);

	if(m_window_listener)
		m_window_listener->OnResize(new_width, new_height);

	if (m_state != MINIMIZED && bReCreated && m_sizemove==TRUE)
	{
		Validate();
		// Remove the pending WM_PAINT message for whatever reason is there
		// to avoid unnecessary redraw.
		::ValidateRect(m_hwnd, NULL);
	}
}

void WindowsOpWindow::UpdateBufferSize(unsigned int width, unsigned int height)
{
	if (m_vega_window->wm_size(width, height))
		m_mde_screen->Invalidate(MDE_MakeRect(0, 0, width, height), true);
}

void WindowsOpWindow::EnsureHiddenIconicState()
{
	if (!m_is_hidden && !m_is_closing && m_parent_window && m_parent_window->m_style == OpWindow::STYLE_WORKSPACE)
	{
		BOOL show = !IsIconic(m_hwnd);
		BOOL is_shown = IsWindowVisible(m_hwnd);

		if (show != is_shown)
		{
			if (!show && !m_parent_window->m_workspace_minimized_visibility)
			{
				ShowWindow(m_hwnd, show ? SW_SHOWNA : SW_HIDE);
			}
		}
	}
}

void WindowsOpWindow::DeactivateAndActivateTopmost(BOOL minimizing)
{
	if (m_parent_window && m_parent_window->m_style == OpWindow::STYLE_WORKSPACE && m_active)
	{
		WindowsOpWindow* window_to_activate = NULL;

		HWND child = GetWindow(m_parent_window->m_hwnd, GW_CHILD);
		/*WindowsOpWindow* iconic_child = NULL;*/

		while (child)
		{
			WindowsOpWindow *child_window = GetWindowFromHWND(child);

			if (child != m_hwnd && child_window)
			{
				if (!IsIconic(child) || m_parent_window->m_workspace_minimized_visibility)
				{
					window_to_activate = child_window;
					break;
				}
			}

			child = GetWindow(child, GW_HWNDNEXT);
		}

		if (window_to_activate)
		{
			window_to_activate->Activate(m_state == MAXIMIZED || m_state == FULLSCREEN);
		}
		else if (minimizing && !m_parent_window->m_workspace_minimized_visibility)
		{
			SendMessage(m_hwnd, WM_NCACTIVATE, FALSE, 0);
			m_active = FALSE;

			if (m_window_listener)
				m_window_listener->OnActivate(FALSE);
		}
	}
}

void WindowsOpWindow::OnPersonaLoaded(OpPersona *loaded_persona)
{
	if(!loaded_persona && m_background_clearer)
	{
		// ensure the background clearer is removed here
		if(m_background_clearer)
			g_skin_manager->RemoveTransparentBackgroundListener(m_background_clearer);
		OP_DELETE(m_background_clearer);
		m_background_clearer = NULL;
	}
	else if(m_background_clearer)
	{
		m_background_clearer->OnPersonaChanged(loaded_persona);
	}
	OnPagebarMoved(m_requested_titlebar_height>0, m_requested_extended_chrome_height);
}

void WindowsOpWindow::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_MENU_SETUP) && style == STYLE_DESKTOP && g_windows_menu_manager)
	{
		SetMenuBarVisibility(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu));
	}
	if(settings->IsChanged(SETTINGS_SKIN))
	{
		OnPagebarMoved(m_requested_titlebar_height>0, m_requested_extended_chrome_height);
	}
}

void WindowsOpWindow::SetTransparency(INT32 transparency)
{
	if (!m_hwnd)
	{
		MDE_OpWindow::SetTransparency(transparency);
	}
	else if (m_vega_window->GetIsLayered())
	{
		// SetWindowTransparent conflicts with layered window mode because it
		// set the blend mode to use the value only instead of per pixel alpha.
		// This will set a transparency value that will be combined with per pixel alpha when calling PPTBlt.
		m_vega_window->SetLayeredTransparency(transparency);
		OpRect rect(0, 0, m_width, m_height);
		m_vega_window->present(&rect, 1);
	}
	else
	{
		OP_ASSERT(!"You should not have ended up here.");
	}
}

void WindowsOpWindow::SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility)
{
	if (m_workspace_minimized_visibility == workspace_minimized_visibility)
		return;

	m_workspace_minimized_visibility = workspace_minimized_visibility;

	if (m_style == STYLE_WORKSPACE)
	{
		HWND child = GetWindow(m_hwnd, GW_CHILD);

		while (child)
		{
			if (IsIconic(child))
			{
				ShowWindow(child, m_workspace_minimized_visibility ? SW_SHOWNA : SW_HIDE);
			}

			child = GetWindow(child, GW_HWNDNEXT);
		}
	}
}

// static
BOOL WindowsOpWindow::IsDestroyingOpWindow(HWND hwnd)
{
	DWORD atom = GetClassLong(hwnd, GCW_ATOM);

	if (hwnd && IsOperaWindow(hwnd) && (atom == s_atom || atom == s_atom_ds))
	{
		return GetWindowLongPtr(hwnd, GWLP_USERDATA) ? FALSE : TRUE;
	}

	return FALSE;
}

// static
WindowsOpWindow* WindowsOpWindow::GetWindowFromHWND(HWND hwnd)
{
	DWORD atom = GetClassLong(hwnd, GCW_ATOM);

	if (hwnd && IsOperaWindow(hwnd) && (atom == s_atom || atom == s_atom_ds))
	{
		return (WindowsOpWindow*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	}

	return NULL;
}

// static
WindowsOpWindow* WindowsOpWindow::GetAdoptedWindowFromHWND(HWND hwnd)
{
	WindowsOpWindow *win = NULL;

	if(s_adopted_windows.GetData(hwnd, &win) == OpStatus::OK)
	{
		return win;
	}

	return NULL;
}

void WindowsOpWindow::SetMenuBarVisibility(BOOL visible)
{
	OP_ASSERT(style == STYLE_DESKTOP);

	if (g_windows_menu_manager && visible != m_menubar_visibility)
	{
		m_menubar_visibility = visible;

		g_windows_menu_manager->DestroyMenuBar(this);

		if (visible)
		{
			g_windows_menu_manager->CreateMenuBar(this, GetMenuName());

			if (m_menubar)
			{
				m_menubar->SetSkin(m_skin);
			}
		}
	}
	OnPagebarMoved(m_requested_titlebar_height>0, m_requested_extended_chrome_height);

	Redraw();
}

BOOL WindowsOpWindow::GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out)
{
	if (GetMenuBar())
	{
		GetMenuBar()->GetMenubarInfoItems(out); 
		return TRUE;
	}
	return FALSE;
}

void WindowsOpWindow::SetFloating( BOOL floating ) 
{	
	//Set/Reset the flag for keeping always on top
	HWND win_style = floating ? HWND_TOPMOST : HWND_NOTOPMOST;
	SetWindowPos(m_hwnd, win_style, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void WindowsOpWindow::SetAlwaysBelow( BOOL below ) 
{
	m_keep_below = below;
	if (IsSystemWinVista())
	{
		if (below)
			SetWindowPos(m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		else
			SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		uni_char* WINDOWS_PROGRAM_MANAGER_CLASS = UNI_L("Progman");
		uni_char* WINDOWS_PROGRAM_MANAGER_TITLE = UNI_L("Program Manager");

		HWND new_parent = NULL;
		if (below)
		{
			//with the program manager as parent, the window will stay below
			new_parent = FindWindow(WINDOWS_PROGRAM_MANAGER_CLASS , WINDOWS_PROGRAM_MANAGER_TITLE );
			OP_ASSERT(new_parent);  //Every Windows desktop should have a Program Manager running}
		}
		::SetParent(m_hwnd, new_parent);
		if (new_parent && !m_is_hidden)
			ShowWindow(m_hwnd,SW_SHOW); //on win95 and friends the gadget hides when set as child window of the Pogram Manager
	}
}

void WindowsOpWindow::SetShowInWindowList( BOOL in_window_list ) 
{
	OP_ASSERT(m_style == STYLE_GADGET); //this method is only working on gadgets because it toggle the toolwindow flags
	//Refresh taskbar (Hide and Show window) in between to be sure it updates its list
	BOOL refresh = IsWindowVisible(m_hwnd);

	//Calculate window style with or without taskbar item
	DWORD win_style_ex = GetWindowLongPtr(m_hwnd, GWL_EXSTYLE);
	win_style_ex = in_window_list ? win_style_ex & ~WS_EX_TOOLWINDOW : win_style_ex | WS_EX_TOOLWINDOW;
	
	//Update style
	if (refresh) ShowWindow(m_hwnd, SW_HIDE); 
	SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, win_style_ex);
	if (refresh) ShowWindow(m_hwnd, SW_SHOW); 
}

HICON WindowsOpWindow::GetIcon(int icon_size)
{
	OpStringC icon_filename = g_pcmswin->GetStringPref(PrefsCollectionMSWIN::ApplicationIconFile);
	if (!icon_filename.IsEmpty())

	if (icon_filename.HasContent())
	{
		if (!s_custom_icon && icon_size == ICON_BIG)
		{
			s_custom_icon = (HICON) LoadImage(hInst, icon_filename.CStr(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE );
		}
		if(!s_custom_icon_small && icon_size == ICON_SMALL)
		{
			s_custom_icon_small = (HICON) LoadImage(hInst, icon_filename.CStr(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE );
		}
	}
	switch (m_type)
	{
#ifdef PRODUCT_OPTYPEDOBJECT_DIALOG_FILE
		case OpTypedObject::DIALOG_TYPE_WIDGET_INSTALLER:
#endif // PRODUCT_OPTYPEDOBJECT_DIALOG_FILE
			{
				if(icon_size == ICON_BIG)
				{
					return s_custom_icon ? s_custom_icon : (HICON)LoadImage(hInst, UNI_L("ZWIDGET"), IMAGE_ICON, 32, 32, LR_SHARED | LR_CREATEDIBSECTION);
				}
				else
				{
					return s_custom_icon_small ? s_custom_icon_small : (HICON)LoadImage(hInst, UNI_L("ZWIDGET"), IMAGE_ICON, 16, 16, LR_SHARED | LR_CREATEDIBSECTION);
				}				
			}
		case OpTypedObject::DIALOG_TYPE_INSTALLER_WIZARD:
			{
				uni_char* icon_name;
				switch (g_desktop_product->GetProductType())
				{
					case PRODUCT_TYPE_OPERA_NEXT:
						icon_name = UNI_L("OPERA1NEXT");
						break;
					case PRODUCT_TYPE_OPERA_LABS:
						icon_name = UNI_L("OPERA2LABS");
						break;
					default:
						icon_name = UNI_L("OPERA");
						break;
				}


				// Prepare the opera icon just in case as a back-up solution.
				if (!s_custom_icon_small && (icon_size == ICON_SMALL))
					s_custom_icon_small = (HICON)LoadImage(hInst, icon_name, IMAGE_ICON, 16, 16, LR_SHARED | LR_CREATEDIBSECTION);
				if (!s_custom_icon && (icon_size == ICON_BIG))
					s_custom_icon = (HICON)LoadImage(hInst, icon_name, IMAGE_ICON, 32, 32, LR_SHARED | LR_CREATEDIBSECTION);

				// [1] If s_winstaller_icon == NULL, it is not initialized yet
				// [2] If s_winstaller_icon == INVALID_HANDLE_VALUE, we can't find the icon, use something else (ie s_custom_icon_small)
				// [3] Else use s_winstaller_icon

				if ((!s_winstaller_icon_small && (icon_size == ICON_SMALL)) || (!s_winstaller_icon && (icon_size == ICON_BIG)))
				{
					// Set it to invalid, so we won't try to load it more than one time if it fails.
					if (icon_size == ICON_SMALL)
						s_winstaller_icon_small = (HICON)INVALID_HANDLE_VALUE;
					else if (icon_size == ICON_BIG)
						s_winstaller_icon = (HICON)INVALID_HANDLE_VALUE;

					// Fallback. Store a reference to the icon we want to return if we can't find the installer icon.
					HICON temp_return_icon = icon_size == ICON_SMALL ? s_custom_icon_small : s_custom_icon;

					OpAutoPtr<ProcessItem> p; 
					HandleInfo h; 
					RETURN_VALUE_IF_ERROR(h.Init(), temp_return_icon);

					RETURN_VALUE_IF_ERROR(h.GetParentProcessItem(GetCurrentProcessId(), p), temp_return_icon);
					RETURN_VALUE_IF_NULL(p.get(), temp_return_icon);
				
					// If this is an elevated process, we have to check the parent of the parent.
					if (g_spawnPath.CompareI(p->GetProcessPath()) == 0)
					{
						DWORD PID = p->GetProcessID();
						p.reset();
						RETURN_VALUE_IF_ERROR(h.GetParentProcessItem(PID, p), temp_return_icon);
					}

					// Verify that it is a Opera binary we try to load the icon from.
					OpAutoPtr<LaunchPI> launch_pi = LaunchPI::Create();

					// Only verify the EXE if it is a release build. If not it will always fail,
					// since we are not signing the exe file in debug builds.
#ifndef _DEBUG
					if (launch_pi->VerifyExecutable(p->GetProcessPath()))
#endif // _DEBUG
					{
						HMODULE parent_module = LoadLibraryExW(p->GetProcessPath(), NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);

						if (icon_size == ICON_SMALL)
							s_winstaller_icon_small = (HICON)LoadImage(parent_module, MAKEINTRESOURCE(101), IMAGE_ICON, 16, 16, LR_SHARED | LR_CREATEDIBSECTION);
						else if (icon_size == ICON_BIG)
							s_winstaller_icon = (HICON)LoadImage(parent_module, MAKEINTRESOURCE(101), IMAGE_ICON, 32, 32, LR_SHARED | LR_CREATEDIBSECTION);
					}
				}
				
				if ((s_winstaller_icon_small == INVALID_HANDLE_VALUE) && icon_size == ICON_SMALL)
					return s_custom_icon_small;
				else if ((s_winstaller_icon == INVALID_HANDLE_VALUE) && icon_size == ICON_BIG)
					return s_custom_icon;

				return icon_size == ICON_SMALL ? s_winstaller_icon_small : s_winstaller_icon;
			}

		default:
			{
				uni_char* icon_name;
				switch (g_desktop_product->GetProductType())
				{
					case PRODUCT_TYPE_OPERA_NEXT:
						icon_name = UNI_L("OPERA1NEXT");
						break;
					case PRODUCT_TYPE_OPERA_LABS:
						icon_name = UNI_L("OPERA2LABS");
						break;
					default:
						icon_name = UNI_L("OPERA");
						break;
				}

				if(icon_size == ICON_BIG)
				{
					return s_custom_icon ? s_custom_icon : (HICON)LoadImage(hInst, icon_name, IMAGE_ICON, 32, 32, LR_SHARED | LR_CREATEDIBSECTION);
				}
				else
				{
					return s_custom_icon_small ? s_custom_icon_small : (HICON)LoadImage(hInst, icon_name, IMAGE_ICON, 16, 16, LR_SHARED | LR_CREATEDIBSECTION);
				}
			}
	}
}

const char* WindowsOpWindow::GetMenuName()
{
	switch (m_type)
	{
		case OpTypedObject::WINDOW_TYPE_BROWSER:
			return "Browser Menu Bar";

		case OpTypedObject::WINDOW_TYPE_MAIL_VIEW:
			return "Mail View Menu Bar";
	}

	return NULL;
}

void WindowsOpWindow::SetIcon(OpWidgetImage* img)
{
	if (!m_hwnd
#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
		&& !m_taskbar_proxy_wnd
#endif // !SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
		)
	{
		// We are not doing anything with the icon in this case at the moment,
		// so we don't want to allocate resources for it.
		return;
	}
	INT32 w, h;
	img->GetSize(&w, &h);
	if (w > 0 && h > 0)
	{
		if(img->HasBitmapImage())
		{
			// if we have a bitmap already, no need to create a new bitmap to paint into
			Image image = img->GetBitmapImage();

			OpBitmap *bitmap = image.GetBitmap(NULL);
			if(bitmap)
			{
				SetIcon(bitmap);

				image.ReleaseBitmap();
			}
		}
		else
		{
			OpBitmap* bitmap = 0;
			if (OpStatus::IsSuccess(OpBitmap::Create(&bitmap, w, h, FALSE, TRUE, 0, 0, TRUE)))
			{
				VisualDevice vd;
				vd.painter = bitmap->GetPainter();		// returns NULL on OOM
				if (vd.painter)
				{
					OpRect rect(0, 0, w, h);

					vd.ClearRect(rect);

					img->Draw(&vd, rect);

					vd.painter = NULL;
					bitmap->ReleasePainter();

					SetIcon(bitmap);
				}
				OP_DELETE(bitmap);
			}
		}
	}
}

void WindowsOpWindow::SetIcon(OpBitmap* bitmap)
{
	if (!m_hwnd)
	{
#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
		if(m_taskbar_proxy_wnd)
		{
			HICON icon = IconUtils::GetAsIcon(bitmap);
			HICON old_icon = (HICON)SendMessage(m_taskbar_proxy_wnd, WM_SETICON, ICON_SMALL, (LPARAM) icon);
			if(old_icon)
			{
				DestroyIcon(old_icon);
			}
			old_icon = (HICON)SendMessage(m_taskbar_proxy_wnd, WM_SETICON, ICON_BIG, (LPARAM) icon);
			if(old_icon)
			{
				DestroyIcon(old_icon);
			}
		}
#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS
		MDE_OpWindow::SetIcon(bitmap);
		return;
	}
	if (!bitmap)
	{
		HICON old_icon = (HICON)SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, NULL);
		if(old_icon)
		{
			DestroyIcon(old_icon);
		}
		old_icon = (HICON)SendMessage(m_hwnd, WM_SETICON, ICON_BIG, NULL);
		if(old_icon)
		{
			DestroyIcon(old_icon);
		}
	}
	else
	{
		HICON icon = IconUtils::GetAsIcon(bitmap);
		HICON old_icon = (HICON)SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon);
		if(old_icon)
		{
			DestroyIcon(old_icon);
		}
		old_icon = (HICON)SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM) icon);
		if(old_icon)
		{
			DestroyIcon(old_icon);
		}
	}
}

#ifdef GADGET_SUPPORT
OpView* WindowsOpWindow::GetBottomGadgetOPView()
{
    if (!m_gadget_opview_child)
        return NULL;

	// FIXME: VEGAOpPainter
	return NULL;
}

BOOL WindowsOpWindow::IsLayeredWindow()
{
	return m_vega_window->GetIsLayered();
}

#endif // GADGET_SUPPORT

BOOL WindowsOpWindow::IsActiveTopmostWindow()
{
	HWND desktop_parent = GetDesktopParent();
	if (!desktop_parent)
		desktop_parent = GetParentHWND();

	return (desktop_parent && (desktop_parent == ::GetForegroundWindow()));
}

#ifdef SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

long FAR PASCAL Windows7TaskbarWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowsOpWindow *window = (WindowsOpWindow*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if(!window)
	{
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	switch(message)
	{
		case WM_DWMSENDICONICTHUMBNAIL:
			{
				if(window->GetTaskbarMessageListener())
				{
					UINT32 width = HIWORD(lParam);
					UINT32 height = LOWORD(lParam);

					window->GetTaskbarMessageListener()->OnMessageSendIconicThumbnail(window, width, height);
				}
			}
			break;

		case WM_DWMSENDICONICLIVEPREVIEWBITMAP:
			{
				if(window->GetTaskbarMessageListener())
				{
					UINT32 width = HIWORD(lParam);
					UINT32 height = LOWORD(lParam);

					window->GetTaskbarMessageListener()->OnMessageSendIconicLivePreviewBitmap(window, width, height);
				}
			}
			break;

		case WM_ACTIVATE:
			{
				if(window->GetTaskbarMessageListener())
				{
					BOOL activate = LOWORD(wParam) != WA_INACTIVE;

					window->GetTaskbarMessageListener()->OnMessageActivateWindow(window, activate);
				}
			}
			break;

		case WM_CLOSE:
			{
				if(window->GetTaskbarMessageListener())
				{
					if(window->GetTaskbarMessageListener()->CanCloseWindow(window))
					{
						window->GetTaskbarMessageListener()->OnMessageCloseWindow(window);
					}
					else
					{
						// if the window is not closable (eg. it's locked by the user), don't call the default handler
						return 0;
					}
				}
			}
			break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void WindowsOpWindow::GetDocumentWidgetCoordinates(int& x, int& y)
{
	x = 0, y = 0;

	if (!m_hwnd)
	{
		GetMDEWidget()->ConvertToScreen(x, y);
	}
}

extern BOOL IsSystemWin7();

HWND WindowsOpWindow::GetTaskbarProxyWindow(BOOL create_if_missing)
{
	if(!IsSystemWin7())
	{
		return NULL;
	}
	if(!g_pcui->GetIntegerPref(PrefsCollectionUI::UseWindows7TaskbarThumbnails))
	{
		return NULL;
	}
	if(m_taskbar_proxy_wnd)
	{
		return m_taskbar_proxy_wnd;
	}
	else if(create_if_missing)
	{
		// create the taskbar proxy window here
		uni_char title[64];

		uni_char *p_title = (uni_char *)&title;

		if(m_hwnd)
		{
			GetWindowText(m_hwnd, title, 64);
		}
		else
		{
			p_title = const_cast<uni_char *>(MDE_OpWindow::GetTitle());
		}

		// Create invisible window (no WS_VISIBLE).
		m_taskbar_proxy_wnd = ::CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, UNI_L("OpTaskbarProxyWnd"), p_title,
			WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION, -32000, -32000, 10, 10, NULL, NULL, NULL/*hInstance*/, NULL);

		if(m_taskbar_proxy_wnd)
		{
			BOOL enable = TRUE; 

			HRESULT hr = OPDwmSetWindowAttribute(m_taskbar_proxy_wnd, DWMWA_FORCE_ICONIC_REPRESENTATION, &enable, sizeof(enable));
			OP_ASSERT(SUCCEEDED(hr));

			enable = TRUE; 

			hr = OPDwmSetWindowAttribute(m_taskbar_proxy_wnd, DWMWA_HAS_ICONIC_BITMAP, &enable, sizeof(enable));
			OP_ASSERT(SUCCEEDED(hr));

			SetWindowLongPtr(m_taskbar_proxy_wnd, GWLP_USERDATA, (LONG_PTR)this);
		}
	}
	return m_taskbar_proxy_wnd;
}

#endif // SUPPORT_WINDOWS7_TASKBAR_THUMBNAILS

