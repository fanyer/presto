/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_MULTIEDIT_H
#define OP_MULTIEDIT_H

#include "modules/widgets/OpWidget.h"
#include "modules/util/simset.h"
#include "modules/widgets/OpTextCollection.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpResizeCorner.h"

#include "modules/display/fontdb.h"
#include "modules/display/vis_dev.h"

#ifdef MULTILABEL_RICHTEXT_SUPPORT
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmltokenhandler.h"
#endif // MULTILABEL_RICHTEXT_SUPPORT

class OpResizeCorner;

/** OpMultilineEdit
 *  Widget for displaying/editing text with multiple lines.
*/

class OpMultilineEdit : public OpWidget, public OpTCListener
#ifndef QUICK
, public OpWidgetListener
#endif
{
protected:
	OpMultilineEdit();
	~OpMultilineEdit();

        virtual void OnScaleChanged();
	virtual void PaintMultiEdit(UINT32 text_color, UINT32 selection_text_color,
		UINT32 background_selection_color, const OpRect& rect);


public:
	/**
	 creates a text area widget
	 @param obj (out) a handle to the created object
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on error
	 */
	static OP_STATUS Construct(OpMultilineEdit** obj);

#ifdef WIDGETS_CLONE_SUPPORT
	virtual OP_STATUS CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded);
	virtual BOOL IsCloneable() { return TRUE; }
#endif

#ifdef WIDGETS_IME_SUPPORT
	/**
           documented in OpWidget
	 */
	virtual BOOL IMEHandleFocusEventInt(BOOL focus, FOCUS_REASON reason);

	void SetFocusComesFromScrollbar() { m_packed.focus_comes_from_scrollbar = TRUE; }
#ifdef SELFTEST
	BOOL GetFocusComesFromScrollbar() { return m_packed.focus_comes_from_scrollbar; }
#endif // SELFTEST
#endif // WIDGETS_IME_SUPPORT

	/**
	 replaces any text in the text area with text. caret will be placed at the end of the text,
	 unless the text area is not part of a form (i.e. a ui text area) in which case the caret
	 is placed at the start of the text.
	 @param text the new text
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS		SetText(const uni_char* text){ return SetTextInternal(text, FALSE); }

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
	virtual OP_STATUS		SetTextWithUndo(const uni_char* text){ return SetTextInternal(text, TRUE); }
#endif
	/**
	 fills text with the current content of the text area. any linebreaks will be discarded.
	 @param text (out) a handle to the fetched text
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS		GetText(OpString& text) {return GetText(text, FALSE);}

	/** Compare contents of text collection to str
	 @param str the string to compare to (used as second arg to uni_strcmp)
	 @param max_len maximum number of characters to compare
	 @param offset offset from start of contents of text collection
	*/
	INT32 CompareText(const uni_char* str, INT32 max_len = -1, UINT32 offset = 0) { return multi_edit->CompareText(str, max_len, offset); }

	/**
	 calculates the prefered size of the text area.
	 @param w (out) the prefered width of the text area
	 @param h (out) the perered height of the text area
	 @param cols the width of the text area, in characters
	 @param rows the height of the text area, in lines of text
	 \note cols and rows will be set to 1 if less than 1
	 */
	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	/**
	 @return the width of the longest text line
	 */
	INT32 GetContentWidth() { return multi_edit->total_width; }
	/**
	 @return the width of the longest text block (not affected by text justification)
	 */
	INT32 GetMaxBlockWidth() { return multi_edit->max_block_width; }
	/**
	 @return the height of all lines of text in the text area
	 */
	INT32 GetContentHeight() { return multi_edit->total_height; }

	/**
	 returns the number of characters in the text area
	 @param insert_linebreak if TRUE, line breaking characters will be included in the count
	 \note line break is always \\r\\n
	 @return the number of characters in the text area
	 */
	INT32 GetTextLength(BOOL insert_linebreak);
	/**
	 fills buf with the current contents of the text area
	 @param buf (out) the target buffer
	 @param buflen the size of the buffer. no more than buflen characters will be copied.
	 @param offset if non-zero, the first offset characters will be skipped
	 @param insert_linebreak if TRUE, linebreaks will be inserted where they occur in the text area
	 \note line break is always \\r\\n
	 */
	void GetText(uni_char *buf, INT32 buflen, INT32 offset, BOOL insert_linebreak);
	/**
	 fills str with the current contents of the text area
	 @param str (out) a handle to the fetched text
	 @param insert_linebreak if TRUE, linebreaks will be inserted where they occur in the text area
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS GetText(OpString &str, BOOL insert_linebreak);

	/**
	 inserts text at the current caret position. any selected text will be deleted before insertion.
	 @param text the text to be inserted
	 @param use_undo_stack if FALSE, the insert is not recorded in the undo stack, otherwise it is
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS InsertText(const uni_char* text, BOOL use_undo_stack = TRUE);

	/**
	 controlls whether the text area is editable or not
	 @param readonly if TRUE, the text area is set to be in read-only mode
	 */
	void SetReadOnly(BOOL readonly);
	/**
	 controlls the wrapping behaviour of the text area
	 @param status if TRUE, text will be wrapped
	 */
	void SetWrapping(BOOL status);
	/**
	 makes text break in the middle of a word if necessary.
	 @param break_word toggles aggressive wrapping if SetWrapping is set to TRUE
	 */
	void SetAggressiveWrapping(BOOL break_word);
        /**
           @return TRUE if aggressive wrapping is enabled, FALSE otherwise
         */
        BOOL GetAggressiveWrapping();

	/**
	 selects all text in the text area. will not call OnSelect on any listener.
	 */
	void SelectText();

	/**
	 sets the ghost text of the text area. the gost text is shown whenever the text area is not
	 focused and has no text
	 */
	OP_STATUS SetGhostText(const uni_char* ghost_text);
	BOOL GhostMode() { return m_packed.ghost_mode; }

	/** sets the text area in flat mode. Flatmode is: no border, no scrollbar, read only. */
	void SetFlatMode();

	/** sets the text area in label mode. Labelmode is: Flatmode + optionally no selection, no context menu, no I-cursor
	  * @param allow_selection if FALSE, there is no selection, context menu or I-cursor
	  */
	void SetLabelMode(BOOL allow_selection = FALSE);

