/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_DOCUMENTEDIT_H
#define OP_DOCUMENTEDIT_H

#ifdef DOCUMENT_EDIT_SUPPORT

#ifdef _DEBUG
#define _DOCEDIT_DEBUG
#endif

#ifdef INTERNAL_SPELLCHECK_SUPPORT
#include "modules/spellchecker/opspellcheckerapi.h"
#endif //INTERNAL_SPELLCHECK_SUPPORT
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/widgets/OpWidget.h"
#include "modules/documentedit/OpDocumentEditUndoRedo.h"
#include "modules/documentedit/OpDocumentEditLayoutModifier.h"
#include "modules/logdoc/html.h"
#include "modules/logdoc/selectionpoint.h"
#include "modules/layout/box/box.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/doc/caret_manager.h"
#include "modules/doc/caret_painter.h"
#include "modules/logdoc/elementref.h"
class OpDocumentEdit;

#ifdef USE_OP_CLIPBOARD
#include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

/** Used when inserting html to create empty textelements and prevent them from being removed by OpDocumentEdit::Tidy.
	The dummycharacter will be removed after insertion. */

#ifdef _DOCEDIT_DEBUG
#define DEBUG_CHECKER(check_begin_count) OpDocumentEditDebugChecker __debug_checker__dbg__ = OpDocumentEditDebugChecker(m_edit,this,check_begin_count,FALSE)
#define DEBUG_CHECKER_STATIC() OpDocumentEditDebugChecker __debug_checker__dbg__ = OpDocumentEditDebugChecker(NULL,NULL,FALSE,TRUE)
#define DEBUG_CHECKER_CONSTRUCTOR() DEBUG_CHECKER_STATIC()
#define document_edit_dummy_str UNI_L("*")
#else
#define DEBUG_CHECKER(x)
#define DEBUG_CHECKER_STATIC()
#define DEBUG_CHECKER_CONSTRUCTOR()
#define document_edit_dummy_str UNI_L("\xFEFF") // zero width nonbreaking space
#endif

/** Define to enable <p> for linebreaks instead of <br> as default. (<br> still available for shift+enter) */
#define DOCUMENT_EDIT_USE_PARAGRAPH_BREAK

/** Styling added without a selection is placed in a list of pending styles, to be added when text is inserted if caret hasn't moved. */

struct OP_DOCUMENT_EDIT_PENDING_STYLES : public Link {
	HTML_Element* helm;
};

#define ATTR_DOCEDIT_AUTO_INSERTED Markup::DOCEDA_AUTO_INSERTED	///< Inserted automatically and should be removed automatically when not needed.

enum OP_DOCUMENT_EDIT_COMMAND {
	OP_DOCUMENT_EDIT_COMMAND_USECSS,
	OP_DOCUMENT_EDIT_COMMAND_STYLEWITHCSS,
	OP_DOCUMENT_EDIT_COMMAND_READONLY,
	OP_DOCUMENT_EDIT_COMMAND_CONTENTREADONLY,
	OP_DOCUMENT_EDIT_COMMAND_HILITECOLOR,
	OP_DOCUMENT_EDIT_COMMAND_BACKCOLOR,
	OP_DOCUMENT_EDIT_COMMAND_BOLD,
	OP_DOCUMENT_EDIT_COMMAND_COPY,
	OP_DOCUMENT_EDIT_COMMAND_CREATELINK,
	OP_DOCUMENT_EDIT_COMMAND_CUT,
	OP_DOCUMENT_EDIT_COMMAND_DELETE,
	OP_DOCUMENT_EDIT_COMMAND_FONTNAME,
	OP_DOCUMENT_EDIT_COMMAND_FONTSIZE,
	OP_DOCUMENT_EDIT_COMMAND_FORECOLOR,
	OP_DOCUMENT_EDIT_COMMAND_FORMATBLOCK,
//	OP_DOCUMENT_EDIT_COMMAND_HEADING,
	OP_DOCUMENT_EDIT_COMMAND_INDENT,
	OP_DOCUMENT_EDIT_COMMAND_INSERTHORIZONTALRULE,
	OP_DOCUMENT_EDIT_COMMAND_INSERTHTML,
	OP_DOCUMENT_EDIT_COMMAND_INSERTIMAGE,
	OP_DOCUMENT_EDIT_COMMAND_INSERTORDEREDLIST,
	OP_DOCUMENT_EDIT_COMMAND_INSERTUNORDEREDLIST,
	OP_DOCUMENT_EDIT_COMMAND_INSERTPARAGRAPH,
	OP_DOCUMENT_EDIT_COMMAND_ITALIC,
	OP_DOCUMENT_EDIT_COMMAND_JUSTIFYCENTER,
	OP_DOCUMENT_EDIT_COMMAND_JUSTIFYFULL,
	OP_DOCUMENT_EDIT_COMMAND_JUSTIFYLEFT,
	OP_DOCUMENT_EDIT_COMMAND_JUSTIFYRIGHT,
	OP_DOCUMENT_EDIT_COMMAND_OUTDENT,
	OP_DOCUMENT_EDIT_COMMAND_PASTE,
	OP_DOCUMENT_EDIT_COMMAND_REDO,
	OP_DOCUMENT_EDIT_COMMAND_REMOVEFORMAT,
	OP_DOCUMENT_EDIT_COMMAND_SELECTALL,
#ifdef SPELLCHECK_COMMAND_SUPPORT
	OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK,
	OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_BLOCKING,
	OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_DEBUG,
	OP_DOCUMENT_EDIT_COMMAND_SPELLCHECK_HAS_MISSPELLING,
#endif
	OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH,
	OP_DOCUMENT_EDIT_COMMAND_SUBSCRIPT,
	OP_DOCUMENT_EDIT_COMMAND_SUPERSCRIPT,
	OP_DOCUMENT_EDIT_COMMAND_DECREASEFONTSIZE,
	OP_DOCUMENT_EDIT_COMMAND_INCREASEFONTSIZE,
	OP_DOCUMENT_EDIT_COMMAND_UNDERLINE,
	OP_DOCUMENT_EDIT_COMMAND_UNDO,
	OP_DOCUMENT_EDIT_COMMAND_UNLINK,
	OP_DOCUMENT_EDIT_COMMAND_UNSELECT,
	OP_DOCUMENT_EDIT_COMMAND_DEFAULT_BLOCK,
	OP_DOCUMENT_EDIT_COMMAND_UNKNOWN
};


/**
 * This is a class that represents how we did selection points before converting to the
 * web official representation.
 *
 * TEXT + offset means the same thing as in normal boundary points.
 * ELEMENT + offset must have offset 0 or 1 and means before or after ELEMENT.
 */
class OldStyleTextSelectionPoint
{
private:
	HTML_Element* m_element;
	int m_offset;
	BOOL m_bind_forward;
public:
	OldStyleTextSelectionPoint() : m_element(NULL), m_offset(0), m_bind_forward(FALSE) {}
	OldStyleTextSelectionPoint(HTML_Element* elm, int offset) : m_element(elm), m_offset(offset), m_bind_forward(FALSE) {}

	void SetLogicalPosition(HTML_Element* elm, int offset) { m_element = elm; m_offset = offset; m_bind_forward = FALSE; }
	/**
	 * If a selection point has multiple visual positions (BiDi and line breaks
	 * make that happen), then this flag says which is preferred in rendering.
	 *
	 * For a line break, if the rendering should be on the previous line, the
	 * direction should be BIND_BACKWARD. If the rendering should be at the start
	 * of the new line, then the direction should be BIND_FORWARD.
	 *
	 * @param[in] direction The new bind direction.
	 */
	void SetBindDirection(SelectionBoundaryPoint::BindDirection direction) { m_bind_forward = (direction == SelectionBoundaryPoint::BIND_FORWARD); }

	/**
	 * If a selection point has multiple visual positions (BiDi and line breaks
	 * make that happen), then this flag says which is preferred in rendering.
	 *
	 * @returns The direction. The default is BIND_BACKWARD.
	 */
	SelectionBoundaryPoint::BindDirection GetBindDirection() const { return m_bind_forward ? SelectionBoundaryPoint::BIND_FORWARD : SelectionBoundaryPoint::BIND_BACKWARD; }

	HTML_Element* GetElement() const { return m_element; }
	int GetOffset() const { return m_offset; }
	int GetElementCharacterOffset() const { return m_offset; }

	BOOL Precedes(const OldStyleTextSelectionPoint& other_selection) const
	{
		return GetElement() == other_selection.GetElement() ?
			m_offset < other_selection.m_offset :
			GetElement()->Precedes(other_selection.GetElement());
	}

	static SelectionBoundaryPoint ConvertFromOldStyleSelectionPoint(const OldStyleTextSelectionPoint& point);
};

/** OpDocumentEditSelection
	Main tasks:
		1. Wrapper/helper for TextSelection in the document.
		2. Can return the selected text as plain text, or as HTML-code (with nearest shared ancestor of start and stop as root).
		3. Can remove the selected elements (and parts of the textelements).
*/

class OpDocumentEditSelection
#ifdef _DOCEDIT_DEBUG
	: public OpDocumentEditDebugCheckerObject
#endif
{
public:
	OpDocumentEditSelection(OpDocumentEdit *edit);

	/**
	 * Sets the HTML_Document selection for the FramesDocument associated this documentedit according to the selection end points.
	 * @parm startpoint The start or end point of the new selection, order doesn't matter infact.
	 * @parm startpoint The start or end point of the new selection, order doesn't matter infact.
	 */
	void Select(OldStyleTextSelectionPoint *startpoint, OldStyleTextSelectionPoint *endpoint, BOOL place_caret_at_endpoint = TRUE);

	/**
	 * Sets the HTML_Document selection to the end-points between the current caret-place and (from_helm, from_ofs).
	 * This is NOT the dom-type of endpoint-sematics with parent-element + offset==child elements before selection end-point, see below.
	 * @parm from_helm The other selection end-point (in addition to the caret-place), must be an element with replaced
	 * content, an HE_BR or an HE_TEXT element.
	 * @parm from_ofs Offset from the beginning of the element, for HE_TEXT elements is this the number of characters BEFORE the
	 * selection end-point. For all other elements can the offset be either 0 or 1, 0 means that the end-point is "just before" the
	 * element and 1 "just after".
	 */
	void SelectToCaret(HTML_Element* from_helm, INT32 from_ofs);

	/**
	 * Selects all content in the HTML_Document, OR if the caret is inside an content-editable element, all content
	 * in that element.
	 */
	void SelectAll();

	/** Clears the selection in the HTML_Document. */
	void SelectNothing();

	/**
	 * Returns whether the HTML_Document has a non-empty selection or not.
	 * @return FALSE if the HTML_Document doesn't have any text-selection or if the selection is empty, that is, if the start
	 * and end end-points points at the same element and has the same "element character offset" (the same logical position).
	 */
	BOOL HasContent();

	/**
	 * Removes the content selected in the HTML_Document.
	 * @parm aggressive If aggressive is TRUE, the containingelement may be removed if it gets empty and the caret may snap to nearest word.
	 * If the selection should be replaced with something new, it's a good idea to use FALSE.
	 */
	void RemoveContent(BOOL aggressive = TRUE);

	/**
	 * Removes the content between the two "end-points" (start_elm, start_ofs) and (stop_elm, stop_ofs).
	 * End-points are represented in the same manner as for SelectToCaret, but all types of elements could be start_elm or stop_elm.
	 * @parm aggressive If aggressive is TRUE, the containingelement may be removed if it gets empty and the caret may snap to nearest word.
	 * If the selection should be replaced with something new, it's a good idea to use FALSE.
	 * @parm start_elm Start element of the selection that should be removed, MUST precede stop_elm.
	 * @parm stop_elm Stop element of the selection that should be removed, MUST be preceded by start_elm.
	 * @parm start_ofs Start "end-point" in start_elm.
	 * @parm start_ofs Stop "end-point" in stop_elm.
	 * @param change_caret_pos If FALSE a content removal does not change the caret's position.
	 */
	void RemoveContent(BOOL aggressive, HTML_Element* start_elm, HTML_Element* stop_elm, INT32 start_ofs, INT32 stop_ofs, BOOL change_caret_pos = TRUE);

	/**
	 * Retrieves the HTML_Document's selection in text form. This does only include selected text in selected HE_TEXT elements (no tags and such).
	 * @return Error-code of the operation.
	 * @parm text OpString object to output the text to, will be empty after return if selection was empty or non-existing.
	 */
	OP_STATUS GetText(OpString& text);

	/**
	 * Retrieves the HTML_Document's selection in html text form. The nearest shared container will act as root of the result, except
	 * for special-case when include_style_containers == TRUE.
	 * @return Error-code of the operation.
	 * @parm text OpString object to output the text to, will be empty after return if selection was empty or non-existing.
	 * @parm include_style_containers If include_style_containers == TRUE, the will the root of the result be adjusted upwards in the
	 * tree so it's NOT an HE_OL, HE_UL or HE_PRE element, this if for keeping the "style" of the selected content.
	 */
	OP_STATUS GetTextHTML(OpString& text, BOOL include_style_containers = TRUE);

	/**
	 * Retrieves the selection between the two "end-points" (start_elm, start_ofs) and (stop_elm, stop_ofs) in html
	 * text form. The nearest shared container will act as root of the result, except for special-case when include_style_containers == TRUE.
	 * @return Error-code of the operation.
	 * @parm text OpString object to output the text to, will be empty after return if selection was empty or non-existing.
	 * @parm include_style_containers If include_style_containers == TRUE, the will the root of the result be adjusted upwards in the
	 * tree so it's NOT an HE_OL, HE_UL or HE_PRE element, this if for keeping the "style" of the selected content.
	 */
	OP_STATUS GetTextHTML(OpString& text, HTML_Element* start_elm, HTML_Element* stop_elm, INT32 start_ofs, INT32 stop_ofs, BOOL include_style_containers = TRUE);

	/** Returns the start element of the HTML_Document's selection (NOT DOM-style with, parent-element and child-element offset) */
	HTML_Element* GetStartElement(BOOL only_actual_element = FALSE);

	/** Returns the stop element of the HTML_Document's selection (NOT DOM-style with, parent-element and child-element offset) */
	HTML_Element* GetStopElement(BOOL only_actual_element = FALSE);

	/** Returns the start offset of the HTML_Document's selection (NOT DOM-style with, parent-element and child-element offset) */
	INT32 GetStartOfs();

	/** Returns the end offset of the HTML_Document's selection (NOT DOM-style with, parent-element and child-element offset) */
	INT32 GetStopOfs();

	/** Returns TRUE if the end point is the focus point and FALSE if the start point is the focus point. */
	BOOL IsEndPointFocusPoint();
private:
	BOOL GetRemoveStopElm(BOOL aggressive, HTML_Element *start_elm, int start_ofs, HTML_Element *stop_elm, int stop_ofs);
public:
	OpDocumentEdit* m_edit;
};

