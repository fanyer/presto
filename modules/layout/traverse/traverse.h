/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file traverse.h
 *
 * Class prototypes for layout structure traversal.
 *
 * @author Geir Ivars�y
 * @author Karl Anders �ygard
 *
 */

#ifndef _TRAVERSE_H_
#define _TRAVERSE_H_

class Content_Box;
class VerticalBox;
class Text_Box;
class TableColGroupBox;
class TableRowBox;
class TableCellBox;
class TableCollapsingBorderRowBox;
class TableCollapsingBorderCellBox;
class TableCollapsingBorderContent;
class MarqueeContainer;
class InlineBox;
class PositionedInlineBox;
class BlockBox;
class FloatingBox;
class LayoutProperties;
class Line;
class ScrollableArea;
class Column;
#ifdef PAGED_MEDIA_SUPPORT
class PagedContainer;
#endif // PAGED_MEDIA_SUPPORT
class MultiColumnContainer;
class HTMLayoutProperties;
#ifdef SEARCH_MATCHES_ALL
class SelectionElm;
#endif // SEARCH_MATCHES_ALL
class Content;
class ButtonContainer;

struct LineSegment;
class WordInfo;

#ifdef NEARBY_ELEMENT_DETECTION
class ElementOfInterest;
class AnchorElementOfInterest;
class FormElementOfInterest;
class TextAnchorFragment;
#endif // NEARBY_ELEMENT_DETECTION

#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/opelminfo.h"
#include "modules/doc/frm_doc.h"
#include "modules/display/vis_dev.h"
#include "modules/util/OpHashTable.h"
#include "modules/layout/layout_coord.h"
#include "modules/layout/element_search.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/multicol.h"
#include "modules/display/color.h"
#include "modules/util/simset.h"
#include "modules/logdoc/selection.h"

#ifndef INT32_MAX
# define INT32_MAX 0x7fffffff
#endif

#ifndef INT32_MIN
#define INT32_MIN (-INT32_MAX - 1)
#endif

#ifndef UINT8_MAX
#define UINT8_MAX 0xff
#endif

class TextSelection;
class SelectionBoundaryPoint;

/** Find next (below from_element) container element of given element. */

HTML_Element* FindNextContainerElementOf(HTML_Element* from_element, HTML_Element* element);

typedef enum
{
	TRAVERSE_ONE_PASS,		// all in one pass
	TRAVERSE_BACKGROUND,	// background of block boxes without space_manager
	TRAVERSE_CONTENT		// content of boxes without space_manager
} TraverseType;

/** TraverseInfo object */

class TraverseInfo
{
public:

					TraverseInfo()
					  : has_clipped(FALSE),
						has_buffered(FALSE),
						is_real_clip_rect(FALSE),
						dry_run(FALSE),
						rescued_outline_inside_inline(FALSE),
						pretend_noscroll(FALSE),
						old_suppress_elements(FALSE),
						old_ancestor_containers_locked(TRUE),
						ellipsis_width(0),
						has_ellipsis_props(FALSE),
						skip_this_inline(FALSE),
						handle_overflow(FALSE) {}

public:

	BOOL			has_clipped;
	BOOL			has_buffered;
	AbsoluteBoundingBox
					old_line_area;
	OpRect			old_clip_rect;

	/** Needed for PaintObject, to be able to postpone the table overflow hidden
		clipping application to EnterTableRow. */

	OpRect			old_table_content_clip_rect;

	BOOL			is_real_clip_rect; // real means smaller than the whole document
	BOOL			dry_run;
	BOOL			rescued_outline_inside_inline;

	/** If TRUE, don't translate by the current scrolling in scrollable
		containers. Valid and readable after a call to
		EnterScrollable() on the traversal object. */

	BOOL			pretend_noscroll;

	/** Used by ElementSearchObject to know when to report found elements again. */

	BOOL			old_suppress_elements;

	/** Used by ElementSearchObject to know when leaving a container should unlock the stored open elements. */

	BOOL			old_ancestor_containers_locked;

	short			ellipsis_width;
	BOOL			has_ellipsis_props;
	BOOL			skip_this_inline;

	/** Handle overflow during traverse rather that when entering a box */

	BOOL			handle_overflow;
};

/** Root translation and scroll position of nearest positioned ancestor. */

class RootTranslationState
{
public:

					RootTranslationState()
						: translate_root_y(0),
						  root_scroll_y(0),
						  translate_root_x(0),
						  root_scroll_x(0) {}

public:

	/* translate_root_{x,y} is relative to the document origin or
	   the nearest transformed element. */

	LayoutCoord		translate_root_y;
	LayoutCoord		root_scroll_y;
	LayoutCoord		translate_root_x;
	LayoutCoord		root_scroll_x;
};

/** Column or page traversal state.

	When doing multiple traversal passes on a block box, each pass must start
	with the same pane state. This class is here to ensure this when a block
	lives in multiple columns or pages. */

class PaneTraverseState
{
	friend class TraversalObject;

	/** Remaining distance before we can start traversing the column / page. */

	LayoutCoord		start_offset;

	/** Column / page traversal started? */

	BOOL			started;

	/** Column / page traversal done? */

	BOOL			done;
};

/** Target traversal state.

	When traversing multiple columns, each column traversal must start with the
	same target state. This class is here to ensure this when a block lives in
	multiple columns. */

class TargetTraverseState
{
	friend class TraversalObject;

	/** Target element. */

	HTML_Element*	target_element;

	/** The child container that is an ancestor of the target element. */

	HTML_Element*	next_container_element;

	/** @see TraversalObject::off_target_path */

	BOOL			off_target_path;

	/** @see TraversalObject::current_target_container */

	Container*		current_target_container;

	/** @see TraversalObject::target_intersection_checking */

	BOOL			target_intersection_checking;
};

struct TranslationState;

/** Traversal object */

class TraversalObject
{
private:

	TraverseType	type;

	BOOL			elements_skipped;

	short			word_index; // used to optimize traversing of text boxes

	HTML_Element*	text_element; // used to optimize traversing of text boxes

	LayoutCoord		word_pos;

	LayoutCoord		old_root_x;

	LayoutCoord		old_root_y;

	LayoutCoord		scroll_x;

	LayoutCoord		scroll_y;

	LayoutCoord		fixed_pos_x;

	LayoutCoord		fixed_pos_y;

#ifdef PAGED_MEDIA_SUPPORT
	LayoutCoord		fixed_pos_print_y;
#endif

	LayoutCoord		root_scroll_x;

	LayoutCoord		root_scroll_y;

	LayoutCoord		pane_translate_x;

	LayoutCoord		pane_translate_y;

	BOOL			out_of_memory;

	BOOL			enter_hidden;

	FloatingBox*	first_float;
	FloatingBox*	last_float;

	PaneFloatList	pane_floats;

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** @return TRUE if traversal of dirty trees is allowed. */

	virtual BOOL	AllowDirtyTraverse() { return FALSE; }

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

protected:

	/** The current column / page we're aiming at traversing, if any. */

	Column*			current_pane;

	/** Remaining distance before we can start traversing the column / page. */

	LayoutCoord		pane_start_offset;

	HTML_Element*	target_element;

	BOOL			target_done;

	/** Clip rectangle of the current column / page, relative to the document root. */

	OpRect			pane_clip_rect;

	/** Traversal of the current column / page started? */

	BOOL			pane_started;

	/** Traversal of the current column / page done? */

	BOOL			pane_done;

	/** TRUE if we're traversing in an inline formatting context right now. */

	BOOL			inline_formatting_context;

	FramesDocument*	document;

	/** Next container element, cached for optimisations. */

	HTML_Element*	next_container_element;

	/** Used by AreaTraversalObject.

		The flag indicates that we currently have a target element set, but we are
		not on the path to it. We are not on the path if target is done or we are
		not in an ancestor of the target currently. */

	BOOL			off_target_path;

	/** Used by AreaTraversalObject.

		TRUE if we should perform intersection checks on our way to the target. */

	BOOL			target_intersection_checking;

	/** Used by AreaTraversalObject.

		The container of the current inline box target. It is used to catch the
		moment in which, during searching for the target, we reached that container.
		So this can happen when entering a new container or leaving a box, whose
		containing element has a container.

		Non NULL when all of the following is TRUE:
		1) target_intersection_checking is TRUE
		2) target_element's box is an inline box
		3) We haven't reached that container yet (gets NULLED then). */

	Container*		current_target_container;

public:
					TraversalObject(FramesDocument* doc)
					  : type(TRAVERSE_BACKGROUND),
						elements_skipped(FALSE),
						word_index(0),
						text_element(NULL),
						word_pos(0),
						old_root_x(0),
						old_root_y(0),
						scroll_x(0),
						scroll_y(0),
						fixed_pos_x(0),
						fixed_pos_y(0),
#ifdef PAGED_MEDIA_SUPPORT
						fixed_pos_print_y(0),
#endif
						root_scroll_x(0),
						root_scroll_y(0),
						pane_translate_x(0),
						pane_translate_y(0),
						out_of_memory(FALSE),
						enter_hidden(FALSE),
						first_float(NULL),
						last_float(NULL),
						current_pane(NULL),
						pane_start_offset(0),
						target_element(NULL),
						target_done(FALSE),
						pane_started(FALSE),
						pane_done(FALSE),
						inline_formatting_context(FALSE),
						document(doc),
						next_container_element(NULL),
						off_target_path(FALSE),
						target_intersection_checking(FALSE),
						current_target_container(NULL) {}
	virtual			~TraversalObject() {}

	/** Traverse document. */

	OP_STATUS       Traverse(HTML_Element* root, Head* props_list = NULL, BOOL allow_reflow=TRUE);

	/** Get document */

	FramesDocument*	GetDocument() const { return document; }

	/** Get LayoutWorkplace. */

	LayoutWorkplace*
					GetLayoutWorkplace() const { return document->GetLogicalDocument()->GetLayoutWorkplace(); }

	/** Set the current column in a multicol container or page in a paged container that we're aiming at traversing. */

	void			SetCurrentPane(Column* pane) { current_pane = pane; }

	/** Get the current column or page we're aiming at traversing, if any. */

	Column*			GetCurrentPane() const { return current_pane; }

	/** Set remaining distance before we can start traversing the column or page. */

	void			SetPaneStartOffset(LayoutCoord offset) { pane_start_offset = offset; }

	/** Get remaining distance before we can start traversing the column or page. */

	LayoutCoord		GetPaneStartOffset() const { return pane_start_offset; }

	/** Return TRUE if we're handling a column or page. */

	BOOL			IsInMultiPane() const { return current_pane != NULL; }

	/** Set clip rectangle of the current column or page. */

	void			SetPaneClipRect(const OpRect& clip_rect);

	/** Return TRUE if this element starts before the start of the current column / page. */

	BOOL			TouchesCurrentPaneStart(LayoutProperties* layout_props) const;

	/** Return TRUE if this element stops after the stop of the current column / page. */

	BOOL			TouchesCurrentPaneStop(LayoutProperties* layout_props) const;

	/** Return TRUE if this element crosses either of the boundaries of the current column / page. */

	BOOL			TouchesCurrentPaneBoundaries(LayoutProperties* layout_props) const { return IsInMultiPane() && (TouchesCurrentPaneStart(layout_props) || TouchesCurrentPaneStop(layout_props)); }

	/** Get clip rectangle of the current column or page. */

	const OpRect&	GetPaneClipRect() const { return pane_clip_rect; }

	/** Set distance from top/left edge of a pane to the document root. */

	void			SetPaneTranslation(LayoutCoord x, LayoutCoord y) { pane_translate_x = x; pane_translate_y = y; }

	/** Get distance from top/left edge of a pane to the document root. */

	void			GetPaneTranslation(LayoutCoord& x, LayoutCoord& y) const { x = pane_translate_x; y = pane_translate_y; }

	/** Store column / page traversal state.

		When traversing a box that require multiple traversal passes
		(background, content, floats, Z elements) and may live in multiple
		columns/rows or pages, we have to restore the pane traversal state
		before each pass. */

	void			StorePaneTraverseState(PaneTraverseState& state) const
	{
		state.start_offset = pane_start_offset;
		state.started = pane_started;
		state.done = pane_done;
	}

	/** Restore column / page traversal state. */

	void			RestorePaneTraverseState(const PaneTraverseState& state)
	{
		/* Only do this if we're actually traversing a multi-column or paged
		   container. It's a waste of time otherwise. */

		if (current_pane)
		{
			pane_start_offset = state.start_offset;
			pane_started = state.started;
			pane_done = state.done;
		}
	}

	/** Store target traversal state.

		When traversing a multi-column or paged container, the target may live
		in multiple columns, rows or pages, and we have to restore the target
		traversal state before traversing each column/row/page. */

	void			StoreTargetTraverseState(TargetTraverseState& state) const
	{
		state.target_element = target_element;
		state.next_container_element = next_container_element;
		state.off_target_path = off_target_path;
		state.current_target_container = current_target_container;
		state.target_intersection_checking = target_intersection_checking;
	}

	/** Restore target traversal state. */

	void			RestoreTargetTraverseState(const TargetTraverseState& state)
	{
		target_element = state.target_element;
		next_container_element = state.next_container_element;
		off_target_path = state.off_target_path;
		target_done = FALSE;
		current_target_container = state.current_target_container;
		target_intersection_checking = state.target_intersection_checking;
	}

	/** Get target */

	HTML_Element*	GetTarget() const { return target_element; }

	/** Is the specified element the target or an ancestor of the target?

		If we're traversing in logical order, there is no such thing as a
		target. See the parameter allow_logical for behavior in that case.

		@param target The element in question
		@param allow_logical If TRUE, always return TRUE if we're traversing in
		logical order. If FALSE, always return FALSE if we're traversing in
		logical order.

		@return If traversing in logical order, see parameter allow_logical for
		return value. Otherwise, we return TRUE if the specified element is on
		the way to the target. */

	BOOL			IsTarget(HTML_Element* target, BOOL allow_logical = TRUE) const;

	/** Set target */

	void			SetTarget(HTML_Element* target) { if (!TraverseInLogicalOrder()) target_element = target; }

	/** Set the new target and the first next containing element.

		Should be called when starting traversing to a target from the target origin.

		@param target_origin The element of the box where we iterate through all
			   the target elements that were not yet traversed to and should be
			   traversed to from this place (stacking context or a box from which we
			   traverse floats).
		@param target The new target. */

	void			InitTargetTraverse(HTML_Element* target_origin, HTML_Element* target)
	{
		OP_ASSERT(!off_target_path && !TraverseInLogicalOrder());
		next_container_element = FindNextContainerElementOf(target_origin, target);
		target_element = target;
	}

	/** Is target done */

	BOOL			IsTargetDone() const { return target_done; }

	/** Are we off target path currently ?

		This method can be used to check whether we should continue iterating over
		layout stacks during the traverse. E.g. in a container or in tables.

		@return The current value of off_target_path flag.

		@see off_target_path */

	BOOL			IsOffTargetPath() const
	{
		/* Two things to assert:
		   1) If we are off target path, there must be a target currently.
		   2) If we are on target path, the target must not be done (either we
		   traverse to a target or no target is set. */

		OP_ASSERT(!off_target_path || target_element);
		OP_ASSERT(off_target_path || !target_done);

		return off_target_path;
	}

	/** Possible result of SwitchTarget() method.

		TARGET_SWITCH_NEXT_Z_ELEMENT:
		We proceed to the next target that is a next Z element on the currently
		traversed stacking context. In such case target_done will be FALSE and
		the target_element would be set to the next one.

		TARGET_SWITCH_NEXT_IS_DESCENDANT:
		Same as TARGET_SWITCH_NEXT_Z_ELEMENT + the next target is a descendant of
		the previous one.

		TARGET_SWITCH_OTHER_RESULT:
		Every possible other result. Like proceeding to the next float or decided
		that we have to return to the current target traversal starting place
		(stacking context or a box from which we traverse floats). */

	enum SwitchTargetResult
	{
		SWITCH_TARGET_NEXT_Z_ELEMENT,
		SWITCH_TARGET_NEXT_IS_DESCENDANT,
		SWITCH_TARGET_OTHER_RESULT
	};

	/** Switch traversal target.

		The outcome is that either we proceed to the next target (float or next
		Z element) or set the target_done flag and return to the "target origin".

		Target origin is the box where we iterate through all the target elements
		that were not yet traversed to and should be traversed to from this place
		(stacking context or a box from which we traverse floats).

		@param containing_elm The containing element.
		@param skipped_subtree Subtree to skip (unless NULL). Target candidates in
			   this subtree are skipped, unless they are fixed positioned, or part of
			   the path down to some fixed positioned element.
		@return The information about the result of the call and the possible next target.

		@see SwitchTargetResult */

	virtual SwitchTargetResult
					SwitchTarget(HTML_Element* containing_elm, HTML_Element* skipped_subtree = NULL);

	/** Notify that we are back at the target origin.

		@param box The target origin.

		@see SwitchTarget() */

	virtual void	TargetFinished(Box* box);

	/** Specify that the column or page start element has been found. */

	void			SetPaneStarted(BOOL started) { pane_started = started; }

	/** Has the column or page start element been found? */

	BOOL			IsPaneStarted() const { return pane_started; }

	/** Set column or page done. */

	void			SetPaneDone(BOOL done) { pane_done = done; }

	/** Is column or page done? */

	BOOL			IsPaneDone() const { return pane_done; }

	/** Specify whether we're traversing in an inline formatting context right now. */

	void			SetInlineFormattingContext(BOOL inl) { inline_formatting_context = inl; }

	/** Return TRUE if we're traversing in an inline formatting context right now. */

	BOOL			GetInlineFormattingContext() const { return inline_formatting_context; }

	/** Get word index to start from when traversing a text box. */

	short			GetWordIndex() const { return word_index; }

	/** Set word index to start from when traversing a text box. */

	void			SetWordIndex(short index) { word_index = index; }

	/** Get text element that word_index refer to. */

	HTML_Element*	GetTextElement() const { return text_element; }

	/** Set text element that word_index refer to. */

	void			SetTextElement(HTML_Element* element) { text_element = element; }

	/* Set the position on the virtual line to start from when traversing a text box */

	void            SetWordPosition(LayoutCoord position) { word_pos = position; }

	/* Get the position on the virtual line to start from when traversing a text box */

	LayoutCoord		GetWordPosition() const { return word_pos; }

	/** Get traverse type */

	TraverseType	GetTraverseType() const { return type; }

	/** Set traverse type */

	void			SetTraverseType(TraverseType traverse_type) { type = traverse_type; }

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return FALSE; }

	/** Get x translation. */

	virtual LayoutCoord
					GetTranslationX() const { return LayoutCoord(0); }

	/** Get y translation. */

	virtual LayoutCoord
					GetTranslationY() const { return LayoutCoord(0); }

	/** Translate by offset */

	virtual void	Translate(LayoutCoord x, LayoutCoord y) {}

	/** Set translation */

	virtual void	SetTranslation(LayoutCoord x, LayoutCoord y) {}

#ifdef CSS_TRANSFORMS

	/** Modify local coordinate system using a 2d transform
	 * @return OpBoolean::IS_TRUE - ok, traverse into the transform context
	 *		   OpBoolean::IS_FALSE - do not traverse into the transform context
	 *		   OpBoolean::ERR_NO_MEMORY - in case of OOM. */

	virtual OP_BOOLEAN PushTransform(const AffineTransform &t, TranslationState &state) { return OpBoolean::IS_TRUE; }

	/** Restore local coordinate system one level up the stack */

	virtual void	PopTransform(const TranslationState &state) {}

	/** Get affine transform to closest transform root */

	virtual AffinePos
					GetTransformRoot() const { return AffinePos(); }

	/** Set affine transform to closest transform root */

	virtual void	SetTransformRoot(const AffinePos &ctm) { }

