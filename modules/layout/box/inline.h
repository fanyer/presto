/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file inline.h
 *
 * Inline box class prototypes for document layout.
 *
 * @author Geir Ivarsøy
 * @author Karl Anders Øygard
 *
 */

#ifndef _INLINE_H_
#define _INLINE_H_

#include "modules/layout/box/blockbox.h"

class InlineVerticalAlignment
{
public:
	/** Line space needed above baseline.
		This variable will be updated by child boxes if this box is part of a loose subtree,
		because that is necessary in order to get the correct 'minimum_line_height'. */

	LayoutCoord		logical_above_baseline;

	/** Line space needed below baseline.
		This variable will be updated by child boxes if this box is part of a loose subtree,
		because that is necessary in order to get the correct 'minimum_line_height'. */

	LayoutCoord		logical_below_baseline;

	/** Line space needed in total. This will be equal to the sum of logical_above_baseline and
		logical_below_baseline, unless this box is part of a loose subtree, in which case it may
		differ. */

	LayoutCoord		minimum_line_height;

	/** Line space needed above baseline, that was not resolved from a percentage value. */

	LayoutCoord		logical_above_baseline_nonpercent;

	/** Line space needed below baseline, that was not resolved from a percentage value. */

	LayoutCoord		logical_below_baseline_nonpercent;

	/** Line space needed in total, that was not resolved from a percentage value. */

	LayoutCoord		minimum_line_height_nonpercent;

	/** Distance from baseline to top border edge. */

	LayoutCoord		box_above_baseline;

	/** Distance from baseline to bottom border edge. */

	LayoutCoord		box_below_baseline;

	/** Number of pixels below baseline of line box. */

	LayoutCoord		baseline;

	/** Number of pixels below baseline of line box, that was not resolved from a percentage value. */

	LayoutCoord		baseline_nonpercent;

	/** Is loose if it is part of a subtree with vertical alignment 'top' or 'bottom'. */

	BOOL			loose;

	/** This box has vertical alignment 'top' or 'bottom'. */

	BOOL			loose_root;

	/** Vertical alignment - if 'loose' is TRUE this defines whether box is top or bottom aligned */

	BOOL			top_aligned;

					InlineVerticalAlignment()
						: logical_above_baseline(LAYOUT_COORD_MIN),
						  logical_below_baseline(LAYOUT_COORD_MIN),
						  minimum_line_height(0),
						  logical_above_baseline_nonpercent(LAYOUT_COORD_MIN),
						  logical_below_baseline_nonpercent(LAYOUT_COORD_MIN),
						  minimum_line_height_nonpercent(0),
						  box_above_baseline(0),
						  box_below_baseline(0),
						  baseline(0),
						  baseline_nonpercent(0),
						  loose(FALSE),
						  loose_root(FALSE),
						  top_aligned(FALSE) {}
};

/** Inline box reflow state. */

class InlineBoxReflowState
  : public ReflowState
{
public:

	/** Previous y root established by positioned box. */

	LayoutCoord		previous_root_y;

	/** Previous y translation for relative positioned inline excluding left offset. */

	LayoutCoord		previous_inline_y;

	/** Previous x root established by positioned box. */

	LayoutCoord		previous_root_x;

	/** Previous x translation for relative positioned inline excluding left offset. */

	LayoutCoord		previous_inline_x;

	/** Pending inline width. */

	LayoutCoord		pending_width;

	/** Vertical alignment. */

	InlineVerticalAlignment
					valign;

	/** Bounding box. */

	RelativeBoundingBox
					bounding_box;

	/** Absolutely positioned bounding box right */

	LayoutCoord		abspos_pending_bbox_right;

	/** Absolutely positioned bounding box bottom */

	LayoutCoord		abspos_pending_bbox_bottom;

					InlineBoxReflowState()
					  : pending_width(0)
					  , abspos_pending_bbox_right(0)
					  , abspos_pending_bbox_bottom(0) {}

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_inlinebox_reflow_state_pool->New(sizeof(InlineBoxReflowState)); }
	void			operator delete(void* p, size_t nbytes) { g_inlinebox_reflow_state_pool->Delete(p); }
};

/** Inline box. */

