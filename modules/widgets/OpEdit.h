/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_EDIT_H
#define OP_EDIT_H

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/UndoRedoStack.h"
#include "modules/widgets/AutoCompletePopup.h"
#include "modules/widgets/OpResizeCorner.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/img/image.h"
#include "modules/display/vis_dev.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT
class OpEditSpellchecker;
#endif // INTERNAL_SPELLCHECK_SUPPORT

// == OpEdit ==================================================

class OpEdit : public OpWidget
#ifdef QUICK
	, public OpDelayedTriggerListener
#endif
{
private:
	/** Second stage constructor. */
	OP_STATUS Init();
protected:
	OpEdit();
	virtual void OnScaleChanged();

public:
	/**
	 creates an OpEdit object
	 @param obj (out) a handle to the created object
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on error
	 */
	static OP_STATUS Construct(OpEdit** obj);

#ifdef WIDGETS_CLONE_SUPPORT
	virtual OP_STATUS CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded);
	virtual BOOL IsCloneable() { return TRUE; }
#endif

	/**
	 destructor. free:s allocated data.
	 */
	virtual ~OpEdit();

	/**
	 returns the part of the OpEdit (in local document coords) that contains text
	 */
	OpRect GetTextRect();

	/** returns the extra indentation the text should have as a result of any internal icon or similar stuff. */
	int GetTextIndentation();

#ifdef WIDGETS_IME_SUPPORT
	/**
	   documented in OpWidget
	 */
	virtual BOOL IMEHandleFocusEventInt(BOOL focus, FOCUS_REASON reason);
#endif // WIDGETS_IME_SUPPORT

	/**
	 tells the edit field's string to update any cached data and calculates the
	 prefered size
	 @param w (out) the prefered width
	 @param h (out) the perefered height
	 @param cols unused
	 @param rows unused
	 */
	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	/**
	 gets the length of the current text
	 @return the length of the current text
	 */
	INT32 GetTextLength();
	/**
	 gets text from the edit field
	 @param buf (out) the target buffer
	 @param buflen the length of the buffer, _minus the null termination_.
	 buflen must always be (at least) one less than the actual size of the buffer!
	 @param offset offset from the start of the source text
	 */
	void GetText(uni_char *buf, INT32 buflen, INT32 offset);
	/**
	 gets the entire text from the edit field
	 @param str (out) the target string (will be cleared)
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS GetText(OpString &str);
	/**
	 if text, inserts text at the current caret pos - any selection will be deleted before insertion.
	 if text is NULL, does nothing.
	 @param text the text to be inserted
	 @return OpStatus::OK, always. memory error is reported through OpWidget::ReportOOM
	 */
	OP_STATUS InsertText(const uni_char* text);

	/**
	 replaces any text in the edit field with text.
	 @param text the new text
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS SetText(const uni_char* text) { return SetTextInternal(text, FALSE, FALSE); }

	/**
	 replaces any text in the edit field with text.
	 @param text the new text
	 @param force_no_onchange if TRUE, no OnChange message will be sent
	 */
	OP_STATUS SetText(const uni_char* text, BOOL force_no_onchange) { return SetTextInternal(text, force_no_onchange, FALSE); }

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
	virtual OP_STATUS SetTextWithUndo(const uni_char* text) { return SetTextInternal(text, FALSE, TRUE); }

	/**
	 replaces any text in the edit field with text.
	 @param text the new text
	 @param force_no_onchange if TRUE, no OnChange message will be sent
	 */
	OP_STATUS SetTextWithUndo(const uni_char* text, BOOL force_no_onchange) { return SetTextInternal(text, force_no_onchange, TRUE); }
