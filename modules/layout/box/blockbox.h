/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file blockbox.h
 *
 * Inline box class prototypes for document layout.
 *
 * @author Geir Ivarsøy
 * @author Karl Anders Øygard
 *
 */

#ifndef _BLOCKBOX_H_
#define _BLOCKBOX_H_

#include "modules/layout/content/content.h"
#include "modules/style/css_all_values.h"

/** Block box. */

class BlockBox
  : public VerticalBox,
    public VerticalLayout
{
private:

	/** Set the left and top CSS properties of positioned boxes, since these values need to be cached. */

	virtual void	SetRelativePosition(LayoutCoord new_left, LayoutCoord new_top) {}

protected:

	union
	{
		struct
		{
			/** Can/must we break page before this block? See type BREAK_POLICY. */

			unsigned int
						page_break_before:3;

			/** Can/must we break page after this block? See type BREAK_POLICY. */

			unsigned int
						page_break_after:3;

			/** Can/must we break column before this block? See type BREAK_POLICY. */

			unsigned int
						column_break_before:2;

			/** Can/must we break column after this block? See type BREAK_POLICY. */

			unsigned int
						column_break_after:2;

			/** Has this block specified an absolute width? */

			unsigned int
						has_absolute_width:1;

			/** Has this block a fixed left position? (not percent values for left margin and parent left border and padding) */

			unsigned int
						has_fixed_left:1;

			/** Block box has clearance.

				This member is only used during reflow and can be moved to
			    reflow_state if you need the space. */

			unsigned int
						has_clearance:1;

			/** TRUE if this block spans all columns in a multi-column-container. */

			unsigned int
						column_spanned:1;

			/** TRUE if this block lives in a paged or multicol container. */

			unsigned int
						in_multipane:1;

			/** TRUE if this block is laid out on a line, logically. */

			unsigned int
						on_line:1;

		} packed;
		unsigned long
					packed_init;
	};

	/** X position of box. */

	LayoutCoord		x;

	/** Y position of box. */

	LayoutCoord		y;

	/** Y position if maximum width is satisfied. */

	LayoutCoord		min_y;

	/** Position on the virtual line the box was laid out on.

		Only useful if TraverseInLine() returns TRUE. Used to optimize logical
		line traversal. */

	LayoutCoord		virtual_position;

	/** Invalidate the bounding box during reflow. Assumes that the
		translation/transformation is at the parent. */

	void			InvalidateBoundingBox(LayoutInfo &info, LayoutCoord offset_x, LayoutCoord offset_y) const;

public:

					BlockBox(HTML_Element* element, Content* content);

	virtual			~BlockBox() { Link::Out(); }

	/** Recalculate the top margin after a new block box has been added to a container's layout
		stack. Collapse the margin with preceding adjacent margins if appropriate. If the top
		margin of this block is adjacent to an ancestor's top margin, it may cause the ancestor's
		Y position to change. If the top margin of this block is adjacent to a preceding sibling's
		bottom margin, this block may change its Y position.

		@return TRUE if the Y position of any element was changed. */

	virtual BOOL	RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL has_bottom_margin = FALSE);

	/** Layout of this box is finished (or skipped). Propagate changes (bottom margins,
		bounding-box) to parents. This may grow the box, which in turn may cause its parents to be
		grown. Bottom margins may participate in margin collapsing with successive content, but
		only if this box is part of the normal flow. In that case, propagate the bottom margin to
		the reflow state of the container of this box. Since this member function is used to
		propagate bounding boxes as well, we may need to call it even when box is empty and
		is not part of the normal flow. To protect against margins being propagated and parent
		container reflow position updated in such case, 'has_inflow_content' flag has been introduced
		in order to notify container that this box is not a part of normal flow but has bounding box
		that still needs to be propagated. Separation of bounding box propagation and margin/reflow state
		update should be considered. */

	virtual void	PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin = 0, BOOL has_inflow_content = TRUE);

	/** Expand relevant parent containers and their bounding-boxes to contain floating and
		absolutely positioned boxes. A call to this method is only to be initiated by floating or
		absolutely positioned boxes.

		@param info Layout information structure
		@param bottom Only relevant for floats: The bottom margin edge of the float, relative to
		top border-edge of this box.
		@param min_bottom Only relevant for floats: The bottom margin edge of the float if
		maximum width of its container is satisfied
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Propagate the right edge of absolute positioned boxes. Used for FlexRoot. */

	virtual void	PropagateRightEdge(const LayoutInfo& info, LayoutCoord right, LayoutCoord noncontent, LayoutFixed percentage);

	/** Propagate a break point caused by break properties or a spanned element.

		These are discovered during layout, and propagated to the nearest
		multicol container when appropriate. They are needed by the columnizer
		to do column balancing.

		@param virtual_y Virtual Y position of the break (relative to the top
		border edge of this box)
		@param break_type Type of break point
		@param breakpoint If non-NULL, it will be set to the MultiColBreakpoint
		created, if any.
		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint);

	/** Get width available for the margin box. */

	virtual LayoutCoord
					GetAvailableWidth(LayoutProperties* cascade);

	/** Set X position of block box. */

	void			SetX(LayoutCoord new_x) { x = new_x; }

	/** Set Y position of block box. */

	virtual void	SetY(LayoutCoord new_y) { y = new_y; }

	/** Get box X position, relative to parent. */

	LayoutCoord		GetX() const { return x; }

	/** Get box Y position, relative to parent. */

	virtual LayoutCoord
					GetStackPosition() const { return y; }

	/** Get box Y position, relative to parent. */

	LayoutCoord		GetY() const { return y; }

	/** Get and add the normal flow position of this box to the supplied coordinate variables. */

	virtual void	AddNormalFlowPosition(LayoutCoord &x, LayoutCoord &y) const { x += this->x; y += this->y; }

	/** Get height of content. */

	virtual LayoutCoord
					GetLayoutHeight() const;

	/** Get height if maximum width is satisfied. */

	virtual LayoutCoord
					GetLayoutMinHeight() const;

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const;

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Is this box a block box? */

	virtual BOOL	IsBlockBox() const { return TRUE; }

	/** Is this box (and potentially also its children) columnizable?

		A box may only be columnizable if it lives in a paged or multicol container.

		A descendant of a box that isn't columnizable can never be columnizable (unless
		there's a new paged or multicol container between that box and its descendant).

		Table-rows, table-captions, table-row-groups and block-level descendant boxes of
		a paged or multicol container are typically columnizable. Being columnizable
		means that they need to be taken into account by the ancestor paged or multicol
		container somehow. They may become a start or stop element of a column or
		page. It may also be possible to split them over multiple columns or pages, but
		that depends on details about the box type, and that is where the
		"require_children_columnizable" parameter comes into play.

		@param require_children_columnizable If TRUE, only return TRUE if this box is of
		such a type that not only the box itself is columnizable, but also that it
		doesn't prevent its children from becoming columnizable. A table-row, block with
		scrollbars, absolutely-positioned box or float prevents children from being
		columnizable (FALSE will be returned it this parameter is TRUE), but the box
		itself may be very well be columnizable. Example: an absolutely positioned box
		may live inside of a paged or multicol container (so that its Y position is
		stored as a virtual Y position within the paged or multicol container), but its
		descendants do not have to worry about the multi-columnness or multi-pagedness.

		@return TRUE if this box (and possibly its children too) is columnizable. Return
		FALSE if this box isn't columnizable, or if require_children_columnizable is set
		and the children aren't columnizable. */

	virtual BOOL	IsColumnizable(BOOL require_children_columnizable = FALSE) const;

	/** Specify whether this block logically sits on a line or not.

		Even if it is considered to sit "on a line", it still gets an entry in the
		vertical layout stack, as long as it's not absolutely positioned. If this is
		a float, it is placed before the line. Otherwise (if it's in-flow), it's
		placed after the line (as usual). */

	void			SetOnLine(BOOL on_line) { packed.on_line = !!on_line; }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip) {}

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return FALSE; }

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Get position on the virtual line the box was laid out on. */

	virtual LayoutCoord
					GetVirtualPosition() const { return virtual_position; }

	/** Set position on the virtual line the box was laid out on. */

	void			SetVirtualPosition(LayoutCoord x) { virtual_position = x; }

	/** Is this box traversed within a line? */

	virtual BOOL	TraverseInLine() const { return !!packed.on_line; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Insert a page break. */

	virtual BREAKST	InsertPageBreak(LayoutInfo& info, int strength);

	/** Attempt to break page. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength);

#endif // PAGED_MEDIA_SUPPORT

	/** Get baseline of vertical layout.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const;

	/** Propagate widths to container / table. */

	virtual void	PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Is this a block? */

	virtual BOOL	IsBlock() const { return TRUE; }

	/** Is this an element that logically belongs in the stack? */

	virtual BOOL	IsInStack(BOOL include_floats = FALSE) const { return TRUE; }

	/** Calculate bottom margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param parent_cascade parent properties
	 * @param info Layout information structure
	 * @param bottom_margin Margin state
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateBottomMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* bottom_margin) const;

	/** Calculate top margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param parent_cascade parent properties
	 * @param info Layout information structure
	 * @param top_margin Margin state
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateTopMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* top_margin) const;

	/** Set page and column breaking policies on this block box. */

	void			SetBreakPolicies(BREAK_POLICY page_break_before, BREAK_POLICY column_break_before,
									 BREAK_POLICY page_break_after, BREAK_POLICY column_break_after);

#ifdef PAGED_MEDIA_SUPPORT

	/** Get page break policy before this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyBefore() const { return (BREAK_POLICY) packed.page_break_before; }

	/** Get page break policy after this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyAfter() const { return (BREAK_POLICY) packed.page_break_after; }

#endif // PAGED_MEDIA_SUPPORT

	/** Get column break policy before this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyBefore() const { return (BREAK_POLICY) packed.column_break_before; }

	/** Get column break policy after this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyAfter() const { return (BREAK_POLICY) packed.column_break_after; }

	/** Is this a True Table candidate? */

	virtual TRUE_TABLE_CANDIDATE
					IsTrueTableCandidate() const { return content->IsTrueTableCandidate(); }

	/** Has this box specified an absolute width? */

	virtual BOOL	HasAbsoluteWidth() const { return packed.has_absolute_width; }

	/** Has this block a fixed left position? (not percent values for left margin and parent left border and padding) */

	BOOL			HasFixedLeft() const { return packed.has_fixed_left; }

	/** Set that this block has a fixed left position. (not percent values for left margin and parent left border and padding) */

	void			SetHasFixedLeft() { packed.has_fixed_left = 1; }

	/** Set that this block has clearance (clear had effect on position). */

	void			SetHasClearance() { packed.has_clearance = TRUE; }

	/**
	 * Move this VerticalLayout and all its content down by an offset.
	 *
	 * @param offset_y Offset to move down
	 * @param containing_element. Element of the container for this vertical layout. Not used in this implementation.
	 */

	virtual void	MoveDown(LayoutCoord offset_y, HTML_Element* containing_element) { y += offset_y; }

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container);

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf, Container* container);

	/** Set minimum Y position.

		This is the position this block would get if maximum width of its
		shrink-to-fit block formatting context is satisfied). */

	void			SetMinY(LayoutCoord y) { min_y = y; }

	/** Get minimum Y position.

		This is the position this block would get if maximum width of its
		shrink-to-fit block formatting context is satisfied). */

	LayoutCoord		GetMinY() const { return min_y; }

	/** Has this box clearance applied */

	BOOL			HasClearance() const { return packed.has_clearance; }

	/** Return TRUE if this block spans all columns in a multi-column-container. */

	BOOL			IsColumnSpanned() const { return packed.column_spanned == 1; }

	/** Return TRUE if this block is inside of a paged or multicol container. */

	BOOL			IsInMultiPaneContainer() const { return packed.in_multipane == 1; }

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc, BoxSignalReason reason = BOX_SIGNAL_REASON_UNKNOWN);

	/** Is this an empty list item that has a marker? */

	BOOL			IsEmptyListItemWithMarker() const { return HasListMarkerBox() && content->IsEmpty(); }
};

