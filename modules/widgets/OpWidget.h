/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_WIDGET_H
#define OP_WIDGET_H

#include "modules/pi/OpInputMethod.h"
#include "modules/util/simset.h"

class VisualDevice;
class Window;
class HTMLayoutProperties;

#ifdef NAMED_WIDGET
class OpNamedWidgetsCollection;
#endif // NAMED_WIDGET

#ifdef INTERNAL_SPELLCHECK_SUPPORT
class OpEditSpellchecker;
#endif // INTERNAL_SPELLCHECK_SUPPORT

#include "modules/display/cursor.h"
#include "modules/display/vis_dev_transform.h" // AffinePos

#include "modules/pi/OpWindow.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "modules/accessibility/opaccessibleitem.h"
class AccessibleOpWidgetLabel;
#endif

#include "modules/inputmanager/inputcontext.h"
#include "modules/widgets/OpEditCommon.h"
#include "modules/widgets/OpWidgetPainterManager.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

#ifdef SKIN_SUPPORT
# include "modules/skin/OpWidgetImage.h"
#endif // SKIN_SUPPORT

#include "modules/widgets/optextfragmentlist.h"

#ifdef WIDGETS_IMS_SUPPORT
#include "modules/widgets/OpIMSObject.h"
#endif // WIDGETS_IMS_SUPPORT

#ifdef USE_OP_CLIPBOARD
# include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

// Will probably be turned into tweaks at some point.. Trying to clean up things.
#if defined(QUICK) || defined(GOGI)
#define WIDGETS_UI
#endif


#define WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP

// How to modify the look & feel
//
// OpWidgetPainter.
//     Inherit to make a entire new painter for the widgets. The default
//     widgetpainter supports CSS. Any function can return FALSE to
//     fallback to the original CssWidgetPainter.
//
//     To support CSS you just need to NOT draw borders or backgrounds if
//     OpWidget::HasCssBorder.. is TRUE. The actual drawing of CSS is done by core.
//
//     A widgetpainter is set on the global paintermanager.
//
// OpWidgetInfo.
//     Inherit your own version and override what you want
//     to change the behaviours or the size of the widgets.
//     Return it from OpWidgetPainter::GetInfo().
//
// Additional info
//    -IndpWidgetPainter draws skinned forms.
//    -Editing and all kind of keyboard interaction is specified
//     with the input.ini.
//    -The colors used by default in the CssWidgetPainter is the
//     systemcolors in OpSystemInfo.
//
// Need more?
//     emil@opera.com
//

class AutoCompletePopup;
class OpWidget;
class OpButton;
class OpRadioButton;
class OpCheckBox;
class OpListBox;
class OpScrollbar;
class OpToolbar;
class OpSplitter;
class OpTreeView;
class OpEdit;
class OpMultilineEdit;
class OpResizeCorner;
class OpBrowserView;
class OpFileChooserEdit;
class OpDropDown;
class OpFontInfo;
class OpTreeModel;
class OpStringItem;
class OpWidgetString;
class OpBitmap;
class FormObject;
class WidgetContainer;
class OpDragObject;
class DesktopWindow;
class OpWorkspace;
class OpItemSearch;
class OpIMSObject;
class OpIMSUpdateList;
class OpCalendar;

#define REPORT_AND_RETURN_IF_ERROR(status)	if (OpStatus::IsError(status)) \
											{ \
												if (OpStatus::IsMemoryError(status)) \
													ReportOOM(); \
												return; \
											}

#define CHECK_STATUS(status)	if (OpStatus::IsError(status)) \
								{ \
									init_status = status; \
									return; \
								}

#define DEFINE_CONSTRUCT(objecttype)		OP_STATUS objecttype::Construct(objecttype** obj) \
											{ \
												*obj = OP_NEW(objecttype, ()); \
												if (*obj == NULL) \
													return OpStatus::ERR_NO_MEMORY; \
												if (OpStatus::IsError((*obj)->init_status)) \
												{ \
													OP_DELETE(*obj); \
													return OpStatus::ERR_NO_MEMORY; \
												} \
												return OpStatus::OK; \
											}

#ifdef WIDGETS_IME_SUPPORT
/** Input method info */
struct IME_INFO {
	INT16 start, stop;			///< Position of eventual inputmethodstring.
	INT16 can_start, can_stop;	///< Position of eventual candidate in inputmethodstring.
	OpInputMethodString* string;
};
#endif // WIDGETS_IME_SUPPORT


/** A collection of all the globals that the widgetmodule use. */
struct WIDGET_GLOBALS {

	// For OpWidget
	OpPoint start_point; ///< used to decide threshold for drag-n-drop
	OpPoint last_mousemove_point; ///< last mouse position over the last hovered widget
	OpWidget* hover_widget; ///< Current hovering widget
#ifdef DRAG_SUPPORT
	OpWidget* drag_widget; ///< Current drag source or drog target widget
#endif // DRAG_SUPPORT
#ifndef MOUSELESS
	/**
	 * A captured widget will receieve all mousemoves and the mouseup
	 * event even if those happens outside the widget. It's typically
	 * triggered by a mousedown "arming" the widget.
	 */
	OpWidget* captured_widget;
#endif // !MOUSELESS

#ifdef SKIN_SUPPORT
	/**
	 * The widget that is currently painted with skin elements.
	 */
	OpWidget* skin_painted_widget;

	/**
	 * Additional widget states of the widget that is currently
	 * painted with skin elements. The information is intended for
	 * native skin drawing code.
	 */
	INT32 painted_widget_extra_state;
#endif

	// For OpWidgetString
	uni_char * passwd_char; ///< the character to show instead of the typed character in password fields
	const uni_char * dummy_str; ///< should always be empty, used to avoid doing null-checks and such
#ifdef WIDGETS_IME_SUPPORT
	IME_INFO ime_info;
#endif

	// OpDropDown
	/**
	 X11 sends two mouse events when a popup window is closed. We have to
	 block the second when the mouse clicked on the dropdown arrow when
	 the dropdown was already visible.
	 The pointer itself is never used, we just compare it with the 'this'
	 pointer
	 */
	OpDropDown* m_blocking_popup_dropdown_widget;

	// Op(Multi)Edit-related

	// Instances of all CharRecognizer subclasses
	WordCharRecognizer          word_char_recognizer;
	WhitespaceCharRecognizer    whitespace_char_recognizer;
	WordDelimiterCharRecognizer word_delimiter_char_recognizer;

	// char_recognizers is an array containing all instances of
	// CharRecognizer subclasses
	CharRecognizer* char_recognizers[N_CHAR_RECOGNIZERS];

	/**
	 X11 sends two mouse events when a popup window is closed. We have to
	 block the second when the mouse clicked on the dropdown arrow when
	 the dropdown was already visible.
	 The pointer itself is never used, we just compare it with the 'this'
	 pointer
	 */
	OpCalendar* m_blocking_popup_calender_widget;

	// OpListBox
	BOOL had_selected_items; ///< fix for OnChange if no items where selected in a dropdown. (If selectedindex was set to -1)
	BOOL is_down; ///< keep track on whether a listbox widget has received mouse down but not mouse up
	OpItemSearch* itemsearch; ///< used for keyboard input searching in listbox

	// AutoCompletePopup
	AutoCompletePopup* curr_autocompletion; ///< for autocompletion
	BOOL autocomp_popups_visible; ///< for autocompletion
};

/**
 Listener for events used by all widgets
 */
class OpWidgetListener
{
public:
	virtual ~OpWidgetListener() {}; ///< destructor, does nothing

	/** Sent by OpWidget when widget is enabled/disabled */
	virtual void OnEnabled(OpWidget *widget, BOOL enabled) {};

	/** Sent by OpWidget when widget's contents should be laid out*/
	virtual void OnLayout(OpWidget *widget) {};

	/** Send when relayout occurs */
	virtual void OnRelayout(OpWidget* widget) {};

      /** Send when post-layout occurs */
    virtual void OnLayoutAfterChildren(OpWidget* widget) {};

	/** Used by all objects */
	virtual void OnClick(OpWidget *widget, UINT32 id = 0) { }

	/** Widget has been painted */
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}

	/** Send before wigdet is painted */
	virtual void OnBeforePaint(OpWidget *widget) { }

	/**
	 * Used by button widgets in addition to OnClick to inform the
	 * listener that a click was generated internally in the button,
	 * maybe as a response to a keypress or action.
	 */
	virtual void OnGeneratedClick(OpWidget *widget, UINT32 id = 0) { }

	/** Used by OpEdit, OpMultilineEdit, OpListbox, OpCombobox, OpTreeView */
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE) {};

	/** Used by OpEdit, OpMultilineEdit
		Should be called if the widget has been changed when it loses the focus (since it got the focus).
	*/
	virtual void OnChangeWhenLostFocus(OpWidget *widget) {};

	/**
	 * Same as OnMouseEvent, but always called before the widget gets time to
	 * handle the mouse event. May consume the mouse event.
	 * Only called for OpEdit and OpMultiEdit.
	 * @return TRUE if the mouse event was consumed by the listener and the widget should
	 *         not continue processing it, FALSE otherwise.
	 */
	virtual BOOL OnMouseEventConsumable(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks) { return FALSE; }

	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks) { }

	/** Called when the mouse pointer moves over the widget.
		@param widget the widget in question
	*/
	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point) {};

	/** Called when the mouse pointer leaves the widget.
		@param widget the widget in question
	*/
	virtual void OnMouseLeave(OpWidget *widget) {}

	/**
	 * User requested popup menu (right mouse up, or keyboard etc)
	 * @param widget The widget on top of which the popup is opened
	 * @param child_index The index of the child widget, e.g. in a toolbar, that is sending the OnContextMenu request
	 * @param menu_point The position (relative to the widget) where the popup should appear
	 * @param avoid_rect The area that should not be covered by the popup. NULL if not needed.
 	 * @param keyboard_invoked TRUE if context menu is invoked by keypress (as opposed to mouse).
	 */
	virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked) { return FALSE; }

	/** Used by OpEdit, OpMultilineEdit when the selection changes */
	virtual void OnSelect(OpWidget *widget) {};

	/** Used by OpScrollbar
	caused_by_input is FALSE if the scroll is caused by f.ex. a SetLimit which shrinks the position, or a SetValue
	called by YOU. */
	virtual void OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input) {};

#ifdef NOTIFY_ON_INVOKE_ACTION
	/** Used by OpEdit to broadcast that an action will now be invoked */
	virtual void OnInvokeAction(OpWidget *widget, BOOL invoked_by_mouse) {}
#endif // NOTIFY_ON_INVOKE_ACTION

#ifdef QUICK
	/** Used by OpTreeView */
	virtual void OnItemChanged(OpWidget *widget, INT32 pos) {};
	virtual void OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text) {};
	/* Returns TRUE if change can be accepted, @ref OnItemEdited is called immediately after this */
	virtual BOOL OnItemEditVerification(OpWidget *widget, INT32 pos, INT32 column, const OpString& text) { return TRUE; };
	virtual void OnItemEditAborted(OpWidget *widget, INT32 pos, INT32 column, OpString& text) {};
	virtual void OnSortingChanged(OpWidget *widget) {};

	/** Should generally be used by all so that we can provide better UI feedback */
	virtual void OnFocusChanged(OpWidget *widget, FOCUS_REASON reason) {}
#endif

#ifdef DRAG_SUPPORT
	/** Used for drag'n'drop */
	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) {}
	virtual void OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) {}
	virtual void OnDragLeave(OpWidget* widget, OpDragObject* drag_object) {};
	virtual void OnDragCancel(OpWidget* widget, OpDragObject* drag_object) {};
#endif // DRAG_SUPPORT

	/** Called from widgets that receive lowlevel key events
	    to check if the key input should be accepted.

	    @param value The character value string produced by the key press/input.
	    @return TRUE if the input should be accepted as consumed, FALSE
	            if it shouldn't be. */
	virtual BOOL OnCharacterEntered( const uni_char *value ) { return TRUE; }

	/** Called with show TRUE *before* the dropdown occurs.
		Called with show FALSE *after* the dropdown has been closed.
	*/
	virtual void OnDropdownMenu(OpWidget *widget, BOOL show) {}

	/** May be passed to OnResizeRequest() to indicate no change in size. */
	static const INT32 NO_RESIZE = -1;

	/** Called when a resize is requested by the user.

	    The dimensions are "border-box" dimensions, which means that they
	    represent the actual footprint the OpWidget should end up with on
	    the page, including padding and borders.

	    @param widget The OpWidget that was resized.
	    @param w The new width of the OpWidget. (Use NO_RESIZE to leave size unchanged).
	    @param h The new height of the OpWidget. (Use NO_RESIZE to leave size unchanged). */
	virtual void OnResizeRequest(OpWidget *widget, INT32 w, INT32 h) {};
};