#endif

	/**
	 changes the read-only status for the edit field. in read-only mode the text cannot be changed.
	 @param readonly the new read-only status for the edit field
	 */
	void SetReadOnly(BOOL readonly);
	/**
	Allows to clear the field even if read-only.
	@param allow_clear TRUE if edit can be cleared even though it's read-only
	*/
	void SetForceAllowClear(BOOL allow_clear) { m_packed.force_allow_clear = allow_clear; }

	/**
	 sets the max allwed string length for the edit field
	 @param length maximum string length
	 */
	void SetMaxLength(INT32 length);
	/**
	 sets whether the OpEdit is in pasword mode or not. in password mode, *:s are drawn instead of text.
	 @param password_mode the new password mode status for the edit field
	 */
	void SetPasswordMode(BOOL password_mode);
	/**
	 gets whether the OpEdit is in pasword mode or not.
	 @return password mode for the edit field
	 */
	BOOL GetPasswordMode(){return string.GetPasswordMode();}
	/**
	 sets flat mode. when in flat mode, the text is not editable and no edges are drawn.
	 */
	void SetFlatMode();
	/**
	 if called, default auto-completion is turned off.
	 */
	void SetUseCustomAutocompletion() { m_packed.use_default_autocompletion = FALSE; }

	/**
	 Returns TRUE if the last user action was a insert of text.
	 Typically inputting text by typing. Not pasting or undo/redo.
	 It is valid during the OnChange callback.
	 */
	BOOL IsLastUserChangeAnInsert() { return m_packed.is_last_user_change_an_insert; }

	/**
	 Returns TRUE in case IME input is ongoing
	 */
	BOOL IsComposing() { return m_packed.im_is_composing; }

	/**
	 * Set one or more text decoration properties on the string. Use 0
	 * to clear the property.
	 *
	 * @param text_decoration One or more of WIDGET_TEXT_DECORATION types
	 * @param replace If TRUE, the current value is fully replaced, if FALSE
	 *        the new value is added to the current value
	 */
	void SetTextDecoration(INT32 text_decoration, BOOL replace) { string.SetTextDecoration(text_decoration, replace); }

	/** Depricated! Use SetCaretOffset! */
	void SetCaretPos(INT32 pos) { SetCaretOffset(pos); }
	/** Depricated! Use GetCaretOffset! */
	INT32 GetCaretPos() { return caret_pos; }

#ifdef OP_KEY_CONTEXT_MENU_ENABLED
	/**
	 * Used to determine the location of the caret so that a context menu can be shown at its
	 * position.
	 *
	 * @returns Position relative to the upper left corner of the widget.
	 */
	OpPoint GetCaretCoordinates();
#endif // OP_KEY_CONTEXT_MENU_ENABLED

	/**
	 moves the caret to the start or end position, depending on start.
	 @param start decides whether the caret is to be moved to the start or end
	 @param visual_order if TRUE, end is always to the right, regardless of text direction
	 */
	void MoveCaretToStartOrEnd(BOOL start, BOOL visual_order);
	/**
	 moves the caret one step forward or backward, depending on forward.
	 @param forward decides whether the caret is to be moved forward or backward
	 @param visual_order if TRUE, forward is always to the right, regardless of text direction
	 */
	void MoveCaret(BOOL forward, BOOL visual_order);

private:
	/**
	 Calls GenerateHighlightRectChanged (in OpWidget) to notify the system that
	 the caret position has changed since the last time it was reported.
	 */
	void ReportCaretPosition();

