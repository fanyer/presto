/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef CARET_MANAGER_H
#define CARET_MANAGER_H

#if defined(KEYBOARD_SELECTION_SUPPORT) || defined(DOCUMENT_EDIT_SUPPORT)

#include "modules/display/vis_dev_transform.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/elementref.h"
#include "modules/layout/wordinfoiterator.h"
#include "modules/windowcommander/OpWindowCommander.h"

class FramesDocument;
class OpDocumentEdit;

/**
 * This class handles caret movements in editable document or in documents
 * with keyboard selection enabled. The caret is really the focus point of
 * the document selection but for historical reasons we work on an
 * older representation. The "old style" selection which has a slightly different
 * representation. In the "old style" points in the document is either
 * text node + offset (same as the standard representation) or element (typically
 * a leaf element) with offset 0 or 1. Offset 0 means "before the element" and
 * offset 1 means "after the element".
 */
class CaretManager
{
public:
	/**
	 * Constructor. Only to be used by FramesDocument.
	 * @param frames_doc The FramesDocument owning this CaretManager.
	 */
	explicit CaretManager(FramesDocument *frames_doc) :
		frames_doc(frames_doc)
	{}

	/**
	 * The caret is the same as the focus point in the selection and that can
	 * be represented in at least two different ways.
	 * The "standard" way of representing it is as container+offset, but
	 * the Opera traditional way is to represent it as
	 * element+0/1 or text+offset. Newly written code should work on
	 * the "standard" model and just read the focus point from the current
	 * TextSelection in FramesDocument but for code that wants the traditional
	 * way, this will do the conversion.
	 *
	 * This is compatible with the expected behaviour of OpDocumentEditCaret::GetElement().
	 *
	 * @returns An element or NULL.
	 */
	HTML_Element* GetElement();

	/**
	 * The caret is the same as the focus point in the selection and that can
	 * be represented in at least two different ways.
	 * The "standard" way of representing it is as container+offset, but
	 * the Opera traditional way is to represent it as
	 * element+0/1 or text+offset. Newly written code should work on
	 * the "standard" model and just read the focus point from the current
	 * TextSelection in FramesDocument but for code that wants the traditional
	 * way, this will do the conversion.
	 *
	 * This is compatible with the expected behaviour of OpDocumentEditCaret::GetElement().
	 *
	 * @returns 0 or 1.
	 */
	int GetOffset();

	/**
	 * Returns whether a caret that has two visual positions (for instance
	 * in line wrapped text where the same logical position appear before
	 * and after the wrap) should be considered being at the end of the
	 * previous line (TRUE) or at the new line (FALSE).
	 */
	BOOL GetIsLineEnd();

	/**
	 * Tries to place the caret in the rectangle defined by
	 * document-coordinates (x1,y1),(x2,y2).
	 * @return TRUE If a "suitable" caret-position was found in the
	 * area and the caret was put on that position (at the matching
	 * logical position).
	 * @param x1 X1 in document-coordinates
	 * @param y1 Y1 in document-coordinates
	 * @param x2 X2 in document-coordinates
	 * @param y2 Y2 in document-coordinates
	 * @param remember_x If TRUE, will we try to keep the X-position
	 * when the user navigates spatial up/down or page-up/page-down.
	 * @param enter_all TextSelectionObject::SetEnterAll will be called
	 * with enter_all to determine if all lines should be entered in
	 * the search for a matching caret-position.
	 * @param selection_container_element Element that should be the
	 * container of the new position, or NULL of no such restriction
	 * should be made.
	 * @param same_pos_is_success If the caret is successfully placed
	 * on the given position and the position is the same as current,
	 * it will return TRUE only if same_pos_is_success is TRUE.
	 * @param search_whole_viewport This is passed to
	 * TextSelectionObject's ctor as its find_nearest parameter.  If
	 * it's TRUE the whole viewport is searched for a place suitable
	 * for the caret if the rectangle specified above doesn't contain
	 * a suitable pleace. This allows better placements of carets
	 * when a user clicks after the end of a line for instance.
	 * @see TextSelectionObject
	 */
	BOOL Place(const AffinePos &ctm, INT32 x1, INT32 y1, INT32 x2, INT32 y2, BOOL remember_x, BOOL enter_all = FALSE, HTML_Element *selection_container_element = NULL, BOOL same_pos_is_success = FALSE, BOOL search_whole_viewport = FALSE);

