/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef FLEXCONTENT_H
#define FLEXCONTENT_H

#include "modules/layout/layout_coord.h"
#include "modules/layout/layout.h"
#include "modules/layout/content/content.h"
#include "modules/util/simset.h"

class FlexItemBox;

/** List of flex items (FlexItemBox objects).

	Sorted both in logical order and by the 'order' property. */

class FlexItemList
	: private Head
{
	friend class FlexItemBox;

private:

	/** First item in list, when sorted by 'order'. */

	FlexItemBox*	flexorder_first;

	/** Last item in list, when sorted by 'order'. */

	FlexItemBox*	flexorder_last;

public:

	FlexItemList()
		: flexorder_first(NULL),
		  flexorder_last(NULL) {}

	/** Get the first item in the list. */

	FlexItemBox*	First(BOOL logical) const;

	/** Get the last item in the list. */

	FlexItemBox*	Last(BOOL logical) const;

	/** Check if list is empty. */

	BOOL			Empty() const { return Head::Empty(); }

	/** Clear list, without deallocating elements. */

	void			RemoveAll();
};

/** Reflow state for a FlexLine. */

class FlexLineReflowState
{
public:

	/** Sum of base main sizes of all items on line.

		Used to determine extra space for flex. */

	LayoutCoord		total_base_size;

	/** Sum of hypothetical main sizes of all items on line.

		Used to determine line breaks. */

	LayoutCoord		total_hypothetical_size;

	/** Line's max cross size.

		Some items' cross size depend on the line's cross size
		(shrink-to-fit). The maximum cross size tells how much space items
		possibly could use. After all lines have been created, the lines can be
		stretched because of this. */

	LayoutCoord		max_cross_size;

	/** Largest distance from top margin edge to baseline among baseline aligned items.

		Only meaningful in horizontal flexboxes.
		Note that this value may be negative due to negative top margin. */

	LayoutCoord		max_above_baseline;

	/** Largest distance from baseline to bottom margin edge among baseline aligned items.

		Only meaningful in horizontal flexboxes.
		Note that this value may be negative due to negative bottom margin. */

	LayoutCoord		max_below_baseline;

	/** Number of items on line. */

	int				item_count;

	/** Total number of auto margins along the main axis on line. */

	int				auto_margins;

					FlexLineReflowState()
						: total_base_size(0),
						  total_hypothetical_size(0),
						  max_cross_size(0),
						  max_above_baseline(LAYOUT_COORD_HALF_MIN),
						  max_below_baseline(LAYOUT_COORD_HALF_MIN),
						  item_count(0),
						  auto_margins(0) {}
};

/** A line in a flexbox. */

class FlexLine : public Link
{
private:

	/** Data used while this line's flex container is being reflowed. */

	FlexLineReflowState*
					reflow_state;

	/** First item on line. */

	FlexItemBox*	start_item;

	/** Line's cross size. */

	LayoutCoord		cross_size;

	/** Line's cross start position. */

	LayoutCoord		cross_pos;

public:

					FlexLine(FlexItemBox* item)
						: reflow_state(NULL),
						  start_item(item),
						  cross_size(0),
						  cross_pos(0) {}

					~FlexLine() { OP_DELETE(reflow_state); }

	FlexLine*		Suc() const { return (FlexLine*) Link::Suc(); }
	FlexLine*		Pred() const { return (FlexLine*) Link::Pred(); }

	/** Initialize object for layout.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			Init();

	/** Finish layout and clean up. */

	void			FinishLayout();

	/** Add item to line.

		Update item count and increase total size. */

	void			AddItem(LayoutCoord base_size, LayoutCoord hypothetical_size, LayoutCoord cross_size, int auto_margin_count)
	{
		if (this->cross_size < cross_size)
			this->cross_size = cross_size;

		reflow_state->total_base_size += base_size;
		reflow_state->total_hypothetical_size += hypothetical_size;
		reflow_state->item_count ++;
		reflow_state->auto_margins += auto_margin_count;
	}

	/** Propagate maximum cross size from an item whose cross size depends on the line's. */

	void			PropagateMaxCrossSize(LayoutCoord cross_size)
	{
		if (reflow_state->max_cross_size < cross_size)
			reflow_state->max_cross_size = cross_size;
	}