#ifndef HAS_NO_SEARCHTEXT
	/**
	 search the text area for occurences of txt
	 @param txt the text to search for
	 @param len the length of the text to search for
	 @param forward if TRUE, searching forward, if not searching backwards
	 @param match_case if TRUE, case is taken into consideration when searching
	 @param words if TRUE, only whole words are considered a match
	 @param type if SEARCH_FROM_CARET, search is commenced from the current caret position
	 if SEARCH_FROM_BEGINNING, search is commenced from the beginning of the text in the text area
	 if SEARCH_FROM_END, search is commenced from the end of the text in the text area
	 @param select_match if TRUE, any match is selected
	 @param scroll_to_selected_match if TRUE, scrolling of text and document is performed to show match
	 @return TRUE if a match was found, FALSE otherwise
	 */
	BOOL SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words, SEARCH_TYPE type, BOOL select_match, BOOL scroll_to_selected_match, BOOL wrap, BOOL next);
#endif // !HAS_NO_SEARCHTEXT
	/**
	 @return TRUE if all or a part of the text in the text area is selected
	 */
	BOOL HasSelectedText() { return multi_edit->HasSelectedText(); }
	/**
	 deselects any selection
	 */
	void SelectNothing();

	/**
	 retrieves the start and stop position of any selected text in the text area
	 @param start_ofs (out) the offset to the start of the selection
	 @param stop_ofs (out) the offset to the end of the selection
	 if nothing is selected start_ofs and stop_ofs are given the value 0
	 @param direction (out) the direction of the selection
	 @param line_normalized if TRUE, then report offsets by normalizing
	 occurrences of CRLF line terminators as LF when counting.
	 */
	virtual void GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction, BOOL line_normalized = FALSE);
	/**
	 selects the part of the text that falls between start_ofs and stop_ofs
	 @param start_ofs the start of the selection
	 @param stop_ofs the end of the selection
	 @param direction the direction of the selection
	 \note if start_ofs < 0, the selection will start from the beginning, and
	 if stop_ofs is bigger than the length of the text it will stop at the end
	 @param line_normalized if TRUE, then the offsets are "line terminator"
	 normalized, meaning that any occurrence of CRLF in the underlying text
	 counts as one, not two characters when computing selection extent.
	 */
	virtual void SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction = SELECTION_DIRECTION_DEFAULT, BOOL line_normalized = FALSE);
	/**
	 highlights the search result by selecting the part of the text that falls between start_ofs and stop_ofs
	 @param start_ofs the start of the selection
	 @param stop_ofs the end of the selection
	 @param is_active_hit TRUE if this is the active search result
	 \note if start_ofs < 0, the selection will start from the beginning, and
	 if stop_ofs is bigger than the length of the text it will stop at the end
	 */
	virtual void SelectSearchHit(INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit);
	virtual BOOL IsSearchHitSelected();
	virtual BOOL IsActiveSearchHitSelected();
	/**
	 returns the offset from the start of the text to the caret
	 */
	virtual INT32 GetCaretOffset();
	/**
	 moves the caret to the position caret_ofs characters from the start of the text
	 @param caret_ofs the new caret position
	 */
	void SetCaretOffset(INT32 caret_ofs);

