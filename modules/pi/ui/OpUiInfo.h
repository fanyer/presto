/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPUIINFO_H
#define OPUIINFO_H

class FontAtt;

enum OP_SYSTEM_COLOR {
	OP_SYSTEM_COLOR_BUTTON,						///< The face of a button
	OP_SYSTEM_COLOR_BUTTON_LIGHT,				///< The light border of a button
	OP_SYSTEM_COLOR_BUTTON_DARK,				///< The dark border of a button
	OP_SYSTEM_COLOR_BUTTON_VERYDARK,			///<
	OP_SYSTEM_COLOR_BUTTON_TEXT,				///< A buttons textcolor
	OP_SYSTEM_COLOR_BACKGROUND,					///< Background in most textfields
	OP_SYSTEM_COLOR_BACKGROUND_SELECTED,		///< Background where there is selected text
	OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS,///< Background where there is selected text when the widget has no focus
	OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED,		///< Background of text that is highlighted (after a search for instance)
	OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS,///< Background of text that is highlighted has no focus
	OP_SYSTEM_COLOR_BACKGROUND_DISABLED,
	OP_SYSTEM_COLOR_TEXT,						///< Textcolor in most textfields
	OP_SYSTEM_COLOR_TEXT_INPUT,					///< Textcolor in single-line textfields
	OP_SYSTEM_COLOR_TEXT_SELECTED,
	OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS,
	OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED,
	OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS,
	OP_SYSTEM_COLOR_TEXT_DISABLED,
	OP_SYSTEM_COLOR_ITEM_TEXT,					///< Text color in list boxes and combo boxes
	OP_SYSTEM_COLOR_ITEM_TEXT_SELECTED,			///< Text selection color in list boxes and combo boxes
	OP_SYSTEM_COLOR_SCROLLBAR_BACKGROUND,
	OP_SYSTEM_COLOR_DOCUMENT_NORMAL,			///< Normal document text
	OP_SYSTEM_COLOR_DOCUMENT_HEADER1,			///< First level header
	OP_SYSTEM_COLOR_DOCUMENT_HEADER2,			///< Second level header
	OP_SYSTEM_COLOR_DOCUMENT_HEADER3,			///< Third level header
	OP_SYSTEM_COLOR_DOCUMENT_HEADER4,			///< Fourth level header
	OP_SYSTEM_COLOR_DOCUMENT_HEADER5,			///< Fifth level header
	OP_SYSTEM_COLOR_DOCUMENT_HEADER6,			///< Sixth level header
	OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND,		///< Paper of document
	OP_SYSTEM_COLOR_DOCUMENT_PRE,				///< Preformatted document text
	OP_SYSTEM_COLOR_LINK,						///< Links
	OP_SYSTEM_COLOR_VISITED_LINK,				///< Visited links
	OP_SYSTEM_COLOR_UI_FONT,					///< UI font color
	OP_SYSTEM_COLOR_UI_DISABLED_FONT,			///< Disabled UI font color
	OP_SYSTEM_COLOR_UI_BACKGROUND,				///< UI background color
	OP_SYSTEM_COLOR_UI_BUTTON_BACKGROUND,		///< UI button background color
	OP_SYSTEM_COLOR_UI_PROGRESS,				///< UI progress color
	OP_SYSTEM_COLOR_UI_HOTLIST_TREE_FONT,		///< UI hotlist tree font color
	OP_SYSTEM_COLOR_UI_WINBAR_ATTENTION,		///< Window bar attention color
	OP_SYSTEM_COLOR_UI_BUTTON_HOVER, 			///< Button bar hover color
	OP_SYSTEM_COLOR_SKIN,						///< the "color" of the system's color scheme
	OP_SYSTEM_COLOR_TOOLTIP_BACKGROUND,			///< Tooltip background color
	OP_SYSTEM_COLOR_TOOLTIP_TEXT,				///< Tooltip foreground color
	OP_SYSTEM_COLOR_HTML_COMPOSE_TEXT,			///< HTML Compose Text Color
	OP_SYSTEM_COLOR_UI_MENU,					///< Textcolor in popup menus
	OP_SYSTEM_COLOR_WORKSPACE					///< Workspace background
};