	/** Add baseline info for an item.

		Only meaningful for baseline aligned items in horizontal flexboxes. */

	void			AddBaseline(LayoutCoord above_baseline, LayoutCoord below_baseline)
	{
		if (reflow_state->max_above_baseline < above_baseline)
			reflow_state->max_above_baseline = above_baseline;

		if (reflow_state->max_below_baseline < below_baseline)
			reflow_state->max_below_baseline = below_baseline;
	}

	/** Finish line. */

	void			Finish()
	{
		LayoutCoord max_baseline = reflow_state->max_above_baseline + reflow_state->max_below_baseline;

		if (cross_size < max_baseline)
			cross_size = max_baseline;
	}

	/** Get the first item on line. */

	FlexItemBox*	GetStartItem() const { return start_item; }

	/** Get the first item on the next line, or NULL if there is no next line. */

	FlexItemBox*	GetNextStartItem() const { if (FlexLine* next_line = Suc()) return next_line->GetStartItem(); return NULL; }

	/** Get sum of base main sizes of all items on line. */

	LayoutCoord		GetMainBaseSize() const { return reflow_state->total_base_size; }

	/** Get sum of hypothetical main sizes of all items on line. */

	LayoutCoord		GetMainHypotheticalSize() const { return reflow_state->total_hypothetical_size; }

	/** Get cross size of line. */

	LayoutCoord		GetCrossSize() const { return cross_size; }

	/** Get amount of extra space that this line can use.

		Items whose cross size depend on the line's cross size contribute to
		this value. */

	LayoutCoord		GetMaxCrossSizeDiff() const
	{
		LayoutCoord diff = reflow_state->max_cross_size - cross_size;

		return MAX(diff, LayoutCoord(0));
	}

	/** Set cross start position of line. */

	void			SetCrossPosition(LayoutCoord pos) { cross_pos = pos; }

	/** Get cross start position of line. */

	LayoutCoord		GetCrossPosition() const { return cross_pos; }

	/** Set cross size of line. */

	void			SetCrossSize(LayoutCoord cross_size) { this->cross_size = cross_size; }

	/** Get baseline of line. */

	LayoutCoord		GetBaseline() const { return reflow_state->max_above_baseline; }

	/** Get the number of in-flow items on line. */

	int				GetItemCount() const { return reflow_state->item_count; }

	/** Get total number of auto margins along the main axis on line. */

	int				GetAutoMarginCount() const { return reflow_state->auto_margins; }

	/** Get break policies for line.

		Only used with horizontal flexboxes. */

	void			GetBreakPolicies(BREAK_POLICY& column_break_before,
									 BREAK_POLICY& page_break_before,
									 BREAK_POLICY& column_break_after,
									 BREAK_POLICY& page_break_after);
};

/** List of flex lines.

	Sorted from cross-start to cross-end. */

class FlexLineStack : public AutoDeleteHead
{
public:
	FlexLine*		First() const { return (FlexLine*) AutoDeleteHead::First(); }
	FlexLine*		Last() const { return (FlexLine*) AutoDeleteHead::Last(); }
};

/** Reflow state for FlexContent. */

class FlexContentReflowState
{
public:

	/** Used value of 'height' if computed value is non-auto, otherwise auto. */

	LayoutCoord		css_height;

	/** Main axis size of containing block to use for percentage resolution. May be auto. */

	LayoutCoord		containing_block_size;

	/** Max space for line main size. LAYOUT_COORD_MAX if unconstrained. */

	LayoutCoord		max_line_main_size;

	/** Hypothetical bottom (just outside the margin edge) of bottom-most item.

		This value is used for height propagation. It is relative to the
		content box top edge of the flexbox. It is not constrained by any
		computed height or max-height, or by preferred, flexed or stretched
		flexbox height. It may be affected by the flexbox _width_, on the other
		hand. */

	LayoutCoord		items_bottom;

	/** Largest minimum content width of current item.

		For min/max width propagation in vertical flexboxes. */

	LayoutCoord		cur_minimum_width;

	/** Largest maximum content width of current item.

		For min/max width propagation in vertical flexboxes. */