/**
	Listener for misc widgets events, set globally for entire opera. Several instances of this listener can be set.
*/
class OpWidgetExternalListener : public Link
{
public:
	/** widget has received or lost focus. */
	virtual void OnFocus(OpWidget *widget, BOOL focus, FOCUS_REASON reason) {}

	/** Widget will be deleted sometime very soon after this call. */
	virtual void OnDeleted(OpWidget *widget) {}
};

struct WIDGET_COLOR {
	BOOL use_default_foreground_color; ///< If TRUE, you should not care about foreground_color
	BOOL use_default_background_color; ///< If TRUE, you should not care about background_color
	UINT32 foreground_color; ///< foreground (i.e. text/checkmark) color
	UINT32 background_color; ///< background color
};

/**
 justification enum
 */
enum JUSTIFY {
	JUSTIFY_LEFT, ///< text is left-aligned
	JUSTIFY_CENTER, ///< text is centered
	JUSTIFY_RIGHT ///< text is right-aligned
};

enum WIDGET_V_ALIGN {
	WIDGET_V_ALIGN_TOP = 0,
	WIDGET_V_ALIGN_MIDDLE
};

/**
 the widget's font info
 */
struct WIDGET_FONT_INFO {
	const OpFontInfo* font_info; ///< the OpFontInfo of the widget's font
	INT16 size; ///< the size of the font
	BOOL italic; ///< true if the text is italic
	short weight; ///< the weight of the font
	int char_spacing_extra; ///< any extra character space
	JUSTIFY justify; ///< the justification
};

/**
 IDs for margins, for GetItemMargin
 */
enum WIDGET_MARGIN {
	MARGIN_TOP, ///< top margin
	MARGIN_LEFT, ///< left margin
	MARGIN_RIGHT, ///< right margin
	MARGIN_BOTTOM ///< bottom margin
};


enum WIDGET_TEXT_DECORATION {
	WIDGET_LINE_THROUGH = 0x01 /// < Draw a line on top of text
};

/**
 scrollbar mode for widgets
 */
enum WIDGET_SCROLLBAR_STATUS {
	SCROLLBAR_STATUS_AUTO, ///< show vertical scrollbar always, and horizontal only when needed
	SCROLLBAR_STATUS_ON, ///< show both scrollbars always
	SCROLLBAR_STATUS_OFF ///< show no scrollbars
};

/**
Describes how a OpWidget can be resized by the user.
*/
enum WIDGET_RESIZABILITY {
	WIDGET_RESIZABILITY_NONE = 0x0, ///< Not resizable. Hide resize corner.
	WIDGET_RESIZABILITY_HORIZONTAL = 0x1, ///< Resizable horizontally only.
	WIDGET_RESIZABILITY_VERTICAL = 0x2, ///< Resizable vertically only.
	WIDGET_RESIZABILITY_BOTH = WIDGET_RESIZABILITY_HORIZONTAL | WIDGET_RESIZABILITY_VERTICAL
};

/**
 Describes hove a widget should move or resize when the parent widget resize.
*/
enum RESIZE_EFFECT
{
	RESIZE_FIXED,	///< Isn't affected at all
	RESIZE_MOVE,	///< Move as much as the parent resize so that if follows the right or bottom of the parent.
	RESIZE_SIZE,	///< Don't move, but resize as much as the parent.
	RESIZE_CENTER	///< Move so it is centered in the parent.
};

/**
 search mode when searching for text in widgets
 */
enum SEARCH_TYPE {
	SEARCH_FROM_CARET, ///< search starts from the current caret pos
	SEARCH_FROM_BEGINNING, ///< search starts from the beginning of the contents
	SEARCH_FROM_END ///< search starts from the end of the contents
};

/**
 the different parts of the scrollbar
 */
enum SCROLLBAR_PART_CODE {
	INVALID_PART			= 0x00,
	LEFT_OUTSIDE_ARROW 		= 0x01, ///< the left arrow on a horizontal scrollbar (with one arrow placed on either side)
	LEFT_INSIDE_ARROW		= 0x02, ///< the right arrow on a horizontal scrollbar with both buttons placed to the left
	LEFT_TRACK				= 0x04, ///< the part between the handle and left arrow button of a horizontal scrollbar
	KNOB					= 0x08, ///< the handle in the middle of the scrollbar, that indicates the current scroll position
	RIGHT_TRACK				= 0x10, ///< the part between the handle and right arrow button of a horizontal scrollbar
	RIGHT_INSIDE_ARROW		= 0x20, ///< the left arrow on a horizontal scrollbar with both button placed to the right
	RIGHT_OUTSIDE_ARROW		= 0x40, ///< the right arrow on a horizontal scrollbar (with one arrow placed on either side)
	TOP_OUTSIDE_ARROW 		= LEFT_OUTSIDE_ARROW,
	TOP_INSIDE_ARROW		= LEFT_INSIDE_ARROW,
	TOP_TRACK				= LEFT_TRACK,
	BOTTOM_TRACK			= RIGHT_TRACK,
	BOTTOM_INSIDE_ARROW		= RIGHT_INSIDE_ARROW,
	BOTTOM_OUTSIDE_ARROW	= RIGHT_OUTSIDE_ARROW
};

/**
 the type of scrollbar
 */
enum SCROLLBAR_ARROWS_TYPE {
	ARROWS_NORMAL				= 0x00, ///< normal scrollbar, one arrow button on each end
	ARROWS_AT_TOP				= 0x01, ///< both arrow buttons are at top
	ARROWS_AT_BOTTOM			= 0x02, ///< both arrow buttons are at bottom
	ARROWS_AT_BOTTOM_AND_TOP 	= 0x04 ///< both arrow buttons are at both bottom and top
};

/**
 ellipsis is drawing three dots whenever there's not enough room for all text in a widget
 */
enum ELLIPSIS_POSITION {
	ELLIPSIS_NONE = 0,		///< If there is no room for all text, it will be clipped.
	ELLIPSIS_CENTER = 1,	///< If there is no room for all text, the beginning and end will be drawn. The center will have ellipsis ("...")
	ELLIPSIS_END			///< If there is no room for all text, the beginning will be drawn and end with ellipsis ("...")
};

/**
 Unknown QUICK feature that is not used in core.
 */
enum DROP_POSITION {
	DROP_ALL    = 0x00,
	DROP_CENTER = 0x01,
	DROP_LEFT   = 0x02,
	DROP_RIGHT  = 0x04,
	DROP_TOP    = 0x10,
	DROP_BOTTOM = 0x20
};

/**
 Unknown QUICK feature.
 */
enum UpdateNeededWhen {
	UPDATE_NEEDED_NEVER = 0,
	UPDATE_NEEDED_WHEN_VISIBLE,
	UPDATE_NEEDED_ALWAYS
};

/**
 Info about sizes and some behaviours for all default widgets used by piforms.
 */

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
enum AccessibilityPrune {
	ACCESSIBILITY_DONT_PRUNE = 0,
	ACCESSIBILITY_PRUNE,
	ACCESSIBILITY_PRUNE_WHEN_INVISIBLE
};
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

class OpWidgetInfo
{
public:
	virtual ~OpWidgetInfo() {} ///< destructor

	/** Se OpWidget.h for parameter details. */
	virtual void GetPreferedSize(OpWidget* widget, OpTypedObject::Type type, INT32* w, INT32* h, INT32 cols, INT32 rows);
	/** Se OpWidget.h for parameter details. */
	virtual void GetMinimumSize(OpWidget* widget, OpTypedObject::Type type, INT32* minw, INT32* minh);

	/**
	 if skinning is enabled, asks skin. if not, asks OpUiInfo.
	 @param color which color is wanted
	 @return the color
	 */
	virtual UINT32 GetSystemColor(OP_SYSTEM_COLOR color);

	virtual UINT32 GetVerticalScrollbarWidth();
	virtual UINT32 GetHorizontalScrollbarHeight();

	/** Shrinks the rect with the border size */
	virtual void AddBorder(OpWidget* widget, OpRect *rect);
	virtual void GetBorders(OpWidget* widget, INT32& left, INT32& top, INT32& right, INT32& bottom);

	virtual INT32 GetItemMargin(WIDGET_MARGIN margin);
	virtual void GetItemMargin(INT32 &left, INT32 &top, INT32 &right, INT32 &bottom);

	virtual INT32 GetDropdownButtonWidth(OpWidget* widget);
	virtual INT32 GetDropdownLeftButtonWidth(OpWidget* widget);

	/** Return the height of the upper button (for a vertical scrollbar), or the width of the left button. */
	virtual INT32 GetScrollbarFirstButtonSize();
	virtual INT32 GetScrollbarSecondButtonSize();

	/**
	* Return the preferred style for scrollbar arrows, default is ARROWS_NORMAL which means a single
	* arrow in each end of the scrollbar.
	*/
	virtual SCROLLBAR_ARROWS_TYPE GetScrollbarArrowStyle();

	/**
	  Return which direction a scrollbar button points to. Could be used to make scrollbars with f.eks. no button in
	  the top and 2 at the bottom.
	  pos is the mousepos relative to the buttons x position (for a horizontal scrollbar) or y position (for a vertical).
	  Return values should be: 0 for UP (or LEFT),
							   1 for DOWN (or RIGHT)
	*/
	virtual BOOL GetScrollbarButtonDirection(BOOL first_button, BOOL horizontal, INT32 pos);

	/**
	* Decide which part of a scrollbar is at the given point.
	* Override this method and return TRUE to make OpScrollbar use it instead of the built in click-detection.
	* The hit part should be passed back in the parameter hitPart.
	*
	* Returns FALSE by default.
	*/
	virtual BOOL GetScrollbarPartHitBy(OpWidget *widget, const OpPoint &pt, SCROLLBAR_PART_CODE &hitPart);

#ifdef PAGED_MEDIA_SUPPORT
	/**
	 * Get the height of a page control widget.
	 */
	virtual INT32 GetPageControlHeight();
#endif // PAGED_MEDIA_SUPPORT

	/** Returnvalue should be in milliseconds. */
	UINT32 GetCaretBlinkDelay() { return 500; }

	UINT32 GetScrollDelay(BOOL bigstep, BOOL first_step);

	/** Return how far away the user can drag outside the scrollbar without dropping it. */
	INT32 GetDropScrollbarLength() { return 100; }

	/** Return TRUE if a button should be released when moving outside it. */
	BOOL CanDropButton() { return TRUE; }
};

/// Handles fontswitching/painting/justifying and storage of a string.

class OpWidgetString
{
public:
	OpWidgetString();
	~OpWidgetString();

	INT32 GetMaxLength() const { return max_length; }
	void SetMaxLength(INT32 length) { max_length = length; }

	void SetPasswordMode(BOOL password_mode) { m_packed.password_mode = password_mode; }
	BOOL GetPasswordMode() { return m_packed.password_mode; }

	void SetConvertAmpersand(BOOL convert_ampersand) {m_packed.convert_ampersand = convert_ampersand;}
	BOOL GetConvertAmpersand() {return m_packed.convert_ampersand;}

	void SetDrawAmpersandUnderline(BOOL draw_ampersand_underline) {m_packed.draw_ampersand_underline = draw_ampersand_underline;}
	BOOL GetDrawAmpersandUnderline() {return m_packed.draw_ampersand_underline;}

	/**
	 * Set one or more text decoration properties on the string. Use
	 * 0 to clear the property.
	 *
	 * @param text_decoration One or more of WIDGET_TEXT_DECORATION types
	 * @param replace If TRUE, the current value is fully replaced, if FALSE
	 *        the new value is added to the current value
	 */
	void SetTextDecoration(INT32 text_decoration, BOOL replace)
		{ m_text_decoration = replace ? text_decoration : m_text_decoration | text_decoration; }
	INT32 GetTextDecoration() const {return m_text_decoration; }

	/** Set if the widgetstring should have italic style even if the OpWidget doesn't have that style. */
	void SetForceItalic(BOOL force) { m_packed.force_italic = force; }
	BOOL GetForceItalic() { return m_packed.force_italic; }

	/** Set if the widgetstring should have bold style even if the OpWidget doesn't have that style. */
	void SetForceBold(BOOL force) { m_packed.force_bold = force; }
	BOOL GetForceBold() { return m_packed.force_bold; }

	/** Set if the widgetstring should be treated as LTR even if the OpWidget is RTL. */
	void SetForceLTR(BOOL force) { m_packed.force_ltr = force; }
	BOOL GetForceLTR() const { return m_packed.force_ltr; }

#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
	enum DomainHighlight
	{
		None,
		RootDomain,	// abc.google.com -> highlight "google.com"
		WholeDomain	// abc.google.com -> highlight all
	};

	/** Set if the widgetstring should grey out the parts that does not belong to a server name if the string is a web address.
		So a address like http://my.opera.com/community/ would have http:// and /community greyed out.

		@param type highlight type
		*/
	void SetHighlightType(DomainHighlight type);
	INT32 GetHighlightType() {return m_highlight_type;}
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING

	void ToggleOverstrike() {m_packed.is_overstrike = !m_packed.is_overstrike;}
	BOOL GetOverstrike() {return m_packed.is_overstrike;}

	void Reset(OpWidget* widget);
	OP_STATUS Set(const uni_char* str, OpWidget* widget);
	OP_STATUS Set(const uni_char* str, int len, OpWidget* widget);
	const uni_char* Get() const;

	void SelectNone();
	void SelectFromCaret(INT32 caret_pos, INT32 delta);
	void Select(INT32 start, INT32 stop);

#ifdef WIDGETS_IME_SUPPORT
	// Input Methods. (For showing the underline in composemode and marking the right candidates in candidatemode).
	void SetIMNone();
	void SetIMPos(INT32 pos, INT32 length);
	void SetIMCandidatePos(INT32 pos, INT32 length);
	void SetIMString(OpInputMethodString* string);
	INT16 GetIMLen() { return m_ime_info ? m_ime_info->stop - m_ime_info->start : 0; }
#ifdef SUPPORT_IME_PASSWORD_INPUT
    void ShowPasswordChar(int caret_pos);
    void HidePasswordChar();
#endif // SUPPORT_IME_PASSWORD_INPUT
#endif // WIDGETS_IME_SUPPORT

	/** Returns the character position (offset) at the point, if the string where drawn in rect.
	 * @param rect The rect the string would be drawn inside.
	 * @param point The point.
	 * @param snap_forward Will return TRUE if the offset has 2 visual positions (As with BIDI) and the second is the one that was found.
	 */
	INT32 GetCaretPos(OpRect rect, OpPoint point, BOOL* snap_forward = NULL);
	/** Returns the x position for the caret_pos (offset), if the string where drawn in rect.
	 * @param rect The rect the string would be drawn inside.
	 * @param caret_pos The logical offset you want to get the x position of.
	 * @param snap_forward If TRUE and there is 2 visual positions for the offset (As with BIDI), the second is the one that is returned.
	 */
	INT32 GetCaretX(OpRect rect, int caret_pos, BOOL snap_forward);

	/** Justify and apply text-indent on the string as if inside rect, and return the new x. */
	int GetJustifyAndIndent(const OpRect &rect);

	INT32 GetWidth();
	INT32 GetHeight();

	/** Make the widget update any cached into next time it's needed (such as width, height, fragments) */
	void NeedUpdate() {m_packed.need_update = TRUE;}

	/** Update cached values and fragments if NeedUpdate has been called (otherwise it does nothing).  */
	OP_STATUS Update(VisualDevice* vis_dev = 0);

#ifdef WIDGETS_ELLIPSIS_SUPPORT
	BOOL DrawFragmentSpecial(VisualDevice* vis_dev, int &x_pos, int y, OP_TEXT_FRAGMENT* frag, ELLIPSIS_POSITION ellipsis_position, int &used_space, const OpRect& rect);
#endif

	/**
	 * You can use OpWidgetStringDrawer class instead of this function to get more control on the drawing options.
	 * @see OpWidgetStringDrawer
	 */
	// original prototype :
	//void Draw(OpRect rect, VisualDevice* vis_dev, UINT32 color,
	//	INT32 caret_pos = -1, ELLIPSIS_POSITION ellipsis_position = ELLIPSIS_NONE,
	//	BOOL underline = FALSE, INT32 x_scroll = 0, BOOL caret_snap_forward = FALSE, BOOL only_text = FALSE);
	void Draw(const OpRect& rect, VisualDevice* vis_dev, UINT32 color);

	static OP_SYSTEM_COLOR GetSelectionTextColor(BOOL is_search_hit, BOOL is_focused);
	static OP_SYSTEM_COLOR GetSelectionBackgroundColor(BOOL is_search_hit, BOOL is_focused);

	void SetScript(WritingSystem::Script script);

	/** Gets a list of rectangles a text selection consist of (if any) and a rectangle being a union of all rectangles in the list.
	 *
	 * @param vis_dev - the edit's visual device.
	 * @param list - a pointer to the list to be filled in.
	 * @param union_rect - a union of all rectangles in the list.
	 * @param scroll - edit's scroll position in pixels.
	 * @param text_rect - edit's text rectangle.
	 * @param only_visible - If true only the visible selection's part will be taken into account.
	 */
	OP_STATUS GetSelectionRects(VisualDevice* vis_dev, OpVector<OpRect>* list, OpRect& union_rect, int scroll, const OpRect& text_rect, BOOL only_visible);

	/** Checks if a given point in contained by any of the selection rectangle.
	 *
	 * @param vis_dev - the edit's visual device.
	 * @param point - a point to be checked.
	 * @param scroll - edit's scroll position in pixels.
	 * @param text_rect - edit's text rectangle.
	 */
	BOOL IsWithinSelection(VisualDevice* vis_dev, const OpPoint& point , int scroll, const OpRect& text_rect);

private:
#ifdef _NO_GLOBALS_
	friend struct Globals;
#endif
	friend class OpEdit;
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	friend class OpEditSpellchecker;
#endif // INTERNAL_SPELLCHECK_SUPPORT
	friend class OpWidgetStringDrawer;

	OpWidget* widget;
	const uni_char* str;
	uni_char* str_original; ///< If we have text_translation on the widget we must keep the unmodified string.

	OpTextFragmentList fragments;

	OP_STATUS UpdateFragments();
	void UpdateVisualDevice(VisualDevice *vis_dev = NULL);
	BOOL ShouldDrawFragmentsReversed(const OpRect& rect, ELLIPSIS_POSITION ellipsis_position) const;
	int GetFontNumber();
	OpWidget* GetWidget() const { return widget; }
	const uni_char* GetStr() const { return str; }
	OpTextFragmentList* GetFragments() { return &fragments; }
	BOOL UseAccurateFontSize();

#if defined(WIDGETS_IME_SUPPORT) && defined(SUPPORT_IME_PASSWORD_INPUT)
	OP_STATUS UpdatePasswordText();
    int uncoveredChar;
#endif

	union
	{
		struct
		{
			unsigned char is_overstrike:1;
			unsigned char password_mode:1;
			unsigned char convert_ampersand:1;
			unsigned char draw_ampersand_underline:1;
			unsigned char need_update:1;
			unsigned char force_italic:1;
			unsigned char force_bold:1;
			unsigned char force_ltr:1;
#ifdef WIDGETS_HEURISTIC_LANG_DETECTION
			unsigned char need_update_script:1;
#endif // WIDGETS_HEURISTIC_LANG_DETECTION
			unsigned char draw_centerline:1;
		} m_packed;
		unsigned short m_packed_init;
	};

	INT32 width;
	INT16 height;
	INT32 max_length;
	WritingSystem::Script m_script;
#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
	DomainHighlight m_highlight_type;
	OP_STATUS UpdateHighlightOffset();
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
#ifdef WIDGETS_HEURISTIC_LANG_DETECTION
	WritingSystem::Script m_heuristic_script;
#endif // WIDGETS_HEURISTIC_LANG_DETECTION
	INT32 m_text_decoration;

public:
#ifdef WIDGETS_IME_SUPPORT
	IME_INFO* m_ime_info;
#endif //WIDGETS_IME_SUPPORT
	INT32 sel_start, sel_stop;
#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
	INT16 m_highlight_ofs;
	INT16 m_highlight_len;
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
};

#ifdef QUICK
# include "adjunct/quick_toolkit/widgets/QuickOpWidgetBase.h"
#endif // QUICK

/** Base class for all widgets.
*/

class OpWidget : public Link, public OpInputContext, public OpTimerListener, public MessageObject
#if defined (QUICK)
				, public QuickOpWidgetBase
#endif
#ifdef SKIN_SUPPORT
				, public OpWidgetImageListener
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				, public OpAccessibleItem
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
#ifdef WIDGETS_IMS_SUPPORT
			   , public OpIMSListener