/** Vertical box reflow state. */

class AbsolutePositionedBoxReflowState
  : public VerticalBoxReflowState
{
public:

	/** Previous x translation. */

	LayoutCoord		previous_x_translation;

	/** Previous y translation. */

	LayoutCoord		previous_y_translation;

	/** The box that establishes the containing block of this box. */

	Box*			containing_box;

	/** Width of containing block. */

	LayoutCoord		containing_block_width;

	/** Width available for this box.

		This is set if width is non-fixed. */

	LayoutCoord		available_width;

	/** Static position within the container.

		This is set when position is auto. It represents the distance from the
		container's border edge to the margin edge of this box (for LTR, the
		edges will be the left edge, and for RTL, the edges will be the right
		edge).  */

	LayoutCoord		static_position;

	/** Left border of containing block. */

	short			containing_border_left;

	/** horizontal position was static before reflow. */

	BOOL			x_was_static;

	/** vertical position was static before reflow. */

	BOOL			y_was_static;

					AbsolutePositionedBoxReflowState()
					  : previous_x_translation(0),
						previous_y_translation(0),
						containing_box(NULL),
						static_position(0),
						containing_border_left(0),
						x_was_static(FALSE),
						y_was_static(FALSE) {}

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_absolutebox_reflow_state_pool->New(sizeof(AbsolutePositionedBoxReflowState)); }
	void			operator delete(void* p, size_t nbytes) { g_absolutebox_reflow_state_pool->Delete(p); }
};

class FLink;
class FloatingBox;

/** List of floats. Either belongs to a SpaceManager or to a temporary list used during reflow. */

class FloatList : public Head
{
public:
	FLink*			First() const { return (FLink*) Head::First(); }
	FLink*			Last() const { return (FLink*) Head::Last(); }

	/** Force full reflow of this block formatting context. */

	virtual void	ForceFullBfcReflow() = 0;
};

/** Reflow state for a SpaceManager.

	Holds a list of floats that existed in this block formatting context prior
	to reflow. They will be moved back into the SpaceManager as they are
	encountered (or skipped) during reflow (unless they are deleted). */

class SpaceManagerReflowState : public FloatList
{
public:

	/** TRUE if all blocks in this block formatting context need to be reflowed (because of floats). */

	BOOL			full_bfc_reflow;

	/** TRUE if all blocks in this block formatting context have to recalculate their min/max widths (because of floats). */

	BOOL			full_bfc_min_max_calculation;

					SpaceManagerReflowState()
						: full_bfc_reflow(FALSE),
						  full_bfc_min_max_calculation(FALSE) {}

					~SpaceManagerReflowState();

	/** Force full reflow of this block formatting context. */

	virtual void	ForceFullBfcReflow() { full_bfc_reflow = full_bfc_min_max_calculation = TRUE; }

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_spacemanager_reflow_state_pool->New(sizeof(SpaceManagerReflowState)); }
	void			operator delete(void* p, size_t nbytes) { g_spacemanager_reflow_state_pool->Delete(p); }
};

/** Space manager.

	The space manager keeps track of floats, etc. and gives out space for
	the block boxes to lay out lines and boxes into. Each block formatting
	context root has its own space manager. */

class SpaceManager : public FloatList
{
private:

	/** Reflow state. Present while the box to which this SpaceManager belongs is reflowed. */

	SpaceManagerReflowState*
					reflow_state;

	union
	{
		struct
		{
			/** Needs full reflow of this block formatting context. */

			unsigned int
					full_bfc_reflow:1;

		} spaceman_packed;
		unsigned long
					spaceman_packed_init;
	};

public:

					SpaceManager()
						: reflow_state(NULL) { spaceman_packed_init = 0; }

					~SpaceManager();

	/** Restart space manager - prepare it for reflow.

		@return FALSE on memory allocation failure. */

	BOOL			Restart();

	/** Reflow of the box associated with this space manager is finished. */

	void			FinishLayout();

	/** Force full reflow of this block formatting context. */

	virtual void	ForceFullBfcReflow() { spaceman_packed.full_bfc_reflow = 1; }

	/** Propagate bottom margins of child floats of element. */

	void			PropagateBottomMargins(LayoutInfo& info, HTML_Element* element, LayoutCoord y_offset, LayoutCoord min_y_offset);

