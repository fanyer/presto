/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SELECTION_H
#define SELECTION_H

#include "modules/logdoc/selectionpoint.h"
#include "modules/logdoc/htm_elm.h" // For HTML_Element::ValueModificationType
#include "modules/doc/doctypes.h" // For TEXT_SELECTION_CHARACTERS
#include "modules/layout/layout_coord.h" // For LayoutCoord

class FormObject;
class FramesDocument;
class LogicalDocument;

/**
 * Class that represents a range in a document, specifically used for selections and highlights. A range is
 * defined by the two edge points, called either start+stop (document order) or
 * anchor+focus (selection order). Those edge points (often called Boundary Points, SelectionBoundaryPoints or variations
 * thereof) at some times have two different representations (read this, it's important!).
 *
 * The old backwards compatible layout representation is that they point to a leaf element (text, BR or replaced content)
 * and has an offset that is either the text offset or 0 or 1. 0 or 1 indicate whether the element is right before
 * or after the boundary point. For instance BR+1 would mean that the edge point is right after the BR. BR+0 would
 * mean that the edge point is right before the BR. This is *NOT* compatible with the new, official representation
 *
 * The "new" official representation has Boundary Points as described in public selection and Range
 * specs. There the edge points are container+offset so <leaf>+1 would
 * be an invalid meaningless boundary point.
 *
 * It is most of the time possible to convert between the representations as long as you know
 * what you are doing and which representation you have.
 *
 * In Opera documentedit and layout is written for the old representation. Scripts and general selection code uses the new
 * representation. The new representation is recommended since it's the only one that doesn't lose information.
 */
class TextSelection
{
public:
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	enum HighlightType
	{
		HT_SELECTION,
		HT_ACTIVE_SEARCH_HIT,
		HT_SEARCH_HIT
	};
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

private:

	/** Selection point that comes first in logical order.
	    The bool start_is_anchor indicates if this the anchor point or
	    the focus point. */

	SelectionBoundaryPoint
					start_selection;

	/** Selection point that comes last in logical order.
	    The bool start_is_anchor indicates if this the anchor point or the
	    focus point. */

	SelectionBoundaryPoint
					end_selection;

	/**
	 * Is start_selection the anchor point?  If start_is_anchor ==
	 * TRUE, then start_selection is the anchor point of the selection
	 * and end_selection is the focus point.  If start_is_anchor ==
	 * FALSE, then start_selection is the focus point of the selection
	 * and end_selection is the anchor point.
	 */
	BOOL			start_is_anchor;

	/** Type of selection. */

	TextSelectionType
					type;

	/** Bounding rectangle of selected area. */

	RECT			bounding_rect;

	/** If a text search found a match in a FormsObject, this is the one */

	FormObject*		form_object;

	/**
	 * During textgroup changes elements can be removed which ruins the correct offset so we need to remember the
	 * offset before the change. This is set to -1 after any change so if it's >= 0 then a change is in progress.
	 */
	int				before_text_change_start_offset;

	/**
	 * During textgroup changes elements can be removed which ruins the correct offset so we need to remember the
	 * offset before the change.
	 */
	int				before_text_change_end_offset;

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	HighlightType	highlight_type;
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

	/**
	 * Selection is dirty and needs to be fixed next time document is
	 * reflowed. This is a flag that might be possible to optimize
	 * away with small changes in our layout engine.
	 */
	BOOL			is_dirty;

	/**
	 * The previous position of the pointer while selecting. Used when we
	 * couldn't make a perfect decision and need more context to make
	 * an educated guess at what the user is trying to do.
	 */
	OpPoint			previous_point;

	/** A flag indicating whether a selection is being updated (e.g. expanded) at the moment. */
	BOOL			is_being_updated;

	/**
	 * Validates the start and end selection points, making sure that the
	 * start point precedes the end point in document order. This function
	 * will switch the start and end selection points and update the
	 * start_is_anchor member when necessary.
	 */
	void			ValidateStartAndEndPoints();