/** OpDocumentEditCaret
	It make use of the SelectionUpdateObject traversal object to find out the visual position of the caret. Same as selection endpoints.

	The caret position is based on a HTML_Element, a offset, and the BOOL m_prefer_first.
	For textelements, the offset is the character offset.
	For other elements, the offset is either 0 or 1 (left or right to the element).
	The offset is a logical position, so we need m_prefer_first to tell if it should stick to the first or second word that has the offset.

	FIX: The DOM API always use the element as parent element, and offset as child offset. That means same as our current behaviour for text, but
	with other elements we have a different behaviour. This is worked around in the doc/DOM code. But we should change this here at some point.
*/

class OpDocumentEditCaret
#ifdef _DOCEDIT_DEBUG
	: public OpDocumentEditDebugCheckerObject
#endif

{
public:
	OpDocumentEditCaret(OpDocumentEdit *edit);

	/**
	 * Initialization function for the OpDocumentEditCaret.
	 * @return Error status code...
	 * @parm create_line_if_empty If TRUE will a new empty text-element be created for the caret if no valid caret-pos exists.
	 * @parm edit_root An editable root element in the document where the caret is "encouraged" to be placed. If edit_root is
	 * NULL will body be used if it's editable (designMode=='on' or body is content editable), or a "parent canditate" which
	 * has previously been found will be used.
	 * @parm only_create_line_if_body If TRUE and create_line_if_empty is TRUE, then will a new line only be created if there
	 * is a body element that could be an ancestor for the new line.
	 */
	OP_STATUS Init(BOOL create_line_if_empty, HTML_Element* edit_root = NULL, BOOL only_create_line_if_body = FALSE);

	/** Returns FALSE if the current position, which has been updated with UpdatePos is not valid (inside something collapsed) */
	BOOL IsValid();

	/**
	 * Tries to "snap" the current caret-position to something valid (if it's not already valid).
	 * Returns FALSE if we still don't have a valid position.
	 */
	BOOL SetToValidPos() { return GetCaretManager()->SetToValidPos(); }

	/** Check if the caret is at the first or last insertionpoint in its element (it might still be collapsed data before/after though). */
	BOOL IsAtStartOrEndOfElement(BOOL start);

	/** Check if the caret is at the first or last insertionpoint in its container (it might still be collapsed data before/after though).
		is_at_edge_to_child_container will be set to TRUE if it's at the start/end before/after a child block within the block. */
	BOOL IsAtStartOrEndOfBlock(BOOL start, BOOL *is_at_edge_to_child_container = NULL);

	/**
	 * When the caret is placed somewhere else should the current caret-element, which should be an empty text-element,
	 * be removed if it's then still an empty text-element and no changes has been made to the logical tree from within
	 * documentedit. This is for creating temporary caret-positions e.g. to permitt the user to navigate below a HR element
	 * at the bottom of the document.
	 */
	void SetRemoveWhenMoveIfUntouched();

	/**
	 * If the current caret-element is a temporary element created as described in the description of SetRemoveWhenMoveIfUntouched, then
	 * will that element be removed if it's not == new_helm and the temporary element is still "untouched".
	 */
	void DeleteRemoveWhenMoveIfUntouched(HTML_Element *new_helm);

	/** Returns TRUE if the caret is in an "untouched" temporary caret-element as described in the description of SetRemoveWhenMoveIfUntouched */
	BOOL IsAtRemoveWhenMoveIfUntounced();

	/** Makes the current possible temporary caret element "permanent", see description of SetRemoveWhenMoveIfUntouched */
	void SetNotRemoveWhenMoveIfUntouched() { m_remove_when_move_elm = NULL; }

	/**
	 * Creates an empty text element under parent as a successor of after_me without making the possible temporary caret-element,
	 * as described in the description of SetRemoveWhenMoveIfUntouched "permanent".
	 * @parm parent The parent for the text-element, can be NULL if after_me != NULL.
	 * @parm after_me The element which the text-element should be successor of, if after_me == NULL, the text-element
	 * will be first child of parent.
	 * @param text If TRUE a text element will be created instead of <br>.
	 */
	HTML_Element* CreateTemporaryCaretHelm(HTML_Element *parent, HTML_Element *after_me = NULL, BOOL text = FALSE);

	/**
	 * Move the caret forward or back in the document in the logical position
	 * @parm forward If TRUE, tries to move one step forward, else backwards...
	 * @parm word If TRUE, we'll move to the next word-boundry.
	 */
	void Move(BOOL forward, BOOL word) { GetCaretManager()->Move(forward, word); }

	/**
	 * Create room for a caret by creating the necessary placeholder elements and then position
	 * the caret there.
	 *
	 * @param new_helm The element that would be the place for the caret if there was room.
	 * @param new_ofs The offset (old style selection) of the element for the caret.
	 * @param forward Whether we reached the current position going forwards or backwards.
	 */
	void MakeRoomAndMoveCaret(HTML_Element* new_helm, int new_ofs, BOOL forward);

	/**
	 * Move the caret forward or back in the document in the logical position, but instead of wrapping to next
	 * or previous line, it will search for the visually closest editable element in the given direction.
	 * @parm forward If TRUE, tries to move one step forward, else backwards...
	 */
	void MoveSpatial(BOOL forward);

	/**
	 * Works like MoveSpatial but tries to move the caret visually up or down in the document.
	 * @parm down If TRUE, tries to move one line down, else up...
	 */
	void MoveSpatialVertical(BOOL down);

	/**
	 * Places the caret on the specified end-point.
	 * @parm point The desired caret-position, if the element in point is not valid for putting the caret on, it tries
	 * to use the DOM-semantics and use the element as parent and the "element character offset" as child-offset.
	 */
	void Place(const SelectionBoundaryPoint& point);

	void Place(INT32 x, INT32 y, BOOL remember_x = TRUE, BOOL enter_all = FALSE, BOOL search_whole_viewport = FALSE);

	/**
	 * Places the caret on the specified place in the document,
	 * e.g. at the beginning of the line where the caret is currently
	 * placed.
	 * @parm place Any of the places specified in the enum
	 * OpWindowCommander::CaretMovementDirection.
	 *
	 * @see OpWindowCommander::CaretMovementDirection
	 */
	void Place(OpWindowCommander::CaretMovementDirection place);

	/**
	 * Tries to place the caret at the logical position defined by helm+ofs.
	 * @parm helm The element to put the caret on.
	 * @parm ofs The element-offset to put the caret on.
	 * @parm prefer_first If (helm, ofs) corresponds to two reasonable visual
	 *       positions (for example, at the end of one line vs. at the start of
	 *       the next line), setting this to TRUE will place the caret at the
	 *       first position relative to the flow of text, and vice versa.
	 * @parm allow_snap If TRUE, helm+ofs will be "snapped" to a valid, non-collapsed, logical position.
	 * @parm keep_within_current_context If TRUE and the current caret-position is withing a different contentEditable container
	 * then helm, then will the operation not do anything.
	 */
	void Place(HTML_Element* helm, int ofs, BOOL prefer_first = TRUE,
	           BOOL allow_snap = TRUE,
	           BOOL keep_within_current_context = FALSE) { GetCaretManager()->Place(helm, ofs, prefer_first, allow_snap, keep_within_current_context); }

	/**
	 * Tries to place the caret first en the editable container edit_root if a valid caret position is within that element
	 * @parm edit_root the editable container to put the caret in at the first available caret position, if NULL will a possible
	 * "parent candidate" or the body (if it's editable) be used.
	 * @parm create_line_if_empty If TRUE, will create a new line if none exists
	 */
	void PlaceFirst(HTML_Element* edit_root = NULL, BOOL create_line_if_empty = TRUE);

	/**
	 * @parm helm The element to put the caret on.
	 * @parm ofs The element-offset to put the caret on.
	 * @parm allow_snap If TRUE, helm+ofs will be "snapped" to a valid, non-collapsed, logical position.
	 * @parm keep_within_current_context If TRUE and the current caret-position is withing a different contentEditable container
	 * then helm, then will the operation not do anything.
	 * Returns FALSE if the caret movement should be aborted.
	 */
	BOOL FixAndCheckCaretMovement(HTML_Element*& helm, int& ofs, BOOL allow_snap, BOOL keep_within_current_context);

	/** Return TRUE if the element is editable or a child of a editable content. */
	BOOL IsElementEditable(HTML_Element* helm);

	/** You should normally not use this function. Use Place instead since it will take care of many special situations for you. */
	void Set(HTML_Element* helm, int ofs, BOOL prefer_first = FALSE, BOOL remember_x = TRUE);

	/** Stores a real caret element and offset. */
	void StoreRealCaretPlacement(HTML_Element* helm, int ofs);

	/** You should normally not use this function. Use Place instead since it will take care of many special situations for you. */
	void Set(HTML_Element* helm, int ofs, int x, int y, int height);

	/** If the caret has offset zero, is not an empty string and has a "friendly" element before, then will the caret "snap" to that element. */
	void StickToPreceding();

	/** If the caret is placed at the "end" of an element and there is a "friendly" "dummy" element after, then "snap" to that element */
	void StickToDummy();

	/** Remember current x-position as the wanted x-position when moving caret from line to line. */
	void UpdateWantedX();

	/** When set to TRUE, caret will behave like it's not between character, but as it selects the entire character */
	void SetOverstrike(BOOL overstrike);

	/** Makes the caret "blink" (toggle from visible->not visible or the other way around...) */
	void BlinkNow();

	/** Restart the caret "blinking" and starts with the caret visible, looks nice when changing the caret position... */
	void RestartBlink();

	/** Makes the caret invisible, stops blinking and entering the invisible state */
	void StopBlink();

	/**
	 * Lock or unlock the possibility for the caret to update its
	 * drawing position. Can be used to optimize, by avoiding to
	 * update it several times during a operation.
	 * @parm lock TRUE to lock and FALSE to unlock (that is, update
	 * caret-pos if we're not inside "nestled" LockUpdatePos calls).
	 * @parm process_update_pos If FALSE will the caret NOT be updated
	 * even though the "nestling-count" reaches zero when lock ==
	 * FALSE.
	 */
	void LockUpdatePos(BOOL lock, BOOL process_update_pos = TRUE);

	/**
	 * Update the drawing position. Returns TRUE if the update is
	 * done. Returns FALSE if it is postponed. F.ex if it was locked
	 * or if the document was dirty.  This is done automatically from
	 * the Place functions.
	 * @returns TRUE if the caret's position was successfully updated.
	 * @parm prefer_first
	 * @parm create_line_if_empty If TRUE, will create a new line if
	 * none exists
	 */
	BOOL UpdatePos(BOOL prefer_first = FALSE, BOOL create_line_if_empty = TRUE);

	/**
	 * Inform the documentedit that the caret position has been
	 * effectively updated.
	 */
	void OnUpdatePosDone();

	/**
	 * Get position and size of the caret in document coordinates.
	 */
	OpRect GetCaretRectInDocument() const;

	/**
	 * Returns the document's CaretManager. Never NULL since it must exist for there
	 * to be a OpDocumentEdit.
	 */
	CaretManager* GetCaretManager();

	HTML_Element* GetElement();
	int GetOffset();

	/**
	 * Removes the temporary text caret element if possible.
	 *
	 * @param new_is_created - TRUE if a new temporary caret text element is being created.
	 */
	void CleanTemporaryCaretTextElement(BOOL new_is_created);

	/**
	 * Converts the temporary text caret element to <br>.
	 *
	 * @return A br element being a replacement for the temporary text caret element.
	 */
	HTML_Element* ConvertTemporaryCaretTextElementToBR();

	/**
	 * @return TRUE if the caret is currently on unmodified temporary text element.
	 */
	BOOL IsUnmodifiedTemporaryCaretTextElement();

	OpDocumentEdit* m_edit;
	HTML_Element* m_parent_candidate;	///< Delayed initialization of the caret. The caret should be placed under this element when initialized.

	HTML_Element *m_remove_when_move_elm;
	int m_remove_when_move_id;

	/** Keeps a reference to a temporary text caret element */
	AutoNullOnDeleteElementRef m_temp_caret_text_elm;

	/** Keeps a reference to a real caret element (the one the caret was placed at). */
	AutoNullOnDeleteElementRef m_real_caret_elm;

	/** An offset within a real caret element. */
	int m_real_caret_elm_off;
};

/** Listener interface for being notified when the caret changes position or text-content is changed, only ONE listener is supported,
  * the litener is added with OpDocumentEdit::SetListener.
  */
class OpDocumentEditListener
{
public :
	virtual ~OpDocumentEditListener() {}

	/** Callback for when the caret changes position or when caret is being updated. */
	virtual void OnCaretMoved() {}

	/** Callback for when documentedit inserts or removes text in the document. */
	virtual void OnTextChanged() {}

#ifdef SUPPORT_TEXT_DIRECTION
	/** Callback for when documentedit changes text direction, with the direction it changed to */
	virtual void OnTextDirectionChanged(BOOL to_rtl) {}
#endif //SUPPORT_TEXT_DIRECTION
};

