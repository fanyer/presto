/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef X11_SKIN_ELEMENT_H
#define X11_SKIN_ELEMENT_H

#include "modules/skin/OpSkinElement.h"

class X11SkinElement : public OpSkinElement
{
public:
	X11SkinElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size);
	virtual	~X11SkinElement();

	virtual OP_STATUS Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args);
	virtual OP_STATUS GetTextColor(UINT32* color, INT32 state);
	virtual OP_STATUS GetSize(INT32* width, INT32* height, INT32 state);
	virtual OP_STATUS GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
	virtual OP_STATUS GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
	virtual void OverrideDefaults(OpSkinElement::StateElement* se);
	virtual OP_STATUS GetSpacing(INT32* spacing, INT32 state);

	virtual BOOL IsLoaded() {return m_native_type == NATIVE_NOT_SUPPORTED ? OpSkinElement::IsLoaded() : TRUE;}

private:
	enum NativeType
	{
		NATIVE_NOT_SUPPORTED,			// for everyone else
		NATIVE_PUSH_BUTTON,
		NATIVE_PUSH_DEFAULT_BUTTON,
		NATIVE_TOOLBAR_BUTTON,
		NATIVE_SELECTOR_BUTTON,
		NATIVE_MENU,
		NATIVE_MENU_BUTTON,
		NATIVE_MENU_RIGHT_ARROW,
		NATIVE_MENU_SEPARATOR,
		NATIVE_POPUP_MENU,
		NATIVE_POPUP_MENU_BUTTON,
		NATIVE_HEADER_BUTTON,
		NATIVE_LINK_BUTTON,
		NATIVE_TAB_BUTTON,
		NATIVE_PAGEBAR_BUTTON,
		NATIVE_CAPTION_MINIMIZE_BUTTON,
		NATIVE_CAPTION_CLOSE_BUTTON,
		NATIVE_CAPTION_RESTORE_BUTTON,
		NATIVE_CHECKBOX,
		NATIVE_RADIO_BUTTON,
		NATIVE_DROPDOWN,
		NATIVE_DROPDOWN_BUTTON,
		NATIVE_DROPDOWN_LEFT_BUTTON,
		NATIVE_DROPDOWN_EDIT,
		NATIVE_EDIT,
		NATIVE_MULTILINE_EDIT,
		NATIVE_BROWSER,
		NATIVE_PROGRESSBAR,
		NATIVE_START,
		NATIVE_SEARCH,
		NATIVE_TREEVIEW,
		NATIVE_LISTBOX,
		NATIVE_CHECKMARK,
		NATIVE_BULLET,
		NATIVE_WINDOW,
		NATIVE_BROWSER_WINDOW,
		NATIVE_DIALOG,
		NATIVE_DIALOG_PAGE,
		NATIVE_DIALOG_TAB_PAGE,
		NATIVE_DIALOG_BUTTON_BORDER,
		NATIVE_TABS,
		NATIVE_HOTLIST,
		NATIVE_HOTLIST_SELECTOR,
		NATIVE_HOTLIST_SPLITTER,
		NATIVE_HOTLIST_PANEL_HEADER,
		NATIVE_SCROLLBAR_HORIZONTAL,
		NATIVE_SCROLLBAR_HORIZONTAL_KNOB,
		NATIVE_SCROLLBAR_HORIZONTAL_LEFT,
		NATIVE_SCROLLBAR_HORIZONTAL_RIGHT,
		NATIVE_SCROLLBAR_VERTICAL,
		NATIVE_SCROLLBAR_VERTICAL_KNOB,
		NATIVE_SCROLLBAR_VERTICAL_UP,
		NATIVE_SCROLLBAR_VERTICAL_DOWN,
		NATIVE_SLIDER_HORIZONTAL_TRACK,
		NATIVE_SLIDER_HORIZONTAL_KNOB,
		NATIVE_SLIDER_VERTICAL_TRACK,
		NATIVE_SLIDER_VERTICAL_KNOB,
		NATIVE_STATUS,
		NATIVE_MAINBAR,
		NATIVE_MAINBAR_BUTTON,
		NATIVE_PERSONALBAR,
		NATIVE_PAGEBAR,
		NATIVE_ADDRESSBAR,
		NATIVE_NAVIGATIONBAR,
		NATIVE_VIEWBAR,
		NATIVE_MAILBAR,
		NATIVE_CHATBAR,
		NATIVE_CYCLER,
		NATIVE_PROGRESS_DOCUMENT,
		NATIVE_PROGRESS_IMAGES,
		NATIVE_PROGRESS_TOTAL,
		NATIVE_PROGRESS_SPEED,
		NATIVE_PROGRESS_ELAPSED,
		NATIVE_PROGRESS_STATUS,
		NATIVE_PROGRESS_GENERAL,
		NATIVE_PROGRESS_CLOCK,
		NATIVE_PROGRESS_EMPTY,
		NATIVE_PROGRESS_FULL,
		NATIVE_IDENTIFY_AS,
		NATIVE_DIALOG_WARNING,
		NATIVE_DIALOG_ERROR,
		NATIVE_DIALOG_QUESTION,
		NATIVE_DIALOG_INFO,
		NATIVE_SPEEDDIAL,
		NATIVE_HORIZONTAL_SEPARATOR,
		NATIVE_VERTICAL_SEPARATOR,
		NATIVE_DISCLOSURE_TRIANGLE,
		NATIVE_SORT_ASCENDING,
		NATIVE_SORT_DESCENDING,
		NATIVE_PAGEBAR_CLOSE_BUTTON,
		NATIVE_NOTIFIER_CLOSE_BUTTON,
		NATIVE_TOOLTIP,
		NATIVE_NOTIFIER,
		NATIVE_EXTENDER_BUTTON,
		NATIVE_LIST_ITEM,
	};

private:
	void DrawDirectionButton(VisualDevice* vd, OpRect rect, int dir, INT32 state,
							 UINT32 fgcol, UINT32 gray, UINT32 lgray, UINT32 dgray, UINT32 dgray2);
	void DrawArrow(VisualDevice* vd, const OpRect& rect, int direction);
	void Draw3DBorder(VisualDevice* vd, UINT32 lcolor, UINT32 dcolor, OpRect rect, int omissions=0);
	void DrawCheckMark(VisualDevice* vd, UINT32 color, const OpRect& rect);
	void DrawIndeterminate(VisualDevice *vd, UINT32 color, const OpRect &rect);
	void DrawBullet(VisualDevice* vd, UINT32 color, const OpRect& rect);
	void DrawCloseCross(VisualDevice* vd, UINT32 color, const OpRect& rect);
	void DrawTab(VisualDevice* vd, UINT32 lcolor, UINT32 dcolor1, UINT32 dcolor2, const OpRect& rect, BOOL is_active);
	void DrawFrame(VisualDevice* vd, UINT32 lcolor, UINT32 dcolor1, UINT32 dcolor2, const OpRect& rect);
	void DrawTopFrameLine(VisualDevice* vd, UINT32 lcolor, UINT32 dcolor1, UINT32 dcolor2, const OpRect& rect);

private:
	NativeType	m_native_type;
};

#endif