	/**
	 * Set new x and y coordinates for the specified point. That might update
	 * one or both of start_selection and end_selection, update the selection
	 * bit in the element tree (clearing or setting depending on what is
	 * appropriate) and schedule a repaint of changed areas.
	 *
	 * @param [in] doc The document to which this text selection belongs.
	 * @param [in] x New x coordinate for point in document coordinate system.
	 * @param [in] y New y coordinate for point in document coordinate system.
	 * @param [in] point Selection point to move to the new position. Must be a
	 *                   pointer to either of this->start_selection or
	 *                   this->end_selection for the code to work.
	 */
	void			SetNewPosition(FramesDocument* doc, LayoutCoord x, LayoutCoord y,
								   const SelectionBoundaryPoint* point);

	/**
	 * Set a new position for the focus point and mark the tree
	 * accordingly. Also schedule a repaint of affected areas of the
	 * screen.
	 *
	 * @param [in] doc Document to perform selection in.
	 * @param [in] anchor_point Current selection anchor point.
	 * @param [in] focus_point Current selection focus point.
	 * @param [in] nearest_boundary_point Boundary point closest to new position.
	 * @param [in] nearest_word Word closes to new position.
	 * @param [in] nearest_word_char_offset Character offset in nearest word.
	 * @param [in] nearest_text_box_x Distance to left side of nearest box.
	 */
	void			SetNewPosition(FramesDocument* doc,
								   const SelectionBoundaryPoint* current_anchor_point,
								   const SelectionBoundaryPoint* current_focus_point,
								   const SelectionBoundaryPoint& nearest_boundary_point,
								   const uni_char* nearest_word, int nearest_word_char_offset,
								   long nearest_text_box_x);

	/**
	 * Gets the document position and line height for a text selection point.
	 *
	 * Might cause Reflows and will use layout tree traversals so it can
	 * be slow and must not be called from within the layout engine.
	 *
	 * @param [in] sel_pt Text selection point.
	 * @param [out] doc_pt Document coordinates for sel_pt.
	 * @param [out] line_height Pixel line height of text selection at sel_pt.
	 * @return OpStatus::OK if operation completed successfully and there is an
	 *         active selection. If there is no active text selection
	 *         OpStatus::ERR is returned. If there is no known visible area for
	 *         the document then OpStatus::ERR is returned.
	 *         OpStatus::ERR_NO_MEMORY is returned if the operation failed
	 *         because of out of memory.
	 */
	static OP_STATUS	GetSelectionPointPosition(FramesDocument *frames_doc, const SelectionBoundaryPoint &sel_pt, OpPoint &doc_pt, int &line_height);

public:

					TextSelection()
					  : start_is_anchor(TRUE),
						type(TEXT_SELECTION_CHARACTERS),
						form_object(NULL)
						, before_text_change_start_offset(-1)
						, before_text_change_end_offset(-1)
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
						, highlight_type(HT_SELECTION)
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
						, is_dirty(FALSE)
						, is_being_updated(FALSE) {}

	/** Clear selection */

	void			Clear(FramesDocument* doc);

	/** Is selection empty? */

	BOOL			IsEmpty();

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	HighlightType	GetHighlightType() { return highlight_type; }
	void			SetHighlightType(HighlightType new_type) { highlight_type = new_type; }

	void			SetBoundingRect(RECT& new_rect);
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

	/** Is selection dirty? */

	BOOL			IsDirty() { return is_dirty; }

	/** Mark selection clean. */

	void			MarkClean() { is_dirty = FALSE; }

	/** Mark selection dirty. */

	void			MarkDirty(HTML_Element* element);

	/**
	 * Remove element from selection, adjusting the selection to no longer refer
	 * to the element in question. Any offsets will also be adjusted to take
	 * into account the changes.
	 */
	void			RemoveElement(HTML_Element* element);

	/**
	 * Inserts element into selection if it is within the current selection range.
	 * Also adjusts offsets to take the inserted element into account.
	 */
	void			InsertElement(HTML_Element* child);

	/**
	 * Called before any text change to give the TextSelection a chance to protect
	 * its contents against HE_TEXTGROUP/HE_TEXT changes.
	 *
	 * param text_node The text or text group element that is about to change.
	 */
	void			BeforeTextDataChange(HTML_Element* text_node);

	/**
	 * Called if a text change signalled with BeforeTextDataChange() couldn't be completed.
	 *
	 * @param text_node The text or text group element that was about to change.
	 */
	void			AbortedTextDataChange(HTML_Element* text_node);