private:
	/**
	 Calls GenerateHighlightRectChanged (in OpWidget) to notify the system that
	 the caret position has changed since the last time it was reported.
	 */
	void ReportCaretPosition();

public:

	/**
	 returns the offset from the start of the text to the caret, counting
	 CRLF line terminators as a single character.
	 */
	INT32 GetCaretOffsetNormalized();
	/**
	 moves the caret to the position caret_ofs characters from the start of the text,
	 a CRLF line terminator counting as a single character.
	 @param caret_ofs the new caret position
	 */
	void SetCaretOffsetNormalized(INT32 caret_ofs);

	/**
	 @return TRUE if the text area has a visible vertical scrollbar
	 */
	BOOL IsVerticalScrollbarVisible() {return y_scroll->IsVisible();}
	/**
	 @return TRUE if the text area has a visible horizontal scrollbar
	 */
	BOOL IsHorizontalScrollbarVisible() {return x_scroll->IsVisible();}

	/**
	 changes the colors of the scrollbars
	 @param colors the new colors of the scrollbars
	 */
	void SetScrollbarColors(const ScrollbarColors &colors);
	/**
	 draws the text area using the color color for the text
	 @param color the text color
	 */
	virtual void OutputText(UINT32 color);

	/**
	 performs the action
	 @param action the action to perform
	 */
	void EditAction(OpInputAction* action);
	/**
	 called before SetSelection, Undo, Redo, Cut and Paste.
	 updates font and prepares the text collection for changes.
	 */
	void BeforeAction();
	/**
	 called after SetSelection, Undo, Redo, Cut and Paste.
	 triggers changes to the text collection.
	 */
	void AfterAction();
	/**
	 removes any selected text from the text area and puts it on the clipboard.
	 can fail on OOM, but will report it.
	 */
	void Cut();
	/**
	 copies the selected text to the clipboard. can fail on OOM, but will report it.
	 @param to_note if TRUE, text is copied as an opera note and not to clipboard
	 */
	void Copy(BOOL to_note = FALSE);
#ifdef USE_OP_CLIPBOARD
	// ClipboardListener API
	void OnPaste(OpClipboard* clipboard);
	void OnCopy(OpClipboard* clipboard);
	void OnCut(OpClipboard* clipboard);
#endif // USE_OP_CLIPBOARD
#ifdef WIDGETS_UNDO_REDO_SUPPORT
	/**
	 undos the last input action performed on the input field
	 */
	void Undo(BOOL ime_undo = FALSE, BOOL scroll_if_needed = TRUE);
	/**
	 redos the last undo action performed on the input field
	 */
	void Redo();
#endif // WIDGETS_UNDO_REDO_SUPPORT
#if defined _TRIPLE_CLICK_ONE_LINE_
	/**
	 selects the entire line of text that the caret is currently on
	 */
	void SelectOneLine();
#endif
	/**
	 selects all text in the text area, and calls OnSelect on any listener
	 */
	void SelectAll();
	/**
	 removes all text in the text area
	 */
	void Clear();

	// == Hooks ======================

	/**
	 called before properties are being changed. keeps track on whether a reformat is needed.
	 */
	void BeginChangeProperties();
	/**
	 called after properties have been changed. will reformat if needed.
	 */
	void EndChangeProperties();

	/**
	 called when the contents of the text area should be scrolled. will scroll the content of the text area.
	 @param widget the scrollbar that performs the scroll
	 @param old_val the scroll position before the scroll
	 @param new_val the scroll position after the scroll
	 @param caused_by_input TRUE if user input caused the scroll
	 */
	void OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input);

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from
	   OpWidgetListener. */
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}
	virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked) { return FALSE; }
	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point) {}
#ifdef DRAG_SUPPORT
	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y) {}
	virtual void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) {}
	virtual void OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) {}
	virtual void OnDragLeave(OpWidget* widget, OpDragObject* drag_object) {}
#endif // DRAG_SUPPORT
#endif // QUICK

	/**
	 called when the text area is being deleted. will abort any open IME and delete the text collection.
	 */
	virtual void			OnDeleted();
	virtual void			OnRemoving();
	/**
	 called when the text area is to be painted. this method should paint the widget.
	 @param widget_painter the painter object
	 @param paint_rect the rect in witch to paint
	 */
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
#ifndef MOUSELESS
	/**
	 called when the mouse is moved over the widget. if the mouse is over the inner part of the text area,
	 the mouse cursor will be changed to a text cursor. if the text area is in label mode, the default arrow
	 will be used.
	 */
	virtual void OnSetCursor(const OpPoint &point);