class InlineBox
  : public Content_Box
{
private:

	/** Get this box' baseline offset */

	static LayoutCoord
					GetBaselineOffset(const HTMLayoutProperties& props, LayoutCoord above_baseline, LayoutCoord below_baseline);

	/** Compute baseline. */

	void			ComputeBaseline(LayoutInfo& info);

	/** Prepare an inline box of a list item marker for layout. Does the following
		(depending on the marker type): computes dimensions,
		checks whether a bullet image is ready, sets the numeration string,
		updates the list item count in the container, allocates line space,
		such that the marker box will begin to the left (right in case of rtl)
		of the line box begin and it will end (with right (inner) margin)
		where the line box begins.

		@param info The LayoutInfo of the current reflow.
		@param layout_props The cascade of the list marker element.
		@return OK or OOM. */

	OP_STATUS		ListMarkerPreLayout(LayoutInfo& info, LayoutProperties* layout_props);

protected:

	/** Position on the virtual line the box was laid out on. */

	LayoutCoord		x;

	/** Initialise reflow state. */

	InlineBoxReflowState*
					InitialiseReflowState();

	/** Helper method for checking inline box types. */

	BOOL			CheckInlineType(const HTMLayoutProperties& props) const;

	/** Get the relative bounding box for this inline. */

	virtual void	GetRelativeBoundingBox(RelativeBoundingBox &bbox) const {}

	/** Set the relative bounding box for this inline. */

	virtual void	SetRelativeBoundingBox(const RelativeBoundingBox& bbox) {}

	/** Reset the relative bounding box. */

	virtual void	ResetRelativeBoundingBox() {}

public:

					InlineBox(HTML_Element* element, Content* content)
					  : Content_Box(element, content),
					    x(0) {}
	virtual			~InlineBox() { DeleteReflowState(); }

	/** Get reflow state. */

	InlineBoxReflowState*
					GetReflowState() const { return (InlineBoxReflowState*) Content_Box::GetReflowState(); }

	/** Is this box an inline box? */

	virtual BOOL	IsInlineBox() const { return TRUE; }

	/** Is content of this inline box weak? */

	virtual BOOL	IsWeakContent() const;

	/** Count the number of words in a segment that overlap with this box.
		Iterates through children's boxes passing the 'words' param,
		until some recursive call returns FALSE.

		Used for justified text.

		@param[in] segment The line segment where we count the words in.
		@param[out] words Gets increased by number of words that overlap with
						  this box and the segment.
		@return TRUE if there may be more words in the segment after this box, FALSE otherwise. */

	virtual BOOL	CountWords(LineSegment& segment, int& words) const;

	/** Adjust the virtual position on the line of this box and its descendants placed
		on the same line.

		The caller must be sure that the call of this method will not affect used space
		on the line and make sure the bounding box of the line will be correct after
		such operation. The method cannot be called on the boxes of elements
		that span more than one line.

		@param offset The offset to adjust the virtual position by. */

	virtual void	AdjustVirtualPos(LayoutCoord offset);

	/** Get underline for elements on the specified line. */

	virtual BOOL	GetUnderline(const Line* line, short& underline_position, short& underline_width) const;

	/** Un-translate inline. */

	virtual void	PopTranslations(LayoutInfo& info, InlineBoxReflowState* state) {}

	/** Translate inline. */

	virtual void	PushTranslations(LayoutInfo& info, InlineBoxReflowState* state) {}

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Grow width of inline box. */

	virtual void	GrowInlineWidth(LayoutCoord increment) { GetReflowState()->pending_width += increment; }

	/** Get position on the virtual line the box was laid out on. */

	virtual LayoutCoord
					GetVirtualPosition() const { return x; }

	/** Get height, minimum line height and base line. */

	virtual void	GetVerticalAlignment(InlineVerticalAlignment* valign);

	/** Propagate vertical alignment values up to nearest loose subtree root. */

	virtual void	PropagateVerticalAlignment(const InlineVerticalAlignment& valign, BOOL only_bounds);

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

	/** Absolute positioned descendants of InlineBlockBoxes contribute to the bounding box. */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Update bounding box. */

	virtual void	UpdateBoundingBox(const AbsoluteBoundingBox& box, BOOL skip_content);

	/** Reduce right and bottom part of relative bounding box. */

	virtual void	ReduceBoundingBox(LayoutCoord bottom, LayoutCoord right, LayoutCoord min_value);

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Paint background and border of this box on screen. */

	void			PaintBgAndBorder(LayoutProperties *cascade, FramesDocument* doc, VisualDevice* visual_device, const RECT& box_area, LayoutCoord position, BOOL draw_left, BOOL draw_right) const;

	/** Get the logical top and bottom of a loose subtree of inline boxes limited by end_position.

		A subtree is defined as 'loose' if the root has vertical-aligment 'top' or 'bottom'
		and consist of all descendants that are not loose themselves. */

	virtual BOOL	GetLooseSubtreeTopAndBottom(TraversalObject* traversal_object, LayoutProperties* cascade, LayoutCoord current_baseline, LayoutCoord end_position, HTML_Element* start_element, LayoutCoord& logical_top, LayoutCoord& logical_bottom);

	/** Is this box traversed within a line? */

	virtual BOOL	TraverseInLine() const { return TRUE; }
};