#endif

	/** Get root translation. */

	void			GetRootTranslation(LayoutCoord& x, LayoutCoord& y) const { x = old_root_x; y = old_root_y; }

	/** Set root translation */

	void			SetRootTranslation(LayoutCoord x, LayoutCoord y) { old_root_x = x; old_root_y = y; }

	/** Translate x and y coordinates of the root translation.

	    Root translation is the translation of the current containing block
		for absolutely positioned boxes.  See @ref Translate() for the
		definition of translation. */

	void			TranslateRoot(LayoutCoord x, LayoutCoord y) { old_root_x += x; old_root_y += y; }

	/** Set the fixed positioned offset */

	void			SetFixedPos(LayoutCoord x, LayoutCoord y)	{
														fixed_pos_x = x; fixed_pos_y = y;
#ifdef PAGED_MEDIA_SUPPORT
														fixed_pos_print_y = LayoutCoord(0);
#endif
													}

#ifdef CSS_TRANSFORMS

	/** Read the fixed positioned offset */

	void			GetFixedPos(LayoutCoord &x, LayoutCoord &y) const { x = fixed_pos_x; y = fixed_pos_y; }

#endif

	/** Get amount of horizontal scroll (root viewport and all scrollable containers) included in translation. */

	LayoutCoord		GetScrollX() const { return scroll_x; }

	/** Get amount of vertical scroll (root viewport and all scrollable containers) included in translation. */

	LayoutCoord		GetScrollY() const { return scroll_y; }

	/** Translate scroll position.

		The document itself and scrollable containers have a scroll position, which
		fixed-positioned boxes (and even, to some extent, non-fixed absolutely positioned boxes)
		must cancel. */

	void			TranslateScroll(LayoutCoord x, LayoutCoord y) { scroll_x += x; scroll_y += y; }

	/** Adjust amount of scroll included in root translation. */

	void			TranslateRootScroll(LayoutCoord x, LayoutCoord y) { root_scroll_x += x; root_scroll_y += y; }

	/** Update root scroll to regular scroll translation. Get the difference between old and new translation. */

	void			SyncRootScroll(LayoutCoord &x, LayoutCoord &y) { x = scroll_x - root_scroll_x; root_scroll_x = scroll_x; y = scroll_y - root_scroll_y; root_scroll_y = scroll_y; }

	/** Get amount of root viewport scroll that is to be canceled by fixed-positioned elements. */

	void			GetViewportScroll(LayoutCoord &x, LayoutCoord &y) const;

	/** Update root translation and root scroll to regular translation and scroll. */

	void			SyncRootScrollAndTranslation(RootTranslationState& translation_state);

	/** Restore previous root translation and root scroll. */

	void			RestoreRootScrollAndTranslation(const RootTranslationState& translation_state);

	/** Callback for objects needing to set up code before starting traversal */

	virtual void	StartingTraverse();

	/** Callback for objects needing to run code after traverse has finished */

	virtual void	TraverseFinished();

	/** Flush backgrounds that may not have been painted yet, because of avoid-overdraw optimization. */

	virtual void	FlushBackgrounds(LayoutProperties* layout_props, Box* box) {}

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info) {}

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info) { return TRUE; }

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info) {}

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info) { return TRUE; }

	/** Leave line */

	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info) {}

	/** Enter text box */

	virtual void	EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment) {}

	/** Leave text box */

	virtual void	LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline) {}

	/** Enter container.

		NOTE: Currently called only if target_element is set. */

	virtual BOOL	EnterContainer(LayoutProperties* layout_props, Container* container, LayoutPoint& translation) { return TRUE; }

	/** Leave container.

		NOTE: Currently called only if target_element is set. */

	virtual void	LeaveContainer(const LayoutPoint& translation) {}

	/** Enter scrollable content. */

	virtual BOOL	EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info) { return TRUE; }

	/** Leave scrollable content. */

	virtual void	LeaveScrollable(TraverseInfo& traverse_info) {}
	virtual void	LeaveScrollable(ScrollableArea* scrollable, TraverseInfo& traverse_info) { LeaveScrollable(traverse_info); }

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info) { return TRUE; }

	/** Leave paged container. */

	virtual void	LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info) {}

#endif // PAGED_MEDIA_SUPPORT

	/** Enter multi-column container. */

	virtual void	EnterMultiColumnContainer(LayoutProperties* layout_props, MultiColumnContainer* content) {}

	/** Enter column in multicol container or page in paged container. */

	virtual BOOL	EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info) { return TRUE; }

	/** Leave column in multicol container or page in paged container. */

	virtual void	LeavePane(TraverseInfo& traverse_info) {}

	/** Handle border collapsing for table row. */

	virtual void	HandleCollapsedBorders(TableCollapsingBorderRowBox* row, TableContent* table) {}

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row) { return TRUE; }

	/** Leave table row. */

	virtual void	LeaveTableRow(TableRowBox* row, TableContent* table) {}

	/** Enter table row group. */

	virtual BOOL	EnterTableRowGroup(TableRowGroupBox* rowgroup) { return TRUE; }

	/** Handle table column (group). */

	virtual void	HandleTableColGroup(TableColGroupBox* col_group_box, LayoutProperties* table_lprops, TableContent* table, LayoutCoord table_top, LayoutCoord table_bottom) {}

	/** Handle marquee. */

	virtual OP_STATUS
					HandleMarquee(MarqueeContainer* marquee, LayoutProperties* layout_props) { return OpStatus::OK; }

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height) {}

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj) {}

	/** Handle replaced content */

	virtual void	HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content) {}

	/** Handle bullet content. */

	virtual void	HandleBulletContent(LayoutProperties* layout_props, BulletContent* content) {}

	/** Enter table content. */

	virtual BOOL	EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info) { return TRUE; }

	/** Leave table content. */

	virtual void	LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info) {}

	/** "Enter" layout break. */

	virtual BOOL	EnterLayoutBreak(HTML_Element* break_element) { return FALSE; }

	/** Handle break box
	 *
	 * @param layout_props Properties for the break element.
	 * @param is_layout_break TRUE if this is a LayoutBreak instance (a lonely <br>).
	 *						  FALSE if it is a forced line end (line that ends with <br>).
	 */

	virtual void	HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break) {}

	/** Handle selectable box */

	virtual void	HandleSelectableBox(LayoutProperties* layout_props) {}

	/** Handle empty table cells. */

	virtual void	HandleEmptyCells(const TableRowBox* row, TableContent* table, LayoutProperties* table_lprops) {}

	/** Begin clipping of a partially collapsed table cell. */

	virtual void	BeginCollapsedTableCellClipping(TableCellBox* box, const OpRect& clip_rect, TraverseInfo& traverse_info) {}

	/** End clipping of a partially collapsed table cell. */

	virtual void	EndCollapsedTableCellClipping(TableCellBox* box, TraverseInfo& traverse_info) {}

	/** Were elements skipped? */

	BOOL			GetElementsSkipped() const { return elements_skipped; }

	/** Mark that elements were skipped; may need to recompute cascade. */

	void			SetElementsSkipped(BOOL skipped) { /*elements_skipped = skipped;*/ }

	BOOL			GetEnterHidden() { return enter_hidden; }
	void			SetEnterHidden(BOOL enter) { enter_hidden = enter; }

	/** Check if this positioned element needs to be traversed. */

	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element) { return TRUE; }

	/** Out of memory? */

	BOOL			IsOutOfMemory() const { return out_of_memory; }

	/** Set out of memory. */

	void			SetOutOfMemory() { out_of_memory = TRUE; }

	/** Set next container element. */

	void			SetNextContainerElement(HTML_Element* element) { next_container_element = element; }

	/** Add float to be traversed. */

	void			AddFloat(FloatingBox* floating_box);

	/** Add pane floats (GCPM float inside multicol or paged containers) to be traversed. */

	void			AddPaneFloats(PaneFloatList& float_list);

	/** Traverse floats, both regular CSS 2.1 and GCPM floats. */

	void			TraverseFloats(Box* box, LayoutProperties* layout_props);

	/** Applies clipping */

	virtual void	PushClipRect(const OpRect& cr, TraverseInfo& info) {};

	/** Removes clipping */

	virtual void	PopClipRect(TraverseInfo& info) {};

	/** Does this traversal object support clipping? */

	virtual BOOL	HasClippingSupport() const { return FALSE; }
};

/** Traversal object */

class AreaTraversalObject
  : public TraversalObject
{
private:

	/** Check the intersection between the given rect in local coordinates and the area.

		This method is called as a return value at the end of the five
		Intersects(...) method series and may be overridden by a subclass. */

	virtual BOOL	IntersectsCustom(RECT& rect) const {return Intersects(rect);}

	/** Check the intersection of a positioned inline box that has its containing
	    block calculated for offset purpose
		(PositionedInlineBox::IsContainingBlockCalculatedForOffset()).

		@param target_box The box of the current target element.
			   When computing the offset of the target_box from the current
			   translations, uses the stored last container's translations and the
			   offset of the target_box'es bounding box from its container.
		@param container_element The container we are currently in or we are just entering
			   or the container of the containing element of an inline or a vertical
			   box that is just being left.
		@return TRUE when the intersection check passes, FALSE otherwise. */

	BOOL			Intersects(PositionedInlineBox* target_box, HTML_Element* container_element) const;

	/** Called when current_target_container is set and we have just reached it
		during the traverse (either just entered it or left an inline or a vertical
		box, whose containing element's container is the current_target_container).

		Checks whether it is possible to check the intersection of the current
		inline box target and decides whether to continue traversing to it or call
		SwitchTarget().

		@param container_elm The html element of the reached container.
		@param entering TRUE when entering a container, FALSE when leaving an
			   inline or a vertical box.
		@return FALSE When we decide to switch to a next target and the outcome is that
				either we should return back to the stacking context or the next target
				is not a descendant of the entered container. TRUE otherwise.
				Also always TRUE when entering param is FALSE.

		@see current_target_container */

	BOOL			CheckTargetContainer(HTML_Element* container_elm, BOOL entering);

	/** Called when leaving an inline or a vertical box and in a need to handle
		some target traversal related logic.

		That logic is for example reaching the current_target_container if present
		or setting the next_container_element.

		@param element The element of the box. */

	void			CalculateNextContainingElement(HTML_Element* element);

protected:

	/** The area of the traversal, given in unscaled document coordinates with a possible fixed translation.
		The transform returned in GetVisualDeviceTransform() must reflect that translation, so that
		we test intersections between the rectangles in the same coordinate system.
		Currently there are two classes that implement AreaTraversalObject:
		- HitTestingTraversalObject - in which the area is in unscaled document coordinates (the translation is zero)
		- VisualTraversalObject - in which the area is in unscaled rendering viewport coordinates (the translation is the viewport's top left). */

	RECT			area;

	BOOL			enter_all;

	/** The translation x of the last entered container.
		Stored in EnterContainer(). */

	LayoutCoord		container_translation_x;

	/** The translation y of the last entered container.
		Stored in EnterContainer(). */

	LayoutCoord		container_translation_y;

	/** Checks whether the area rectangle is a single point. */

	virtual BOOL	IsAreaSinglePoint() const {return area.left + 1 == area.right && area.top + 1 == area.bottom;}

	/** Gets the area point in local coordinates. Valid only when the above returns TRUE. */

	virtual OpPoint	GetLocalAreaPoint() const {OP_ASSERT(IsAreaSinglePoint()); return GetVisualDeviceTransform()->ToLocal(OpPoint (area.left, area.top));}

	/** Get the visual device transform. Used for hit testing. */

	virtual const VisualDeviceTransform *GetVisualDeviceTransform() const = 0;

	/** Handle a situation when during traversing to a target, we do not enter
		some box that is an ancestor of it.

		Asserts target_element.

		@param current_containing_elm The containing element that we are currently in.
		@param tested_elm The element of the box that we've just decided not to enter.
		@return TRUE when we proceed to a next Z element target that is a descendant
				of the tested_elm anyway (e.g. a fixed positioned element or an element
				whose stacking context contains fixed positioned elements). */

	BOOL			HandleSkippedTarget(HTML_Element* current_containing_elm, HTML_Element* tested_elm);

public:

					AreaTraversalObject(FramesDocument* doc, const RECT& area)
					  : TraversalObject(doc),
						area(area),
						enter_all(FALSE),
						container_translation_x(0),
						container_translation_y(0) {}

					AreaTraversalObject(FramesDocument* doc)
					  : TraversalObject(doc),
						enter_all(FALSE),
						container_translation_x(0),
						container_translation_y(0) {}

	/** Set to TRUE if all boxes should be entered no matter the area is. */

	void			SetEnterAll(BOOL status) { enter_all = status; }

	/** Switch traversal target. */

	virtual SwitchTargetResult
					SwitchTarget(HTML_Element* containing_elm, HTML_Element* skipped_subtree = NULL);

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Enter container. */

	virtual BOOL	EnterContainer(LayoutProperties* layout_props, Container* container, LayoutPoint& translation);

	/** Leave container. */

	virtual void	LeaveContainer(const LayoutPoint& translation);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Enter column in multicol container or page in paged container. */

	virtual BOOL	EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info);

	/** Leave column in multicol container or page in paged container. */

	virtual void	LeavePane(TraverseInfo& traverse_info);

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Leave table row. */

	virtual void	LeaveTableRow(TableRowBox* row, TableContent* table);

	/** Check if this positioned element needs to be traversed. */

	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element);

	/** Get area that needs to be traversed. */

	const RECT&		GetTraversalArea() const { return area; }

	/** The six Intersects(...) methods below (excluding Intersects(RECT) and Intersects(OpRect))
	 	and IntersectsColumn(...) act in the following way:

		- they compute a bounding rectangle (relative to the current translation, in local coords)
		  connected with a first layout object pointer or their boundary arguments (IntersectsColumn
		  case).
		- they call the IntersectsCustom(RECT) method, passing the computed rectangle and return,
		  what that method has returned. */

	/** Does the Content_Box intersect the current area? */

	BOOL			Intersects(const Content_Box *box, BOOL include_overflow = TRUE) const;

	/** Does the Line intersect the current area? */

	BOOL			Intersects(const Line *line) const;

	/** Does the TableRow intersect the current area? */

	BOOL			Intersects(const TableRowBox *table_row) const;

	/** Does the line segment intersect the current area? */

	BOOL			Intersects(const LineSegment& segment, LayoutCoord x, LayoutCoord word_width, LayoutCoord space_width) const;

	/** Does the Content_Box, positioned with x and y, intersect the current area? */

	BOOL			Intersects(const Content_Box *box, LayoutCoord x, LayoutCoord y, BOOL adjust_for_multicol) const;

	/** Does the inline box intersect the current area? */

	BOOL			Intersects(const InlineBox *inline_box) const;

	/** Do column's boundaries intersect the current area? */

	BOOL			IntersectsColumn(LayoutCoord left_limit, LayoutCoord right_limit) const;

	/** Does the local area defined by `rect' intersect the current area? */

	BOOL			Intersects(const RECT &rect) const;

	/** Does the local area defined by `rect' intersect the current area? */

	BOOL			Intersects(const OpRect &rect) const;
};

class VisualTraversalObject
  : public AreaTraversalObject
{
protected:

	VisualDevice*	visual_device;

	LayoutCoord		translation_x;

	LayoutCoord		translation_y;

	AffinePos		transform_root;

public:

					VisualTraversalObject(FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
						: AreaTraversalObject(doc, area),
						  visual_device(visual_device), translation_x(0), translation_y(0) { }

					VisualTraversalObject(FramesDocument* doc, const RECT& area)
						: AreaTraversalObject(doc, area), translation_x(0), translation_y(0) { visual_device = doc->GetVisualDevice(); }

					VisualTraversalObject(FramesDocument* doc)
						: AreaTraversalObject(doc), translation_x(0), translation_y(0) { visual_device = doc->GetVisualDevice(); }

	/** Get visual device */

	VisualDevice*	GetVisualDevice() const { return visual_device; }


	/* Get visual device transform */

	virtual const VisualDeviceTransform *
					GetVisualDeviceTransform() const { return visual_device; }

	/** Get translations. */

	virtual LayoutCoord
					GetTranslationX() const { return translation_x; }

	virtual LayoutCoord
					GetTranslationY() const { return translation_y; }

	/** Set translation */

	virtual void	SetTranslation(LayoutCoord x, LayoutCoord y) { translation_x = x; translation_y = y; }

#ifdef CSS_TRANSFORMS

	/** Push transform */

	virtual OP_BOOLEAN PushTransform(const AffineTransform &t, TranslationState &state);

	/** Pop transform */

	virtual void	PopTransform(const TranslationState &state);

#endif

	/** Get affine transform to closest transform root */

	virtual AffinePos
					GetTransformRoot() const { return transform_root; }

	/** Set affine transform to closest transform root */

	virtual void	SetTransformRoot(const AffinePos &ctm) { transform_root = ctm; }

	/** Translate x and y coordinates */

	virtual void	Translate(LayoutCoord x, LayoutCoord y) { visual_device->Translate(x, y); translation_x += x; translation_y += y; }

	/** Enter column in multicol container or page in paged container. */

	virtual BOOL	EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info);

	/** Leave column in multicol container or page in paged container. */

	virtual void	LeavePane(TraverseInfo& traverse_info);
};

class RectList;

/** Object used to calculate box edges in absolute values */