	/** Skip over an element. */

	void			SkipElement(LayoutInfo& info, HTML_Element* element);

	/** Add floating block box to space manager.

		After a float has been added to the space manager, it won't give
	    out space that would intersect with that float. */

	void			AddFloat(FloatingBox* floating_box);

	/** Get new space from space manager.

		The space manager will find a suitable position for content defined by
		the rectangle (bfc_x, bfc_y, min_width, min_height) that does not
		overlap with any of the floats' (in this block formatting context)
		margin boxes.

		It may not always be possible to satisfy the space requirements (if the
		containing block is too narrow). However, no matter what, the position
		returned (bfc_x, bfc_y) will never overlap with the margin edge of any
		floats in this block formatting context.

		@param bfc_y (in/out) Relative to the top border edge of the block
		formatting context. In: highest possible Y position for the
		element. Out: The in-value will be increased if that is needed to
		satisfy the space requirements. If it is below all the floats, it will
		not be increased any further (even if the space requirement cannot be
		satisfied at this position).

		@param bfc_x (in/out) Relative to the left border edge of the block
		formatting context. In: leftmost possible X position for the
		element. Out: Will be changed if putting an element at that position
		would overlap with any floats.

		@param width (in/out) In: containing block width (ensures that space
		handed out is confined to the containing block). Out: width available
		at the final position.

		@param min_width Minimum width requirement

		@param min_height Minimum height requirement

		@param lock_vertical If TRUE, the vertical position (@see y) will
		remain untouched, even if that would mean that the layout element will
		overlap with floats. The horizontal position may be affected by floats,
		though.

		@return How much vertical space is available at the final position. */

	LayoutCoord		GetSpace(LayoutCoord& bfc_y, LayoutCoord& bfc_x, LayoutCoord& width, LayoutCoord min_width, LayoutCoord min_height, VerticalLock vertical_lock) const;

	/** Get maximum width of all floats at the specified minimum Y position, and change minimum Y if necessary.

		This method is similar to GetSpace(), just that it is only used when
		calculating min/max widths. It will pretend that maximum widths are
		satisfied and try to find space among the floats for content at the Y
		position bfc_min_y with height min_height.

		The caller suggests a Y position for some content and wants to know the
		maximum widths of floats that are beside the content. This method may
		change the Y position if it finds that there is no way the content
		would fit there, and then provide the maximum width of floats to the
		left and to the right of the content at its potentially new position.

		@param bfc_min_y (in/out) Minimum Y position (the position that content
		would get if the maximum width of its container is satisfied), relative
		to the top border edge of the block formatting context. In: Highest
		(suggested) minimum Y position for the content. Out: Same or lower
		minimum Y position where there might actually be room for the content.

		@param min_height Minimum height - height that the content would get if
		the maximum width of its container is satisfied.

		@param max_width Maximum width of content (the width that the content
		would need in order to avoid implicit line breaks and floats being
		shifted downwards)

		@param bfc_left_edge_min_distance Minimum distance from left content
		edge of the content to the left content edge of the block formatting
		context. Does not include percentual padding and margin. May be
		negative (due to negative margins).

		@param bfc_right_edge_min_distance Minimum distance from right content
		edge of the content to the right content edge of the block formatting
		context. Does not include percentual padding and margin. May be
		negative (due to negative margins).

		@param container_width Width of the container of the content. If this
		is unknown (because of auto/percentual widths), it will be
		LONG_MAX.

		@param left_floats_max_width (out) The maximum width by which floats to
		the left of the content may possibly overlap the content's container.

		@param right_floats_max_width (out) The maximum width by which floats
		to the right of the content may possibly overlap the content's
		container. */

	void			GetFloatsMaxWidth(LayoutCoord& bfc_min_y, LayoutCoord min_height, LayoutCoord max_width, LayoutCoord bfc_left_edge_min_distance, LayoutCoord bfc_right_edge_min_distance, LayoutCoord container_width, LayoutCoord& left_floats_max_width, LayoutCoord& right_floats_max_width);

	/** Find bottom of floats.

		Returns the first possible Y position below all floats of the given
		type (left, right or both), relative to the top content edge of the
		block formatting context. If no relevant floats were found, LONG_MIN is
		returned.

		@param clear Which floats to consider; left, right or both. */

	LayoutCoord		FindBfcBottom(CSSValue clear) const;

	/** Find minimum bottom of floats.

		Returns the first possible minimum Y position (the hypothetic Y
		position if the maximum width of a shrink-to-fit block formatting
		context is satisfied) below all floats of the given type (left, right
		or both), relative to the top content edge of the block formatting
		context. If no relevant floats were found, LAYOUT_COORD_MIN is returned.

		@param clear Which floats to consider; left, right or both. */

	LayoutCoord		FindBfcMinBottom(CSSValue clear) const;

	/** Get the last float in this block formatting context. */

	FloatingBox*	GetLastFloat() const;

	/** Any pending floats? */

	BOOL			HasPendingFloats() const { return reflow_state && !reflow_state->Empty(); }

	/** Enable full reflow.

		Specify that all blocks in this block formatting context need to be reflowed. */

	void			EnableFullBfcReflow() { reflow_state->full_bfc_reflow = TRUE; }

	/** Is full reflow enabled? */

	BOOL			IsFullBfcReflowEnabled() const { return reflow_state->full_bfc_reflow; }

	/** Enable full min/max width calculation.

		Specify that all blocks in this block formatting context need to recalculate min/max widths. */

	void			EnableFullBfcMinMaxCalculation() { reflow_state->full_bfc_min_max_calculation = TRUE; }

	/** Is full min/max width calculation enabled? */

	BOOL			IsFullBfcMinMaxCalculationEnabled() const { return reflow_state->full_bfc_min_max_calculation; }
};

/** Block box with space manager.

    Used by any statically-positioned block box type that establishes a new
    block formatting context (e.g. if it has overflow: auto/scroll). */

class SpaceManagerBlockBox
  : public BlockBox
{
private:

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

public:

					SpaceManagerBlockBox(HTML_Element* element, Content* content)
					  : BlockBox(element, content) {}

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }
};

#define NO_HOFFSET LAYOUT_COORD_MIN
#define NO_VOFFSET LAYOUT_COORD_MIN

/** Absolute positioned box. */

