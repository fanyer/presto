/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef NATIVE_SKIN_ELEMENT_H
#define NATIVE_SKIN_ELEMENT_H

#include <stdint.h>

class NativeSkinElement
{
public:
	enum NativeType
	{
		NATIVE_NOT_SUPPORTED,
		NATIVE_PUSH_BUTTON,
		NATIVE_PUSH_DEFAULT_BUTTON,
		NATIVE_TOOLBAR_BUTTON,
		NATIVE_SELECTOR_BUTTON,
		NATIVE_MENU,
		NATIVE_MENU_BUTTON,
		NATIVE_MENU_RIGHT_ARROW,
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
		NATIVE_STATUS,
		NATIVE_MAINBAR,
		NATIVE_MAINBAR_BUTTON,
		NATIVE_PERSONALBAR,
		NATIVE_PERSONALBAR_BUTTON,
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
		NATIVE_DISCLOSURE_TRIANGLE,
		NATIVE_NOTIFIER,
		NATIVE_TOOLTIP,
		NATIVE_LISTITEM,
		NATIVE_SLIDER_HORIZONTAL_TRACK,
		NATIVE_SLIDER_HORIZONTAL_KNOB,
		NATIVE_SLIDER_VERTICAL_TRACK,
		NATIVE_SLIDER_VERTICAL_KNOB,
		NATIVE_MENU_SEPARATOR,
		NATIVE_LABEL,
		NATIVE_COUNT // Always the last, don't remove
	};

	struct NativeRect
	{
		int x;
		int y;
		int width;
		int height;

		NativeRect(int x_, int y_, int width_, int height_) : x(x_), y(y_), width(width_), height(height_) {}
	};

	enum NativeState
	{
		// States that result in cached bitmaps
		STATE_DISABLED		= 1 << 0,
		STATE_HOVER			= 1 << 1,
		STATE_PRESSED		= 1 << 2,
		STATE_SELECTED		= 1 << 3,
		STATE_FOCUSED		= 1 << 4,
		STATE_INDETERMINATE = 1 << 5,
		STATE_RTL			= 1 << 6,
		STATE_COUNT			= 1 << 7, // Always the last of bitmaps states, don't remove

		// States that do not trigger chaching
		STATE_TAB_FIRST         = 1 << 24,
		STATE_TAB_LAST          = 1 << 25,
		STATE_TAB_SELECTED      = 1 << 26,
		STATE_TAB_PREV_SELECTED = 1 << 27,
		STATE_TAB_NEXT_SELECTED = 1 << 28
	};

	virtual ~NativeSkinElement() {}

	/** Draw the element into a specified bitmap location
	  * @param bitmap Where to place bitmap data in ARGB format, ordered by line first (top to bottom) and by column second (left to right)
	  * @param width Width of bitmap
	  * @param height Height of bitmap
	  * @param clip_rect Which part should be drawn
	  * @param state state in which element should be drawn (ORed flags from NativeState)
	  */
	virtual void Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state) = 0;

	/** Change the default padding used for this element
	  * @param left [in/out] Left padding
	  * @param top [in/out] Top padding
	  * @param right [in/out] Right padding
	  * @param bottom [in/out] Bottom padding
	  * @param state State for which padding is requested (ORed flags from NativeState)
	  */
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state) = 0;

	/** Change the default margin used for this element
	  * @param left [in/out] Left margin
	  * @param top [in/out] Top margin
	  * @param right [in/out] Right margin
	  * @param bottom [in/out] Bottom margin
	  * @param state State for which margin is requested (ORed flags from NativeState)
	  */
	virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state) = 0;

	/** Change the default size used for this element
	  * @param width [in/out] Width of the element
	  * @param height [in/out] Height of the element
	  * @param state State for which size is requested (ORed flags from NativeState)
	  */
	virtual void ChangeDefaultSize(int& width, int& height, int state) = 0;

	/** Change the default text color used on this element
	  * @param red [out] Red component of color
	  * @param green [out] Green component of color
	  * @param blue [out] Blue component of color
	  * @param alpha [out] Alpha component of color
	  * @param state State for which text color is requested (ORed flags from NativeState)
	  */
	virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) = 0;

	/** Change the default spacing
	  * @param spacing [out] spacing Spacing
	  * @param state State for which spacing is requested
	  */
	virtual void ChangeDefaultSpacing(int& spacing, int state) = 0;
 
	/** Determine if element can be cached or not. Default behavior is to allow caching
	 *  @return Do not cache if true
	 */
	virtual bool IgnoreCache() { return false; }
};

#endif // NATIVE_SKIN_ELEMENT_H