class BoxEdgesObject
  : public VisualTraversalObject
{
public:
	enum AreaType {
		CONTENT_AREA,
		PADDING_AREA,
		BORDER_AREA,
		ENCLOSING_AREA, // Will also include child boxes when a rect list is requested
		BOUNDING_AREA,

		/** Very much like BORDER_AREA, but with extra special cases for
			offsetLeft and offsetTop properties, like not including
			scrolling in scrollable containers. */

		OFFSET_AREA,

		/** Special mode that calculates the containing block established
			by relatively positioned inline boxes, but also at the same time
			calculates the bounding area used for deciding if a positioned
			inline box needs to be traversed (used by
			AreaTraversalObject::TraversePositionedElement()). */

		CONTAINING_BLOCK_AREA
	};

private:

	/** The total transform to the nearest transformed ancestor
		element of the found box, including the box ifself.  If there
		are no transformed ancestor element of the box or the box
		itself isn't transformed, it is empty. */

	AffinePos		box_transform_root;

	/** The box edges in document coordinates */

	RECT			box_edges;

	/** The size and position of the containing block established by the
		target box (for CONTAINING_BLOCK_AREA) in document coordinates. */

	RECT			containing_block_rect;

	BOOL			box_found;

	BOOL			box_finished;

	AreaType		area_type;

	HTML_Element*	element;

	RectList* 		box_edges_list;

	int				element_text_start;

	int				element_text_end;

	HTML_Element*	target_ancestor;

	LayoutCoord		target_ancestor_x;

	LayoutCoord		target_ancestor_y;

	/** Add a rect to the list of rectangles. */

	void			AddLocalRect(LayoutCoord x, LayoutCoord y, LayoutCoord width, LayoutCoord height);

	/** Add a rect to the list of rectangles. */

	void			AddLocalRect(const RECT &rect);

	/** In the normal case, calculate the union of 'rect' and the
		current area and save that as the current area. It has a
		number of optional paramteter to control how we calculate the
		union.

		@rect		 The rectangle to add
		@force_left	 When TRUE, regardless of current left edge, move
					 the left edge of the current area to that of
					 'rect' (unless skip_left is TRUE)
	    @force_right When TRUE, regardless of current right edge, move
					 the right edge of the current area to that of
					 'rect' (unless skip_right is TRUE)
	    @skip_left	 When TRUE, don't move the left edge of the current
					 area. Wins over force_left.
	    @skip_right	 When TRUE, don't move the left edge of the current
					 area. Wins for force_right.
		@skip_for_containing_block_rect When calculating containing block area,
										inline descendants should not be included. */

	void			UnionLocalRect(const RECT &rect,
								   BOOL force_left = FALSE,
								   BOOL force_right = FALSE,
								   BOOL skip_left = FALSE,
								   BOOL skip_right = FALSE,
								   BOOL skip_for_containing_block_rect = FALSE);

	/** Set the current area to a rect in local coordinates. */

	void			SetLocalRect(const RECT &rect);

	/** Return a rectangle translated in the current transform
		context, if any.  If there are no transforms, it will be
		relative to the document root. */

	RECT			MakeTranslated(const RECT &rect) const;

public:

		BoxEdgesObject(FramesDocument* doc, HTML_Element* html_element, HTML_Element* container_element, AreaType area_type = BOUNDING_AREA, int element_text_start = 0, int element_text_end = -1)
					  : VisualTraversalObject(doc),
						box_found(FALSE),
						box_finished(FALSE),
						area_type(area_type),
						element(html_element),
						box_edges_list(NULL),
						element_text_start(element_text_start),
						element_text_end(element_text_end),
						target_ancestor(NULL),
						target_ancestor_x(0),
						target_ancestor_y(0)
					{
                        SetTraverseType(TRAVERSE_ONE_PASS);

						box_edges.bottom = LONG_MIN;
						box_edges.top = LONG_MAX;
						box_edges.right = LONG_MIN;
						box_edges.left = LONG_MAX;
						containing_block_rect.bottom = LONG_MIN;
						containing_block_rect.top = LONG_MAX;
						containing_block_rect.right = LONG_MIN;
						containing_block_rect.left = LONG_MAX;
						next_container_element = container_element;

						box_transform_root.SetTranslation(0, 0);
					}

	/** Get box edges in absolute values */

	const RECT&		GetBoxEdges() const { return box_edges; }

	/** @return The calculated size and position of the containing block
				established by the target box. This method only makes sense
				when used with CONTAINING_BLOCK_AREA as the area type. */

	const RECT&		GetContainingBlockRect() const { OP_ASSERT(area_type == CONTAINING_BLOCK_AREA); return containing_block_rect; }

	/** Get box transform */

	const AffinePos& GetBoxCTM() const { return box_transform_root; }

	/** Is target_box found? */

	BOOL			GetBoxFound() const { return box_found; }

	/** Set list to record line boxes into.

        When set, RectListItems are allocated and inserted into this
        list. This should of course be set before the traversion is
        started. Another traversion would add the rects once more,
        unless this method is called again with NULL.

        It is the callers responsibility to clear the list when done
        with it. */

	void 			SetList(RectList *list) { box_edges_list = list; }

	/** Set actual ancestor element to compute position for.

		BoxEdgesObject traversals are not always started at the root element;
		see the html_element and container_element parameters to the
		constructor. Usually, when we are interested in the distance from
		descendant D to ancestor A, we specify D as html_element and A as
		container_element. In some cases, however, A is not a suitable place
		for traversal to start. Then we find the nearest suitable ancestor of A
		for that and specify that element as container_element, and the actual
		ancestor element is passed to this method instead.

		This is the case if the ancestor is an inline element. Then we need to
		find the nearest block ancestor of that inline element and pass that as
		container_element to the constructor, and pass the inline element to
		this method instead.

		This is also the case if the ancestor is inside a multi-column or paged
		container. Then we need to find the ancestor multi-column or paged
		container and pass that one as container_element to the constructor,
		and pass the actual ancestor that is inside the multi-column or paged
		container to this method instead.

		After traversal GetTargetAncestorX() and GetTargetAncestorY() can be
		used for getting position between container_element the element passed
		to this method.

		Note: Does currently not support there being a transformed
		element between the inline ancestor and the sought element. */

	void			SetTargetAncestor(HTML_Element *ancestor) { OP_ASSERT(!next_container_element || next_container_element->IsAncestorOf(ancestor)); OP_ASSERT(ancestor->IsAncestorOf(element)); target_ancestor = ancestor; }

	/** Get X distance from the original target_ancestor to container_element.

		@see SetTargetAncestor() */

	LayoutCoord		GetTargetAncestorX() const { return target_ancestor_x; }

	/** Get Y distance from the original target_ancestor to container_element.

		@see SetTargetAncestor() */

	LayoutCoord		GetTargetAncestorY() const { return target_ancestor_y; }

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Enter table content. */

	virtual BOOL	EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info);

	/** Enter text box */

	virtual void	EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment);

	/** Leave text box */

	virtual void	LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline);

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Enter table row group. */

	virtual BOOL	EnterTableRowGroup(TableRowGroupBox* rowgroup);

	/** Handle table column (group). */

	virtual void	HandleTableColGroup(TableColGroupBox* col_group_box, LayoutProperties* table_lprops, TableContent* table, LayoutCoord table_top, LayoutCoord table_bottom);

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x, LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Check if this positioned element needs to be traversed. */

	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element);

	/** Enter scrollable content. */

	virtual BOOL	EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);
};

/** Used for calculating the position of the rectangles surrounding the elements in a range.

	Will include padding for elements that are entirely within the range, but
	not for partially selected start or end elements. An object of this class
	can either produce a list of each separate element rectangle, or a bounding
	box from the the union of all the elements. */

class ClientRectObject
  : public VisualTraversalObject
{
private:

	/// Used during text traversal to hold the size of the currently selected text
	RECT			m_box_edges;

	/// Points to the global bounding box if non-null
	RECT*			m_bounding_rect;

	/// Points to the currently traversed text data
	TextData*		m_text_block;

	/// Width in pixels of the space after the previous word
	LayoutCoord		m_prev_space;

	BOOL			m_start_found;

	BOOL			m_range_finished;

	/** When we have added the rectangle for the whole table
		there is no need to make rectangles for the content
		of that same table so we should skip adding content
		when this member is non-NULL. */
	TableContent*	m_skip_contents_of_this_table;

	/** When we have added the rectangle for the whole inline
		there is no need to make rectangles for the content
		of that same inline so we should skip adding content
		when this member is non-NULL. */
	InlineBox*		m_skip_contents_of_this_inline;

	HTML_Element*	m_start_elm;

	HTML_Element*	m_end_elm;

	/// Points to the global list of rectangles if non-null
	OpVector<RECT>*	m_client_rect_list;

	/// Offset, in characters, into the start text element
	int				m_start_offset;

	/// Offset, in characters, into the end text element
	int				m_end_offset;

	void			PushRect(LayoutCoord left, LayoutCoord top, LayoutCoord right, LayoutCoord bottom);

	void			ResetRect()
					{
						m_box_edges.bottom = LONG_MIN;
						m_box_edges.top = LONG_MAX;
						m_box_edges.right = LONG_MIN;
						m_box_edges.left = LONG_MAX;
					}

public:

	/** Constructor.

		@param[in,out] bounding_rect If non-NULL, this points to a
		pre-allocated rectangle that will contain the union of all the client
		rects after traversal. If no visible element was found, left = top =
		LONG_MAX and right = bottom = LONG_MIN.

		@param[in,out] list If non-NULL, list points to a pre-allocated vector
		that will be filled with all the client rectangles of the range after
		traversal. If no visible elements were found, the list will be empty.

		@param start_elm The element where the range starts.

		@param end_elm The element where the range ends.

		@param element_text_start Offset, in characters, into the start text
		element. If 0, the start is from the first character.

		@param element_text_end	Offset, in characters, into the start text
		element. If -1, the end is after the last character. */

		ClientRectObject(FramesDocument* doc, RECT* bounding_rect, OpVector<RECT>* list, HTML_Element* start_elm, HTML_Element* end_elm, int element_text_start = 0, int element_text_end = -1)
					  : VisualTraversalObject(doc),
						m_bounding_rect(bounding_rect),
						m_text_block(NULL),
						m_prev_space(0),
						m_start_found(FALSE),
						m_range_finished(FALSE),
						m_skip_contents_of_this_table(NULL),
						m_skip_contents_of_this_inline(NULL),
						m_start_elm(start_elm),
						m_end_elm(end_elm),
						m_client_rect_list(list),
						m_start_offset(element_text_start),
						m_end_offset(element_text_end)
					{
                        SetTraverseType(TRAVERSE_ONE_PASS);
					}

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** Callback for objects needing to set up code before starting traversal */

	virtual void	StartingTraverse();

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Leave line */

	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Enter table content. */

	virtual BOOL	EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info);

	/** Leave table content. */

	virtual void	LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info);

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Enter table row group. */

	virtual BOOL	EnterTableRowGroup(TableRowGroupBox* rowgroup);

	/** Enter text box */

	virtual void	EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment);

	/** Leave text box */

	virtual void	LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline);

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x, LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);
};

class BorderCollapseCell
{
public:
	TableCollapsingBorderCellBox*
					cell_box;

	int				rowspan;

	int				colspan;

					BorderCollapseCell()
					  : cell_box(NULL),
						rowspan(0),
						colspan(0) {}
};

class PaintObject;

/** Helper class for painting collapsed borders.

	Creates and maintains border-collapse state of the cells in one row in one
	table. Each column is represented by a BorderCollapseCell.

	It is used during the background pass of PaintObject. For each table row we
	leave, we store all the cells that that row contains. In order to paint
	collapsed borders on a row and its cells, we need to look at the next row
	and cells in that row before one can know how things will look, because
	they may affect collapsing. As such, collapsed borders on one row are
	painted when traversing the next row. */

class BorderCollapsedCells
  : public Link
{
private:

	/** Array of columns.

		Stores 3 rows of cells for each column in the table. The first row
		represents cells in the previous table row handled, the middle row
		represents the table row currently being handled, the last row
		represents the next table row to be handled (the last row traversed so
		far). Cells are stored from left to right in visual order, not logical
		order. When a row is done, the array is "shifted" one position
		upwards. */

	BorderCollapseCell(*cells)[3];

	/** X translation at the previous row.

		This is needed since we're painting the borders on the last row in the
		table when we have left both the row and its row group. */

	int				row_translation_x;

	/** Y translation at the previous row.

		This is needed since we're painting borders on the previous row when
		traversal has actually moved to a new row. */

	int				row_translation_y;

	/** TRUE until we're done with the first row in the table. */

	BOOL			first_row;

	/** TRUE if we have skipped past one or more rows.

		As an optimization, when we skip traversal of rows, we also skip
		calculation of border collapsing. Later, when resuming at some row, in
		addtion to resetting this flag, all state in the 'cells' array must be
		recalculated (which includes calculating the state from the two
		previous rows to that row). */

	BOOL			rows_skipped;

	/** The table we belong to. */

	TableCollapsingBorderContent*
					table;

	/** The table-row we're currently looking at. */

	TableCollapsingBorderRowBox*
					cur_row_box;

	/** Calculate the corners between the current ([1]) and next ([2]) row.

		@param i Index into the 'cells' array; i.e. column number, counting
		from the left side of the table. */

	void			CalculateCorners(int i);

	/** Fill out a row of cell information.

		Will shift previous cell information one position upwards and then fill
		out cells[*][2]. */

	void			FillRow(TableCollapsingBorderRowBox* next_row_box, BOOL restore);

public:
					BorderCollapsedCells()
						: cells(0),
						  row_translation_y(INT_MIN),
						  first_row(TRUE),
						  rows_skipped(FALSE),
						  cur_row_box(NULL) { }

					~BorderCollapsedCells() { OP_DELETEA(cells); }

	/** Second-phase constructor. Returns FALSE on memory allocation error. */

	BOOL			Init(TableCollapsingBorderContent* table_content);

	/** Get the table which this object represents. */

	TableCollapsingBorderContent*
					GetTableContent() const { return table; }

	/** Paint cell borders of previous row, and advance to the next one.

		If we have previously skipped past one or more rows (rows_skipped),
		full collapse state (the 'cells' array) must be recalculated now. */

	void			PaintCollapsedCellBorders(PaintObject* paint_object, TableCollapsingBorderRowBox* next_row_box);

	/** Flush borders at the final row.

		Since a row's border is actually painted when entering the next row, we
		need to flush to get the last row's border painted when leaving the
		table, and before skipping rows. */

	void			FlushBorders(PaintObject* paint_object);

	/** Has traversal skipped previous rows? */

	BOOL			AreRowsSkipped() const { return rows_skipped; }

	/** Mark that traversal has skipped a row. */

	void			SetRowsSkipped() { rows_skipped = TRUE; }
};

/** Allows us to work with Selections as we did before the change of representation in 2012.

	This change was when the code also moved from layout to logdoc. By wrapping
	a TextSelection in this it's possible to get the same API as the old
	TextSelection object. */

class LayoutSelection
{
private:
	TextSelection*	m_selection;
	const uni_char*	m_cached_start_word;
	const uni_char*	m_cached_end_word;
public:
	explicit		LayoutSelection(TextSelection* selection) : m_selection(selection), m_cached_start_word(NULL), m_cached_end_word(NULL) {}
	LayoutSelection&operator=(TextSelection* selection) { m_selection = selection; m_cached_start_word = m_cached_end_word = NULL; return *this; }
					operator BOOL() { return m_selection != NULL; }

	/** Get start of selected text */

	const uni_char*	GetStartText();

	/** Get end of selected text */

	const uni_char*	GetEndText();

	/** Get start of word containing start of selected text */

	const uni_char*	GetStartWord();

	/** Get start of word containing end of selected text */

	const uni_char*	GetEndWord();

	/** Get start text box */

	HTML_Element*	GetStartElement() { return m_selection->GetStartElement(); }

	/** Get end text box */

	HTML_Element*	GetEndElement() { return m_selection->GetEndElement(); }

	/** Get the end selection point (not adapted for the traverse code) */

	const SelectionBoundaryPoint&
					GetEndSelectionPoint() { return m_selection->GetEndSelectionPoint(); }

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT

	/** Returns the highlight type, which is used to see the difference between active search hits and normal search hits. */

	TextSelection::HighlightType
					GetHighlightType() { return m_selection->GetHighlightType(); }

#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

	/** Check if the selection is empty (undefined or collapsed). */

	BOOL			IsEmpty() { return m_selection->IsEmpty(); }
};

/** Paint object */

class PaintObject
  : public VisualTraversalObject
{
private:

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	/** Current search hit to be painted */
	SelectionElm*	m_current_hit;

	/** Are we still inside current search hit? Used when search hit spans over several words */
	BOOL			m_inside_current_hit;

	/** Does this Text_Box has more search hits after 'm_current_hit' or is 'm_current_hit' last search hit? */
	BOOL			m_has_more_hits;

	/** Text box containing last painted search hit. We need to reinitialize 'm_has_more_hits' when switching text boxes */
	Text_Box*		m_last_hit_textbox;
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

	/** Width of an ellipsis string. Updated when entering/leaving container with
	tex-overflow set either to ellipsis or -o-ellipsis-lastline */
	short			m_ellipsis_width;

	/** TRUE if we're traversing last entirely visible line and container is supposed
		to draw last line ellipsis */
	BOOL			m_draw_lastline_ellipsis;

	/**TRUE if line's content overflowed container area horizontaly */
	BOOL			m_line_overflowed;

	/** Ellipsis data */
	OpPoint			m_ellipsis_point;

	/** Bounding box for current Line, relative to that Line. */
	AbsoluteBoundingBox
					line_area;

	short			start_word_number;

	LayoutCoord		decoration_start;

	LayoutCoord		decoration_width;

	LayoutSelection	text_selection;

	/** List of BorderCollapsedCells; one instance for each table (with collapsing borders) we're currently inside. */

	Head			border_collapsed_cells_list;

	/** Width of trailing whitespace of the previous sibling text box */

	int				prev_trailing_whitespace_width;

	BOOL			paint_cell_background_only;

#ifdef _PRINT_SUPPORT_
	BOOL			check_visible_color_on_white_bg;
#endif // _PRINT_SUPPORT_

	/* Minimum color contrast between font color and current background color (not background image). */

	int				minimum_color_contrast;

	/* Light and dark font color to use if not sufficient contrast. */

	long			light_font_color;
	int				dark_font_color;

	/** An element that is highlighted in the displayed document. */

	HTML_Element*	highlighted_elm;

	/** Used for finding out if highlighted element contains text. */

	BOOL			highlight_has_text;

#define				MAX_HIGHLIGHT_RECTS 4

	/** Highlightrects. Rectangles for the highlighted element's text. */

	OpRect			highlight_rects[MAX_HIGHLIGHT_RECTS];

	/** Number of highlightrects found. If equal to MAX_HIGHLIGHT_RECTS the rects
		may be incomplete and should not be used. */

	int				num_highlight_rects;

	/** If we are traversing the highlight or its children at the moment. */

	BOOL			is_in_highlight;

	/** Highlight clipping rectangle - limits highlight rectangle not letting
		highlight to overflow containers with overflow != visible. */

	OpRect			highlight_clip_rect;

	/** Clipping rectangle for currently traversed element */

	OpRect			current_clip_rect;

	/** Clip rect that comes from current table with overflow-x:hidden.
		INT32_MAX width means that either we are not in such table currently
		or the clipping has been already applied to VisualDevice.
		We need to postpone clipping application only in case of table
		with border-collapse:collapse.

		Expressed in current transform's root coordinates. */

	OpRect			table_content_clip_rect;

	/** We are currently adding rects to an outline. */

	int				is_in_outline;

	/** The count of outlines started by inline box elements.

	    Keeps track of how many outlines were begun for an inline box elements
	    that are currently being traversed. In the case where some of them are
	    not traversed fully due to being wrapped and being partially off-screen,
	    the LeaveVerticalBox() will take care of ending them. */

	int				is_in_inlinebox_outline;

	BOOL			m_incomplete_outline;

	OpRect			outline_bounding_rect;

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/** Structure for keeping x + width for words that should be painted as "misspelled" */

	struct MisspellingPaintInfo
	{
		UINT16		word_left;
		UINT16		word_width;
		MisspellingPaintInfo*
					next;
	};

	MisspellingPaintInfo*
					misspelling_paint_info;
#endif // INTERNAL_SPELLCHECK_SUPPORT

	/* Check that color contrast between given font color and background color is better than minimum value.
	   If not, return a new font_color. */

	COLORREF		CheckColorContrast(COLORREF font_color, COLORREF bg_color, int minimum_contrast) { return ::CheckColorContrast(font_color, bg_color, minimum_contrast, light_font_color, dark_font_color); }

	/** Paint all text decorations. */

	void			PaintTextDecoration(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord text_left, LayoutCoord text_width, COLORREF bg_color);

	/** Paint ellipsis */

	void PaintEllipsis(const HTMLayoutProperties& container_props, short baseline);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	void			AddMisspelling(const WordInfo& word_info, int text_left, int text_width, int space_width, LayoutProperties* layout_props, LineSegment& segment);
	void			PaintPendingMisspellings(LayoutProperties* layout_props, LayoutCoord base_line);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	void			PaintText(const HTMLayoutProperties& props, const uni_char* word, int word_length, short word_width, int x, LayoutCoord baseline, LineSegment& segment, int text_format, const WordInfo& word_info, COLORREF text_color, short orig_word_width);

	void			PaintTextShadow(const HTMLayoutProperties& props, int x, int y, const uni_char* word, int word_length, int text_format, COLORREF text_color, short orig_word_width);

	/** Handle text with highlight - paints text with highlight (and handles highlight iself)
		@param	layout_props	layout properties
		@param	highlight		current selection element
		@param	word			word to be pained
		@param	word_length		length (in characters) of painted word
		@param	word_width		word width (in pixels) - may be 0 for white spaces
		@param	space_width		space width
		@param	x				word position (relative to segment)
		@param	baseline		baseline position
		@param	segment			segment this word belongs to
		@param	text_format		text_format
		@param	word_info		word info structure
		@param	text_color		text color
		@param	orig_word_width	original word width (required by linear text scale feature) - word_width may differ from orig_word_width
								in case of ellipsis replacing (and therefore shortening the word itself) some part of a word.
		@param	local_blink_on
		@param	draw_text		HandleTextWithHighlight handles a highlight (by updating highlight element status) and paints a text.
								There are however cases where we need to handle highlight without painting the word itself
								(this is a case when ellipsis replaces whole word so it's painting must be skipped) otherwise we will break
								highlight painting. When 'draw_text' is set to FALSE, text will not be painted but selection will be updated as if this word was painted normally.
	*/

	void			HandleTextWithHighlight(LayoutProperties *layout_props, LayoutSelection highlight, const uni_char* word, int word_length, short word_width, short space_width, int x, LayoutCoord baseline, LineSegment& segment, int text_format, const WordInfo& word_info, COLORREF text_color, short orig_word_width, BOOL local_blink_on, BOOL draw_text);

	/** Limit opacity rect to something that the lower level graphics
		layer can handle.

		@param rect The rect to create opacity layer for (inout) */

	void			LimitOpacityRect(OpRect& rect) const;

	/** Create a new opacity layer for an inline element with opacity < 1.

		@param layout_props The inline element's layout properties.
		@param line The Line on which the inline element lies (for this part of the traversal).
		@param box The inline element's layout box.
		@param box_area Same as the box_area sent to EnterInlineBox for the inline element.
		@param traverse_info Current TraverseInfo for this traversal. */

	void			BeginOpacityLayer(LayoutProperties* layout_props, const Line* line, InlineBox *box, const RECT &box_area, TraverseInfo& traverse_info);

#ifdef LAYOUT_BEGIN_END_TEXT_SHADOW
	virtual void	BeginTextShadow() {}

	virtual void	EndTextShadow() {}
#endif //LAYOUT_BEGIN_END_TEXT_SHADOW

public:
					PaintObject(FramesDocument* doc,
								VisualDevice* visual_device,
								const RECT& paint_area,
								TextSelection* selection,
								HTML_Element* highlighted_elm,
#ifdef _PRINT_SUPPORT_
								BOOL check_color_on_white_bg,
#endif // _PRINT_SUPPORT_
								int min_color_contrast,
								long light_color,
								long dark_color);
#ifdef _DEBUG
					~PaintObject()
					{
						OP_ASSERT(border_collapsed_cells_list.Empty());
# ifdef INTERNAL_SPELLCHECK_SUPPORT
						OP_ASSERT(!misspelling_paint_info);
# endif // INTERNAL_SPELLCHECK_SUPPORT
					}
#endif

	/** Get number of highlight rects */

	int				GetNumHighlightRects() { return num_highlight_rects == MAX_HIGHLIGHT_RECTS ? 0 : num_highlight_rects; }

	/** Get highlightrects */

	OpRect			*GetHighlightRects() { return highlight_rects; }

	/** Get highlight clipping rect */

	OpRect			GetHiglightClipRect() { return highlight_clip_rect; }

	/** Flush backgrounds that may not have been painted yet, because of avoid-overdraw optimization. */

	virtual void	FlushBackgrounds(LayoutProperties* layout_props, Box* box);

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Leave line */

	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Enter text box */

	virtual void	EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment);

	/** Leave text box */

	virtual void	LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline);

	/** Enter scrollable content. */

	virtual BOOL	EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave scrollable content. */

	virtual void	LeaveScrollable(TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave paged container. */

	virtual void	LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Enter multi-column container. */

	virtual void	EnterMultiColumnContainer(LayoutProperties* layout_props, MultiColumnContainer* content);

	/** Enter column in multicol container or page in paged container. */

	virtual BOOL	EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info);

	/** Leave column in multicol container or page in paged container. */

	virtual void	LeavePane(TraverseInfo& traverse_info);

	/** Handle border collapsing for table row. */

	virtual void	HandleCollapsedBorders(TableCollapsingBorderRowBox* row, TableContent* table);

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Leave table row. */

	virtual void	LeaveTableRow(TableRowBox* row, TableContent* table);

	/** Handle table column (group). */

	virtual void	HandleTableColGroup(TableColGroupBox* col_group_box, LayoutProperties* table_lprops, TableContent* table, LayoutCoord table_top, LayoutCoord table_bottom);

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Handle selectable box */

	virtual void	HandleSelectableBox(LayoutProperties* layout_props);

	/** Handle break box */

	virtual void	HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break);

	/** Handle replaced content */

	virtual void	HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content);

	/** Handle bullet content (paint it). */

	virtual void	HandleBulletContent(LayoutProperties* layout_props, BulletContent* content);

	/** Enter table content. */

	virtual BOOL	EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info);

	/** Leave table content. */

	virtual void	LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info);