class AbsolutePositionedBox
  : public BlockBox
{
private:

	/** Z element for stacking context. */

	ZElement		z_element;

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

	/** Initialise reflow state. */

	AbsolutePositionedBoxReflowState*
					InitialiseReflowState();

	union
	{
		struct
		{
			/** Right aligned. */

			unsigned int
					right_aligned:1;

			/** Cached right border of containing box. */

			unsigned int
					cached_right_border:14;

			/** Bottom aligned. */

			unsigned int
					bottom_aligned:1;

			/** Cached bottom border of containing box. */

			unsigned int
					cached_bottom_border:15;

			/** Is fixed. */

			unsigned int
					fixed:1;

		} abs_packed;
		unsigned long
					abs_packed_init;
	};

	union
	{
		struct
		{
			/** Fixed left position and margin. */

			unsigned int
					fixed_left:1;

			/** Does the width of the containing block affect the layout of
				this box? */

			unsigned int
					containing_block_width_affects_layout:1;

			/** Does the height of the containing block affect the layout of
				this box? */

			unsigned int
					containing_block_height_affects_layout:1;

			/** Does the containing block affect the position of this box? */

			unsigned int
					containing_block_affects_position:1;

			/** Is the containing block established by a positioned inline
				box? */

			unsigned int
					containing_block_is_inline:1;

			/** Does this box have a pending reflow because its containing
				block has changed in a way that affects it? */

			unsigned int
					pending_reflow_check:1;

			/** This would have been an inline, hadn't the position property
				forced the display property to change to block. Its horizontal
				position is hypothetical-static (left:auto, right:auto). */

			unsigned int
					horizontal_static_inline:1;

			/** Is inside transformed element.  Note: If the element
				itself is transformed, this is not necessarily true,
				only if it has an transformed ancestor. */

			unsigned int
					inside_transformed:1;


		} abs_packed2;
		unsigned int
					abs_packed2_init;
	};

	/** Horizontal offset. */

	LayoutCoord		offset_horizontal;

	/** Vertical offset. */

	LayoutCoord		offset_vertical;

	/** Minimum width of containing block required to prevent this box from overflowing.

		Used for FlexRoot. If the value is negative, it signifies the
		percentage width of this box. */

	LayoutCoord		flexroot_width_required;

	/** Width of horizontal border and padding if box-sizing is content-box. */

	LayoutCoord		horizontal_border_padding;

	/** Clipping rectangle - this caches value of CSS 'clip' property.

		Caching is necessary as we may need to clip an abspos bbox during SkipBranch() call
		in which cascade and thus CSS properties will not be available */

	RECT			clip_rect;

	/** Get the height of the initial containing block. In normal
		cases this is the height of the layout viewport.

		Content sized iframes is an exception since the height of the
		view can depend on the size of the frame, causing a dual
		dependency. Thus, the height of the containing block is set to
		zero for content sized iframes. */

	LayoutCoord		InitialContainingBlockHeight(LayoutInfo& info) const;

	/** Calculate width available for this box. */

	void			CalculateAvailableWidth(const LayoutInfo& info);

	/** Calculate X position of box. The width may be needed to calculate this. */

	void			CalculateHorizontalPosition();

	/** Set clipping rectangle */

	void			SetClipRect(LayoutCoord left, LayoutCoord top, LayoutCoord right, LayoutCoord bottom) { clip_rect.left = left; clip_rect.top = top; clip_rect.right = right; clip_rect.bottom = bottom; }

public:

					AbsolutePositionedBox(HTML_Element* element, Content* content)
					  : BlockBox(element, content),
					    z_element(element),
						offset_horizontal(0),
						offset_vertical(0),
						flexroot_width_required(0),
						horizontal_border_padding(0)
					{
						abs_packed_init = 0;
						abs_packed2_init = 0;
						SetClipRect(CLIP_NOT_SET, CLIP_NOT_SET, CLIP_NOT_SET, CLIP_NOT_SET);
					}

	/** Get reflow state. */

	AbsolutePositionedBoxReflowState*
					GetReflowState() const { return (AbsolutePositionedBoxReflowState*) BlockBox::GetReflowState(); }

	/** Get width available for the margin box. */

	virtual LayoutCoord
					GetAvailableWidth(LayoutProperties* cascade);

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Is this box an absolute positioned box? */

	virtual BOOL	IsAbsolutePositionedBox() const { return TRUE; }

	/** Is this an element that logically belongs in the stack? */

	virtual BOOL	IsInStack(BOOL include_floats = FALSE) const { return FALSE; }

	/** Is this box a fixed positioned box?

		Normally fixed positioned boxes inside transforms isn't
		included here, because they are not fixed to the viewport.
		Instead they are more similar to position:absolute with the
		exception that they have the nearest transformed element as
		containing block, instead of the nearest positioned element.

		If 'include_transformed' is TRUE, fixed positioned boxes
		inside transformed elements are also included. */

	virtual BOOL	IsFixedPositionedBox(BOOL include_transformed = FALSE) const { return abs_packed.fixed
#ifdef CSS_TRANSFORMS
			&& (include_transformed || !abs_packed2.inside_transformed)
#endif
			;
	}

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const;

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc, BoxSignalReason reason = BOX_SIGNAL_REASON_UNKNOWN);

	/** Recalculate the top margin after a new block box has been added to a container's layout
		stack. Collapse the margin with preceding adjacent margins if appropriate. If the top
		margin of this block is adjacent to an ancestor's top margin, it may cause the ancestor's
		Y position to change. If the top margin of this block is adjacent to a preceding sibling's
		bottom margin, this block may change its Y position.

		@return TRUE if the Y position of any element was changed. */

	virtual BOOL	RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL has_bottom_margin = FALSE) { return FALSE; }

	/** Layout of this box is finished (or skipped). Propagate changes (bottom margins,
		bounding-box) to parents. This may grow the box, which in turn may cause its parents to be
		grown. Bottom margins may participate in margin collapsing with successive content, but
		only if this box is part of the normal flow. In that case, propagate the bottom margin to
		the reflow state of the container of this box. Since this member function is used to
		propagate bounding boxes as well, we may need to call it even when box is empty and
		is not part of the normal flow. To protect against margins being propagated and parent
		container reflow position updated in such case, 'has_inflow_content' flag has been introduced
		in order to notify container that this box is not a part of normal flow but has bounding box
		that still needs to be propagated. Separation of bounding box propagation and margin/reflow state
		update should be considered. */

	virtual void	PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin = 0, BOOL has_inflow_content = TRUE);

	/** Get bounding box relative to top/left border edge of this box. Overflow may include overflowing content as well as
	absolutely positioned descendants. Returned box may be clipped to a rect defined by CSS 'clip' property. */

	virtual void	GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow = TRUE, BOOL adjust_for_multicol = FALSE, BOOL apply_clip = TRUE) const;

	/** Propagate a break point caused by break properties or a spanned element.

		These are discovered during layout, and propagated to the nearest
		multicol container when appropriate. They are needed by the columnizer
		to do column balancing.

		@param virtual_y Virtual Y position of the break (relative to the top
		border edge of this box)
		@param break_type Type of break point
		@param breakpoint If non-NULL, it will be set to the MultiColBreakpoint
		created, if any.
		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint);

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

#ifdef PAGED_MEDIA_SUPPORT

	/** Insert a page break. */

	virtual BREAKST	InsertPageBreak(LayoutInfo& info, int strength) { return BREAK_NOT_FOUND; }