#endif // WIDGETS_IMS_SUPPORT
#ifdef USE_OP_CLIPBOARD
, public ClipboardListener
#endif // USE_OP_CLIPBOARD
{
	friend class ResourceManager;
	friend class OpWidgetString;
	friend class OpDropDown;
	friend class QuickOpWidgetBase;
public:

	/**
	 constructor. initializes anything to nothing.
	 */
	OpWidget();

protected:

	/**
	 destructor is protected because it is illegal to destruct an item that has a parent..
	 use Delete() instead so it can remove itself first
	 */
	virtual ~OpWidget();

public:

	/** This is here for compability-reasons. You should NOT write new code using OpWidget::over_widget or OpWidget::hover_widget.
		Use g_widget_globals->hover_widget instead!! */
	static OpWidget*& over_widget();
	/** This is here for compability-reasons. You should NOT write new code using OpWidget::over_widget or OpWidget::hover_widget.
		Use g_widget_globals->hover_widget instead!! */
	#define over_widget over_widget()

#ifndef MOUSELESS
	/**
	 calls OnCaptureLost on any captured widget and zero:s the pointer
	 */
	static void ZeroCapturedWidget();
	/**
	 calls OnMouseLeave on any currently hovered widget and zero:s the pointer
	 */
	static void ZeroHoveredWidget();
	/**
	 gets hooked_widget, which is the same as captured_widget. whenever a widget receives mouse down,
	 this widget is set as the hooked widget. when the widget receives its last mouse up or a mouse
	 leave, hooked_widget is zero:ed.
	 @return a reference to the hooked widget pointer
	 */
	static OpWidget*& hooked_widget();
	/**
	 shorthand for OpWidget::hooked_widget()
	 */
	#define hooked_widget hooked_widget()
#endif

	/**
	 initializes all global stuff in the widget class. might LEAVE.
	 */
	static void InitializeL();
	/**
	 cleans up all global stuff in the widget class
	 */
	static void Free();

	/**
	 prepares the widget for deletion. removes the widget, calls OnDeleted on the widget and inserts it in the deleted_widgets list.
	 */
	void Delete();

	// OpInputContext
	// currently only handles pan actions, though scrolling via mousewheel probably ought to go through here as well
	/**
	 called on input action. currently only handles pan actions.
	 */
	virtual BOOL			OnInputAction(OpInputAction* /*action*/);

	/**
	   @param return the writing system of the widget
	 */
	WritingSystem::Script GetScript() { return m_script; }
	void GetBorders(short& left, short& top, short& right, short& bottom) const;

	void SetMargins(short margin_left, short margin_top, short margin_right, short margin_bottom);
	void GetMargins(short& left, short& top, short& right, short& bottom) const;

#ifdef SKIN_SUPPORT
	/**
	   documented in OpWidgetPainterManager
	 */
	BOOL UseMargins(short margin_left, short margin_top, short margin_right, short margin_bottom,
					UINT8& left, UINT8& top, UINT8& right, UINT8& bottom);
	/**
	   should return TRUE for widgets that want to draw their focus
	   rect in the margins - will cause margins to be included when
	   invalidating and clipping
	 */
	virtual BOOL FocusRectInMargins() { return FALSE; }

	/**
	 sets the skin manager for all skins, and all children's skins
	 @param skin_manager the new skin manager
	 */
	virtual void			SetSkinManager(OpSkinManager* skin_manager);
	/**
	 @param return the current skin manager
	 */
	OpSkinManager*			GetSkinManager() {return m_skin_manager;}

	/**
	 sets whether the widget should be drawn using skin or not. if packed.is_skinned is true and the widget doesn't need
	 to be drawn with the CssWidgetPainter, the border is drawn using its skin.
	 @param skinned tells the widget to draw its borders using its skin if possible
	 */
	void					SetSkinned(BOOL skinned)	{ packed.is_skinned = skinned != FALSE; }
	/**
	 @return TRUE if the widget will draw its background and borders using its skin if possible
	 */
	BOOL					GetSkinned()				{ return packed.is_skinned; }

	/**
	 sets whether the skin in this widget should be considered a background for child widgets only. If set, the skin
	 will only be drawn if/when skinned children are drawn over it.
	 */
	void					SetSkinIsBackground(BOOL is_background) { packed.skin_is_background = is_background != FALSE; }

	/** Is the widget in minisize. A UI widget in minisize should take up less space than normally. */
	BOOL IsMiniSize() const { return packed.is_mini; }
	virtual void			SetMiniSize(BOOL is_mini);

	/** Is the widget floating. A floating widget is a widget that should not be affected by normal UI-layout. */
	BOOL IsFloating() const { return packed.is_floating; }
	virtual void			SetFloating(BOOL is_floating) { packed.is_floating = is_floating; }

#ifdef WIDGET_IS_HIDDEN_ATTRIB
	BOOL IsHidden() const { return packed.is_hidden; }
	virtual void			SetHidden(BOOL is_hidden) { packed.is_hidden = is_hidden; }
#endif // WIDGET_IS_HIDDEN_ATTRIB

	/** Draw the background and border skins for this widget. contents need still be drawn via OnPaint. */
	void					EnsureSkin(const OpRect& rect, BOOL clip = TRUE, BOOL force = FALSE);

	/** Draw the overlay skins for this widget. Overlay is drawn after OnPaint. */
	void					EnsureSkinOverlay(const OpRect& rect);

	/**
	 @return the border skin of the widget
	 */
	OpWidgetImage*			GetBorderSkin()		{return &m_border_skin;}
	/**
	 @return the foreground skin of the widget
	 */
	OpWidgetImage*			GetForegroundSkin() {return &m_foreground_skin;}
#endif
#ifdef QUICK
	/** Restrict the foreground skin to 16x16 or the size specified in skin element "Window Document Icon" */
	virtual void			SetRestrictImageSize(BOOL b) { m_foreground_skin.SetRestrictImageSize(b); }
	/** @return TRUE if the foreground skin is restricted in size */
	BOOL					GetRestrictImageSize() { return m_foreground_skin.GetRestrictImageSize(); }
#endif

	virtual void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**	Adds a childwidget (f.eks scrollbars in listbox).
		Childwidgets will automatically be deleted when this widget is deleted.
	*/
	virtual void			AddChild(OpWidget* child, BOOL is_internal_child = FALSE, BOOL first = FALSE);

	/** Gives ability to add child before or after the given reference child widget
	* @param child widget which is to be inserted.
	* @param ref_widget is a reference widget and based on this one child widget is
	* inserted before or after it.
	* If ref_widget is NULL then this widget will be inserted at the end of the list
	* @param after_ref_widget flag if TRUE then widget will be inserted after given
	* 'ref_widget' otherwise before.
	* @param is_internal_child flag if TRUE then widget will be inserted as a internal
	* child.
	* @param first flag if TRUE, widget will be added first in the list
	* @note Out of 'first' and 'ref_widget' arguments 'first' will be given priority.
	*/
	virtual void			InsertChild(OpWidget* child,
										OpWidget* ref_widget = NULL,
										BOOL after_ref_widget = TRUE,
										BOOL is_internal_child = FALSE,
										BOOL first = FALSE);

	/**
	 removes the widget. generates OpRemovoing and OnRemoved on the widget, and removes all children.
	 */
	void Remove();

	enum Z {
		Z_TOP,		///< Visually on top over other widgets
		Z_BOTTOM	///< Visually at bottom behind other widgets.
	};
	/** Set the Z order of this widget relatively to its siblings */
	void SetZ(Z z);

	/**
	 @return the parent widget
	 */
	OpWidget* GetParent() const { return parent; }
	/**
	 @return a pointer to the widget's OpWidgetInfo instance
	 */
	OpWidgetInfo* GetInfo();

	/**
	 @return TRUE if the widget is enabled
	 */
	BOOL IsEnabled() const { return packed.is_enabled; }
	/**
	 @return TRUE if the widget is visible
	 */
	BOOL IsVisible() const { return packed.is_visible; }
	/**
	 @return TRUE if the widget and all its ancestors are visible
	 */
	BOOL IsAllVisible() const;
	/**
	 @return TRUE if the widget is an internal child (i.e. the button or edit field of a file upload widget)
	 */
	BOOL IsInternalChild() const { return packed.is_internal_child; }
	/**
	 @return if TRUE, the widget will not get focus when cycling focus using TAB
	 */
	BOOL IsTabStop() const { return packed.is_tabstop; }

	/** Set if the image should not care about mouse events. */
	BOOL IgnoresMouse() const { return packed.ignore_mouse; }
	/** @param ignore Set if the image should not care about mouse events. */
	void SetIgnoresMouse(BOOL ignore) {packed.ignore_mouse = ignore;}

	/**
	 * @param forward - TRUE if we should look forward. FALSE if we should look backwards.
	 * @return TRUE if the current focused subwidget is the last focusable.
	 */
	BOOL IsAtLastInternalTabStop(BOOL forward) const;
	/**
	 * @param forward - TRUE if we should look forward. FALSE if we should look backwards.
	 * @return NULL if none was found.
	 */
	OpWidget* GetNextInternalTabStop(BOOL forward) const;

	/**
	 * Used to find the first/last internal widget to focus when moving
	 * through widgets.
	 *
	 * @param[in] front If TRUE, then return the first
	 * internal widget, if FALSE, the last internal widget.
	 *
	 * @returns The first/last internal widget that can be focused or NULL if none found, not even this.
	 */
	OpWidget* GetFocusableInternalEdge(BOOL front) const;

	BOOL CanHaveFocusInPopup() const { return packed.can_have_focus_in_popup; }
	void SetCanHaveFocusInPopup(BOOL new_val);

	/** When true, No border should be drawn. It is drawn by the layoutengine. */
	BOOL HasCssBorder() const { return packed.has_css_border; }
	/** When true, No background should be drawn. It is drawn by the layoutengine. */
	BOOL HasCssBackgroundImage() const { return packed.has_css_backgroundimage; }
	/** When true, draw focus rect if it has focus. */
	BOOL HasFocusRect() const { return packed.has_focus_rect; }
	/** When true 'box-sizing' is 'border-box' for this widget. */
	BOOL IsBorderBox() const { return packed.border_box; }

	/**
	 * Gets the size of this OpWidget, in border-box.
	 *
	 * The border-box of an element is the actual footprint the element leaves
	 * on the page, i.e. the content-box plus padding plus borders.
	 *
	 * Note that this returns the border-box dimensions, regardless of the
	 * return value of IsBorderBox().
	 *
	 * @param[out] width The border-box width.
	 * @param[out] height The border-box height.
	 */
	void GetBorderBox(INT32& width, INT32& height);

	/**
	 sets the widget's form object
	 @param form the new form object
	 */
	void SetFormObject(FormObject* form) {form_object = form;}
	/**
	 @return TRUE if the widget or one of its ancestors has a form object
	 */
	BOOL IsForm();

	/**
	 sets the padding of the widget
	 @param padding_left the widget's left paddding
	 @param padding_top the widget's top paddding
	 @param padding_right the widget's right paddding
	 @param padding_bottom the widget's bottom paddding
	 */
	void SetPadding(short padding_left, short padding_top, short padding_right, short padding_bottom);

	/** Add the padding values to the rect */
	void AddPadding(OpRect& rect);
	// Get the padding values (inside spacing) values of the widget
	virtual void GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom);
	/**
	 @return the widget's left padding
	 */
	short GetPaddingLeft() {return m_padding_left;}
	/**
	 @return the widget's top padding
	 */
	short GetPaddingTop() {return m_padding_top;}
	/**
	 @return the widget's right padding
	 */
	short GetPaddingRight() {return m_padding_right;}
	/**
	 @return the widget's bottom padding
	 */
	short GetPaddingBottom() {return m_padding_bottom;}

	/** If several properties are going to be set, this can be used to minimize updating of the widgets internals, but it is not required.
	    (F.ex: not reformatting a OpMultiEdit for each change, like SetFontInfo, SetRect etc..) */
	virtual void BeginChangeProperties() {}
	/** If several properties are going to be set, this can be used to minimize updating of the widgets internals, but it is not required.
	    (F.ex: not reformatting a OpMultiEdit for each change, like SetFontInfo, SetRect etc..) */
	virtual void EndChangeProperties() {}

	/**
	 sets the widget's listener. will call SetListener with same arguments for all non-internal children.
	 @param listener the listener
	 @param force if TRUE, listener replaces the widget's previous listener if present
	 */
	virtual void SetListener(OpWidgetListener* listener, BOOL force = TRUE);
	/**
	 @return the widget's listener
	 */
	OpWidgetListener* GetListener() {return listener;}

	/**
	 sets the widget's pointer to its visual device to vis_dev. the same is done for all children.
	 @param vis_dev
	 */
	void SetVisualDevice(VisualDevice* vis_dev);

	/**
	 tells the widget whether it has CSS borders or not. a widget with CSS borders should always have its borders
	 drawn by core. this also has some implications on the size of the widgets - widgets without CSS border get a 2px
	 sunken border on all sides.
	 */
	void SetHasCssBorder(BOOL has_css_border);
	/**
	 tells the widget whether it has CSS background or not. a widget with CSS background should always have its
	 background drawn by core.
	 */
	void SetHasCssBackground(BOOL has_css_background);
	/**
	 determines if a focus rect should be drawn around the widget
	 @param has_focus_rect TRUE if a focus rect should be drawn around the rectangle
	 */
	void SetHasFocusRect(BOOL has_focus_rect);

	/** Starts a timer which will call OnTimer.
		If the timer is already running, it will be stopped before it starts again. */
	void StartTimer(UINT32 millisec);
	/** Stops the timer if it is running. */
	void StopTimer();
	/**
	 returns the timer delay. if non-zero, the timer is running and will be restarted with the same delay again
	 when it times out.
	 */
	UINT32 GetTimerDelay() const;
	/** @return TRUE if the timer is running */
	BOOL IsTimerRunning() const;

	/**
	 changes the dimensions and position of the widget to the ones denoted by rect.
	 @param rect the new dimensions of the widget
	 @param invalidate if TRUE, the entire widget (old rect and new rect) is invalidated
	 @param resize_children if TRUE, children will be resized as well (applies only for QUICK)
	 */
	void SetRect(const OpRect &rect, BOOL invalidate = TRUE, BOOL resize_children = TRUE);
	/**
	 changes the dimentions of the widget to the ones denoted by w, h.
	 will generate OnResized to any listener if the widget was resized.
	 will generate OnMove to any listener if the widget was moved.
	 @param w the new width of the widget
	 @param h the new height of the widget
	 \note calls SetRect with default values for invalidate and resize_children
	 */
	void SetSize(INT32 w, INT32 h);
	/**
	 moves the widget to the position denoted by pos. will generate OnMove to any listener if the widget was moved.
	 @param pos the new position of the widget
	 */
	void SetPosInDocument(const AffinePos& pos);

	/**
	 @return the widget's container
	 */
	WidgetContainer* GetWidgetContainer() {return m_widget_container;}
	/**
	 sets the widget's container
	 @param container the container
	 */
	void SetWidgetContainer(WidgetContainer* container);

	/** Handle pending paint operations */
	void Sync();

	/**
	 Reports a OOM situation to the core.
	 In functions which returns a OP_STATUS this is not neccesary.
	 Any OOM in the constructor should set the init_status member.
	 */
	void ReportOOM();

	/** Get mouse position relative to this widget.
		You should always use the point parameter sent during mouse events instead.
		This should be used only if there is no other solution. */
	virtual OpPoint GetMousePos();

	/** Returns the position in the document, in document coordinates relative to the top of the document. */
	AffinePos GetPosInDocument();

	/** Returns the position of the widget, in document coordinates relative to the origo of the root document. */
	AffinePos GetPosInRootDocument();

	/** Returns the rect */
	OpRect GetRect(BOOL relative_to_root = FALSE);

	/** Returns the rect in document coordinates (if applicable) */
	OpRect GetDocumentRect(BOOL relative_to_root = FALSE);

	/** Returns the rect translated to 0, 0. if include_margin is TRUE any margins will be added to the rect. */
	OpRect GetBounds(BOOL include_margin = FALSE);

	/** Add the nonclickable margins to rect (areas that should not be hit by the mouse). */
	OpRect AddNonclickableMargins(const OpRect& rect);

	/** Returns the clickable rect relative to the parent widget (Same as GetRect but excluding areas that should not be hit by the mouse). */
	OpRect GetClickableRect() { return AddNonclickableMargins(GetRect()); }

	/** Returns the clickable rect relative to this widget. */
	OpRect GetClickableBounds() { return AddNonclickableMargins(GetBounds()); }

	/** Returns the rect relative to screen */
	OpRect GetScreenRect();

	/**
	 @return the color of the widget
	 */
	WIDGET_COLOR GetColor() const { return m_color; }
	/**
	 @param color the new color of the widget
	 */
	void SetColor(WIDGET_COLOR color) { m_color = color; }
	/**
	 @return the widget's visual device
	 */
	VisualDevice* GetVisualDevice() const { return vis_dev; }
	/**
	 @return TRUE if the widget or one of its ancestors is part of a form
	 */
	BOOL IsInWebForm() const;
	/**
	 returns a pointer to the widget's form object. if it hasn't got one and iterate_parents is TRUE,
	 the call will bubble upwards through its ancestors.
	 @param iterate_parents if TRUE, bubble upwards
	 @return the form object
	 */
	FormObject* GetFormObject(BOOL iterate_parents = FALSE) const;
	/**
	 @return the width of the widget
	 */
	INT32 GetWidth() {return rect.width;}
	/**
	 @return the height of the widget
	 */
	INT32 GetHeight() {return rect.height;}

	/** Sets the cliprect relative to this widget. */
	void SetClipRect(const OpRect &rect);

	/** Removes the cliprect */
	void RemoveClipRect();

	/** Get the cliprect relative to this widget. */
	OpRect GetClipRect();

	/** Returns true if the widget is someway visible and if it is, it sets rect
		to the cliprect of the parent layoutobject (f.eks. a div).
		Note, that it doesn't check the visibility of the documentview or window */
	BOOL GetIntersectingCliprect(OpRect &rect);

	/**
	 Get the visible child widget that intersect point, or this if none where found.
	 @param point The point relative to this widget.
	 @param include_internal_children If TRUE, widgets marked as internal-child can also be returned.
	 @param include_ignored_mouse If TRUE, widgets that's marked SetIgnoresMouse(TRUE) can also be returned.
	 */
	OpWidget* GetWidget(OpPoint point, BOOL include_internal_children = FALSE, BOOL include_ignored_mouse = FALSE);

	OpWidget* Suc() { return (OpWidget*) Link::Suc(); }
	OpWidget* Pred() { return (OpWidget*) Link::Pred(); }

	/** @return the first child */
	OpWidget* GetFirstChild() {return (OpWidget*) childs.First();}
	/** @return the last child */
	OpWidget* GetLastChild() {return (OpWidget*) childs.Last();}
	/** @return next sibling */
	OpWidget* GetNextSibling() {return Suc();}
	/** @return previous sibling */
	OpWidget* GetPreviousSibling() {return Pred();}

	/**
	 deletes any current input actions and keeps action. probably used by quick.
	 @param action the new input action
	 */
	virtual void SetAction(OpInputAction* action);
	/**
	 @return the currently stored input action
	 */
	virtual OpInputAction* GetAction() {return m_action;}

	/**
	 tries to obtain the widget's parent window, through widget container if present, otherwize through document manager
	 @return the parent window
	 */
	OpWindow* GetParentOpWindow();

	/** Invalidates rect, relative to the widget. If intersect is TRUE, the rectangle will be clipped to not
		invalidate a bigger rect than this widget.
		If timed is TRUE, the update might be delayed. See VisualDevice::Update for more info about that. */
	void Invalidate(const OpRect &rect, BOOL intersect = TRUE, BOOL force = FALSE, BOOL timed = FALSE);
	/**
	 invalidates the entire contents of the widget
	 */
	void InvalidateAll();

	/**
	 sets the enabled-status of the widget
	 @param enabled if TRUE, the widget is enabled
	 */
	virtual void SetEnabled(BOOL enabled);
	/**
	 sets the read-only status of the widget
	 @param readonly if TRUE, the widget is put in read-only mode
	 */
	virtual void SetReadOnly(BOOL readonly) {}

	/**
	 gets the read-only status of the widget
	 */
	virtual BOOL IsReadOnly() { return FALSE; }

	/**
	 sets the visibility of the widget
	 @param visible if TRUE, the widget is set to visible
	 */
	void SetVisibility(BOOL visible);

	/**
	 sets if this widget should be focused when tabbing (otherwise it will be skipped).
	 */
	void SetTabStop(BOOL is_tabstop) {packed.is_tabstop = is_tabstop;};

	/**
	 sets if this widget should be treated like a FormObject even though it doesn't have one.
	 F.ex the <button> element has no FormObject but should still be treated like one.
	 */
	void SetSpecialForm(BOOL val) { packed.is_special_form = val; }

	/** sets if this widget wants the OnMove callback. If TRUE, it will recurse to set its
		parents to TRUE too. */
	void SetOnMoveWanted(BOOL onmove_wanted);

	/** Set ellipsis. Single line widgets will respect this value, but not multiline widgets (might change). */
	virtual void SetEllipsis(ELLIPSIS_POSITION ellipsis) { packed.ellipsis = (unsigned int)ellipsis; }
	virtual ELLIPSIS_POSITION GetEllipsis() { return (ELLIPSIS_POSITION)packed.ellipsis; }

	// DEPRICATED!!!
	BOOL HasCenterEllipsis() { return GetEllipsis() == ELLIPSIS_CENTER; }

	/** sets if this widget is a dead widget. A dead widget look normal but doesn't invoke on input. */
	void SetDead(BOOL dead) {packed.is_dead = dead;}
	BOOL IsDead() {
#ifdef QUICK
		return packed.is_dead || IsCustomizing();
#else
		return packed.is_dead;
#endif //QUICK
	}

	/**
	 @param relayout_needed if TRUE, relayout is needed
	 */
	void SetRelayoutNeeded(BOOL relayout_needed) {packed.relayout_needed = relayout_needed;};
	/**
	 @return TRUE if relayout is needed
	 */
	BOOL IsRelayoutNeeded() const {return packed.relayout_needed;}

	/**
	 @param layout_needed if TRUE, layout is needed
	 */
	void SetLayoutNeeded(BOOL layout_needed) {packed.layout_needed = layout_needed;};
	/**
	 @return TRUE if layout is needed
	 */
	BOOL IsLayoutNeeded() const {return packed.layout_needed;}

	/** Sets that this widget is invalidated and are waiting for a paint. Note, that the widget might still
		get a paint even though this hasn't been set */
	void SetPaintingPending();

	/** Return TRUE if this widget are waiting for a OnPaint. Note, that it might get OnPaint even if this isn't TRUE,
		and even if it's TRUE, it might never get a OnPaint (f.ex if it is hidden) */
	BOOL IsPaintingPending() const {return packed.is_painting_pending;}

	/** Update this widgets visual states with current properties. Default implementation is empty and most widget doesn't
		do anything since they update themselfs when properties change. */
	virtual void Update() {}

	/**
	 returns the foreground color of the widget
	 @param def_color fallback, in case the widget couldn't get the foreground color from any skin or ancestor
	 @param state really a SkinState bitmask, used to obtain the foreground color from a skin
	 @return the foreground color, or def_color if no foreground color was obtained
	 */
	UINT32 GetForegroundColor(UINT32 def_color, INT32 state);
	/**
	 returns the background color of the widget
	 @param def_color fallback, in case the widget is set to use its default background color
	 @return the background color, or def_color if default background color is to be used
	 */
	UINT32 GetBackgroundColor(UINT32 def_color);
	/**
	 sets the foreground (i.e. text/check mark) color of the widget and its children.
	 color will only be changed on non-form widgets or if styling of forms is enabled in prefs.
	 @param color the color
	 \note format might be dependent on platform, but the macro RGB can be used
	 */
	void SetForegroundColor(UINT32 color);
	/**
	 sets the background color of the widget and its children.
	 color will only be changed on non-form widgets or if styling of forms is enabled in prefs.
	 @param color the color
	 \note format might be dependent on platform, but the macro RGB can be used
	 */
	void SetBackgroundColor(UINT32 color);
	/**
	 tells the widget and all its children to use its default foreground color
	 */
	void UnsetForegroundColor();
	/**
	 tells the widget and all its children to use its default background color
	 */
	void UnsetBackgroundColor();

	/**
	 sets the painter manager for the widget and all its children
	 @param painter_manager the painter manager
	 */
	void SetPainterManager(OpWidgetPainterManager* painter_manager);
	/**
	 @return the widget's painter manager
	 */
	OpWidgetPainterManager* GetPainterManager() { return painter_manager; }

	/**
	 changes the font information for the widget and its children. if anything changes, OnFontChanged is called.
	 will set custom_font to true, which will bail calls to UpdateSystemFont unles force_system_font is TRUE.
	 @param font_info a pointer to a font_info instance
	 @param size the new font size
	 @param italic TRUE if the font should be italic
	 @param weight the new weight of the font
	 @param justify the new justification mode of the widget
	 @param char_spacing_extra the new extra char spacing value
	 @return TRUE if the font changed on this widget or any of the child widgets.
	 */
	BOOL SetFontInfo(const OpFontInfo* font_info, INT32 size, BOOL italic, INT32 weight, JUSTIFY justify, int char_spacing_extra = 0);
	BOOL SetFontInfo(const WIDGET_FONT_INFO& wfi);

	/** Set justify for the textcontent.
		If changed_by_user is TRUE, the justify value will be sticky (It won't change to something else unless changed_by_user is TRUE).
		That should be used if user changes justify manually and we don't want it to be reset by css or something else. */
	BOOL SetJustify(JUSTIFY justify, BOOL changed_by_user);

	/**
	 updates the widget's font to the font for the message-box CSS system UI element. (see
	 documentation for GetUICSSFont in pi/ui.) will bail if SetFontInfo has been called for
	 the widget if force_system_font is not set.
	 @param force_system_font force update to system font even if the widget has a custom font
	 */
	void SetVerticalAlign(WIDGET_V_ALIGN align);
	WIDGET_V_ALIGN GetVerticalAlign() { return (WIDGET_V_ALIGN) packed.v_align; }

	void UpdateSystemFont(BOOL force_system_font = FALSE);

	/**
	 sets the text transform mode for the widget
	 @param value the text-transform value. valid types documented in layout/cssprops.h
	 */
	void SetTextTransform(short value) { text_transform = value; }

	/**
	 Set various CSS properties on the widget.
	 */
	void SetProps(const HTMLayoutProperties& props);

	/** Set the current cursor. Should be called from OnSetCursor to have effect. */
	void SetCursor(CursorType cursor);

	/** Searches for txt.
		@param txt The searchstring
		@param len The length of the searchstring
		@param forward Forward or backward search
		@param match_case Match case or not
		@param words Match whole word only
		@param type Search from beginning, end or current caret position
		@param select_match Select the found match, or do nothing when it's found.
		@param scroll_to_selected_match if TRUE, scrolling to any selected match will be performed
		@return TRUE if text was found.
	*/
	BOOL SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case,
							BOOL words, SEARCH_TYPE type, BOOL select_match,
							BOOL scroll_to_selected_match = TRUE)
							{ return SearchText(txt, len, forward, match_case, words, type, select_match, scroll_to_selected_match, FALSE, TRUE); }
	virtual BOOL SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case,
							BOOL words, SEARCH_TYPE type, BOOL select_match,
							BOOL scroll_to_selected_match, BOOL wrap, BOOL next) { return FALSE; }

	/** If include_document is TRUE the document is eventual scrolled so that the caret will be visible */
	virtual void ScrollIfNeeded(BOOL include_document = FALSE, BOOL smooth_scroll = FALSE) {}

	/** should return true if the widget contains text, of which some is selected */
	virtual BOOL HasSelectedText() { return FALSE; }

	/** should deselect any selected text in the widget */
	virtual void SelectNothing() {}

	/** should select all text in the widget */
	virtual void SelectAll() {}

	/** Get current selection or 0->0->SELECTION_DIRECTION_DEFAULT if there is no selection */
	virtual void GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction, BOOL line_normalized = FALSE) { start_ofs = stop_ofs = 0; direction = SELECTION_DIRECTION_DEFAULT; }

	/** Get current selection or 0->0 if there is no selection */
	void GetSelection(INT32 &start_ofs, INT32 &stop_ofs, BOOL line_normalized = FALSE) { SELECTION_DIRECTION direction; GetSelection(start_ofs, stop_ofs, direction, line_normalized); }

	/** Gets a list of rectangles a text selection consist of (if any) and a rectangle being a union of all rectangles in the list.
	 *
	 * @param list - a pointer to the list.
	 * @param union_rect - a union of all rectangles in the list.
	 * @param visible_only - If true only the visible selection's part will be taken into account.
	 */
	virtual OP_STATUS GetSelectionRects(OpVector<OpRect>* list, OpRect& union_rect, BOOL visible_only) { return OpStatus::OK; }

	/** Checks if given x and y are over a text selection.
	 *
	 * @param x - the x coordinate to be checked (in widget coordinates).
	 * @param y - the y coordinate to be checked (in widget coordinates).
	 *
	 * @return TRUE if (x, y) point is over the selection. FALSE othewise.
	 */
	virtual BOOL IsWithinSelection(int x, int y) { return FALSE; }

	/**
	 should select any text between start_ofs and stop_ofs
	 @param start_ofs the start offset
	 @param stop_ofs the stop offset
	 @param direction the direction of the selection
	 @param line_normalized flag controlling interpretation of line terminator.
	 */
	virtual void SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction, BOOL line_normalized = FALSE) {}

	/**
	 should select any text between start_ofs and stop_ofs
	 @param start_ofs the start offset
	 @param stop_ofs the stop offset
	 */
	void SetSelection(INT32 start_ofs, INT32 stop_ofs, BOOL line_normalized = FALSE) { SetSelection(start_ofs, stop_ofs, SELECTION_DIRECTION_DEFAULT, line_normalized); }

	/**
	 should select any text between start_ofs and stop_ofs
	 @param start_ofs the start offset
	 @param stop_ofs the stop offset
	 */
	virtual void SelectSearchHit(INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit) { SetSelection(start_ofs, stop_ofs); }
	virtual BOOL IsSearchHitSelected() { return FALSE; }
	virtual BOOL IsActiveSearchHitSelected() { return FALSE; }

	/**
	 should return the offset from the start of any (editable) text to the caret position
	 @return the caret offset
	 */
	virtual INT32 GetCaretOffset() { return 0; }

	/**
	 should move the caret to the position designated by caret_ofs
	 @param caret_ofs the new caret position
	 */
	virtual void SetCaretOffset(INT32 caret_ofs) {}

	/** Return TRUE if it has scrollbar and it is scrollable in any direction. */
	virtual BOOL IsScrollable(BOOL vertical) { return FALSE; }