enum TIDY_LEVEL {
	TIDY_LEVEL_NONE,		///< Do not tidy anything.
	TIDY_LEVEL_MINIMAL,		///< Don't remove anything. Just convert the document_edit_dummy_str to empty textnode.
	TIDY_LEVEL_NORMAL,		///< Remove elements that doesn't have any children etc. Except if they contain a empty textnode (the user has removed the text and might want to type new text).
	TIDY_LEVEL_AGGRESSIVE	///< Same as TIDY_LEVEL_NORMAL without exceptions.
};

/** Enum for describing how list items in a selection are ordered,
    according to the outmost nestling-level.
*/
enum SELECTION_ORDERING
{
  UNKNOWN,		/// Not initialized yet
  ORDERED,		/// All items are ordered
  UN_ORDERED,	/// All items are un-ordered
  SPLIT_ORDER	/// Some items are ordered while other are un-ordered
};

/** "Abstraction" over OpDocumentEditSelection that is used internally, mainly for simplifying handling
 * of cases when the start of the selection is at the end of an element and/or the end of the selection
 * is before the start of an element (OpDocumentEditSelection::GetStopOfs() == 0).
 * For this purpose are editable_start_elm, editable_stop_elm, editable_start_ofs and editable_stop_ofs
 * used, these fields are also "trimmed" so that they points at end-points which are valid for the caret.
 * This struct are obtained by OpDocumentEdit::GetSelectionState and restored using
 * OpDocumentEdit::RestoreSelectionState.
 */
struct SelectionState
{
	SelectionState() : caret_elm(NULL), caret_ofs(0), start_elm(NULL), stop_elm(NULL),
		editable_start_elm(NULL), editable_stop_elm(NULL), start_ofs(0), stop_ofs(0),
		editable_start_ofs(0), editable_stop_ofs(0), removed_selection(FALSE), removed_caret(FALSE), caret_at_end(FALSE) {}

	HTML_Element *caret_elm; ///< Caret element when this SelectionState was retrieved.
	int caret_ofs; ///< Caret offset when this SelectionState was retrieved.
	HTML_Element *start_elm, *stop_elm; ///< Start and stop elements as retrieved from OpDocumentEditSelection when this SelectionState was retrieved.
	HTML_Element *editable_start_elm, *editable_stop_elm; ///< The result when start_elm and stop_elm has been "trimmed" according to the description of SelectionState
	INT32 start_ofs, stop_ofs; ///< Start and stop offset as retrieved from OpDocumentEditSelection when this SelectionState was retrieved.
	INT32 editable_start_ofs, editable_stop_ofs; ///< The result when start_elm and stop_elm has been "trimmed" according to the description of SelectionState
	BOOL removed_selection; ///< Was the HTML_Document selection removed when this SelectionState was retrieved?
	BOOL removed_caret; ///< Was the caret removed when this SelectionState was retrieved?
	BOOL caret_at_end; ///< Was the caret at the end (as opposed to the start) selection point?

	/** Returns TRUE if it existed a selection and/or caret position when this SelectionState was retrieved */
	BOOL IsValid() { return start_elm && stop_elm; }

	/** Returns TRUE if this SelectionState represents a non-empty selection */
	BOOL HasContent() { return start_elm && stop_elm && (start_elm != stop_elm || start_ofs != stop_ofs); }

	/** Returns TRUE if this SelectionState represents editable content, that is, if (editable_start_elm, editable_start_ofs) and (editable_stop_elm, editable_stop_ofs) are different caret-positions. */
	BOOL HasEditableContent() { return editable_start_elm && (editable_start_elm != editable_stop_elm || editable_start_ofs != editable_stop_ofs); }
};

/** This class is intended for representing a consecutive range of HTML elements with the same parent.
*/
class SiblingBlock
{
public:
	SiblingBlock() : start(NULL), stop(NULL) {}
	SiblingBlock(HTML_Element *start_elm, HTML_Element *stop_elm) : start(start_elm), stop(stop_elm) {}
	bool operator==(const SiblingBlock &other) const {return start==other.start && stop==other.stop;}
	bool operator!=(const SiblingBlock &other) const {return !(*this == other);}
	bool IsEmpty() {return start == NULL;}
public:
	HTML_Element *start,*stop;
};

// The following are functions that might be used as arguments to OpDocumentEdit::GetSelectionMatchesFunction...

/** Returns TRUE if helm has an ATTR_ALIGN value that is NOT CSS_VALUE_left */
BOOL IsNonLeftAligned(HTML_Element *helm, void *dont_care_about_me);

/** Returns TRUE if helm has an ATTR_ALIGN value equals to CSS_align_value */
BOOL IsAlignedAs(HTML_Element *helm, void *CSS_align_value);

/** Returns TRUE if helm has any of the types in elm_types, elm_types should be an HTML_ElementType[] array ending with HE_UNKNOWN */
BOOL StaticIsMatchingType(HTML_Element *helm, void *elm_types);

// The following are functions that might be used as arguments to OpDocumentEdit::ApplyFunctionBetween...

/**
 * Sets ATTR_HREF on helm if it's an HE_A element according to arguments in arg.
 * @return Error status code...
 * @parm helm The HE_A element to change ATTR_HREF for.
 * @parm arg (OpDocumentEdit*)(((void**)(args))[0]) should be an OpDocumentEdit instance and
 * (OpDocumentEdit*)(((void**)(args))[1]) should be the new href string.
 * @parm ret Set to TRUE if ATTR_HREF was updated, otherwise FALSE.
 */
OP_STATUS SetNewHref(HTML_Element *helm, void *arg, BOOL &ret);

/** Change flags informing UNDO/REDO stack about a type of a change */
enum ChangeFlags
{
	/** No flags */
	CHANGE_FLAGS_NONE = 0,
	/** The change may be appended to the previous one, if it appends it */
	CHANGE_FLAGS_ALLOW_APPEND = 1,
	/** The must not be seen by the user */
	CHANGE_FLAGS_USER_INVISIBLE = 2
};

/** UNDO/REDO stack event type */
enum UndoRedoEventType
{
	/** unknown event */
	UNDO_REDO_TYPE_UNKNOWN = -16, /* -16 */
	/** An event reverting an element change */
	UNDO_REDO_ELEMENT_REVERT_CHANGE = -3, /* -3 */
	/** An element removal */
	UNDO_REDO_ELEMENT_REMOVE, /* -2 */
	/** An event storing start caret placement */
	UNDO_REDO_CARET_PLACEMENT_BEGIN /* -1 */,
	/** An event storing end caret placement */
	UNDO_REDO_CARET_PLACEMENT_END, /* 0 => ~(-1) => ~UNDO_REDO_CARET_PLACEMENT_BEGIN */
	/** An element insertion */
	UNDO_REDO_ELEMENT_INSERT, /* 1 => ~(-2) => ~UNDO_REDO_ELEMENT_REMOVE */
	/** An element change - e.g. some of its attribute */
	UNDO_REDO_ELEMENT_CHANGE /* 2 => ~(-3) => ~UNDO_REDO_ELEMENT_REVERT_CHANGE */
};

/** OpDocumentEdit handles editing of a document.
	Will be created and assigned to a FramesDocument by calling FramesDocument::SetEditable(TRUE).
	Userinterface is done elsewhere and should interact with OpDocumentEdit through execCommand and the queryCommand* functions.
	All the editing is limited to the <body> element.
*/

class OpDocumentEdit : public OpInputContext, public MessageObject
#ifdef _DOCEDIT_DEBUG
	, public OpDocumentEditDebugCheckerObject
#endif
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	, public SpellUIHandler
#endif
#ifdef USE_OP_CLIPBOARD
	, public ClipboardListener
