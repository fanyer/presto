/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _HTML_DOCUMENT_
#define _HTML_DOCUMENT_

#include "modules/doc/doctypes.h"
#include "modules/doc/css_mode.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"
#include "modules/layout/layout_workplace.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/windowcommander/OpViewportController.h"

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
#  include "modules/util/OpHashTable.h"
#  include "modules/doc/searchinfo.h" // Needed for the deletion in OpHashTable
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

#ifdef LINK_TRAVERSAL_SUPPORT
# include "modules/util/adt/opvector.h"
#endif // LINK_TRAVERSAL_SUPPORT

class TextSelection;
class SelectionBoundaryPoint;
class OpElementInfo;
class FramesDocument;

#ifndef USEMAP_BORDER_SIZE
# define USEMAP_BORDER_SIZE	2
#endif // USERMAP_BORDER_SIZE

#define SET_NEW_ELEMENT(variable, new_value) \
    variable = new_value; \
    if (variable != NULL) \
        variable->SetReferenced(TRUE)

/* Forward declaration for the recorded_mouse_actions list. */
class RecordedPointAction;

/**
 * Returns the given element's parent being scrollable, or NULL if the element
 * doesn't have a scrollable ancestor.
 */
class ScrollableArea* GetScrollableParent(HTML_Element* helm);

/**
 * A class representing the inner parts of a visible document. Can be
 * seen as a sub part of FramesDocument. Don't be tricked by the name,
 * it can show HTML, XHTML, WML or any generic XML document.
 *
 * @author Too many. From GI to the one hired the last week.
 */
class HTML_Document
{
	DocumentManager*	doc_manager;
	FramesDocument*		frames_doc;

private:

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	OpPointerHashTable<HTML_Element, SelectionElm>
						selections_map;
	AutoDeleteList<SelectionElm>
						selections;
	SelectionElm*		active_selection;
	OpString			last_search_txt;
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

	TextSelection*		search_selection;
	AutoNullElementRef	current_area_element;
	AutoNullElementRef	current_form_element;
	BOOL				image_form_selected;

	/**
	 * Returns the element that should be highlighted or NULL. This is typically the
	 * navigation element.
	 *
	 * @returns The element to highlight or NULL.
	 */
	HTML_Element*		CalculateElementToHighlight();

	/**
	 * Draws the current highlight depending on current configuration, element
	 * type or some other information. Passes the routine to
	 * LayoutWorkplace::DrawHighlight in some cases.
	 *
	 * @see LayoutWorkplace::DrawHighlight
	 *
	 * @param highlight_rects The rectangles that cover the highlight area. Should
	 * be valid if num_highlight_rects is greater than 0.
	 * @param num_highlight_rects The number of the rectangles that cover the
	 * highlight area. Used mostly for the inline elements, which can span more
	 * than one line. Zero is allowed - in such case a single rect will be
	 * retrieved using the layout workplace.
	 * @param highlight_overflow_clip_rect A clip rect of the potential highlight
	 * (e.g. may come from cotainers with overflow other than visible).
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS			DrawHighlight(OpRect *highlight_rects, int num_highlight_rects, const OpRect& highlight_overflow_clip_rect);

	/**
	 * Invalidates the parts of the document that must be repainted for the highlight.
	 */
	void				InvalidateHighlightArea();

	CSSMODE				css_handling;

	/**
	 * The element targetted by the ref part of the url,
	 * as in www.opera.com#foo. We need to remember this
	 * to reset the :target pseudo class on it
	 * when moving away from it.
	 */
	AutoNullElementRef	url_target_element;
	/**
	 * An element currently hovered by the mouse. It's not being updated during drag'n'drop.
	 * but an immediate user selection element is updated instead.
	 * @see GetImmediateSelectionElement()
	 */
	AutoNullElementRef	hover_html_element;
	AutoNullElementRef	hover_pseudo_html_element;
	AutoNullElementRef	hover_referencing_html_element;
	AutoNullElementRef	active_html_element;
	AutoNullElementRef	active_pseudo_html_element;

	BOOL				show_img;

    BOOL				all_loaded_sent;
	BOOL				send_all_loaded;

	BOOL				loading_finished;

	AutoNullElementRef	focused_element;

	/// Current element when doing keyboard navigation (either with spatnav, tab, or Q/A-navigation)
	/// Merge of what was known as highlighted element and current element previously
	AutoNullElementRef	navigation_element;

	/** Should we highlight the navigation_element?
	 *
	 * The default value is FALSE. Can't be TRUE if there is no
	 * navigation element currently.
	 * This flag is accessed during document display routine, so can be used to
	 * simply cancel drawing the highlight when the element was already
	 * highlighted in some way (depends on configuration and element type).
	 */
	BOOL				navigation_element_is_highlighted;

	/** Keeps track of the latest formobject with selection. */
	HTML_Element*		element_with_selection;
public:
	const void*			current_focused_formobject; // compile fix. spatial_navigation tries to access this but the real pointer is in FramesDocument.

	AutoNullElementRef	captured_widget_element; ///< If not NULL, the formswidget should have the mousemove events until mouseup.
private:

	BOOL			area_selected;

	class ScrollElementElmRef : public ElementRef
	{
	public:
		ScrollElementElmRef() : m_saved_scroll_to_text_offset(-1) {}

		virtual	void	OnDelete(FramesDocument *document) { Reset(); m_saved_scroll_to_text_offset = -1; }
		virtual	void	OnRemove(FramesDocument *document) { return OnDelete(document); }
		virtual void	OnInsert(FramesDocument *old_document, FramesDocument *new_document) { OnDelete(old_document); }

		int				GetScrollToTextOffset() { return m_saved_scroll_to_text_offset; }
		void			SetScrollToTextOffset(int offset) { m_saved_scroll_to_text_offset = offset; }
		OpPoint			GetScrollToPointOffset() const { return m_saved_scroll_to_point_offset; }
		void			SetScrollToPointOffset(const OpPoint& p) { m_saved_scroll_to_point_offset = p; }
		OpRect			GetScrollToRect() const { return m_saved_scroll_to_rect; }
		void			SetScrollToRect(const OpRect& r) { m_saved_scroll_to_rect = r; }
		OpViewportRequestListener::Align
						GetScrollToTextAlign() const { return m_saved_scroll_to_text_align; }
		void			SetScrollToTextAlign(OpViewportRequestListener::Align a) { m_saved_scroll_to_text_align = a; }

	private:
		int				m_saved_scroll_to_text_offset;
		/** Position of point-of-interest relative to the origin of the saved
		 * element, in document pixels. */
		OpPoint			m_saved_scroll_to_point_offset;
		/** Bounding box of saved element. */
		OpRect			m_saved_scroll_to_rect;
		/** Alignment of text in saved element's container. */
		OpViewportRequestListener::Align
						m_saved_scroll_to_text_align;
	};

	// Scroll to this element and text offset after reflow
	ScrollElementElmRef	saved_scroll_to_element;

	unsigned		record_mouse_actions;
	BOOL			replaying_recorded_mouse_actions;
	List<RecordedPointAction> recorded_mouse_actions;

	/* last mouse action yielded, so replaying mouse actions has priority. */
	BOOL			mouse_action_yielded;

#ifndef MOUSELESS
	int				last_mousemove_x, last_mousemove_y;
#endif

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	/**
	 *
	 * @todo Examine and possibly improve raw performance or perceived
	 * performance.
	 */
	OP_BOOLEAN		FindAllMatches(SearchData *data, HTML_Element *highlighted_elm, int highlight_pos);

	/**
	 * Used to update the selected status of a search hit
	 * @param selection The selection you want to update
	 * @param enable TRUE if you want to set it to selected
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS		UpdateSearchHit(SelectionElm *selection, BOOL enable);
# endif // SEARCH_MATCHES_ALL
#endif // !HAS_NO_SEARCHTEXT

	/**
	 * Helper method for HighlightElement and FocusElement. Regardless of how
	 * we move navigation to an area element, the same code should run.
	 *
	 * @param[in] helm a html:area element.
	 *
	 * @returns The element with the usemap attribute (typically
	 * an image or object). Can be used to scroll to it for instance.
	 */
	HTML_Element* SetAreaElementAsNavigationElement(HTML_Element* helm);

#if defined DRAG_SUPPORT && defined GADGET_SUPPORT
	/**
	 * Checks if any parent of frm_doc has control regions defined on
	 * coordinates x, y.
	 * @note This function relies on FramesDocElem::GetAbsX/Y, which may not always
	 * be valid(though they should always be if frm_doc is inside visual viewport).
	 * If you want to use it make sure FramesDocElem::GetAbsX/Y will be valid in
	 * your case.
	 *
	 * @param frm_doc FramesDocument which parents will be checked.
	 * @param x[out] x coordinate relative to frm_doc.
	 * @param y[out] y coordinate relative to frm_doc.
	 * @param control_region_found[out] set to TRUE if control region was found in parent docs.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS CheckControlRegionOnParents(FramesDocument* frm_doc, int x, int y, BOOL &control_region_found);
#endif // defined DRAG_SUPPORT && defined GADGET_SUPPORT
#ifdef DRAG_SUPPORT
	/** Current drag'n'drop target element (the element which will get ondrop event if the drop occurs) */
	AutoNullElementRef current_target_elm;
	/**
	 * An element selected currently by a user using the pointing device's cursor during the drag'n'drop operation.
	 * This element is like the hover element for mouse events.
	 */
	AutoNullElementRef immediate_selection_elm;
	/** An element selected previously by a user using the pointing device's cursor during the drag'n'drop operation. */
	AutoNullElementRef prev_immediate_selection_elm;
#endif // DRAG_SUPPORT
public:

	/**
	 * Constructor. Stores the DocumentManager, the url, the start
	 * width and height for the document.
	 *
	 * @param frm_doc The FramesDocument that owns this HTML_Document,
	 * must not be NULL.
	 *
	 * @param doc_manager The owning doc_manager, must not be NULL.
	 *
	 */
						HTML_Document(FramesDocument* frm_doc, DocumentManager* doc_manager);
						~HTML_Document();

	/**
	 * This handles a rel part of an url (the part after the hash sign
	 * (#) and can be changed several times for a document. The
	 * rel_name is used for different things in different documents,
	 * for HTML we scroll to a named anchor with the same name, in SVG
	 * it describes, or modifies, what and how the SVG is shown.
	 *
	 * For HTML, see HTML5 6.11.8 Navigating to a fragment identifier
	 *
	 * @param[in] rel_name A string, not NULL but may be empty.
	 *
	 * @param alternative_rel_name If rel_name is not found, try this
	 * string instead. By HTML5 it will only find elements of
	 * type HE_A with the correct name attribute. May be NULL.
	 *
	 * @param[in] scroll A boolean, TRUE to scroll and FALSE to not
	 * scroll. Default value is TRUE.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 *
	 * @see DocumentManager::JumpToRelativePosition
	 */
	OP_STATUS			SetRelativePos(const uni_char* rel_name, const uni_char* alternative_rel_name, BOOL scroll);

	/**
	 * Remove the target pseudo class on the old target element and
	 * apply on the new if not NULL.
	 *
	 * @param target_element the element to apply target to or NULL if
	 * no target.
	 */
	void ApplyTargetPseudoClass(HTML_Element *target_element);

	/**
	 * There are two types of documents, HTML_Document and
	 * FramesDocument. This can be used to check which this Document
	 * is. Normally this shouldn't be needed since the code shouldn't
	 * keep HTML_Documents and FramesDocument pointers at the same
	 * place.
	 *
	 * @returns DOC_HTML since this is HTML_Document.
	 *
	 * @deprecated No use for this method anymore.
	 */
	DocType				Type() const { return DOC_HTML; }

	/**
	 * Call this when the document is about to be removed from the
	 * display to passivate everything active on it. This used to be
	 * called Undisplay(BOOL, BOOL) but has been renamed to avoid
	 * mixing it with Document::Undisplay().
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
    OP_STATUS           UndisplayDocument();

	/**
	 * Paints the document on the VisualDevice. This method should
	 * only be called from FramesDocument.
	 *
	 * @param rect The part to repaint. This is in document
	 * coordinates relative the upper left corner of the view.
	 *
	 * @param vd The VisualDevice to paint on. This might be another
	 * VisualDevice than the document's normal VisualDevice.
	 */
    OP_DOC_STATUS		Display(const RECT& rect, VisualDevice* vd);

	/**
	 * This method restarts various things we stopped when going away
	 * from it.  I.e. it is run when we go back/forward in history
	 * and will restart things like background sounds, refresh timers etc.
	 * in the document we're going to.
	 * 
	 * This function used to be known as RestoreForms, but since it didn't
	 * do anything related to forms it was eventually (after a decade or so)
	 * renamed to something which is hopefully more descriptive.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
    OP_STATUS           ReactivateDocument();

	/**
	 * Returns the width of the document. This is the width the layout
	 * engine has given the root container.
	 *
	 * @returns The width of the document in document coordinates.
	 *
	 * @see Height()
	 *
	 * @see FramesDocument::Width()
	 */
    int					Width();

	/**
	 * Returns the height of the document. This is the height the
	 * layout engine has given the root container.
	 *
	 * @returns The height of the document in document coordinates.
	 *
	 * @see Width()
	 *
	 * @see FramesDocument::Height()
	 */
    long				Height();

	/**
	 * For RTL (right-to-left) documents, it's possible that content
	 * overflows to the left of x=0, so that you can scroll to the
	 * left from the initial scroll position. If that's the case, this
	 * returns the amount.
	 *
	 * @see Document::Width();
	 */
	int					NegativeOverflow() const;

#ifndef MOUSELESS

	/**
	 * The generic "handle mouse events" code. This may not be
	 * synchronous. In case of scripts, events may be recorded here
	 * for replay later on.
	 *
	 * @param[in] event The type of mouse event, MOUSEMOVE, MOUSEUP,
	 * MOUSEDOWN, MOUSEWHEEL and others I don't recall now.
	 *
	 * @param[in] x The x coordinate of the mouse action in document
	 * coordinates.
	 *
	 * @param[in] y The y coordinate of the mouse action in document
	 * coordinates.
	 *
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportX()
	 *
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportY()
	 *
	 * @param[in] button_or_delta If it's a button press this is a mix
	 * of a sequence count and a button identifier. Use the macros
	 * EXTRACT_MOUSE_BUTTON and EXTRACT_SEQUENCE_COUNT to get the
	 * different parts. If it's a mouse wheel action, it says how far
	 * and in what direction the mouse wheel was spun.
	 *
	 * @param[in] shift_pressed TRUE if the shift key was pressed when
	 * doing the mouse action.
	 *
	 * @param[in] control_pressed TRUE if control key was pressed when
	 * doing the mouse action.
	 *
	 * @param[in] alt_pressed TRUE if alt key was pressed when doing
	 * the mouse action.
	 *
	 * @param[in] outside_document Set this to TRUE if the action
	 * really appeared outside the document and the document is only
	 * told to reset such things as hover.
	 *
	 * @param[in] simulated TRUE if injected by touch handler to simulate
	 * the actions of a mouse.
	 *
	 * @param[in] radius Touch radius. Used for simulated events. Given
	 * in document units.
	 */
    DocAction	MouseAction(DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int button_or_delta, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, BOOL outside_document = FALSE, BOOL simulated = FALSE, int radius = 0);

	/**
	 * Should be called when the mouse pointer leaves the document so
	 * that we know that we should stop expecting mouse messages and
	 * update the currently hovered element.
	 */
	void		MouseOut();

	/**
	 * While handling a mouse event through scripts we mustn't let any
	 * other mouse event pass since the order between mouse events are
	 * important. While processing a mouse event we store (record) all
	 * other mouse events here and when that mouse event is processed
	 * we replay them into the event system.
	 *
	 * <p> This is the method that initiates the replay and is
	 * triggered by HTML_Element::HandleEvent when it has processed a
	 * mouse event.
	 *
	 * <p> When touch events are enabled, they also use this mechanism
	 * and queue.
	 */
	void				ReplayRecordedMouseActions();

	void				CleanRecordedMouseActions();

#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
	/**
	 * The generic "handle touch events" code. This may not be
	 * synchronous. In case of scripts, events may be recorded here
	 * for replay later on.
	 *
	 * @param[in] event The type of touch event, TOUCHSTART, TOUCHMOVE,
	 * TOUCHEND or TOUCHCANCEL.
	 *
	 * @param[in] id The numeric id of the touch.
	 *
	 * @param[in] x The x coordinate of the touch action in document
	 * coordinates.
	 *
	 * @param[in] y The y coordinate of the touch action in document
	 * coordinates.
	 *
	 * @param[in] visual_viewport_x The x offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's x offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportX()
	 *
	 * @param[in] visual_viewport_y The y offset of the visual viewport, in document coordinates.
	 * Note that this must be the visual vieport's y offset and not the rendering view port's one.
	 * @see GetVisualViewport()
	 * @see GetVisualViewportY()
	 *
	 * @param[in] radius The radius of the touch in document units.
	 *
	 * @param[in] sequence_count_and_button_or_delta Contains click, button and sequence information.
	 *
	 * @param[in] shift_pressed TRUE if the shift key was pressed when
	 * doing the mouse action.
	 *
	 * @param[in] control_pressed TRUE if control key was pressed when
	 * doing the mouse action.
	 *
	 * @param[in] alt_pressed TRUE if alt key was pressed when doing
	 * the mouse action.
	 *
	 * @param[in] user_data Platform supplied event identifier. See
	 * TWEAK_DOC_RETURN_TOUCH_EVENT_TO_SENDER.
	 *
	 * @return DOC_ACTION_HANDLED or DOC_ACTION_NONE.
	 */
    DocAction	TouchAction(DOM_EventType event, int id, int x, int y, int visual_viewport_x, int visual_viewport_y, int radius, int sequence_count_and_button_or_delta, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, void* user_data = NULL);
#endif // TOUCH_EVENTS_SUPPORT