	/**
	 * Text element changed in the specified way and the TextSelection should compensate.
	 * This is preceded by a call to BeforeTextDataChange.
	 *
	 * @param text_element The HTML_Element that just changed.
	 *
	 * @see HTML_Element::SetText() for the documentation of the other arguments.
	 */
	void			TextDataChange(HTML_Element* text_element, HTML_Element::ValueModificationType modification_type, unsigned offset, unsigned length1, unsigned length2);

	/**
	 * Internal docxs function that must be called when the parser or something else causes
	 * a HE_TEXT element to be converted to a HE_TEXTGROUP element. Text selections
	 * point to HE_TEXT or elements, not to HE_TEXTGROUP so when this happens
	 * we need to adjust the internal pointers.
	 *
	 * @param elm The element that used to be an HE_TEXT but is now an HE_TEXTGROUP.
	 *
	 * @see HTML_Element::AppendText().
	 */
	void			OnTextConvertedToTextGroup(HTML_Element*elm);

/** Set new x and y coordinates for the selection's anchor point. That
	    might update one or both of start_selection and end_selection, update
	    the selection bit in the element tree (clearing or setting depending
	    on what is appropriate) and schedule a repaint of changed areas.
	    @param [in] doc The document to which this text selection belongs.
	    @param [in] x New x coordinate for the anchor point in document coordinate system.
	    @param [in] y New y coordinate for the anchor point in document coordinate system. */

	void			SetAnchorPosition(FramesDocument* doc, LayoutCoord x, LayoutCoord y);

	/**
	 * Set new x and y coordinates for the selection's focus point. That
	 * might update one or both of start_selection and end_selection, update
	 * the selection bit in the element tree (clearing or setting depending
	 * on what is appropriate) and schedule a repaint of changed areas.
	 * @param [in] doc The document to which this text selection belongs.
	 * @param [in] x New x coordinate for the focus point in document coordinate system.
	 * @param [in] y New y coordinate for the focus point in document coordinate system.
	 * @param [in] end_selection TRUE if this seems to be the last
	 * SetFocusPosition in a sequence of SetFocusPosition calls.
	 * If it's TRUE and we have a delayed decision, we might force a
	 * collapsed selection somewhere anyway.
	 */
	void			SetFocusPosition(FramesDocument* doc, LayoutCoord x, LayoutCoord y, BOOL end_selection);

	/**
	 * Set new selection to all the textual content of the element, starting from the first interesting child of the element to select and ending with the last such child.
	 * @param doc The document to which this text selection belongs
	 * @param element The element to select.
	 * @param mark_tree_selected TRUE if the elements should be marked as selected in the document tree. That will make them appear as visually highlighted. If FALSE, no selected flag will be set on the elements and the only effect will be the setting of the start and end of the selection. This should be used by visual selections such as the main document selection but not by temporary or internal selections.
	 * @param req_inside_doc TRUE if the new start and end selection elements must be visible in the document. If FALSE the start and end selection elements may be completely outside the document area.
	 * @param do_visual_selection TRUE if the selection should be adapted to only cover the visual parts of element.
	 */
	void			SetNewSelection(FramesDocument* doc, HTML_Element* element, BOOL mark_tree_selected, BOOL req_inside_doc, BOOL do_visual_selection);

	/**
	 * Set the initial X- and Y-coordinates for the selection anchor point.
	 *
	 * This function should only be called once when the object is created.

	 * @param [in] doc Document to perform selection in.
	 * @param [in] x Document x-position.
	 * @param [in] y Document y-position.
	 * @param [in] find_nearest Set to true if the entire visual viewport should
	 *							 be traversed in order to find the nearest selectable
	 *							 box. This is useful if one wants to find the nearest
	 *							 text element among all visible text elements, not
	 *							 just the nearest local text element.
	 * @param [in] only_alphanum Set to true to find words starting with an
	 *							  alpha-numeric or single quote character. This is
	 *							  useful when wanting to select the nearest word by
	 *							  a later call to ExpandSelection(). If only_alphanum
	 *							  is false the nearest text element may be word
	 *							  delimiter which ExpandSelection() cannot expand
	 *							  to a word.
	 */
	OP_STATUS		SetCollapsedPosition(FramesDocument* doc, LayoutCoord x, LayoutCoord y,
					                     BOOL find_nearest, BOOL only_alphanum);