#endif // USE_OP_CLIPBOARD
{
public:
							OpDocumentEdit();
	virtual					~OpDocumentEdit();

	/** Static contstructor... */
	static OP_STATUS		Construct(OpDocumentEdit** obj, FramesDocument* doc, BOOL designmode);

	/** Called upon construction, should not be called explicitly */
	OP_STATUS				Init(FramesDocument* doc, BOOL designmode);

	/** Removes all elements in body, clears undo stack an re-initializes caret */
	OP_STATUS				Clear();

	/** Should remove all collapsed whitspaces in the document, FIXME: It just reflows!? */
	OP_STATUS				CollapseWhitespace();

	/** Paint stuff related to editing. Caret and resize-handles for boxes/tables etc.
		Should be done after document is fully painted. */
	void					Paint(VisualDevice* vis_dev);

	/**
	 * Mouse-event handling, will "unactivate" the current "layout modifier" if such exists and event is ONMOUSEDOWN,
	 * that is, if the user has previously clicked on an e.g. image so that the dashed border around it has appeared,
	 * that will be removed, and possibly, if helm is also an "layout modifiable" element then helm will be the new
	 * active "layout modifier".
	 * @return TRUE if a new "layout modifier" has been "activated".
	 * @parm helm The target for the mouse-event.
	 * @parm event The mouse-event, currently will nothing happen for other events other then ONMOUSEDOWN.
	 * @parm x X-coordinate in document-coordinates.
	 * @parm y Y-coordinate in document-coordinates.
	 * @parm button The mouse-button, currently is the bahaviour for all mouse buttons the same.
	 */
	BOOL					HandleMouseEvent(HTML_Element* helm, DOM_EventType event, int x, long y, MouseButton button);

	/**
	 * Checks if a particular element can be the currently active "layout modifier", see description for HandleMouseEvent.
	 * @return TRUE if helm is layout modifiable.
	 * @parm helm The html-element...
	 */
	BOOL					IsLayoutModifiable(HTML_Element* helm);

	/**
	 * Returns how the mouse-cursor should appear if hovering over element helm.
	 * @return The cursor-type, if the position (x,y) is over a scroll-bar or if helm is not editable (not within a
	 * design-mode document or an contenteditable element) will CURSOR_AUTO be returned.
	 * @parm helm The html-element
	 * @parm x X-coordinate in document-coordinates.
	 * @parm y Y-coordinate in document-coordinates.
	 */
	CursorType				GetCursorType(HTML_Element* helm, int x, int y);

	/**
	 * Initiates the clipboard copy operation which may end up placing the selected
	 * part in the clipboard.
	 *
	 * @param cut - If TRUE the cut operation is initiated instead.
	 */
	void					Copy(BOOL cut = FALSE);

	/**
	 * Initiates the clipboard cut operation which may end up placing the selected
	 * part in the clipboard and removing it from the document.
	 *
	 * @see Copy()
	 */
	void					Cut() { Copy(TRUE); }

	/**
	 * Initiatse the clipboard paste operation which may end up inserting the clipboard's content
	 * at the current caret-position.
	 */
	void					Paste();

#ifdef USE_OP_CLIPBOARD
	// ClipboardListener API
	void					OnCopy(OpClipboard* clipboard);
	void					OnPaste(OpClipboard* clipboard);
	void					OnCut(OpClipboard* clipboard)
	{
		if(m_readonly || (!m_layout_modifier.IsActive() && !m_selection.HasContent()))
			return;
		OnCopy(clipboard);
		execCommand(OP_DOCUMENT_EDIT_COMMAND_DELETE);
	}
#endif // USE_OP_CLIPBOARD

	/**
	 * Undo the last event or linked chain of events recorded on the undo-stack, only works if these events have been
	 * created within documentedit and not if e.g. DOM has manually inserted elements.
	 */
	void					Undo();

	/**
	 * Redo the last event or linked chain of events recorded on the undo-stack, only works if these events have been
	 * created within documentedit and not if e.g. DOM has manually inserted elements.
	 */
	void					Redo();

	/**
	 * Removes all content inside body and inserts text.
	 * @return Error status code...
	 * @parm The text to inserted in the empty body, just plain text, doesn't parse html-tags and such.
	 */
	OP_STATUS				SetText(const uni_char* text);

	/**
	 * Removes all content inside body and the content represented as html-code in text.
	 * @return Error status code...
	 * @parm The html-content to be inserted in the empty body, transforms the html-code to a logical tree.
	 */
	OP_STATUS				SetTextHTML(const uni_char* text);

	/**
	 * Retrieves the text of the document. This does only include text in HE_TEXT elements (no tags and such).
	 * @return Error-code of the operation.
	 * @parm text OpString object to output the text to.
	 * @parm block_quotes_as_text If TRUE, blockquotes will make containing lines start with '>' characters.
	 */
	OP_STATUS				GetText(OpString& text, BOOL block_quotes_as_text = FALSE);

	/** Outputs the html-code for the document's body element and it's inner content. */
	OP_STATUS				GetTextHTML(OpString& text);

	/** Called when script has modified the selection. */
	void					OnDOMChangedSelection();

	/**
	 * Sets the active OpDocumentEditListener for this OpDocumentEdit instance, see description of OpDocumentEditListener
	 * @return Always TRUE.
	 * @parm listener The OpDocumentEditListener, may be NULL.
	 */
	BOOL					SetListener(OpDocumentEditListener* listener) { m_listener = listener; return TRUE; }

	/**
	 * Returns the active OpDocumentEditListener for this OpDocumentEdit instance, see description of OpDocumentEditListener
	 * @return The OpDocumentEditListener, might be NULL.
	 */
	OpDocumentEditListener*	GetListener() { return m_listener; }

	/**
	 * Gives selection end-points for the current selection or caret-position.
	 * @return TRUE if we succeeded to get a selection.
	 * @parm anchor The selection end-point where the user STARTED the selection, that is, the end-point where
	 * the caret is NOT located. This is if we're not having a selection and just the caret, then will both anchor
	 * and focus be the caret end-point.
	 * @parm focus The selection end-point where the user is currently selection, that is, the end-point where
	 * the caret IS located. Unfortunately we currently might have situations where the caret is not located at
	 * any of the selection end-points, then will anchor be the start of the selection and foucs the end, in logica
	 * order.
	 */
	BOOL					GetSelection(SelectionBoundaryPoint &anchor, SelectionBoundaryPoint &focus);

	// == Command API for use by the DOM engine ==================

	/** Converts a text-representation of a command to an OP_DOCUMENT_EDIT_COMMAND
		enum, e.g. "useCss" will return OP_DOCUMENT_EDIT_COMMAND_USECSS. */
	static OP_DOCUMENT_EDIT_COMMAND ConvertCommand(const uni_char* command);

	/**
	 * Executes a command on the current document or current selection, or the given range.
	 *
	 * @param  command The command type to execute.
	 * @param  document_edit The OpDocumentEdit instance. Can be NULL in which
	 *                       case the check is performed based on the origin
	 *                       of the provided URL and FramesDocument (for
	 *                       a limited set of commands).
	 * @param  showui The value of the 'show UI' argument.
	 * @param  value The value of the 'Value' argument.
	 * @param  origin_url The URL of the originating document.
	 * @param  frm_doc The document in context of which the command is performed
	 *                 if OpDocumentEdit is not provided. Must be non-NULL if
	 *                 document_edit is NULL.
	 *
	 * @return OpStatus::ERR_NOT_SUPPORTED if the command is not supported or
	 *                                     can't work in current context.
	 *         OpStatus::ERR_NO_ACCESS if the command is not allowed to run.
	 */
	static OP_STATUS		execCommand(const uni_char* command, OpDocumentEdit* document_edit, BOOL showui, const uni_char* value, URL& origin_url, FramesDocument* frm_doc);

	/**
	 * Returns a Boolean value that indicates whether a specified command can be
	 * successfully executed using execCommand, given the current state of the
	 * document.
	 *
	 * @param  command To command type to query for.
	 * @param  document_edit The OpDocumentEdit instance. Can be NULL in which
	 *                       case the check is performed based on the origin
	 *                       of the provided URL and FramesDocument (for
	 *                       a limited set of commands).
	 * @param  origin_url The URL of the originating document.
	 * @param  frm_doc The document in context of which the query is performed
	 *                 if OpDocumentEdit is not provided. Must be non-NULL if
	 *                 document_edit is NULL.
	 *
	 * @return a value indicating whether a specified command can be executed.
	 */
	static BOOL				queryCommandEnabled(const uni_char* command, OpDocumentEdit* document_edit, URL& origin_url, FramesDocument* frm_doc);

	/** Returns a Boolean value that indicates the current state of the command. */
	BOOL					queryCommandState(const uni_char* command);

	/**
	 * Returns a Boolean value that indicates whether the current command is
	 * supported on the current range.
	 *
	 * @param  command The command type to query for.
	 * @param  document_edit The OpDocumentEdit instance. Can be NULL in which
	 *                       case the check is performed based on the origin
	 *                       of provided URL (for a limited set of commands).
	 * @param  origin_url The URL of the originating document.
	 * @param  frm_doc The document in context of which the query is performed
	 *                 if OpDocumentEdit is not provided. Must be non-NULL if
	 *                 document_edit is NULL.
	 *
	 * @return a value indicating whether the command is supported.
	 */
	static BOOL				queryCommandSupported(const uni_char* command, OpDocumentEdit* document_edit, URL& origin_url, FramesDocument* frm_doc);

	/** Returns the current value of the document, range, or current selection for the given command. */
	OP_STATUS				queryCommandValue(const uni_char* command, TempBuffer& value);

	// == Internal functions for command API =====================

	OP_STATUS				execCommand(OP_DOCUMENT_EDIT_COMMAND command, BOOL showui = FALSE, const uni_char* value = NULL);
	BOOL					queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND command);
	BOOL					queryCommandState(OP_DOCUMENT_EDIT_COMMAND command);
	OP_STATUS				queryCommandSupported(OP_DOCUMENT_EDIT_COMMAND command, URL& origin_url);
	OP_STATUS				queryCommandValue(OP_DOCUMENT_EDIT_COMMAND command, TempBuffer& value);

	// == Internal functions for editing ==========================

	/**
	 * Sets the selected text to the given style. If there is no selection, the style will
	 * apply to typed text from that point.
	 * @parm type The type of the style-elements to create (e.g. HE_STRONG, HE_EM).
	 * @parm exclude_type If not 0, this specifies that style elements should NOT be inserted for elements
	 * that are already under style-elements of this type.
	 * @parm allow_nestling If TRUE, this indicates that it's ok with nestled style-elements, useful for
	 * e.g. nestling of BIG/SMALL elements.
	 * @parm must_be_below_this_type If not 0, it's ensured that the new style elements are not inserted above
	 * elements of this type. For example: BIG/SMALL elements should be inserted below FONT elements because
	 * if ATTR_SIZE is defined for the FONT elements, then will BIG/SMALL have no effect if they are inserted
	 * above those FONT elements.
	 */
	void					InsertStyle(HTML_ElementType type, HTML_ElementType exclude_type = (HTML_ElementType)0, BOOL allow_nestling = FALSE, HTML_ElementType must_be_below_this_type = (HTML_ElementType)0);

	/**
	 * Inserts HE_FONT elements with when necessary with ATTR_COLOR on the selection to make all selected text to have color color. If there is no selection, the style will
	 * apply to typed text from that point. This function uses InsertStyleElement.
	 * @parm color The color value.
	 */
	void					InsertFontColor(UINT32 color);

	/**
	 * Inserts HE_FONT elements with when necessary with ATTR_FACE on the selection to make all selected text to have the font
	 * specified by fontface. If there is no selection, the style will apply to typed text from that point.
	 * This function uses InsertStyleElement.
	 * @parm fontface The name of the font.
	 */
	void					InsertFontFace(const uni_char* fontface);

	/**
	 * Inserts HE_FONT elements with when necessary with ATTR_SIZE on the selection to make all selected text to have fontsize size. If there is no selection, the style will
	 * apply to typed text from that point. This function uses InsertStyleElement.
	 * @parm size The font size.
	 */
	void					InsertFontSize(UINT32 size);

	/**
	 * Inserts HE_FONT elements with when necessary with attribute attr which should have value val, on the selection. If there is no selection, the style will
	 * apply to typed text from that point. This function uses InsertStyleElement.
	 * @parm attr The attribute to set on the inserted HE_FONT elements.
	 * @parm val The attribute value to use.
	 */
	void					InsertFont(short attr, void* val);

	/**
	 * Sets the selected text to be "styled" under element helm, or several copies of helm. If there is no selection, the style will
	 * apply to typed text from that point.
	 * @parm helm The "style element" to use, should NOT be deleted or accessed after the call.
	 * @parm exclude_type If not 0, this specifies that style elements should NOT be inserted for elements
	 * that are already under style-elements of this type.
	 * @parm allow_nestling If TRUE, this indicates that it's ok with nestled style-elements, useful for
	 * e.g. nestling of BIG/SMALL elements.
	 * @parm must_be_below_this_type If not 0, it's ensured that the new style elements are not inserted above
	 * elements of this type. For example: BIG/SMALL elements should be inserted below FONT elements because
	 * if ATTR_SIZE is defined for the FONT elements, then will BIG/SMALL have no effect if they are inserted
	 * above those FONT elements.
	 */
	OP_STATUS				InsertStyleElement(HTML_Element* helm, HTML_ElementType exclude_type = (HTML_ElementType)0, BOOL allow_nestling = FALSE, HTML_ElementType must_be_below_this_type = (HTML_ElementType)0);

	/**
	 * "Extracts" the subtree from start_elm to stop_elm under old_root and moves it under new_parent after after_me.
	 * @return Error status code.
	 * @parm start-elm The first element in logical ordering to extract. If start_elm == NULL will it be set to the first element
	 * in the same block as stop_elm.
	 * @parm start-elm The last element (except of possible children to stop_elm which will follow in the move)
	 * in logical ordering to extract. If stop_elm == NULL will it be set to the last element in the same block as start_elm.
	 * @parm old_root The root_element which should stay in place, the extraction should occur under this element.
	 * @parm new_root The new parent for the extracted subtree.
	 * @parm after_me The element under new_parent which should precede the extracted tree, if it's NULL will the extracted
	 * tree be added before the possible first child of new_root.
	 */
	OP_STATUS				ExtractElementsTo(HTML_Element *start_elm, HTML_Element *stop_elm, HTML_Element *old_root, HTML_Element *new_root, HTML_Element *after_me);

#ifdef INTERNAL_SPELLCHECK_SUPPORT

	// == SpellUISession =======================
	virtual OpSpellCheckerSession *GetSpellCheckerSession() { return m_spell_session; }
	virtual BOOL SetSpellCheckLanguage(const uni_char *lang);
	virtual void DisableSpellcheck();
	virtual OpSpellCheckerWordIterator* GetAllContentIterator();
	virtual OP_STATUS ReplaceWord(OpSpellCheckerWordIterator *word_iterator, const uni_char *new_word);
	virtual void OnSessionReady(OpSpellCheckerSession *session);
	virtual void OnError(OpSpellCheckerSession *session, OP_STATUS error_status, const uni_char *error_string);
	virtual void OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements);
	virtual void OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word);
	virtual void OnCheckingComplete(OpSpellCheckerSession *session);
	virtual void OnCheckingTakesABreak(OpSpellCheckerSession *session);


	OP_STATUS EnableSpellcheckInternal(BOOL by_user, const uni_char *lang);
	void DisableSpellcheckInternal(BOOL by_user, BOOL force);
	void OnSpellcheckerStopped();
	void RunPendingSpellCheck();
	OP_STATUS SpellCheckRange(HTML_Element *first, HTML_Element *last);
	void RepaintElement(HTML_Element *helm);
	void OnBeforeNewCaretPos(HTML_Element *helm, int ofs);
	void DoSpellWordInfoUpdate(SpellWordInfoObject *old_info);
	void PossiblyDelayMisspell(BOOL was_delayed_misspell);
	OP_STATUS MarkMisspellingInTextElement(BOOL misspelled, HTML_WordIterator *word, HTML_Element *helm, int start_ofs, int stop_ofs);
	OP_STATUS MarkNodesMisspelled(BOOL misspelled, HTML_WordIterator *word);
	void SpellInvalidateAround(HTML_Element *helm, BOOL must_be_outside_helm = FALSE);
	BOOL HandleSpellCheckCommand(BOOL showui, const uni_char *value);

	OP_STATUS CreateSpellUISessionInternal(IntersectionObject *intersection, int &spell_id);
	OP_STATUS CreateSpellUISession(IntersectionObject *intersection, int &spell_id);

	WordInfo *GetDelayedMisspellWordInfo() { return m_delay_misspell_word_info; }
#endif // INTERNAL_SPELLCHECK_SUPPORT

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	/**
	   posts a message to recreate caret, for when an element is
	   removed and it is not possible to get a new caret position
	   because of this.
	 */
	OP_STATUS PostRecreateCaretMessage();

	// From OpInputContext
	void SetFocus(FOCUS_REASON reason)
	{
		m_is_focusing = TRUE;
		OpInputContext::SetFocus(reason);
		m_is_focusing = FALSE;
	}

	void ReleaseFocus(FOCUS_REASON reason = FOCUS_REASON_RELEASE)
	{
		if (!m_is_focusing)
			OpInputContext::ReleaseFocus(reason);
	}