/** Default system fonts. */
enum OP_SYSTEM_FONT {
	OP_SYSTEM_FONT_DOCUMENT_NORMAL,				///< Normal document text
	OP_SYSTEM_FONT_DOCUMENT_HEADER1,			///< First level header
	OP_SYSTEM_FONT_DOCUMENT_HEADER2,			///< Second level header
	OP_SYSTEM_FONT_DOCUMENT_HEADER3,			///< Third level header
	OP_SYSTEM_FONT_DOCUMENT_HEADER4,			///< Fourth level header
	OP_SYSTEM_FONT_DOCUMENT_HEADER5,			///< Fifth level header
	OP_SYSTEM_FONT_DOCUMENT_HEADER6,			///< Sixth level header
	OP_SYSTEM_FONT_DOCUMENT_PRE,				///< Preformatted document text
	OP_SYSTEM_FONT_FORM_TEXT,					///< Textcolor in most textfields
	OP_SYSTEM_FONT_FORM_TEXT_INPUT,				///< Textcolor in single-line textfields
	OP_SYSTEM_FONT_FORM_BUTTON,					///< The face of a button
	OP_SYSTEM_FONT_CSS_SERIF,
	OP_SYSTEM_FONT_CSS_SANS_SERIF,
	OP_SYSTEM_FONT_CSS_CURSIVE,
	OP_SYSTEM_FONT_CSS_FANTASY,
	OP_SYSTEM_FONT_CSS_MONOSPACE,
	OP_SYSTEM_FONT_EMAIL_COMPOSE,				///< Compose font
	OP_SYSTEM_FONT_HTML_COMPOSE,				///< HTML Compose font
	OP_SYSTEM_FONT_EMAIL_DISPLAY,				///< Main e-mail display font
	OP_SYSTEM_FONT_UI_MENU,						///< Popup menu and menu bar font
	OP_SYSTEM_FONT_UI_TOOLBAR,					///< Toolbar buttons and labels
	OP_SYSTEM_FONT_UI_DIALOG,					///< Dialogs
	OP_SYSTEM_FONT_UI_PANEL,					///< Hotlist panels
	OP_SYSTEM_FONT_UI_TOOLTIP,					///< Tooltips
	OP_SYSTEM_FONT_UI_ADDRESSFIELD,				///< Address field
	OP_SYSTEM_FONT_UI_HEADER,					///< UI Font that is nice when large (eg 24 px)
	OP_SYSTEM_FONT_UI_TREEVIEW,					///< Treeview
	OP_SYSTEM_FONT_UI_TREEVIEW_HEADER,			///< Treeview header
	// These one should be phased out
	OP_SYSTEM_FONT_UI,							///< UI font
	OP_SYSTEM_FONT_UI_DISABLED,					///< Disabled UI font
	OP_SYSTEM_FONT_UI_HOTLIST_TREE				///< UI hotlist tree font
};


/**
 * @short Interface definition for Opera abstract system info class
 *
 * A utility class to get information from the platform about the system ui properties.
 */

class OpUiInfo
{
public:
	/** Creates an OpUiInfo object */
	static OP_STATUS Create(OpUiInfo** new_opuiinfo);

	virtual ~OpUiInfo() {}

	/** Get the default width of a vertical scrollbar */
	virtual UINT32 GetVerticalScrollbarWidth() = 0;

	/** Get the default height of a vertical scrollbar */
	virtual UINT32 GetHorizontalScrollbarHeight() = 0;

	/** Get the color to use for the specified element type. */
	virtual UINT32 GetSystemColor(OP_SYSTEM_COLOR color) = 0;