/** Inline run-in box. */

class InlineRunInBox
  : public InlineBox
{
private:

	/** Convert back to block? */

	BOOL			convert_to_block;

public:

					InlineRunInBox(HTML_Element* element, Content* content)
					  : InlineBox(element, content), convert_to_block(FALSE) {}

	/** Is this box a run-in box? */

	virtual BOOL	IsInlineRunInBox() const { return TRUE; }

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc, BoxSignalReason reason = BOX_SIGNAL_REASON_UNKNOWN);

	/** Request or cancel request for block conversion. */

	void			SetRunInBlockConversion(BOOL convert, LayoutInfo& info);
};

/** Inline compact box. */

class InlineCompactBox
  : public InlineRunInBox
{
public:

					InlineCompactBox(HTML_Element* element, Content* content)
					  : InlineRunInBox(element, content) {}

	/** Is this box a compact box? */

	virtual BOOL	IsInlineCompactBox() const { return TRUE; }
};

/** Inline block box. */

class InlineBlockBox
  : public InlineBox
{
private:

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

	/** Bounding box */

	RelativeBoundingBox
					bounding_box;

protected:

	/** Get the relative bounding box for this inline. */

	virtual void	GetRelativeBoundingBox(RelativeBoundingBox &bbox) const { bbox = bounding_box; }

	/** Set the relative bounding box for this inline. */

	virtual void	SetRelativeBoundingBox(const RelativeBoundingBox& bbox) { bounding_box = bbox; }

	/** Reset the relative bounding box for this inline */

	virtual void	ResetRelativeBoundingBox() { bounding_box.Reset(LayoutCoord(0), LayoutCoord(0)); }

public:

					InlineBlockBox(HTML_Element* element, Content* content)
					  : InlineBox(element, content) {}

	/** Is this box an inline block box? */

	virtual BOOL	IsInlineBlockBox() const { return TRUE; }

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

	/** Grow width of inline box. */

	virtual void	GrowInlineWidth(LayoutCoord increment) { OP_ASSERT(!content->IsInlineContent()); }

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }

	/** Absolute positioned descendants of InlineBlockBoxes contribute to the bounding box. */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

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
};

/** Relative positioned inline box. */

