/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef WIDGETS_CAPABILITIES_H
#define WIDGETS_CAPABILITIES_H

#define OP_WIDGET_CAP_PADDING	1			/// Padding is supported
#define OP_WIDGET_CAP_IMPROVED_PADDING 1	/// Padding support is improved since the above. (Was only working on buttons using css)
#define OP_WIDGET_CAP_HILDON_EXTENSIONS 1	/// More features in skinsystem resulted in more functions in OpWidget.
#define OP_WIDGET_CAP_IME_IN_INPUTCONTEXT 1	/// Using the new IME API (virtual functions in OpInputContext)
#define IME_COMPOSE_PART
#define OP_WIDGET_CAP_GROUPED_GLOBALS 1		/// All statics and globals is placed in a single global struct.
#define OP_WIDGET_CAP_WF2CC					/// Updated to call-for-comments version of WF2

// Continue using this syntax: WIDGETS_CAP_xxxxxx

/**
 * Has a slider widget (used by WebForms2)
 */
#define WIDGETS_CAP_SLIDER_WIDGET

/**
 * Has OpWidget::IsAtLastInternalTabStop and OpWidget::GetNextInternalTabStop.
 */
#define WIDGET_CAP_HAS_INTERNAL_TAB_STOP

/**
 * The calendar now supports week selection and can be used for input
 * type week. Added november 2004 in core-2.
 */
#define WIDGETS_CAP_CALENDAR_WEEK

#define WIDGETS_CAP_SKIN_WITHOUT_QUICK

/** Has OpTextFragmentList */
#define WIDGETS_CAP_OPTEXTFRAGMENTLIST
#define WIDGETS_CAP_OPTEXTFRAGMENTPACKED

#define WIDGETS_CAP_CHECKBOX_INHERITS_BUTTON	// OpCheckBox has parent OpButton
#define WIDGETS_CAP_RADIOBUTTON_INHERITS_BUTTON	// OpRadioButton has parent OpButton
#define WIDGETS_CAP_HAS_ONMOVE_WANTED			// OpWidget has the SetOnMoveWanted function.
#define WIDGETS_CAP_HAS_SET_FORCED_FOCUSED_LOOK // OpButton has SetForcedFocusedLook
#define WIDGETS_CAP_HAS_CARETOFFSET				// OpWidget has SetCaretOffset, GetCaretOffset
#define WIDGETS_CAP_HAS_OPWIDLAYOUT				// OpWidgetLayout, GenerateOnLayout et al.

#define WIDGETS_CAP_HAS_SETRTL					// OpWidget has SetRTL

#define WIDGETS_CAP_HAS_REMOVEGROUP				// Has RemoveGroup (optiongroups in listboxes)
#define WIDGETS_CAP_HAS_REMOVEALLGROUPS			// Has RemoveAllGroups (optiongroups in listboxes)

#define WIDGETS_CAP_HAS_OPTIONCOLOR				// Can set colors on individual listitems.

#define WIDGETS_CAP_HAS_STEP_NUMBER				// The number widgets handles steps

#define WIDGETS_CAP_HAS_SETLINEHEIGHT			// OpMultilineEdit has SetLineHeight

#define WIDGETS_CAP_HAS_SETSCROLLBARSTATUS		// OpMultilineEdit has SetScrollbarStatus
#define WIDGETS_CAP_HAS_SETSCROLLBARSTATUSXY	// OpMultilineEdit has SetScrollbarStatus with x and y parameter

#define WIDGET_CAP_HAS_OPSTRINGITEM_SETCOLOR	// OpStringItem::SetFgColor and SetBgColor

#define WIDGETS_CAP_HAS_ISSCROLLABLE			// OpWidget has IsScrollable

#define WIDGETS_CAP_DATALIST_AUTOCOMPLETE		// AutoComplete can be done based on WF2's datalist element.

#define WIDGETS_CAP_HAS_BIDI_DIRECTION_OVERRIDE	// Has override in OpTextFragmentList::Update