	LayoutCoord		cur_maximum_width;

	/** Width of content box. */

	LayoutCoord		content_width;

	/** Height of content box. */

	LayoutCoord		content_height;

	/** Line state. */

	struct
	{
		/** Space used by items (hypothetical size) on current line.

			This is used to guess where to insert line breaks, for min/max
			width propagation. */

		LayoutCoord		hyp_main_space_used;

		/** Cross space (unflexed and unstretched) used by non-baseline aligned items on current line. */

		LayoutCoord		cross_space_used;

		/** Amount of cross size height used by this item if maximum width is satisfied.

			Used to calculate minimum / intrinsic height. */

		LayoutCoord		min_height_used;

		/** Largest distance from top margin edge to baseline among baseline aligned items.

			Only meaningful in horizontal flexboxes.
			Note that this value may be negative due to negative top margin. */

		LayoutCoord		max_above_baseline;

		/** Largest distance from baseline to bottom margin edge among baseline aligned items.

			Only meaningful in horizontal flexboxes.
			Note that this value may be negative due to negative bottom margin. */

		LayoutCoord		max_below_baseline;

		/** Like max_above_baseline, but only intrinsic sizes.

			Used for minimum height / intrinsic height calculation. */

		LayoutCoord		nonpercent_above_baseline;

		/** Like max_below_baseline, but only intrinsic sizes.

			Used for minimum height / intrinsic height calculation. */

		LayoutCoord		nonpercent_below_baseline;

		/** Largest minimum content width among items on line.

			For min/max width propagation in vertical flexboxes. */

		LayoutCoord		minimum_width;

		/** Largest maximum content width among items on line.

			For min/max width propagation in vertical flexboxes. */

		LayoutCoord		maximum_width;

		/** Number of items on current line. */

		int				item_count;

		/** Prepare for a new line. */

		void			Reset()
		{
			hyp_main_space_used = LayoutCoord(0);
			cross_space_used = LayoutCoord(0);
			min_height_used = LayoutCoord(0);
			max_above_baseline = LAYOUT_COORD_HALF_MIN;
			max_below_baseline = LAYOUT_COORD_HALF_MIN;
			nonpercent_above_baseline = LAYOUT_COORD_HALF_MIN;
			nonpercent_below_baseline = LAYOUT_COORD_HALF_MIN;
			minimum_width = LayoutCoord(0);
			maximum_width = LayoutCoord(0);
			item_count = 0;
		}

		/** Get line's cross size. */

		LayoutCoord		GetCrossSize() const
		{
			LayoutCoord cross_size = cross_space_used;
			LayoutCoord baseline_aligned_height = max_above_baseline + max_below_baseline;

			if (cross_size < baseline_aligned_height)
				cross_size = baseline_aligned_height;

			return cross_size;
		}

		/** Get line's minimum height / intrinsic height.

			Only useful for horizontal flexboxes. */

		LayoutCoord		GetMinHeight() const
		{
			LayoutCoord min_height = min_height_used;
			LayoutCoord baseline_aligned_height = nonpercent_above_baseline + nonpercent_below_baseline;

			if (min_height < baseline_aligned_height)
				min_height = baseline_aligned_height;

			return min_height;
		}

	} line;

	/** State of list item layout. */

	ListItemState	list_item_state;

	/** Set if the height of at least one item got its hypothetical height changed. */

	BOOL			hypothetical_item_height_changed;

	/** Set if there's at least one item with visibility:collapse. */

	BOOL			has_collapsed_items;
};

/** Flexbox (also known as flex container).

	Created when 'display' is 'flex' or 'inline-flex'.

	A flexbox contains flex items (FlexItemBox objects). */