private:

	/** Retrieves the left-margin of element helm in result_px */
	OP_STATUS GetCSSIndent(HTML_Element *helm, int &result_px);

	/** Sets the left-margin in helm to px */
	OP_STATUS SetCSSIndent(HTML_Element *helm, int px);

	/**
	 * Removes the inline CSS-property prop between start_elm and stop_elm.
	 * @return Error status code.
	 * @parm start_elm The first element to apply the change to.
	 * @parm stop_elm The last element to apply the change to.
	 * @parm prop The inline CSS-propery to remove.
	 * @parm only_block_element If TRUE, only remove from block-level elements.
	 */
	OP_STATUS RemoveInlinePropertyBetween(HTML_Element *start_elm, HTML_Element *stop_elm, int prop, BOOL only_block_elements = FALSE);

	/**
	 * Sets the ATTR_ALIGN attribute on an element. Currently this function uses HTML_Element::SetAttribute
	 * after converting value to a string because it seems HTML_Element::SetAttr is not enough anymore...
	 * @return Error status code.
	 * @param helm The html element...
	 * @param value The alignment value (CSS_VALUE_left etc).
	 */
	OP_STATUS SetAlignAttrFromCSSValue(HTML_Element *helm, CSSValue value);

	/**
	 * Extracts the tree to the left or right of the path between root and helm, the elements
	 * in the path up to (and including) root is copied if necessary, no elements without
	 * children are created. Currently will this function OP_ASSERT if root->FirstLeaf()==helm->LastLeaf()
	 * && extract_left==TRUE || root->LastLeaf()==helm->LastLeaf() && extract_left=FALSE, this means -
	 * this function should only be used when there is really something to "extract".
	 *
	 * Example: ExtractTreeBeside(root,e5,TRUE); on this tree...
	 *
	 *                         ...
	 *                          |
	 *                        <root>
	 *                       /  |   \
	 *                    <e1> <e2> <e3>
	 *                         /  \
	 *                      <e4> <e5>
	 *
	 * ...makes the tree look like this...
	 *
	 *                         ...
	 *                          |
	 *                        <root>
	 *                          |   \
	 *                        <e2> <e3>
	 *                           \
	 *                          <e5>
	 *
	 * ...and returns the copy of root...
	 *                           NULL
	 *                            |
	 *                        <copy of root>
	 *                       /    |
	 *                    <e1> <copy of e2>
	 *                          /
	 *                       <e4>
	 *
	 * @return the root of the extracted tree or NULL in case of an error.
	 * @parm root The root element for the operation, the returned element will be a copy of
	 * this element, see the example above.
	 * @parm helm The element in the tree that will be first/last (extract_left==TRUE/FALSE)
	 * -leaf of root after the operation after the left/right part of the tree has been extracted.
	 * Or more precisely: If extract_left==TRUE -> root->FirstLeaf()==helm->LastLeaf() after the
	 * operation, and: If extract_left==FALSE -> root->LastLeaf()==helm->LastLeaf() after operation.
	 * @parm extract_left Determines whether the extraction should apply to the left or right of
	 * the path between helm and root.
	 */
	HTML_Element*			ExtractTreeBeside(HTML_Element *root, HTML_Element *helm, BOOL extract_left);

	/**
	 * Copies the inline "style-elements" (STRONG, EM, etc) between src_helm and it's ancestor src_containing_elm
	 * (which is supposed to be a block-level element). The result will be a hierarchy of style elements where
	 * each element has one child (except of the lowest...), and if put_empty_string_at_bottom==TRUE - an empty
	 * text element will be at the bottom.
	 * @return Error status code.
	 * @parm src_helm The element somewhere below src_containing_helm where the traversal up until src_containing_elm
	 * should start in the search for style-elements to copy, src_helm itself is included in this search.
	 * @parm src_containing_elm This is supposed to be the block-level containing element of src_helm which the search
	 * (by traversing ->Parent()) should proceed up until. If src_containing_elm==NULL, then GetContainingElement(src_helm)
	 * will be used.
	 * @parm new_top_helm The root in the "style-tree" created by the function.
	 * @parm new_bottom_helm The bottom child in the "style-tree" created by the function. If put_empty_string_at_bottom
	 * was TRUE, then this element will be an empty text element.
	 * @parm put_empty_string_at_bottom If TRUE, then an empty string will be created as the bottom child in the "style-tree".
	 * @parm make_empty_dummy If put_empty_string_at_bottom==TRUE and make_empty_dummy==TRUE, then will the "empty"
	 * text element att the bottom of the "style-tree" infact contain the "dummy text string" for marking empty text
	 * elements which should not be removed by the Tidy() function.
	 */
	OP_STATUS				CreateStyleTreeCopyOf(HTML_Element *src_helm, HTML_Element *src_containing_elm, HTML_Element *&new_top_helm, HTML_Element *&new_bottom_helm, BOOL put_empty_string_at_bottom, BOOL make_empty_dummy);

	/** Inserts helm over the currently active layout "modifier", used by InsertStyleElement. */
	OP_STATUS               InsertStyleElementLayoutModified(HTML_Element* helm);

	/** Inserts multiple copies of helm over the selected text-content (and deletes helm itself), see description of InsertStyleElement */
	OP_STATUS				InsertStyleElementMultiple(HTML_Element* helm, HTML_ElementType exclude_type, BOOL allow_nestling);

	/** Inserts helm over the selected text-content, see description of InsertStyleElement */
	OP_STATUS				InsertStyleElementSingle(HTML_Element* helm, HTML_ElementType exclude_type, BOOL allow_nestling);

	/** Removes all "style elements" matching type and attr from the current selection, see description of RemoveStyle */
	void					RemoveStyleElementMultiple(HTML_ElementType type, short attr, BOOL just_on_level);

	/** Removes all "style elements" matching type and attr from the current selection, the same as RemoveStyleElementMultiple but
	 it's implemented differently and only works(?) when start- and stop-elements in the selection are "friends". See description of RemoveStyle. */
	void					RemoveStyleElementSingle(HTML_ElementType type, short attr, BOOL just_one_level);

	/**
	 * Returns a consecutive range of html elements with the same parent. It starts from
	 * helm and scans in both directions (using Suc() and Pred()). If helm is a block-level
	 * element, then will the range only consist of this element. If helm is an inline-level
	 * element, then will the range include consecutive inline elments before and after helm.
	 * @return Returns the range of siblings.
	 * @parm stop_at_br FIXME: remove this argument or change behaviour
	 * @parm stop_pred Stop scan when Pred() returns this element, and don't include it.
	 * @parm stop_suc Stop scan when Suc() returns this element, and don't include it.
	 */
	SiblingBlock			GetBlockSiblings(HTML_Element *helm, BOOL stop_at_br = FALSE, HTML_Element *stop_pred = NULL, HTML_Element *stop_suc = NULL);

	/**
	 * If the parent of the elements in block is of type HE_LI, HE_OL or HE_UL - then will
	 * this function remove "unnecessary" children of that parent that are not contained
	 * inside block.
	 * @param block The range of elements which should NOT be "cleaned".
	 */
	void					CleanAroundBlock(SiblingBlock block);

	/**
	 * This function is a help-function for GetNextListBlock.
	 * @return Returns a range of HTML elements which are "friends" immediately under root or immediately
	 * under a OL/UL element. The search is made in Depth-First-Search order and starts from block but
	 * does not include this block or any block below this block. An empty block is returned if no suitable
	 * blocks are found below the parent of block.
	 * @param root If the parent of block == root, then a block immediately under root could be returned
	 * even though root is not a OL/UL element.
	 * @param block The block to start the search from, trees that are successors of block will be traversed.
	 */
	SiblingBlock			GetSucListBlock(HTML_Element *root, SiblingBlock block);

	/**
	 * This function is used for iterating through "friend blocks" of HTML elements that are either
	 * children below root or chilren below OL/UL elements which root is an ancestor for.
	 * @return Returns a range of HTML elements which are "friends" immediately under root or immediately
	 * under a OL/UL element. The search is made in Depth-First-Search order and starts from block but
	 * does not include this block or any block below this block. An empty block is returned if no suitable
	 * blocks are found below root and after block in DFS order.
	 * @param root The root in the tree so search through.
	 * @param block The block to start the search from.
	 */
	SiblingBlock			GetNextListBlock(HTML_Element *root, SiblingBlock block);

	/**
	 * Moves the elements in block and appends them after after_me which is a child of new_parent.
	 * The old parent of block might be splitted in two parts and if the parent of the old parent
	 * is a LI element, this element might be splitted as well.
	 * @return OpStatus::OK if successful, otherwise some error...
	 * @param block The block to move away into new_parent.
	 * @param new_parent The new parent for block.
	 * @param after_me The new successor element of block when it's moved into new_parent. If after_me
	 * is NULL, block will be added first under new_parent.
	 * @split_parent The element under which the split occured.
	 * @split_pred The lowest child under split_parent that belongs to the "upper part" of the split.
	 */
	OP_STATUS				SplitListAroundAndMoveBlock(SiblingBlock block, HTML_Element *new_parent, HTML_Element *after_me, HTML_Element *&split_parent, HTML_Element *&split_pred);

	/**
	 * Moves the elements in block and appends them after after_me which is a child of new_parent.
	 * @param block The block to move away into new_parent.
	 * @param new_parent The new parent for block.
	 * @param after_me The new successor element of block when it's moved into new_parent. If after_me
	 * is NULL, block will be added first under new_parent.
	 */
	void					MoveSiblingBlock(SiblingBlock block, HTML_Element *new_parent, HTML_Element *after_me = NULL);

	/**
	 * Deletes HE_TEXT elements in block that is non-empty that contains only chars with values <= 32.
	 * @parm block The block to "clean", will be updated before returning if block.start or block.stop
	 * is "unnecessary".
	 * @save_at_least_one If true and all elements in block are "unnecessary", one element will remain anyway.
	 */
	void					CleanUnnecessaryElementsInBlock(SiblingBlock *block, BOOL save_at_least_one = FALSE);

	/**
	 * Adds all blocks >= start_block and <= stop_block as returned by GetNextListBlock to the same list.
	 * If start_block is already contained inside a list of the same type as ordered specifies, this
	 * list will be used. Otherwise a new list will be created.
	 * @return Error status code.
	 * @param shared_elm The shared containing element for start_block and stop_block (may infact just be
	 * an ancestor of this element because the most deeply nestled shared containing element might be split
	 * by this function).
	 * @param start_block First block for performing the operation on.
	 * @param stop_block Last block for performing the operation on.
	 * @parm ordered Should the list be ordered (OL) or un-ordered (UL)
	 */
	OP_STATUS				InsertAndOrToggleListOrdering(HTML_Element *shared_elm, SiblingBlock start_block, SiblingBlock stop_block, BOOL ordered);

	/**
	 * Increases the nestling level of elements inside OL/UL lists that are selected, elements that are
	 * participating in any lists are unaffected.
	 * @return Error status code.
	 */
	OP_STATUS				IncreaseListNestling();

	/**
	 * Decreases the nestling level of elements inside OL/UL lists that are selected, elements that are
	 * participating in any lists are unaffected. LI elements that are not under a list anymore are
	 * "converted" to P elements.
	 * @return Error status code.
	 */
	OP_STATUS				DecreaseListNestling();

	/**
	 * Decreases the maximum nestling level by decreasing the list-nestling by one of list elements
	 * that have a nestling of max_nestling.
	 * @return Error status code.
	 * @param shared_elm The shared containing element for start_block and stop_block (may infact just be
	 * an ancestor of this element because the most deeply nestled shared containing element might be split
	 * by this function).
	 * @param start_block First block for performing the operation on.
	 * @param stop_block Last block for performing the operation on.
	 * @param max_nestling The maximum nestling that any list elements >= start_block and <= stop_block are
	 * assumed to have before this operation.
	 */
	OP_STATUS				DecreaseMaxListNestling(HTML_Element *shared_elm, SiblingBlock start_block, SiblingBlock stop_block, INT32 max_nestling);

	/**
	 * "Adjusts" start_elm and stop_elm so that if they're not block-elements will they be adjusted upwards in the tree
	 * so their ParentActual() will be a block-level element and PredActual() for start-elm and SucActual() for stop_elm
	 * will be either NULL or a block-level element.
	 * @parm start_elm The start element.
	 * @parm stop_elm The stop element.
	 */
	void					AdjustStartStopToBlocks(HTML_Element *&start_elm, HTML_Element *&stop_elm);

	/**
	 * "Adjusts" start_elm and stop_elm "forward" in the tree for start_elm and "backwards" for stop_elm so that
	 * both start_elm and stop_elm will be GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL.
	 * @parm start_elm The start element, might be NULL after return (then, stop_elm will also be NULL):
	 * @parm stop_elm The stop element, might be NULL after return (then, start_elm will also be NULL):
	 */
	void					AdjustStartStopToActual(HTML_Element *&start_elm, HTML_Element *&stop_elm);

	/**
	 * "Adjusts" start_elm and stop_elm "forward" in the tree for start_elm and "backwards" for stop_elm so that
	 * there are not any ancestors of types to start_elm and stop_elm.
	 * @parm start_elm The start element, might be NULL after return (then, stop_elm will also be NULL):
	 * @parm stop_elm The stop element, might be NULL after return (then, start_elm will also be NULL):
	 */
	void					AdjustStartStopNotBreakTypes(HTML_Element *&start_elm, HTML_Element *&stop_elm, HTML_ElementType *types);

	/**
	 * "Adjust" start_elm and stop_elm upwards in the tree as long as start_elm->PredActual() == NULL and
	 * stop_elm->SucActual() == NULL (sort of...).
	 * @return The shared containing element for start_elm and stop_elm, which is the same both before and after the call.
	 * @parm start_elm The start element.
	 * @parm stop_elm The stop element.
	 */
	HTML_Element*			AdjustStartStopCloserRoot(HTML_Element *&start_elm, HTML_Element *&stop_elm);

	/**
	 * Justifies the current selection by setting the ATTR_ALIGN attribute on "suitable" block-level
	 * elements, and, in general (special case for tables), removing it from all of these elements
	 * children. DIV elements are created for inline elements that are not under block-level parents.
	 * @param align How to justify the selection (CSS_VALUE_left, etc).
	 */
	void					JustifySelection(CSSValue align);

	/**
	 * Indents/Outdents the current selection by setting the inline CSS_PROPERTY_margin_left property on "suitable" block-level
	 * elements. DIV elements are created for inline elements that are not under block-level parents.
	 * @param px How many px to indent (positive value) or outdent (negative value), added to the previous left-margin.
	 */
	OP_STATUS				IndentSelection(int px);

	/** Detect the direction now from the current text. */
	void					AutodetectDirection();