#endif // PAGED_MEDIA_SUPPORT

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }

	/** Do we need to calculate min/max widths of this box's content? */

	virtual BOOL	NeedMinMaxWidthCalculation(LayoutProperties* cascade) const;

	/** Propagate widths to container / table. */

	virtual void	PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width) {}

	/** Get height and top/bottom border of the containing box.
	 *
	 * @param info the info
	 * @param container The container for the element whose containing box's height we are requesting
	 * @param containing_border_top (out) the top border width of the containing box
	 * @param containing_border_bottom (out) the bottom border width of the containing box
	 * @param current_page_is_containing_block (out) returns TRUE if this box should be positioned
	 *												 relative to the current page. Used in opera show
	 *												 if the containing element is the document root.
	 *
	 * @return the height of the containing box
	 */

	LayoutCoord		GetContainingBoxHeightAndBorders(LayoutInfo& info, Container* container, short& containing_border_top, short& containing_border_bottom, BOOL& current_page_is_containing_block) const;

	/** Set the initial Y position of this absolute positioned box.
	 * This may be updated later through AbsolutePositionedBox::UpdatePosition
	 * It will also set the root translation to incorporate the new y value. */

	void			CalculateVerticalPosition(LayoutProperties* cascade, LayoutInfo& info);

	/** Calculate used vertical CSS properties (height and margins).

		Resolve the actual content height of this absolute positioned box,
		based on constraints from variables such as min/max height, height, top
		& bottom. Also calculate vertical margins.

		Note! This function should avoid setting state, since it is currently
		called from different places.

		@return always TRUE, meaning that height and vertical margins have been
		resolved by this method, so that we shouldn't calculate them in the
		default manner. */

	virtual BOOL	ResolveHeightAndVerticalMargins(LayoutProperties* cascade, LayoutCoord& content_height, LayoutCoord& margin_top, LayoutCoord& margin_bottom, LayoutInfo& info) const;

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Element was skipped, reposition. */

	virtual BOOL	SkipZElement(LayoutInfo& info);

	/** Update bottom aligned absolutepositioned boxes. */

	virtual void	UpdateBottomAligned(LayoutInfo& info);

	/** Update the position. The Y position depends on the box height
		if it is bottom-aligned.  When this function is called the
		root translation must be at current position of the absolute
		positioned box.

		@param info Layout information structure
		@param translate When TRUE, the normal translation and the
		root translation is updated to the new box position.  Note: the
		normal translation must also be at the absolute positioned
		box for this to be meaningful. */

	void			UpdatePosition(LayoutInfo &info, BOOL translate = FALSE);

	/** Get horizontal offset. */

	LayoutCoord		GetHorizontalOffset() const { return offset_horizontal; }

	/** Get vertical offset. */

	long			GetVerticalOffset() const { return offset_vertical; }

	/** Find the normal right edge of the rightmost absolute positioned box. */

	virtual LayoutCoord
					FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade);

	/** Is this box right aligned? */

	BOOL			IsRightAligned() const { return abs_packed.right_aligned == 1; }

	/** Has this box fixed left position and margin? */

	BOOL			HasFixedPosition() const { return abs_packed2.fixed_left == 1; }

	/** The size or position of the containing block has changed, and this
	 * absolutely positioned box needs to know (and probably reflow).
	 * @param info Layout information structure
	 * @param width_changed Was the containing block width changed?
	 * @param height_changed Was the containing block height changed?
	 */

	void			ContainingBlockChanged(LayoutInfo &info, BOOL width_changed, BOOL height_changed);

	/** Check if this box is affected by the size or position of its containing
		block. */

	BOOL			CheckAffectedByContainingBlock(LayoutInfo &info, Box* containing_block, BOOL skipped);

	/** If the position property hadn't forced the display property to block,
		would this have been an inline, and is the horizontal position
		hypothetical-static (left:auto, right:auto)? */

	BOOL			IsHypotheticalHorizontalStaticInline() { return abs_packed2.horizontal_static_inline == 1; }

	/**
	 * Move this VerticalLayout and all its content down by an offset.
	 *
	 * @param offset_y Offset to move down
	 * @param containing_element. Element of the container for this vertical layout. Not used in this implementation.
	 */
	virtual void	MoveDown(LayoutCoord offset_y, HTML_Element* containing_element) { if (GetVerticalOffset() == NO_VOFFSET) { y += offset_y; } }

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container);

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf, Container* container);

	/** Calculate the translations for normal translation and root translation that
		AbsolutePositionedBox::Traverse must translate before traversing its children.

		@param traversal_object The TraversalObject used for the partial traverse.
		@param container_props The computed styles for the closest container of
							   this absolute positioned element. This is needed
							   to adjust the hypothetical static inline position of
							   this box with text-align offsets.
		@param cancel_x_scroll The amount of horizontal scroll translation between this
							   absolute positioned box and its containing block that needs
							   to be cancelled out because it shouldn't affect the offset
							   for this absolute positioned box.
		@param cancel_y_scroll The amount of vertical scroll translation between this
							   absolute positioned box and its containing block that needs
							   to be cancelled out because it shouldn't affect the offset
							   for this absolute positioned box.
		@param translate_x The returned horizontal translation.
		@param translate_y The returned vertical translation.
		@param translate_root_x The returned horizontal root translation.
		@param translate_root_y The returned vertical root translation. */

	void			GetOffsetTranslations(TraversalObject* traversal_object,
										  const HTMLayoutProperties& container_props,
										  LayoutCoord cancel_x_scroll,
										  LayoutCoord cancel_y_scroll,
										  LayoutCoord& translate_x,
										  LayoutCoord& translate_y,
										  LayoutCoord& translate_root_x,
										  LayoutCoord& translate_root_y);

	/** Return TRUE if this absolutely positioned box is affected by its containing block in any way. */

	BOOL			IsAffectedByContainingBlock() const { return abs_packed2.containing_block_width_affects_layout || abs_packed2.containing_block_height_affects_layout || abs_packed2.containing_block_affects_position; }
};

/** Absolute positioned z indexed box. */

class AbsoluteZRootBox
  : public AbsolutePositionedBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					AbsoluteZRootBox(HTML_Element* element, Content* content)
					  : AbsolutePositionedBox(element, content) {}

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return stacking_context.HasNegativeZChildren(); }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Find the normal right edge of the rightmost absolute positioned box. */

	virtual LayoutCoord
					FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade);

	/** Should TraversalObject let Box handle clipping/overflow on its own?

		Some boxes, depending on traversed content, may need to differentiate
		between clipping and overflow. Clipping rectangle should include
		overflow (even if overflow is hidden) for some descendants (ie. objects
		on StackingContext) therefore clipping must be applied on-demand during
		traverse rather than in Enter*Box. */

	virtual BOOL	HasComplexOverflowClipping() const { return TRUE; }
};

/** Information needed while the space manager in which a floating box lives is reflowed.

	A reflow cache is created for a floating box when it is
	created and laid out, or when reflow of its space manager is
	started, whichever comes first. It is deleted when reflow of
	its space manager is done. */

struct FloatReflowCache
{
	/** Cached lowest bottom margin edge of all left-floats so far in the block
		formatting context (including this float), relative the top border edge
		of the block formatting context. Offset caused by position:relative is
		not included. */

	LayoutCoord		left_floats_bottom;

	/** Cached lowest bottom margin edge of all right-floats so far in the
		block formatting context (including this float), relative the top
		border edge of the block formatting context. Offset caused by
		position:relative is not included. */

	LayoutCoord		right_floats_bottom;

	/** Cached lowest minimum bottom margin edge of all left-floats so far in
		the block formatting context (including this float), relative to the
		top border edge of the block formatting context. Offset caused by
		position:relative is not included. */

	LayoutCoord		left_floats_min_bottom;

	/** Cached lowest minimum bottom margin edge of all right-floats so far in
		the block formatting context (including this float), relative to the
		top border edge of the block formatting context. Offset caused by
		position:relative is not included. */

	LayoutCoord		right_floats_min_bottom;

	/** Distance from the top border edge of the block formatting context (to
		which this float belongs) to the top margin edge of this float. Offset
		caused by position:relative is not included. */

	LayoutCoord		bfc_y;

	/** Distance from the minimum top border edge of the block formatting
		context (to which this float belongs) to the minimum top margin edge of
		this float. Offset caused by position:relative is not included. */

	LayoutCoord		bfc_min_y;

	/** Distance from the left border edge to the block formatting context (to
		which this float belongs) to the left margin edge of this float. Offset
		caused by position:relative is not included. */

	LayoutCoord		bfc_x;

	/** margin-left property. */

	LayoutCoord		margin_left;

	/** margin-right property. */

	LayoutCoord		margin_right;

	/** Highest accumulated maximum width of all left floats so far in the
		block formatting context (including this float). */

	LayoutCoord		highest_left_acc_max_width;

	/** Highest accumulated maximum width of all right floats so far in the
		block formatting context (including this float). */

	LayoutCoord		highest_right_acc_max_width;

	/** Next float to traverse.

		Traversal while the space manager is being reflowed may
		happen during incremental rendering. */

	FloatingBox*	next_float;

	/** If TRUE, the cache is invalid / dirty and needs an update before being accessed. */

	BOOL			invalid;

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_float_reflow_cache_pool->New(sizeof(FloatReflowCache)); }
	void			operator delete(void* p, size_t nbytes) { g_float_reflow_cache_pool->Delete(p); }
};

class FLink
  : public Link
{
public:
	FLink*			Suc() { return (FLink*) Link::Suc(); }
	FLink*			Pred() { return (FLink*) Link::Pred(); }

	/** Force full reflow of the block formatting context to which this float belongs. */

	void			ForceFullBfcReflow() { if (FloatList* float_list = (FloatList*) GetList()) float_list->ForceFullBfcReflow(); }

	FloatingBox*	float_box;
};

/** Floating box. */