	/**
	 * Moves the caret in the way specified by the direction argument.
	 *
	 * @param place Any of the places specified in the enum
	 * OpWindowCommander::CaretMovementDirection (see OpWindowCommander::CaretMovementDirection...).
	 * @param containing_element The "paragraph" element for the move.
	 * @param root_element The boundary element for the move. In a fully
	 * accessible document that would typically be the body element.
	 */
	void MoveCaret(OpWindowCommander::CaretMovementDirection direction, HTML_Element *containing_element, HTML_Element *root_element);

	/**
	 * Tries to place the caret on document-coordinates x,y.
	 * @return TRUE If a "suitable" caret-position where found and the
	 * caret was put on that position (at the matching logical
	 * position).
	 * @param x X in document-coordinates
	 * @param y Y in document-coordinates
	 * @param remember_x If TRUE, will we try to keep the X-position
	 * when the user navigates spatial up/down or page-up/page-down.
	 * @param enter_all TextSelectionObject::SetEnterAll will be called
	 * with enter_all to determine if all lines should be entered in
	 * the search for a matching caret-position.
	 * @param search_whole_viewport This is passed to
	 * TextSelectionObject's ctor as its find_nearest parameter.  If
	 * it's TRUE the whole viewport is searched for a place suitable
	 * for the caret.
	 * @see TextSelectionObject
	 */
	BOOL PlaceAtCoordinates(const AffinePos &ctm, INT32 x, INT32 y, BOOL remember_x = TRUE, BOOL enter_all = FALSE, BOOL search_whole_viewport = FALSE);

	/**
	 * Works like GetBestCaretPosFrom but includes helm itself and its possible child-elements to find a caret-position.
	 * If helm has no own caret-position but has children and ofs == 0 then the scan for caret position among it's children
	 * will start at helm->FirstChildActual() and go forward, if ofs >= 1 it will start from helm->LastLeafActual() and go
	 * backwards. If still no valid position has been found will GetBestCaretPosFrom be used.
	 * @returns TRUE if a valid caret-position was found.
	 * @param doc The document with the element.
	 * @param helm The element to start the scan from, including helm and it's children.
	 * @param ofs The offset of element to use as a starting point of the scan.
	 * @param new_helm The element that was found, if it was found.
	 * @param new_ofs The offset that was found, if it was found.
	 */
	static BOOL GetValidCaretPosFrom(FramesDocument* doc, HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs);

	/**
	 * Tries to find the "best" caret position starting from helm, somewhere close to helm hopefully.
	 * @return TRUE if such position was found.
	 * @param helm The element to start the scan from, but excluding helm and it's children.
	 * @param new_helm The element that was found, if it was found (will NOT be helm itself).
	 * @param new_ofs The offset that was found, if it was found.
	 * @param prefer_first If YES, give a hint that we would prefer an element before helm, or if NO that we prefer one after.
	 * @param must_be_tightly_connected If TRUE, only valid caret-positions at elements that are "tightly connected" (see IsTightlyConnectedInlineElements) is accepted.
	 * @param helm_to_remove Element that is to be removed in a moment, it is not a valid position for the caret.
	 */
	static BOOL GetBestCaretPosFrom(FramesDocument *doc, HTML_Element *helm, HTML_Element *&new_helm, int &new_ofs, BOOL3 prefer_first = MAYBE, BOOL must_be_tightly_connected = FALSE, HTML_Element *helm_to_remove = NULL);

	/**
	 * Anyone that know how what this is trying to achieve should document it.
	 *
	 * The old documentation was "Is a element that doesn't have endtag. F.ex. text, br, hr.."
	* but that doesn't match how it's used.
	 */
	static BOOL IsStandaloneElement(HTML_Element* helm);

	/**
	 * Returns TRUE if helm is a block-level element.
	 *
	 * @todo Not quite accurate, it currently only checks helm's type.
	 */
	static BOOL IsBlockElement(FramesDocument* doc, HTML_Element *helm);

	/* Is helm an ending BR (there are content on the line before the BR)
	 *
	 * @param helm The BR element to check.
	 * @param helm_to_remove Element that is to be removed in a moment, it will not be considered as content.
	 */
	static BOOL IsEndingBr(FramesDocument* doc, HTML_Element* helm, HTML_Element* helm_to_remove);