	/**
	 * Set new selection to new selection points
	 *
	 * @param[in] remember_old_wanted_x_position When moving a selection/caret up and down we want to remember
	 * the column the user is moving in so that we can stay in it even if we temporarily lands on a
	 * short (shorter than the wanted X position) line. On the other hand, for any other selection move,
	 * we should forget that information. Use FALSE here to reset the "wanted X" to the current position, and
	 * TRUE to remember the old value. This argument is only relevant if this is the document's official TextSelection
	 * object. Otherwise the argument is ignored.
	 * @param[in] end_is_focus Connecting the start and end with anchor focus. If this is TRUE then start
	 * is anchor and end is focus. If this is FALSE then start is focus and end is anchor.
	 */
	void			SetNewSelection(FramesDocument* doc, const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, BOOL update, BOOL remember_old_wanted_x_position, BOOL end_is_focus);

#ifndef HAS_NOTEXTSELECTION
	/**
	 * Set new selection focus and anchor.
	 *
	 * @param[in] remember_old_wanted_x_position When moving a selection/caret up and down we want to remember
	 * the column the user is moving in so that we can stay in it even if we temporarily lands on a
	 * short (shorter than the wanted X position) line. On the other hand, for any other selection move,
	 * we should forget that information. Use FALSE here to reset the "wanted X" to the current position, and
	 * TRUE to remember the old value. This argument is only relevant if this is the document's official TextSelection
	 * object. Otherwise the argument is ignored.
	 */
	void			SetNewSelectionPoints(FramesDocument* doc, const SelectionBoundaryPoint& anchor, const SelectionBoundaryPoint& focus, BOOL update, BOOL remember_old_wanted_x_position);
#endif // HAS_NOTEXTSELECTION

	/** Update the document's navigation element */

	void			UpdateNavigationElement(FramesDocument* doc, HTML_Element* element);

	/** Sets that a formobject has a selection (To continue search from there next time) */

	void			SetFormObjectHasSelection(FormObject* form_object) { this->form_object = form_object; }

	/** Returns the selected formobject */

	FormObject*		GetSelectedFormObject() { return form_object; }

	/**
	 * Retrieves the text selection start coordinates. The coordinates are
	 * specified in the document coordinate system.
	 *
	 * This function should be used carefully. It may suffer slow performance
	 * as it will trigger a reflow that might take a long time.
	 *
	 * @param [out] pos Selection start position (in document coordinates).
	 * @param [out] line_height Line pixel height at selection start position.
	 * @return OpStatus::OK if operation completed successfully and there is an
	 *         active selection. If there is no active text selection
	 *         OpStatus::ERR is returned. If there is no known visible area for
	 *         the document then OpStatus::ERR is returned.
	 *         OpStatus::ERR_NO_MEMORY is returned if the operation failed
	 *         because of out of memory.
	 */
	OP_STATUS			GetAnchorPointPosition(FramesDocument *frames_doc, OpPoint &pos, int &line_height);

	/**
	 * Retrieves the text selection end coordinates. The coordinates are
	 * specified in the document coordinate system.
	 *
	 * This function should be used carefully. It may suffer slow performance
	 * as it will trigger a reflow that might take a long time.
	 *
	 * @param [out] pos Selection end position (in document coordinates).
	 * @param [out] line_height Line pixel height at selection end position.
	 * @return OpStatus::OK if operation completed successfully and there is an
	 *         active selection. If there is no active text selection
	 *         OpStatus::ERR is returned. If there is no known visible area for
	 *         the document then OpStatus::ERR is returned.
	 *         OpStatus::ERR_NO_MEMORY is returned if the operation failed
	 *         because of out of memory.
	 */
	OP_STATUS			GetFocusPointPosition(FramesDocument *frames_doc, OpPoint &pos, int &line_height);

	/** Expand selection to given type */

	void			ExpandSelection(FramesDocument* doc, TextSelectionType selection_type);

	/**
	 * Get the containter element of the start of the selection.
	 *
	 * @returns An element or NULL if there is no element (the selection is empty/undefined).
	 */
	HTML_Element*	GetStartElement() const { return start_selection.GetElement(); }

	/**
	 * Get the containter element of the end of the selection.
	 *
	 * @returns An element or NULL if there is no element (the selection is empty/undefined).
	 */
	HTML_Element*	GetEndElement() const { return end_selection.GetElement(); }