#ifdef DOCUMENT_EDIT_SUPPORT
	/** "Enter" layout break. */

	virtual BOOL	EnterLayoutBreak(HTML_Element* break_element) { return !target_element && document->GetDocumentEdit(); }
#endif //  DOCUMENT_EDIT_SUPPORT

	/** Handle marquee. */

	virtual OP_STATUS
					HandleMarquee(MarqueeContainer* marquee, LayoutProperties* layout_props);

	/** Handle empty table cells. */

	virtual void	HandleEmptyCells(const TableRowBox* row, TableContent* table, LayoutProperties* table_lprops);

	/** Begin clipping of a partially collapsed table cell. */

	virtual void	BeginCollapsedTableCellClipping(TableCellBox* box, const OpRect& clip_rect, TraverseInfo& traverse_info);

	/** End clipping of a partially collapsed table cell. */

	virtual void	EndCollapsedTableCellClipping(TableCellBox* box, TraverseInfo& traverse_info);

	/** Applies clipping */

	virtual void	PushClipRect(const OpRect& clip_rect, TraverseInfo& info);

	/** Removes clipping */

	virtual void	PopClipRect(TraverseInfo& info);

	/** Does this traversal object subbport clipping? */

	virtual BOOL	HasClippingSupport() const { return TRUE; }

};

/** HitTestingTraversalObject is a subclass of AreaTraversalObject
	that doesn't operate directly on any VisualDevice but still needs
	to do hit testing, like intersection testing or text selection.
	The class handles also clipping logic. Because of the fact that clip rects
	might be pushed at the end of EnterScrollable or EnterVerticalBox the class
	introduces two additional methods - HandleScrollable and HandleVerticalBox
	that can be used by the subclasses instead of Enter methods.
	In case of EnterVerticalBox, the clipping rectangle is pushed when all the following are TRUE:
	- the element introduces clipping that affects traversed descendants
	- clipping is not disabled
	- we are not on a dry run
	In case of EnterScrollable, the clipping rectangle is pushed, when the container
	introduces clipping that affects traversed descendants. */

class HitTestingTraversalObject
	: public AreaTraversalObject
{
protected:

	/** The values of this enum describe, how precise the clipping is at the moment.
		Precision means how close to the exact clip polygon (which may not be a rectangle when
		using CSS transforms), we may expect the clip rect returned by GetClipRect() and GetRoughClipRect()
		methods to be. It also reflects the probability of a false positive when checking
		intersection.
		PRECISE - both GetClipRect() and GetRoughClipRect() return the exact clip polygon
		IMPRECISE - for future use, GetClipRect() may return too big clip rect (because of BBoxes calculation)
					GetRoughClipRect() may return too small clip rect
		MERGED_WITH_AREA - at least once the area has been adjusted by intersecting it with the clip rect
					GetClipRect() may return too big clip rect (because some may not be taken into account at all)
					GetRoughClipRect() returns empty rect always (to avoid including pixels that are outside the overall clip area). */
	enum ClippingPrecision
	{
		PRECISE,
		IMPRECISE,
		MERGED_WITH_AREA
	};

private:

	BOOL			clipping_disabled;

	/** The current clipping rectangle. The field is assumed to be one of the following:
	 *  - an empty rectangle (when width == 0 || height == 0)
	 *  - BBox of the whole document rectangle in current coordinate system
	 *  - a valid, finite rectangle, which is always fully inside the above and which should be possible to transform
	 *    with a reasonable transformation matrix, so that the INT32 range is not exceeded (called further the proper clipping rectangle)
	 *  The rectangle is relative to the current translation, expressed in current's transform's root coordinate system.
	 *  Once a transform is pushed, the BBox of the rectangle in unscaled document coordinates is intersected with the current area. */

	OpRect			m_clip_rect;

	/** TRUE when the m_clip_rect is empty (which means nothing is visible after entering the box that has last introduced a new clip rect)
		or when the m_clip rect does not cover the entire document in the current transform ctx coordinates. */

	BOOL			m_has_clip_rect;

	/** The current clipping precision, see ClippingPrecision description. */

	ClippingPrecision m_clipping_precision;

	/** Intersects the rect in local coords with the current clip rect, if the area is not a single point.
	 *	Then checks the intersection between the resulting rect and the area.
	 *	Overrides AreaTraversalObject::IntersectsCustom. */

	virtual BOOL	IntersectsCustom(RECT& rect) const;

	/** Intersects the current clipping rect with the new one. */

	virtual void	PushClipRect(const OpRect& clip_rect, TraverseInfo& info);

	/** Restores the clipping rect to be equal to the one before pushing the last one. */

	virtual void	PopClipRect(TraverseInfo& info);

protected:

	LayoutCoord		translation_x;
	LayoutCoord		translation_y;

	AffinePos		transform_root;

	/** Set translation */

	virtual void	SetTranslation(LayoutCoord x, LayoutCoord y) { translation_x = x; translation_y = y; }

	VisualDeviceTransform
					visual_device_transform;

	/** Get the visual device transform, used for hit testing */

	virtual const VisualDeviceTransform*
					GetVisualDeviceTransform() const { return &visual_device_transform; }

	/** Get x translation */

	virtual LayoutCoord
					GetTranslationX() const { return translation_x; }

	/** Get y translation */

	virtual LayoutCoord
					GetTranslationY() const { return translation_y; }

	/** Translate x and y coordinates */

	virtual void	Translate(LayoutCoord x, LayoutCoord y);

#ifdef CSS_TRANSFORMS

	/** Push transform. Does the following:
	 *	- Stores the current translation state, clip rect and area
	 *	- Transforms the current clip rect (if finite) into BBox in doc coordinates and intersects the area with it
	 *	- Pushes the new transform to the visual_device_transform and stores the current transform root
	 *	- Resets the translation state and the clip rect to be the whole document's rect in current transform's coordinates.
	 * @return OpBoolean::IS_TRUE if ok.
	 *		   OpBoolean::IS_FALSE in case of a transform that empties the box that it transforms (e.g. scale(0)).
	 *		   OpBoolean::ERR_NO_MEMORY in case of OOM. */

	virtual OP_BOOLEAN PushTransform(const AffineTransform &t, TranslationState &state);

	/** Pop transform. Pops the top transform from the visual_device_transform.
	 *  Restores the clip rect, area, translation, transform root to the state before last transform push. */

	virtual void	PopTransform(const TranslationState &state);

#endif

	/** Get affine transform to closest transform root */

	virtual AffinePos
					GetTransformRoot() const { return transform_root; }

	/** Set affine transform to closest transform root */

	virtual void	SetTransformRoot(const AffinePos &ctm) { transform_root = ctm; }

	/** Returns the current clipping precision. */

	ClippingPrecision GetClippingPrecision() {return m_clipping_precision;}

	/** Returns the current clip rect (if it is proper or empty), NULL otherwise.
		The rect pointed by the returned pointer may be bigger than the exact clip polygon.
		The method may also return NULL (no clip rect, infinite clip rect), in some cases
		when the clip polygon is finite). See GetClippingPrecision(). */

	const OpRect*	GetClipRect() const {return m_has_clip_rect ? &m_clip_rect : NULL;}

	/** Returns the current clip rect.
		The returned rect may be smaller than the exact clip polygon.
		Additionally, it never contains any point that is outside the exact clip polygon.
		The method may also return empty rect, in some cases when the clip polygon is
		non empty. See GetClippingPrecision(). */

	OpRect			GetRoughClipRect() const {return m_clipping_precision == MERGED_WITH_AREA ? OpRect() : m_clip_rect;}

	/** Checks whether the given rectangle, intersected with the current total clip rect, intersects the area.
	 *  May return false positive. See GetClippingPrecision().
	 * @param rect a non empty rectangle to check
	 * @param area optional pointer to the area to check against (if NULL, then AreaTraversalObject::area will be used to check against)
	 * @param[out] clipped_rect optional pointer. If not NULL and the clipped rect intersects the area, then it will be placed under the pointer.
     * @return TRUE if the clipped rect intersects the area, FALSE otherwise. */

	BOOL			CheckIntersectionWithClipping(const OpRect& rect, const RECT* area = NULL, OpRect* clipped_rect = NULL) const;
	BOOL			CheckIntersectionWithClipping(const RECT& rect, const RECT* area = NULL, RECT* clipped_rect = NULL) const;

	/** Checks whether the given rectangle, intersected with the current total clip rect, intersects the AreaTraversalObject::area or the inner area.
	 *  May return false positive. See GetClippingPrecision().
	 * @param rect a non empty rectangle to check
	 * @param inner an inner area to check against
	 * @param intersects_inner[out] the param is assigned TRUE, when the intersection check against the inner succeeds
	 * @param[out] clipped_rect optional pointer. If not NULL and the clipped rect intersects the area, then it will be placed under the pointer.
     * @return TRUE if the clipped rect intersects the area or the inner, FALSE otherwise. */

	BOOL			DoubleCheckIntersectionWithClipping(const OpRect& rect, const RECT& inner, BOOL& intersects_inner, OpRect* clipped_rect = NULL) const;
	BOOL			DoubleCheckIntersectionWithClipping(const RECT& rect, const RECT& inner, BOOL& intersects_inner, RECT* clipped_rect = NULL) const;

	/** Checks whether the last clip rect (from the current coordinate system) is fully outside the common polygon
		of the previous clip rects and the area. May result in a false negative. See GetClippingPrecision(). */

	BOOL			IsLastClipRectOutsideHitArea() const;

	/** Check whether the given point is inside the current clip polygon. May result in a false positive.
		See GetClippingPrecision(). */

	BOOL			IsPointInClipRect(const OpPoint& point) {return !m_has_clip_rect || m_clip_rect.Contains(point);}

	/** Called from inside EnterVerticalBox box, when we should enter it according to HitTestingTraversalObject
	 *  and it is not dry run currently.
	 *  When called, the clip rect (if any) that origins from the current VerticalBox has not been pushed yet.
	 * @param clip_affects_traversed_descendants TRUE when the box introduces clipping (because of overflow hidden or clip property)
	 *		  and affects the descendant(s) that will be traversed to in that run. In other words this param is TRUE iff
	 *		  (there is some clipping) && ((no target element set) || ((target element is set) && (clipping affects the target)))
	 * @return TRUE when we should enter the vertical box according to the subclass, FALSE otherwise. */

	virtual BOOL	HandleVerticalBox(LayoutProperties* parent_lprops,
									  LayoutProperties*& layout_props,
									  VerticalBox* box,
									  TraverseInfo& traverse_info,
									  BOOL clip_affects_traversed_descendants) {return TRUE;}

	/** Called from inside EnterScrollable.
	 *  When called, the clip rect that origins from the current scrollable has not been pushed yet.
	 *  @return TRUE when we should enter the scrollable, FALSE otherwise. */

	virtual BOOL	HandleScrollable(const HTMLayoutProperties& props,
									 ScrollableArea* scrollable,
									 LayoutCoord width,
									 LayoutCoord height,
									 TraverseInfo& traverse_info,
									 BOOL clip_affects_target,
									 int scrollbar_modifier) {return TRUE;}

public:

	/** Constructors. The disable_clipping parameter is added, so it is possible to make an exception for the TextSelectionObject,
	 *  which currently must not use clipping logic of this class. */

	HitTestingTraversalObject(FramesDocument* doc, const RECT& area, BOOL disable_clipping = FALSE)
		: AreaTraversalObject(doc, area)
		, clipping_disabled(disable_clipping)
		, m_has_clip_rect(FALSE)
		, m_clipping_precision(PRECISE)
		, translation_x(0)
		, translation_y(0) {}

	HitTestingTraversalObject(FramesDocument* doc, BOOL disable_clipping = FALSE)
		: AreaTraversalObject(doc)
		, clipping_disabled(disable_clipping)
		, m_has_clip_rect(FALSE)
		, m_clipping_precision(PRECISE)
		, translation_x(0)
		, translation_y(0) {}

	/** Callback for objects needing to set up code before starting traversal */

	virtual void	StartingTraverse();

	/** Enter vertical box. WARNING: See HandleVerticalBox, before you override. */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter scrollable content. WARNING: See HandleScrollable, before you override. */

	virtual BOOL	EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave scrollable content. */

	virtual void    LeaveScrollable(TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave paged container. */

	virtual void	LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Begin clipping of a partially collapsed table cell. */

	virtual void	BeginCollapsedTableCellClipping(TableCellBox* box, const OpRect& clip_rect, TraverseInfo& traverse_info);

	/** End clipping of a partially collapsed table cell. */

	virtual void	EndCollapsedTableCellClipping(TableCellBox* box, TraverseInfo& traverse_info);

	/** Enter table content. */

	virtual BOOL	EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info);

	/** Leave table content. */

	virtual void	LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info);
};

/** Intersection object */