#define WIDGETS_CAP_USES_GNTF_WITH_CODEPAGE     // Uses GetNextTextFragment which takes OpFontInfo::CodePage as an argument

#define WIDGETS_CAP_HAS_STEP_TIME				// OpTime handles steps, min and max

#define WIDGETS_CAP_HAS_STEP_CALENDAR			// OpCalendar handles steps

#define WIDGETS_CAP_HAS_SEARCHTEXT_NOSELECT		// Has scroll_to_selected_match parameter in SearchText

#define WIDGETS_CAP_HAS_LAYOUT_AFTER_CHILDREN	// Has OnLayoutAfterChildren() in the OpWidgetListener

#define WIDGETS_CAP_GEN_HRECT_CHANGED			// Has GenerateHighlightRectChanged() in OpWidget

#define WIDGETS_CAP_MINIMAL_UPLOAD_WIDGET		// Has a very small OpFileChooserEdit widget even when _FILE_UPLOAD_SUPPORT_ is not set.

#define WIDGETS_CAP_LISTBOX_GROUPS				// There are some methods to query lists about the existance of grouping

#define WIDGETS_CAP_HAS_ISIMEACTIVE				// Has WidgetInputMethodListener::IsIMEActive

#define WIDGETS_CAP_RESIZE_CORNER_FOR_MOUSELESS // Has a limited functionality OpResizeCorner even when MOUSELESS is set

#define WIDGETS_CAP_DROPDOWN_HAS_IS_DROPPED_DOWN // OpDrowDown::IsDroppedDown() exists

#define WIDGET_CAP_IME_SHOW_CANDIDATE			// Implementation of OnCandidateShow

#define WIDGETS_CAP_MOUSE_UP_HAS_COUNT			// OpWidget::GenerateOnMouseUp has a nclick arguments similar to GenerateOnMouseDown

#define WIDGET_CAP_SET_FALLBACK_PAINTER			// Has OpWidgetPainterManager::SetUseFallbackPainter

#define WIDGETS_CAP_LISTBOX_FIND_ITEM			// Has OpListBox::FindItemAtPosition

#define WIDGETS_CAP_BEGINCHANGEPROPERTIES		// Has OpWidget::BeginChangeProperties

#define WIDGETS_CAP_SMOOTHSCROLL_IS_PUBLIC		// OpScrollbar inherits public OpSmoothScroller

#define WIDGETS_CAP_SET_MULTIPLE				// ItemHandler and OpListBox has SetMultiple(BOOL) methods

#define WIDGETS_CAP_ZERO_ACTIVE_WIDGETS			// OpWidget has ZeroCapturedWidget and ZeroHoveredWidget

#define WIDGETS_CAP_CALENDARHOVER				// OpCalendar has m_is_hovering_button

#define WIDGETS_CAP_NEW_ELLIPSIS_FUNCTIONS		// OpWidget uses Set/GetEllipsis functions

#define WIDGETS_CAP_HAS_INTERNAL_TAB_STOP		// OpWidget::GetFocusableInternalEdge exists

#define WIDGETS_CAP_WIDGETWINDOW_INIT_EFFECTS	// WidgetWindow::Init() has an effects parameter (which is passed on to OpWindow::Init())

#define WIDGETS_CAP_MINI_SIZE					// Set when IsMiniSize() function exists

#define WIDGETS_CAP_MULTIEDIT_IN_USE			// OpMultilineEdit::IsInUseByExternalCode exists

#define WIDGETS_CAP_EDIT_MARQUEE				// Allows marquees in OpLabel (Quick ONLY)

#define WIDGETS_CAP_MOVE_LEAVE_IN_LISTENER      // OpWidgetListener has OnMouseMove and OnMouseLeave

#define WIDGETS_CAP_INCLUDE_CSSPROPS			// This module includes layout/cssprops.h where needed, instead of relying on logdoc/htm_elm.h doing it.

#define WIDGETS_CAP_QUICK_WIDGET_MOVED			// QuickWidget definition has moved to Quick