#endif // !MOUSELESS

	/**
	 Called when the widget changes visibility so the caret timer can be stopped or started
	 */
	void OnShow(BOOL show);
	/**
	 called when the text area is resized. if the text area is in wrapping mode, the text will be rearranged
	 to the new size. the scrollbars will be updated to reflect the new size.
	 */
	void OnResize(INT32* new_w, INT32* new_h);
	/**
	 called when the widget is moved. any IME will be notified of the move.
	 */
	void OnMove();
	/**
	 makes the caret blink, and scrolls the text if the mouse pointer ever falls outside the boundaries
	 of the text area during selection.
	 */
	void OnTimer();
	/**
	 called when the focus status for the text area changes. lots of platform-dependent behaviour here.
	 if text area is focused, the timer is started so that the caret will blink. otherwise the timer is
	 stopped. will trigger OnChangeWhenLostFocus to any listener if the contents has been changed.
	 @param focus TRUE if the text area gained focus, FALSE if it lost it
	 @param reason the reason for the focus change
	 */
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	/**
	 called when the widget's justification is changed. will invalidate.
	 */
	void OnJustifyChanged();
	/**
	 called when the widget's font is changed. will reformat the widget, either directly or eventually.
	 */
	void OnFontChanged();
	/**
	 called when the text direction of the widget is changed. does nothing.
	 */
	void OnDirectionChanged();

	/**
	Called when the OpWidget becomes resizable, or when the OpWidget isn't
	resizable anymore.
	@see SetResizability
	*/
	void OnResizabilityChanged();

	/**
	 calls OnScroll on the vertical or horizontal scrollbar depending on vertical
	 @param delta the amount to scroll
	 @param vertical TRUE if the scrolling is vertical
	 @param smooth TRUE if smooth scrolling is to be pserformed
	 @return TRUE if scroll was performed (new value != old value)
	 */
	BOOL OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth = TRUE);

	/**
	 calls SetValue on the vertical or horizontal scrollbar depending on vertical
	 @param value place to scroll to
	 @param vertical TRUE if the scrolling is vertical
	 @param caused_by_input see OpScrollbar::SetValue caused_by_input parameter
	 */
	BOOL SetScrollValue(INT32 value, BOOL vertical, BOOL caused_by_input);

#ifndef MOUSELESS
	/**
	 called when the text area should show its context menu. if QUICK is defined,
	 the context menu corresponding to the mode the text area is in will be shown.
	 @param point the mouse position (in local document coords)
	 @return TRUE, always
	 */
	virtual BOOL OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);
	/**
	 triggers scrolling of the text area
	 @param delta the mouse wheel delta
	 @param vertical if TRUE, the wheel action is interpreted as vertical, as opposed to horizontal
	 */
	BOOL OnMouseWheel(INT32 delta,BOOL vertical);
	/**
	 called whenever the text area receives a mouse down. will trigger mouse event to any listener.
	 if the text area is not in a forms element (i.e. it's a ui widget) it will focus itself.
	 if IME support is enabled and text area is in compose mode nothing will be done with the click.
	 clicks on the border do nothing.
	 triple click will select all text in the text area, lest _TRIPLE_CLICK_ONE_LINE_ is defined, in
	 which case the line of text that currently has the caret will be selected.
	 if a double click is received, the currently clicked word will be selected.
	 if one click is received and the shift key is held down, selection will be made from the previous
	 caret position to the click, and the caret is moved to the clicked position.
	 if shift is not held down and one click is received, the caret will be moved to the clicked position.

	 @param point the mouse position (in local doc coords)
	 @param button the mouse button that was pushed down
	 @param nclicks the number of clicks
	 */
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	/**
	 called whenever the text area receives a mouse up. will trigger mouse event to any listener.
	 if a selection was made, any listener will receive OnSelect.

	 @param point the mouse position (in locald doc coords)
	 @param button the mouse button that was released
	 @param nclicks the number of clicks
	 */
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	/**
	 called whenever the text area receives a mouse move. will scroll the contents if selecting and
	 mouse hits an edge. if left mouse button is held down, will select text - words or characters
	 depend on the number of mouse downs the edit field has received.

	 @param point the current mouse position
	 */
	void OnMouseMove(const OpPoint &point);
#endif // !MOUSELESS

#ifdef WIDGETS_IME_SUPPORT
	/** documented in OpInputMethod */
	IM_WIDGETINFO OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose);
	/** documented in OpInputMethod */
	IM_WIDGETINFO OnCompose();
	/** documented in OpInputMethod */
	IM_WIDGETINFO GetWidgetInfo();
	/** documented in OpInputMethod */
	void OnCommitResult();
	/** documented in OpInputMethod */
	void OnStopComposing(BOOL cancel);


#ifdef IME_RECONVERT_SUPPORT
	/** documented in OpInputMethod */
	virtual void OnPrepareReconvert(const uni_char*& str, int& sel_start, int& sel_stop);
	/** documented in OpInputMethod */
	virtual void OnReconvertRange(int sel_start, int sel_stop);