class IntersectionObject
  : public HitTestingTraversalObject
{
private:
#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** @return TRUE if traversal of dirty trees is allowed. */

	virtual BOOL	AllowDirtyTraverse();

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

protected:

	/** The inner box that intersects with (xpos, ypos) and is not transparent */

	Box*			inner_box;

	/** The horizontal distance from top left of inner_box to xpos */

	LayoutCoord		relative_x;

	/** The vertical distance from top left of inner_box to ypos */

	LayoutCoord		relative_y;

	/** The original x coordinate, relative to left edge of document. */

	LayoutCoord		orig_x;

	/** The original y coordinate, relative to top edge of document. */

	LayoutCoord		orig_y;

	/** 'orig_x' with current translation included. */

	LayoutCoord		translated_x;

	/** 'orig_y' with current translation included. */

	LayoutCoord		translated_y;

	/** Hashtable for storing offset to different boxes hit. */

	OpAutoPointerHashTable<Box, OpPoint> box_relative_hash;

	/** Include text boxes */

	BOOL			include_text_boxes;

#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
	/** Text alignment in the container of the intersected element. */

	OpViewportRequestListener::Align
					text_align;

	/** Grab information about the container when box has been hit. */

	BOOL			grab_container_info;

	/** Bounding rectangle of the container of the intersected element. */

	OpRect			container_rect;

	/** Bounding rectangle of the line of the intersected element. */

	OpRect			line_rect;
#endif // LAYOUT_INTERSECT_CONTAINER_INFO

	/** The word clicked */

	const uni_char* word;

	/** length of the word clicked */

	int             word_length;

#ifdef GADGET_SUPPORT
    BOOL			control_region_found;
#endif // GADGET_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION

	/** Elements of interest found within the specified radius.

		Contains elements of type ElementOfInterest, sorted by distance from origin point. */

	Head*			eoi_list;

	TextAnchorFragment*
					cur_text_anchor_fragment;

	AnchorElementOfInterest*
					cur_anchor_eoi;

	FormElementOfInterest*
					cur_box_form_eoi;

	/** Radius (in pixels) within which to search for elements of interest (anchor and form elements). */

	unsigned int	eoi_radius;

	BOOL			inside_cur_anchor_eoi;

	int 			eoi_first_text_anchor_fragment_x;

	/** Check if an anchor should be added to the list of candidates, and, if it should, add it. */

	BOOL			CheckAnchorCandidate(LayoutProperties* layout_props, const RECT& box_area);

	/** Expand the region of the specified element of interest. */

	void			ExpandEoiRegion(ElementOfInterest* eoi, OpRect& eoi_rect, const OpRect& box_rect);

#endif // NEARBY_ELEMENT_DETECTION

	/** opaque content like replaced elements and text was found.

		This content will receive events instead of transparent boxes
		on top of the element with mouseover handlers etc...
		(IE compatibility fix) */

	BOOL content_found;

	BOOL			CheckIntersection(LayoutCoord x, LayoutCoord y,
									  LayoutProperties*& layout_props,
									  VerticalBox* box
#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
									  , BOOL real_intersection = TRUE
#endif // LAYOUT_INTERSECT_CONTAINER_INFO
									 );

	/** Stores the data as the "current (currently innermost) hit" and puts offset
		information about an intersected box in our storage, to be used
		by the caller later. It can fail in OOM situations and then the effect will
		be that nothing is added in the global storage. Also, only one
		coordinate per box is allowed.

		@param box The box, not NULL.
		@param box_relative_x The horizontal distance from the box's left content edge
		                      to the intersection point.
		@param box_relative_y The vertical distance from the box's top content edge
		                      to the intersection point.

		@see GetRelativeToBox() */

	void			RecordHit(Box* box, int box_relative_x, long box_relative_y);

#ifndef NEARBY_ELEMENT_DETECTION
	/** Checks whether the area rectangle is a single point. */

	virtual BOOL	IsAreaSinglePoint() const {OP_ASSERT(area.left + 1 == area.right && area.top + 1 == area.bottom); return TRUE;}
#endif // !NEARBY_ELEMENT_DETECTION

	/** Gets the area point in local coordinates. Valid only when the above returns TRUE. */

	virtual OpPoint	GetLocalAreaPoint() const {OP_ASSERT(IsAreaSinglePoint()); return OpPoint(translated_x, translated_y);}

	/** Checks whether, the central point of the area is inside the current clip rect.
		In particular, if the area is a single point (e.g. standard intersection check, without NEARBY_ELEMENT_DETECTION),
		it is always TRUE, because HitTestingTraversalObject ensures it. */

	BOOL			IsCentralPointInClipRect() {return IsAreaSinglePoint() || IsPointInClipRect(OpPoint(translated_x, translated_y));}

	/** Called from inside HitTestingTraversalObject::EnterVerticalBox box, when we should enter it according to HitTestingTraversalObject
	 *  and it is not dry run currently.
	 *  When called, the clip rect (if any) that origins from the current VerticalBox has not been pushed yet.
	 * @param clip_affects_traversed_descendants TRUE when the box introduces clipping (because of overflow hidden or clip property)
	 *		  and affects the descendant(s) that will be traversed to in that run. In other words this param is TRUE iff
	 *		  (there is some clipping) && ((no target element set) || ((target element is set) && (clipping affects the target)))
	 * @return TRUE when we should enter the vertical box according to the subclass, FALSE otherwise. */

	virtual BOOL	HandleVerticalBox(LayoutProperties* parent_lprops,
									  LayoutProperties*& layout_props,
									  VerticalBox* box,
									  TraverseInfo& traverse_info,
									  BOOL clip_affects_traversed_descendants);

	/** Called from inside HitTestingTraversalObject::EnterScrollable.
	 *  When called, the clip rect that origins from the current scrollable has not been pushed yet.
	 *  @return TRUE when we should enter the scrollable, FALSE otherwise. */

	virtual BOOL	HandleScrollable(const HTMLayoutProperties& props,
									 ScrollableArea* scrollable,
									 LayoutCoord width,
									 LayoutCoord height,
									 TraverseInfo& traverse_info,
									 BOOL clip_affects_target,
									 int scrollbar_modifier);

public:

					IntersectionObject(FramesDocument* doc, LayoutCoord x, LayoutCoord y, BOOL text_boxes = FALSE)
					  : HitTestingTraversalObject(doc),
						inner_box(NULL),
						relative_x(0),
						relative_y(0),
						include_text_boxes(text_boxes),
#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
						text_align(OpViewportRequestListener::VIEWPORT_ALIGN_NONE),
						grab_container_info(FALSE),
#endif // LAYOUT_INTERSECT_CONTAINER_INFO
						word(NULL),
						word_length(0),
#ifdef GADGET_SUPPORT
						control_region_found(FALSE),
#endif // GADGET_SUPPORT
#ifdef NEARBY_ELEMENT_DETECTION
						eoi_list(0),
						cur_text_anchor_fragment(0),
						cur_anchor_eoi(0),
						cur_box_form_eoi(0),
						eoi_radius(0),
						inside_cur_anchor_eoi(FALSE),
						eoi_first_text_anchor_fragment_x(0),
#endif // NEARBY_ELEMENT_DETECTION
						content_found(FALSE)
						{
							area.left = translated_x = orig_x = x;
							area.top = translated_y = orig_y = y;
							area.right = area.left + 1;
							area.bottom = area.top + 1;
						}

	/** Get inner box */

	virtual Box*	GetInnerBox() const { return inner_box; }

	/** Get the word clicked */

	const uni_char* GetWord() const { return word; }

	/** Get the length of the word clicked */

	int             GetWordLength() const { return word_length; }

	/** Get the relative x position where inner_box is intersected */

	LayoutCoord		GetRelativeX() const { return relative_x; }

	/** Get the relative y position where inner_box is intersected */

	LayoutCoord		GetRelativeY() const { return relative_y; }

	/** Get the relative position to a box this intersection hit or FALSE if the Box wasn't hit. Will not change out variables if returning FALSE. */

	BOOL			GetRelativeToBox(Box* box, LayoutCoord& out_relative_x, LayoutCoord& out_relative_y);

	/** Translate x and y coordinates */

	virtual void	Translate(LayoutCoord x, LayoutCoord y);

	/** Callback for objects needing to run code after traverse has finished */

	virtual void	TraverseFinished();

#ifdef GADGET_SUPPORT
	/** Enter vertical box. Overrides HitTestingTraversalObject::EnterVerticalBox, but see also HandleVerticalBox. */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);
#endif //GADGET_SUPPORT

#if defined(LAYOUT_INTERSECT_CONTAINER_INFO) || defined NEARBY_ELEMENT_DETECTION

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave line */

	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

#endif // LAYOUT_INTERSECT_CONTAINER_INFO || NEARBY_ELEMENT_DETECTION

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

#ifdef NEARBY_ELEMENT_DETECTION

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);

#endif // NEARBY_ELEMENT_DETECTION

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

#ifdef NEARBY_ELEMENT_DETECTION

	/** Handle replaced content */

	virtual void	HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content);

#endif // NEARBY_ELEMENT_DETECTION

#ifdef LAYOUT_INTERSECT_CONTAINER_INFO

	/** Get text alignment of container of intersected box. */

	OpViewportRequestListener::Align
					GetTextAlign() const { return text_align; }

	/** Get size of container of intersected box. */

	const OpRect&	GetContainerRect() const { return container_rect; }

	/** Get size of line of intersected box. */

	const OpRect&	GetLineRect() const { return line_rect; }

#endif // LAYOUT_INTERSECT_CONTAINER_INFO

#ifdef GADGET_SUPPORT

    /** Did we find a control region? */

	BOOL			HasControlRegionBeenFound() const { return control_region_found; }

	/** See if this element has a control region, and if mouse is inside this controle region.
		If we are inside, this->control_region_found is set to true. if control_region_found is
		already set, nothing more is done.

		Method handles:
			Controle elements like buttons, text-areas, inputs
			CSS controle_regions
			Scrollbars in overflowed elements*/

	void			CheckControlRegion(LayoutProperties* layout_props, Box* box, const RECT* box_area);

#endif // GADGET_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION

	/** Enable element of interest (nearby anchor and form elements) detection.

		Set the radius within which to search for elements of interest, and a list in which to put them.

		@param eoi_list Where to put elements of interest found.
		@param radius Radius in pixels. */

	void			EnableEoiDetection(Head* eoi_list, unsigned int radius);

#endif // NEARBY_ELEMENT_DETECTION

#ifdef CSS_TRANSFORMS

	/* Push transform */

	virtual OP_BOOLEAN PushTransform(const AffineTransform &t, TranslationState &state);

	/* Pop transform */

	virtual void	PopTransform(const TranslationState &state);

#endif
};


/** TraversalObject used to search for visible html elements and scrollbars in some area of a document.
	Visible here means that the element is not fully clipped and will be painted.
	ElementSearchObject exposes several new interface methods used to notify about found elements and their locations and opaque areas.
	All usemap areas are reported with bounding rectangles.
	For all html elements that are not usemap areas the element is considered to occupy a region that is an union of all regions occupied
	by it's descendants excluding clipped areas. Those regions consist of rectangles and transformed rectangles, which may be a parallelogram.
	The exception is that the part of a descendant element that overflows and is not positioned,
	is not considered to be a part od the ancestor region.
	ElementSearchObject's behavior can be customized by the implementations of the ElementSearchCustomizer class.
	It optionally allows to check the boxes that are outside the base search area, but intersect the defined
	extended area. However, the rectangle that comes from a vertical box and that doesn't intersect the main area,
	is not reported for the element that this block corresponds to. It may be reported for it's ancestors in some cases.
	That results in reporting additional rectangles (possibly transformed) of the interesting elements.
	Those rectangles are marked, when reported that they intersect only the extended area. This can be used to extend the
	regions of the elements, such that at least one rectangle intersects the main area. */

class ElementSearchObject
  : public HitTestingTraversalObject
{
private:

	ElementSearchCustomizer& customizer;

	/** This flag is used to mark when we are inside a vertical box that has overflow, but it's bounding box
	 *	is only intersecting the extended area, not the main area. We do not need to report elements then. */

	BOOL			suppress_elements;

	/** Set when a subclass signals that something went wrong (like OOM) and continuing the traverse doesn't make sense.
	 *	When set to TRUE, the ElementSearchObject will keep returning FALSE from every EnterSomething method. */

	BOOL			terminate_traverse;

	/** Used to indicate the last result of intersection check in AreaTraversalObject.
		TRUE when the clipped box was intersecting the main area, FALSE when it was
		only intersecting the extended area. Needs to be mutable, because modified
		inside const method and really is just a return value passed several steps
		higher on the callstack. */

	mutable BOOL	intersects_main_area;

	/** Used to mark that opacity has been started when handling a vertical box, so that in case of not entering it
		we can call EndOpacity(). */

	BOOL			opacity_in_vertical_box_started;

	/** During the traverse there might be situations, where we need to add a rectangle from a found element to it's ancestors regions.
	 *	This happens in two cases. First is a positioned element.
	 *	Second is that inline elements occupy the region of all it's descendants boxes.
	 *	The flag is initally set to TRUE. Containers are unlocked when we start a target traversal from a block box.
	 *	Then they are locked again after entering a container after reaching the target.
	 *	When this flag is FALSE, we use the open_elements vector to iterate through all ancestors that the given rectangle
	 *	needs to be added to their regions. */

	BOOL			ancestor_containers_locked;

	/**	Stores all the elements that allow to check all the ancestors of a positioned element (or inline descendant of a positioned inline element),
	 *	whether the rectangle of the given element's box should be added to their regions. Namely, each time we encounter a container during
	 *	the target traversal to positioned element, we iterate until the first ancestor with a container (or table content) exclusive and add
	 *	a first element that passes the filter. */

	OpVector<HTML_Element> open_elements;

	/** The flag indicating, whether we should use extended area in the search. */

	BOOL			use_extended_area;

	/** The flag indicates whether the main_area is an empty rect at the moment. When using extended area, this might happen
		After entering some transform context and intersecting both extended and main areas with the current clip rect. */

	BOOL			main_area_empty;

	/** The main_area rectangle. In use when we use extended area, where in such case AreaTraversalObject::area is an extended area. */

	RECT			main_area;

	/** Checks whether the rect (with current clipping applied) intersects the area and optionally the extended area.
	* @param rect the rect to be checked
	* @param check_extended TRUE we should check intersection both with main and extended area
	*						FALSE we should only check against the main area
	* @param[out] intersects_extended_only when check extended is TRUE this is used to output whether the rect intersects the main area or
	*									   only the extended area. Not used when check_extended is FALSE (can be NULL).
	*									   Must not be NULL when check_extended is TRUE. The check_extended is read only once before any
	*									   operation with intersects_extended_only pointer, so it is safe to pass the same variable twice
	*									   (value and pointer).
	* @param[out] clipped_rect an optional pointer. When the method returns TRUE, the rect with clipping applied is placed under the pointer.
	* @return TRUE iff (check_extended && (rect intersects extended area)) || (!check_extended && (rect intersects main area)). */

	BOOL			IsRectIntersectingAreas(const RECT& rect, BOOL check_extended, BOOL* intersects_extended_only = NULL, RECT* clipped_rect = NULL) const;
	BOOL			IsRectIntersectingAreas(const OpRect& rect, BOOL check_extended, BOOL* intersects_extended_only = NULL, OpRect* clipped_rect = NULL) const;

	/** Iterates through all elements starting from given html_element up the tree and calls HandleElementFound
	 *	for each element that passes the filter until encountering an element that has a container (block or inline-block) or table content.
	 * @param elm the starting element
	 * @param elm_rect the rectangle to add to each accepted element's region
	 * @param in_extended_only TRUE when the elm_rect intersects the extended area only, FALSE if it intersects the main area
	 * @param check_first TRUE means that we check the elm, whether it passes the filter, FALSE - we presume it does
	 * @return TRUE in case of success, FALSE in of HandleElementFound failure. */

	BOOL			HandleElementsUpToContainer(HTML_Element* elm, const OpRect& elm_rect, BOOL in_extended_only, BOOL check_first = TRUE);

	/** Iterates through all the open elements. Adds the rect to each one of them and for all it's ancestors until first container
	 *	exclusive (calls HandleElementsUpToContainer for each open element).
	 * @param elm_rect the rectangle to add to each interested element's region
	 * @param in_extended_only TRUE when the elm_rect intersects the extended area only, FALSE if it intersects the main area
	 * @return TRUE in case of success, FALSE in of HandleElementsUpToContainer failure. */

	BOOL			AddRectToOpenElements(const OpRect& elm_rect, BOOL in_extended_only);

	/** Finds the first ancestor of container_elm that passes the filter (container_elm inclusive).
	*	Stops, on encountering the first element that has a container or a table content
	*	and that element is not checked against the filter. If found, adds the element to open_elements vector.
	*	In case of OOM, sets the appropriate flags.
	* @return TRUE if ok, FALSE on OOM.  */

	BOOL			AddFirstInterestingElement(HTML_Element* container_elm);

	/** Opposite to the above, removes the element previously found, when started a search from container_elm. */

	void			RemoveCorrespondingElement(HTML_Element* container_elm);

	/** Handles the usemap, which intersects with the searched area.
	 * @param usemap_owner the element, which has usemap attribute
	 * @param clip_rect the rectangle that bounds the usemap (it is the intersection of the usemap_owner box and the current clip rect)
	 * @param offset_x the x offset for the areas in the usemap, so that after adding it they are relative to the current translation
	 * @param offset_y the y offset for the areas in the usemap, so that after adding it they are relative to the current translation
	 * @return TRUE if reporting all the found areas went ok, FALSE when the subclass signals wish to terminate the traverse. */

	BOOL			HandleUsemapElement(HTML_Element* usemap_owner, const OpRect& clip_rect, INT32 offset_x, INT32 offset_y);

	/** Checks and possibly reports a scrollbar in terms of an object and an overlapping rect. */

	BOOL			CheckScrollbar(ScrollableArea* scrollable, OpRect& scrollbar_rect, BOOL check_extended, BOOL vertical);

	/** Intersects the rect in local coords with the current clip rect.
	 *	Then checks the intersection between the resulting rect and the extended area (if used) or the main area.
	 *  Marks in intersects_main_area whether the rect intersected the main area or only the extended area
	 *	Overrides HitTestingTraversalObject::IntersectsCustom. */

	virtual BOOL	IntersectsCustom(RECT& rect) const;

protected:

	void			TerminateTraverse() {terminate_traverse = TRUE;}

	BOOL			IsUsingExtendedArea() {return use_extended_area;}

	/** Returns a pointer to the main area. Never NULL, but valid only if extended area is used. */

	const RECT*		GetMainArea()	{return &main_area;}

	/**
    * @param    doc the doument, which is searched
    * @param    area area of the document that we are searching
    * @param    customizer object that is used to customize the behavior of the object being created
    * @param    extended_area optional, extended area. See ElementSearchObject description.
				The extended area (if used) MUST contain the main area.
	* @param	one_pass_traverse should the traverse be done in one pass. TRUE will make the search faster, but with
				a possible slight loss of precision regarding the visual overlapping checking. The order of
				HandleElementFound and HandleOverlappingRect calls may not reflect the painting order and thus
				giving the false negatives. This param is ignored when the customizer object passed to the ctor
				has the notify_overlapping_rects set to FALSE - then the traverse is always one pass. */

					ElementSearchObject(FramesDocument *doc,
										const RECT& area,
										ElementSearchCustomizer& customizer,
										const RECT* extended_area = NULL,
										BOOL one_pass_traverse = FALSE);

	/** Called from inside HitTestingTraversalObject::EnterVerticalBox box, when we should enter it according to HitTestingTraversalObject.
	 *  and it is not dry run currently.
	 *  When called, the clip rect (if any) that origins from the current VerticalBox has not been pushed yet.
	 *  @return TRUE when we should enter the vertical box according to this class (or the subclass), FALSE otherwise. */

	virtual BOOL	HandleVerticalBox(LayoutProperties* parent_lprops,
									  LayoutProperties*& layout_props,
									  VerticalBox* box,
									  TraverseInfo& traverse_info,
									  BOOL clip_affects_traversed_descendants);

	/** Called from inside HitTestingTraversalObject::EnterScrollable.
	 *  When called, the clip rect that origins from the current scrollable has not been pushed yet.
	 *  @return TRUE when we should enter the scrollable, FALSE otherwise. */

	virtual BOOL	HandleScrollable(const HTMLayoutProperties& props,
									 ScrollableArea* scrollable,
									 LayoutCoord width,
									 LayoutCoord height,
									 TraverseInfo& traverse_info,
									 BOOL clip_affects_target,
									 int scrollbar_modifier);

#ifdef CSS_TRANSFORMS

	/* Push transform */

	virtual OP_BOOLEAN PushTransform(const AffineTransform &t, TranslationState &state);

	/* Pop transform */

	virtual void	PopTransform(const TranslationState &state);

#endif // CSS_TRANSFORMS

	/** Checks whether the Box is considered opaque. If it is considered opaque, outputs a rectangle that approximates the covered area.
	*	NOTE: To simplify things, this method always outputs one rectangle, even if the box consisists of parts that are opaque and the ones that are not.
	*	It just outputs the rectangle that does not cover too much space that should not be covered nor the other way round.
	*	E.g. if the box has an opaque content and doesn't have any background, but has both non-zero border and padding, then the
	*	output rect can be either border including or only inside padding, depending on the border and padding proportions.
	* @param box the box that we check for being opaque
	* @param props
	* @param box_rect rectangle of the box including border relative to the current translation
	* @param[out] covering_rect the rectangle, relative to the current translation, of the covered area if the box is opaque. */

	BOOL			IsBoxOpaque(Box* box, const HTMLayoutProperties& props, const OpRect& box_rect, OpRect& covering_rect) const;

	// BEGIN: Interface methods provided by ElementSearchObject

	/** Called when a scrollbar in the searched area was found and the appropriate flag in the customizer object
	*	is set.
	* @param element the element that the scrollable that the scrollbar belongs to is associated with
	* @param vertical TRUE for a vertical scrollbar, FALSE for a horizontal one
	* @param rect the rectangle occupied by the scrollbar excluding the part outside current clipping rectangle,
	*				relative to the last transformation point and in current coordinate system.
	*				In particular - when there are no transforms in the document, the rectangle is always in document coordinates.
	* @return TRUE - ok, continue, FALSE - terminate traverse (from that point ElementSearchObject will not call any of the four:
	*  HandleScrollbarFound, HandleElementFound, HandleOverlappingRect, BeginOpacity. */

	virtual BOOL	HandleScrollbarFound(HTML_Element* element, BOOL vertical, const OpRect& rect) = 0;

	/** Called when a html element fragment in the searched area(s) was found. This method may be called multiple times for the
	* same html element, but with different rects. The element occupies the union of all the rects passed.
	* @param html_element the found element
	* @param rect the rectangle occupied by the element excluding the part outside current clipping rectangle,
	*				relative to the last transformation point and in current coordinate system.
	*				In particular - when there are no transforms in the document, the rectangle is always in document coordinates.
	* @param extended_area TRUE when the part of the element that occupies the rect intersects only the extended area and not the main area
	* @return TRUE - ok, continue, FALSE - terminate traverse (from that point ElementSearchObject will not call any of the four:
	*  HandleScrollbarFound, HandleElementFound, HandleOverlappingRect, BeginOpacity. */

	virtual BOOL	HandleElementFound(HTML_Element* html_element, const OpRect& rect, BOOL extended_area = FALSE) = 0;

	/** Called whenever a box that has opaque content or background was detected (or some other opaque object in the document, like a scrollbar).
	*	NOTE: This does not care for the opacity property. See BeginOpacity().
	* @param overlapping_box the HTML_Element that is associated with the opaque area
	* @param overlapping_rect the rectangle that surrounds the opaque area associated with the box or the object, relative to the last transformation point
	*						  and in current coordinate system. In particular - when there are no transforms in the document,
	*						  the rectangle is always in document coordinates.
	* @param extended_area TRUE when the overlapping rectangle intersects only the extended area and not the main area.
	* @return TRUE - ok, continue, FALSE - terminate traverse (from that point ElementSearchObject will not call any of the four:
	*  HandleScrollbarFound, HandleElementFound, HandleOverlappingRect, BeginOpacity. */

	virtual BOOL	HandleOverlappingRect(HTML_Element* element, const OpRect& overlapping_rect, BOOL extended_area = FALSE) = 0;

	/** Called whenever we enter a context with opacity < 1 and the appropriate flag of the customizer's object is set.
	 * @return TRUE - ok, continue, FALSE - terminate traverse (from that point ElementSearchObject will not call any of the four:
	 *  HandleScrollbarFound, HandleElementFound, HandleOverlappingRect, BeginOpacity. */

	virtual BOOL	BeginOpacity() = 0;

	/** Called whenever we leave a previously entered context with opacity < 1 and the appropriate flag of the customizer's object is set. */

	virtual void	EndOpacity() = 0;

	// END: Interface methods provided by ElementSearchObject

public:

	/** Enter vertical box. */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

    /** Enter line */

    virtual BOOL    EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

    /** Enter inline box */

    virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Enter column in multicol container or page in paged container. */

	virtual BOOL	EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info);

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Notify that we are back at the target origin. */

	virtual void	TargetFinished(Box* box);

	/** Check if this positioned element needs to be traversed. */

	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element);
};