class FloatingBox
  : public BlockBox
{
private:

	/** float_packed.has_float_reflow_cache determines which object is active in this union. */

	union
	{
		/** Will be set while reflowing the space manager of this box.

			float_packed.has_float_reflow_cache is set when this object is active. */

		FloatReflowCache*
					float_reflow_cache;

		/** Next float to traverse.

			float_packed.has_float_reflow_cache is not set when this object is active. */

		FloatingBox*
					next_float;
	};

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

	/** Top margin of floating box. */

	LayoutCoord		margin_top;

	/** Bottom margin of floating box. */

	LayoutCoord		margin_bottom;

	/** Horizontal margin not resolved from percentual values. */

	LayoutCoord		nonpercent_horizontal_margin;

	/** Total maximum width of all floats to the left (if this is a left-float)
		or to the right (if this is a right-float) of this float, if the block
		formatting context's maximum width is satisfied. */

	LayoutCoord		prev_accumulated_max_width;

	/** Left or right edge of float. */

	LayoutCoord		float_edge;

	union
	{
		struct
		{
			/** Float is left aligned float? */

			unsigned int
					left:1;

			/** Do we have a FloatReflowCache? */

			unsigned int
					has_float_reflow_cache:1;

			/** Is margin-top resolved from a percentual value? */

			unsigned int
					margin_top_is_percent:1;

			/** Is margin-bottom resolved from a percentual value? */

			unsigned int
					margin_bottom_is_percent:1;
		} float_packed;
		unsigned long
					float_packed_init;
	};

	FloatReflowCache*
					GetFloatReflowCache() const { OP_ASSERT(float_packed.has_float_reflow_cache); return float_reflow_cache; }

public:

	FLink			link;

					FloatingBox(HTML_Element* element, Content* content)
					  : BlockBox(element, content),
						margin_top(0),
						margin_bottom(0),
						nonpercent_horizontal_margin(0),
						prev_accumulated_max_width(0) { float_packed_init = 0; link.float_box = this; }

	virtual			~FloatingBox();

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Get the left and top relative offsets caused by position:relative. */

	virtual void	GetRelativeOffsets(LayoutCoord& left, LayoutCoord& top) const { left = LayoutCoord(0); top = LayoutCoord(0); }

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Recalculate the top margin after a new block box has been added to a container's layout
		stack. Collapse the margin with preceding adjacent margins if appropriate. If the top
		margin of this block is adjacent to an ancestor's top margin, it may cause the ancestor's
		Y position to change. If the top margin of this block is adjacent to a preceding sibling's
		bottom margin, this block may change its Y position.

		@return TRUE if the Y position of any element was changed. */

	virtual BOOL	RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL has_bottom_margin = FALSE) { return FALSE; }

	/** Layout of this box is finished (or skipped). Propagate changes (bottom margins,
		bounding-box) to parents. This may grow the box, which in turn may cause its parents to be
		grown. Bottom margins may participate in margin collapsing with successive content, but
		only if this box is part of the normal flow. In that case, propagate the bottom margin to
		the reflow state of the container of this box. Since this member function is used to
		propagate bounding boxes as well, we may need to call it even when box is empty and
		is not part of the normal flow. To protect against margins being propagated and parent
		container reflow position updated in such case, 'has_inflow_content' flag has been introduced
		in order to notify container that this box is not a part of normal flow but has bounding box
		that still needs to be propagated. Separation of bounding box propagation and margin/reflow state
		update should be considered. */

	virtual void	PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin = 0, BOOL has_inflow_content = TRUE);

	/** Propagate a break point caused by break properties or a spanned element.

		These are discovered during layout, and propagated to the nearest
		multicol container when appropriate. They are needed by the columnizer
		to do column balancing.

		@param virtual_y Virtual Y position of the break (relative to the top
		border edge of this box)
		@param break_type Type of break point
		@param breakpoint If non-NULL, it will be set to the MultiColBreakpoint
		created, if any.
		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint);

	/** Get height of float, including margins. */

	LayoutCoord		GetHeightAndMargins() const { return margin_top + margin_bottom + GetHeight(); }

	/** Get minimum height of float, including non-percentual margins. */

	LayoutCoord		GetMinHeightAndMargins() const { return (float_packed.margin_top_is_percent ? LayoutCoord(0) : margin_top) + (float_packed.margin_bottom_is_percent ? LayoutCoord(0) : margin_bottom) + content->GetMinHeight(); }

	/** Get offset from top margin edge to top border edge. */

	LayoutCoord		GetMarginTop(BOOL ignore_percent = FALSE) const { return ignore_percent && float_packed.margin_top_is_percent ? LayoutCoord(0) : margin_top; }

	/** Get distance between left border edge and left (right-floats) / right (left-floats) margin edge.

		For left-floats, return offset from left border edge to right margin edge.
		For right-floats, return offset from left margin edge to left border edge. */

	LayoutCoord		GetMarginToEdge() const { return float_edge; }

	/** Is this box a floating box? */

	virtual BOOL	IsFloatingBox() const { return TRUE; }

	/** Is this an element that logically belongs in the stack? */

	virtual BOOL	IsInStack(BOOL include_floats = FALSE) const { return include_floats; }

	/** Is this float a left aligned float? */

	BOOL			IsLeftFloat() const { return float_packed.left; }

	/** Get next float to traverse. */

	FloatingBox*	GetNextFloat() const { return float_packed.has_float_reflow_cache ? float_reflow_cache->next_float : next_float; }

	/** Set next float to traverse. */

	void			SetNextFloat(FloatingBox* next) { (float_packed.has_float_reflow_cache ? float_reflow_cache->next_float : next_float) = next; }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

#ifdef PAGED_MEDIA_SUPPORT

	/** Insert a page break. */

	virtual BREAKST	InsertPageBreak(LayoutInfo& info, int strength);

#endif // PAGED_MEDIA_SUPPORT

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }

	/** Get the lowest bottom outer edge of this float and preceding floats.

		@param float_types Which float types to look at. Allowed values are:
		CSS_VALUE_left, CSS_VALUE_right and CSS_VALUE_both
		@return One pixel below the bottom outer edge of the lowest float,
		relative to the top border edge of the block formatting
		context. LAYOUT_COORD_MIN is returned if no relevant floats were found. */

	LayoutCoord		GetLowestFloatBfcBottom(CSSValue float_types);

	/** Get the lowest minimum bottom outer edge of this float and preceding floats.

		@param float_types Which float types to look at. Allowed values are:
		CSS_VALUE_left, CSS_VALUE_right and CSS_VALUE_both
		@return One pixel below the bottom outer edge of the lowest float,
		relative to the top content edge of this block formatting
		context. LAYOUT_COORD_MIN is returned if no relevant floats were found. */

	LayoutCoord		GetLowestFloatBfcMinBottom(CSSValue float_types);

	/** Get the highest maximum width among this float and preceding floats.

		@param float_type CSS_VALUE_left or CSS_VALUE_right */

	LayoutCoord		GetHighestAccumulatedMaxWidth(CSSValue float_type);

	/** Update the reflow cache of this float (and preceding floats with invalid cache, if any) if invalid. */

	void			UpdateFloatReflowCache();

	/** Initialise the reflow cache for this floating box. See FloatReflowCache.

		@return FALSE on memory allocation error, TRUE otherwise */

	BOOL			InitialiseFloatReflowCache();

	/** Delete the reflow state for this floating box. See FloatReflowCache. */

	void			DeleteFloatReflowCache();

	/** Mark the reflow cache of this float, and all succeeding
		floats, invalid/dirty, so that it is updated before next time
		it needs to be accessed. */

	void			InvalidateFloatReflowCache();

	/** Is the reflow cache of this float invalid/dirty? */

	BOOL			IsFloatReflowCacheInvalid() { return GetFloatReflowCache()->invalid; }

	/** Remove the float from the space manager. */

	void			RemoveFromSpaceManager() { OP_ASSERT(!link.Suc()); link.Out(); }

	/** Set Y position relative to the block formatting context.

		This is only valid during reflow of the block formatting context in
		which this float lives. */

	void			SetBfcY(LayoutCoord y) { GetFloatReflowCache()->bfc_y = y; }

	/** Set minimum Y position relative to the block formatting context.

		This is only valid during reflow of the block formatting context in
		which this float lives. The value represents the Y position that this
		float would get if maximum width of its shrink-to-fit block formatting
		context is satisfied). */

	void			SetBfcMinY(LayoutCoord min_y) { GetFloatReflowCache()->bfc_min_y = min_y; }

	/** Set X position relative to the block formatting context.

		This is only valid during reflow of the block formatting context in
		which this float lives. */

	void			SetBfcX(LayoutCoord x) { GetFloatReflowCache()->bfc_x = x; }

	/** Get Y position relative to the block formatting context.

		This is only valid during reflow of the block formatting context in
		which this float lives. */

	LayoutCoord		GetBfcY() const { return GetFloatReflowCache()->bfc_y; }

	/** Get minimum Y position relative to the block formatting context.

		This is only valid during reflow of the block formatting context in
		which this float lives. The value represents the Y position that this
		float would get if maximum width of its shrink-to-fit block formatting
		context is satisfied). */

	LayoutCoord		GetBfcMinY() const { return GetFloatReflowCache()->bfc_min_y; }

	/** Get X position relative to the block formatting context.

		This is only valid during reflow of the block formatting context in
		which this float lives. */

	LayoutCoord		GetBfcX() const { return GetFloatReflowCache()->bfc_x; }

	/** Get left margin property used value. */

	LayoutCoord		GetMarginLeft() const { return GetFloatReflowCache()->margin_left; }

	/** Get left margin property used value. */

	LayoutCoord		GetMarginRight() const { return GetFloatReflowCache()->margin_right; }

	/** Get total maximum width of this float, plus all floats to the left (if
		this is a left-float) or to the right (if this is a right-float) of
		this float, if the maximum width of the block formatting context in
		which this float lives is satisfied. */

	LayoutCoord		GetAccumulatedMaxWidth() const;

	/** Set total maximum width of all floats to the left (if this float is a
		left float) or right (if this float is a right float) of this float, if
		the maximum width of the block formatting context in which this float
		lives is satisfied. */

	void			SetPrevAccumulatedMaxWidth(LayoutCoord max_width) { prev_accumulated_max_width = max_width; }

	/** Skip layout of this float. */

	void			SkipLayout(LayoutInfo& info);
};