	/**
	 * Is element inside a block that the page has marked as editable?
	 */
	static BOOL IsElementEditable(FramesDocument* doc, HTML_Element* helm);

	/**
	 * Returns TRUE if the element has replaced content, or is expected
	 * to have replaced content whenever it gets a layout box.
	 */
	static BOOL IsReplacedElement(HTML_Element *helm, BOOL must_have_replaced_content = FALSE);

	/**
	 * Is element valid for putting the caret on?
	 * @return TRUE if there are at least one valid caret-position inside helm.
	 * @param doc The document with helm.
	 * @param helm The element to perform the check on...
	 * @param is_in_tree If FALSE, helm is not supposed to be in the document's logical tree which implies that
	 * helm doesn't need to have an editable container and that no check for if helm is collapsed occurs.
	 * @param ending_br_is_ok If it's ok to have the caret at a trailing BR.
	 * @param valid_if_possible If TRUE then the definition of "valid" will be more relaxed.
	 * @param helm_to_remove Element that is to be removed in a moment, it is not a valid position for the caret.
	 */
	static BOOL IsElementValidForCaret(FramesDocument* doc, HTML_Element *helm, BOOL is_in_tree = TRUE, BOOL ending_br_is_ok = FALSE, BOOL valid_if_possible = FALSE, HTML_Element* helm_to_remove = NULL);

	/**
	 * Finds an editable element starting from from_helm, that is, an elements which includes a valid caret-position.
	 * @return The editable element that was found, or NULL if no such element was found.
	 * @param forward If TRUE, searches forward in the logical tree, else backwards.
	 * @param include_current If TRUE, from_helm itself will be the first candidate scanned.
	 * @param require_box Only return elements with a layout box.
	 * @param include_ending_br If TRUE may an ending BR be returned, that is, a BR that ends a line with previous valid caret-positions.
	 * @param helm_to_remove Element that is to be removed in a moment, it is not a valid position for the caret and should be omitted
	 * during the search
	 */
	static HTML_Element* FindEditableElement(FramesDocument* doc, HTML_Element* from_helm, BOOL forward, BOOL include_current, BOOL require_box, BOOL include_ending_br = FALSE, HTML_Element* helm_to_remove = NULL);

	/**
	 * Checks if two elements are inline, and tightly connected so that we should not position any caret in between them.
	 *
	 * For instance <b>A</b><b>B</b> would not result in a caret between the bold elements and this will return TRUE.
	 *
	 * This used to be very magic and known as OpDocumentEdit::IsFriends(). It's still magic but it has a better name.
	 *
	 * @param helm_a The first element.
	 * @param helm_b The second element.
	 * @param include_helm_b Anyone asking what this is will be auto-assigned to figure it out and document it.
	 * @param include_helm_replaced Anyone asking what this is will be auto-assigned to figure it out and document it.
	 * @param br_is_friendly Anyone asking what this is will be auto-assigned to figure it out and document it.
	 */
	BOOL IsTightlyConnectedInlineElements(HTML_Element* helm_a, HTML_Element* helm_b, BOOL include_helm_b = FALSE, BOOL include_replaced = FALSE, BOOL br_is_friendly = FALSE);

	/**
	 * Check if an element is displayed right to left.
	 * @param helm The element to check.
	 * @return TRUE if the element is displayed rtl and FALSE otherwise.
	 */
	BOOL GetRTL(HTML_Element *helm);

	/**
	 * Tries to "snap" the current caret-position to something valid (if it's not already valid).
	 * @returns FALSE if we still don't have a valid position.
	 */
	BOOL SetToValidPos();

	/**
	 * Move the caret forward or back in the document in the logical position
	 * @param forward If TRUE, tries to move one step forward, else backwards...
	 * @param word If TRUE, we'll move to the next word-boundary.
	 */
	void Move(BOOL forward, BOOL word);