/** TraversalObject used to collect the data related with the elements reported by ElemenSearchObject.
	Cares for the visiblity of elements (along with the ElementSearchObject's logic), by implementing
	HandleOverlappingRect and BeginOpacity methods, so that it marks the covered areas of the elements found before
	and possibly removes the totally covered elements.

	After successful traverse the data is put into the ElementCollection vector. If extended area is used,
	during traverse, it collects the fragments of elements that intersect the extended area only.
	But in the final call of CollectElements, it adds the rectangles from extended area, only for those elements
	that have at least one rectangle in main area. */

class ElementCollectingObject :
	public ElementSearchObject
{
private:

#ifdef SELFTEST
	friend class ST_layoutelement_search;
#endif // SELFTEST

	// Brew compiler issue with private member classes.

	class ElementHashFunctions;
	struct RectLink;
	class RectList;
	struct ElementValue;
	struct OpacityMarker;
	friend class ElementHashFunctions;
	friend struct RectLink;
	friend class RectList;
	friend struct ElementValue;
	friend struct OpacityMarker;

	/** Used in m_elm_hash and m_extended_hash below. */

	class ElementHashFunctions : public OpHashFunctions
	{
	public:
		virtual UINT32	Hash(const void* key);

		virtual BOOL	KeysAreEqual(const void* key1, const void* key2);
	};

	/**	Used to store the encountered transform contexts. Tree makes it easier to quickly move the pointer to the current
		transform context. */

	struct TransformTree
		: public Tree
	{
		TransformTree(AffinePos* transform = NULL) : m_transform(transform), ref_counter(0) {}
		~TransformTree() {if (m_transform) OP_DELETE(m_transform);}

		TransformTree*	FirstChild() const { return static_cast<TransformTree*>(Tree::FirstChild()); }
		TransformTree*	Parent() const { return static_cast<TransformTree*>(Tree::Parent()); }
		TransformTree*	Next() const { return static_cast<TransformTree*>(Tree::Next()); }

		AffinePos*		m_transform; // Transform root. NULL only for the root of the tree, meaning the document root.
		int				ref_counter; // How many rectangles of all that belong to the found element's regions use the m_transform transform root
	};

	// Represents one rectangle (may be transformed) of an element's region.

	struct RectLink : public Link
	{
		RectLink*	Suc() const {return static_cast<RectLink*>(Link::Suc());}
		RectLink*	Pred() const {return static_cast<RectLink*>(Link::Pred());}

		RectLink(const RECT* rect, TransformTree* tree) : Link(), rect(rect), transform_tree(tree) {}
		RectLink(const OpRect& rect, TransformTree* tree) : Link(), rect(rect), transform_tree(tree) {}
		~RectLink();

		OpRect				rect;
		TransformTree*		transform_tree;
	};

	// The Head for RectLink objects.

	class RectList : public AutoDeleteHead
	{
	public:
		RectLink*	First() const { return static_cast<RectLink*>(Head::First());}
		RectLink*	Last() const { return static_cast<RectLink*>(Head::Last());}
	};

	/** Identifies the given element (or scrollbar). */

	struct ElementKey : public Link
	{
		ElementKey(HTML_Element* elm, DocumentElementType type) : Link(), elm(elm), type(type) {}

		// Copy ctor. Copies only the ElementKey fields.

		ElementKey(const ElementKey &second) : Link(), elm(second.elm), type(second.type) {}

		BOOL operator==(const ElementKey& second) const {return elm == second.elm && type == second.type;}

		ElementKey*		Suc() const {return static_cast<ElementKey*>(Link::Suc());}
		ElementKey*		Pred() const {return static_cast<ElementKey*>(Link::Pred());}

		HTML_Element*	elm;
		DocumentElementType type;
	};

	/** A piece of data associated with the given element. */

	struct ElementValue
	{
		/** Adds a new transformed rect. */

		OP_STATUS		AddRect(const OpRect& rect, TransformTree *tree);

		/** Unchains RectLink's from the second and adds it to this object's list. */

		void			MergeRects(const RectList& second);

		/** Computes the region that is to_divide->rect minus the dividing_rect
			and adds the appropriate RectLinks that cover up the remaining region.
			(Both to_divide->rect and dividing rect must have the same transform root).
			Does not unchain to_divide from rects - the caller is responsible for that. */

		OP_STATUS		DivideRect(const RectLink* to_divide, const OpRect& dividing_rect);

		RectList rects; // Holds RectLinks
	};

	struct OpacityMarker : public Link
	{
		OpacityMarker*		Suc() const {return static_cast<OpacityMarker*>(Link::Suc());}
		OpacityMarker*		Pred() const {return static_cast<OpacityMarker*>(Link::Pred());}

		ElementKey *main_key, *extended_key;
	};

	typedef OpPointerHashTable<ElementKey, ElementValue> ElementHashMap;

	ElementHashFunctions m_hash_functions;

	/** The heads of the elements lists. Each list owns the pointers to ElementKey objects.
	 *  m_extended_list is used only in case of extended area being used. */

	AutoDeleteHead	m_elm_list, *m_extended_list;

	/** The top of the transforms tree. The tree owns the pointers to AffinePos objects.
	 *	The top TransformTree is the only node that has m_transform equal to NULL.
	 *	Others must have a pointer to valid transform root. */

	TransformTree	m_transform_tree;

	/** The below two act as mappings from an ElementKey to ElementValue. They are also used for faster access to the elements
	 *  found so far. m_extended_hash is used only in case of using extended area in ElementSearchObject. */

	ElementHashMap	*m_elm_hash, *m_extended_hash;

	/** A pointer to the current position in transform tree. */

	TransformTree*	m_current_transform;

	/** A pointer to the list of structures that mark the places in the m_elm_list and m_extended_list, where we have
		entered opacity < 1 (from now on the elements before that markers cannot be visually covered). */

	AutoDeleteHead*	m_opacity_markers;

	/** Removes the element from it's list and hash and deletes it. */

	void			RemoveElement(ElementKey* elm, ElementHashMap* hash);

protected:

	/** Implementation of ElementSearchObject. */

	virtual BOOL	HandleScrollbarFound(HTML_Element* element, BOOL vertical, const OpRect& rect);
	virtual BOOL	HandleElementFound(HTML_Element* html_element, const OpRect& rect, BOOL extended_area = FALSE);
	virtual BOOL	HandleOverlappingRect(HTML_Element* element, const OpRect& overlapping_rect, BOOL extended_area = FALSE);
	virtual BOOL	BeginOpacity();
	virtual void	EndOpacity();

#ifdef CSS_TRANSFORMS
	/* Push transform */

	virtual OP_BOOLEAN PushTransform(const AffineTransform &t, TranslationState &state);

	/* Pop transform */

	virtual void	PopTransform(const TranslationState &state);
#endif //CSS_TRANSFORMS

public:

	/** Parameter set is the same as for the ctor of ElementSearchObject. */

					ElementCollectingObject(FramesDocument *doc,
											const RECT& area,
											ElementSearchCustomizer& customizer,
											const RECT* extended_area = NULL,
											BOOL one_pass_traverse = FALSE)
						: ElementSearchObject(doc, area, customizer, extended_area, one_pass_traverse)
						, m_extended_list(NULL)
						, m_elm_hash(NULL)
						, m_extended_hash(NULL)
						, m_current_transform(&m_transform_tree)
						, m_opacity_markers(NULL) {}

	/** Initializes the ElementCollectingObject instance. Must be called after ctor, before traversing.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if OOM, in such case must not be used. */

	OP_STATUS		Init();

					~ElementCollectingObject();

	/** Collects all the found elements and their locations. Can be called once after successful traverse.
	 * NOTE: The behavior is undefined, when the method is called after unsuccessful traverse or is called
	 * for the second or more time on the same object.
	 * In case of OOM the caller is responsible for the cleanup of incomplete collection.
	 * @param[out] collection an output vector for all the collected elements data. Should be empty when passed.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY in case of OOM. */

	OP_STATUS		CollectElements(DocumentElementCollection& collection);
};

/** Represents a selection point as handled internally by Layout.

    TEXT + offset means the same thing as in normal boundary points.
    ELEMENT + offset must have offset 0 or 1 and means before or after ELEMENT.

    TextSelection/SelectionBoundaryPoint use a different representation that is
    used in other parts of Core (and is also used in specifications). */

class LayoutSelectionPoint
{
	HTML_Element*	m_element;
	int				m_offset;
	SelectionBoundaryPoint::BindDirection
					m_bind_direction;

public:
					LayoutSelectionPoint() :
						m_element(NULL),
						m_offset(0),
						m_bind_direction(SelectionBoundaryPoint::BIND_BACKWARD) {}

					LayoutSelectionPoint(HTML_Element* elm, int offset) :
						m_element(elm),
						m_offset(offset),
						m_bind_direction(SelectionBoundaryPoint::BIND_BACKWARD) {}

					LayoutSelectionPoint(const SelectionBoundaryPoint& boundary_point) :
						m_bind_direction(boundary_point.GetBindDirection())
					{
						TextSelection::ConvertPointToOldStyle(boundary_point, m_element, m_offset);
					}

	void			SetLogicalPosition(HTML_Element* elm, int offset) { m_element = elm; m_offset = offset; m_bind_direction = SelectionBoundaryPoint::BIND_BACKWARD; }

	/** Set the bind direction.
	
	    @see SelectionBoundaryPoint::SetBindDirection()

	    @param[in] direction The new bind direction. */

	void			SetBindDirection(SelectionBoundaryPoint::BindDirection direction) { m_bind_direction = direction; }

	/** Get the bind direction.

	    @see SelectionBoundaryPoint::GetBindDirection() */

	SelectionBoundaryPoint::BindDirection
					GetBindDirection() const { return m_bind_direction; }

	HTML_Element*	GetElement() const { return m_element; }
	int				GetOffset() const { return m_offset; }
};


/** A class for extracting word information from a selection point. */

class SelectionPointWordInfo
{
	/** The word a selection point indicates. */

	const uni_char*	m_word;

	/** Offset in the selected word. See the word member variable. */

	int				m_word_character_offset;

	/** Takes element_character_offset and calculates the word based offset.

	    NOTE: The method does the calculation only if element is a text element that
	    has a layout box. */

	void			CalculateWordBasedOffset(const HTML_Element* element, unsigned int character_offset, BOOL prefer_word_before_boundary);

public:

	/** Default constructor, creating an empty object. */

					SelectionPointWordInfo() :
						m_word(NULL),
						m_word_character_offset(0) {}

	/** Constructor.

		@param selection_point The SelectionBoundaryPoint to base the word selection on.
		@param prefer_word_before_boundary If a SelectionBoundaryPoint has more than one
		       visual position, this controls which is preferred in the end result. */

					SelectionPointWordInfo(const SelectionBoundaryPoint& selection_point, BOOL prefer_word_before_boundary)
					{
						CalculateWordBasedOffset(selection_point.GetElement(), selection_point.GetOffset(), prefer_word_before_boundary);
					}

	/** Constructor.

		@param selection_point The LayoutSelectionPoint to base the word selection on.
		@param prefer_word_before_boundary If a LayoutSelectionPoint has more than one
		       visual position, this controls which is preferred in the end result. */

					SelectionPointWordInfo(const LayoutSelectionPoint& selection_point, BOOL prefer_word_before_boundary)
					{
						CalculateWordBasedOffset(selection_point.GetElement(), selection_point.GetOffset(), prefer_word_before_boundary);
					}

	/** Get start of word containing selected text */

	const uni_char*	GetWord() { return m_word; }

	/** Get character offset in text element or 0/1 offset for before/after other elements. */

	int				GetOffsetIntoWord() { return m_word_character_offset; }

	/** Assignment operator. */

	SelectionPointWordInfo&
					operator=(const SelectionPointWordInfo& other) { m_word = other.m_word; m_word_character_offset = other.m_word_character_offset; return *this; }
};

/** Useful calculcate information on top of the real LayoutSelectionPoint.

	Keeps a backwards compatible API to ease migration. */

class LayoutSelectionPointExtended
{
#ifdef DEBUG_ENABLE_OPASSERT
	/** Whether we have adjusted the selection point to work with layout.

		We do that lazily to allow the Reflow() to run first. */

	BOOL			m_adjusted;
#endif // DEBUG_ENABLE_OPASSERT

	BOOL			m_prefer_word_before_boundary;

	BOOL			m_adjust_direction_forward;

	BOOL			m_require_text;

	const SelectionBoundaryPoint&
					m_original_selection_point;

	LayoutSelectionPoint
					m_adjusted_selection_point;

	SelectionPointWordInfo
					m_word_info;

	/** Converts a "standard" selection point to one suitable for layout.

		There are currently two ways to represent a selection. There is one
		used in specs, in scripts and in TextSelection/SelectionBoundaryPoint. Then
		there is the representation used historically in Opera. This method
		converts SelectionBoundaryPoint to one that matches the format
		that our selection traversal objects are used to. Multiple SelectionBoundaryPoint
		points can map to the same "adjusted" point. */

	static LayoutSelectionPoint
					LayoutAdjustSelectionPoint(const SelectionBoundaryPoint& selection_point, BOOL adjust_direction_forward, BOOL require_text);

public:
					LayoutSelectionPointExtended(const SelectionBoundaryPoint& selection_point, BOOL prefer_word_before_boundary, BOOL adjust_direction_forward, BOOL require_text) :
#ifdef DEBUG_ENABLE_OPASSERT
						m_adjusted(FALSE),
#endif // DEBUG_ENABLE_OPASSERT
						m_prefer_word_before_boundary(prefer_word_before_boundary),
						m_adjust_direction_forward(adjust_direction_forward),
						m_require_text(require_text),
						m_original_selection_point(selection_point) {}

	/** Makes this object suitable for usage.

		It must be called before any data is read but after Reflow(). Typically
		in TraversalObject::StartTraversing(). */

	void			AdjustForTraverse();

	/** Returns the word that the adjusted selection point indicated or NULL if no word. */

	const uni_char*	GetWord() { OP_ASSERT(m_adjusted); return m_word_info.GetWord(); }

	/** Returns the number of characters into word that the adjusted selection point indicates. */

	int				GetOffsetIntoWord() {  OP_ASSERT(m_adjusted); return m_word_info.GetOffsetIntoWord(); }

	/** Returns the element in the adjusted selection point. */

	HTML_Element*	GetElement() const { OP_ASSERT(m_adjusted); return m_adjusted_selection_point.GetElement(); }

	/** Returns TRUE if this adjusted selection point is different from 'other'. */

	BOOL			operator!=(const LayoutSelectionPointExtended& other) const {return GetElement() != other.GetElement() || GetElementCharacterOffset() != other.GetElementCharacterOffset(); }

	/** Returns the offset from the start of the element to the adjusted selection point. */

	int				GetElementCharacterOffset() const { OP_ASSERT(m_adjusted); return m_adjusted_selection_point.GetOffset(); }
};

/** Information about position and size for a LayoutSelectionPoint.

	Typically to be used for sizing carets or start/end text markers. */

class SelectionPointPosition
{
	LayoutCoord		virtual_position;

#ifdef LAYOUT_CARET_SUPPORT

	/* The x position of the caret, in pixels, into the word. */

	int				pixel_caret_x;

	/** The x translation */

	int				pixel_translation_x;

	/** The y translation */

	int				pixel_translation_y;

	/** The height of the element this text selection point belongs
		to.  Can be the line height, then equal to `lineheight' or the
		height of the text box. */

	int				height;

	/** The y translation of the line this text selection belongs
		to.  */

	int				line_y;

	/** The height of the line this text selection point belongs
		to. */

	int				lineheight;

	/** If the text selection point is inside a transform root, it is
		saved here. Otherwise this is an empty translation.  */

	AffinePos		transform_root;


	/** The width of the character following the caret position.
		Used when we want to display a block caret. */

	int				character_width;

#endif // LAYOUT_CARET_SUPPORT

	/** Declaration of method that must not be used, to prevent automatic generation. */

					SelectionPointPosition(const SelectionPointPosition& other);

	/** Declaration of method that must not be used, to prevent automatic generation. */

					SelectionPointPosition& operator=(const SelectionPointPosition& other);

public:
					SelectionPointPosition()
						: virtual_position(0)
#ifdef LAYOUT_CARET_SUPPORT
						, pixel_caret_x(0),
						  pixel_translation_x(0),
						  pixel_translation_y(0),
						  height(0),
						  line_y(0),
						  lineheight(0),
						  character_width(0)
#endif // LAYOUT_CARET_SUPPORT
		{}

	void			Reset() { virtual_position = LayoutCoord(0); }

	void			SetVirtualPosition(LayoutCoord virtual_pos) { virtual_position = virtual_pos; }

	/** Get virtual position of selection point inside text.

		This is the number of pixels from the start of the virtual line. */

	LayoutCoord		GetVirtualPosition() const { return virtual_position; }

#ifdef LAYOUT_CARET_SUPPORT

	/** Get x-position in pixels in the word for the caret */