	/**
	 * Resets all cached information and sets all HTML_Element
	 * pointers to NULL to reduce memory usage to a minimum and
	 * dependency on other elements to zero since they will also go
	 * away. The only caller should be FramesDocument::Free.
	 *
	 * @param[in] unused Just don't put anything here.
	 */
	BOOL				Free();

	/**
	 * To be called when loading stops. See FramesDocument who is the
	 * only one that should call this method.
	 *
	 * Tells the LogicalDocument that loading has stopped/should stop
	 * and if format is TRUE, also sends ONLOAD if it should be sent.
	 *
	 * @param format The only effect of this is that the ONLOAD event
	 * might be sent when it's TRUE and not when it's FALSE.
	 *
	 * @param aborted TRUE if the load should end immediately
	 * and with as little fuss as possible. FALSE if the load should
	 * stop cleanly with a document ready to be used.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 *
	 * @see FramesDocument::StopLoading()
	 */
    OP_STATUS           StopLoading(BOOL format, BOOL aborted);

#ifndef HAS_NOTEXTSELECTION

	/**
	 * Sets a new selection replacing the current selection.
	 *
	 * @param[in] start The start point of the selection. Not NULL.
	 *
	 * @param[in] end The end point of the selection. Not NULL. May be
	 * earlier in the document than start.
	 *
	 * @param[in] end_is_focus If TRUE, end will be made focus point and
	 * start will be anchor point. If FALSE, then it will be the opposite.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS			SetSelection(const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, BOOL end_is_focus = FALSE);

	/**
	 * Returns the two edges of the text selection, anchor and
	 * focus. Anchor is where text selection started and focus where
	 * the current end posision is. The order in the document can be
	 * any.
	 *
	 * @param[out] anchor The start of the selection.
	 *
	 * @param[out] focus The end of the selection (can be earlier in
	 * the selection than anchor).
	 *
	 * @returns TRUE if a selection was returned in
	 * anchor/focus. FALSE otherwise.
	 */
	BOOL				GetSelection(SelectionBoundaryPoint &anchor, SelectionBoundaryPoint &focus);

	/**
	 * Selects everything in the document or optionally everything
	 * below a certain element.
	 *
	 * @param[in] select If TRUE, everything below parent (or everything in
	 * the document if parent is NULL) is selected, replacing the
	 * previous selection. If FALSE, the previous selection is removed.
	 *
	 * @param[in] parent If select is TRUE and this is not NULL, only
	 * everything below parent is selected.
	 *
	 * @returns OpBoolean::IS_FALSE if select was FALSE and there was
	 * no previous selection. OpBoolean::IS_TRUE otherwise unless we
	 * encountered an OOM situation in case it returns
	 * OpStatus::ERR_NO_MEMORY.
	 */
	OP_BOOLEAN			SelectAll(BOOL select = TRUE, HTML_Element* parent = NULL);

	/** Checks if given x and y are over a text selection.
	 * This is a heavy operation involving the layout tree traversing thus
	 * causes reflows.
	 *
	 * @param x - the x coordinate to be checked (in document coordinates).
	 * @param y - the y coordinate to be checked (in document coordinates).
	 *
	 * @return OpBoolean::ERR_NO_MEMORY in case of OOM,
	 * OpBoolean::IS_TRUE if (x, y) point is over the selection.
	 * OpBoolean::IS_FALSE othewise.
	 */
	OP_BOOLEAN			IsWithinSelection(int x, int y);

	/**
	 * Removes the current selection and starts a new selection
	 * based in the coordinate sent in. The other point of the
	 * selection is set/changed through later calls to MoveSelectionFocusPoint.
	 *
	 * x and y are in the document coordinate system.
	 *
	 * @param[in] x The document x position of the starting point.
	 *
	 * @param[in] y The document y position of the starting point.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS			StartSelection(int x, int y);

#ifdef KEYBOARD_SELECTION_SUPPORT
	/**
	 * Removes the current selection and sets a collapsed selection
	 * based on the coordinates set in.
	 *
	 * x and y are in the document coordinate system.
	 *
	 * @param[in] x The document x position of the starting point.
	 *
	 * @param[in] y The document y position of the starting point.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS			SetCollapsedSelection(int x, int y);
#endif // KEYBOARD_SELECTION_SUPPORT

	/**
	 * Moves the anchor selection point to the new position. If there is no
	 * active selection a new selection will be initiated at the specified
	 * coordinates. In addition to modifying the text selection this function
	 * will also move the document caret if in documment edit mode.
	 *
	 * x and y are in the document coordinate system.
	 *
	 * @param[in] x The new document x position for the anchor point.
	 *
	 * @param[in] y The new document y position for the anchor point.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS			MoveSelectionAnchorPoint(int x, int y);

	/**
	 * Moves the current ending position of the selection. It can be
	 * changed through later calls to MoveSelectionFocusPoint.
	 *
	 * @param x The document x position of the new ending point.
	 *
	 * @param y The document y position of the new ending point.
	 *
	 * @param[in] end_selection TRUE if this seems to be the last
	 * MoveSelectionFocusPoint in a sequence of MoveSelectionFocusPoint calls.
	 * If it's TRUE and we have a delayed decision, we'll force a
	 * collapsed selection somewhere anyway.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 *          OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS			MoveSelectionFocusPoint(int x, int y, BOOL end_selection);

	/**
	 * Selects all selectable content of the specified element, including its
	 * children.
	 *
	 * This function will always clear any existing selection no matter if it
	 * succeeds with selecting the specified element or not.
	 *
	 * @param [in] elm Element to select.
	 *
	 * @return OpStatus::OK if the operation was successfull and the element
	 *         was selected. If the element didn't contain any selectable
	 *         content or if an error occurred OpStatus::ERR is returned. If
	 *         OOM OpStatus::ERR_NO_MEMORY is returned.
	 */
	OP_STATUS			SelectElement(HTML_Element* elm);

	/**
	 * Selects the word at the specified coordinates. If no word exists
	 * at the point the nearest word will be picked for selection.
	 *
	 * @param[in] x The document x position.
	 *
	 * @param[in] y The document y position.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 *          OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS			SelectWord(int x, int y);

	/**
	 * Returns the selected text in the given buffer. Check with
	 * GetSelectedTextLen how big the buffer has to be, but make sure
	 * to add room for a terminating NULL.
	 *
	 * @param buf The buffer where text will be put in.
	 *
	 * @param buf_len The size of the buffer in the buf parameter.
	 *
	 * @param include_element_selection If a selection in an element
	 * should be returned, or if the method should only bother
	 * with selections in the document text. Elements can either
	 * be form elements or SVG.
	 *
	 * @returns TRUE if a selection could be retrieved, FALSE
	 * otherwise, for example if there was nothing selected.
	 */
	BOOL				GetSelectedText(uni_char *buf, long buf_len, BOOL include_element_selection = FALSE);

	/**
	 * See GetSelectedText.
	 *
	 * @param[in] include_element_selection If a selection in an element
	 * should be returned or if the method should only consider
	 * selections in the document text. Elements can either
	 * be form elements or SVG.
	 *
	 * @returns The length of the selected text, not including any
	 * terminating NULL.
	 */
	long				GetSelectedTextLen(BOOL include_element_selection = FALSE);

	/**
	 * Returns if there are any selected text.
	 *
	 * @param[in] include_element_selection If a selection in an element
	 * should be returned or if the method should only consider
	 * selections in the document text. Elements can either
	 * be form elements or SVG.
	 *
	 * @returns TRUE if there is a selection, FALSE otherwise.
	 */
	BOOL				HasSelectedText(BOOL include_element_selection = FALSE);

	/**
	 * Expands the selection so that if a part of a word is selected,
	 * the whole word will get selected, or the whole sentence or the
	 * whole paragraph. All depending on the selection_type.
	 *
	 * If there is no current text selection, nothing will happen.
	 *
	 * @param[in] selection_type How much to expand, for instance
	 * TEXT_SELECTION_SENTENCE.
	 */
	void				ExpandSelection(TextSelectionType selection_type);

#endif // !HAS_NOTEXTSELECTION

	/**
	 * This method makes sure that the search selection doesn't have any
	 * reference to the element. Used when elements are removed from
	 * the tree.
	 *
	 * @param[in] element The element to remove references too.
	 */
	void				RemoveFromSearchSelection(HTML_Element* element);

	/**
	 * This method makes sure that the current selections (normal text
	 * selection and search hit text selection) don't have any cached
	 * data that depnds on the current layout tree. Used when elements
	 * are reflowed/have their layout box recreated.
	 *
	 * @param[in] The element to remove layout references too.
	 */
	void			RemoveLayoutCacheFromSearchHits(HTML_Element* element);

# ifdef SEARCH_MATCHES_ALL
	/**
	 * Removes all references to |element| in the search hits.
	 *
	 * @param[in] element The element that must be removed from
	 * the search hits.
	 */
	void			RemoveElementFromSearchHit(HTML_Element *elm);
# endif // SEARCH_MATCHES_ALL