class PositionedInlineBox
  : public InlineBox
{
protected:

	/** Z element for z list. */

	ZElement		z_element;

	/** X position of the containing block established by this positioned inline relative to its containing block,
		excluding any offset caused by left/right properties on this box and any other positioned inline
		box ancestor on the same line. */

	LayoutCoord		containing_block_x;

	/** Y position of the containing block established by this positioned inline relative to its containing block,
		excluding any offset caused by top/bottom properties on this box and any other positioned inline
		box ancestor on the same line. */

	LayoutCoord		containing_block_y;

	/** Width of the containing block established by this positioned inline. */

	LayoutCoord		containing_block_width;

	/** Height of the containing block established by this positioned inline. */

	LayoutCoord		containing_block_height;

	/** Accumulated left offset for positioned inline ascendants in the
		element's container including this element's left offset. */

	LayoutCoord		accumulated_left_offset;

	/** Accumulated top offset for positioned inline ascendants in the
		element's container including this element's top offset. */

	LayoutCoord		accumulated_top_offset;

	/** Used to define different reasons of containing block calculation.

		The values act as flags. Except CONTAINING_BLOCK_REASON_NOT_CALCULATED,
		which is a shortcut to "all flags disabled".

		CONTAINING_BLOCK_REASON_ABSOLUTE_DESCENDANT:
		A positioned inline box serves as a containing block for
		at least one absolutely positioned descendant. So the reason of calculation
		was the fact that such descendant needs it (for example to be able to calculate
		correct translations during traversing).

		CONTAINING_BLOCK_REASON_OFFSET_CALCULATION:
		Containg block was calculated because we can make use of it to determine
		the offset of this box from some block level ancestor up the tree (e.g.
		the box of the zroot of the Z element of this).

		Generally we don't calculate containing block for this reason if such offset
		would be useless, because e.g. a transform on this inline element or some
		inline ancestor that is a descedant of this element's container.
		Curently, for simplicity, we just check this element and inline ancestors
		for a local stacking contexts. */

	enum ContainingBlockReason
	{
		CONTAINING_BLOCK_REASON_NOT_CALCULATED = 0,
		CONTAINING_BLOCK_REASON_ABSOLUTE_DESCENDANT = 1,
		CONTAINING_BLOCK_REASON_OFFSET_CALCULATION = 2
	};

	/** Is the containing block size/position calculated ?

		Two bits are used in order to keep the reason of the containing
		block calculation (both bits disabled mean that the containing block
		is not calculated at all).

		@see ContainingBlockReason */

	unsigned int	containing_block_calculated:2;

	/** Did the containing block size change in the previous reflow? */

	unsigned int	containing_block_changed:1;

	/** Bounding box. */

	RelativeBoundingBox
					bounding_box;

	/** Calculate size and position of the containing block established by this box.

		Only to be called after the container of this box has been reflowed.

		@return FALSE on memory error, TRUE otherwise. */

	BOOL			CalculateContainingBlock(FramesDocument* doc, ContainingBlockReason reason);

public:

					PositionedInlineBox(HTML_Element* element, Content* content)
					  : InlineBox(element, content),
						z_element(element),
						containing_block_x(0),
						containing_block_y(0),
						containing_block_width(0),
						containing_block_height(0),
						accumulated_left_offset(0),
						accumulated_top_offset(0),
						containing_block_calculated(CONTAINING_BLOCK_REASON_NOT_CALCULATED),
						containing_block_changed(0) { }

	/** Traverse box with children. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth() { return containing_block_width; }

	/** Get the value to be used as the containing block height for
		absolutely positioned children.
		NOTE: While for all other implementations of GetContainingBlockWidth()
		and GetContainingBlockHeight(), padding and border will be part of this
		value, in this particular case, only the content height will be
		returned. */

	virtual LayoutCoord
					GetContainingBlockHeight() { return containing_block_height; }

	/** Relative to its containing block, excluding any offset caused by left/right
		properties on this box and any other positioned inline box ancestor on the same line,
		get the X position of the containing block that this inline element establishes
		(for absolutely positioned descendants). */

	LayoutCoord		GetContainingBlockX() { return containing_block_x; }

	/** Relative to its containing block, excluding any offset caused by top/bottom
		properties on this box and any other positioned inline box ancestor on the same line,
		get the Y position of the containing block that this inline element establishes
		(for absolutely positioned descendants). */

	LayoutCoord		GetContainingBlockY() { return containing_block_y; }

	/** @return TRUE if containing block x/y/width/height has been calculated after last reflow. */

	BOOL			IsContainingBlockCalculated() { return containing_block_calculated != CONTAINING_BLOCK_REASON_NOT_CALCULATED; }

	/** @return TRUE if containing block x/y/width/height has been calculated after
				last reflow, because of the possibility to calculate the meaningful offset
				of this box from some block level ancestor.

		@see enum ContainingBlockReason. */

	BOOL			IsContainingBlockCalculatedForOffset() { return containing_block_calculated & CONTAINING_BLOCK_REASON_OFFSET_CALCULATION; }

	/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

	virtual void	RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box);

	/** Un-translate inline. */

	virtual void	PopTranslations(LayoutInfo& info, InlineBoxReflowState* state);

	/** Translate inline. */

	virtual void	PushTranslations(LayoutInfo& info, InlineBoxReflowState* state);

	/** Return accumulated left offset for positioned inline ascendants in the
		element's container including this element's left offset. */

	LayoutCoord		GetAccumulatedLeftOffset() { return accumulated_left_offset; }

	/** Return accumulated top offset for positioned inline ascendants in the
		element's container including this element's top offset. */

	LayoutCoord		GetAccumulatedTopOffset() { return accumulated_top_offset; }

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc, BoxSignalReason reason = BOX_SIGNAL_REASON_UNKNOWN);

	/** Get the relative bounding box for this inline. */

	virtual void	GetRelativeBoundingBox(RelativeBoundingBox &bbox) const { bbox = bounding_box; }

	/** Set the relative bounding box for this inline. */

	virtual void	SetRelativeBoundingBox(const RelativeBoundingBox& bbox) { bounding_box = bbox; }

	/** Get bounding box relative to top/left border edge of this box */

	virtual void	GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow = TRUE, BOOL adjust_for_multipane = FALSE, BOOL apply_clip = TRUE) const;

	/** Reset the relative bounding box for this inline */

	virtual void	ResetRelativeBoundingBox() { bounding_box.Reset(LayoutCoord(0), LayoutCoord(0)); }
};