public:

	/**
	 selects all text in the edit field. does not call OnSelect
	 */
	void SelectText();
	/**
	 gets the interval of text currenly selected. returns 0,0 if nothing selected.
	 @param start_ofs (out) the start position (in _logical_ order) of the selection
	 @param stop_ofs (out) the stop position (in _logical_ order) of the selection
	 @param direction (out) the direction of the selection
	 @param line_normalized flag controlling interpretation of line terminator (unused.)
	 */
	virtual void GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction, BOOL line_normalized = FALSE);
	/**
	 sets the current selection of the text in the edit field
	 @param start_ofs the start position (in _logical_ order) of the selection
	 @param stop_ofs the stop position (in _logical_ order) of the selection
	 @param direction the direction of the selection
	 @param line_normalized flag controlling interpretation of line terminator (unused.)
	 */
	virtual void SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction = SELECTION_DIRECTION_DEFAULT, BOOL line_normalized = FALSE);
	// overrides OpWidget::SelectSearchHit
	virtual void SelectSearchHit(INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit);
	// overrides OpWidget::IsSearchHitSelected
	BOOL IsSearchHitSelected();
	// overrides OpWidget::IsActiveSearchHitSelected
	BOOL IsActiveSearchHitSelected();

	/**
	 returns the current character position
	 @return the current character position (in _logical_ order)
	 */
	virtual INT32 GetCaretOffset();
	/**
	 moves the caret to the requested position
	 @param caret_ofs the new caret pos (in _logical_ order)
	 if less than zero, caret is moved to the start of the string
	 if greater than string length, caret is moved to end of string
	 */
	void SetCaretOffset(INT32 caret_ofs) { SetCaretOffset(caret_ofs, TRUE); }
	void SetCaretOffset(INT32 caret_ofs, BOOL reset_snap_forward);

	/**
	 determins whether point lies inside the bounding box of the currently selected text
	 @param point the point (in local document coords)
	 @return TRUE if there is selected text, the point falls inside it,
	 the edit field is not in password mode and is editable
	 FALSE otherwise
	 */
	BOOL IsDraggable(const OpPoint& point);

	/**
	 gets the ghost text from the edit field
	 @param str (out) the target string (will be cleared)
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS GetGhostText(OpString &str) { return str.Set(ghost_string.Get()); }

	/**
	 sets the ghost text the ghost_text. a ghost text is a text that is only visible when the edit field
	 contains no text and is not focused.
	 @param ghost_text the new ghost text
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY om OOM
	 */
	OP_STATUS SetGhostText(const uni_char* ghost_text);

	/** 
	 provides option of displaying the ghost string in the textfield when it receives focus.
	 @param show if sets to TRUE the ghost-string will be displayed regardless of focus in the textfield
	*/
	void SetShowGhostTextWhenFocused(BOOL show);
	BOOL GetShowGhostTextWhenFocused() const { return m_packed.show_ghost_when_focused; }

	/** 
	 enables to set alignment for the ghost string in the textfield when it receives focus.
	 @param justify identifies the justification of ghost-string when the textfield receives focus.
	 Applicable justifications are JUSTIFY_LEFT and JUSTIFY_RIGHT
	*/
	void SetGhostTextJusifyWhenFocused(JUSTIFY justify = JUSTIFY_RIGHT);
	
	/** 
	 enables to set foreground color for the ghost string when textfield receives focus.
	 @param fcol identifies foreground color for the ghost string
	*/
	void SetGhostTextFGColorWhenFocused(UINT32 fcol) {  m_ghost_text_fcol_when_focused = fcol; }
	UINT32 GetGhostTextFGColorWhenFocused() { return m_ghost_text_fcol_when_focused; }

	/**
	 determines if double clicking the edit field should select all or one word's worth of text
	 @param status if TRUE, future double clicks will select all text
	 if FALSE, future double clicks will select one word's worth of text
	 */
	void SetDoubleClickSelectsAll(BOOL status) { m_packed.dblclick_selects_all = !!status; }

#ifndef HAS_NO_SEARCHTEXT
	/**
	 search the edit field for occurences of txt
	 @param txt the text to search for
	 @param len the length of the search string
	 @param forward if TRUE, search forward, else search backwawrds
	 @param match_case if TRUE, case is taken into consideration when looking for matches
	 @param words if TRUE, match only whole words
	 @param type if SEARCH_FROM_CARET, searching is commenced from the current caret pos
	 if SEARCH_FROM_BEGINING search is commenced from the beginning of the text
	 if SEARCH_FROM_END search is commenced from the end of the text
	 @param select_match if TRUE, any found match will be selected
	 @param scroll_to_selected_match if TRUE, text and document scrolling to any found match will be performed
	 @return TRUE if a match was found, FALSE otherwise
	 */
	BOOL SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words, SEARCH_TYPE type, BOOL select_match, BOOL scroll_to_selected_match, BOOL wrap, BOOL next);
#endif // !HAS_NO_SEARCHTEXT
	/**
	 determines if the edit field has selected text
	 @return TRUE, if one or more characters of the text is selected, FALSE otherwise
	 */
	BOOL HasSelectedText();
	/**
	 deselects all text
	 */
	void SelectNothing();

	/**
	 performes the action
	 @param action the action to perform
	 */
	void EditAction(OpInputAction* action);
	/**
	 removes any selected text and puts it on the clipboard. can fail on OOM, but will report it.
	 */
	void Cut();
	/**
	 copies the selected text to the clipboard, or to note, if to_note is TRUE. can fail on OOM, but will report it.
	 @param to_note if TRUE, the selection is copied as an opera note and not to clipboard
	 */
	void Copy(BOOL to_note = FALSE);

#if defined(SELFTEST) && defined(USE_OP_CLIPBOARD)
	// to be used from selftests only.
	// paste any selection into the widget
	OP_STATUS SelftestPaste()
	{
		m_autocomplete_after_paste = FALSE;
		const OP_STATUS s = g_clipboard_manager->Paste(this);
		m_autocomplete_after_paste = TRUE;
		return s;
	}
#endif // SELFTEST && USE_OP_CLIPBOARD

#ifdef USE_OP_CLIPBOARD
	// ClipboardListener API
	void OnCut(OpClipboard* clipboard);
	void OnCopy(OpClipboard* clipboard);
	void OnPaste(OpClipboard* clipboard);