	/**
	 * Removes any current selections.
	 *
	 * @param[in] clear_focus_and_highlight If TRUE (default) removes
	 * the focus and highlight from any elements having those.
	 *
	 * @param clear_search If TRUE (FALSE is the default) also the
	 * search selection will be cleared. Note: Search hits are marked
	 * with selections internally.
	 */
	void			ClearDocumentSelection(BOOL clear_focus_and_highlight = TRUE, BOOL clear_search = FALSE);
#ifndef HAS_NO_SEARCHTEXT

	/**
	 * Remove (only) search hits in current document.
	 */
	void		RemoveSearchHits();

	// Two version of the search API. Either for searching the whole
	// document at once or for searching one hit at a time.
# ifdef SEARCH_MATCHES_ALL
	/**
	 * Either starts a new search or continues a current search
	 * depending on if the data object has the "is new search" flag
	 * set or not. The caller is responsible for keeping that flag
	 * updated as wanted.
	 *
	 * The core external API to this
	 * functionality is Window::HighlightNextMatch.
	 *
	 * @param[in] data The search information. Should be reused
	 * between calls to this method or search might restart.
	 * @param[out] rect Returns the rectangle of the found element
	 * in document coordinates
	 *
	 * @returns OpBoolean::IS_TRUE if a next match was
	 * found. OpBoolean::IS_FALSE if there was no next match.
	 *
	 * @see Window::HighlightNextMatch
	 *
	 */
	OP_BOOLEAN			HighlightNextMatch(SearchData *data, OpRect& rect);

# endif // SEARCH_MATCHES_ALL
	/**
	 * Used to search a documemt. This changes the current search hit.
	 *
	 * @param[in] txt The text to search for. Not NULL.
	 *
	 * @param[in] len The length of the text. Larger than 0.
	 *
	 * @param[in] forward If TRUE searches forward, if FALSE searches
	 * backwards from the current search hit.
	 *
	 * @param[in] match_case If TRUE requires matching case, if FALSE
	 * the search is case insensitive.
	 *
	 * @param[in] words Match whole words if TRUE, match partial words
	 * as well.
	 *
	 * @param[in] next If set to TRUE will continue searching after
	 * first letter of the current search hit. If FALSE the current
	 * search hit will be considered once again.
	 *
	 * @param[in] wrap If TRUE, the search will restart from the
	 * beginning/end of the document in case there was no hit from the
	 * first search and the result of a search from that position will
	 * be returned.
	 *
	 * @param[in] only_links If TRUE only text inside links will be
	 * considered. FALSE (the default) searches all text.
	 *
	 * @param[out] left_x If a hit was found, this will contain the
	 * left edge of the visible area of the hit in document
	 * coordinates.
	 *
	 * @param[out] top_y If a hit was found, this will contain the
	 * top edge of the visible area of the hit in document
	 * coordinates.
	 *
	 * @param[out] right_x If a hit was found, this will contain the
	 * document coordinate for the right edge of the visible area of the hit.
	 *
	 * @param[out] bottom_y If a hit was found, this will contain the
	 * document coordinate for the bottom edge of the visible area of
	 * the hit.
	 *
	 * @returns OpBoolean::IS_TRUE if something was found,
	 * OpBoolean::IS_FALSE if search was completed but nothing was
	 * found and OpStatus::ERR_NO_MEMORY if the seach encountered an
	 * OOM situation.
	 *
	 * @see SearchTextObject in the layout module which does the
	 * actual search.
	 */
	OP_BOOLEAN			SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words, BOOL next, BOOL wrap, BOOL only_links, int &left_x, long &top_y, int &right_x, long &bottom_y);