public:

	/** Clears the list of "styles elements" that should be inserted when the user starts typing, see description of InsertStyle. */
	void					ClearPendingStyles();

	/** If lock == TRUE, ClearPendingStyles will do nothing. Use FALSE to unlock. Calls might be "nestled". */
	void LockPendingStyles(BOOL lock) { m_pending_styles_lock += lock ? 1 : -1; OP_ASSERT(m_pending_styles_lock >= 0); }

	/**
	 * This function is intended for doing all the work when the InsertOrderedList/InsertUnOrderedList
	 * commands are executed.
	 * @parm ordered Should be TRUE when InsertOrderedList is executed and FALSE when InsertUnOrderedList
	 * is executed.
	 */
	void					ToggleInsertList(BOOL ordered);

	/**	Returns TRUE is helm has block-level children */
	BOOL					HasBlockElementChildren(HTML_Element *helm);

	/**
	 * "Adjusts" start_elm, stop_elm, start_ofs and stop_ofs to an "editable subrange", this means that (start_elm, start_ofs) and
	 * (stop_elm, stop_ofs) will be valid caret-positions after return. That is, if such positions exists >= (start_elm, start_ofs)
	 * and <= (stop_elm, stop_ofs)
	 * @return TRUE if an editable subrange where found.
	 * @parm start_elm The start element to be adjusted.
	 * @parm stop_elm The stop element to be adjusted.
	 * @parm start_ofs The start offset to be adjusted.
	 * @parm stop_ofs The stop offset to be adjusted.
	 */
	BOOL					GetEditableSubrange(HTML_Element *&start_elm, HTML_Element *&stop_elm, int &start_ofs, int &stop_ofs);

	/**
	 * "Snaps" state.start_elm, state.stop_elm, state.start_ofs and state.stop_ofs so that state.start_elm and state.stop_elm
	 * are GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL, state are supposed no NOT be empty.
	 * @return TRUE if state is still valid upon return and not empty.
	 * @parm state The SelectionState to adjust.
	 */
	BOOL ActualizeSelectionState(SelectionState &state);

	/** Returns the current start_elm+stop_elm+start_ofs+stop_ofs if the current selection and de-selects
	    if there where no selection the caret-position will be reflected in the result instead. */
	SelectionState			GetSelectionState(BOOL de_select = TRUE, BOOL de_caret = FALSE, BOOL require_editable = TRUE, BOOL actual_only = FALSE);

	/** Restores selection according to the SelectionState argument */
	void					RestoreSelectionState(SelectionState state);

	/**
	 * "Adjusts" helm and ofs to a later text element in the tree if ofs > helm->GetTextContentLength(). This is usefull
	 * when helm has been splitted so that ofs is not within the element anymore and we wants to find the corresponing
	 * position in another text element.
	 * @return TRUE if a matching helm+ofs where found.
	 * @parm helm An HE_TEXT element.
	 * @parm ofs The offset.
	 */
	BOOL					AdjustTextOfs(HTML_Element *&helm, int &ofs);

	/**
	 * Removes block-level elements like H3, BLOCKQUOTE, P, etc - from the current selection.
	 * @parm types An array of the kind of elements to be removed, terminated by HE_UNKNOWN.
	 * @parm attr If attr != ATTR_NULL, then will only elements with this attribute be removed.
	 * @parm just_one_level If TRUE -> just remove one nestling-level. For example, if HE_BLOCKQUOTE
	 * should be removed and one such element contains another such element, then only the first
	 * (ancestor) element will be removed.
	 * @parm exclude_at_bottom_type If TRUE, don't remove elements with no block-level children.
	 * This is sort of a hack for implementing the FormatBlock command by first inserting new blocks
	 * and then removing the old ones, the old blocks will then (if they contained TEXT/BR elements)
	 * contain newly created block elements, while the newly created elements will not contain any
	 * block-level elements.
	 */
	void					RemoveBlockTypes(HTML_ElementType *types, short attr = ATTR_NULL, BOOL just_one_level = FALSE, HTML_ElementType exclude_at_bottom_type = (HTML_ElementType)0);

	/**
	 * Inserts copies of the block-level element helm over all TEXT/BR elements (and their
	 * inline-level ancestors). If only_over_cursor==TRUE however, helm itself will be inserted over the
	 * cursor-position, and helm will NOT be deleted - which it otherwise will.
	 * @return Error status code.
	 * @parm helm A block-level element to possibly create multiple duplicates of for inserting. helm
	 * will always be deleted before this function returns.
	 * @parm exclude_lists Don't inser elements at any place below OL/UL elements.
	 * @parm only_over_cursor If TRUE, helm itself (not a copy) will be inserted over the current cursor-
	 * position - and helm will not be deleted;
	 * @parm dont_insert_where_already_exists Dont insert elements when there is already an element of that type over
	 * the elements inside that block, and that already-existing element isn't ancestor of other elements.
	 * @parm will_be_removed_types This block-types are about to be removed. Helps this function to determine whether
	 * a block of elements are already "covered" if dont_insert_where_already_exists==TRUE.
	 */
	OP_STATUS				InsertBlockElementMultiple(HTML_Element *helm, BOOL exclude_lists = FALSE, BOOL only_over_cursor = FALSE, BOOL dont_insert_where_already_exists = FALSE, HTML_ElementType *will_be_removed_types = NULL);

	/**
	 * Returns whether if the selection contains an matching block level element or if it contains elements
	 * which have such ancestors.
	 * @return TRUE if such element exists.
	 * @parm type The type to find...
	 * @parm attr If attr != ATTR_NULL, then an element must have this attribute to be consider as a match.
	 */
	BOOL					GetHasBlockType(HTML_ElementType type, short attr = ATTR_NULL);

	/**
	 * Checks whether the current selection contains any block-level elements of the types specified
	 * by the types arrray.
	 * @return TRUE if there are any elements of the types in types in the selection.
	 * @parm types An array of the types to check for, terminated by HE_UNKNOWN.
	 * @parm attr If attr != ATT_NULL -> defines an attribute that must be present in the elements
	 * matching the types in types in order to consider it as a match.
	 */
	BOOL					GetHasBlockTypes(HTML_ElementType *types, short attr = ATTR_NULL);

	/**
	 * Applies a function on all selected elements and returns whether all/some/at least one of the calls
	 * returns TRUE.
	 * Pseudo-code example:
	 *
	 * BOOL OpDocumentEdit::IsAllTextBold() { // returns FALSE if no HE_TEXT elements are selected
	 *	return GetSelectionMatchesFunction(&HasElmType,(void*)HE_BOLD,&HasElmType,(void*)HE_TEXT,TRUE,FALSE);
	 * }
	 *
	 * @return TRUE if all/some/at least one of the calls to func applied on the elements that are selected returns TRUE.
	 * @parm func A function to apply on selected elements that returns an boolean.
	 * @parm func_arg An argument that will be passed along with the html element to func upon each call.
	 * @must_match If all_must_match==TRUE and func returns FALSE for an element, then GetSelectionMatchesFunction
	 * might ANYWAY return TRUE. This is a way for "relaxing" all_must_match a bit. May be NULL, and MUST be NULL
	 * if all_ancestors_must_match==TRUE.
	 * @parm must_match_arg An argument that will be passed along with the html element to must_match upon each call.
	 * @parm all_must_match If TRUE, func must return TRUE for all elements (at least one) that matches must_match.
	 * @parm all_ancestors_must_match ALL elements (except of body and above) that are selected including elements who's
	 * children are selected must be returned TRUE by func.
	 */
	BOOL GetSelectionMatchesFunction(BOOL (*func)(HTML_Element*,void*), void *func_arg,	BOOL (*must_match)(HTML_Element*,void*), void *must_match_arg, BOOL all_must_match, BOOL all_ancestors_must_match);

	/**
	 * Calls function func for all elements between start_elm and stop_elm including all ancestors up to and including the
	 * editable root (body for designMode or the contenteditable root).
	 * @return Error status code, if any of the calls to func returns an error, this error will be returned (immedietly).
	 * @parm start_elm The first element to make the call for.
	 * @parm stop_elm The last element to make the call for.
	 * @parm func The function to call for each element: func(element, func_arg, return_value).
	 * @parm call_count If != NULL, will record how many calls that where made (one for each affected argument).
	 * @parm true_count If != NULL, will record how many of the calls that where returning TRUE in it's return-value argument.
	 * @parm only_actual If TRUE will only elements with GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL be processed.
	 */
	OP_STATUS ApplyFunctionBetween(HTML_Element *start_elm, HTML_Element *stop_elm, OP_STATUS (*func)(HTML_Element*,void*,BOOL&), void *func_arg, int *call_count = NULL, int *true_count = NULL, BOOL only_actual = TRUE);

	/**
	 * "Adjusts" the current selection (or caret-position) "backward" for start and "forward" to stop so that start
	 * is the first element on a line (first in block or after BR) and stop is the last on a line.
	 * @parm start Where the start element should be returned.
	 * @parm stop Where the stop element should be returned.
	 */
	void					GetBlockStartStop(HTML_Element** start, HTML_Element** stop);

	/** Has the same effect as GetBlockStartStop but start the search from a single element (like when there are no
	    selection but just the caret for GetBlockStartStop), see description of GetBlockStartStop */
	void					GetBlockStartStopInternal(HTML_Element** start, HTML_Element** stop, HTML_Element* from);

	/** Checks if helm is of type type and has attr attr.
		If type is HE_ANY, any friendly element-type matches.
		If attr is ATTR_NULL there will be no check for any attr. */
	BOOL					IsMatchingStyle(HTML_Element* helm, HTML_ElementType type, short attr);

	/** Checks if helm has any of the types in the HE_UNKNOWN-terminated array types, and if attr!=ATTR_NULL - if
	    that attribute is present. */
	BOOL					IsAnyOfTypes(HTML_Element *helm, HTML_ElementType *types, short attr = ATTR_NULL);

	/** Returns the ancestor of helm closest to the root matching any of the types in the HE_UNKNOWN-terminated array types, and
	    if attr != ATTR_NULL, also has attribute attr. */
	HTML_Element*			GetRootOfTypes(HTML_Element *helm, HTML_ElementType *types, short attr = ATTR_NULL);

	/** Will make sure the element that matches type&attr is removed from the current selection. It will create new branches when needed, to
		preserve the style on surrounding content. If just_one_level==TRUE, then will only one nestling level of style elements be removed,
		this is useful for e.g. decreasing font-size where just one level of BIG elements should be removed */
	void					RemoveStyle(HTML_ElementType type, short attr = ATTR_NULL, BOOL just_one_level = FALSE);

	/* Rreturn TRUE if any pending style elements (in m_pending_styles) is of type type */
	BOOL GetHasPendingStyle(HTML_ElementType type, short attr = 0);

	/** Return TRUE if the current selection or the insertpoint has parents that matches type&attr.
		It will search parents up to nearest containing element. If all_must_have_style==TRUE, then
		must all TEXT and BR elements have the matching style for TRUE to be returned. */
	BOOL					GetHasStyle(HTML_ElementType type, short attr = ATTR_NULL, BOOL all_must_have_style = FALSE, BOOL include_pending_styles = TRUE);

	/** Like GetHasStyle but only checks the ancestors of helm instead of all selection. */
	BOOL					GetHasStyleInternal(HTML_Element* helm, HTML_ElementType type, short attr);

	/** Returns the ancestor for helm that matches type and attr (or only type if attr == ATTR_NULL) */
	HTML_Element*			GetStyleElementInternal(HTML_Element* helm, HTML_ElementType type, short attr);

	/** Checks if any ancestor of helm is matching any of the types defined in the HE_UNKNOWN-terminated array types and
	    has attribute attr (if attr != ATTR_NULL, then this doesn't need to be true). */
	BOOL					GetHasBlockTypesInternal(HTML_Element *helm, HTML_ElementType *types, short attr);

	/** Get the top-most parent of type type */
	HTML_Element*			GetTopMostParentOfType(HTML_Element *helm, HTML_ElementType type);

	/** Get the fontsize at the current location of the caret or 0 if not set. */
	short					GetFontSize();

	/** Get the fontface at the current location of the caret or NULL if not set. */
	const uni_char*				GetFontFace();

	/** Get the fontface at the current location of the caret or NULL if not set.
		The output may include quotes and multiple font names separated with comma. */
	OP_STATUS					GetFontFace(TempBuffer &output);

	/** Inserts linebreak at current caret position. If break_list is TRUE, new listitems may be created or deleted
		depending of where the break is inserted.
		if new_paragraph is TRUE a new paragraph is created if apropriate (not in list etc.), Otherwise a BR will be created. */
	void					InsertBreak(BOOL break_list = TRUE, BOOL new_paragraph = FALSE);

	/** Inserts helm at current caret position. */
	void					InsertElement(HTML_Element* helm);

	/** Clone the list of pending style elements into head */
	OP_STATUS				ClonePendingStyles(Head &head);

	/** Add the style elements in head to the pending list (and remove them from head) */
	void					AddPendingStyles(Head &head);

	/** Inserts pending style elements at current caret position.
	 *
	 * @param outmost_element Set to point to the outmost inserted element. May be NULL.
	 */
	OP_STATUS				InsertPendingStyles(HTML_Element** outmost_element = NULL);

	/** Insert text at current caret position. If allow_append is TRUE, the text may be appended to the last event in the
	 * undo&redo-stack. (But it only will, if it follows the last event precisely.)
	 * If delete_selection is FALSE the current selection (if any) won't be deleted.
	 */
	OP_STATUS				InsertText(const uni_char* text, INT32 len,
										BOOL allow_append = FALSE, BOOL delete_selection = TRUE);

	/** Insert HTML-text at current caret position. It will skip eventual bodytag and only take the content.
		If specified, result_start and result_stop will be set to the first and last child (Not last leaf!) that is inserted.
		The new html can split the current branch up to split_root (or ContainingElement if not specified). */
	OP_STATUS				InsertTextHTML(const uni_char* text, INT32 len,
											HTML_Element** result_start = NULL,
											HTML_Element** result_stop = NULL,
											HTML_Element* split_root = NULL,
											TIDY_LEVEL tidy_level = TIDY_LEVEL_NORMAL,
											BOOL delete_selection = TRUE);

	/** Converts newlinecodes in text into <BR> elements. Use InsertText instead, since it will call this function if needed */
	OP_STATUS				InsertTextWithLinebreaks(const uni_char* text, INT32 len,
										BOOL allow_append = FALSE);

	/** Delete selected content or element active in layoutmodifier. */
	void					DeleteSelectedContent(BOOL aggressive = FALSE);

	/** Returns true if helm is on an empty line, that is, if it only exists one valid caret-position. */
	BOOL					LineIsEmpty(HTML_Element *helm);

	/** Returns TRUE if this containing_element needs to contain a BR element to not collapse.
		If ignore_current_auto_inserted_br is TRUE, any existing automatically inserted BR will be ignored. */
	BOOL					NeedAutoInsertedBR(HTML_Element *containing_element, BOOL ignore_current_auto_inserted_br = FALSE);
	/** Return TRUE if this containing_element contains a automatically inserted br. */
	BOOL					HasAutoInsertedBR(HTML_Element *containing_element);

	/**
	 * Returns if buf[0] (buf is supposed to be a pointer somewhere into a string represent helm's text content),
	 * would collapse if it's a collapsable space, NOT considering that it might be collapsed anyway due to the
	 * previous content.
	 * @return TRUE if the char would collapse due to being at the end of a line.
	 * @parm buf Supposed to be a pointer somewhere into a string represent helm's text content.
	 * @helm Supposed to be an HE_TEXT element.
	 */
	BOOL					WillEndLineCollapseIfWs(uni_char *buf, HTML_Element *helm);

	/** Reflows the document immediately. */
	void					ReflowAndUpdate();

	/** Scroll to ensure caret is visible. */
	void					ScrollIfNeeded();

	/** Let's the memory-manager rise an OOM condition. */
	void					ReportOOM();

	/** Let's the memory-manager rise an OOM condition if status == ERR_NO_MEMORY. */
	void					ReportIfOOM(OP_STATUS status);

	/** Check if the logical tree has changed (F.ex. after DOM has removed or added elements). If this has
		happened we have to update the caret and the undoredostack. */
	void					CheckLogTreeChanged(BOOL caused_by_user = FALSE);

	/** Called after reflow so that the caret's visual position could be updated. */
	virtual void			OnReflow();

	/** Called when the document get focused or unfocused, starts or stops caret-blinking, and if focus == TRUE, the carets editable container will be focused */
	virtual void			OnFocus(BOOL focus, FOCUS_REASON reason);

	/** Should be called when a element is inserted in the editable document. */
	void					OnElementInserted(HTML_Element* elm);

	/** Should be called when an element is removed by HTML_Element::OutSafe, when it's STILL in the tree */
	void					OnBeforeElementOut(HTML_Element* elm);

	/** Should be called before an element change e.g. its attributes */
	void					OnElementChange(HTML_Element* elm);

	/** Should be called after an element has been changed e.g. its attributes */
	void					OnElementChanged(HTML_Element* elm);

	/** Returns TRUE if OnBeforeElementOut is currently executing with elm as it's argument (see above). */
	BOOL					IsBeforeOutElm(HTML_Element *elm);

	BOOL					IsInBeforeElementOut() { return !m_before_out_elements.Empty(); }

	/** Should be called when a element is removed from the editable document. */
	void					OnElementRemoved(HTML_Element *elm);

	/** Should be called when a element is deleted. */
	void					OnElementDeleted(HTML_Element *elm);

	/**
	 * Internal docxs function that must be called when the parser or something else causes
	 * an HE_TEXT element to be converted into an HE_TEXTGROUP element.
	 *
	 * @param elm The element that used to be an HE_TEXT but is now an HE_TEXTGROUP.
	 *
	 * @see HTML_Element::AppendText().
	 */
	void					OnTextConvertedToTextGroup(HTML_Element* elm);

	void					OnTextChange(HTML_Element *elm);
	void					OnTextChanged(HTML_Element *elm);

	void					OnTextElmGetsLayoutWords(HTML_Element *elm);

	/** Should be called when scalefactor has been changed, to update caret position. */
	void					OnScaleChanged();

	/** Should be called when layout has been moved without reflow (F.ex. when a ScrollableContainer is scrolled) */
	void					OnLayoutMoved();