#ifdef WIDGETS_CLONE_SUPPORT
	/** Clones most properties from source to this widget. */
	OP_STATUS CloneProperties(OpWidget *source, INT32 font_size = 0);

	/** Create new widget cloned from this one. Must specify a parent widget for the new widget.
		Can specify a different font-size, or 0 if the same fontsize should be used. */
	virtual OP_STATUS CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded) { return OpStatus::ERR_NOT_SUPPORTED; }

	/** Returns TRUE of this widget is cloneable */
	virtual BOOL IsCloneable() { return FALSE; }
#endif

	/**
	 sets the text direction of the widget
	 @param is_rtl if TRUE, the widget is set to have RTL text direction
	 */
	void SetRTL(BOOL is_rtl);
	/**
	 returns the text direction of the widget
	 @return TRUE if the text direction of the widget is RTL
	 */
	BOOL GetRTL() const { return packed.is_rtl; }

	/** Returns TRUE if UI elements (like scrollbars) should be placed to the left,
		takes user preferences and Right-To-Left content into account */
	BOOL LeftHandedUI();

	/** Read the EnableStylingOnForms prefs and see if styling is allowed for this widget. */
	BOOL GetAllowStyling();

	/** Change the resizability of the OpWidget.

		Note that not all OpWidget subclasses are resizable at all. It is up
		to each subclass to respond to this setting.

		Calling this function will trigger OnResizabilityChanged().

		@param resizability The new resizability of the OpWidget.
	*/
	void SetResizability(WIDGET_RESIZABILITY resizability);

	/** @return the resizability of this OpWidget. */
	WIDGET_RESIZABILITY GetResizability() const { return static_cast<WIDGET_RESIZABILITY>(packed.resizability); }

	/** @return TRUE if the OpWidget is resizable along at least one axis. */
	BOOL IsResizable() const { return (packed.resizability & WIDGET_RESIZABILITY_BOTH) != 0; }

	/** @return TRUE if the OpWidget is at least resizable along the horizontal axis. */
	BOOL IsHorizontallyResizable() const { return (packed.resizability & WIDGET_RESIZABILITY_HORIZONTAL) != 0; }

	/** @return TRUE if the OpWidget is at least resizable along the vertical axis. */
	BOOL IsVerticallyResizable() const { return (packed.resizability & WIDGET_RESIZABILITY_VERTICAL) != 0; }

	/**
		Is called to see which size the widget wants to have. cols and rows is the (by html/css) specified
		width/height in characters or items (for listbox). They are 0 if they are not specified.
	*/
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows) {}
	/**
		Is called to see which size the widget needs to have.
	*/
	virtual void GetMinimumSize(INT32* minw, INT32* minh) {}