#endif // !HAS_NO_SEARCHTEXT

	/**
	 * Utility function that checks what element is under a point in
	 * the document. It's recommended to use GetInnerBox at
	 * HTML_Element instead so that this can be removed.
	 *
	 * @param[in] x The x coordinate in document coordinates.
	 *
	 * @param[in] y The y coordinate in document coordinates.
	 *
	 * @param[in] text_nodes Include text nodes.
	 *
	 * @returns The hit HTML_Element or NULL if no element was
	 * hit. Will return the root element (for instance HE_HTML) in
	 * case HE_DOC_ROOT was hit.
	 */
    HTML_Element*		GetHTML_Element(int x, int y, BOOL text_nodes = TRUE);

	/**
	 * Draws a focused/highlighted border around navigation element.
	 *
	 * Asserts navigation element being set.
	 *
	 * @param[in] helm The element to set as current form element.
	 */
	void				InvertFormBorderRect(HTML_Element* helm);

	/**
	 * Highlights the next element of a specified type, looking either
	 * forward or backwards. It highlights ranges of HTML_ElementTypes
	 * which is seldom very useful except for one special case, HE_H1
	 * to HE_H6. This method looks in the active document and will
	 * wrap if nothing is found from the current element.
	 *
	 * @param lower_tag The lowest HTML_ElementType value to include.
	 *
	 * @param upper_tag The highest HTML_ElementType value to include.
	 *
	 * @param forward TRUE if we should look forward, FALSE if we
	 * should look backwards.
	 *
	 * @param all_anchors If TRUE anything that can link or act as an
	 * HE_A shoulid be included, that is also HE_AREA.
	 *
	 * @returns TRUE if an element was selected, FALSE if nothing was
	 * found and the selection cleared.
	 *
	 * @todo Namespace checking, or is this HTML-only and the code
	 * fixed to handle that?
	 */
	BOOL				HighlightNextElm(HTML_ElementType lower_tag, HTML_ElementType upper_tag, BOOL forward, BOOL all_anchors = FALSE);

	/**
	 * Moves highlight/focus to the next tabbable element. There are
	 * some debate regarding what is a tabbable element in Opera. For
	 * instance HE_A without ATTR_TABINDEX is tabbable in other
	 * browsers, but long time users of Opera don't want that and
	 * instead uses spatnav (shift-arrow in normal desktop builds) for
	 * link navigation.
	 *
	 * @param[in] forward If TRUE looks forward from the current
	 * element, if FALSE, looks backward.
	 *
	 * @returns TRUE if there was a "next" element that was
	 * highlighted, FALSE otherwise.
	 */
	BOOL				NextTabElm(BOOL forward);

	/**
	 * Returns the url of the currently selected element in that
	 * document if any.
	 *
	 * @param[out] win_name Out parameter that will contain the target
	 * for the url if an url was found.
	 *
	 * @param[in] unused_parameter This used to return a sub_wid maybe
	 * and was an int&, but is now unused and should not be included.
	 *
	 * @returns An url that is empty if there was no url on the
	 * current element.
	 */
	URL					GetCurrentURL(const uni_char* &win_name, int unused_parameter = 0);

	/**
	 * Makes this document the current document or make it not be the
	 * current document. Should only be called from FramesDocument and
	 * everyone else should call FramesDocument::SetAsCurrentDoc().
	 *
	 * @param state If the document should be made current (TRUE) or
	 * moved to history/oblivion (FALSE).
	 *
	 * @see FramesDocument::SetAsCurrentDoc()
	 */
	void				SetAsCurrentDoc(BOOL state);

	/**
	 * Returns the URL for the background image (ATTR_BACKGROUND) on
	 * the body element or an empty URL if there was no document, no
	 * HTML, no body element or no background attribute.
	 *
	 * @returns An URL that may be empty.
	 */
	URL			GetBGImageURL();

	/**
	 * Clears the currently highlighted/focused element and sends a
	 * BLUR event unless blocked by the relevant parameter. This is
	 * another of those messy functions that may still contain
	 * bugs. It's normally used to prepare for moving the
	 * focus/highlight to somewhere else and in those cases it may be
	 * less than efficient to first clear focus and then set it again
	 * since that may cause duplicate pseudo class recalculations.
	 *
	 * @param[in] send_event If TRUE (default) will send the BLUR
	 * event from the element that loses focus.
	 *
	 * @param[in] clear_textselection If the text selection should be
	 * removed as well (default TRUE).
	 *
	 * @param[in] clear_highlight If set to FALSE (default TRUE), the
	 * highlighted element will be ignored and only the focused
	 * element will be affected.
	 */
	void			ClearFocusAndHighlight(BOOL send_event = TRUE, BOOL clear_textselection = TRUE, BOOL clear_highlight = TRUE);

	/**
	 * Make sure that the specified element is within the visual
	 * viewport, by calculating its bounding rectangle and then pass
	 * it to ScrollToRect(). If the element is inside a scrollable
	 * container it will first scroll the scrollable container and
	 * then the document.
	 *
	 * @param[in] helm The element to scroll to. Not NULL.
	 *
	 * @param[in] align Where on the screen to put the element. See
	 * the SCROLL_ALIGN enum.
	 *
	 * @param[in] strict_align If TRUE, request visual viewport to
	 * change as long as the rectangle isn't positioned exactly where
	 * it should be in the visual viewport, as specified by
	 * 'align'. If FALSE, only request visual viewport to be changed
	 * if the specified rectangle is outside the current visual
	 * viewport (fully or partially).
	 *
	 * @param[in] reason Reason for scrolling.
	 *
	 * @param [in] text_offset If the element is a text element, an offset
	 * into the element can be given.
	 *
	 * @return TRUE if a new visual viewport position was requested
	 * (we needed to scroll), FALSE otherwise.
	 */
	BOOL			ScrollToElement(HTML_Element *helm, SCROLL_ALIGN align, BOOL strict_align, OpViewportChangeReason reason, int text_offset = -1);

	/**
	 * Deprecated. Use the other method that takes ::SCROLL_ALIGN.
	 *
	 * @deprecated
	 */
	void			DEPRECATED(ScrollToElement(HTML_Element *helm, SCROLL_ALIGN align));


	/**
	 * Scroll the saved element into view.
	 */
	void			ScrollToSavedElement();


	/**
	 * Save the element at the specific document position (and various
	 * properties for it) to scroll to after reflow.
	 *
	 * @param[in] pos The position in the document to save the element at
	 */
	void			SaveScrollToElement(OpPoint& pos);

	/**
	 * Save the specified element (and various properties for it) to
	 * scroll to after reflow.
	 *
	 * @param helm The element to scroll to after reflow.
	 *
	 * @param text_offset If non-negative, it specifies the character offset to
	 * scroll to (in case a text node is too large to fit completely within the
	 * view). If non-negative, the specified element must be a text node, or
	 * offset won't make any sense.
	 *
	 * @param point_offset Position of point-of-interest relative to the element.
	 *
	 * @param rect Element bounding box.
	 *
	 * @param text_align Text alignment in container hosting the element.
	 */
	void			SaveScrollToElement(HTML_Element* helm, int text_offset = -1, const OpPoint& point_offset = OpPoint(0, 0), const OpRect& rect = OpRect(0, 0, 0, 0), OpViewportRequestListener::Align text_align = OpViewportRequestListener::VIEWPORT_ALIGN_NONE);

	/**
	 * Helper method for SaveScrollToElement to find a nearby
	 * candidate for element to save and later scroll to.
	 *
	 * @param p where in the document, in document coordinates, to start looking
	 * @param elm contains element if found, otherwise left untouched
	 * @param text_offset contains character offset inside element if found and a text element, if element found but not text then -1, otherwise left untouched
	 * @param text_align contains text alignment of container if element found, otherwise left untouched
	 * @retval OpStatus::ERR_NO_MEMORY if an OOM error occured
	 * @retval OpStatus::OK if no error occured
	 */
	OP_STATUS FindNearbySaveScrollToElement(const OpPoint& p, HTML_Element** elm, int& text_offset, OpViewportRequestListener::Align& text_align);

	/**
	 * This method searches the tree for something that's using this
	 * map. The first found element will be returned.
	 *
	 * @param[in] map The element with the name attribute (typically
	 * an HE_AREA).
	 *
	 * @param[in] root The root to look under.
	 *
	 * @returns The found image or NULL of no such element.
	 */
	HTML_Element*	FindAREAObjectElement(HTML_Element *map, HTML_Element *root);

	/**
	 * enum used for HighlighElement and FocusElement. Could probably
	 * be replaced by the normal focus system focus_reason.
	 */
	enum FocusOrigin
	{
		FOCUS_ORIGIN_UNKNOWN,
		FOCUS_ORIGIN_DOM,
		FOCUS_ORIGIN_STORED_STATE,
		FOCUS_ORIGIN_SPATIAL_NAVIGATION,
		FOCUS_ORIGIN_DEACTIVATED,
		FOCUS_ORIGIN_KEYBOARD,
		FOCUS_ORIGIN_MOUSE
	};

	/**
	 * This is a mess, but this is the method to use if something
	 * should be focused for instance in tab or spatnav navigation or
	 * as an activation event. It will (might) send the necessary
	 * events and might scroll and might clear other disturbing
	 * markings like text selections.
	 *
	 * @param[in] helm The element to focus.
	 *
	 * @param[in] origin What triggered this event. Used to determine,
	 * among other things, whether FOCUS/BLUR and other reasonable
	 * events should be sent.
	 *
	 * @param[in] clear_textselection If TRUE (default), will clear
	 * the current selection to not confuse the user (since focus or
	 * highlight is often displayed with a text selection lookalike
	 * marking).
	 *
	 * @param[in] allow_scroll Scroll to the focused element in case
	 * it outside the viewable area. Greatly recommended.
	 *
	 * @param[in] do_highlight if TRUE will also highlight the focused element,
	 * otherwise it will just mark it as focused, without visual indication for
	 * most elements.
	 *
	 * @returns TRUE if the element was focused, FALSE otherwise.
	 */
	BOOL			FocusElement(HTML_Element* helm, FocusOrigin origin = FOCUS_ORIGIN_UNKNOWN, BOOL clear_textselection = TRUE, BOOL allow_scroll = TRUE, BOOL do_highlight = TRUE);

	/**
	 * One element per document can be the "focused" element and that
	 * element is stored here. This is not directly connected to the
	 * keyboard focus though it would be logical to keep them in sync.
	 *
	 * <p>This sets the focused element to NULL.
	 */
    void            ResetFocusedElement();

	/**
	 * One element per document can be the "focused" element and that
	 * element is stored here. This is not directly connected to the
	 * keyboard focus though it would be logical to keep them in sync.
	 *
	 * <p>This sets the focused element, but it should most often not
	 * be called from code outside the doc module. Use
	 * HTML_Document::Highlight instead unless you already know that
	 * the element already has normal keyboard focus and just want to
	 * keep HTML_Document in sync.
	 *
	 * @param[in] helm The element to make as the focused element.
	 *
	 * @param[in] clear_textselection If text selection should be removed.
	 */
	void			SetFocusedElement(HTML_Element* helm, BOOL clear_textselection = TRUE);

	/**
	 * One element per document can be the "focused" element and that
	 * element is stored here. This is not directly connected to the
	 * keyboard focus though it would be logical to keep them in sync.
	 *
	 * <p>Normally it's better to call FramesDocument::GetFocusElement
	 * if you just want a suitable element to process. That takes much
	 * more elements than the focused element under consideration.
	 *
	 * @returns The element that is the focused element or NULL if no
	 * such exists.
	 *
	 * @see FramesDocument::GetFocusElement()
	 */
	HTML_Element*	GetFocusedElement() { return focused_element.GetElm(); }

	/**
	 * For scripting there should be a document global element that
	 * has the current selection. We store that element here, and let
	 * it be updated by the forms module.
	 *
	 * @param[in] helm The element to make as the element with
	 * selection right now. Typically a forms element. Set to NULL if
	 * there should be no such element.
	 */
	void            SetElementWithSelection(HTML_Element* helm);

	/**
	 * For scripting there should be a document global element that
	 * has the current selection. We store that element here, and let
	 * it be updated by the forms module.
	 *
	 * @returns The element that is the element with selection right
	 * now. Typically a forms element. NULL if no such element.
	 */
	HTML_Element*   GetElementWithSelection();

	/**
	 * This is a mess, but this is the method to use if something
	 * should be highlighted or potentially even focused for instance
	 * in tab or spatnav navigation. It will (might) send the
	 * necessary events and might scroll and might clear other
	 * disturbing markings like text selections.
	 *
	 * @param[in] helm The element to highlight.
	 *
	 * @param[in] send_event Send FOCUS/BLUR and other reasonable
	 * events. Recommended.
	 *
	 * @param[in] focusFormElm If the highlighted element is a form
	 * element, should we rather focus it than just highlight? In that
	 * case use TRUE here.
	 *
	 * @param[in] scroll Scroll to the highlighted element in case it
	 * outside the viewable area. Greatly recommended.
	 *
	 * @param[in, out] update_rect Area in the document to update, or
	 * NULL to update the whole document.  Modified by the function
	 * (expanded slightly).
	 *
	 * @param[in] clear_textselection If TRUE (default), will clear
	 * the current selection to not confuse the user (since highlight
	 * is often displayed with a text selection lookalike marking).
	 *
	 * @param[in] invert_form_border_rect If TRUE (not default), and
	 * a form control is highlighted, a border is drawn around it.
	 *
	 * @deprecated use the FocusOrigin variant instead.
	 */
	BOOL			DEPRECATED(HighlightElement(HTML_Element* helm, BOOL send_event, BOOL focusFormElm = FALSE, BOOL scroll = TRUE, OpRect* update_rect = NULL, BOOL clear_textselection = TRUE, BOOL invert_form_border_rect = FALSE));

	/**
	 * This is a mess, but this is the method to use if something
	 * should be highlighted or potentially even focused for instance
	 * in tab or spatnav navigation. It will (might) send the
	 * necessary events and might scroll and might clear other
	 * disturbing markings like text selections.
	 *
	 * @param[in] helm The element to highlight.
	 *
	 * @param[in] origin What triggered the highlight. Used to make
	 * decisions about which events to send and similar.
	 *
	 * @param[in] focusFormElm If the highlighted element is a form
	 * element, should we rather focus it than just highlight? In that
	 * case use TRUE here.
	 *
	 * @param[in] scroll Scroll to the highlighted element in case it
	 * outside the viewable area. Greatly recommended.
	 *
	 * @param[in, out] update_rect Area in the document to update, or
	 * NULL to update the whole document.  Modified by the function
	 * (expanded slightly).
	 *
	 * @param[in] clear_textselection If TRUE (default), will clear
	 * the current selection to not confuse the user (since highlight
	 * is often displayed with a text selection lookalike marking).
	 *
	 * @param[in] invert_form_border_rect If TRUE (not default), and
	 * a form control is highlighted, a border is drawn around it.
	 */
	BOOL			HighlightElement(HTML_Element* helm, FocusOrigin origin = FOCUS_ORIGIN_UNKNOWN, BOOL focusFormElm = FALSE, BOOL scroll = TRUE, OpRect* update_rect = NULL, BOOL clear_textselection = TRUE, BOOL invert_form_border_rect = FALSE);

	/**
	 * @deprecated Use SetNavigationElement(..., TRUE) for the same
	 * effect as this method.  Or HighlightElement if you actually
	 * want to highlight an element.
	 *
	 * Each document can have a highlighted element. Highlight is like
	 * a weaker kind of focus and can be used for items that can't get
	 * normal focus. It's often displayed with a text selection marker
	 * or a border.
	 *
	 * <p>This method sets an element to be highlighted and resets all
	 * other similar elements (text selection, focus). If you want to
	 * add highlight to an element without modifying text selection
	 * and focus, use SetHighlightedElementOnly.
	 *
	 * <p>For external code, this method shouldn't be used and instead
	 * HighlightElement should be called.
	 *
	 * @param[in] helm An element to highlight or NULL.
	 */
	void			DEPRECATED(SetHighlightedElement(HTML_Element* helm)); // inlined below

	/**
	 * @deprecated  Use SetNavigationElement(..., FALSE) for the same
	 * effect as this method.
	 *
	 * Each document can have a highlighted element. Highlight is like
	 * a weaker kind of focus and can be used for items that can't get
	 * normal focus. It's often displayed with a text selection marker
	 * or a border.
	 *
	 * This method will not affect focus or text selection, which
	 * SetHighlightedElement will.
	 *
	 * @param[in] helm An element to highlight or NULL.
	 */
	void			DEPRECATED(SetHighlightedElementOnly(HTML_Element* helm)); // inlined below

	/**
	 * @deprecated  Use GetNavigationElement() or GetFocusedElement()
	 *
	 * Each document can have a highlighted element. Highlight is like
	 * a weaker kind of focus and can be used for items that can't get
	 * normal focus. It's often displayed with a text selection marker
	 * or a border.
	 *
	 * @returns The highlighted element or NULL if no element is
	 * highlighted.
	 */
	DEPRECATED(HTML_Element*	GetHighlightedElement()); // inlined below

	/**
	 * When doing keyboard navigation (through either spatial navigation,
	 * tab or Q/A-navigation, the last element navigated to will be
	 * the navigation element.  Searching will also set this element with
	 * current search result.
	 */
	HTML_Element*	GetNavigationElement() { return navigation_element.GetElm(); }

	/**
	 * Sets navigation element (used for spatial navigation, tab-navigation,
	 * Q/A-navigation and search results).
	 * Also removes focus from any element which has it already.
	 *
	 * If you want to highlight the element you should consider using
	 * HighlightElement which will also set the navigation element.
	 * @param helm	The new element to use as navigation element
	 * @param unset_focus_element  If TRUE will set FocusElement to NULL
	 */
	void			SetNavigationElement(HTML_Element* helm, BOOL unset_focus_element);

	/**
	 * Returns:
	 * If navigation element is set: should it be highlighted or not.
	 * If navigation element is not set: FALSE.
	 */
	BOOL			IsNavigationElementHighlighted() { OP_ASSERT(!navigation_element_is_highlighted || navigation_element.GetElm()); return navigation_element_is_highlighted; }

	/**
	 * Sets whether the navigation element should be highlighted or not.
	 */
	void			SetNavigationElementHighlighted(BOOL value) { navigation_element_is_highlighted = value; }

	/**
	 * This is the method to call if you want something to happen with
	 * an element. It can be that a link should be followed or a text
	 * field focused or a button pressed. This method can do it all!
	 *
	 * @param[in] helm An element to activate or NULL if the currently
	 * highlighted element should be activated. If there is no
	 * currently highlighted element nothing happens and the method
	 * returns OpBoolean::IS_FALSE.
	 *
	 * @param[in] keystate The modifier keys pressed when the action
	 * is triggered. That might modify the result of the
	 * activation. Links might for instance open in another window.
	 *
	 * @returns OpBoolean::IS_TRUE if the method found an element to
	 * do something at. At the very least it will simulate a click on
	 * elements that have no obvious action. If no element was found
	 * or the element had no suitable action, OpBoolean::IS_FALSE will
	 * be returned. For errors (oom), normal OpStatus codes will be
	 * returned.
	 */
	OP_BOOLEAN		ActivateElement(HTML_Element* helm = NULL, ShiftKeyState keystate = SHIFTKEY_NONE);

	/**
	 * I'm not 100% sure but I think this method is used to degrade
	 * the currently focused element from Focused to Highlighted. It
	 * will also signal a NULL link navigation to OpDocumentListener.
	 *
	 * @todo Figure out what this is for and setup suitable ifdefs
	 * around it.
	 */
	void			DeactivateElement();

	/**
	 * Takes care of some things that should be done when the document
	 * has finished loading. This is a close relative to
	 * FramesDocument::FinishDocument and
	 * DocumentManager::HandleDocFinished and it's mostly random if
	 * things are here or in any of those methods.
	 *
	 * <p>Things done here include wand fillin and starting background
	 * sounds and plugins.
	 *
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS       HandleLoadingFinished();

	/**
	 * The active HTML_Element is the element has armed with a mouse
	 * down. If the user does mouse up over the armed (active)
	 * HTML_Element it will trigger a click. A mouse up elsewhere will
	 * not trigger any click and the active HTML_Element will be
	 * reset.
	 *
	 * @returns The element or NULL.
	 */
	HTML_Element*	GetActiveHTMLElement() const { return active_html_element.GetElm(); }
	/**
	 * The active HTML_Element is the element has armed with a mouse
	 * down. If the user does mouse up over the armed (active)
	 * HTML_Element it will trigger a click. A mouse up elsewhere will
	 * not trigger any click and the active HTML_Element will be
	 * reset.
	 *
	 * @param[in] active The element or NULL.
	 */
    void			SetActiveHTMLElement(HTML_Element* active);

	/**
	 * This is the element that we last put :active psuedo class
	 * on. We need to remember that to undo that :active when we later
	 * get another active elment or remove that style for another
	 * reason. This element may or may not be the same as the one from
	 * GetActiveHTMLElement.
	 *
	 * @returns The element or NULL.
	 */
	HTML_Element*	GetActivePseudoHTMLElement() const { return active_pseudo_html_element.GetElm(); }

	/**
	 * @see     HTML_Document::SetHoverHTMLElement()
	 * @returns The element or NULL.
	 */
	HTML_Element*	GetHoverHTMLElement() const { return hover_html_element.GetElm(); }

	/**
	 * We don't really care where the mouse pointer is when doing
	 * things like mouse down but instead we remember which element we
	 * last moved the mouse over (but see UpdateMouseMovePostion if something
	 * other than mouse actions set hover_html_element).
	 *
	 * That is the "hover element" and will be activated by mouse down.
	 * This is changed with mousemove events.
	 *
	 * @param[in] new_elm The element or NULL.
	 * @param[in] apply_hover_style  If TRUE will change styling to the css-specified
	 *            hover-styling (if any).  Should be TRUE if a mouse action sets the,
	 *            hover element, but not for if it's a result of a keyboard action
     */
    void			SetHoverHTMLElement(HTML_Element *new_elm, BOOL apply_hover_style=TRUE);


