/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SVG_TEXT_SELECTION_H
#define SVG_TEXT_SELECTION_H

# ifdef SVG_SUPPORT_TEXTSELECTION

#include "modules/svg/SVGManager.h"
#include "modules/util/adt/opvector.h"
#include "modules/layout/traverse/traverse.h"

class SVGTextRootContainer;
class SVGEditable;

class SVGTextSelection
{
private:
	HTML_Element*	m_text_element;			///< This is the <text> element that contains the selected element

	SelectionBoundaryPoint m_start;				///< Logical textselection, keeps HE_TEXT + offset in that element
	SelectionBoundaryPoint m_end;				///< Logical textselection, keeps HE_TEXT + offset in that element
	SelectionBoundaryPoint m_cursor;

	OpPoint			m_start_screen_mousepos;///< The mousepos from StartSelection in screen coordinates
	OpPoint			m_last_screen_mousepos;	///< The last reported mousepos in screen coordinates
	OpRect			m_invalid_screenrect;

	union {
		struct {
			unsigned int is_selecting:1;
			unsigned int is_valid:1;
			unsigned int mouse_was_moved:1;
			/* 16 bits */
		} textselection_info;
		unsigned short textselection_info_init;
	};

	enum { MouseMovePixelThreshold = 4 };

	/**
	 * Invalidates selection.
	 * @param invalidate_element If TRUE then do MarkForRepaint, that is: invalidate element (and parents if necessary)
	 */
	void Update(BOOL invalidate_element = FALSE);

	OpRect GetSelectionScreenRect();

	void AddSelectionToInvalidRect();

	/**
	 * Returns TRUE if mouse has moved more than MouseMovePixelThreshold pixels.
	 */
	BOOL MouseHasMoved(const OpPoint& oldpos, const OpPoint& newpos);

	BOOL MouseWasMoved() { return textselection_info.mouse_was_moved; }
	void SetMouseWasMoved(BOOL val) { textselection_info.mouse_was_moved = val ? 1 : 0; }

	SVGTextRootContainer *GetTextRootContainer() const;

	void SetTextSelectionPoint(SelectionBoundaryPoint& point, HTML_Element* textroot_elm, HTML_Element* sel_elm, int index);

public:
	SVGTextSelection();

	/**
	 * Must be called when elements are removed from the document tree. Cleans up any HTML_Element pointers that may be held by this class.
	 */
	void		HandleRemovedSubtree(HTML_Element* subtree);

	OP_STATUS	DOMSetSelection(HTML_Element* selected_elm, UINT32 logical_startindex, UINT32 num_chars);
	/**
	 * Intended to mimic FramesDocument::MaybeStartTextSelection, if nothing prevents us from starting a textselection
	 * then start a textselection. If we were prevented from starting a selection (OOM or other conditions), then clear the selection.
	 */
	void		MaybeStartSelection(const SVGManager::EventData& data);
	OP_STATUS	StartSelection(const SVGManager::EventData& data);
	OP_STATUS	EndSelection(const SVGManager::EventData& data);

	void			SelectAll(HTML_Element* text_root);
	OP_STATUS		SetSelection(SelectionBoundaryPoint *startpoint, SelectionBoundaryPoint *endpoint);
	HTML_Element*	GetStartElement() { return GetLogicalStartPoint().GetElement(); }
	HTML_Element*	GetEndElement() { return GetLogicalEndPoint().GetElement(); }
	const uni_char*	GetStartText() { SelectionBoundaryPoint& pt = GetLogicalStartPoint(); SelectionPointWordInfo word_info(pt, FALSE); return word_info.GetWord() + word_info.GetOffsetIntoWord(); }
	const uni_char*	GetEndText() { SelectionBoundaryPoint& pt = GetLogicalEndPoint(); SelectionPointWordInfo word_info(pt, FALSE); return word_info.GetWord() + word_info.GetOffsetIntoWord(); }
	const uni_char*	GetStartWord() { SelectionBoundaryPoint& pt = GetLogicalStartPoint(); SelectionPointWordInfo word_info(pt, FALSE); return word_info.GetWord(); }
	const uni_char*	GetEndWord() { SelectionBoundaryPoint& pt = GetLogicalEndPoint(); SelectionPointWordInfo word_info(pt, FALSE); return word_info.GetWord(); }

	/**
	 *
	 * @param logical_start_char_index The hit position if hit on left
	 * @param logical_end_char_index The hit position if hit on right
	 * @param hit_right_half If TRUE then the end value should be used, otherwise the start value
	 */
	void		SetCurrentIndex(HTML_Element* last_intersected, int logical_start_char_index, int logical_end_char_index, BOOL hit_right_half);
	void		SetMousePosition(const OpPoint& screenpos);

	/**
	* Clears current selection.
	* Must be called before the SVGDocumentContext keeping the selection is destroyed, because of OpInputContext.
	*
	* @param invalidate If FALSE no invalidations are made even if the selection is cleared.
	*/
	void		ClearSelection(BOOL invalidate = TRUE);

	BOOL		IsValid() const { return textselection_info.is_valid; }
	BOOL		IsSelecting() const { return m_text_element && GetTextRootContainer() && textselection_info.is_selecting; }

	HTML_Element* GetElement() const { return m_text_element; }

	SelectionBoundaryPoint&	GetLogicalStartPoint() { return m_start.Precedes(m_end) ? m_start : m_end; }
	SelectionBoundaryPoint&	GetLogicalEndPoint() { return m_start.Precedes(m_end) ? m_end : m_start; }

	/** Copy selected text to a buffer.
	    @param buffer The buffer in which to put the selected text. This parameter may be NULL, if
		it is the caller's intention to just get the number of characters in the text selection. If
		non-NULL, this string buffer will always be NUL-terminated. If the currently selected text
		is longer than what there's room for in 'buffer' (the capacity specified by the
		'buffer_length' parameter), 'buffer' will only contain the 'buffer_length-1' first
		characters of the selected text.
		@return number of characters selected and a copy in 'buffer' if it isn't NULL.
	*/
	int			GetText(TempBuffer* buffer);

	BOOL		IsEmpty();

	OP_STATUS	GetIntersected(const OpPoint& screenpos, HTML_Element*& intersected_elm, int& intersected_glyph_index);

	/**
	 * Checks if a text selection contains given point (in document coordinates).
	 *
	 * @param doc The document containing the selection.
	 * @param doc_point The point to be checked if it's contained by the selection.
	 */
	BOOL Contains(SVG_DOCUMENT_CLASS* doc, const OpPoint& doc_point);

	/**
	 * Checks if a text selection contains given TextSelectionPoint.
	 *
	 * @param sel_point TextSelectionPoint to be checked if it's contained by the selection.
	 *
	 * @see TextSelectionPoint.
	 */
	BOOL Contains(const SelectionBoundaryPoint& sel_point);

	/**
	 * Retrieves a list of rectangles enclosing the selection. The rectangles are in in the text root's userspace.
	 *
	 * @param list The list of rectangles to be filled in. Its elements  must be freed by a caller.
	 */
	void GetRects(OpVector<SVGRect>& list);

	/** Gets cursor's point */
	SelectionBoundaryPoint& GetCursor() { return m_cursor; }
};

# endif // SVG_SUPPORT_TEXTSELECTION
#endif // SVG_TEXT_SELECTION_H