	int				GetCaretPixelOffset() const { return pixel_caret_x; }

	/** Set x-position in pixels in the word for the caret */

	void			SetCaretPixelOffset(int x) { pixel_caret_x = x; }

	/** Get translation x in pixels for the selection point */

	int				GetPixelTranslationX() const { return pixel_translation_x; }

	/** Set translation x in pixels for the selection point */

	void			SetPixelTranslationX(int translation_x) { pixel_translation_x = translation_x; }

	/** Get translation y in pixels for the selection point */

	int				GetPixelTranslationY() const { return pixel_translation_y; }

	/** Set translation y in pixels for the selection point */

	void			SetPixelTranslationY(int translation_y) { pixel_translation_y = translation_y; }

	/** Set height in pixels of the selected element */

	void			SetHeight(int height) { this->height = height; }

	/** Get height in pixels of the selected element */

	int				GetHeight() const { return height; }

	/** Set lineheight in pixels of the selected element */

	void			SetLineHeight(int lineheight) { this->lineheight = lineheight; }

	/** Get lineheight in pixels of the selected element */

	int				GetLineHeight() const { return lineheight; }

	/** Set ypos of the line in pixels of the selected element */

	void			SetLineTranslationY(int line_y) { this->line_y = line_y; }

	/** Get ypos of the line in pixels of the selected element */

	int				GetLineTranslationY() const { return line_y; }

	/** Set affine transform */

	void			SetTransformRoot(const AffinePos &ctm) { transform_root = ctm; }

	/** Get affine transform */

	AffinePos		GetTransformRoot() const { return transform_root; }

	/** Set the width of the character following the caret position. */

	void			SetCharacterWidth(int character_width) { this->character_width = character_width; }

	/** Get the width of the character following the caret position. */

	int				GetCharacterWidth() const { return character_width; }

#endif // LAYOUT_CARET_SUPPORT
};

/** Text selection object.

	Used to find the selectable box that is nearest to a focus point.

	Input to a text selection object (the constructor) is a pair of x,y
	coordinates which represent previous and current focus points (pointer
	positions) when selecting.
	The two points are necessary to know the movement vector in case of fast
	pointer movement (in order to avoid losing characters).
	The text selection object can then (if Traverse() is called) search through
	the layout structure and find the element+offset that is nearest to the new
	anchor point (this element is returned from GetNearestElement() and offset
	from GetNearestOffset()), and, if this element is a text node, it can also
	tell which word in that text node is nearest to the focus point
	(GetNearestWord()) and also which character in that word is
	nearest (GetNearestWordCharacterOffset()). */

class TextSelectionObject
  : public HitTestingTraversalObject
{
private:

	HTML_Element*	selection_container_element;

	HTML_Element*	nearest_element;

	HTML_Element*	nearest_word_element;

	const uni_char*	nearest_word;

	int				nearest_word_character_offset;

	/** Euclidian distance to nearest box. */

	LayoutCoord		nearest_box_distance;

	LayoutCoord		nearest_box_x;

	LayoutCoord		nearest_line_distance;

	const Line*		nearest_line;

	const Line*		line;

	/** Position relative to the nearest element: before, after or offset inside.

		If the element is not a text element (typically a replaced element), it
		is 0 if the chosen point is before it or 1 if the chosen point is after
		it. If this is a text element, this is an offset into the character
		data. */

	int				nearest_offset;

	/** @see IsAtEndOfLine(). */

	BOOL			is_at_end_of_line;

	/** Should we find the nearest out of all visible elements. */

	BOOL			find_nearest;

	/** Should we only accept words starting with alphanumeric characters in the selection? */

	BOOL			only_alphanum;

	/** The original x translation. */

	LayoutCoord		orig_x;

	/** The original y translation.  */

	LayoutCoord		orig_y;

	/** The previous position of the pointer (x axis) */

	LayoutCoord		previous_x;

	/** The previous position of the pointer (y axis) */

	LayoutCoord		previous_y;


	/** Compute the selection focus area, based on current
	 *  and previous focus points, in local coordinates
	 *  of the current translation/transform. */

	RECT			GetLocalSelectionFocusArea() const;

public:

	/** Creates a new selection object.

		@param [in] doc Document to perform selection in.
		@param [in] x Current focus point x-position in document coordinate system.
		@param [in] y Current focus point y-position in document coordinate system.
		@param [in] previous_x Previous focus point x-position in document coordinate system.
		@param [in] previous_y Previous focus point y-position in document coordinate system.
		@param [in] find_nearest Set to true if the entire visual viewport should
								 be traversed in order to find the nearest selectable box.
								 This is useful if one wants to find the nearest box among
								 all visible boxes, not just the nearest local box.
		@param [in] only_alphanum Set to true to accept only text boxes with a word
								  starting with an alpha-numeric or single quote
								  character. This is useful when wanting to find a
								  word that is selectable using ExpandSelection(). */

					TextSelectionObject(FramesDocument* doc, LayoutCoord x, LayoutCoord y, LayoutCoord previous_x, LayoutCoord previous_y,
										BOOL find_nearest = FALSE, BOOL only_alphanum = FALSE)
					  : HitTestingTraversalObject(doc, TRUE),
						selection_container_element(NULL),
						nearest_element(NULL),
						nearest_word_element(NULL),
						nearest_word(NULL),
						nearest_word_character_offset(0),
						nearest_box_distance(LAYOUT_COORD_MAX),
						nearest_box_x(0),
						nearest_line_distance(LAYOUT_COORD_MAX),
						nearest_line(NULL),
						line(NULL),
						nearest_offset(0),
						is_at_end_of_line(FALSE)
					{
						SetTraverseType(TRAVERSE_ONE_PASS);

						this->find_nearest = find_nearest;
						this->only_alphanum = only_alphanum;

						if (this->find_nearest)
						{
							OpRect vp_rect = doc->GetVisualViewport();
							area.left = vp_rect.x;
							area.top = vp_rect.y;
							area.right = vp_rect.x + vp_rect.width;
							area.bottom = vp_rect.y + vp_rect.height;
						}
						else
						{
							area.left = MIN(previous_x, x);
							area.right = MAX(previous_x, x) + 1;
							area.top = MIN(previous_y, y);
							area.bottom = MAX(previous_y, y) + 1;
						}

						orig_x = x;
						this->previous_x = previous_x;
						orig_y = y;
						this->previous_y = previous_y;
					}

	/** Restrict the selection to inside a container. Use for contentEditable and scrollables */

	void			SetSelectionContainer(HTML_Element*	container) { selection_container_element = container; }

	/** Get the document position that is closest to (xpos, ypos). */

	SelectionBoundaryPoint
					GetNearestBoundaryPoint() const { return TextSelection::ConvertFromOldStyle(nearest_element, nearest_offset); }

	/** Check if there is a valid nearest boundary point. */

	BOOL			HasNearestBoundaryPoint() const { return nearest_element != NULL; }

	/** Get the text box that is closest to (xpos, ypos) */

	HTML_Element*	GetNearestWordElement() const { return nearest_word_element; }

	/** Get start of word that is closest to (xpos, ypos) */

	const uni_char*	GetNearestWord() const { return nearest_word; }

	/** Get the offset into the word that is closest to (xpos, ypos) */

	int				GetNearestWordCharacterOffset() const { return nearest_word_character_offset; }

	/** Get distance beetween left side of nearest box and xpos */

	LayoutCoord		GetNearestTextBoxX() const { return nearest_box_x; }

	/** @return TRUE if the focus point was at the end of or to the right of the line; otherwise, FALSE. */

	BOOL			IsAtEndOfLine() const { return is_at_end_of_line; }

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Leave line */

	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

#ifdef DOCUMENT_EDIT_SUPPORT

	/** "Enter" layout break. */

	virtual BOOL	EnterLayoutBreak(HTML_Element* break_element) { return !target_element && document->GetDocumentEdit(); }

#endif //  DOCUMENT_EDIT_SUPPORT

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Handle selectable box */

	virtual void	HandleSelectableBox(LayoutProperties* layout_props);
};

/** Traversal object that invalidates the area between two selection points.

	The input to a selection update object (constructor) consists of two text
	selection points. These may be anything (for example the old and the new
	focus points when the user has moved the cursor while pressing the left
	mouse button), as long as the first point precedes the second one logically
	in the element tree.

	The selection update update object will go through the elements in the
	range constituted by the two text selection points and request that they be
	repainted. */

class SelectionUpdateObject
  : public VisualTraversalObject
{
private:

	/** The logical start of the new selection.

		Is either the pointer position or the previous end point. */

	LayoutSelectionPointExtended
					start_update_range;

	/** The logical end of the new selection.

		Is either the pointer position or the previous end point. */

	LayoutSelectionPointExtended
					end_update_range;

	/** State info */

	BOOL			start_box_done;

	BOOL			end_box_done;

	/** Calculate bounding rect. */

	BOOL			calculate_bounding_rect;

	/** Bounding rectangle for updated area. */

	RECT			bounding_rect;

public:

					SelectionUpdateObject(FramesDocument* doc, const SelectionBoundaryPoint* new_start, const SelectionBoundaryPoint* new_end, BOOL calc_bounding_rect = FALSE);

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Callback for objects needing to set up code before starting traversal */

	virtual void	StartingTraverse();

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Check if this positioned element needs to be traversed. */

	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element);

	/** Handle selectable box */

	virtual void	HandleSelectableBox(LayoutProperties* layout_props);

	/** Get bounding rectangle for updated area. Set calculate_bounding_rect to TRUE when the object is created to use this. */

	const RECT&		GetBoundingRect() const { OP_ASSERT(calculate_bounding_rect); return bounding_rect; }

	/** State info */

	BOOL			GetStartBoxDone() { return start_box_done; }

	/** State info */

	BOOL			GetEndBoxDone() { return end_box_done; }

	/** "Enter" layout break. */

	virtual BOOL	EnterLayoutBreak(HTML_Element* break_element);

	/** Handle break box */

	virtual void	HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break);
};

/** Class that calculates interesting information about the point in the document specified by a SelectionBoundaryPoint.

	The "calculate selection point" object will gather information about
	character width, line height, position and other information.
	@see SelectionPointPosition */

class CalcSelPointPositionObject
  : public VisualTraversalObject
{
private:

	/** The selection point to examine. */

	const LayoutSelectionPoint
					sel_point;

	/** The output object. This is where the result is saved. */

	SelectionPointPosition
					sel_point_layout;

	/** State info */

	BOOL			sel_point_done;

	/** Calculate bounding rect. */

	BOOL			calculate_bounding_rect;

	/** Bounding rectangle for updated area. */

	RECT			bounding_rect;

public:

					CalcSelPointPositionObject(FramesDocument* doc, const SelectionBoundaryPoint* new_sel_point, BOOL calc_bounding_rect = FALSE);

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Check if this positioned element needs to be traversed. */

	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element);

	/** Handle selectable box */

	virtual void	HandleSelectableBox(LayoutProperties* layout_props);

	/** Get bounding rectangle for updated area. Set calculate_bounding_rect to TRUE when the object is created to use this. */

	const RECT&		GetBoundingRect() const { OP_ASSERT(calculate_bounding_rect); return bounding_rect; }

	/** Returns TRUE if the selection point was found by the traverse.

		It's possible to specify selection points that have no layout position
		at all and then this might return FALSE and the output will be
		nonsensical. */

	BOOL			HasSelPointPosition() { return sel_point_done; }

	/** "Enter" layout break. */

	virtual BOOL	EnterLayoutBreak(HTML_Element* break_element);

	/** Get the calculated result for the place in the document pointed to by the LayoutSelectionPoint. */

	const SelectionPointPosition&
					GetSelectionPointPosition() { return sel_point_layout; }
};

/** Traversal object that calculates a union rectangle and the list of rectangles enclosing
 * the selection (a selected text and a replaced content with the selection backgound) */

class CalculateSelectionRectsObject
  : public VisualTraversalObject
{
private:

	/** The start point of the selection. */

	LayoutSelectionPointExtended
					start_range;

	/** The end point of the selection. */

	LayoutSelectionPointExtended
					end_range;

	/** State info */

	BOOL			start_box_done;

	BOOL			end_box_done;

	/** Bounding rectangle of the selection. */

	OpRect			union_rect;

	/** The list of rectangles the selection consists of (in document coordinates). */

	OpAutoVector<OpRect>
					selection_rects;

	/** Clipping rectangle for currently traversed element */

	OpRect			clip_rect;

	/** If TRUE, it's only checked whether point_to_check is contained by any of selection's rectangles.

		@see point_to_check. */

	BOOL			check_point_only;

	/** A point (in document coordinates) to be checked whether it's contained by any of selection's rectangles. */

	OpPoint			point_to_check;

	/** Becomes TRUE if point_to_check is contained by any of the selection's rectangles.

		@see point_to_check. */

	BOOL			point_is_contained;

	void			HandleSelectionRectangle(const OpRect& rect);

public:

	/** Constructor.

		Should be used when the union rectangle and the list of rectangles
		should be calculated. */

	CalculateSelectionRectsObject(FramesDocument* doc, const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end);

	/** Constructor.

		Should be used when checking whether the given point is contained by
		any of selection's rectangle. In such case the union rect and the list
		of rectangles are not calculated. */

	CalculateSelectionRectsObject(FramesDocument* doc, const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, const OpPoint& point);

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Handle replaced content */

	virtual void	HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content);

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Check if this positioned element needs to be traversed. */

	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element);

	/** Handle selectable box */

	virtual void	HandleSelectableBox(LayoutProperties* layout_props);

	/** Get bounding rectangle of thr selection. */

	const OpRect&	GetUnionRect() const { OP_ASSERT(!check_point_only); return union_rect; }

	/** Returns TRUE if the point passed to constructor is contained by the selection. */

	BOOL			IsPointContained() const { OP_ASSERT(check_point_only); return point_is_contained; }

	/** Get the list of rectangles the selection consists of (rectangles are in document coordinates). */

	const OpVector<OpRect>*
					GetSelectionRects() const { OP_ASSERT(!check_point_only); return &selection_rects; }

	/** State info */

	BOOL			GetStartBoxDone() { return start_box_done; }

	/** State info */

	BOOL			GetEndBoxDone() { return end_box_done; }

	/** "Enter" layout break. */

	virtual BOOL	EnterLayoutBreak(HTML_Element* break_element);

	/** Handle break box */

	virtual void	HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break);

	/** Applies clipping */

	virtual void	PushClipRect(const OpRect& cr, TraverseInfo& info);

	/** Removes clipping */

	virtual void	PopClipRect(TraverseInfo& info);

	/** Callback for objects needing to set up code before starting traversal */

	virtual void	StartingTraverse();

	/** Enter scrollable content. */

	virtual BOOL	EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave scrollable content. */

	virtual void    LeaveScrollable(TraverseInfo& traverse_info);

	/** Enter column in multicol container or page in paged container. */

	virtual BOOL	EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info);

	/** Leave column in multicol container or page in paged container. */

	virtual void	LeavePane(TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave paged container. */

	virtual void	LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Enter table content. */

	virtual BOOL	EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info);

	/** Leave table content. */

	virtual void	LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info);

	/** Begin clipping of a partially collapsed table cell. */

	virtual void	BeginCollapsedTableCellClipping(TableCellBox* box, const OpRect& clip_rect, TraverseInfo& traverse_info);

	/** End clipping of a partially collapsed table cell. */

	virtual void	EndCollapsedTableCellClipping(TableCellBox* box, TraverseInfo& traverse_info);
};


/** Traversal object that expands given selection. */

class ExpandSelectionObject
  : public VisualTraversalObject
{
private:

	/** Start of the old selection. */

	const SelectionBoundaryPoint&
					start_old_selection;

	/** End of the old selection. */

	const SelectionBoundaryPoint&
					end_old_selection;

	/** Start of the new selection. */

	SelectionBoundaryPoint
					start_new_selection;

	/** End of the new selection. */

	SelectionBoundaryPoint
					end_new_selection;

	/** Type of selection. */

	TextSelectionType
					selection_type;

	/** Has the new start element been found? */

	BOOL			start_new_found;

	/** Finished? */

	BOOL			finished;

	/** State info. */

	BOOL			start_old_found;
	BOOL			new_selection;

public:

					ExpandSelectionObject(FramesDocument* doc, const SelectionBoundaryPoint& old_start, const SelectionBoundaryPoint& old_end, TextSelectionType type);

	/** Get start of the new selection. */

	const SelectionBoundaryPoint*
					GetStartSelection() const { return &start_new_selection; }

	/** Get end of the new selection. */

	const SelectionBoundaryPoint*
					GetEndSelection() const { return &end_new_selection; }

	/** Is area selected? */

	BOOL			GetFinished() { return finished; }

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	/** Enter table row. */

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** "Enter" layout break. */

	virtual BOOL	EnterLayoutBreak(HTML_Element* break_element) { return !finished; }

	/** Handle break box */

	virtual void	HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break);

	/** Check if this positioned element needs to be traversed. */

	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element) { return !finished; }

	/** Callback for objects needing to set up code before starting traversal */

	virtual void	StartingTraverse();

};


/** Traversal object that copies the text between two selection points */

class SelectionTextCopyObject
  : public TraversalObject
{
private:

	LayoutSelectionPointExtended
					start_selection;

	LayoutSelectionPointExtended
					end_selection;

	BOOL			start_box_done;
	BOOL			end_box_done;

	BOOL			add_newline;
	int				blockquote_count;
	BOOL			previous_was_cell;

	BOOL			blockquotes_as_text;

	uni_char*		buffer;
	int				buffer_length;

	int				copy_length;

	/** Add a newline to the copied text. */

	void			AddNewline();

	/** Add a number of ">" characters, corresponding to the the current number of
	 * <blockquote> nestings.
	 */

	void			AddQuoteCharacters();

public:

					SelectionTextCopyObject(FramesDocument* doc, const SelectionBoundaryPoint* start_selection_p, const SelectionBoundaryPoint* end_selection_p, uni_char* buf, int buf_len, BOOL blockquotes_as_text)
					  : TraversalObject(doc),
						start_selection(*start_selection_p, FALSE, FALSE, TRUE),
						end_selection(*end_selection_p, FALSE, FALSE, TRUE),
						start_box_done(start_selection_p->GetElement() == NULL),
						end_box_done(end_selection_p->GetElement() == NULL),
						add_newline(FALSE),
						blockquote_count(0),
						previous_was_cell(FALSE),
						blockquotes_as_text(blockquotes_as_text),
						buffer(buf),
						buffer_length(buf_len),
						copy_length(0)
					{
						next_container_element = doc->GetLogicalDocument()->GetRoot();
						SetTraverseType(TRAVERSE_ONE_PASS);
						SetEnterHidden(TRUE);
					}

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** Callback for objects needing to set up code before starting traversal */

	virtual void	StartingTraverse();

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Leave inline box */

	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Leave line */

	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Leave table row. */

	virtual void	LeaveTableRow(TableRowBox* row, TableContent* table);

	/** "Enter" layout break. */

	virtual BOOL	EnterLayoutBreak(HTML_Element* break_element) { return !end_box_done; }

	/** Handle break box */

	virtual void	HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break);

	/** Get length of selected text */

	int				GetLength() { return copy_length; }
};


/** Find first text object within the entire visual viewport closest
	to (x, y) */

class FirstTextObject
  : public TextSelectionObject
{
public:

					FirstTextObject(FramesDocument* doc, LayoutCoord x, LayoutCoord y)
						: TextSelectionObject(doc, x, y, x, y, TRUE) { }
};

/** Cliprect object. Finds out if a box is covered by something else than its own children. */