#ifndef MOUSELESS
	/**
	 * When SetHoverHTMLElement is set by something other than as a result
	 * of a mouse movement, the mouse won't notice that it is positioned
	 * over another element until it is moved again.  That means that e.g.
	 * any mouse clicks will go to the hover element even if the mouse is
	 * somewhere else.
	 *
	 * This function allows the caller to tell HTML_Document the real position
	 * of the hover_element (otherwise it assumes it is under the mouse pointer)
	 * so that further mouse action knows that the pointer is somewhere else
	 * than the hover element.
	 *  */
	void			UpdateMouseMovePosition(int x, int y) { last_mousemove_x = x; last_mousemove_y = y; }
#endif // !MOUSELESS

	/**
	 * Returns the element that the current fragment identifier of the
	 * document's url points to.
	 *
	 * @returns The element or NULL.
	 */
	HTML_Element*	GetURLTargetElement() const { return url_target_element.GetElm(); }

	/**
	 * See SetHoverHTMLElement for normal usage. This is only used for
	 * the special case where the hover is moved up to a parent since
	 * the original element is going away. In that case some things
	 * can, and should not be done.
	 *
	 * @param[in] hover The new hover element. Must be a parent of
	 * the current hover element.
	 */
	void			SetHoverHTMLElementSilently(HTML_Element* hover);

	/**
	 * This is the element that we last put :hover pseudo class on. We
	 * need to remember that to undo that :hover when we later get
	 * another hover element. This element may or may not be the same
	 * as the one from GetHoverHTMLElement.
	 *
	 * @returns The element or NULL.
	 */
	HTML_Element*	GetHoverPseudoHTMLElement() const { return hover_pseudo_html_element.GetElm(); }

	/**
	 * If hovering something with a usemap attribute, the part that we
	 * hover might reference something else in the document. This is
	 * that something else.
	 *
	 * @returns The element or NULL.
	 */
	HTML_Element*	GetHoverReferencingHTMLElement() const { return hover_referencing_html_element.GetElm(); }

	/**
	 * If hovering something with a usemap attribute, the part that we
	 * hover might reference something else in the document. This is
	 * that something else.
	 *
	 * @param[in] new_elm The element or NULL.
	 */
    void			SetHoverReferencingHTMLElement(HTML_Element *new_elm) { hover_referencing_html_element.SetElm(new_elm); }