#endif // USE_OP_CLIPBOARD
#ifdef WIDGETS_UNDO_REDO_SUPPORT
	/**
	 undos the last input action performed on the input field
	 */
	void Undo(BOOL ime_undo = FALSE);
	/**
	 redos the last undo action performed on the input field
	 */
	void Redo();
#endif
	/**
	 selects all text in the edit field, and calls OnSelect on any listener
	 */
	void SelectAll();
	/**
	 removes all text in the edit field. can run out of memory, but will report it.
	 */
	void Clear();
	/** Calls OnChange when enter is pressed instead of every char (for UI). */
	void SetOnChangeOnEnter(BOOL status) { m_packed.onchange_on_enter = !!status; }

	// State
	/**
	 determines if the edit field is in read-only mode or not
	 @return TRUE if the edit field is in read-only mode, FALSE otherwise
	 */
	BOOL IsReadOnly() { return m_packed.is_readonly; }

	/**
	Determines if a field can be cleared even if it's read-only.
	@return TRUE if the field can be cleared even if it's read-only
	*/
	BOOL IsForceAllowClear() { return m_packed.force_allow_clear; }

#ifdef WIDGETS_UNDO_REDO_SUPPORT
private:
	BOOL IsUndoRedoEnabled() { return m_pattern.IsEmpty(); }
public:
	/**
	 determines if a redo is possible
	 @return TRUE if there is something to redo, FALSE otherwise
	 */
	BOOL IsRedoAvailable() const { return undo_stack.CanRedo(); }
	/**
	 determines if an undo is possible
	 @return TRUE if there is something to undo, FALSE otherwise
	 */
	BOOL IsUndoAvailable() const { return undo_stack.CanUndo(); }
#endif // WIDGETS_UNDO_REDO_SUPPORT
	/**
	 determines if the edit field is empty
	 @return TRUE if the edit field contains no text, FALSE otherwise
	 */
	BOOL IsEmpty() { return GetTextLength() == 0; }
	/**
	 determines if all text is selected
	 @return TRUE if all text is selected, FALSE otherwise
	 */
	BOOL IsAllSelected();

	/** Return TRUE if the selection start is visually before stop. */
	BOOL IsSelectionStartVisuallyBeforeStop();

	/**
	 causes the contents (icon if present, ghost string or text, and caret) of the edit field
	 to be painted onto the visual device
	 @param inner_rect the bounds within which to paint
	 @param fcol the text color
	 @param draw_ghost if TRUE, the gost string is drawn, otherwise the edit field's string is drawn
	 */
	void PaintContent(OpRect inner_rect, UINT32 fcol, BOOL draw_ghost);

	// == Hooks ======================
	/**
	 called when the edit field is deleted
	 */
	virtual void OnDeleted();
	virtual void OnRemoving();
	/**
	 called when the edit field is to be painted. this method should paint the widget.
	 @param widget_painter the painter object
	 @param paint_rect the rect in witch to paint
	 */
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	/**
	 called when the timer times out, and makes the caret blink
	 */
	virtual void OnTimer();
	/**
	 called when the edit field gains or looses focus.
	 if focus is gained, the caret is shown, and a timer started to make it blink.
	 depending on the focus reason, all text might be selected.
	 if IME support is enabled, SetInputMethodMode will be called if IME is not spawning.
	 @param focus if TRUE, the edit field gained focus, if FALSE it lost it
	 @param reason the reason the widget got focused
	 */
	virtual void OnFocus(BOOL focus,FOCUS_REASON reason);
	/**
	 called on resize. will scroll text if needed, to make sure caret is visible
	 */
	virtual void OnResize(INT32* new_w, INT32* new_h);
	/**
	 if IME support is enabled, will call SetInputMoved()
	 */
	virtual void OnMove();

	/**
	 Update the state of the resize corner.
	*/
	void UpdateCorner();

	/**
	Called when widget becomes resizable, or when the widget is no longer resizable.

	Will show/hide resize handle.
	*/
	virtual void OnResizabilityChanged();