/** Relative positioned inline block box. */

class PositionedInlineBlockBox
  : public PositionedInlineBox
{
private:

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

public:

					PositionedInlineBlockBox(HTML_Element* element, Content* content)
					  : PositionedInlineBox(element, content) {}

	/** Is this box an inline block box? */

	virtual BOOL	IsInlineBlockBox() const { return TRUE; }

	/** Grow width of inline box. */

	virtual void	GrowInlineWidth(LayoutCoord increment) { OP_ASSERT(!content->IsInlineContent()); }

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }
};

/** Inline box that has z index. */

class InlineZRootBox
  : public PositionedInlineBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					InlineZRootBox(HTML_Element* element, Content* content)
					  : PositionedInlineBox(element, content) {}

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }
};

/** Relative positioned inline block box that has z index. */

class InlineBlockZRootBox
  : public InlineZRootBox
{
private:

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

public:

					InlineBlockZRootBox(HTML_Element* element, Content* content)
					  : InlineZRootBox(element, content) {}

	/** Is this box an inline block box? */

	virtual BOOL	IsInlineBlockBox() const { return TRUE; }

	/** Grow width of inline box. */

	virtual void	GrowInlineWidth(LayoutCoord increment) { OP_ASSERT(!content->IsInlineContent()); }

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }
};

/** Inline box which creates a new stacking context. For elements with opacity < 1. */

class ZRootInlineBox
	: public InlineBox
{
private:

	/** Z element for z list. */

	ZElement		z_element;

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					ZRootInlineBox(HTML_Element* element, Content* content)
						: InlineBox(element, content), z_element(element) {}

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

	/** Find the normal right edge of the rightmost absolute positioned box. */

	virtual LayoutCoord
					FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade) { return stacking_context.FindNormalRightAbsEdge(hld_profile, parent_cascade); }
};

/** Inline block box which creates a new stacking context. For elements with opacity < 1. */

class ZRootInlineBlockBox
	: public InlineBlockBox
{
private:

	/** Z element for z list. */

	ZElement		z_element;

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					ZRootInlineBlockBox(HTML_Element* element, Content* content)
						: InlineBlockBox(element, content), z_element(element) {}

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse box with children.

		This method will traverse this inline element and its children.  It
		will only traverse the part of the virtual line that the elements have been
		laid out on indicated by position and length. */

	virtual BOOL	LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline);

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

#ifdef CSS_TRANSFORMS
TRANSFORMED_BOX(InlineBlockZRootBox);
TRANSFORMED_BOX(InlineZRootBox);
#endif // CSS_TRANSFORMS
#endif // _INLINE_H_