class CliprectObject
  : public HitTestingTraversalObject
{
private:

	BOOL			invalid_cliprect;

	BOOL			target_found;

	BOOL			content_done;

	Box*			target_box;

	void			Intersect(const RECT &box_area);

protected:

	/** Called from inside HitTestingTraversalObject::EnterVerticalBox box, when we should enter it according to HitTestingTraversalObject
	 *  and it is not dry run currently.
	 *  When called, the clip rect (if any) that origins from the current VerticalBox has not been pushed yet.
	 * @param clip_affects_traversed_descendants TRUE when the box introduces clipping (because of overflow hidden or clip property)
	 *		  and affects the descendant(s) that will be traversed to in that run. In other words this param is TRUE iff
	 *		  (there is some clipping) && ((no target element set) || ((target element is set) && (clipping affects the target)))
	 * @return TRUE when we should enter the vertical box according to the subclass, FALSE otherwise. */

	virtual BOOL	HandleVerticalBox(LayoutProperties* parent_lprops,
									  LayoutProperties*& layout_props,
									  VerticalBox* box,
									  TraverseInfo& traverse_info,
									  BOOL clip_affects_traversed_descendants);

	/** Called from inside HitTestingTraversalObject::EnterScrollable.
	 *  When called, the clip rect that origins from the current scrollable has not been pushed yet.
	 *  @return TRUE when we should enter the scrollable, FALSE otherwise. */

	virtual BOOL	HandleScrollable(const HTMLayoutProperties& props,
									 ScrollableArea* scrollable,
									 LayoutCoord width,
									 LayoutCoord height,
									 TraverseInfo& traverse_info,
									 BOOL clip_affects_target,
									 int scrollbar_modifier);

public:

					CliprectObject(FramesDocument* doc, Box* box)
					  : HitTestingTraversalObject(doc),
						invalid_cliprect(FALSE),
						target_found(FALSE),
						content_done(FALSE),
						target_box(box)	{ area.left = 0; area.right = LONG_MAX; area.top = 0; area.bottom = LONG_MAX; }

	/** Get rectangle */

	BOOL			GetCliprect(RECT& rect) const { rect = area; return target_found && !invalid_cliprect; }

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);
};

struct SearchTextMatch
{
	/** Start of matched text. */

	SelectionBoundaryPoint
					start_match;

	/** Number of characters already matched. */

	int				characters_done;
};

/** Traversal object that locates a text */

class SearchTextObject
  : public TraversalObject
{
protected:

	/** Start of search. */

	SelectionBoundaryPoint
					start_point;

	/** Pointer to a FormObject if the search should start from it */

	FormObject*		start_form_object;

	/** Start of matched text. */

	SelectionBoundaryPoint
					start_match;

	/** End of matched text. */

	SelectionBoundaryPoint
					end_match;

	/** When matching whole words we can have cases where we don't find the matches until the next element.
		In those cases we set end_pending_word_finished to mark the end of the matched word that we should
		use if the next word is a end-of-word, or something. **/

	SelectionBoundaryPoint
					end_pending_word_finished;

	/** Text to match. */

	const uni_char*	text;

	/** Direction of search.*/

	BOOL			forward;

	/** Case sensitive? */

	BOOL			match_case;

	/** Match whole words? */

	BOOL			match_words;

	/** Match the current selected word. (For incremental search while typing) */

	BOOL			match_current;

	/** Match only text within links */

	BOOL			match_only_links;

	/** Number of characters already matched. */

	int				characters_done;

	/** Finished? */

	BOOL			finished;

	/** In backward searces, when start_point is reached but we have to complete already started matches. */

	BOOL			pending_finished;

	/** Is start point passed? */

	BOOL			start_found;

	/** Array of partial matches. */

	SearchTextMatch*
					matches;

	/** The formobject we found it in, if GetTextFoundInForm is TRUE */

	FormObject*		match_form_object;

	/** Remove a partial match and compact array. */

	void			RemovePartialMatch(int i);

protected:

	BOOL			IsFinished() { return finished; }
	void			ResetSearch(BOOL reset_to_previous_match, int *next_pos = NULL);

	void			FinishPendingMatches(const Line* line);

	void			MatchAll(LayoutProperties* layout_props,
							 Text_Box* box,
							 const uni_char* word,
							 int word_length,
							 LayoutCoord word_width,
							 LayoutCoord space_width,
							 LayoutCoord justified_space_extra,
							 const WordInfo& word_info,
							 LayoutCoord virtual_pos,
							 int start_word_idx = 0);

public:

					SearchTextObject(FramesDocument* doc, const uni_char* txt, BOOL forward_search, BOOL matchcase, BOOL matchwords, BOOL matchcurrent, BOOL matchonlylinks)
					  : TraversalObject(doc),
						start_form_object(NULL),
						text(txt),
						forward(forward_search),
						match_case(matchcase),
						match_words(matchwords),
						match_current(matchcurrent),
						match_only_links(matchonlylinks),
						characters_done(0),
						finished(FALSE),
						pending_finished(FALSE),
						start_found(TRUE),
						matches(NULL),
						match_form_object(NULL) { SetTraverseType(TRAVERSE_ONE_PASS); }

					~SearchTextObject() { OP_DELETEA(matches); }

	/** Initialize search. */

	BOOL			Init();

	/** Set selection point to start searching from. */

	void			SetStartPoint(const SelectionBoundaryPoint& start) { start_point = start; start_found = !forward; }

	/** Set a formobject from which the search should start */

	void			SetStartFormObject(FormObject* start_form_object) { this->start_form_object = start_form_object; }

	/** Get start of matched text. */

	const SelectionBoundaryPoint*
					GetStart() const { return &start_match; }

	/** Get end of matched text. */

	const SelectionBoundaryPoint*
					GetEnd() const { return &end_match; }

	/** Is text found? */

	BOOL			GetTextFound() const { return start_match.GetElement() && end_match.GetElement(); }

	/** Is text found? */

	BOOL			GetTextFoundInForm() const { return !!match_form_object; }

	/** The formobject we found it in, if we found it in a formobject */

	FormObject*		GetMatchFormObject() { return match_form_object; }

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return FALSE; }

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj);

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info) { return !finished; }

	/** Enter line */

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info) { return !finished; }

	/** Leave line */

	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);
};

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT

class SearchTextAllObject
  : public SearchTextObject
{
private:
	void			LeaveContext(const Line* line);

public:

					SearchTextAllObject(FramesDocument* doc, const uni_char* txt, BOOL matchcase, BOOL matchwords, BOOL matchonlylinks)
					  : SearchTextObject(doc, txt, TRUE, matchcase, matchwords, TRUE, matchonlylinks) {}

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	virtual void	HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj);

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

	virtual BOOL	TraverseInLogicalOrder() const { return FALSE; }
};


class HighlightUpdateObject
  : public VisualTraversalObject
{
private:

	/** The current search hit */

	SelectionElm*	m_next_hit;

	/** Are we between endpoints of a selection */

	BOOL			m_start_done;

	/** Should we select or deselect the search hits */

	BOOL			m_select;

	/** Bounding rectangle of selected area. */

	RECT			m_bounding_rect;

public:

					HighlightUpdateObject(FramesDocument* doc, SelectionElm* first_hit, BOOL select);

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** methods called to check if we should enter the different boxes when traversing */

	virtual void	EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment);

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	virtual void	HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj);
	virtual void	HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content);

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);
	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);
	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	virtual BOOL	EnterTableRow(TableRowBox* row);

	/** Check if this positioned element needs to be traversed. */
	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element);
};

#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

/** Traversal object that updates boxes which have text decoration blink */

class BlinkObject
  : public VisualTraversalObject
{
public:

					BlinkObject(FramesDocument* doc, const RECT& area)
					  : VisualTraversalObject(doc, area) {}

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);
};

#ifdef LINK_TRAVERSAL_SUPPORT

/** Traversal object that locates links in the document, used by the link panel */

class LinkTraversalObject
	: public TraversalObject
{
private:

	HTML_Element*					m_last_elm;
	OpElementInfo*					m_op_elm_info;
	OpAutoVector<OpElementInfo>*	m_vector;

	void	InsertCommonInfo(HTML_Element *elm, LayoutProperties* layout_props, URL *url, OpElementInfo::EIType type, LinkType kind = LINK_TYPE_OTHER);
	void	CheckForLinkElements();
	void	CheckForCLink(HTML_Element *elm, LayoutProperties* layout_props);

public:

	LinkTraversalObject(FramesDocument* doc, OpAutoVector<OpElementInfo>* vector)
		: TraversalObject(doc),
		m_last_elm(NULL),
		m_op_elm_info(NULL),
		m_vector(vector) { SetTraverseType(TRAVERSE_ONE_PASS); }

	/** Enter vertical box */
	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Leave vertical box */
	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Handle text content */

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Enter inline box */
	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Leave inline box */
	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);
};

#endif // LINK_TRAVERSAL_SUPPORT

/** TraversalObject for finding relevant text containers in a document and store coordinates for those to
	give some hints to an automatic scrolling mechanism. */

class TextContainerTraversalObject : public HitTestingTraversalObject
{
private:

	Head&			list;

	int				text_content_count;

	OpRect			pending_rectangle;

	BOOL			starting_line;

	OP_STATUS		CommitContainerRectangle(BOOL force, Markup::Type);

	BOOL			ImportantContent(ReplacedContent* c);

	/** Handles a text or list item markers (both numeration and bullet ones).

		@param box_rect the rectangle of the content in doc root coordinates.
						May be bboxed in case of transforms.
		@param line_height when non zero, it will ensure the new height to be line_height + the vertical offset
						   between the box_rect and the pending one. Otherwise standard union will be done. */

	void			HandleLineContent(const OpRect& box_rect, LayoutCoord line_height = LayoutCoord(0));

protected:

	/** Called from inside HitTestingTraversalObject::EnterVerticalBox box, when we should enter it according to HitTestingTraversalObject
		and it is not dry run currently.
		When called, the clip rect (if any) that origins from the current VerticalBox has not been pushed yet.

		@param clip_affects_traversed_descendants TRUE when the box introduces clipping (because of overflow hidden or clip property)
			   and affects the descendant(s) that will be traversed to in that run. In other words this param is TRUE iff
			   (there is some clipping) && ((no target element set) || ((target element is set) && (clipping affects the target)))
		@return TRUE when we should enter the vertical box according to the subclass, FALSE otherwise. */

	virtual BOOL	HandleVerticalBox(LayoutProperties* parent_lprops,
									  LayoutProperties*& layout_props,
									  VerticalBox* box,
									  TraverseInfo& traverse_info,
									  BOOL clip_affects_traversed_descendants);

public:

	TextContainerTraversalObject(FramesDocument* doc, const RECT& area, Head& alist)
		: HitTestingTraversalObject(doc, area),
		  list(alist),
		  text_content_count(0),
		  starting_line(TRUE) { SetTraverseType(TRAVERSE_ONE_PASS); }

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	virtual void	HandleTextContent(LayoutProperties* layout_props,
									  Text_Box* box,
									  const uni_char* word,
									  int word_length,
									  LayoutCoord word_width,
									  LayoutCoord space_width,
									  LayoutCoord justified_space_extra,
									  const WordInfo& word_info,
									  LayoutCoord x,
									  LayoutCoord virtual_pos,
									  LayoutCoord baseline,
									  LineSegment& segment,
									  LayoutCoord line_height);

	/** Leave vertical box */

	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);
};


#ifdef RESERVED_REGIONS
/** Container for clip rects in ReservedRegionTraversalObject. */

struct ReservedRegionClipRect : public ListElement<ReservedRegionClipRect>
{
	ReservedRegionClipRect(OpRect& rect) : rect(rect) {}
	OpRect rect;
};

/**
  * TraversalObject that determines region where input events will induce select DOM input events.
  *
  * Allows platforms to discover the region of a visible document where input event handlers are
  * attached, so that they may send input events to Core only when required and sacrifice little
  * in the way of response time and usability from the end-user's point of view.
  */

class ReservedRegionTraversalObject : public HitTestingTraversalObject
{
private:
	/** Region to which found rectangles are added. */

	OpRegion&		reserved_region;

	/** Number of table row boxes we are currently inside. */

	int				table_row_ancestor_count;

	/** Clip rectangles of the scrollable containers we have entered (in root coordinates). */

	List<ReservedRegionClipRect> clip_rects;

protected:

	/** Called from inside HitTestingTraversalObject::EnterVerticalBox box, when we should enter it according to HitTestingTraversalObject
	 *  and it is not dry run currently.
	 *  When called, the clip rect (if any) that origins from the current VerticalBox has not been pushed yet.
	 * @param clip_affects_traversed_descendants TRUE when the box introduces clipping (because of overflow hidden or clip property)
	 *		  and affects the descendant(s) that will be traversed to in that run. In other words this param is TRUE iff
	 *		  (there is some clipping) && ((no target element set) || ((target element is set) && (clipping affects the target)))
	 * @return TRUE when we should enter the vertical box according to the subclass, FALSE otherwise. */

	virtual BOOL	HandleVerticalBox(LayoutProperties* parent_lprops,
									  LayoutProperties*& layout_props,
									  VerticalBox* box,
									  TraverseInfo& traverse_info,
									  BOOL clip_affects_traversed_descendants);

	/** Called from inside HitTestingTraversalObject::EnterScrollable.
	 *  When called, the clip rect that origins from the current scrollable has not been pushed yet.
	 *  @return TRUE when we should enter the scrollable, FALSE otherwise. */

	virtual BOOL	HandleScrollable(const HTMLayoutProperties& props,
									 ScrollableArea* scrollable,
									 LayoutCoord width,
									 LayoutCoord height,
									 TraverseInfo& traverse_info,
									 BOOL clip_affects_target,
									 int scrollbar_modifier);

public:
	ReservedRegionTraversalObject(FramesDocument* doc, const RECT& area, OpRegion& region);

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }
	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& position, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);
	virtual BOOL	EnterTableRow(TableRowBox* row);
	virtual void	LeaveTableRow(TableRowBox* row, TableContent* table);

#ifdef PAGED_MEDIA_SUPPORT
	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);
#endif // PAGED_MEDIA_SUPPORT

	/** Enter scrollable content. */

	virtual BOOL	EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave scrollable content. */

	virtual void    LeaveScrollable(TraverseInfo& traverse_info);

	/** Check if 'reserved' events sent to this element will be handled. */

	BOOL			HasReservedEventHandler(HTML_Element* element, BOOL test_ancestors);

	/** Adds a rectangle to reserved region. Limits it to the current clip rect and transforms
	 * to root coordinates (computes a bbox).
	 * @param rect a rectangle to add. It should be in current translation's and transform's coordinates. */

	void			AddRectangle(const OpRect& rect);

#ifdef SVG_SUPPORT
	/** Include reserved rectangles of an SVG element in the reserved region. */

	void			ProcessSVGElement(HTML_Element* element, OpRect& clip_rect);
#endif // SVG_SUPPORT
};
#endif // RESERVED_REGIONS

/** The CoreViewFinder cleans up remaining coreviews on the screen by matching
	currently visible CoreViews with the CoreViews layout expects to be on
	the screen. */

class CoreViewFinder : public VisualTraversalObject
{
private:

	/** Private ctor, because the traverse process is defined inside the static
		CoreViewFinder::TraverseWithFixedPositioned. The params have the same meaning. */

					CoreViewFinder(FramesDocument* doc, const RECT& area, OpPointerHashTable<HTML_Element, CoreView>& core_views, OpPointerSet<HTML_Element>& fixed_position_subtrees)
					  : VisualTraversalObject(doc, area),
						core_views(core_views),
						fixed_position_subtrees(fixed_position_subtrees)
					{
						SetTraverseType(TRAVERSE_ONE_PASS);
					}

	/** The collection of CoreViews to check.

		During the traverse, every encountered CoreView that is intersecting
		the searched area is removed. */

	OpPointerHashTable<HTML_Element, CoreView>&
					core_views;

	/** The collection of fixed positioned subtrees.

		See the param description in TraverseWithFixedPositioned().
		@see TraverseWithFixedPositioned(). */

	OpPointerSet<HTML_Element>&
					fixed_position_subtrees;

public:

	/** Traverse the document in the specified area and removes the encountered
		CoreViews from the hash.

		Uses the special type of traverse, which is a logical order traverse with
		the guarantee to reach all the fixed positioned subtrees that are stored
		in fixed_position_subtrees. Normally we could skip some fixed positioned
		subtrees. That is because fixed positioned elements stop propagating
		bounding boxes and in combination with logical order area traversal of
		this an ancestor of a fixed positioned element could be falsely not entered.

		@param doc The FramesDocument
		@param area The area to check whether the CoreViews are intersecting.
		@param core_views The collection of candidate CoreViews. The ones that
			   do intersect the area will be removed.
		@param fixed_position_subtrees The collection of all the fixed positioned
			   elements that we want to reach during the traverse. It means that
			   if any of the elements in that collection was skipped during the
			   former traverses, it will be traversed to once again starting
			   from its box. The only exception in which some fixed positioned
			   element could be skipped is when the offset from document root of
			   that element can't be determined without and additional traverse.
			   That is because this method is performance critical, so we can't
			   afford extra traverses.
		@return OpStatus::OK or OpStatus::ERR_NO_MEMORY if OOM. */

	static OP_STATUS
					TraverseWithFixedPositioned(FramesDocument* doc,
												const OpRect& area,
												OpPointerHashTable<HTML_Element, CoreView>& core_views,
												OpPointerSet<HTML_Element>& fixed_position_subtrees);

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return TRUE; }

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);
};

#ifdef LAYOUT_WIDTHTRAVERSALOBJECT

/* The WidthTraversalObject tries to find the width of a page. This is used
   for thumbnail generation. */

class WidthTraversalObject
	: public HitTestingTraversalObject
{
private:
	long left, right;

	void HandleWidth(long l, long r)
	{
		if (r <= 0 || l == r) // box deliberately hidden from view
			return;
		if (l < 0) // box gets clipped on the left
			l = 0;

		if (left == right) // first time
		{
			left = l;
			right = r;
		}
		else
		{
			left = MIN(left, l);
			right = MAX(right, r);
		}
	}

protected:

	/** Called from inside HitTestingTraversalObject::EnterScrollable.
	 *  When called, the clip rect that origins from the current scrollable has not been pushed yet.
	 *  @return TRUE when we should enter the scrollable, FALSE otherwise. */

	virtual BOOL	HandleScrollable(const HTMLayoutProperties& props,
									 ScrollableArea* scrollable,
									 LayoutCoord width,
									 LayoutCoord height,
									 TraverseInfo& traverse_info,
									 BOOL clip_affects_target,
									 int scrollbar_modifier);

public:
	WidthTraversalObject(FramesDocument* doc, const RECT& area)
		: HitTestingTraversalObject(doc, area), left(0), right(0) {}

	virtual void StartingTraverse() {HitTestingTraversalObject::StartingTraverse(); left = right = 0; }
	virtual void HandleTextContent(LayoutProperties* layout_props,
								   Text_Box* box,
								   const uni_char* word,
								   int word_length,
								   LayoutCoord word_width,
								   LayoutCoord space_width,
								   LayoutCoord justified_space_extra,
								   const WordInfo& word_info,
								   LayoutCoord x,
								   LayoutCoord virtual_pos,
								   LayoutCoord baseline,
								   LineSegment& segment,
								   LayoutCoord line_height)
	{
		HandleWidth(translation_x + x, translation_x + x + word_width);
	}
	virtual void HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content);
	virtual BOOL EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info);
	virtual void HandleSelectableBox(LayoutProperties* layout_props);

	/** Get the recommended horizontal range (snapped to common screen resolutions).
	    @return FALSE if unable to find a recommended range, TRUE otherwise */

	BOOL GetRecommendedHorizontalRange(long &l, long &r);

	/** Get the horizontal range.
	    @return FALSE if no content was encountered, TRUE otherwise */

	BOOL GetHorizontalRange(long &l, long &r);
};

#endif // LAYOUT_WIDTHTRAVERSALOBJECT

#endif // _TRAVERSE_H_