#endif

	INT32 GetCaretCharPosGlobal() { return multi_edit ? multi_edit->caret.GetGlobalOffset(TCGetInfo()): 0;}
	void SetCaretCharPosGlobal(INT32 ofs) { if (multi_edit) multi_edit->caret.SetGlobalOffset(TCGetInfo(), ofs); }

	/**
	 * Sets the im style which is a string given by the webpage for instance
	 * and sent to the IME system. The string will be copied.
	 *
	 * @param[in] style A string or NULL. Will be copied.
	 */
	OP_STATUS SetIMStyle(const uni_char* style);

	/**
	 * Gets the im style which is a string given by the webpage for instance
	 * and sent to the IME system.
	 *
	 * @returns A string previously set by SetIMStyle. The string is owned by the widget.
	 */
	const uni_char* GetIMStyle() { return im_style.CStr(); }
#endif // WIDGETS_IME_SUPPORT

	/** Return the necessary height to display all the current content */
	INT32 GetPreferedHeight();

	/** returns the height of a line in the text area */
	INT32 GetLineHeight() { return multi_edit->line_height; }
	/** changes the line height of the text area */
	void SetLineHeight(int line_height);

	/** Get visible lineheight. MAX of fontheight and lineheight */
	INT32 GetVisibleLineHeight();

	/**
	 changes the scrollbar status for the text area.
	 @param status if SCROLLBAR_STATUS_AUTO, the vertical scrollbar is always shown,
	 while the horizontal will be turned on and off as needed
	 if SCROLLBAR_STATUS_ON, both scrollbars will be shown whether they're needed or not
	 if SCROLLBAR_STATUS_OFF, no scrollbars will be shown
	 */
	void SetScrollbarStatus(WIDGET_SCROLLBAR_STATUS status);

	/**
	 changes the scrollbar status for the text area.
	 */
	void SetScrollbarStatus(WIDGET_SCROLLBAR_STATUS status_x, WIDGET_SCROLLBAR_STATUS status_y);

	/**
	 updates the scrollbars. this include recalculating the entire height of the contents and
	 deciding which scrollbars are to be shown.
	 Returns TRUE if it changed so a reformat of the content is needed.
	 */
	BOOL UpdateScrollbars(BOOL keep_vertical = FALSE);
	/**
	 returns the scroll value of the horizontal scrollbar
	 */
	INT32 GetXScroll();
	/**
	 returns the scroll value of the vertical scrollbar
	 */
	INT32 GetYScroll();

	/**
	 retrieves the read-only status for the text area
	 @return the read-only status for the text area. if TRUE, the text area is in read-only mode.
	 */
	BOOL IsReadOnly() { return m_packed.is_readonly; }
	/**
	 retrieves the wrapping status for the text area
	 @return the wrapping status for the text area. if TRUE, the text area is in wrap mode.
	 */
	BOOL IsWrapping() { return m_packed.is_wrapping; }
	/**
	 retrieves the flat-mode status for the text area
	 @return the flat-mode status for the text area. if TRUE, the text area is in flat mode.
	 */
	BOOL IsFlatMode() { return m_packed.flatmode; }
#ifdef WIDGETS_UNDO_REDO_SUPPORT
	/**
	 @return TRUE if there is something to undo
	 */
	BOOL IsRedoAvailable() { return multi_edit->undo_stack.CanRedo(); }
	/**
	 @return TRUE if there is something to redo
	 */
	BOOL IsUndoAvailable() { return multi_edit->undo_stack.CanUndo(); }
#endif
	/**
	 @return TRUE, if the text area contains no text
	 */
	BOOL IsEmpty();
	/**
	 @return FALSE, always. should be implemented if needed.
	 */
	BOOL IsAllSelected() { OP_ASSERT(!"FIXME: not implemented!"); return FALSE; }

	/**
	 * Returns if this object is used by code right now and can't be deleted.
	 * This is a workaround for the nested message loops caused by
	 * the Quick context menus. See OnContextMenu().
	 *
	 * When this returns TRUE, the widget must not be deleted.
	 */
	BOOL IsInUseByExternalCode()
	{
#ifdef QUICK
		return m_packed.is_in_use_by_external_code;
#else
		return FALSE;
#endif // QUICK
	}

	INT32 GetVisibleWidth() { return multi_edit->visible_width; }
	/**
	 retrieves the visible height of the text area
	 @return the visible height of the text area
	 */
	INT32 GetVisibleHeight() { return multi_edit->visible_height; }
	/**
	 retrieves the visible rect (in local document coords) for the text area, not including the border or scrollbar
	 */
	OpRect GetVisibleRect();

	/**
	 updates the font information in visual device to reflect what's in OpWidget::font_info
	 */
	void UpdateFont();

	/**
	 if SUPPORT_TEXT_DIRECTION is enabled and autodetect direction is on, text will be scanned
	 for text direction in EndChange
	 @param autodetect TRUE to enable text direction auto detection
	 */
	void SetAutodetectDirection(BOOL autodetect) { multi_edit->SetAutodetectDirection(autodetect); }

	/**
	 scroll the contents of the text area if needed, so that the caret is visible
	 @param include_document if TRUE the document will be scrolled as well
	 */
	void ScrollIfNeeded(BOOL include_document = FALSE, BOOL smooth_scroll = FALSE);
	/**
	 translates point from local document coords to coords relative to the entire text contents
	 @param point the point to translate
	 */
	OpPoint TranslatePoint(const OpPoint &point);

	/**
	 @param status if TRUE, text field will accept tabs as input rather than propagate them upwards
	 */
	void SetWantsTab(BOOL status) { m_packed.wants_tab = !!status; }