#ifndef MOUSELESS
	/**
	 will try to spawn a context menu via listener. if listener doesn't spawn a context menu
	 quick gets a go if enabled.
	 @return TRUE, always
	 */
	virtual BOOL OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);
	/**
	 called when the edit field receives a mouse down.
	 will trigger mouse event to any listener.
	 depending on which mouse button was pushed down and the number of clicks,
	 different things might happen.
	 whatever button was pushed down the caret blinking is (re)started
	 if the left mouse button was pushed down and more than one click was received a word, all text
	 or nothing is selected depending on the number of clicks and the status of dblclick_selects_all.
	 if shift is held down and the left mouse button was spushed down, the text between the current
	 caret pos and the clicked position will be selected.

	 @param point the mouse position (in local document coords)
	 @param button the mouse button that was pushed down
	 @param nclicks the number of clicks
	 */
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	/**
	 called when the edit field receives a mouse up.
	 will trigger mouse event to any listener.
	 if mouse up fell inside edit field, will trigger OnClick to any listener.
	 if left mouse button was released and resulted in a selection, OnSelect will be triggered to any listener.
	 if left mouse button was released and didn't result in a selection, any selected text will be deselected
	 and the caret pos will be updated to the point of the mouse up.
	 
	 @param point the mouse position (in local document coords)
	 @param button the mouse button that was released
	 @param nclicks the number of clicks
	 */
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	/**
	 called when the edit field receives a mouse move.
	 if edit field received focus due to a mouse down and has yet to receive the corresponding mouse up
	 and the difference in mouse position is big enough, text selection will be started.
	 if selection is started, the mouse move will update the selected portion of text. depending on the number
	 of mouse downs before selection was started, words or characters will be selected. the text will be scrolled
	 when the mouse reaches one of the vertical edges of the edit field.

	 @param point the current mouse position (in local document coords)
	 */
	virtual void OnMouseMove(const OpPoint &point);
	/**
	 called when the mouse is moved over the widget. if the mouse is over the inner part of the edit field,
	 the mouse cursor will be changed to a text cursor.
	 */
	virtual void OnSetCursor(const OpPoint &point);
#endif // !MOUSELESS

#ifdef WIDGETS_IME_SUPPORT
	/**
	 documented in the pi moudle
	 */
	IM_WIDGETINFO OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose);
	/**
	 documented in the pi moudle
	 */
	IM_WIDGETINFO OnCompose();
	/**
	 documented in the pi moudle
	 */
	IM_WIDGETINFO GetWidgetInfo();
	/**
	 documented in the pi moudle
	 */
	void OnCommitResult();
	/**
	 documented in the pi moudle
	 */
	void OnStopComposing(BOOL cancel);
	/**
	 documented in the pi moudle
	 */
	virtual void OnCandidateShow(BOOL visible);

#ifdef IME_RECONVERT_SUPPORT
	/**
	documented in the pi moudle
	*/
	virtual void OnPrepareReconvert(const uni_char*& str, int& sel_start, int& sel_stop);
	/**
	documented in the pi moudle
	*/
	virtual void OnReconvertRange(int sel_start, int sel_stop);
#endif
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

	// Implementing the OpTreeModelItem interface
	/**
	 gets the type of the widget
	 @return WIDGET_TYPE_EDIT
	 */
	virtual Type		GetType() {return WIDGET_TYPE_EDIT;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_EDIT || OpWidget::IsOfType(type); }

	// == OpInputContext ======================

	/**
	 called when widget receives an input action. what happens depend on the input action.
	 */
	virtual BOOL		OnInputAction(OpInputAction* action);
	/**
	 returns the name of the widget type
	 @return "Edit Widget"
	 */
	virtual const char*	GetInputContextName() {return "Edit Widget";}

	/**
	 invokes the currenly set action, if any. if no action is set, calls BroadcastOnChange(FALSE,TRUE)
	 @return TRUE if an action was invoked, FALSE otherwise
	 */
	virtual BOOL		InvokeAction();

#ifdef QUICK
	BOOL HasToolTipText(OpToolTip* tooltip)
	{
		// Check for the debug tooltip first
		if (IsDebugToolTipActive())
			return TRUE;
		return !m_tooltip_text.IsEmpty();
	}

	void GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
	{
		// Check for the debug tooltip first
		if (IsDebugToolTipActive())
			OpWidget::GetToolTipText(tooltip, text);
		else
			text.SetTooltipText( m_tooltip_text.CStr() );
	}
	
	void SetToolTipText(const OpString& text)
	{
		m_tooltip_text.Set(text);
	}
	
	// Hacky functions to make marquee work for OpLabel in quick
	int		GetXScroll() { return x_scroll; }
	void	SetXScroll(int scroll) { x_scroll = scroll; }
	int		GetStringWidth() { return string.GetWidth(); }
