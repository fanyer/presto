/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifndef WINDOWS_WINDOWSBACKGROUNDCLEARER_H
#define WINDOWS_WINDOWSBACKGROUNDCLEARER_H

#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/utils/win_icon.h"
#include "modules/skin/skin_listeners.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/opskinmanager.h"

#define METHOD_CALL // OutputDebugStringA( __FUNCTION__ "\n" )

#if(WINVER < 0x0600)
#define WM_GETTITLEBARINFOEX            0x033F
typedef struct tagTITLEBARINFOEX
{
	DWORD cbSize;
	RECT rcTitleBar;
	DWORD rgstate[CCHILDREN_TITLEBAR + 1];
	RECT rgrect[CCHILDREN_TITLEBAR + 1];
} TITLEBARINFOEX, *PTITLEBARINFOEX, *LPTITLEBARINFOEX;
#endif

/*
* This is the implementation of the clear background hooks and assorted Persona-related 
* platform code for Windows. The clear background hooks are also used for toolbars
* that has the ClearBackground=1 attribute set in the Skin in the transparent sections.
* The hook is called during paint when the skin module detects that the background for
* some elements must be clearer.  If a persona is active, this clearing will actually
* paint the persona image in those areas.
*/

enum WindowBorderIcons
{
	WINICON_CLOSE,
	WINICON_CLOSE_HOVER,
	WINICON_CLOSE_PRESSED,
	WINICON_MAXIMIZE,
	WINICON_MAXIMIZE_HOVER,
	WINICON_MAXIMIZE_PRESSED,
	WINICON_MINIMIZE,
	WINICON_MINIMIZE_HOVER,
	WINICON_MINIMIZE_PRESSED,
	WINICON_RESTORE,
	WINICON_RESTORE_HOVER,
	WINICON_RESTORE_PRESSED,
	WINICON_CLOSE_RTL,
	WINICON_CLOSE_HOVER_RTL,
	WINICON_CLOSE_PRESSED_RTL,
	WINICON_MAXIMIZE_RTL,
	WINICON_MAXIMIZE_HOVER_RTL,
	WINICON_MAXIMIZE_PRESSED_RTL,
	WINICON_MINIMIZE_RTL,
	WINICON_MINIMIZE_HOVER_RTL,
	WINICON_MINIMIZE_PRESSED_RTL,
	WINICON_RESTORE_RTL,
	WINICON_RESTORE_HOVER_RTL,
	WINICON_RESTORE_PRESSED_RTL,
	WINICON_OVERLAY,
	WINICON_COUNT
};
#define WINICON_RTL_OFFSET (WINICON_CLOSE_RTL - WINICON_CLOSE)

class OpBorderButtons
{
private:
	OpBorderButtons() {}

public:
	OpBorderButtons(WindowsOpWindow *window, HTHEME theme) : 
		  m_window(window),
		  m_theme(theme) {}

	virtual ~OpBorderButtons() {}
	static OP_STATUS Construct(OpBorderButtons** obj, WindowsOpWindow *window, HTHEME theme, BOOL pagebar_on_top);
	static void CalculateCaptionButtonRects(WindowsOpWindow* window, RECT* btnrect);

	virtual OP_STATUS Init() = 0;
	virtual void Paint(HDC) = 0;
	virtual void Layout() = 0;
	virtual void DeleteButtons() = 0;
	virtual void GetSize(INT32* w, INT32* h) = 0;

protected:
	WindowsOpWindow*				m_window;
	HTHEME							m_theme;
};

enum WindowsRectOffsets
{
	WIN_RECT_CLOSE_BUTTON,
	WIN_RECT_MAXIMIZE_BUTTON,
	WIN_RECT_MINIMIZE_BUTTON,
	WIN_RECT_MAX
};

enum WindowsButtonId
{
	WINPART_NONE,
	WINPART_CLOSE,
	WINPART_MAXIMIZE,
	WINPART_MINIMIZE
};

/**
 *  This class handles the regular themed max/min/close buttons in the windows border
 */
class NativeBorderButtons : public OpBorderButtons, public OpWidget
{
	class NativeBorderButton
	{
		WindowsButtonId id;
		RECT btn_rect;

		inline WindowsRectOffsets ButtonIdToRectId();
		inline int ButtonIdToWindowPart(bool is_maximized);
		inline int GetCurrentButtonState(WindowsButtonId pushed, WindowsButtonId hovered, bool is_maximized, bool is_hwnd_active);
	public:
		NativeBorderButton(WindowsButtonId id): id(id) {}
		void SetRect(const RECT (&btnrects)[WIN_RECT_MAX])
		{
			btn_rect = btnrects[ButtonIdToRectId()];
		}
		const RECT& GetRect() const { return btn_rect; }