#ifdef SUPPORT_TEXT_DIRECTION
	/** Set direction of current editable container. Returns TRUE if it was changed, FALSE if it already had the given direction. */
	BOOL					SetRTL(bool is_rtl);
#endif
	BOOL					GetRTL() { return m_caret.GetCaretManager()->GetRTL(GetEditableContainer(m_caret.GetElement())); }

#ifdef DOCUMENTEDIT_AUTODETECT_DIRECTION
	/** Enable or Disable automatic detection of direction change. Direction change is signaled to OpTCListener::TCOnRTLDetected */
	void					SetAutodetectDirection(BOOL autodetect) { m_autodetect_direction = autodetect; }
#endif

#ifdef DOCUMENTEDIT_SPLIT_BLOCKQUOTE
	/** Enable or Disable split of blockquotes on insert of break */
	void					SetSplitBlockquote(BOOL split) { m_blockquote_split = split; }
#endif

#ifdef DOCUMENTEDIT_PLAIN_TEXT_MODE
	/** Enable or Disable plain text mode (disables formatting shortcuts and formatted text pasting) */
	void					SetPlainTextMode(BOOL enable) { m_plain_text_mode = enable; }
#endif

	/* Set if the tab-key should have any effect or not while editing. */
	void					SetWantsTab(BOOL status) { m_wants_tab = status; }

	/** Prepare root element for editing */
	void					InitEditableRoot(HTML_Element* root);

	/** Unactivate editing on this element */
	void					UninitEditableRoot(HTML_Element* root);

	/** Set focus to the editable element (element should be contentEditable) */
	void					FocusEditableRoot(HTML_Element* helm, FOCUS_REASON reason);

	/** Returns the FramesDocument associated with this OpDocumentEdit instance. */
	FramesDocument*			GetDoc() { return m_doc; }

	/* Returns TRUE if designMode is on for the document. */
	BOOL IsUsingDesignMode() { return m_body_is_root; }

	/** Begin something that will change element-tree in some way but shouldn't be recorded by the undostack.
		F.ex. a undo or redo event, or initiating stuff. Note, that all events (execpt undo+redo) that change
		the element-tree must use BeginChange to inform the undostack or clear it. */
	void Begin();
	void End();
	void Abort();

	/** Begin something that will change element-tree in some way.
		containing_elm MUST be a element that contains the entire change. As close in to the change as possible
		to save memory. If allow_append is TRUE, the change will be merged with the previous if they are next to
		each other.
		Tidy will be called with the aggressive_tidy flag. (Read more about it on Tidy)*/
	OP_STATUS BeginChange(HTML_Element* containing_elm, int flags = CHANGE_FLAGS_NONE);
	OP_STATUS EndChange(HTML_Element* containing_elm, TIDY_LEVEL tidy_level = TIDY_LEVEL_NORMAL);

	/** A more "customized" variant of EndChange that also defines the start- and stop-elements which should be used as arguments to Tidy() */
	OP_STATUS EndChange(HTML_Element* containing_elm, HTML_Element *start_elm, HTML_Element *stop_elm, BOOL include_start_stop, TIDY_LEVEL tidy_level = TIDY_LEVEL_NORMAL);

	void AbortChange();

	// == Input =======================
	void EditAction(OpInputAction* action);
	virtual BOOL OnInputAction(OpInputAction* action);
	virtual const char*     GetInputContextName() { return "Edit Widget"; }
	virtual void			OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void			OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual BOOL			GetBoundingRect(OpRect &rect);
#ifdef WIDGETS_IME_SUPPORT
# ifndef DOCUMENTEDIT_DISABLE_IME_SUPPORT
	virtual IM_WIDGETINFO OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose);
	virtual IM_WIDGETINFO OnCompose();
	virtual IM_WIDGETINFO GetWidgetInfo();
	virtual void OnCommitResult();
	virtual void OnStopComposing(BOOL cancel);
# endif
# ifdef IME_RECONVERT_SUPPORT
	virtual void OnPrepareReconvert(const uni_char*& str, int& sel_start, int& sel_stop);
	virtual void OnReconvertRange(int sel_start, int sel_stop) ;