#endif

	/**
	 called when font is changed. tells string and ghost string it needs to be updated.
	 */
	virtual void		OnFontChanged() {string.NeedUpdate(); ghost_string.NeedUpdate();}

	virtual void		OnDirectionChanged() { OnFontChanged(); }

#ifdef DRAG_SUPPORT
	virtual void OnDragStart(const OpPoint& point);
	virtual void OnDragMove(OpDragObject* drag_object, const OpPoint& point);
	virtual void OnDragDrop(OpDragObject* drag_object, const OpPoint& point);
	virtual void OnDragLeave(OpDragObject* drag_object);
#endif // DRAG_SUPPORT
	virtual BOOL IsSelecting() { return is_selecting != 0; }

#ifdef QUICK
	virtual	BOOL			GetLayout(OpWidgetLayout& layout);

	virtual	OpRect			LayoutToAvailableRect(const OpRect& rect);
	virtual void SetAction(OpInputAction* action);

	OpDelayedTrigger	m_delayed_trigger;
	void				SetOnChangeDelay(INT32 onchange_delay) {m_delayed_trigger.SetTriggerDelay(onchange_delay);}
	INT32				GetOnChangeDelay() {return m_delayed_trigger.GetInitialDelay();}
	virtual void		OnTrigger(OpDelayedTrigger*) {BroadcastOnChange(FALSE, TRUE);}
#endif

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindSinglelineTextedit;}
	virtual OP_STATUS		AccessibilitySetSelectedTextRange(int start, int end);
	virtual OP_STATUS		AccessibilityGetSelectedTextRange(int &start, int &end);
	virtual OP_STATUS		AccessibilityGetDescription(OpString& str);
	virtual Accessibility::State AccessibilityGetState();
#endif

	BOOL GetFlatMode() const{ return m_packed.flatmode; }
	void SetFlatMode(BOOL b){ m_packed.flatmode = !!b; }

	BOOL GetReplaceAllOnDrop() const{ return m_packed.replace_all_on_drop; }
	void SetReplaceAllOnDrop(BOOL b){ m_packed.replace_all_on_drop = !!b; }

	/** @return whether the text direction is forced to be LTR */
	BOOL GetForceTextLTR() const { return string.GetForceLTR(); }
	/** Force the text to be LTR even if the OpEdit is RTL. */
	void SetForceTextLTR(BOOL b) { string.SetForceLTR(b); }

#if defined(WIDGETS_HEURISTIC_LANG_DETECTION) && defined(SELFTEST)
	WritingSystem::Script GetHeuristicScript() { return string.m_heuristic_script; }
# endif // WIDGETS_HEURISTIC_LANG_DETECTION && SELFTEST


	virtual OP_STATUS GetSelectionRects(OpVector<OpRect>* list, OpRect& union_rect, BOOL visible_part_only)
	{
		return string.GetSelectionRects(GetVisualDevice(), list, union_rect, x_scroll, GetTextRect(), visible_part_only);
	}

	virtual BOOL IsWithinSelection(int x, int y);

private:
	OP_STATUS SetTextInternal(const uni_char* text, BOOL force_no_onchange, BOOL use_undo_stack);

public:
	int caret_pos; ///< Which character the caret is on
	int caret_blink; ///< if true, temporaliy sets caret pos to -1 (not visible) while drawing. the value is toggled on timer.
	int selecting_start; ///< holds the caret position of the start of a selection
	int is_selecting; ///< 0 not selecting, 1 selecting, 2 word-by-word selecting
#ifdef WIDGETS_UNDO_REDO_SUPPORT
	UndoRedoStack undo_stack; ///< the undo-redo stack
#endif
	UINT32 alt_charcode; ///< holds the numeric value of a sequence of digits inputted while alt is held down

	/**
	 trigger OnChange to any listener
	 @param changed_by_mouse TRUE if mouse caused the change
	 @param force_now if TRUE, OnChange will be called directly
    */
	void BroadcastOnChange(BOOL changed_by_mouse = FALSE, BOOL force_now = FALSE, BOOL changed_by_keyboard = FALSE);

#ifdef WIDGETS_UP_DOWN_MOVES_TO_START_END
	/** Set if the TWEAK_WIDGETS_UP_DOWN_MOVES_TO_START_END feature should ignore go to start in this widget. */
	void SetIgnoreGotoLineStart(BOOL ignore_go_to_line_start) { m_packed.ignore_go_to_line_start = ignore_go_to_line_start; }