	/** Copy selected text to a buffer.
	    @param doc The document to which this text selection belongs
		@param buffer The buffer in which to put the selected text. This parameter may be NULL, if
		it is the caller's intention to just get the number of characters in the text selection. If
		non-NULL, this string buffer will always be NUL-terminated. If the currently selected text
		is longer than what there's room for in 'buffer' (the capacity specified by the
		'buffer_length' parameter), 'buffer' will only contain the 'buffer_length-1' first
		characters of the selected text.
		@param buffer_length capacity of 'buffer' - i.e. maximum number of characters (including
		the NUL-terminator) to put in 'buffer'
		@param unused Unused parameter; provided for API backwards compatibility.
		@param blockquotes_as_text If TRUE, blockquotes will make containing lines start with '>' characters.
		@return number of characters selected and a copy in 'buffer' if it isn't NULL.
	*/
	int GetSelectionAsText(FramesDocument* doc, uni_char* buffer, int buffer_length, BOOL unused = TRUE, BOOL blockquotes_as_text = FALSE);

	/** Get selection point that comes first in logical order */

	const SelectionBoundaryPoint&
					GetStartSelectionPoint() const { return start_selection; }

	/** Get selection point that comes last in logical order */

	const SelectionBoundaryPoint&
					GetEndSelectionPoint() const { return end_selection; }

	/** @Returns the anchor point (the point where the selection started). It is one of start_selection or end_selection. Will never return NULL. */

	const SelectionBoundaryPoint*
					GetAnchorPoint() const { return start_is_anchor ? &start_selection : &end_selection; }

	/** @Returns the focus point (the point where the selection operation ended; the opposite of the anchor point). It is one of start_selection or end_selection. Will never return NULL. */

	const SelectionBoundaryPoint*
					GetFocusPoint() const { return start_is_anchor ? &end_selection : &start_selection; }

	/** Sets the anchor point (the point where the selection operation started;
	    the opposite of the focus point). It will change one or both of
	    start_selection or end_selection, and might change start_is_anchor as
	    well if the selection is inverted. No state except for start and end
	    selection will be changed.
	    @param new_point The new anchor point. */

	void			SetAnchorPoint(const SelectionBoundaryPoint& new_point);

	/** Sets the focus point (the point where the selection operation ended;
	    the opposite of the anchor point). It will change one or both of
	    start_selection or end_selection, and might change start_is_anchor as
	    well if the selection is inverted. No state except for start and end
	    selection will be changed.
	    @param new_point The new focus point. */

	void			SetFocusPoint(const SelectionBoundaryPoint& new_point);

	int				GetStartOffset() { return start_selection.GetOffset(); }

	int				GetEndOffset() { return end_selection.GetOffset(); }

	/** Get bounding rectangle for selected area. */

	const RECT&		GetBoundingRect() const { return bounding_rect; }

	/** Copies all members of ts. */

	void			Copy(TextSelection *ts);

	/**
	 * Get an old-style selection point from the SelectionBoundaryPoint. Old style selection
	 * points are limited to layouted, visible leaf nodes and use <leafnode>+offset=1
	 * to signal "after the leaf" which is incompatible with the standard way of
	 * specifying boundary points. This will search forward from the selection point.
	 *
	 * @param[in] point The point to use as source of the conversion.
	 *
	 * @param[out] old_style_elm The resulting old style selection point element. This will either
	 * become NULL or point to a leaf node (text, br or img).
	 *
	 * @param[out] old_style_offset The resulting offset. This will either be 0/1 if old_style_elm
	 * is not a text node or an offset into the text node (same semantics as standard boundary points).
	 */
	static void		ConvertPointToOldStyle(const SelectionBoundaryPoint& point, HTML_Element*& old_style_elm, int& old_style_offset);