		void Paint(HTHEME theme, HDC hdc, WindowsButtonId pushed, WindowsButtonId hovered, bool is_maximized, bool is_hwnd_active);
	};

	WindowsButtonId m_pushed;
	WindowsButtonId m_hovered;
	NativeBorderButton m_btn_close, m_btn_maximize, m_btn_minimize;

	inline WindowsButtonId ButtonFromPoint(const OpPoint &point);

	// Retruns true, if the native buttons should not take part
	// in laying out, false otherwise. For more info, see comment
	// inside NativeBorderButtons::OnLayout
	bool ShouldNotLayOut();

public:
	NativeBorderButtons(WindowsOpWindow *window, HTHEME theme)
		: OpBorderButtons(window, theme)
		, m_pushed(WINPART_NONE)
		, m_hovered(WINPART_NONE)
		, m_btn_close(WINPART_CLOSE)
		, m_btn_maximize(WINPART_MAXIMIZE)
		, m_btn_minimize(WINPART_MINIMIZE)
	{
	}
	~NativeBorderButtons();

	OP_STATUS Init();
	void Paint(HDC dc);
	void DeleteButtons() { delete this; }
	void Layout() {}
	void GetSize(INT32* w, INT32* h);
	void OnLayout(OpWidget *widget);

	void OnMouseMove(OpWidget *widget, const OpPoint &point);
	void OnMouseLeave(OpWidget *widget);
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
};

/**
 *  This class handles the persona max/min/close buttons in the windows border
 */
class QuickBorderButtons : public OpBorderButtons, public OpWidget
{
public:
	QuickBorderButtons(WindowsOpWindow *window, HTHEME theme) : OpBorderButtons(window, theme), 
			m_btn_minimize(NULL),
			m_btn_restore(NULL),
			m_btn_close(NULL),
			m_language_is_RTL(false)
	{
		for(int x = 0; x < WINICON_COUNT; x++)
		{
			m_content_providers[x] = NULL;
		}
	}
	virtual ~QuickBorderButtons();
	OP_STATUS Init();

	void Paint(HDC) {} // we'll be painted by the widget container when needed, no need for us to do anything

	void DeleteButtons() { OpWidget::Delete(); }	// make sure we are deleted with the widget's delete method as it unwinds the stack first

	void Layout();

	// assumes the resource is in the resource section as RCDATA in PNG format
	static Image CreateReplacementWindowButton(QuickBorderButtons* btn_class, WindowBorderIcons icon_offset, const uni_char *resource_name);

private:
	Image GetWindowControlImage(WindowBorderIcons image) { return m_window_icons[image]; }

	void GetSize(INT32* w, INT32* h);

	// OpWidgetListener
	void OnLayout(OpWidget *widget) 
	{
		if (widget == this)
		{
			LayoutCustomWindowButtons();
		}
	}
	void OnClick(OpWidget *widget, UINT32 id = 0);
	void SetButtonImage(OpWidget *widget, BOOL hover, BOOL pressed = FALSE);

	// as we use manually decoded bitmaps for the windows control buttons, we need to switch them out manually
	void OnMouseMove(OpWidget *widget, const OpPoint &point) 
	{
		SetButtonImage(widget, TRUE, GetVisualDevice()->GetView()->GetMouseButton(MOUSE_BUTTON_1));
	}
	void OnMouseLeave(OpWidget *widget) 
	{
		SetButtonImage(widget, FALSE);
	}
	void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks) 
	{
		if(button == MOUSE_BUTTON_1 && down)
		{
			SetButtonImage(widget, FALSE, TRUE);
		}
	}
	void LayoutCustomWindowButtons();

private:
	Image							m_window_icons[WINICON_COUNT];	// all the icons replacing the standard ones on Windows when using a persona
	OpButton*						m_btn_minimize;			// the alternative minimize, restore and close buttons when using a personas
	OpButton*						m_btn_restore;
	OpButton*						m_btn_close;
	ImageContentProvider*			m_content_providers[WINICON_COUNT];	// The content providers for the icons
	bool							m_language_is_RTL;
};

class WindowsBackgroundClearer : public TransparentBackgroundListener
{
protected:
	// needed for derived classes that do not want all the init in our usual constructor
	WindowsBackgroundClearer();

public:

	WindowsBackgroundClearer(OpWindow* win, BOOL use_composition, BOOL use_transparent_state, BOOL menubar_visibility, BOOL pagebar_on_top);

	virtual ~WindowsBackgroundClearer();

	static WindowsBackgroundClearer* WindowsBackgroundClearer::Create(WindowsOpWindow* win, BOOL use_transparent_state, BOOL use_composition, BOOL pagebar_on_top);