#ifndef MOUSELESS
	/**
	 * Checks the current position of the mouse and updates the hover
	 * flags as required.
	 *
	 * This is a complex operation and can not be called inside a
	 * reflow or focus operation. Should not be called from outside
	 * doc.
	 */
	void            RecalculateHoverElement();
#endif // MOUSELESS

	/**
	 * @deprecated Use GetNavigationElement
	 *
	 * In a document one element can be considered "current",
	 * i.e. focused/highlighted so that non-targetted actions should
	 * operate on that element. That element is stored hereso that we
	 * know for which element to draw highlight for and to get an URL
	 * for in GetCurrentURL.
	 *
	 * @returns The HTML_Element (for instance an HE_A) that is
	 * currently highlighted/focused, or NULL.
	 */
    DEPRECATED(HTML_Element*   GetCurrentElement()); // inlined below

	/**
	 * @deprecated Use SetNavigationElement(..., FALSE) to get the same
	 * functionallity.
	 *
	 * Change which element is currently focused/highlighted or set to
	 * NULL.
	 *
	 * @param[in] helm The element to set as currently
	 * focused/highlighted element or NULL if no element is
	 * focused/highlighted.
	 */
    void            DEPRECATED(SetCurrentElement(HTML_Element *helm)); // inlined below

	/**
	 * If an area element is focused/highlighted, that element is
	 * stored here so that we know for which element to draw highlight
	 * for and to get an URL for in GetCurrentURL.
	 *
	 * @returns The HTML_Element (area element) that is currently
	 * highlighted/focused, or NULL.
	 */
    HTML_Element*   GetCurrentAreaElement() { return current_area_element.GetElm(); }

	/**
	 * Change which area element is currently focused/highlighted or
	 * set to NULL.
	 *
	 * @param[in] new_elm The element to set as currently
	 * focused/highlighted area element or NULL if no area element is
	 * focused/highlighted.
	 */
    void            SetCurrentAreaElement(HTML_Element *new_elm) { current_area_element.SetElm(new_elm); }

	/**
	 * If a form element is focused/highlighted, that element is
	 * stored here so that we know for which element to draw highlight
	 * for.
	 *
	 * @returns The HTML_Element (form element) that is currently
	 * highlighted/focused, or NULL.
	 */
    HTML_Element*   GetCurrentFormElement() { return current_form_element.GetElm(); }

	/**
	 * Change which form element is currently focused/highlighted or
	 * set to NULL.
	 *
	 * @param[in] new_elm The element to set as currently
	 * focused/highlighted form element or NULL if no form element is
	 * focused/highlighted.
	 */
    void            SetCurrentFormElement(HTML_Element *new_elm) { current_form_element.SetElm(new_elm); }

	/**
	 * Checks if anything but text is selected.
	 *
	 * @returns TRUE if anything more complicated than text is
	 * selected, for instance form elements, plugins or
	 * images. Returns FALSE otherwise.
	 */
	BOOL            GetImageFormSelected() { return image_form_selected; }

	/**
	 * Sometimes a widget (formobject) want to have mouse events that
	 * happens outside itself. In those cases its HTML_Element can be
	 * registered as capturing widget element, though all that is a
	 * mess, and it's probably enough or even required to set it as
	 * hooked_widget (see the widget module) as well.
	 *
	 * @returns The HTML_Element registered as capturing widget
	 * element. May be NULL.
	 *
	 * @todo Figure out how this actully works and clean it up.
	 */
    HTML_Element*   GetCapturedWidgetElement() { return captured_widget_element.GetElm(); }

	/**
	 * This can register (or unregister) an element as capturing
	 * widget element.
	 *
	 * @param[in] new_elm The HTML_Element, may be NULL.
	 *
	 * @see GetCapturedWidgetElement().
	 */
    void            SetCapturedWidgetElement(HTML_Element *new_elm) { captured_widget_element.SetElm(new_elm); }

#ifdef SAVE_DOCUMENT_AS_TEXT_SUPPORT
	/**
	 * Serializes the document into a stream.
	 *
	 * @param[in] stream The document will be saved to this stream if
	 * it's not NULL. The only exception is text files that will be
	 * saved directly to a file named by the parameter fname if that
	 * is non-NULL.
	 *
	 * @param[in] fname The name of a file to save to. This is only
	 * used if the document is a text file or if stream is NULL. If
	 * this is NULL, the stream is always used.
	 *
	 * @param[in] force_encoding A charset to use instead of the
	 * document's charset.
	 *
	 * @returns OpStatus::OK if the operation finished successfully.
	 */
	OP_STATUS SaveDocAsText(UnicodeOutputStream* stream, const uni_char* fname=NULL, const char *force_encoding=NULL);
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

	/**
	 * Every HTML_Document is owned by a FramesDocument. This method
	 * returns the FramesDocument owning this HTML_Document.
	 *
	 * @returns The owning FramesDocument.
	 */
	FramesDocument*	GetFramesDocument() { return frames_doc; }

	/**
	 * Returns the area in the view that is affected by the selection
	 * to minimize the amount we repaint.
	 *
	 * @param[out] x The upper left x coordinate in document
	 * coordinate scale, relative the upper left corner of the view.
	 *
	 * @param[out] y The upper left y coordinate in document
	 * coordinate scale, relative the upper left corner of the view.
	 *
	 * @param[out] width The width in document coordinate scale.
	 *
	 * @param[out] height The height of the selection in document
	 * coordinate scale.
	 *
	 * @returns TRUE if there was a selection, FALSE otherwise. If it
	 * returns FALSE, no out parameters are set.
	 */
	BOOL	GetSelectionBoundingRect(int &x, int &y, int &width, int &height);

#ifdef LINK_TRAVERSAL_SUPPORT
	/**
	 * Returns all explicit links in a document. Can be used for doing
	 * auto surfers or listing links in the UI.
	 *
	 * @param[in, out] vector The address of an empty vector that will
	 * be filled with OpElementInfo elements owned by the vector (thus
	 * letting it be an OpAutoVector instead of an OpVector). Links
	 * might be missing in case of lack of memory.
	 */
	void			GetLinkElements(OpAutoVector<OpElementInfo>* vector);
#endif // LINK_TRAVERSAL_SUPPORT

	/**
	 * Returns the current selection. If this is NULL or if the
	 * returned TextSelection returns TRUE on its IsEmpty() method,
	 * then nothing is selected.
	 *
	 * @returns NULL or a TextSelection (that may be empty).
	 */
	TextSelection*	GetTextSelection() { return frames_doc->GetTextSelection(); }

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	/**
	 * @param set_active If TRUE, the added hit will be the active search hit
	 * @returns OpStatus::OK if operation completed successfully and
	 * an error code otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS		AddSearchHit(const SelectionBoundaryPoint *start, const SelectionBoundaryPoint *end, BOOL set_active = FALSE);

	/**
	 * Record a connection between a HTML_Element and a SelectionElm
	 * so that we later when looking at the HTML_Element easily can
	 * find the SelectionElm and see if/how it's selected.
	 *
	 * @param[in] elm The HTML_Element to connect to the SelectionElm.
	 *
	 * @param[in] selection The SelectionElm to connect to the HTML_Element.
	 *
	 * @returns OpStatus::OK if success and an error code
	 * otherwise. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS		MapSearchHit(HTML_Element *elm, SelectionElm *selection);

	/**
	 * Call this to forget all current selection data.
	 */
	OP_STATUS		ClearSelectionElms();

	/**
	 * Check if the HTML_Element is recorded as having some selection
	 * data and return that in that case.
	 *
	 * @param[in] he The element that may or may not have a selection.
	 *
	 * @returns A SelectionElm if there was a selection or NULL
	 * otherwise.
	 */
	SelectionElm*	GetSelectionElmFromHe(HTML_Element *he);

	/**
	 * Returns the first in the list of SelectionElm:s (search hits).
	 *
	 * @returns A pointer to the first SelectionElm or NULL.
	 */
	SelectionElm*	GetFirstSelectionElm() { return (SelectionElm*) selections.First(); }
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