class FlexContent
	: public Content
{
protected:

	/** Reflow state. Pointer is valid during reflow only, otherwise NULL. */

	FlexContentReflowState*
					reflow_state;

	/** List of flex items.

		This list is both sorted in logical order (pass TRUE to
		layout_stack.First(), item->Suc() and item->Pred()), and according to
		'order' (pass FALSE to layout_stack.First(), item->Suc() and
		item->Pred()). */

	FlexItemList	layout_stack;

	/** List of flex lines. */

	FlexLineStack	line_stack;

	/** Width of border box. */

	LayoutCoord		width;

	/** Height of border box. */

	LayoutCoord		height;

	/** Height of box if maximum_width is satisfied.

		Used with min/max width propagation. */

	LayoutCoord		min_height;

	/** Sum of top border and padding. */

	LayoutCoord		top_border_padding;

	/** Minimum content width; for shrink-to-fit. */

	LayoutCoord		minimum_width;

	/** Maximum content width; for shrink-to-fit. */

	LayoutCoord		maximum_width;

	union
	{
		struct
		{
			/** TRUE if needs another reflow pass. */

			unsigned int
					needs_reflow:1;

			/** Non-zero if the next reflow is allowed to trigger another reflow.

				This indicates the number of reflow passes left that we are
				allowed to trigger after the next reflow pass.

				Flexboxes may sometimes need four reflow passes, but we have to
				be careful with circular dependencies (or even engine bugs) to
				avoid infinite reflow loops. */

			unsigned int
					additional_reflows_allowed:2;

#ifdef WEBKIT_OLD_FLEXBOX

			/** TRUE if this is an old-spec flexbox.

				If it is, we attempt to emulate the flexbox spec from
				2009-07-23 as much as reasonable. */

			unsigned int
					oldspec_flexbox:1;

#endif // WEBKIT_OLD_FLEXBOX

			/** Is a real shrink-to-fit in the box model (e.g. a float or absolutely positioned box with auto width)? */

			unsigned int
					is_shrink_to_fit:1;

			/** Set if height is percent-based. */

			unsigned int
					relative_height:1;

			/** Set if this flexbox lays out its items vertically. */

			unsigned int
					is_vertical:1;

			/** Set if flexbox item order is reversed. */

			unsigned int
					items_reversed:1;

			/** Set if flexbox line order is reversed. */

			unsigned int
					lines_reversed:1;

			/** Avoid page break inside flexbox? */

			unsigned int
					avoid_page_break_inside:1;

			/** Avoid column break inside flexbox? */

			unsigned int
					avoid_column_break_inside:1;

			/** Set if content is up-to-date, i.e. when min/max is calculated. */

			unsigned int
					content_uptodate:1;

		} flex_packed;
		UINT32		flex_packed_init;
	};

	/** Find and set / propagate explicit (column/page) breaks in flexbox. */

	OP_STATUS		FindBreaks(LayoutInfo& info, LayoutProperties* cascade, LayoutCoord& stretch);

#ifdef PAGED_MEDIA_SUPPORT

	/** Insert page or column break. */

	OP_STATUS		BreakPage(LayoutInfo& info, FlexItemBox* break_before_item, FlexLine* break_before_line, LayoutCoord virtual_y, BREAK_POLICY page_break, LayoutCoord& stretch);

#endif // PAGED_MEDIA_SUPPORT

	/** Calculate baseline.

		May return LAYOUT_COORD_MIN if calculating the baseline fails, in which
		case the caller must calculate it on its own. */

	LayoutCoord		CalculateBaseline() const;

	/** Get baseline of flexbox. */

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const;

	/** Get baseline of inline-flexbox. */

	virtual LayoutCoord
					GetBaseline(const HTMLayoutProperties& props) const;

	/** Get the baseline if maximum width is satisfied. */

	virtual LayoutCoord
					GetMinBaseline(const HTMLayoutProperties& props) const;

	/** Calculate need for scrollbars (if this is a scrollable flexbox). */

	virtual void	CalculateScrollbars(LayoutInfo& info, LayoutProperties* cascade) {}

	/** Get extra width that should be added to min/max widths. */

	virtual LayoutCoord
					GetExtraMinMaxWidth(LayoutProperties* cascade) const { return LayoutCoord(0); }

	/** Get extra height that should be added to minimum height. */

	virtual LayoutCoord
					GetExtraMinHeight(LayoutProperties* cascade) const { return LayoutCoord(0); }

public:

					FlexContent()
						: reflow_state(NULL),
						  width(0),
						  height(0),
						  min_height(0),
						  top_border_padding(0),
						  minimum_width(0),
						  maximum_width(0),
						  flex_packed_init(0) {}

	virtual			~FlexContent() { OP_DELETE(reflow_state); OP_ASSERT(layout_stack.Empty()); }

	/** Get min/max width. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Cast this object to FlexContent, if it is one. */

	virtual FlexContent*
					GetFlexContent() { return this; }

	/** Compute size of content and return TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Get width of border box. */

	virtual LayoutCoord
					GetWidth() const { return width; }

	/** Get height of border box. */

	virtual LayoutCoord
					GetHeight() const { return height; }

	/** Get minimum height (height if maximum width is satisfied) */

	virtual LayoutCoord
					GetMinHeight() const { return min_height; }

	/** Get content box width. Only available during reflow. */

	LayoutCoord		GetContentWidth() { return reflow_state->content_width; }

	/** Get content box height. Only available during reflow. */

	LayoutCoord		GetContentHeight() { return reflow_state->content_height; }

	/** Calculate flexbox border box height.

		@param info Layout info
		@param cascade The cascade for this element
		@param content_height Content box height as if height were auto.
		@param hor_scrollbar_height Height of horizontal scrollbar, if any

		@return Border box height, flexed or stretched if appropriate, and
		clamped to max-height and min-height. */

	LayoutCoord		CalculateHeight(LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord hor_scrollbar_height) const;

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain);