	virtual void OnBackgroundCleared(OpPainter *painter, const OpRect& clear_rect);
	static void PaintBackground(unsigned int width, unsigned int height, WindowsOpWindow *window, VEGAOpPainter *painter, VisualDevice *vd, const OpRect& paint_rect, BOOL inactive = FALSE);
	BOOL HasPersonas() { return m_has_painted_personas; }
	BOOL IsClearingBackground() { return m_clear_background || g_skin_manager->HasPersonaSkin(); }
	BOOL GetMenubarVisibility() { return m_menubar_visibility; }
	void GetBorderButtonsSize(INT32 *w, INT32 *h);

	void OnPersonaChanged(OpPersona *persona);

	enum WinClearerType
	{
		WIN_CLEARER_TYPE_NONE,
		WIN_CLEARER_TYPE_NORMAL,
		WIN_CLEARER_TYPE_CLASSIC,
		WIN_CLEARER_TYPE_DUMMY

	};
	/* These methods are used to check if the callback was created with certain parameters
	* of is of a certain type.
	* This is used to determine if they need to be recreated or if we can just keep using
	* the existing one.
	*/
	virtual WinClearerType	GetClearerType() { return WIN_CLEARER_TYPE_NORMAL; }
	virtual BOOL			HasTheme() { return m_theme != NULL; }
	virtual BOOL			HasComposition() { return m_composition; }
	virtual BOOL			HasTransparentState() { return m_use_transparent_state; }
	BOOL					IsPagebarOnTop() { return m_pagebar_on_top; }
	virtual void            UpdateFrameLanguageDirection() {}

	/*
	 * Install a clip rect for the HDC that points to the vega backbuffer native bitmap
	 */
	class VegaBufferClipRect
	{
	public:
		VegaBufferClipRect(HDC dc, WindowsOpWindow* window, OpRect& rect, VEGAPixelStore* ps) : m_dc(dc)
		{
			// Clip the invisible rect, this is important as we don't do bound check when operating ps->buffer
			// Fix for DSK-312213
			rect.IntersectWith(OpRect(0,0,ps->width,ps->height));
			IntersectClipRect(dc, rect.x, rect.y, rect.Right(), rect.Bottom());
		}
		~VegaBufferClipRect()
		{
			SelectClipRgn(m_dc, NULL);
		}
	private:
		HDC m_dc;
	};

private:
	void drawGradient(VEGARenderer* rend, UINT32 color, unsigned int width, unsigned int height);

protected:
	// Set up the memory buffer and DC which we can draw native theme skin on
	VEGAPixelStore* SetupBackBuffers(HDC& dc, UINT32 winw, UINT32 winh);

	WindowsOpWindow*				m_window;
	BOOL							m_has_painted_personas;	// set first time we paint a personas

private:
	HTHEME							m_theme;
	BOOL							m_composition;			// TRUE if we're using composition. We don't need to clear so much if we do
	BOOL							m_clear_background;		// TRUE if we need to clear the background
	BOOL							m_use_transparent_state;
	BOOL							m_menubar_visibility;	// was the menu bar visible when this hook was created
	BOOL							m_pagebar_on_top;		// is the pagebar on top
	OpBorderButtons					*m_border_buttons;
	HDC								m_hwBmpDC;
	HBITMAP							m_hwBmp;
	HBITMAP							m_hwOldBmp;
	UINT32*							m_hwBmpData;
	OpBitmap*						m_hwOpBitmap;
	VEGAPixelStore					m_hwPs;
};

class DummyBackgroundClearer : public WindowsBackgroundClearer
{
public:
	DummyBackgroundClearer(OpWindow* win, BOOL use_composition, BOOL use_transparent_state, BOOL menubar_visibility, BOOL pagebar_on_top)	{ }

	virtual WinClearerType GetClearerType() { return WIN_CLEARER_TYPE_DUMMY; }

	virtual void OnBackgroundCleared(OpPainter *painter, const OpRect& clear_rect) { }

	virtual BOOL HasTheme() { return FALSE; }
	virtual BOOL HasComposition() { return FALSE; }
};

/** Implementation of WindowsBackgroundClearer that works for classic ui (non-themed) looks */
class WindowsClassicBackgroundClearer : public WindowsBackgroundClearer
{
public:
	WindowsClassicBackgroundClearer(OpWindow* win, BOOL use_composition, BOOL use_transparent_state, BOOL menubar_visibility, BOOL pagebar_on_top);

	virtual WinClearerType GetClearerType() { return WIN_CLEARER_TYPE_CLASSIC; }

	virtual void OnBackgroundCleared(OpPainter *painter, const OpRect& clear_rect);
};


#endif // WINDOWS_WINDOWSBACKGROUNDCLEARER_H