	/**
	 * Get an old-style selection point from the SelectionBoundaryPoint. Old style selection
	 * points are limited to layouted, visible leaf nodes and use <leafnode>+offset=1
	 * to signal "after the leaf" which is incompatible with the standard way of
	 * specifying boundary points.
	 *
	 * @param[in] point The point to use as source of the conversion.
	 *
	 * @param[out] old_style_elm The resulting old style selection point element. This will either
	 * become NULL or point to a leaf node (text, br or img).
	 *
	 * @param[out] old_style_offset The resulting offset. This will either be 0/1 if old_style_elm
	 * is not a text node or an offset into the text node (same semantics as standard boundary points).
	 *
	 * @param[in] scan_forward If we should scan forward or backwards for a suitable "old style" position.
	 * TRUE for scanning forward. TRUE is the default.
	 *
	 * @param[in] scan_block If this is not NULL then we'll scan no further than scan_block for a suitable
	 * "old style" position.
	 */
	static void		ConvertPointToOldStyle(const SelectionBoundaryPoint& point, HTML_Element*& old_style_elm, int& old_style_offset, BOOL scan_forward, const SelectionBoundaryPoint& scan_block_point);

	/**
	 * Converts an element+offset in the "old" style to a modern style SelectionBoundaryPoint. The "old" style
	 * is either text+offset which is identical to the standard representation, or element+0/1 which
	 * indicates before (0) or after (1) element.
	 *
	 * @param old_style_elm The element.
	 *
	 * @param old_style_offset The offset (0 or 1 if old_style_elm is an element, otherwise the character offset in the HE_TEXT.
	 *
	 * @returns A SelectionBoundaryPoint that might be empty if the input couldn't be represented as anything else.
	 */
	static SelectionBoundaryPoint ConvertFromOldStyle(HTML_Element* old_style_elm, int old_style_offset);

	class ElementOperation
	{
	public:
		~ElementOperation() {}

		virtual void Call(HTML_Element* element) = 0;
	};

	/** Performs an operation on each of the elements in the given range.
	 *
	 * The range is defined by start and stop arguments.
	 * Text nodes being at the end points of the range are always considered
	 * within the range even if no characters are in selection, i.e. for
	 * <text_node + text_length> at the start and <text_node + 0> as the end.
	 *
	 * @param start Start of the range to perform the operation on.
	 * @param stop End of the range to perform the operation on.
	 * @param operation The operation to perform.
	 * @param exclude_text_start Omit the start element if it is a text node.
	 * @param exclude_text_stop Omit the stop element if it is a text node.
	 */
	static void ForEachElement(const SelectionBoundaryPoint& start_point, const SelectionBoundaryPoint& stop_point, ElementOperation& operation, BOOL exclude_text_start = FALSE, BOOL exclude_text_stop = FALSE);


	/** @return TRUE if a selection is being updated (e.g. expanded) at the moment. FALSE otherwise. */
	BOOL IsBeingUpdated() const { return is_being_updated; }

private:
	/** Reflow document to prepare the layout tree for text-selection */

	static OP_STATUS
					ReflowDocument(FramesDocument* doc);

	/** Tells the DOM Selection object about changes if appropriate. */

	void NotifyDOMAboutSelectionChange(FramesDocument* doc);

#ifdef SELFTEST
public:
#endif // SELFTEST
	/** Calls SetIsInSelection on all elements in the selection described by start and stop selection points.
	 *
	 * Selects or unselects all elements that are within the range designated
	 * by the start and stop selection points.
	 * Text nodes are affected if at least one character of the node is within
	 * the range. This behaviour can be modified with exclude_text_start and
	 * exclude_text_stop flags.
	 *
	 * @param start Start of the range to (un)select.
	 * @param stop End of the range to (un)select.
	 * @param value Whether to set elements as selected or unselected.
	 * @param exclude_text_start Omit the start element if it is a text node.
	 * @param exclude_text_stop Omit the stop element if it is a text node.
	 */
	static void SetElementsIsInSelection(const SelectionBoundaryPoint& start, const SelectionBoundaryPoint& stop, BOOL value, BOOL exclude_text_start = FALSE, BOOL exclude_text_stop = FALSE);

	/** Finds the first element after selection.
	 *
	 * @param stop_point The end point of selection after which the
	 *        element is to be found.
	 * @param exclude_text_stop Whether to exclude the last text element.
	 *        If TRUE, the last element within the selection is returned
	 *        on the condition that it's a text node.
	 * @return the first, possibly non-actual, element after selection
	 *         or NULL if there are no elements after selection.
	 */
	static HTML_Element* GetFirstElementAfterSelection(const SelectionBoundaryPoint& stop_point, BOOL exclude_text_stop);
};

#endif // !SELECTION_H