#endif // WIDGETS_UP_DOWN_MOVES_TO_START_END

	/**
	 a string containing the set of characters allowed to be inserted. if m_allowed_chars is non-empty everyting from the
	 first charater in a string to be inserted that is not in this set will be discarded.
	 */
	OpString8 m_allowed_chars;
	/**
	 sets the set of characters that are accepted as input
	 @return OpStatus::OK if all is well, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS SetAllowedChars(const char* new_allowed_chars) { return m_allowed_chars.Set(new_allowed_chars); };
	/**
	 sets the text in the edit field to text, and calls OnChange on any listener.
	 \note text passed to SetTextAndFireOnChange will not be checked against m_allowed_chars
	 */
	OP_STATUS SetTextAndFireOnChange(const uni_char* text);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/** Spellchecking should be disabled if we're getting disabled */
	virtual void SetEnabled(BOOL enabled) { OpWidget::SetEnabled(enabled); DisableSpellcheckIfNotUsable(); }
	
	/** Spellchecking should be disabled if we're "dead" */
	virtual void SetDead(BOOL dead) { OpWidget::SetDead(dead); DisableSpellcheckIfNotUsable(); }
	
	/** Called by m_spellchecker when spellchecking is being disabled */
	void OnSpellcheckDisabled();
	
	/**
	 * Checks whether the spellchecker has been put in its current state by the user.
	 */
	BOOL SpellcheckByUser();

	/**
	 * Enables spellchecking
	 *
	 * The spellchecker is only enabled if it hasn't been explicitly disabled by the user.
	 */
	void EnableSpellcheck();

	/**
	 * Disable spellchecking.
	 *
	 * Unless force is TRUE the spellchecker will only be disabled if it wasn't originally enabled by the user (e.g. by a script).
	 *
	 * @param force Disable spellchecking even if it was enabled by the user.
	 */
	void DisableSpellcheck(BOOL force);
	
	void SetDelaySpelling(BOOL delay) { m_packed.delayed_spell = FALSE; }
	BOOL GetDelayedSpelling() const { return m_packed.delayed_spell; }
	
	/** Returns FALSE, if e.g. this widget is disabled. */
	BOOL CanUseSpellcheck();
	void DisableSpellcheckIfNotUsable() { if(!CanUseSpellcheck()) DisableSpellcheck(TRUE /*force*/); }

	OpEditSpellchecker *m_spellchecker; ///< Handles spellchecking for this OpEdit

	/**
	 * Returns 0 if spell session isn't possible.
	 *
	 * @param[in] p A point in the field which should be spell checked. If NULL (default) uses the caret
	 * position.
	 */
	int CreateSpellSessionId(OpPoint* p = NULL);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	void SetThisClickSelectedAll(BOOL b) { m_packed.this_click_selected_all = !!b; }
	BOOL GetThisClickSelectedAll() const { return m_packed.this_click_selected_all; }

	int x_scroll; ///< the scrolling of the text in the edit field (local document coords)
	OpWidgetString string; ///< the text shown in the edit field
	OpWidgetString ghost_string; ///< the ghost string is shown when the edit field has no content and is not focused


#ifdef WIDGETS_IME_SUPPORT
	int im_pos;			///< The pos where the imstring is inserted.
	OpInputMethodString* imstring; ///< Only valid when input method composing is active. Isn't own by this.
	IM_WIDGETINFO GetIMInfo(); ///< fills and returns an IM_WIDGETINFO struct. see OpInputMethod.
	IM_COMPOSE im_compose_method; ///< the compose method of the IME. see OpInputMethod.
#ifdef SUPPORT_IME_PASSWORD_INPUT
	/**
	 if SUPPORT_IME_PASSWORD_INPUT is set, the last inputted character will be briefly shown as-is when the edit
	 field is in password mode. after two caret blinks, a caret move, a click or a focus event the character will
	 be displayed as is normally done in password fields (i.e. replaced with an asterisk etc). this is the counter
	 for the number of caret blinks left until the character is to be hidden.
	 */
    int hidePasswordCounter;
#endif //SUPPORT_IME_PASSWORD_INPUT
	/**
	 * String defined by the web element that controls the ime implementation.
	 * Related to the HTML attribute istyle and the CSS property input-format-
	 */
	OpString im_style;