#ifdef COLOURED_MULTIEDIT_SUPPORT
	/** Enable/Disable syntax highlighting. You need FEATURE_COLOUR_MULTIEDIT. */
	void SetColoured(BOOL colour) { m_packed.coloured = !!colour; }
#endif

	// Implementing the OpTreeModelItem interface
	/** @return WIDGET_TYPE_MULTILINE_EDIT */
	virtual Type		GetType() {return WIDGET_TYPE_MULTILINE_EDIT;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_MULTILINE_EDIT || OpWidget::IsOfType(type); }

	/**
	 determines if the text area is scrollable along an axis
	 @param vertical TRUE if the vertical axis is to be used, FALSE for horizontal
	 @return TRUE if scrolling is possible along the desired axis
	 \note the return is TRUE if it is possible to scroll in atleast ONE direction along the given axis
	 */
	virtual BOOL IsScrollable(BOOL vertical);

	/**
	 determines if the contents at point is draggable
	 @param point the point from where dragging would be started
	 @return TRUE if the contents at point is draggable
	 */
	BOOL IsDraggable(const OpPoint& point);

	// == OpInputContext ======================

	/**
	 performs the given input action
	 @param action the action to be performed
	 */
	virtual BOOL		OnInputAction(OpInputAction* action);
	/**
	 @return "Edit Widget"
	 */
	virtual const char*	GetInputContextName() {return "Edit Widget";}

#ifdef DRAG_SUPPORT
	virtual void OnDragStart(const OpPoint& point);
	virtual void OnDragMove(OpDragObject* drag_object, const OpPoint& point);
	virtual void OnDragDrop(OpDragObject* drag_object, const OpPoint& point);
	virtual void OnDragLeave(OpDragObject* drag_object);
#endif
	virtual BOOL IsSelecting() { return is_selecting != 0; }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindMultilineTextedit;}
	virtual OP_STATUS		AccessibilitySetSelectedTextRange(int start, int end);
	virtual OP_STATUS		AccessibilityGetSelectedTextRange(int &start, int &end);
	virtual Accessibility::State AccessibilityGetState();
#endif

	/**
	 sets the caret position
	 @param pos the new caret position (top of caret), relative to the top-left point of the contents
	 \note the point is relative to the _content_, not the widget - scrolling is not taken into account
	 @return TRUE if successful, FALSE if no text block could be found at the requested caret position
	 */
	BOOL SetCaretPos(const OpPoint& pos) {return multi_edit->SetCaretPos(pos);}
	/**
	 @return the current caret position (top of caret), relative to the top-left point of the contents
	 \note the point is relative to the _content_, not the widget - scrolling is not taken into account
	 */
	OpPoint GetCaretPos() { return multi_edit->caret_pos; };
	/**
	 @return the offset from the start of the text to the current caret position
	 */
	INT32 GetCaretCharacterPos() { return multi_edit->caret.GetGlobalOffset(TCGetInfo()); }

#ifdef OP_KEY_CONTEXT_MENU_ENABLED
	/**
	 * Used to determine the location of the caret so that a context menu can be shown at its
	 * position.
	 *
	 * @returns Position relative to the upper left corner of the widget.
	 */
	OpPoint GetCaretCoordinates();