#endif // PAGED_MEDIA_SUPPORT

	/** Combine the specified break policies with child break policies that should be propagated to their parent. */

	virtual void	CombineChildBreakProperties(BREAK_POLICY& page_break_before,
												BREAK_POLICY& column_break_before,
												BREAK_POLICY& page_break_after,
												BREAK_POLICY& column_break_after);

	/** Is height relative? */

	virtual BOOL	HeightIsRelative() const { return flex_packed.relative_height; }

	/** Is considered a containing element, e.g. container, table or flexbox? */

	virtual BOOL	IsContainingElement() const { return TRUE; }

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation from its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return !!flex_packed.is_shrink_to_fit; }

	/** Does width propagation from this flexbox affect FlexRoot? */

	BOOL			AffectsFlexRoot() const;

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const;

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container);

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf);

	/** Return TRUE if the actual value of 'overflow' is 'visible'. */

	virtual BOOL	IsOverflowVisible() { return TRUE; }

#ifdef WEBKIT_OLD_FLEXBOX

	/** Is this an old-spec flexbox?

		If it is, return TRUE, and we will attempt to emulate the flexbox spec
		from 2009-07-23 as much as reasonable. */

	BOOL			IsOldspecFlexbox() const { return flex_packed.oldspec_flexbox; }