#ifndef QUICK
	virtual void GetRequiredSize(INT32& width, INT32& height) {GetPreferedSize(&width, &height, 0, 0);}
#endif

#ifdef WIDGETS_IME_SUPPORT
	/**
		triggered from FramesDocument::HandleEventFinished, to allow
		widgets that suppressed IME spawning on focus to spawn from
		here instead.
	 */
	void IMEHandleFocusEvent(BOOL focused) { if (!m_suppress_ime) return; m_suppress_ime = FALSE; IMEHandleFocusEventInt(focused, m_suppressed_ime_reason); }
	/**
	   called from IMEHandleFocusEvent if IME spawning was suppressed.

	   @param reason reason why IME was focused

	   @return FALSE if an attempt to spawn an IME failed, TRUE
	   otherwise
	*/
	virtual BOOL IMEHandleFocusEventInt(BOOL focused, FOCUS_REASON reason) { return TRUE; }
#endif // WIDGETS_IME_SUPPORT

#ifdef SKIN_SUPPORT
	// Implementing OpWidgetImageListener interface

	/**
	 implements the OpWidgetImageListener interface. called when an image has chaged. invalidates the widget.
	 if animated skins are allowed, starts any animation of the image.

	 */
	virtual void			OnImageChanged(OpWidgetImage* widget_image);

#ifdef ANIMATED_SKIN_SUPPORT
	virtual void			OnAnimatedImageChanged(OpWidgetImage* widget_image);
	virtual BOOL			IsAnimationVisible();
#endif
#endif

	// Hooks

	/**
	 called when the window is activated
	 */
	virtual void OnWindowActivated(BOOL activate) {}
	/**
	 called when the window is moved
	 */
	virtual void OnWindowMoved() {}

	/** Return TRUE if scrolling was performed, FALSE otherwize. */
	virtual BOOL OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth = TRUE) { return FALSE; }

#ifndef MOUSELESS
	/**
	 called when the widget revceives a mouse move
	 */
	virtual void OnMouseMove(const OpPoint &point) {}
	/**
	 called when the widget revceives a mouse leave
	 */
	virtual void OnMouseLeave() {}
	/**
	 called when the widget revceives a mouse down
	 */
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks) {}
	/**
	 deprecated. Change your implementation to the new OnMouseUp that takes UINT8 nclicks as third parameter!
	 */
	virtual struct DEPRECATED_DUMMY_RET *OnMouseUp(const OpPoint &point, MouseButton button) { return NULL; }
	/**
	 called when the widget revceives a mouse up
	 */
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks) {}

	/** Return TRUE if you use this hook, FALSE to send it to the parent or document. */
	virtual BOOL OnMouseWheel(INT32 delta,BOOL vertical) { return FALSE; }
#endif // !MOUSELESS

	/**
	 * called when a context menu is requested for the widget. should return TRUE if a context menu is shown.
	 * otherwize, the call will bubble upwards.
	 * @param menu_point The position (relative to the widget) where the popup should appear
	 * @param avoid_rect The area that should not be covered by the popup. NULL if not needed.
	 * @param keyboard_invoked TRUE if context menu is invoked by keypress (as opposed to mouse).
	 */
	virtual BOOL OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked) { return FALSE; }
	/**
	 called before the widget is painted
	 */
	virtual void OnBeforePaint() {}
	/**
	 should paint the widget
	 */
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect) {}
	/**
	 called after all the widget's children are painted
	 */
	virtual void OnPaintAfterChildren(OpWidgetPainter* widget_painter, const OpRect &paint_rect) {}

	/**
	 called whenever the widget's timer times out
	 */
	virtual void OnTimer() {}
	/**
	 called when the widget is added
	 */
	virtual void OnAdded() {}
	/**
	 called when the widget is removed, before removing it
	 */
	virtual void OnRemoving() {}
	/**
	 called when the widget is removed, after removing it
	 */
	virtual void OnRemoved() {}
	/**
	 called when the widget is deleted
	 */
	virtual void OnDeleted() {}
	/**
	 called when the widget's justification mode is changed
	 */
	virtual void OnJustifyChanged() {}
	/**
	 called when the widget's font has changed
	 */
	virtual void OnFontChanged() {}
	/**
	 called when the widget's text direction has changed
	 */
	virtual void OnDirectionChanged() {}

	/** Get widget opacity, from 0 (fully transparent) to 255 (fully solid) */
	virtual UINT8 GetOpacity();
#ifndef MOUSELESS
	/** Called when moving the mouse into the widget. Default sets the defaultcursor. */
	virtual void OnSetCursor(const OpPoint &point);
#endif // !MOUSELESS
	virtual void OnSelectAll() {}
	/**
	  Called when the widgets size changes. new_w and new_h contains the new size
	  and can be changed to force/limit the widgets size.
	*/
	virtual void OnResize(INT32* new_w, INT32* new_h) {}

#ifdef DRAG_SUPPORT
	virtual void OnDragLeave(OpDragObject* drag_object)
	{
		if (listener)
			listener->OnDragLeave(this, drag_object);
	}
	virtual void OnDragCancel(OpDragObject* drag_object)
	{
		if (listener)
			listener->OnDragCancel(this, drag_object);
	}
	virtual void OnDragMove(OpDragObject* drag_object, const OpPoint& point);
	virtual void OnDragStart(const OpPoint& point);
	virtual void OnDragDrop(OpDragObject* drag_object, const OpPoint& point)
	{
		if (listener)
			listener->OnDragDrop(this, drag_object, 0, point.x, point.y);
	}
	virtual void GenerateOnDragStart(const OpPoint& point);
	virtual void GenerateOnDragMove(OpDragObject* drag_object, const OpPoint& point);
	virtual void GenerateOnDragDrop(OpDragObject* drag_object, const OpPoint& point);
	virtual void GenerateOnDragLeave(OpDragObject* drag_object);
	virtual void GenerateOnDragCancel(OpDragObject* drag_object);
#endif // DRAG_SUPPORT
	virtual BOOL IsSelecting() { return FALSE; }

	virtual void OnResizeRequest(INT32 w, INT32 h)
	{
		if (listener)
			listener->OnResizeRequest(this, w, h);
	}

	/**
	  Called when widget will be shown / hidden due to SetVisibility being called on this widget or parent etc
	*/

	virtual void OnShow(BOOL show) {}

	/**
	  Called when widget has been moved relative to root somehow. (that means it might not have been moved relative
	  to parent, but OnMove is still called.)

	  NOTE: it is only called for widgets that have asked for it with SetOnMoveWanted(TRUE)
	*/
	virtual void OnMove() {}

	/** Called when a relayout occurs.. default implementation is to let listener know about it */

	virtual void OnRelayout() {if (listener) listener->OnRelayout(this);}

	/** Called when layout occurs.. widget should layout its contents.. default implementation is to ask listener to do it */

	virtual void OnLayout() {if (listener) listener->OnLayout(this);}

	/** Called when layout is done, and post layout occurs.. widget should do post layout tasks.. default implementation is to ask listener to do it */

	virtual void OnLayoutAfterChildren() {if (listener) listener->OnLayoutAfterChildren(this);}

	/** Called when widget becomes resizable, or when the widget is no longer resizable. */

	virtual void OnResizabilityChanged() {}

	virtual void SetLocked(BOOL locked) { packed.locked = locked; }
	virtual BOOL IsLocked() { return packed.locked; }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

public:
	void	AccessibilitySkipsMe(BOOL skip_me = TRUE) {packed.accessibility_skip = skip_me;}
	void	AccessibilityPrunesMe(AccessibilityPrune prune_me = ACCESSIBILITY_PRUNE) {packed.accessibility_prune = prune_me;}
	BOOL	AccessibilitySkips() {return packed.accessibility_skip;}
	BOOL	AccessibilityPrunes() const {return (packed.accessibility_prune == ACCESSIBILITY_PRUNE || packed.accessibility_prune == ACCESSIBILITY_PRUNE_WHEN_INVISIBLE && !IsVisible());}

	void	SetAccessibilityLabel(AccessibleOpWidgetLabel* label) { m_acc_label = label; }
	void	DisableAccessibleItem();
#ifdef NAMED_WIDGET
	void	FindLabelAssociations();
#endif

	void SetAccessibleParent (OpAccessibleItem* parent) {m_accessible_parent = parent; }

	// accessibility methods
	virtual BOOL		AccessibilityIsReady() const;
	virtual OP_STATUS	AccessibilityClicked();
	virtual OP_STATUS	AccessibilitySetValue(int value);
	virtual OP_STATUS	AccessibilitySetText(const uni_char* text);
	virtual BOOL		AccessibilityChangeSelection(Accessibility::SelectionSet flags, OpAccessibleItem* child);
	virtual BOOL		AccessibilitySetFocus();
	virtual OP_STATUS	AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual OP_STATUS	AccessibilityGetValue(int &value);
	virtual OP_STATUS	AccessibilityGetText(OpString& str);
	virtual OP_STATUS	AccessibilityGetDescription(OpString& str);
	virtual OP_STATUS	AccessibilityGetURL(OpString& str);
	virtual OP_STATUS	AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut);

	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindUnknown;}
	virtual Accessibility::State AccessibilityGetState();

	virtual OpAccessibleItem* GetAccessibleLabelForControl();

	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleParent();
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetNextAccessibleSibling();
	virtual OpAccessibleItem* GetPreviousAccessibleSibling();
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();

	virtual OpAccessibleItem* GetLeftAccessibleObject();
	virtual OpAccessibleItem* GetRightAccessibleObject();
	virtual OpAccessibleItem* GetDownAccessibleObject();
	virtual OpAccessibleItem* GetUpAccessibleObject();

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef WIDGETS_HEURISTIC_LANG_DETECTION
	/** If multilingual is true heuristic language detection will be run on the widget text(if any)  */
	BOOL IsMultilingual() const { return packed.multilingual; }
	void SetMultilingual(BOOL val) { packed.multilingual = val; }
#endif // WIDGETS_HEURISTIC_LANG_DETECTION