# endif
#endif

	// == Misc. helpers ========================

	/** Sets the text-content of helm to text... */
	void SetElementText(HTML_Element* helm, const uni_char* text, int len = -1);

	/** Sets if the element is inserted by the documentedit code automatically (automatically inserted elements are removed automatically too in the Tidy function). */
	OP_STATUS SetInsertedAutomatically(HTML_Element* helm, BOOL inserted_automatically);
	BOOL GetInsertedAutomatically(HTML_Element* helm);

	OP_STATUS SetSpecialAttr(HTML_Element* helm, Markup::AttrType attr, BOOL value);
	BOOL GetSpecialAttr(HTML_Element* helm, Markup::AttrType attr);

	/** Is CSS_VALUE_pre or CSS_VALUE_pre_wrap used when layouting helm? */
	BOOL IsInPreFormatted(HTML_Element *helm);

	/** Use when user is inserting or removing text in a textelement. Removes or inserts nbsp or regular spaces.
		(If user types several spaces after each other, we have to make nbsp to not collapse them) */
	BOOL RemoveNBSPIfPossible(HTML_Element* helm);

	/** Returns if helm is first on a line or if the last character before is a collapsable whitespace. */
	BOOL IsBeforeCollapsedOrEdge(HTML_Element* helm);

	/** Returns if helm is last on a line or if the last character after is a collapsable whitespace. */
	BOOL IsAfterCollapsedOrEdge(HTML_Element* helm);

	/* Is helm an ending BR (there are content on the line before the BR)
	 *
	 * @param helm The BR element to check.
	 * @param helm_to_remove Element that is to be removed in a moment, it will not be considered as content.
	 */
	BOOL IsEndingBr(HTML_Element* helm, HTML_Element* helm_to_remove = NULL);

	/**
	 * Get the nearest valid caret position starting from from_helm.
	 * @return If such position was found.
	 * @parm from_helm The element to start the scan from.
	 * @parm nearest_helm The element that was found, if it was found.
	 * @parm nearest_ofs The offset that was found, if it was found.
	 * @parm forward TRUE if the logical tree should be scanned forward, else backward.
	 * @parm include_current TRUE if we should try with from_helm itself first.
	 * @parm helm_to_remove Element that is to be removed in a moment, it is not a valid position for the caret.
	 */
	BOOL GetNearestCaretPos(HTML_Element* from_helm, HTML_Element** nearest_helm, int* nearest_ofs, BOOL forward, BOOL include_current, HTML_Element* helm_to_remove = NULL);

	/**
	 * Finds the next valid caret-position starting from helm and ofs.
	 * @return If such position was found.
	 * @parm helm The element to start the scan from.
	 * @parm ofs The offset to start the scan from.
	 * @parm new_helm The element that was found, if it was found (might be helm itself).
	 * @parm new_ofs The offset that was found, if it was found.
	 * @parm snap If TRUE, tries to "snap" the result to an HE_TEXT element
	 * @parm accept_no_diff If TRUE, it's alright if the new position visually is on the same place as the old one,
	 * for example: hej|<b>du</b> -> hej<b>|du</b> where | represents the position.
	 */
	BOOL GetOneStepBeside(BOOL forward, HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs, BOOL snap = TRUE, BOOL accept_no_diff = FALSE) { return m_caret.GetCaretManager()->GetOneStepBeside(forward, helm, ofs, new_helm, new_ofs, snap, accept_no_diff); }

	/**
	 * Tries to find the "best" caret position starting from helm, somewhere close to helm hopefully.
	 * @return TRUE if such position was found.
	 * @parm helm The element to start the scan from, but excluding helm and it's children.
	 * @parm new_helm The element that was found, if it was found (will NOT be helm itself).
	 * @parm new_ofs The offset that was found, if it was found.
	 * @parm prefer_first If YES, give a hint that we would prefer an element before helm, or if NO that we prefer one after.
	 * @parm must_be_friends If TRUE, only valid caret-positions at elements that are "friends" (see IsFriends) is accepted.
	 * @parm helm_to_remove Element that is to be removed in a moment, it is not a valid position for the caret.
	 */
	BOOL GetBestCaretPosFrom(HTML_Element *helm, HTML_Element *&new_helm, int &new_ofs, BOOL3 prefer_first = MAYBE, BOOL must_be_friends = FALSE, HTML_Element *helm_to_remove = NULL);

	/**
	 * Works like GetBestCaretPosFrom but includes helm itself and it's possible child-elements to find a caret-position.
	 * If helm has no own caret-position but has children and ofs == 0 then the scan for caret position among it's children
	 * will start at helm->FirstChildActual() and go forward, if ofs >= 1 it will start from helm->LastLeafActual() and go
	 * backwards. If still no valid position has been found will GetBestCaretPosFrom be used.
	 * @returns TRUE if a valid caret-position was found.
	 * @parm helm The element to start the scan from, including helm and it's children.
	 * @parm new_helm The element that was found, if it was found.
	 * @parm new_ofs The offset that was found, if it was found.
	 */
	BOOL GetValidCaretPosFrom(HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs);

	/**
	 * Returns the last valid caret-offset in helm (consider white-space collapsing in text-elements).
	 * @return TRUE if such caret-offset was found in helm.
	 * @parm helm The element to perform the operation on...
	 * @parm ofs The resulting last valid caret-offset.
	 */
	BOOL GetLastValidCaretOfs(HTML_Element *helm, int &ofs);

	/**
	 * Returns the first valid caret-offset in helm (consider white-space collapsing in text-elements).
	 * @return TRUE if such caret-offset was found in helm.
	 * @parm helm The element to perform the operation on...
	 * @parm ofs The resulting first valid caret-offset.
	 */
	BOOL GetFirstValidCaretOfs(HTML_Element *helm, int &ofs);

	/**
	 * Finds an editable element starting from from_helm, that is, an elements which includes a valid caret-position.
	 * @return The editable element that was found, or NULL if no such element was found.
	 * @parm forward If TRUE, searches forward in the logical tree, else backwards.
	 * @parm include_current If TRUE, from_helm itself will be the first candidate scanned.
	 * @parm require_box Only return elements with a layout box.
	 * @parm include_ending_br If TRUE may an ending BR be returned, that is, a BR that ends a line with previous valid caret-positions.
	 * @parm helm_to_remove Element that is to be removed in a moment, it is not a valid position for the caret and should be omitted
	 * during the search
	 */
	HTML_Element* FindEditableElement(HTML_Element* from_helm, BOOL forward, BOOL include_current, BOOL require_box, BOOL include_ending_br = FALSE, HTML_Element* helm_to_remove = NULL);

	/**
	 * Finds the first element previous to helm that is of type type.
	 * @return Such element, or NULL if not found.
	 * @parm type The type to find.
	 * @parm require_box If TRUE, only returns an element with a layout box.
	 */
	HTML_Element* FindElementBeforeOfType(HTML_Element* helm, HTML_ElementType type, BOOL require_box = FALSE);

	/**
	 * Finds the first element next to helm that is of type type.
	 * @return Such element, or NULL if not found.
	 * @parm type The type to find.
	 * @parm require_box If TRUE, only returns an element with a layout box.
	 */
	HTML_Element* FindElementAfterOfType(HTML_Element* helm, HTML_ElementType type, BOOL require_box = FALSE);

	/**
	 * Makes sure the editable container container (that is, body for designMode or a content editable element) has
	 * a valid caret-position at the end or beginning. This means that if e.g. container ends with a HR element, then
	 * a new empty text-element will be inserted after the HR in order to permitt the user to put the caret under the
	 * HR. If no editing action has been performed when the user puts the caret somewhere else will the text-element
	 * be removed.
	 * @return If not NULL, a empty-text element that was created at the beginning or end of container.
	 * @parm helm If container == NULL and helm != NULL, then will container be GetEditableContainer for helm.
	 * @parm container The editable container, may be NULL if helm != NULL.
	 * @parm at_end If TRUE, possibly add empty text element at end of container, otherwise at the beginning.
	 */
	HTML_Element* MakeSureHasValidEdgeCaretPos(HTML_Element *helm, HTML_Element *container, BOOL at_end);

	/** Creates and returns a new HE_TEXT element with text-content the first len characters of text. */
	HTML_Element* NewTextElement(const uni_char* text, int len);

	/** Creates a new HTML_Element with type type and calls HTML_Element::SetEndTagFound on it. */
	HTML_Element* NewElement(HTML_ElementType type, BOOL set_automatic_flag = FALSE);

	/** Duplicates the element and returns the new duplicate. It does not include children. */
	HTML_Element* NewCopyOfElement(HTML_Element* helm, BOOL remove_id = FALSE);

	/** Returns TRUE if helm->GetTextContent()[ofs] is collapsed for HE_TEXT element helm. */
	BOOL IsCharCollapsed(HTML_Element *helm, int ofs);

	/**
	 * Deletes len charachters in the text-content of the HE_TEXT element helm starting from offset start_ofs.
	 * @return TRUE if text-content was deleted.
	 * @parm start_ofs The offset to start the deleting from.
	 * @len How many characters to delete.
	 */
	BOOL DeleteTextInElement(HTML_Element* helm, INT32 start_ofs, INT32 len);

	/** Returns TRUE if helm has no layout-box or if helm is a text-element with no valid caret-position. */
	BOOL IsCollapsed(HTML_Element* helm);

	/**
	 * Returns the next (if forward) or previous (if !forward) non-empty text-element from (but excluding) helm
	 * if that element starts (if forward) or ends (if !forward) with a "collapsable" space that is currently
	 * not collapsed, if this is not the situation - NULL is returned.
	 */
	HTML_Element *GetEdgeHelmMightWsCollapse(HTML_Element *helm, BOOL forward);

	/** Split a (text)element in 2 if ofs is not 0 or at the last caracter. Returns TRUE if the split was done. */
	BOOL SplitElement(HTML_Element* helm, INT32 ofs);

	/** Is a element that doesn't have endtag. F.ex. text, br, hr.. */
	BOOL IsStandaloneElement(HTML_Element* helm) { return CaretManager::IsStandaloneElement(helm); }

	/** Returns TRUE if the element has replaced content */
	BOOL IsReplacedElement(HTML_Element *helm, BOOL must_have_replaced_content = FALSE) { return CaretManager::IsReplacedElement(helm, must_have_replaced_content); }

	void AddInternalEventListener(OpDocumentEditInternalEventListener *listener) { if(listener){listener->Out();listener->Into(&m_internal_event_listeners);}}
	void RemoveInternalEventListener(OpDocumentEditInternalEventListener *listener) { if(listener)listener->Out(); }

	/**
	 * Adds a "whitespace preserving operation", this means that the caret might be put at text-elements involved in the
	 * operation even though they currently only have collapsed whitespaces as they will "soon" (when preserver->WsPreserve
	 * is called) will have un-collapsed whitespaces.
	 * @parm preserver The OpDocumentEditWsPreserver that will later be used to preserve the collapse-state of the
	 * possible whitespaces before and after the range that has been marked upon construction of preserver.
	 */
	void AddWsPreservingOperation(OpDocumentEditWsPreserver *preserver);

	/** Removes preserver from the list of current "whitespace preserving operations", see AddWsPreservingOperation. */
	void RemoveWsPreservingOperation(OpDocumentEditWsPreserver *preserver);

	/**
	 * Returns whether helm is in a "whitespace preserving operation" (see AddWsPreservingOperation).
	 * @return TRUE if helm is in such an operation + some more constraints, see was_collapsed below.
	 * @parm The element...
	 * @was_collapsed If was_collapsed == MAYBE, return if helm is in whitespace preserving operation. If
	 * was_collapsed == YES, only return TRUE if the "preserved" whitespace was collapsed. if was_collapsed == NO,
	 * only return TRUE if the "preserved" whitespace was un-collapsed. The current collapsing-state might differ
	 * from the original, but the original will be resored upon calling OpDocumentEditWsPreserver::WsPreserve.
	 */
	BOOL IsInWsPreservingOperation(HTML_Element *helm, BOOL3 was_collapsed = MAYBE);

	/**
	 * Is element valid for putting the caret on?
	 * @return TRUE if there are at least one valid caret-position inside helm.
	 * @parm helm The element to perform the check on...
	 * @parm is_in_tree If FALSE, helm is not supposed to be in the document's logical tree which implies that
	 * helm doesn't need to have an editable container and that no check for if helm is collapsed occurs.
	 * @parm helm_to_remove Element that is to be removed in a moment, it is not a valid position for the caret.
	 */
	BOOL IsElementValidForCaret(HTML_Element *helm, BOOL is_in_tree = TRUE, BOOL ending_br_is_ok = FALSE, BOOL valid_if_possible = FALSE, HTML_Element* helm_to_remove = NULL);

	/** Returns TRUE if helm is a block-level element. FIXME: not quite accurate, it currently only checks helm's type. */
	BOOL IsBlockElement(HTML_Element *helm) { return CaretManager::IsBlockElement(m_doc, helm); }

	/** Is a element which childs is treated as if in the same string as other childs in a friendly element.
		Basically all inlineelements, with some exceptions if include_replaced is FALSE.
		Use to determine if the caret should move right into it or require a extra keypress. F.ex. a, b, u */
	BOOL IsFriendlyElement(HTML_Element* helm, BOOL include_replaced = FALSE, BOOL br_is_friendly = FALSE, BOOL non_editable_is_friendly = FALSE);

	/** Checks if helm_a and helm_b is friends. (Has the same "unfriendly" parent) */
	BOOL IsFriends(HTML_Element* helm_a, HTML_Element* helm_b, BOOL include_helm_b = TRUE, BOOL include_replaced = FALSE, BOOL br_is_friendly = FALSE);

	/** Just a wrapper around Box::GetBBox... */
	BOOL GetContentRectOfElement(HTML_Element *helm, AffinePos &ctm, RECT &rect, BoxRectType type = CONTENT_BOX);

	/** If the element should be kept when tidying (all conditions. even if it's fully enclosed by the selection start and stop) */
	BOOL KeepWhenTidy(HTML_Element* helm);

	/** If the element should be kept when tidying (even if no childelements. but remove if fully enclosed by the selection start and stop) */
	BOOL KeepWhenTidyIfEmpty(HTML_Element* helm);

	/** If the element can have text content */
	BOOL IsNoTextContainer(HTML_Element* helm);

	/** If helm and all its children is enclosed by start and stop. */
	BOOL IsEnclosedBy(HTML_Element* helm, HTML_Element* start, HTML_Element* stop);

	/** If there is any textelements between start and stop (not including start and stop) */
	BOOL ContainsTextBetween(HTML_Element* start, HTML_Element* stop);

	/**
	 * Returns a TextSelectionPoint representing the (caret-position helm, character_offset), does currently NOT
	 * perform any conversion to DOM-style semantics with parent and child-offset.
	 * @return the OldStyleTextSelectionPoint...
	 * @parm character_offset The "element character offset", might be "snapped" to a valid value.
	 * @parm prefer_first FIXME: This is argument is not used anymore, remove it???
	 */
	OldStyleTextSelectionPoint GetTextSelectionPoint(HTML_Element* helm, int character_offset, BOOL prefer_first = FALSE);

	/**
	 * Stores helm and it's child-tree as text-html representation in text.
	 * @return Error status code...
	 * @parm text The place to store the result.
	 * @parm helm The element!
	 * @parm include_helm If FALSE, only the content of helm's child-tree will be "textualized" and NOT helm itself.
	 */
	OP_STATUS GetTextHTMLFromElement(OpString& text, HTML_Element* helm, BOOL include_helm);

	/** Delete helm in a safe manner */
	void DeleteElement(HTML_Element* helm, BOOL check_caret = TRUE) { DeleteElement(helm, this, check_caret); }
	static void DeleteElement(HTML_Element* helm, OpDocumentEdit* edit, BOOL check_caret = TRUE);

	/**
	 * Stores helm and it's child-tree as text-html representation in text.
	 *
	 * @param[out] text The place to store the result.
	 * @param[in]  element_name name of the helm!
	 * @param[in]  include_helm If FALSE, only the content of helm's child-tree will be "textualized" and NOT helm itself.
	 *
	 * @return OpStatus::OK if found, OpStatus::ERR if element not found or out of memory
	 */
	OP_STATUS GetTextHTMLFromNamedElement(OpString& text, const uni_char* element_name, BOOL include_helm);

	/**
	 * Delete a named helm in a safe manner and return TRUE if it succeeded finding and deleting it.
	 *
	 * NOTE: This method doesn't submit changes to the undo/redo stack, so any existing events in
	 * the undo/redo stack will be cleared when this is called!
	 */
	BOOL DeleteNamedElement(const uni_char* element_name);

	/** Find top-most parent that is content-editable.
	 *
	 * Returns NULL if not found.
	 */
	HTML_Element* GetTopEditableParent(HTML_Element* element);

	/** Returns the closest containing element that contains both elm1 and elm2 */
	HTML_Element* GetSharedContainingElement(HTML_Element* elm1, HTML_Element* elm2);

	/** Returns the root of the logical document. */
	HTML_Element* GetRoot();

	/** Returns the document's body element, if not present the HE_HTML element will be returned,
	    if it doesn't exists either, the logical document's root will be returned. */
	HTML_Element* GetBody();

	/** Returns the editable container for helm, that is, body when designMode is on or the nearest
	    enclosing content editable element otherwise.
	    The returned container is never outside of the body element, e.g. in case when the html
	    element is contentEditable the body element is returned by this function
	    (provided that there is one). */
	HTML_Element* GetEditableContainer(HTML_Element* helm);

	/** Returns the editable container for helm, that is, body when designMode is on or the nearest
	    enclosing content editable element otherwise. If helm is outside of body, but is still
		within an editing host, body will be returned as the container.
	    The returned container is never outside of the body element, e.g. in case when the html
	    element is contentEditable the body element is returned by this function
	    (provided that there is one). */
	HTML_Element* GetFocusableEditableContainer(HTML_Element* helm);

	/** Returns an ancestor of helm (or helm itself) which is suitable for inserting an HE_HR element below (e.g. usually NOT an HE_P element) */
	HTML_Element* GetHrContainer(HTML_Element *helm);

	/** Remove garbage elements. It won't remove elements returned by KeepWhenTidy.
		If aggressive_tidy is TRUE it will remove empty textelements and their parents if
		they become empty. It will never touch any elements above start_helm and stop_helm. */
	void Tidy(HTML_Element* start_helm, HTML_Element* stop_helm, BOOL include_start_stop, TIDY_LEVEL tidy_level = TIDY_LEVEL_NORMAL, BOOL keep_dummy = FALSE, HTML_Element *shared_containing_elm = NULL);
public:
	FramesDocument*			m_doc;
	OpDocumentEditCaret		m_caret;
	OpDocumentEditSelection m_selection;
	Head m_internal_event_listeners;
	OpDocumentEditUndoRedoStack m_undo_stack;
	OpDocumentEditLayoutModifier m_layout_modifier;

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	OpSpellCheckerSession *m_spell_session;
	BOOL m_has_spellchecked;
	HTML_WordIterator m_word_iterator;
	HTML_WordIterator m_replace_word;
	HTML_WordIterator m_background_updater;
	HTML_Element *m_pending_spell_first, *m_pending_spell_last;
	WordInfo *m_delay_misspell_word_info;
	BOOL m_doc_has_changed;
	HTML_Element *m_last_helm_spelled;
	int m_next_wi_index_to_spell;
	BOOL m_last_helm_spelled_needs_update;
	BOOL m_enable_spellcheck_later;
	BOOL m_blocking_spellcheck;
	BOOL m_by_user;
#endif // INTERNAL_SPELLCHECK_SUPPORT

	int m_pending_styles_lock;
	Head m_pending_styles;
	Head m_ws_preservers;
	Head m_before_out_elements;
	INT32 m_begin_count;
	BOOL m_logtree_changed;
	BOOL m_usecss;
	BOOL m_readonly;
	BOOL m_wants_tab;
	BOOL m_body_is_root;
	BOOL m_autodetect_direction;
	BOOL m_blockquote_split;
	BOOL m_plain_text_mode;

#ifdef WIDGETS_IME_SUPPORT
# ifndef DOCUMENTEDIT_DISABLE_IME_SUPPORT
	IM_WIDGETINFO GetIMInfo();
	HTML_Element* BuildInputMethodStringElement();
	void EmptyInputMethodStringElement(BOOL remove=FALSE);
	BOOL IsImComposing()		{ return m_ime_string_elm != NULL; }
	const OpInputMethodString* m_imstring;
	HTML_Element* m_ime_string_elm;
	BOOL im_waiting_first_compose;
# endif
#endif

private:
	OP_STATUS InsertPlainText(const uni_char *text, INT32 len);
	// Temporary storage of selection-points and caret element.
	OldStyleTextSelectionPoint m_start_sel_copy;
	OldStyleTextSelectionPoint m_stop_sel_copy;
	HTML_Element *m_caret_elm_copy;

	HTML_Element *m_content_pending_helm;	///< Content is about to be added to this element (Set during a insert operation before it's inserted)

	OpDocumentEditListener*
					m_listener;
#ifdef _DOCEDIT_DEBUG
	BOOL			m_random_seed_initialized;
	OpDocumentEdit *m_edit;
#endif // _DOCEDIT_DEBUG

	BOOL m_recreate_caret;

	HTML_ElementType m_paragraph_element_type; ///< The element to use for paragraphs (e.g. HE_P or HE_DIV). Should be a block element.

	/** The flag remembering if OpDocumentEdit is being focused atm.
	 *  It's needed because sometimes if <html contenteditable> is being
	 *  focused we'll focus <body> anyway to prevent going out of <body>
	 *  in editable documents/elements. Since <body> is set as the focused
	 *  element in a document while focusing if <html contenteditable> was
	 *  set as such before the focus will be tried to be cleared and it has
	 *  to be prevented.
	 */
	BOOL m_is_focusing;
};

/** The document edit specific element reference. It protects HTML elements being currently used against Tidy(). */
class DoceditRef : public ElementRef
{
public:
	virtual BOOL IsA(ElementRefType type) { return type == ElementRef::DOCEDITELM; }
};

#endif // DOCUMENT_EDIT_SUPPORT

#endif // OP_DOCUMENTEDIT_H