#define WIDGETS_CAP_CHECKBOX_RADIO_PADDING		// Is handling padding on checkbox and radio more like the spec (if there where any spec, but less like other browsers and more like layoutboxes)

#define WIDGETS_CAP_HAS_SETPROPS				// OpWidget has SetProps

#define WIDGETS_CAP_BEFOREPAINT_RETURNS			// BeforePaint returns a value if it yielded

#define WIDGETS_CAP_IMAGEANDTEXT_ON_LEFT		// Has OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT

#define WIDGETS_CAP_HAS_ONITEMEDITABORTED		// OpWidgetListener has OnItemEditAborted()

#define WIDGETS_CAP_HAS_LEFTHANDEDUI			// Has OpWidget::LeftHandedUI

#define WIDGETS_CAP_INHERITABLE_FILECHOOSER		// OpFileChooserEdit has been reduced in scope and uses windowcommander apis for ui interaction

#define WIDGETS_CAP_WIC_FILESELECTION			// Will use OpFileSelectionListener in windowcommander if available (instead of OpFileChooser in pi)

#define WIDGETS_CAP_IME_STYLE					// There is {Get,Set}IMStyle in OpEdit and OpMultiEdit

#define WIDGETS_CAP_HAS_ACTION_SELECTED_ITEM	// Has ACTION_SELECT_ITEM and removes hardcoded OP_KEY_ENTER for selecting items in dropdown.

#define WIDGETS_CAP_EDIT_CREATE_SPELLSESSION	// OpEdit and OpMultiEdit have CreateSpellSessionId()

#define WIDGETS_CAP_INI_KEYS_ASCII				// Supports ASCII key strings for actions/menus/dialogs/skin

#define WIDGETS_CAP_HAS_EXTENDED_SEARCH			// Has wrap and next parameters in SearchText

// uses GetBestFont and GetNextTextFragment taking script if present
#define WIDGETS_CAP_MULTISTYLE_AWARE

/** New context menu system which means that display does much less and doc/logdoc much more. */
#define WIDGETS_CAP_ONCONTEXTMENU

#define WIDGETS_CAP_ITEMHANDLER_SETSCRIPT // ItemHandler has SetScript
#define WIDGETS_CAP_HAS_GETRELEVANTOPTIONRANGE // OpDropDown and OplistBox has GetRelevantOptionRange

#define WIDGETS_CAP_CARET_POS					// Has methods to check for caret pixel position

#define WIDGETS_CAP_HAS_DISABLE_SPELLCHECK		// OpMultiLineEdit has DisableSpellcheck

#define WIDGETS_CAP_WRITINGSYSTEM_ENUM			// Understands that HTMLayoutProperties::current_script may be an enum

#define WIDGETS_CAP_UNICODE_POINT_SUPPORT		// Uses UnicodePoint to represent code points.

#define WIDGETS_CAP_DRAWSTRING_ONLY_TEXT		// OpWidgetString::Draw has parameter only_text

#define WIDGETS_CAP_SPELLCHECK_ATTR_SUPPORT     // OpMultiLineEdit and OpEdit provides EnableSpellcheck() and DisableSpellcheck(BOOL force) needed to control the spellchecker state according to the spellcheck attribute.

#define WIDGETS_CAP_MULTILINEEDIT_CARETCOORD	// Has OpMultilineEdit::GetCaretCoordinates

#define WIDGETS_CAP_BUTTON_STYLE_OMENU			// Has OpButton::STYLE_OMENU

#define WIDGETS_CAP_NUMBEREDIT_ALLOWED_CHARS	// Has OpNumberEdit::SetAllowedChars

#define WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN	// Set when the platform can use the default and custom drop down windows

#define WIDGETS_CAP_SETTEXT_DOES_UNDOREDO		//SetText stores events on the unro/redo stack so it can be undone

#define WIDGETS_CAP_MULTILINEEDIT_AGGRESSIVE_WRAP   // Has OpMultilineEdit::SetAggressiveWrapping

#endif // !WIDGETS_CAPABILITIES_H