/** Positioned floating box. */

class PositionedFloatingBox
  : public FloatingBox
{
protected:

	/** Z element for stacking context. */

	ZElement		z_element;

	/** Top relative offset. */

	LayoutCoord		top_offset;

	/** Left relative offset. */

	LayoutCoord		left_offset;

public:

					PositionedFloatingBox(HTML_Element* element, Content* content)
					  : FloatingBox(element, content),
						z_element(element),
						top_offset(0),
						left_offset(0) {}

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Get the left and top relative offsets caused by position:relative. */

	virtual void	GetRelativeOffsets(LayoutCoord& left, LayoutCoord& top) const { left = left_offset; top = top_offset; }

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }
};

/** Positioned floating box with z-index. */

class PositionedZRootFloatingBox
  : public PositionedFloatingBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootFloatingBox(HTML_Element* element, Content* content)
						: PositionedFloatingBox(element, content) {}

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return stacking_context.HasNegativeZChildren(); }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }
};

/** Block compact box. */

class BlockCompactBox
  : public BlockBox
{
public:

					BlockCompactBox(HTML_Element* element, Content* content)
					  : BlockBox(element, content) {}

	/** Is this box a compact box? */

	virtual BOOL	IsBlockCompactBox() const { return TRUE; }
};

/** Relative positioned or transformed block box.  Transformed block
	boxes behave simliar to relative positioned block boxes. */

class PositionedBlockBox
  : public BlockBox
{
private:

	/** Cached top CSS property. */

	LayoutCoord		top;

	/** Cached left CSS property. */

	LayoutCoord		left;

	/** Set the left and top CSS properties of positioned boxes, since these values need to be cached. */

	virtual void	SetRelativePosition(LayoutCoord new_left, LayoutCoord new_top) { left = new_left; top = new_top; }

	/** Set Y position. */

	virtual void	SetY(LayoutCoord new_y) { y = new_y + LayoutCoord(top); }

protected:

	/** Z element for stacking context. */

	ZElement		z_element;

public:

					PositionedBlockBox(HTML_Element* element, Content* content)
					  : BlockBox(element, content),
						top(0),
						left(0),
						z_element(element) {}

	/** Traverse box with children. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Get box Y position, relative to parent. */

	virtual LayoutCoord
					GetStackPosition() const { return y - LayoutCoord(top); }

	/** Get and add the normal flow position of this box to the supplied coordinate variables. */

	virtual void	AddNormalFlowPosition(LayoutCoord &x, LayoutCoord &y) const { x += this->x - LayoutCoord(left); y += LayoutCoord(this->y) - LayoutCoord(top); }

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Propagate a break point caused by break properties or a spanned element.

		These are discovered during layout, and propagated to the nearest
		multicol container when appropriate. They are needed by the columnizer
		to do column balancing.

		@param virtual_y Virtual Y position of the break (relative to the top
		border edge of this box)
		@param break_type Type of break point
		@param breakpoint If non-NULL, it will be set to the MultiColBreakpoint
		created, if any.
		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint);

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }
};

/** Positioned block box with space manager.

    Used by any relatively-positioned block box type that establishes a new
    block formatting context (e.g. if it has overflow: auto/scroll). */

class PositionedSpaceManagerBlockBox
	: public PositionedBlockBox
{
private:

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

public:

					PositionedSpaceManagerBlockBox(HTML_Element* element, Content* content)
					  : PositionedBlockBox(element, content) {}

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }
};

/** Relative positioned block box. */

class PositionedZRootBox
  : public PositionedBlockBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootBox(HTML_Element* element, Content* content)
					  : PositionedBlockBox(element, content) {}

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return stacking_context.HasNegativeZChildren(); }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }
};


/** Positioned block box with ZList and space manager.

    Used by any relatively-positioned block box type that establishes a new
    block formatting context (e.g. if it has overflow: auto/scroll) and a new
    stacking context (e.g. non-auto z-index). */

class PositionedSpaceManagerZRootBox
	: public PositionedZRootBox
{
private:

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

public:

					PositionedSpaceManagerZRootBox(HTML_Element* element, Content* content)
					  : PositionedZRootBox(element, content) {}

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }
};


/** Block box which establishes new stacking context. Used for opacity < 1. */

class ZRootBlockBox
	: public BlockBox
{
private:

	/** Z element for stacking context. */

	ZElement		z_element;

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					ZRootBlockBox(HTML_Element* element, Content* content)
					  : BlockBox(element, content),
						z_element(element) {}

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip) { stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip); }

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return stacking_context.HasNegativeZChildren(); }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Find the normal right edge of the rightmost absolute positioned box. */

	virtual LayoutCoord
					FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade) { return stacking_context.FindNormalRightAbsEdge(hld_profile, parent_cascade); }
};

/** Floating box which establishes new stacking context. Used for opacity < 1. */

class ZRootFloatingBox
	: public FloatingBox
{
private:

	/** Z element for stacking context. */

	ZElement		z_element;

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					ZRootFloatingBox(HTML_Element* element, Content* content)
					  : FloatingBox(element, content),
						z_element(element) {}

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip) { stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip); }

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return stacking_context.HasNegativeZChildren(); }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Find the normal right edge of the rightmost absolute positioned box. */

	virtual LayoutCoord
					FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade) { return stacking_context.FindNormalRightAbsEdge(hld_profile, parent_cascade); }

	/** Should TraversalObject let Box handle clipping/overflow on its own?

		Some boxes, depending on traversed content, may need to differentiate
		between clipping and overflow. Clipping rectangle should include
		overflow (even if overflow is hidden) for some descendants (ie. objects
		on StackingContext) therefore clipping must be applied on-demand during
		traverse rather than in Enter*Box. */

	virtual BOOL	HasComplexOverflowClipping() const { return TRUE; }
};

/** Block box with space manager which establishes new stacking context. Used for opacity < 1. */

class ZRootSpaceManagerBlockBox
	: public SpaceManagerBlockBox
{
private:

	/** Z element for stacking context. */

	ZElement		z_element;

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					ZRootSpaceManagerBlockBox(HTML_Element* element, Content* content)
					  : SpaceManagerBlockBox(element, content),
						z_element(element) {}

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip) { stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip); }

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return stacking_context.HasNegativeZChildren(); }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Find the normal right edge of the rightmost absolute positioned box. */

	virtual LayoutCoord
					FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade) { return stacking_context.FindNormalRightAbsEdge(hld_profile, parent_cascade); }

	/** Should TraversalObject let Box handle clipping/overflow on its own?

		Some boxes, depending on traversed content, may need to differentiate
		between clipping and overflow. Clipping rectangle should include
		overflow (even if overflow is hidden) for some descendants (ie. objects
		on StackingContext) therefore clipping must be applied on-demand during
		traverse rather than in Enter*Box. */

	virtual BOOL	HasComplexOverflowClipping() const { return TRUE; }
};