	/**
	 * Finds the next valid caret-position starting from helm and ofs.
	 * @return TRUE if such a position was found.
	 * @param helm The element to start the scan from.
	 * @param ofs The offset to start the scan from.
	 * @param new_helm The element that was found, if it was found (might be helm itself).
	 * @param new_ofs The offset that was found, if it was found.
	 * @param snap If TRUE, tries to "snap" the result to a HE_TEXT element
	 * @param accept_no_diff If TRUE, it's alright if the new position visually is on the same place as the old one,
	 * for example: hej|<b>du</b> -> hej<b>|du</b> where | represents the position.
	 */
	BOOL GetOneStepBeside(BOOL forward, HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs, BOOL snap = TRUE, BOOL accept_no_diff = FALSE);

	/**
	 * Tries to place the caret at the logical position defined by helm+ofs.
	 * @param helm The element to put the caret on.
	 * @param ofs The element-offset to put the caret on.
	 * @param prefer_first If (helm, ofs) corresponds to two reasonable visual
	 *       positions (for example, at the end of one line vs. at the start of
	 *       the next line), setting this to TRUE will place the caret at the
	 *       first position relative to the flow of text, and vice versa.
	 * @param allow_snap If TRUE, helm+ofs will be "snapped" to a valid, non-collapsed, logical position.
	 * @param keep_within_current_context If TRUE and the current caret-position is withing a different contentEditable container
	 * then helm, then will the operation not do anything.
	 * @param remember_x If TRUE, will we try to keep the X-position
	 * when the user navigates spatial up/down or page-up/page-down.
	 * @param set_selection If TRUE this method will try to set the
	 * documentselection.
	 */
	void Place(HTML_Element* helm, int ofs, BOOL prefer_first = TRUE, BOOL allow_snap = TRUE, BOOL keep_within_current_context = FALSE, BOOL remember_x = TRUE, BOOL set_selection = TRUE);

	/**
	 * Returns the element that acts as outer limit for caret movements. Typically the body but can be something smaller
	 * if for instance a div has the contentediatable attribute. The div would then be the containter.
	 */
	HTML_Element* GetCaretContainer(HTML_Element* search_origin);

	/**
	 * Wrapper for Box::GetContainingElement.
	 * @param elm The element to find containing element for.
	 * @return The containing element which may be NULL.
	 */
	HTML_Element* GetContainingElement(HTML_Element* elm);

	/**
	 * Same as GetContainingElement but only returns actual
	 * elements. Also for the body element it just returns the
	 * element itself. The returned element will not be outside
	 * of <body> (if there is one).
	 *
	 * @param elm The element to find containing element for.
	 * @return The containing element which may be NULL.
	 */
	HTML_Element* GetContainingElementActual(HTML_Element *elm);

private:
	/** Is a element which childs is treated as if in the same string as other childs in a friendly element.
		Basically all inlineelements, with some exceptions if include_replaced is FALSE.
		Use to determine if the caret should move right into it or require a extra keypress. F.ex. a, b, u */
	static BOOL MightBeTightlyConnectedToOtherElement(FramesDocument* doc, OpDocumentEdit* documentedit, HTML_Element* helm, BOOL include_replaced = FALSE, BOOL br_is_friendly = FALSE, BOOL non_editable_is_friendly = FALSE);

	/**
	 * Get the nearest valid caret position starting from from_helm.
	 * @returns TRUE if such a position was found, FALSE otherwise.
	 * @param from_helm The element to start the scan from. NULL is a legal argument but will result in a return value of FALSE.
	 * @param nearest_helm The element that was found, if it was found. NULL is a legal argument but will result in a return value of FALSE.
	 * @param nearest_ofs The offset that was found, if it was found.
	 * @param forward TRUE if the logical tree should be scanned forward, else backward.
	 * @param include_current TRUE if we should try with from_helm itself first.
	 * @param helm_to_remove Element that is to be removed in a moment, it is not a valid position for the caret.
	 */
	static BOOL GetNearestCaretPos(FramesDocument* doc, HTML_Element* from_helm, HTML_Element** nearest_helm, int* nearest_ofs, BOOL forward, BOOL include_current, HTML_Element* helm_to_remove = NULL);

	/**
	 * Helper method to do Box::GetRect().
	 * @see Box::GetRect()
	 */
	BOOL GetContentRectOfElement(HTML_Element *helm, AffinePos &ctm, RECT &rect, BoxRectType type = CONTENT_BOX);