#endif // WIDGETS_IME_SUPPORT

	AutoCompletePopup autocomp; ///< the autocompletion object. see AutoCompletePopup.

	/**
	 inserts instr into the edit field at the caret pos. any selection will be deleted before insertion.
	 @param instr the text to be inserted
	 @param no_append_undo if TRUE, single characters inserted into the edit field will not be appended to
	 the same undo event as a stream of previously inserted characters. this is needed for IMEs.
	 */
	void InternalInsertText(const uni_char* instr, BOOL no_append_undo = FALSE);
	/**
	 scrolls the text in the edit field so that the caret is visible if it currently isn't
	 @param include_document if TRUE, also scrolls document if needed
	 */
	void ScrollIfNeeded(BOOL include_document = FALSE, BOOL smooth_scroll = FALSE);
	/**
	 scrolls the text in the edit field so that the start of the text is visible
	 */
	void ResetScroll();
	/**
	 invalidates the part of the edit field where the input caret is currently located
	 */
	void InvalidateCaret();

	BOOL IsPatternWriteable(int pos);
	OP_STATUS SetPattern(const uni_char* new_pattern);

private:
	JUSTIFY m_ghost_text_onfocus_justify;
	UINT32 m_ghost_text_fcol_when_focused;

#ifdef QUICK
	OpString m_tooltip_text;
#endif

	VD_TEXT_HIGHLIGHT_TYPE m_selection_highlight_type;

#ifdef IME_SEND_KEY_EVENTS
	int m_fake_keys;
	int previous_ime_len;
#endif // IME_SEND_KEY_EVENTS

protected:
	union
	{
		struct
		{
			bool caret_snap_forward:1;
			bool is_changed:1; ///< Is changed since the user focused the widget
			bool is_readonly:1; ///< TRUE if the widget is read-only
			/**
			 makes a selection made by mouse always expand when afterwards selecting with keyboard.
			 this variable is only used when TWEAK_RANGESELECT_FROM_EDGE is on.
			 */
			bool determine_select_direction:1;
			bool flatmode:1; ///< true when widget is in flat mode. when in flat mode, text is not editable and no edges are drawn
			bool onchange_on_enter:1; ///< if true, OnChange is called when enter is pressed rather than for every char (for UI).
			bool dblclick_selects_all:1; ///< if true, double clicking the input fields selects all text, as opposed to one words worth
			/**
			 TRUE if edit field was focused when it received mouse down. this will always be true if the widget is inside a FormObject.
			 this is used to make unfocused edit fields not in a form object select all text when clicked.
			 */
			bool mousedown_focus_status:1;
			/**
			 */
			bool show_drag_caret:1; ///< TRUE if the caret should be shown because something is being dragged
			bool replace_all_on_drop:1; ///< if TRUE, the dropped text replaces the current text, rather than being inserted
			bool has_been_focused:1; ///< TRUE if the widget has been focused. used in OnFocus to determine if all text should be selected.
			/**
			 TRUE if all text was selected when the widget was focused through mouse down. will be reset on mouse up. if set
			 and mouse move exceeds a certain threshold, selection is made from the position of the mouse down to the cursor.
			 */
			bool this_click_selected_all:1;
			bool use_default_autocompletion:1;
#ifdef WIDGETS_UP_DOWN_MOVES_TO_START_END
			bool ignore_go_to_line_start:1;
#endif //WIDGETS_UP_DOWN_MOVES_TO_START_END
#ifdef INTERNAL_SPELLCHECK_SUPPORT
			bool delayed_spell:1; ///< Is set to TRUE when spellchecking is postponed because the user currently writes a word
			bool enable_spellcheck_later:1; ///< TRUE if the spellchecker has been enabled, but the OpEditSpellcheck has not been created yet
#endif //INTERNAL_SPELLCHECK_SUPPORT
#ifdef WIDGETS_IME_SUPPORT
			bool im_is_composing:1; ///< TRUE when the edit field is in compose mode. see OpInputMethod.
#endif //WIDGETS_IME_SUPPORT
			bool is_last_user_change_an_insert:1;
			bool show_ghost_when_focused:1; ///< Show ghost string also when the input field is focused
			BOOL force_allow_clear:1; ///< TRUE when the field can be cleared even though is_readonly is FALSE
		} m_packed;
		unsigned m_packed_init;
	};

	OpString m_pattern;

#ifdef USE_OP_CLIPBOARD
	BOOL m_autocomplete_after_paste; ///< If TRUE autocomplete action will be performed after pasting into the edit.
#endif // USE_OP_CLIPBOARD
	OpWidgetResizeCorner* m_corner;
};

#endif // OP_EDIT_H