/** Column/pane-attached float.

	These have a column, multi-column container or page as their flow root, and
	works quite differently from regular CSS 2.1 floats (which have a block
	formatting context as their flow root.)

	The 'x' and 'y' positions stored here are relative to the top/left edge of
	the first pane in which the float lives. */

class FloatedPaneBox
	: public SpaceManagerBlockBox
{
private:

	/** Entry in PaneFloatList. */

	PaneFloatEntry	list_entry;

	/** Entry in a list sorted by document tree order. */

	PaneFloatEntry	logical_entry;

	/** Entry in list of traversal targets. */

	PaneFloatEntry	traversal_entry;

	union
	{
		struct
		{
			/** TRUE if top-aligned, FALSE if bottom-aligned. */

			unsigned int
					is_top_float:1;

			/** TRUE if aligned with the far (right in LTR mode) corner. */

			unsigned int
					is_far_corner_float:1;

			/** TRUE if this float should be put in the row following the one it's defined in. */

			unsigned int
					put_in_next_row:1;

#ifdef PAGED_MEDIA_SUPPORT

			/** TRUE if this float should be put on the page following the one it's defined in. */

			unsigned int
					put_on_next_page:1;

			/** TRUE if a page break is required after this floated pane box. */

			unsigned int
					force_page_break_after:1;

#endif // PAGED_MEDIA_SUPPORT

			/** TRUE if a column break is required after this floated pane box. */

			unsigned int
					force_column_break_after:1;

			/** Actual value of column-span. */

			unsigned int
					column_span:14;
		} panebox_packed;
		UINT32		panebox_packed_init;
	};

protected:

	/** Actual value of margin-top. */

	LayoutCoord		margin_top;

	/** Actual value of margin-bottom. */

	LayoutCoord		margin_bottom;

	/** The pane in which this float starts. -1 means auto or N/A. */

	int				start_pane;

	/** Set the left and top CSS properties of positioned boxes, since these values need to be cached. */

	virtual void	SetRelativePosition(LayoutCoord new_left, LayoutCoord new_top) {}

public:

					FloatedPaneBox(HTML_Element* element, Content* content)
						: SpaceManagerBlockBox(element, content),
						  list_entry(this),
						  logical_entry(this),
						  traversal_entry(this),
						  panebox_packed_init(0),
						  margin_top(0),
						  margin_bottom(0) {}

	virtual			~FloatedPaneBox() { list_entry.Out(); logical_entry.Out(); traversal_entry.Out(); }

	/** Return TRUE if this is a FloatedPaneBox. */

	virtual BOOL	IsFloatedPaneBox() const { return TRUE; }

	/** Propagate bottom margins and bounding box to parents, and walk the dog. */

	virtual void	PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin = 0, BOOL has_inflow_content = TRUE);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Get width available for the margin box. */

	virtual LayoutCoord
					GetAvailableWidth(LayoutProperties* cascade);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Traverse box with children. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Traverse box with children.

		This method will traverse this inline element and its children. It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Is this an element that logically belongs in the stack? */

	virtual BOOL	IsInStack(BOOL include_floats = FALSE) const { return include_floats; }

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf, Container* container);

	/** Insert this float into a float list. */

	void			IntoPaneFloatList(PaneFloatList& float_list, PaneFloatEntry* insert_before = NULL);

	/** Insert this float into a float list. */

	void			IntoPaneFloatList(PaneFloatList& float_list, int start_pane, BOOL in_next_row);

	/** Insert this float into the logically sorted list of floats. */

	void			IntoLogicalList(PaneFloatList& float_list) { logical_entry.Out(); logical_entry.Into(&float_list); }

	/** Queue box for traversal (floats are traversed in a separate pass between background and content). */

	void			QueueForTraversal(PaneFloatList& float_list)
	{
		/* Protect against duplicate queueing, as this method may be called on
		   the same box more than once in some cases (happens for containers
		   that are both paged and multicol). */

		if (!traversal_entry.InList())
		{
			traversal_entry.Out();
			traversal_entry.Into(&float_list);
		}
	}

	/** Advance to next traversal target.

		Takes this box out of the traversal list. */

	FloatedPaneBox*	AdvanceToNextTraversalTarget() { PaneFloatEntry* next_entry = traversal_entry.Suc(); traversal_entry.Out(); return next_entry ? next_entry->GetBox() : NULL; }

	/** Return TRUE if this box is top-floated, or FALSE if it is bottom-floated. */

	BOOL			IsTopFloat() const { return panebox_packed.is_top_float; }

	/** Return TRUE if aligned with the far (right in LTR mode) corner. */

	BOOL			IsFarCornerFloat() const { return panebox_packed.is_far_corner_float; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Return TRUE if this float should be put in the page following the one it's defined in. */

	BOOL			IsForNextPage() const { return panebox_packed.put_on_next_page == 1; }

#endif // PAGED_MEDIA_SUPPORT

	/** Get actual value of column-span. */

	int				GetColumnSpan() const { return panebox_packed.column_span; }

	/** Get actual value of margin-top. */

	LayoutCoord		GetMarginTop() const { return margin_top; }

	/** Get actual value of margin-bottom. */

	LayoutCoord		GetMarginBottom() const { return margin_bottom; }

	/** Get the start pane for this float. -1 means auto or N/A. */

	int				GetStartPane() const { return start_pane; }

	/** Return TRUE if this float should be put in the row following the one it's defined in. */

	BOOL			IsForNextRow() const { return panebox_packed.put_in_next_row; }

	/** Reset "for next row" flag. Happens when advancing to next row. */

	void			ResetIsForNextRow() { panebox_packed.put_in_next_row = 0; }

	/** Move float to next row.

		There were not enough columns left for it in the current row. */

	void			MoveToNextRow();

#ifdef PAGED_MEDIA_SUPPORT

	/** Return TRUE if a page break is required after this floated pane box. */

	BOOL			IsPageBreakAfterForced() const { return panebox_packed.force_page_break_after == 1; }

#endif // PAGED_MEDIA_SUPPORT

	/** Return TRUE if a column break is required after this floated pane box. */

	BOOL			IsColumnBreakAfterForced() const { return panebox_packed.force_column_break_after == 1; }
};

/** Column/pane-attached positioned float. */

class PositionedFloatedPaneBox
	: public FloatedPaneBox
{
protected:

	/** Z element for stacking context. */

	ZElement		z_element;

	/** Cached top CSS property. */

	LayoutCoord		top;

	/** Cached left CSS property. */

	LayoutCoord		left;

public:

					PositionedFloatedPaneBox(HTML_Element* element, Content* content)
						: FloatedPaneBox(element, content),
						  z_element(element) {}

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Set the left and top CSS properties of positioned boxes, since these values need to be cached. */

	virtual void	SetRelativePosition(LayoutCoord new_left, LayoutCoord new_top) { left = new_left; top = new_top; }

	/** Set Y position. */

	virtual void	SetY(LayoutCoord new_y) { y = new_y + top; }
};

/** Column/pane-attached positioned float that establishes a new stacking context. */

class PositionedZRootFloatedPaneBox
	: public PositionedFloatedPaneBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

public:

					PositionedZRootFloatedPaneBox(HTML_Element* element, Content* content)
						: PositionedFloatedPaneBox(element, content) {}

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return stacking_context.HasNegativeZChildren(); }
};

/** Column/pane-attached float that establishes a new stacking context. */

class ZRootFloatedPaneBox
	: public FloatedPaneBox
{
private:

	/** Z element for stacking context. */

	ZElement		z_element;

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

public:

					ZRootFloatedPaneBox(HTML_Element* element, Content* content)
						: FloatedPaneBox(element, content),
						  z_element(element) {}

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);

	/** Has z children with negative z-index */

	virtual BOOL	HasNegativeZChildren() { return stacking_context.HasNegativeZChildren(); }
};

#ifdef CSS_TRANSFORMS
TRANSFORMED_BOX(AbsoluteZRootBox);
TRANSFORMED_BOX(PositionedSpaceManagerZRootBox);
TRANSFORMED_BOX(PositionedZRootBox);
TRANSFORMED_BOX(PositionedZRootFloatingBox);
TRANSFORMED_BOX(PositionedZRootFloatedPaneBox);
#endif // CSS_TRANSFORMS

#endif // _BLOCKBOX_H_