	/** Get the font to use for the specified element type.
	 *
	 * This is used as the default setting and can be overridden using
	 * the preferences.
	 *
	 * @param font The font to retrieve.
	 * @param outfont FontAtt structure describing the font.
	 */
	virtual void GetSystemFont(OP_SYSTEM_FONT font, FontAtt &outfont) = 0;

#ifdef _QUICK_UI_FONT_SUPPORT_
	virtual void GetFont(OP_SYSTEM_FONT font, FontAtt &retval, BOOL use_system_default) = 0;
#endif // _QUICK_UI_FONT_SUPPORT_

	/** Get the color for the specified CSS system UI element.
	 *
	 * The system colors are defined in chapter 18 "User interface",
	 * section 2 "System colors" of the CSS 2.1 specification.
	 *
	 * @param css_color_value The system UI element, of type CSSValue
	 * @return The color to use for the specified color element
	 */
	virtual COLORREF GetUICSSColor(int css_color_value) = 0;

	/** Get the font for the specified CSS system UI element.
	 *
	 * The system fonts are defined in chapter 18 "User interface",
	 * section 3 "User preferences for fonts" of the CSS 2.1
	 * specification. The allowed values in CSS 2.1 are "caption",
	 * "icon", "menu", "message-box", "small-caption" and
	 * "status-bar".
	 *
	 * @param css_font_value The system UI element, of type CSSValue
	 * @param font (output) Will be set to the font to use for the
	 * specified UI element, unless FALSE is returned.
	 * @return Result of the operation, TRUE if success. If FALSE is
	 * returned, the content of the font output parameter is to be
	 * ignored.
	 */
	virtual BOOL GetUICSSFont(int css_font_value, FontAtt &font) = 0;

#ifndef MOUSELESS
	/** Is the mouse set up for right-handed or left-handed usage?
	 *
	 * @return TRUE if the mouse is right-handed, FALSE if it is
	 * left-handed. In left-handed mouse, the right mouse button is
	 * the standard button and the left is the context menu
	 * button. This setting affects the default value for the
	 * "left-handed back and forward gestures" setting.
	 */
	virtual BOOL IsMouseRightHanded() = 0;
#endif // !MOUSELESS

 	/** Is full keyboard access active?
	 *
	 * @return TRUE if full keyboard access is active. When FALSE,
	 * only edit fields and listboxes can get focus. When TRUE, all
	 * controls can get focus. Most implementations always return TRUE
	 * here.
	 */
	virtual BOOL IsFullKeyboardAccessActive() = 0;

	/** Get the width of the edit field caret.
	 *
	 * This is the width of the typically thin blinking thing inside of edit
	 * fields which indicates where keyboard input goes. Normally it is 1 pixel
	 * wide, but accessibility settings may override that.
	 *
	 * @return The width of the system caret in pixels.
	 */
	virtual UINT32 GetCaretWidth() = 0;

#ifdef PI_OPUIINFO_DEFAULT_BUTTON
	/** Determine whether the default button in a UI context should be on the
	  * right-hand side of the containing UI element (ie. Mac-style) or on
	  * the left-hand side (ie. Windows-style)
	  *
	  * @return Whether the default button should be on the right hand side
	  */
	virtual BOOL DefaultButtonOnRight() = 0;
#endif // PI_OPUIINFO_DEFAULT_BUTTON

#ifdef PI_UIINFO_TOUCH_EVENTS
	/**
	 * Returns whether touch events on web pages is desired. This is typically
	 * enabled if the platform has decided touch is available and wants
	 * to enable touch events support in core.
	 * @note If a touch device is connected, the platform may still decide
	 * to not expose touch events to web pages by letting this method return
	 * false.
	 */
	virtual bool IsTouchEventSupportWanted() = 0;
#endif // PI_UIINFO_TOUCH_EVENTS
};

#endif // OPUIINFO_H