#endif // WEBKIT_OLD_FLEXBOX

	/** Return TRUE if this flexbox lays out its items vertically. */

	BOOL			IsVertical() const { return !!flex_packed.is_vertical; }

	/** Is flexbox item order reversed?

		In LTR mode this is the case when flex-direction is either row-reverse
		or column-reverse. In RTL mode this is the case when flex-direction is
		either row or column-reverse. */

	BOOL			IsItemOrderReversed() const { return !!flex_packed.items_reversed; }

	/** Is flexbox line order reversed?

		If flex-wrap is wrap-reverse, line order is reversed unless writing
		mode is RTL and the flexbox is vertical, in which case it is reversed
		if flex-wrap is wrap instead. */

	BOOL			IsLineOrderReversed() const { return !!flex_packed.lines_reversed; }

	/** Return TRUE if this flexbox can be split into multiple lines if necessary. */

	BOOL			IsWrappable() const { return placeholder->GetCascade()->GetProps()->flexbox_modes.GetWrap() != FlexboxModes::WRAP_NOWRAP; }

	/** Return TRUE if we are to avoid page breaks inside this container. */

	BOOL			AvoidPageBreakInside() const { return !!flex_packed.avoid_column_break_inside; }

	/** Return TRUE if we are to avoid column breaks inside this container. */

	BOOL			AvoidColumnBreakInside() const { return !!flex_packed.avoid_column_break_inside; }

	/** Finish current line. */

	void			FinishLine();

	/** Add a new flexbox item to this flex content.

		Called when starting layout of the item. */

	LAYST			GetNewItem(LayoutInfo& info, LayoutProperties* box_cascade, FlexItemBox* item_box);

	/** Finish new flexbox item.

		Called when finishing layout of the item. */

	void			FinishNewItem(LayoutInfo& info, LayoutProperties* box_cascade, FlexItemBox* item_box);

	/** Propagate maximum content width from flex item child.

		Values are in margin-box dimensions. */

	void			PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord max_width);

	/** Propagate bounding box from absolutely positioned child of flex item. */

	virtual void	PropagateBottom(const LayoutInfo& info, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Update bounding box. */

	virtual void	UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content);

	/** Called when a flex item gets its hypothetical height changed. */

	void			SetItemHypotheticalHeightChanged() { reflow_state->hypothetical_item_height_changed = TRUE; }

	/** Get the numeric value to assign to the specified list item element.

		Will also update an internal counter, so that list item elements added
		that don't specify a value attribute will use and increment this value.

		@param list_item_element The list item element whose numbering we process.
		@return The number that list_item_element should use. */

	int				GetNewListItemValue(HTML_Element* list_item_element) { return reflow_state->list_item_state.GetNewValue(list_item_element); }

	/** Is this container a reversed list? */

	BOOL			IsReversedList() { return reflow_state->list_item_state.IsReversed(); }

	/** Return TRUE if we are allowed to collapse items with visibility:collapse in this reflow pass.

		Before we can actually collapse such items, we need to know how they
		affect layout when not collapsed, so that flex cross size isn't changed
		when uncollapsing items. */

	BOOL			AllowVisibilityCollapse(const LayoutInfo& info) const { return !info.external_layout_change && !flex_packed.additional_reflows_allowed; }
};

/** Flexbox with non-visible overflow. */

class ScrollableFlexContent
	: public FlexContent,
	  public ScrollableArea
{
protected:

	/** Calculate need for scrollbars (if this is a scrollable flexbox). */

	virtual void	CalculateScrollbars(LayoutInfo& info, LayoutProperties* cascade);

	/** Get extra width that should be added to min/max widths. */

	virtual LayoutCoord
					GetExtraMinMaxWidth(LayoutProperties* cascade) const;

	/** Get extra height that should be added to minimum height. */

	virtual LayoutCoord
					GetExtraMinHeight(LayoutProperties* cascade) const;

public:

					~ScrollableFlexContent();

	/** Get the layout box that owns this ScrollableArea. */

	virtual Content_Box*
					GetOwningBox() { return placeholder; }

	/** Cast this object to ScrollableArea, if it is one. */

	virtual ScrollableArea*
					GetScrollable() { return this; }

	/** Is there typically a CoreView associated with this content ?

		@see class CoreView */

	virtual BOOL	HasCoreView() const { return TRUE; }

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Return the OpInputContext of this container, if it establishes one. */

	virtual OpInputContext*
					GetInputContext() { return this; }

	/** Return TRUE if this element can be split into multiple outer columns or pages. */

	virtual BOOL	IsColumnizable() const;

	/** Handle event that occurred on this element. */

	virtual void	HandleEvent(FramesDocument* doc, DOM_EventType event, int offset_x, int offset_y);

	/** Return TRUE if the actual value of 'overflow' is 'visible'. */

	virtual BOOL	IsOverflowVisible() { return FALSE; }

	/** Get scroll offset, if applicable for this type of box / content. */

	virtual OpPoint	GetScrollOffset();

	/** Set scroll offset, if applicable for this type of box / content.

		@param x If non-NULL, the new X position. If NULL, leave current X position as is.
		@param y If non-NULL, the new Y position. If NULL, leave current Y position as is. */

	virtual void	SetScrollOffset(int* x, int* y);

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					Enable(FramesDocument* doc);

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Propagate bounding box from absolutely positioned child of flexbox item. */

	virtual void	PropagateBottom(const LayoutInfo& info, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Update bounding box. */

	virtual void	UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content);

	// BEGIN - implementation of OpInputContext
	virtual const char*
					GetInputContextName() { return "Scrollable FlexContent"; }
	// END - implementation of OpInputContext
};

#endif // FLEXCONTENT_H
