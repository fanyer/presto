/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
** File : WindowsOpSkinElement.h
**
**
*/
#ifndef WINDOWS_OP_SKINELEMENT_H_
#define WINDOWS_OP_SKINELEMENT_H_

#ifdef SKIN_NATIVE_ELEMENT

#include "modules/skin/OpSkinElement.h"

#define MAX_CACHED_SKIN_BITMAPS 5

// enable to cache native skin elements
//#define SKIN_CACHED_NATIVE_ELEMENTS

enum WindowsNativeType
{
	WINDOWS_NOT_SUPPORTED,			// for everyone else
	WINDOWS_PUSH_BUTTON,
	WINDOWS_PUSH_DEFAULT_BUTTON,
	WINDOWS_TOOLBAR_BUTTON,
	//			WINDOWS_SELECTOR_BUTTON,
	WINDOWS_HEADER_BUTTON,
	WINDOWS_LINK_BUTTON,
	WINDOWS_TAB_BUTTON,
	WINDOWS_PAGEBAR_BUTTON,
	WINDOWS_CAPTION_MINIMIZE_BUTTON,
	WINDOWS_CAPTION_CLOSE_BUTTON,
	WINDOWS_CAPTION_RESTORE_BUTTON,
	WINDOWS_CHECKBOX,
	WINDOWS_RADIO_BUTTON,
	WINDOWS_DROPDOWN,
	WINDOWS_DROPDOWN_BUTTON,
	WINDOWS_DROPDOWN_LEFT_BUTTON,
	WINDOWS_EDIT,
	WINDOWS_MULTILINE_EDIT,
	WINDOWS_BROWSER,
	WINDOWS_PROGRESSBAR,
	WINDOWS_START,
	WINDOWS_SEARCH,
	WINDOWS_TREEVIEW,
	WINDOWS_LISTBOX,
	WINDOWS_CHECKMARK,
	WINDOWS_BULLET,
	WINDOWS_WINDOW,
	WINDOWS_BROWSER_WINDOW,
	WINDOWS_DIALOG,
	WINDOWS_DIALOG_PAGE,
	WINDOWS_DIALOG_TAB_PAGE,
	WINDOWS_DIALOG_BUTTON_BORDER,
	WINDOWS_TABS,
	WINDOWS_HOTLIST,
	//			WINDOWS_HOTLIST_SELECTOR,
	WINDOWS_HOTLIST_SPLITTER,
//	WINDOWS_HOTLIST_PANEL_HEADER,
	WINDOWS_SCROLLBAR_HORIZONTAL,
	WINDOWS_SCROLLBAR_HORIZONTAL_KNOB,
	WINDOWS_SCROLLBAR_HORIZONTAL_LEFT,
	WINDOWS_SCROLLBAR_HORIZONTAL_RIGHT,
	WINDOWS_SCROLLBAR_VERTICAL,
	WINDOWS_SCROLLBAR_VERTICAL_KNOB,
	WINDOWS_SCROLLBAR_VERTICAL_UP,
	WINDOWS_SCROLLBAR_VERTICAL_DOWN,
	WINDOWS_SLIDER_HORIZONTAL_TRACK,
	WINDOWS_SLIDER_HORIZONTAL_KNOB,
	WINDOWS_SLIDER_VERTICAL_TRACK,
	WINDOWS_SLIDER_VERTICAL_KNOB,
	WINDOWS_STATUS,
	WINDOWS_MAINBAR,
	WINDOWS_PERSONALBAR,
	WINDOWS_PAGEBAR,
	WINDOWS_ADDRESSBAR,
	WINDOWS_NAVIGATIONBAR,
	WINDOWS_VIEWBAR,
	WINDOWS_MAILBAR,
	WINDOWS_CHATBAR,
	WINDOWS_CYCLER,
	WINDOWS_PROGRESS_DOCUMENT,
	WINDOWS_PROGRESS_IMAGES,
	WINDOWS_PROGRESS_TOTAL,
	WINDOWS_PROGRESS_SPEED,
	WINDOWS_PROGRESS_ELAPSED,
	WINDOWS_PROGRESS_STATUS,
	WINDOWS_PROGRESS_GENERAL,
	WINDOWS_PROGRESS_CLOCK,
	WINDOWS_PROGRESS_EMPTY,
	WINDOWS_PROGRESS_FULL,
	WINDOWS_IDENTIFY_AS,
	WINDOWS_DIALOG_WARNING,
	WINDOWS_DIALOG_ERROR,
	WINDOWS_DIALOG_QUESTION,
	WINDOWS_DIALOG_INFO,
	WINDOWS_HORIZONTAL_SEPARATOR,
	WINDOWS_VERTICAL_SEPARATOR,
	WINDOWS_DISCLOSURE_TRIANGLE,
	WINDOWS_SORT_ASCENDING,
	WINDOWS_SORT_DESCENDING,
	WINDOWS_PAGEBAR_CLOSE_BUTTON,
	WINDOWS_NOTIFIER_CLOSE_BUTTON,
	WINDOWS_TOOLTIP,
	WINDOWS_NOTIFIER,
	WINDOWS_EXTENDER_BUTTON,
	WINDOWS_PAGEBAR_FLOATING,
	WINDOWS_PAGEBAR_FLOATING_BUTTON,
	WINDOWS_PAGEBAR_THUMBNAIL_FLOATING_BUTTON,
	WINDOWS_DRAG_SCROLLBAR,
	WINDOWS_ADDRESSBAR_BUTTON,
	WINDOWS_LEFT_ADDRESSBAR_BUTTON,
	WINDOWS_RIGHT_ADDRESSBAR_BUTTON,
	WINDOWS_PERSONALBAR_BUTTON,
	WINDOWS_MAINBAR_BUTTON,
	WINDOWS_PAGEBAR_HEAD_BUTTON,
	WINDOWS_THUMBNAIL_PAGEBAR_HEAD_BUTTON,
	WINDOWS_PAGEBAR_TAIL_BUTTON,
	WINDOWS_THUMBNAIL_PAGEBAR_TAIL_BUTTON,
	WINDOWS_LISTITEM,
	WINDOWS_DROPDOWN_EDIT_SKIN
};