#ifdef USE_OP_CLIPBOARD
	// ClipboardListener API
	void OnPaste(OpClipboard* clipboard) {}
	void OnCopy(OpClipboard* clipboard) {}
	void OnCut(OpClipboard* clipboard) {}
#endif // USE_OP_CLIPBOARD

	/** By default, widget is not hoverable when disabled. Set this to true, to override this behaviour */
	BOOL IsAlwaysHoverable() const { return packed.is_always_hoverable; }
	void SetAlwaysHoverable(BOOL val);

protected:
	INT32			m_id; ///< Identification for widgets used in UI

	AffinePos document_ctm; ///< Position in document (if any)

	OpRect rect;
	short text_transform;
	short text_indent;

	OpWidgetPainterManager* painter_manager;
	OpWidgetListener* listener;
	WIDGET_COLOR m_color;
	VisualDevice* vis_dev;
	OpWindow* opwindow;
	WritingSystem::Script m_script;
	short m_overflow_wrap;
	short m_border_left, m_border_top, m_border_right, m_border_bottom;
	short m_margin_left, m_margin_top, m_margin_right, m_margin_bottom;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibleOpWidgetLabel* m_acc_label;
	OpAccessibleItem* m_accessible_parent;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	/** triggers OnScaleChanged if needed */
	void CheckScale();
	/** called from CheckScale (mainly triggered from GenerteOnPaint) if scale of vis_dev has changed */
	virtual void OnScaleChanged() { }
	/** vis_dev scale - should be used as comparison to trigger
		OnScaleChanged only, may not always correspond to actual scale
		of visual device */
	int m_vd_scale;

#ifdef WIDGETS_IME_SUPPORT
	BOOL m_suppress_ime;
	BOOL m_suppress_ime_mouse_down; // only suppresses ime due to mouse down
	FOCUS_REASON m_suppressed_ime_reason; ///< Focus reason used when suppressing IME. Only valid if m_suppress_ime is TRUE.
# ifdef SELFTEST
public:
	BOOL IMESuppressed() const { return m_suppress_ime; }
# endif // SELFTEST
#endif // WIDGETS_IME_SUPPORT
private:

	struct {
		unsigned int is_internal_child:1; ///< Used on f.ex. scrollbars which listener should never be redirected recursive from SetListener.
		unsigned int is_enabled:1;
		unsigned int is_visible:1;
		unsigned int is_tabstop:1;
		unsigned int has_css_border:1;
		unsigned int has_css_backgroundimage:1;
		unsigned int has_focus_rect:1;				// only draw focus rect if keyboard navigation is being used
		unsigned int is_special_form:1; ///< It might belong to the document even though it has no FormObject.
		unsigned int wants_onmove:1;
		unsigned int ellipsis:2; ///< For widgets that draw text that must be clipped. Must have enough bits to hold ELLIPSIS_POSITION.
		unsigned int custom_font:1;
		unsigned int justify_changed_by_user:1;
		unsigned int relayout_needed:1;
		unsigned int layout_needed:1;
		unsigned int is_painting_pending:1;
		unsigned int is_dead:1;
		unsigned int is_added:1;
		unsigned int is_rtl:1;
		unsigned int ignore_mouse:1;
		unsigned int can_have_focus_in_popup:1;
		unsigned int is_mini:1;	///< Set for a widget to try and show itself with it's .mini state (i.e. smaller)
		unsigned int v_align:2; ///< Must have enough bits to hold WIDGET_V_ALIGN
#ifdef WIDGET_IS_HIDDEN_ATTRIB
		unsigned int is_hidden:1;	///< Set for widget that should be included in hierarchy but not used in the layout
#endif // WIDGET_IS_HIDDEN_ATTRIB
		unsigned int is_floating:1; ///< A floating widget is a widget that should not be affected by normal UI-layout
		unsigned int has_received_user_input:1;
#ifdef NAMED_WIDGET
		unsigned int added_to_name_collection:1;
#endif // NAMED_WIDGET
		unsigned int locked:1;
		unsigned int multilingual:1;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
			unsigned int accessibility_skip:1;
			unsigned int accessibility_prune:2; ///<Must have enough bits to hold AccessibilityPrune
			unsigned int accessibility_is_ready:1;
			unsigned int checked_for_label:1;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
		unsigned int is_always_hoverable:1; ///< Set when you want to override m_enabled for mouse events
#ifdef SKIN_SUPPORT
		unsigned int is_skinned:1;
		unsigned int skin_is_background:1; ///< Set when the skin set on this element should only be painted as a background for widgets on top of it
#endif // SKIN_SUPPORT
		unsigned int resizability:3; ///< @see WIDGET_RESIZABILITY
		unsigned int border_box:1; ///< TRUE of box-sizing is set to border-box for this OpWidget.
	} packed;


	OpInputAction*		m_action;
	WidgetContainer*	m_widget_container;
	FormObject*			form_object;
	OpTimer*			m_timer;
	UINT32				m_timer_delay;

	short m_padding_left;
	short m_padding_top;
	short m_padding_right;
	short m_padding_bottom;

#ifdef SKIN_SUPPORT
	OpWidgetImage			m_border_skin;
	OpWidgetImage			m_foreground_skin;

	OpSkinManager*			m_skin_manager;
#endif // SKIN_SUPPORT
#ifndef MOUSELESS
	unsigned char			m_button_mask;
#endif // !MOUSELESS

public:
	Head childs; ///< the list of children, composite widgets such as the file upload consist of several widgets
	OpWidget* parent; ///< the parent widget

	WIDGET_FONT_INFO font_info; ///< the widget's font-info struct

	/**
	 implements OpTimerListener::OnTimeOut - called when m_timer times out. calls UpdatePosition on any form object.
	 will restart timer with m_timer_delay. calls OnTimer.
	 */
	virtual void OnTimeOut(OpTimer* timer);

	OP_STATUS init_status;		///< Should be set if the constructor fails (It is automatically OpStatus::OK).

	/**
	 called when highlight rect changes. will translate the rect to view coords and call OnHighlightRectChanged on the view.
	 */
	void GenerateHighlightRectChanged(const OpRect &rect, BOOL report_caret = FALSE);

	/// Functions that generates hooks for the apropriate childobjects in this widget
	/**
	 sets the widget's visual device, window and widget container. sets its parent as parent input context.
	 executes GenerateOnAdded for all children. calls OnAdded.
	 generates OpWidget::OnAdded
	 @param vd the widget's visual device
	 @param container the widget's container
	 @param window the widget's window
	 */
	virtual void GenerateOnAdded(VisualDevice* vd, WidgetContainer* container, OpWindow* window);

	/**
	 runs GenerateOnRemoved on all children. resets state a bit. calls OnRemoved.
	 */
	virtual void GenerateOnRemoved();
	/**
	 runs GenerateOnDeleted for all children. stops timer and zero:s listener. calls OnDeleted.
	 */
	virtual void GenerateOnDeleted();
	/**
	 runs GenerateOnRemoving for all children. calls OnRemoving.
	 */
	virtual void GenerateOnRemoving();
	/**
	 will call OnMove and run GenerateOnMove on all children if widget says it wants OnMove events
	 */
	virtual void GenerateOnMove();
	/**
	 will call OnWindowActivated and run GenerateOnWindowActivated for all children
	 */
	virtual void GenerateOnWindowActivated(BOOL activate);
	/**
	 will call OnWindowMoved and run GenerateOnWindowMoved for all children
	 */
	virtual void GenerateOnWindowMoved();

	/**
	 if the widget is not visible, nothing is done. if it is, is_painting_pending will be set to FALSE, call
	 OnBeforePaint and run GenerateOnBeforePaint for all children.
	 */
	virtual void GenerateOnBeforePaint();
	/**
	 if paint_rect is empty (width or height 0) or marked not visible, nothing is done. for widgets that have not
	 yet received OnBeforePaint, GenerateOnBeforePaint is called.
	 will update visual device with the font properties of the widget and draw it, using doublebuffering if requested.
	 will run GenerateOnPaint on all children that fall inside the paint rect.
	 @param paint_rect the rect to be painted
	 @param force_ensure_skin if TRUE, widget background and border will be drawn using skin even if it doesn't want to
	 */
	virtual void GenerateOnPaint(const OpRect &paint_rect, BOOL force_ensure_skin = FALSE);
	/**
	 will call OnScrollAction
	 @param delta the scroll delta
	 @param vertical TRUE if vertical scrolling is to be performed
	 @param smooth if TRUE, smooth scrolling is to be used
	 */
	virtual BOOL GenerateOnScrollAction(INT32 delta, BOOL vertical, BOOL smooth = TRUE);
#ifndef MOUSELESS
	/**
	 if there is a captured widget, will make the capured widget the current mouse input context, transform the
	 mouse coords in point to coords local to the captured widget and call OnMouseMove on it.
	 if not, the widget will search its children for a widget that contains the point and call OnMouseMove on it.
	 if none is found, widget will make itself the current input context. if it is enabled, it will change hovered
	 widget to itself if necessary (and generate OnMouseLeave on the previously hovered widget) and call OnMouseMove
	 on itself. if the widget is not enabled it will simply generate OnMouseLeave on any hovered widget.
	 @param point the mouse position (in local document coords)
	 */
	virtual void GenerateOnMouseMove(const OpPoint &point);
	/**
	 zero:s and genreates OnMouseLeave to the hovered and/or captured widget, in that order.
	 */
	virtual void GenerateOnMouseLeave();
#if defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_SUPPRESS_ON_MOUSE_DOWN)
	/**
	 Find the child widget containing the point and call SuppressIMEOnMouseDown on it if it is enabled.
	 @param point the mouse position (in local document coords)
	 */
	virtual void TrySuppressIMEOnMouseDown(const OpPoint &point);

	/**
	 Set flags to not spawn IME after OnMouseDown
	 */
	virtual void SuppressIMEOnMouseDown();
#endif
	/**
	 scans children for one that contains mouse. if there is one, GenerateOnMouseLeave is called on the child and nothing more is done.
	 otherwise, this widget will be set as the captured widget, and is made mouse input context. if the widget is enabled, OnMouseDown
	 will be called on it. if not, any listener will receive OnMouseEvent.
	 @param point the mouse position (in local document coords)
	 @param button the mouse button that was pushed down
	 @param nclicks the number of clicks
	 */
	virtual void GenerateOnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	/**
	 if captured_widget is not set, nothing is done. otherwise, if the captured widget is enabled it will receive OnMouseUp.
	 otherwise any listener will receive OnMouseEvent.
	 if button that was released was MOUSE_BUTTON_2, the widget will try to spawn a context menu. if the widget fails,
	 any listener gets a go. if this also fails, the same process is repeated for its parent, until a context menu is shown
	 or no parent exists.
	 @param point the mouse position (in local document coords)
	 @param button the mouse button that was pushed down
	 @param nclicks the number of clicks
	 */
	virtual void GenerateOnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks = 1);
	/**
	 if the widget is tagged as non-visible, nothing is done. otherwise, OnMouseWheel is called.
	 @param delta the mouse wheel delta
	 @param vertical if TRUE, the mouse wheel delta is to be interpreted as vertical
	 */
	virtual BOOL GenerateOnMouseWheel(INT32 delta,BOOL vertical);
	/**
	 will generate an OnSetCursor event if widget is hovered and tagged visible.
	 */
	virtual void GenerateOnSetCursor(const OpPoint &point);
	/**
	 will call OnMouseUp for all mouse buttons for which the widget has received mouse down but not mouse up
	 */
	virtual void OnCaptureLost();
#endif // !MOUSELESS

	/**
	 called when the widget is shown/hidden due to SetVisibility being called on this widget or parent etc.
	 if the widget is to be hidden and is the hover widget, OnMouseLeave is called and hover widget is cleared.
	 OnShow is called for the widget, and all its visible children.
	 @param show if TRUE, the widget is to be shown
	 */
	virtual void GenerateOnShow(BOOL show);

	// - GetFocused() returns the focused widget (may be NULL).
	/**
	 returns the currently focused widget
	 @return the currently focused widget, or NULL if none is focused
	 */
	static OpWidget*		GetFocused();

	// Overloading OpInputContext functionality

	/**
	 Overloading OpInputContext functionality. if this widget is new_input_context, GenerateHighlightRectChanged may be called.
	 @param new_input_context the input context that gained keyboard input focus
	 @param old_input_context the input context that lost keyboard input focus
	 @param reason the reason for the change in keyboard input focus
	 */
	virtual void			OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	/**
	 Overloading OpInputContext functionality.
	 @param new_input_context the input context that gained keyboard input focus
	 @param old_input_context the input context that lost keyboard input focus
	 @param reason the reason for the change in keyboard input focus
	 */
	virtual void			OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	/**
	 Overloading OpInputContext functionality.
	 @param reason the reason for the change in input focus
	 @return TRUE if the widget is available for focusing
	 */
	virtual BOOL			IsInputContextAvailable(FOCUS_REASON reason);
	/**
	 Overloading OpInputContext functionality.
	 @param reason the reason for the change in input focus
	 */
	virtual void			RestoreFocus(FOCUS_REASON reason);
	/**
	 Overloading OpInputContext functionality.
	 @param parent_context the parent context
	 */
	virtual void			SetParentInputContext(OpInputContext* parent_context, BOOL keep_keyboard_focus = FALSE);