	/** Just a wrapper for IsTightlyConnectedInlineElements... */
	BOOL IsOnSameLine(HTML_Element *a, HTML_Element *b, BOOL a_first);

	FramesDocument *frames_doc;
};

/**
 * Helper class for our WordInfoIterator.
 *
 * @see CaretManagerWordIterator
 */
class WordIteratorSurroundChecker : public WordInfoIterator::SurroundingsInformation
{
private:
	FramesDocument* m_doc;
public:
	WordIteratorSurroundChecker(FramesDocument *doc) : m_doc(doc) {}
	virtual BOOL HasWsPreservingElmBeside(HTML_Element* helm, BOOL before);
};

/**
 * CaretManagerWordIterator is an abstraction for easier access to the layout word styling information.
 * It also contains some logic for determining valid caret-positions inside text-elements.
 */
class CaretManagerWordIterator : public WordInfoIterator
{
public:

	virtual ~CaretManagerWordIterator() {}

	/** Constructor, helm should be a HE_TEXT element and doc its FramesDocument as some functions like
	    FramesDocument::Reflow and FramesDocument->caret_manager->IsTightlyConnectedInlineElements are used. */
	CaretManagerWordIterator(HTML_Element* helm, FramesDocument* doc);

	/** @returns error status code from the constructor, should be checked before using any functions below. */
	OP_STATUS GetStatus() { return m_status; }

	/** @returns TRUE if there are no valid caret-position in the element. */
	BOOL IsCollapsed() { return !IsValidForCaret(); }

	/** @returns TRUE if there is a valid caret-offset in the element. This is basically the inverse of the IsCollapsed() function. */
	BOOL IsValidForCaret(BOOL valid_if_possible = FALSE);

	/**
	 * Retrieves the first valid caret-offset into the element in res_ofs.
	 * @returns FALSE if the element has no valid caret-position.
	 */
	BOOL GetFirstValidCaretOfs(int &res_ofs);

	/**
	 * Retrieves the last valid caret-offset into the element in res_ofs.
	 * @returns FALSE if the element has no valid caret-position.
	 */
	BOOL GetLastValidCaretOfs(int &res_ofs);

	/**
	 * "Snaps" offset ofs to a valid caret-ofset.
	 * @return FALSE if the element has no valid caret-offset.
	 * @param ofs The offset to start the scan from.
	 * @param res_ofs The result, if ofs was valid will res_ofs be equals to ofs. If ofs is after the the last valid
	 * caret-offset will we snap to the last valid caret-offset, else we'll snap not the next valid caret-offset.
	 */
	BOOL SnapToValidCaretOfs(int ofs, int &res_ofs);

	/**
	 * Same as SnapToValidCaretOfs but res_ofs will be the offset as it would have been if all collapsed characters
	 * in the element where removed, ofs should still be an offset into the full text-content of the element though.
	 * @see SnapToValidCaretOfs().
	 */
	BOOL GetCaretOfsWithoutCollapsed(int ofs, int &res_ofs);

	/**
	 * Retrieves the next of previous valid character-offset starting from caret-offset ofs.
	 * @return TRUE if such offset was found.
	 * @param ofs The offset to start from, ofs might be valid or not
	 * @param res_ofs The result...
	 * @param TRUE if we should scan forward, else backward.
	 */
	BOOL GetValidCaretOfsFrom(int ofs, int &res_ofs, BOOL forward);

	/**
	 * Wrapper around GetValidCaretOfsFrom with forward == TRUE.
	 * @see GetValidCaretOfsFrom()
	 */
	BOOL GetNextValidCaretOfs(int ofs, int &res_ofs) { return GetValidCaretOfsFrom(ofs, res_ofs, TRUE); }

	/**
	 * Wrapper around GetValidCaretOfsFrom with forward == FALSE.
	 * @see GetValidCaretOfsFrom()
	 */
	BOOL GetPrevValidCaretOfs(int ofs, int &res_ofs) { return GetValidCaretOfsFrom(ofs, res_ofs, FALSE); }

private:
	BOOL HasWsPreservingElmBeside(BOOL before);

	WordIteratorSurroundChecker m_surround_checker;
	OP_STATUS m_status;
	FramesDocument* m_doc;
	BOOL3 m_is_valid_for_caret;
};
#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT
#endif // CARET_MANAGER_H