/***********************************************************************************
**
**	WindowsOpSkinElement
**
***********************************************************************************/
class WindowsOpSkinElement : public OpSkinElement
{
	public:

							WindowsOpSkinElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size);
		virtual				~WindowsOpSkinElement();

		virtual OP_STATUS	Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args);
		virtual OP_STATUS	GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
		virtual OP_STATUS	GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
		virtual OP_STATUS	GetSpacing(INT32* spacing, INT32 state);
		virtual OP_STATUS	GetTextColor(UINT32* color, INT32 state);
		virtual OP_STATUS	GetBorderWidth(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
		virtual OP_STATUS	GetSize(INT32* width, INT32* height, INT32 state);
		virtual void		OverrideDefaults(OpSkinElement::StateElement* se);

		virtual BOOL		IsLoaded() {return m_windows_native_type == WINDOWS_NOT_SUPPORTED ? OpSkinElement::IsLoaded() : TRUE;}
		BOOL				IsSupported() { return m_windows_native_type != WINDOWS_NOT_SUPPORTED; }

		static void			Flush() {m_theme_hash_table.DeleteAll();}

	private:

		class Theme : public OpString
		{
			public:

				Theme(const uni_char* theme_name, HTHEME theme_handle) : m_theme_handle(theme_handle) {Set(theme_name);}
				~Theme() {if (m_theme_handle) CloseThemeData(m_theme_handle);}

				const uni_char* GetThemeName() {return CStr();}
				HTHEME			GetThemeHandle() {return m_theme_handle;}

			private:
				HTHEME	m_theme_handle;
		};

		struct ThemeData
		{
			HTHEME	theme;
			INT32	part;
			INT32	part_state;
			INT32	edge_type;
			INT32	border_type;
			HDC		hdc;
			BOOL	empty;
		};

		HICON		GetIcon(WindowsNativeType type);
		HTHEME		GetTheme(const uni_char* theme_name);
		BOOL		GetThemeData(const INT32 state, RECT* draw_rect, ThemeData& theme_data, HDC hdc);

		BOOL		DrawThemeStyled(HDC hdc, INT32 state, RECT rect);
		BOOL		DrawOldStyled(HDC hdc, INT32 state, RECT rect);

		static INT32 GetPhysicalPixelFromRelative(INT32 relative_pixels);

		static OpAutoStringHashTable<Theme>	m_theme_hash_table;
		static UINT32 m_dpi;

#ifdef SKIN_CACHED_NATIVE_ELEMENTS
		HDC			m_cached_hdc;
		HBITMAP		m_cached_bitmap;
#endif // SKIN_CACHED_NATIVE_ELEMENTS

		HBITMAP		m_close_button_bitmap;
		WindowsNativeType	m_windows_native_type;

		OpBitmap* cachedBitmaps[MAX_CACHED_SKIN_BITMAPS];
		INT32 cachedStates[MAX_CACHED_SKIN_BITMAPS];
};

#endif // SKIN_NATIVE_ELEMENT

#endif // !WINDOWS_OP_SKINELEMENT_H_