#ifdef QUICK
	// Implementing the OpTreeModelItem interface

	virtual OP_STATUS		GetItemData(ItemData* item_data);
	void					SetID(INT32 id)		{ m_id = id; }
#endif // QUICK

	/**
	 returns the ID of the widget. used for UI widgets only.
	 @return the ID of the widget
	 */
	virtual INT32			GetID() {return m_id;}
	/**
	 returns the type of the widget. this is the top-level type: WIDGET_TYPE. inheriting widgets should overload.
	 @return WIDGET_TYPE
	 */
	virtual Type			GetType() {return WIDGET_TYPE;}

	/** Return TRUE  if the OpWidget is of a specific type. The difference from GetType is that this should propagate up if the
		type mismatch.
		Note: You must ensure that the subclass of the type you want to check for implements this call! */
	virtual BOOL			IsOfType(Type type) { return type == WIDGET_TYPE; }

	// Almost every widget needs a SetText() / GetText() call, so let's make it common

	/**
	 sets the contents of a widget that contains text to text.
	 @param text the new text
	 @return OpStatus::OK if the text was successfully changed, OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR for other errors
	 */
	virtual OP_STATUS		SetText(const uni_char* text) {return OpStatus::ERR_NOT_SUPPORTED;}

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	/**
	 * Sets the contents of a widget that contains text with the parameter text.
	 * Typically, SetText clears the entire undo history because the undo stack only
	 * stores differences between different input actions from the user.
	 * Because SetText does a full replace that would be equivalent to
	 * removing all text and setting a new value. However, that would generate two
	 * undo events. ideally, when undoing or redoing, both events (remove+insert)
	 * should be handled simultaneously.
	 * This method ensures that the SetText call is undoable by the user
	 *
	 * @param text the new text
	 * @return OpStatus::OK if the text was successfully changed, OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR for other errors
	 */
	virtual OP_STATUS		SetTextWithUndo(const uni_char* text) { return OpStatus::ERR_NOT_SUPPORTED; }
#endif

	/**
	 gets the contents of a widget that contains text.
	 @param text (out) filled with the contents of the widget, if it contains text
	 @return OpStatus::OK if the text was successfully retrieved, OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR for other errors
	 */
	virtual OP_STATUS		GetText(OpString& text) {return OpStatus::ERR_NOT_SUPPORTED;}

	/**
	 * Tells if the widget has received user input. This flag is set on OnInputAction
	 * which is typically triggered by user input, if there has been a change of the value
	 * held by the widget.
	 * User input is text or anything that might change the value of the widget.
	 * Interaction that does not change the value of the widget, like panning or scrolling
	 * does not affect this state.
	 */
	inline BOOL HasReceivedUserInput() const { return packed.has_received_user_input; }

	/**
	 * Sets flag which tells if the widget has received user input.
	 * This flag is used to tell if the undo stack should be used for SetText operations
	 * and if the UNDO and REDO actions should be always consumed.
	 * So, after being set, it should be set forever, even when changed not by the user.
	 * Override with reason.
	 **/
	inline BOOL SetReceivedUserInput(BOOL b = TRUE) { return packed.has_received_user_input = b; }

	// .. and some widgets need a SetValue() / GetValue() call, so let's make it common

	/**
	 used by all sorts of widgets that have a value that can be seen as an int.
	 @param value the value to set
	 */
	virtual void			SetValue(INT32 value) {}
	/**
	 @return the value
	 */
	virtual INT32			GetValue() {return 0;}

#ifdef WIDGETS_IMS_SUPPORT

	/**
	 * For select widgets: call from sub class to request the platform
	 * to show and handle the activated widget.
	 *
	 * The widget object will receive a response in either DoCommitIMS() or DoCancelIMS().
	 *
	 *
	 * @return OP_STATUS::OK if the platform successfully handled this activated widget
	 *         OP_STATUS::ERR_NOT_SUPPORTED if the platform doesn't support handling this activated widget
	 *         OP_STATUS::<other error message> if the platform does support handling activated widgets, but failed in doing so because of e.g. OOM
	 */
	OP_STATUS StartIMS();

	/**
	 * For select widgets: call from the sub class to notify platform that
	 * a widget which was previously activated with StartIMS() now has been
	 * updated.
	 *
	 * @return OP_STATUS::OK if the platform successfully handled this event
	 *         OP_STATUS::ERR_NOT_SUPPORTED if the platform doesn't support handling this event
	 *         OP_STATUS::<other error message> if the platform does support this event, but failed in doing so because of e.g. OOM
	 */
	OP_STATUS UpdateIMS();

	/**
	 * For select widgets: call from the sub class to notify platform
	 * that the widget was destroyed when it was active (i.e. after
	 * StartIMS() but befor DoCommitIMS()/DoCancelIMS().
	 *
	 * The sub class will not receive DoCommitIMS() or DoCancelIMS()
	 * after having called DestroyIMS().
	 */
	void DestroyIMS();

	/**
	 * OpIMSListener interfaces.
	 *
	 * Do not implement these, but implement DoCommitIMS() and
	 * DoCancelIMS() instead.
	 */
	void OnCommitIMS(OpIMSUpdateList* updates);
	void OnCancelIMS();

protected:
	/**
	 Called from OpWidget when IMS is committed -- derived classes need to override this.
	 */
	virtual void DoCommitIMS(OpIMSUpdateList* updates) {}
	/**
	 Called from OpWidget when IMS is cancelled -- derived classes need to override this.
	 */
	virtual void DoCancelIMS() {}

	/**
	 * Set any widget specific information in the OpIMSObject that
	 * will be used with communicating with the platform
	 *
	 * Contains a default implementation to set rectangle etc
	 * correctly; override, set widget specific attributs and call
	 * baseclass in derived class.
	 *
	 * @return OP_STATUS::OK if information was successfully set
	 *         OP_STATUS::<error message> otherwise
	 */
	virtual OP_STATUS SetIMSInfo(OpIMSObject& ims_object);

private:
	OpIMSObject m_ims_object;
#endif // WIDGETS_IMS_SUPPORT

	// ---------------------------------------
	// Extended APIS exported by module.export
	//
	// Products that want to use these will have
	// to explicitly import them.
	// ---------------------------------------

#ifdef NAMED_WIDGET
	friend class OpNamedWidgetsCollection;

public:
	/** Return the closest parent collection of named widgets. It may return NULL as some widgets may not have a collection. */
	virtual OpNamedWidgetsCollection *GetNamedWidgetsCollection() { return parent ? parent->GetNamedWidgetsCollection() : NULL; }
	/**
	 * Sets the name of the widget, this is only necessary for widgets
	 * that you will want to search for by name. Note that that means
	 * that the name will have to be unique within a collection. By default that means unique within its
	 * WidgetContainer but you can create new collections for any widget by implementing GetNamedWidgetsCollection.
	 *
	 * @param name of the widget
	 */
    void SetName(const OpStringC8 & name);

	/**
	 * Gets the name of a widget. Note that most widgets will not have
	 * names.
	 *
	 * @return the name of the widget
	 */
    const OpStringC8 & GetName() {return m_name; }

	/**
	 * @return whether the widget has a name
	 */
    BOOL HasName() {return m_name.HasContent();}

	/**
	 * Checks if the name of the widget is the same as name.
	 *
	 * @param name to compare with
	 * @return true if name has content and that content is the same as the name of the widget
	 */
    BOOL IsNamed(const OpStringC8 & name) {return name.HasContent() && m_name.CompareI(name) == 0;}

	/**
	 * Search the closest parent collection for the widget with this name. This uses the function
	 * by the same name in OpNamedWidgetsCollection. Note that this function will therefore
	 * only work for widgets that are in a collection (widgets in a WidgetContainer will always have one).
	 *
	 * @param name to search for
	 * @return pointer to the widget with that name or NULL if no such widget exists
	 */
	OpWidget* GetWidgetByName(const OpStringC8 & name);

	/**
	 * Search only the subtree below this widget for the widget with that name.
	 * Note : this is slower than using GetWidgetByName and is not recomended
	 * unless that is specifically what you want to do.
	 *
	 * @param name to search for
	 * @return pointer to the widget with that name or NULL if no such widget exists
	 */
	OpWidget * GetWidgetByNameInSubtree(const OpStringC8 & name);

protected :

	/**
	 * Will be called if/when the name of this widget has been set.
	 * Can be redefined in the subclasses to perform actions that
	 * depend on the widget name being known.
	 */
	virtual void OnNameSet() {}

private :

    OpString8 m_name;

#ifdef DEBUG_ENABLE_OPASSERT
	BOOL	m_has_duplicate_named_widget;
#endif // DEBUG_ENABLE_OPASSERT
#endif // NAMED_WIDGET

};

// ---------------------------------------------------------------------------------

#ifdef QUICK

// Cannot use normal OpVector because DeleteAll doesn't work with
// OpWidget's protected destructor

template<class T>
class OpWidgetVector : private OpGenericVector
{
public:

	/**
	 * Creates a vector, with a specified allocation step size.
	 */
	OpWidgetVector() {}

	/**
	 * Clear vector, returning the vector to an empty state
	 */
	void Clear() { OpGenericVector::Clear(); }

	/**
	 * Makes this a duplicate of vec.
	 */
	OP_STATUS DuplicateOf(const OpVector<T>& vec) { return OpGenericVector::DuplicateOf(vec); }

	/**
	 * Replace the item at index with new item
	 */
	OP_STATUS Replace(UINT32 idx, T* item) { return OpGenericVector::Replace(idx, item); }

	/**
	 * Insert and add an item of type T at index idx. The index must be inside
	 * the current vector, or added to the end. This does not replace, the vector will grow.
	 */
	OP_STATUS Insert(UINT32 idx, T* item) { return OpGenericVector::Insert(idx, item); }

	/**
	 * Adds the item to the end of the vector.
	 */
	OP_STATUS Add(T* item) { return OpGenericVector::Add(item); }

	/**
	 * Removes item if found in vector
	 */
	OP_STATUS RemoveByItem(T* item) { return OpGenericVector::RemoveByItem(item); }

	/**
	 * Removes count items starting at index idx and returns the pointer to the first item.
	 */
	T* Remove(UINT32 idx, UINT32 count = 1) { return static_cast<T*>(OpGenericVector::Remove(idx, count)); }

	/**
	 * Removes AND Delete (call actual delete operator) item if found
	 */

	OP_STATUS Delete(T* item)
	{
		RETURN_IF_ERROR(RemoveByItem(item));

		item->Delete();

		return OpStatus::OK;
	}

	/**
	 * Removes AND Delete (call actual delete operator) count items starting at index idx
	 */

	void Delete(UINT32 idx, UINT32 count = 1)
	{
		// delete items

		for (UINT32 i = 0; i < count; i++)
		{
			Get(idx + i)->Delete();
		}

		// clear nodes

		Remove(idx, count);
	}

	/**
	 * Removes AND Delete (call actual delete operator) all items.
	 */

	void DeleteAll()
	{
		Delete(0, GetCount());
	}

	/**
	 * Finds the index of a given item
	 */
	INT32 Find(T* item) const { return OpGenericVector::Find(item); }

	/**
	 * Returns the item at index idx.
	 */
	T* Get(UINT32 idx) const { return static_cast<T*>(OpGenericVector::Get(idx)); }

	/**
	 * Returns the number of items in the vector.
	 */
	UINT32 GetCount() const { return OpGenericVector::GetCount(); }
};

#endif // QUICK

/**
  * WidgetSafePointer will keep a OpWidget pointer that will automatically be set to NULL
  * if the widget is deleted. Only use this for short periods, for example on the stack to
  * detect if a call result in a widget being deleted. Do not create excessive amounts of
  * it, because it's expensive.
  */
class WidgetSafePointer : public OpWidgetExternalListener
{
public:
	WidgetSafePointer(OpWidget* widget)
	: m_widget(widget)
	, m_deleted(FALSE)
	{
		g_opera->widgets_module.AddExternalListener(this);
	}

	~WidgetSafePointer()
	{
		g_opera->widgets_module.RemoveExternalListener(this);
	}

	OpWidget* GetPointer() { return m_widget; }
	BOOL IsDeleted() { return m_deleted; }

private:
	virtual void OnDeleted(OpWidget *widget)
	{
		if (m_widget == widget)
		{
			m_widget = NULL;
			m_deleted = TRUE;
		}
	}

	OpWidget*	m_widget;
	BOOL		m_deleted;
};

#endif // OP_WIDGET_H