#endif // OP_KEY_CONTEXT_MENU_ENABLED

	/** @return TRUE if the caret is on the first line of text */
	BOOL CaretOnFirstInputLine() const;
	/** @return TRUE if the caret is on the last line of text */
	BOOL CaretOnLastInputLine() const;

	/** Returns the character offset from a position. rect is not used. */
	INT32 GetCharacterOfs(OpRect rect, OpPoint pos);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/**
	 * Returns 0 if spell session isn't possible.
	 *
	 * @param[in] p A point in the field which should be spell checked. If NULL (default) uses the caret
	 * position.
	 */
	int CreateSpellSessionId(OpPoint* p = NULL);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	// == OpTCListener ======================

	/**
	 called from OpTextCollection whenever the contents of the text collection changes. updates is_changed.
	 */
	virtual void TCOnContentChanging();
	/**
	 called from OpTextCollection whenever the total size of the text collection has changed. calls UpdateScrollbars.
	 */
	virtual void TCOnContentResized();
	/**
	 called from OpTextCollection whenever the text need to be invalidated. invalidates part of the text area.
	 @param rect the rect to be invalidated
	 */
	virtual void TCInvalidate(const OpRect& rect);
	/**
	 called from OpTextCollection when all text is selected. calls InvalidateAll.
	 */
	virtual void TCInvalidateAll();
	/**
	 a means for OpTextCollection to get information on the text area. fills and returns a pointer to an OP_TCINFO scruct.
	 @return a pointer to a filled OP_TCINFO struct
	 */
	virtual OP_TCINFO* TCGetInfo();
	/**
	 called when OpTextCollection determines the direction of the contents of the text area.
	 @param is_rtl TRUE if the contents is detected to be RTL
	 */
	virtual void TCOnRTLDetected(BOOL is_rtl);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/** Called when spellcheck has changed content. */
	virtual void TCOnTextCorrected();
#endif // INTERNAL_SPELLCHECK_SUPPORT

        uni_char* m_ghost_text;

	virtual OP_STATUS GetSelectionRects(OpVector<OpRect>* list, OpRect& union_rect, BOOL visible_part_only);
	virtual BOOL IsWithinSelection(int x, int y);

private:
	friend class OpTextCollection;
	friend class OpLabel;
#ifdef MULTILABEL_RICHTEXT_SUPPORT
	friend class OpRichtextEdit;
#endif

	void PrepareOffsetAndArea(OpPoint& offset, OpRect& area, BOOL visible_part_only);

	OP_STATUS SetTextInternal(const uni_char* text, BOOL use_undo_stack);

	OpPoint sel_start_point;

	INT32 caret_wanted_x; ///< Used when stepping from one row to another to keep it at the same x position.

	int caret_blink;
	int is_selecting; ///< 0 not selecting, 1 selecting, 2 word-by-word selecting
	VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type;
	OpTextCollection* multi_edit;
	OpScrollbar* x_scroll;
	OpScrollbar* y_scroll;
	OpWidgetResizeCorner *corner;
	UINT32 alt_charcode;
	int is_changing_properties;	///< counter for Begin/EndChangeProperties
	int needs_reformat;			///< If we need to reformat after EndChangeProperties. 1 for Reformat(FALSE) 2 for reformat(TRUE)
	WIDGET_SCROLLBAR_STATUS scrollbar_status_x;
	WIDGET_SCROLLBAR_STATUS scrollbar_status_y;
	void ReformatNeeded(BOOL update_fragment_list);
	OP_STATUS Reformat(BOOL update_fragment_list);

	void GetLeftTopOffset(int &x, int &y);

	/** Enable or disable the caret blinking timer */
	void EnableCaretBlinking(BOOL enable);

#ifdef USE_OP_CLIPBOARD
	/** Places the selected text in the clipboard. If remove is TRUE the selected text is removed afterwards (if possible). */
	void PlaceSelectionInClipboard(OpClipboard* clipboard, BOOL remove);
#endif // USE_OP_CLIPBOARD

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/** Spellchecking should be disabled if we're getting disabled */
	virtual void SetEnabled(BOOL enabled) { OpWidget::SetEnabled(enabled); DisableSpellcheckIfNotUsable(); }

	/** Spellchecking should be disabled if we're "dead" */
	virtual void SetDead(BOOL dead) { OpWidget::SetDead(dead); DisableSpellcheckIfNotUsable(); }

  public:
	void EnableSpellcheck();

	/** Disabled spellchecking... */
	void DisableSpellcheck(BOOL force=TRUE);

	BOOL SpellcheckByUser();
  private:
	/** Returns FALSE, if e.g. this widget is disabled. */
	void DisableSpellcheckIfNotUsable() { if(!CanUseSpellcheck()) DisableSpellcheck(TRUE /*force*/); }

	BOOL CanUseSpellcheck();
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WIDGETS_IME_SUPPORT
	// Input Methods
	INT32 im_pos;			///< The pos where the imstring is inserted.
	OpInputMethodString* imstring;
	IM_WIDGETINFO GetIMInfo();
	IM_COMPOSE im_compose_method;
	/**
	 * String defined by the web element that controls the ime implementation.
	 * Related to the HTML attribute istyle and the CSS property input-format-
	 */
	OpString im_style;