#if defined(SUPPORT_DEBUGGING_SHELL)
	/**
	 * Accessor method for use in the debugging shell.
	 *
	 * @returns The all_loaded_sent internal member.
	 *
	 * @todo Candidate for making an external API with module.export.
	 */
    BOOL AllLoadedSent() { return all_loaded_sent; }

	/**
	 * Accessor method for use in the debugging shell.
	 *
	 * @returns The send_all_loaded internal member.
	 *
	 * @todo Candidate for making an external API with module.export.
	 */
	BOOL SendAllLoaded() { return send_all_loaded; }
#endif // SUPPORT_DEBUGGING_SHELL

	/**
	 * Removes all references to |element| so that it can be safely
	 * deleted without any risk for crashes.
	 *
	 * @param[in] element The element HTML_Document must forget all
	 * about since it will go away.
	 */
	void CleanReferencesToElement(HTML_Element* element);

	/**
	 * Called before any text change to give the HTML_Document a chance to protect
	 * its contents against HE_TEXTGROUP/HE_TEXT changes.
	 *
	 * @param text_node The text or text group element that is about to change.
	 */
	void BeforeTextDataChange(HTML_Element* text_node);

	/**
	 * Called if a text change signalled with BeforeTextDataChange() couldn't be completed.
	 *
	 * @param text_node The text or text group element that was about to change.
	 */
	void AbortedTextDataChange(HTML_Element* text_node);

	/**
	 * Tell the HTML_Document that the text element changed in the specified way.
	 * This is preceded by a call to BeforeTextDataChange.
	 *
	 * @param text_element The HTML_Element that just changed.
	 *
	 * @see HTML_Element::SetText() for the documentation of the other arguments.
	 */
	void TextDataChange(HTML_Element* text_element, HTML_Element::ValueModificationType modification_type, unsigned offset, unsigned length1, unsigned length2);

	/**
	 * Internal docxs function that must be called when the parser or something else causes
	 * a HE_TEXT element to be converted to a HE_TEXTGROUP element.
	 *
	 * @param elm The element that used to be an HE_TEXT but is now an HE_TEXTGROUP.
	 *
	 * @see HTML_Element::AppendText().
	 */
	void			OnTextConvertedToTextGroup(HTML_Element*elm);

	/**
	 * Only updates the security state based on the main url.
	 *
	 * @param[in] include_loading_docs FALSE if only documents with
	 * state NOT_LOADING should be considered.
	 *
	 * @see DocumentManager::UpdateSecurityState(BOOL include_loading_docs)
	 */
	void	UpdateSecurityState(BOOL include_loading_docs);

	/**
	 * Every FramesDocument is owned by a DocumentManager (and every
	 * HTML_Document is owned by a FramesDocument that is owned by a
	 * DocumentManager). This returns a pointer to that
	 * DocumentManager.
	 *
	 * @returns A pointer to the DocumentManager owning this
	 * FramesDocument (or the DocumentManager owning the
	 * FramesDocument owning this HTML_Document).
	 *
	 * @see DocumentManager
	 */
	DocumentManager*	GetDocManager() { return doc_manager; }

	/**
	 * Every document tree hangs on a DocumentManager owned by a Window.
	 * This is a utility function to retrieve that Window and
	 * is equivalent to GetDocManager()->GetWindow().
	 *
	 * @returns the Window that ultimately owns this document.
	 *
	 * @see DocumentManager::GetWindow()
	 */
	Window*				GetWindow() { return doc_manager->GetWindow(); }

	/**
	 * Every DocumentManager has a VisualDevice that it uses to
	 * draw and interact with the documents. This is a utility function
	 * to extract that VisualDevice pointer. It's equivalent to
	 * GetDocManager()->GetVisualDevice().
	 *
	 * @returns the VisualDevice object
	 *
	 * @see DocumentManager::GetVisualDevice();
	 */
	VisualDevice*		GetVisualDevice() { return doc_manager->GetVisualDevice(); }

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
	/**
	 * Finds the interactive items that are near to the given rectangle in this doc.
	 * See FEATURE_NEARBY_INTERACTIVE_ITEM_DETECTION.
	 * @param[in] rect The rectangle to find items within, in document
	 *				   coordinates. Must be fully inside the part of the visual viewport
	 *				   of the frames_doc of that doc that is visible on the physical screen.
	 * @param[out] list A list to store the information about the interactive items in.
	 *					The elements stored will be of type InteractiveItemInfo. They will be either
	 *					in doc root coordinates or in local coordinates of a transform context
	 *					with a transformation matrix to the doc root attached.
	 *					Each InteractiveItemInfo element is guaranted to have at least zero length url.
	 *					The caller is responsible for the list cleanup regardless of the call result.
	 * @returns OpStatus::OK
	 *			OpStatus::ERR_NO_MEMORY in case of OOM.
	 */
	OP_STATUS			FindNearbyInteractiveItems(const OpRect& rect, List<InteractiveItemInfo>& list);

	/**
	 * Finds links that are near to the given rectangle in this doc.
	 * Recurses into sub documents. See FEATURE_NEARBY_INTERACTIVE_ITEM_DETECTION.
	 * @param rect The rectangle to find link within, in this document
	 *			   coordinates. Must be fully inside the part of the visual viewport
	 *			   of this doc that is visible on the physical screen.
	 * @param[out] list A list to store pointers of the HTML_Elements that
	 *					represent the links. The caller does not gain ownership
	 *					of the pointers in the list.
	 * @returns OpStatus::OK
	 *			OpStatus::ERR_NO_MEMORY in case of OOM.
	 */
	OP_STATUS			FindNearbyLinks(const OpRect& rect, OpVector<HTML_Element>& list);
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#ifdef SELFTEST
	void				GetSavedScrollToElement(HTML_Element** elm, int& text_offset)
	{
		*elm = saved_scroll_to_element.GetElm();
		text_offset = saved_scroll_to_element.GetScrollToTextOffset();
	}
#endif // SELFTEST

#ifdef DRAG_SUPPORT
	/**
	 * Returns current d'n'd target element.
	 *
	 * @see current_target_elm
	 */
	HTML_Element* GetCurrentTargetElement() { return current_target_elm.GetElm(); }

	/**
	 * Returns the element currently selected by the pointing device's cursor.
	 * This element is like the hover element for mouse events.
	 *
	 * @see immediate_selection_elm
	 */
	HTML_Element* GetImmediateSelectionElement() { return immediate_selection_elm.GetElm(); }

	/**
	 * Returns the element previously selected by the pointing device's cursor.
	 *
	 * @see prev_immediate_selection_elm
	 */
	HTML_Element* GetPreviousImmediateSelectionElement() { return prev_immediate_selection_elm.GetElm(); }

	/**
	 * Sets current d'n'd target element.
	 *
	 * @see GetCurrentTargetElement()
	 */
	void SetCurrentTargetElement(HTML_Element* elm) { current_target_elm.SetElm(elm); }

	/**
	 * Sets the element currently selected by the pointing device's cursor.
	 *
	 * @see GetImmediateSelectionElement()
	 */
	void SetImmediateSelectionElement(HTML_Element* elm) { immediate_selection_elm.SetElm(elm); }

	/** Sets the element previously selected by the pointing device's cursor.
	 *
	 * @see GetPreviousImmediateSelectionElement()
	 */
	void SetPreviousImmediateSelectionElement(HTML_Element* elm) { prev_immediate_selection_elm.SetElm(elm); }
#endif // DRAG_SUPPORT
};

/* gcc 3 < 3.4 has trouble with combining DEPRECATED and inline; so separate
 * DEPRECATED declaration from inline definition.
 */
inline void HTML_Document::SetHighlightedElement(HTML_Element* helm)	 { SetNavigationElement(helm, TRUE); }
inline void HTML_Document::SetHighlightedElementOnly(HTML_Element* helm) { SetNavigationElement(helm, FALSE); }
inline void HTML_Document::SetCurrentElement(HTML_Element *helm)		 { SetNavigationElement(helm, FALSE); }
inline HTML_Element* HTML_Document::GetHighlightedElement() { return GetNavigationElement(); }
inline HTML_Element* HTML_Document::GetCurrentElement()		{ return GetNavigationElement(); }

#endif /* _HTML_DOCUMENT_ */