#endif // WIDGETS_IME_SUPPORT
private:

#ifdef IME_SEND_KEY_EVENTS
	int m_fake_keys;
	int previous_ime_len;
#endif // IME_SEND_KEY_EVENTS

	union
	{
		struct
		{
			bool ghost_mode:1;
			bool flatmode:1;
			bool is_wrapping:1;
			bool is_readonly:1;
			bool determine_select_direction:1;
			bool is_changed:1;
			bool wants_tab:1;
			bool is_label_mode:1;
			bool show_drag_caret:1; ///< TRUE if the caret should be shown because something is being dragged
			bool line_height_set:1;
			bool is_overstrike:1;
#ifdef COLOURED_MULTIEDIT_SUPPORT
			bool coloured:1;
#endif
#ifdef QUICK
			/**
			 * See IsInUseByExternalCode()
			 */
			bool is_in_use_by_external_code:1;
#endif // QUICK
#ifdef INTERNAL_SPELLCHECK_SUPPORT
			bool never_had_focus:1;
			bool enable_spellcheck_later:1;
#endif // INTERNAL_SPELLCHECK_SUPPORT
#ifdef WIDGETS_IME_SUPPORT
			// Input Methods
			bool im_is_composing:1;
#endif
			bool focus_comes_from_scrollbar:1;
			bool fragments_need_update:1;
		} m_packed;
		unsigned m_packed_init;
	};
};

#ifdef MULTILABEL_RICHTEXT_SUPPORT
class OpRichtextEdit : public OpMultilineEdit
					  ,public XMLTokenHandler
{

protected:
		OpRichtextEdit();
		~OpRichtextEdit();

public:
	/**
	 creates a text area widget
	 @param obj (out) a handle to the created object
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on error
	 */
	static OP_STATUS Construct(OpRichtextEdit** obj);

	/** Inherited from OpMultilineEdit and changed */
	virtual OP_TCINFO* TCGetInfo();
	virtual void OutputText(UINT32 color);

	virtual OP_STATUS		SetText(const uni_char* text);

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	virtual OP_STATUS		SetTextWithUndo(const uni_char* text){ return OpStatus::ERR_NOT_SUPPORTED; }
#endif

#ifndef MOUSELESS
	/**
	 This is used in RichTextMultiEdit for changing a cursor over links
	 */
	virtual void OnSetCursor(const OpPoint &point);
#endif // !MOUSELESS

	// Methods used to parse given input string.
	virtual XMLTokenHandler::Result HandleToken(XMLToken &token);
	virtual void ParsingStopped(XMLParser *parser);

protected:

	class StringAttribute
	{
		public:
			enum Type {
					TYPE_UNKNOWN,
					TYPE_BOLD,
					TYPE_ITALIC,
					TYPE_UNDERLINE,
					TYPE_HEADLINE,
					TYPE_LINK
				};
				Type     attribute_type;
				int		 range_start;
				int		 range_length;
				OpString link_value;
	};

	/*
	 * Set style on given text.
	 * @param start     Start index of setting style
	 * @param end       End index of setting style
	 * @param style     Style
	 * @param param     Alternative style parameter in text format
	 * @param param_len Length of param parameter
	 * @return OpStatus::OK on success, appropriate error message if style cannot be applied
	 */
	OP_STATUS SetTextStyle(UINT32 start, UINT32 end, UINT32 style, const uni_char* param = NULL, int param_len = 0);

	/*
	 * Returns origin positions of all links in the widget with minimal size and width
	 * that is needed to treat those as clickable buttons. That is needed by watir
	 * for proper automated testing of this widget.
	 * Note: caller is responsible for releasing memory allocated for rects added to vector
	 */
	OP_STATUS GetLinkPositions(OpVector<OpRect>& rects);
	void ClearAllStyles();

	void SetLinkColor(UINT32 col) { m_link_col = col; };

	OP_TCSTYLE**				m_styles_array;
	UINT32					m_styles_array_size;
	OpString					m_current_string_value;
	OpString					m_raw_text_input;
	OpVector<StringAttribute>	m_attribute_list;
	OpVector<StringAttribute>	m_attribute_stack;
	OpVector<OpString>		m_links;

	uni_char*	m_pointed_link;
	UINT32		m_link_col;
};
#endif // MULTILABEL_RICHTEXT_SUPPORT

#ifdef WIDGETS_IME_SUPPORT

/** Get the new pos as if the string had \r\n newlines (any \n newline will be counted as \r\n) */
int offset_half_newlines(const uni_char *str, int pos);

/** Get string length as if the string had \r\n newlines (any \n newline will be counted as \r\n) */
int strlen_offset_half_newlines(const uni_char *str);

#endif

#endif // OP_MULTIEDIT_H
