/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/layout/content/flexcontent.h"

#include "modules/layout/layout_workplace.h"
#include "modules/layout/box/flexitem.h"
#include "modules/layout/traverse/traverse.h"

/** Get the first item in the list. */

FlexItemBox*
FlexItemList::First(BOOL logical) const
{
	return logical ? (FlexItemBox*) Head::First() : flexorder_first;
}

/** Get the last item in the list. */

FlexItemBox*
FlexItemList::Last(BOOL logical) const
{
	return logical ? (FlexItemBox*) Head::Last() : flexorder_last;
}

/** Clear list, without deallocating elements. */

void
FlexItemList::RemoveAll()
{
	while (FlexItemBox* item = First(TRUE))
		item->Out();
}

/** Initialize object. */

BOOL
FlexLine::Init()
{
	OP_ASSERT(!reflow_state);
	reflow_state = OP_NEW(FlexLineReflowState, ());

	return reflow_state != NULL;
}

/** Finish layout and clean up. */

void
FlexLine::FinishLayout()
{
	OP_DELETE(reflow_state);
	reflow_state = NULL;
}

/** Get break policies for line. */

void
FlexLine::GetBreakPolicies(BREAK_POLICY& column_break_before,
						   BREAK_POLICY& page_break_before,
						   BREAK_POLICY& column_break_after,
						   BREAK_POLICY& page_break_after)
{
	FlexLine* next_line = Suc();
	FlexItemBox* next_line_box = next_line ? next_line->GetStartItem() : NULL;

	column_break_before = BREAK_ALLOW;
	page_break_before = BREAK_ALLOW;
	column_break_after = BREAK_ALLOW;
	page_break_after = BREAK_ALLOW;

	for (FlexItemBox* item_box = start_item; item_box != next_line_box; item_box = item_box->Suc(FALSE))
	{
		column_break_before = CombineBreakPolicies(column_break_before, item_box->GetColumnBreakPolicyBefore());
		page_break_before = CombineBreakPolicies(page_break_before, item_box->GetPageBreakPolicyBefore());
		column_break_after = CombineBreakPolicies(column_break_after, item_box->GetColumnBreakPolicyAfter());
		page_break_after = CombineBreakPolicies(page_break_after, item_box->GetPageBreakPolicyAfter());
	}
}

/** Find and set / propagate explicit (column/page) breaks in flexbox. */

OP_STATUS
FlexContent::FindBreaks(LayoutInfo& info, LayoutProperties* cascade, LayoutCoord& stretch)
{
	BREAK_POLICY prev_page_break_policy = BREAK_AVOID;
	BREAK_POLICY prev_column_break_policy = BREAK_AVOID;

#ifdef PAGED_MEDIA_SUPPORT
	/* Set to something other than BREAK_AVOID if we should insert a page break
	   (either implicit or explicit) before processing any more items. */

	BREAK_POLICY insert_break = BREAK_AVOID;

	/* The last flex item box that we're past; on the previous line if this is
	   a horizontal wrappable flexbox. */

	FlexItemBox* prev_box = NULL;
#endif // PAGED_MEDIA_SUPPORT

	stretch = LayoutCoord(0);

	if (flex_packed.is_vertical)
	{
		/* With the current design, a vertical flexbox can be fragmented only
		   if it is single-line. Furthermore, we cannot fragment reverse item
		   stacking, because of the mismatch between logical and visual
		   order. */

		if ((!line_stack.First() || !line_stack.First()->Suc()) && !flex_packed.items_reversed)
		{
			for (FlexItemBox* item_box = layout_stack.First(FALSE); item_box; item_box = item_box->Suc(FALSE))
			{
				LayoutCoord y = item_box->GetNewStaticMarginY(this);
				BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, item_box->GetPageBreakPolicyBefore());
				BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, item_box->GetColumnBreakPolicyBefore());

				if (cascade->multipane_container)
				{
					if (BreakForced(page_break_policy) || BreakForced(column_break_policy))
					{
						BREAK_TYPE break_type =
#ifdef PAGED_MEDIA_SUPPORT
							BreakForced(page_break_policy) ? PAGE_BREAK :
#endif // PAGED_MEDIA_SUPPORT
							COLUMN_BREAK;

						if (!placeholder->PropagateBreakpoint(y, break_type, NULL))
							return OpStatus::ERR_NO_MEMORY;
					}
				}
#ifdef PAGED_MEDIA_SUPPORT
				else
				{
					if (info.paged_media == PAGEBREAK_ON && BreakForced(page_break_policy))
					{
						insert_break = page_break_policy;

						/* Break policies on the first line of boxes have been
						   propagated to the flex container, so we should never
						   find explicit breaks on the first line. */

						OP_ASSERT(prev_box);

						if (prev_box)
							/* Need to mark the break, so that future attempts
							   to insert implicit page breaks don't consider
							   anything preceding this item. */

							prev_box->AddTrailingPageBreak();
					}

					if (insert_break != BREAK_AVOID)
					{
						RETURN_IF_ERROR(BreakPage(info, item_box, NULL, y, insert_break, stretch));
						insert_break = BREAK_AVOID;
					}

					if (info.KeepPageBreaks())
					{
						if (item_box->FindPendingPageBreak())
						{
							/* Found (and reset) the pending page break mark. Resume
							   normal page breaking. */

							OP_ASSERT(info.paged_media == PAGEBREAK_FIND);
							info.paged_media = PAGEBREAK_ON;
						}

						if (item_box->HasTrailingPageBreak())
							insert_break = BREAK_ALLOW;
					}
					else
					{
						/* We're either looking for new page breaks, or page breaking
						   is disabled. Any page break here is just residue and no
						   longer valid. */

						item_box->RemoveTrailingPageBreak();

						if (info.paged_media == PAGEBREAK_ON)
							if (y + item_box->GetNewMainMarginSize() > info.doc->GetRelativePageBottom())
								// This item overflows the current page. Try to break.

								for (int strength = 0; strength <= 3; strength ++)
								{
									BREAKST status = InsertPageBreak(info, strength, HARD);

									if (status == BREAK_OUT_OF_MEMORY)
										return LAYOUT_OUT_OF_MEMORY;

									if (status == BREAK_FOUND)
										/* Pending page break inserted. Page breaking is
										   disabled for the rest of this reflow pass. */

										break;
								}
					}

					prev_box = item_box;
				}
#endif // PAGED_MEDIA_SUPPORT

				prev_page_break_policy = item_box->GetPageBreakPolicyAfter();
				prev_column_break_policy = item_box->GetColumnBreakPolicyAfter();
			}
		}
	}
	else
		/* Cannot fragment reverse line stacking, because of the mismatch
		   between logical and visual order. */

		if (!flex_packed.lines_reversed)
		{
			for (FlexLine* line = line_stack.First(); line; line = line->Suc())
			{
				LayoutCoord y = line->GetCrossPosition() + top_border_padding;
				BREAK_POLICY cur_column_break_policy;
				BREAK_POLICY cur_page_break_policy;
				BREAK_POLICY next_column_break_policy;
				BREAK_POLICY next_page_break_policy;

				line->GetBreakPolicies(cur_column_break_policy, cur_page_break_policy, next_column_break_policy, next_page_break_policy);

				BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, cur_column_break_policy);
				BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, cur_page_break_policy);

				if (cascade->multipane_container)
				{
					if (BreakForced(page_break_policy) || BreakForced(column_break_policy))
					{
						BREAK_TYPE break_type =
#ifdef PAGED_MEDIA_SUPPORT
							BreakForced(page_break_policy) ? PAGE_BREAK :
#endif // PAGED_MEDIA_SUPPORT
							COLUMN_BREAK;

						if (!placeholder->PropagateBreakpoint(y, break_type, NULL))
							return OpStatus::ERR_NO_MEMORY;
					}
				}
#ifdef PAGED_MEDIA_SUPPORT
				else
				{
					FlexItemBox* start_box = line->GetStartItem();
					FlexItemBox* next_line_box = line->GetNextStartItem();

					if (info.paged_media == PAGEBREAK_ON && BreakForced(page_break_policy))
					{
						insert_break = page_break_policy;

						/* Break policies on the first line of boxes have been
						   propagated to the flex container, so we should never
						   find explicit breaks on the first line. */

						OP_ASSERT(prev_box);

						if (prev_box)
							/* Need to mark the break, so that future attempts
							   to insert implicit page breaks don't consider
							   anything preceding this item. */

							prev_box->AddTrailingPageBreak();
					}

					if (insert_break != BREAK_AVOID)
					{
						RETURN_IF_ERROR(BreakPage(info, start_box, line, y, insert_break, stretch));
						insert_break = BREAK_AVOID;
					}

					for (FlexItemBox* item_box = start_box; item_box != next_line_box; item_box = item_box->Suc(FALSE))
						if (info.paged_media == PAGEBREAK_FIND)
						{
							if (item_box->FindPendingPageBreak())
								/* Found (and reset) the pending page break mark. Resume
								   normal page breaking. */

								info.paged_media = PAGEBREAK_ON;

							if (item_box->HasTrailingPageBreak())
								insert_break = BREAK_ALLOW;
						}
						else
						{
							/* We're either looking for new page breaks, or page breaking
							   is disabled. Any page break here is just residue and no
							   longer valid. */

							item_box->RemoveTrailingPageBreak();

							if (info.paged_media == PAGEBREAK_ON)
								if (y + line->GetCrossSize() > info.doc->GetRelativePageBottom())
									// This item overflows the current page. Try to break.

									for (int strength = 0; strength <= 3; strength ++)
									{
										BREAKST status = InsertPageBreak(info, strength, HARD);

										if (status == BREAK_OUT_OF_MEMORY)
											return LAYOUT_OUT_OF_MEMORY;

										if (status == BREAK_FOUND)
											/* Pending page break inserted. Page breaking is
											   disabled for the rest of this reflow pass. */

											break;
									}
						}

					prev_box = start_box;
				}
#endif// PAGED_MEDIA_SUPPORT

				prev_page_break_policy = next_page_break_policy;
				prev_column_break_policy = next_column_break_policy;
			}
		}

	return OpStatus::OK;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Insert page break before the specified item. */

OP_STATUS
FlexContent::BreakPage(LayoutInfo& info, FlexItemBox* break_before_item, FlexLine* break_before_line, LayoutCoord virtual_y, BREAK_POLICY page_break, LayoutCoord& stretch)
{
	LayoutCoord offset(0);

	OP_ASSERT(info.paged_media != PAGEBREAK_OFF);

	do
	{
		PageDescription* next_page_description = info.doc->AdvancePage(virtual_y);

		OP_ASSERT(info.doc->GetCurrentPage());

		if (!next_page_description)
			return OpStatus::ERR_NO_MEMORY;

		LayoutCoord page_top(info.doc->GetRelativePageTop());

		offset += page_top - virtual_y;
		virtual_y = page_top;

		if (page_break == BREAK_LEFT)
		{
			if (next_page_description->GetNumber() % 2 == 1)
				continue; // Wrong side - need a blank page
		}
		else
			if (page_break == BREAK_RIGHT)
			{
				if (next_page_description->GetNumber() % 2 == 0)
					continue; // Wrong side - need a blank page
			}

		break;
	}
	while (TRUE);

	if (offset > 0)
	{
		// Page break moved items and lines further down.

		for (FlexLine* line = break_before_line; line; line = line->Suc())
			// Only for horizontal flexboxes

			line->SetCrossPosition(line->GetCrossPosition() + offset);

		for (FlexItemBox* item_box = break_before_item; item_box; item_box = item_box->Suc(FALSE))
			item_box->MoveNewStaticMarginY(this, offset);

		stretch += offset;
	}

	return OpStatus::OK;
}

#endif // PAGED_MEDIA_SUPPORT

/** Calculate baseline. */

LayoutCoord
FlexContent::CalculateBaseline() const
{
	if (FlexLine* line = line_stack.First())
	{
		FlexItemBox* start_box = line->GetStartItem();

		if (!IsVertical())
		{
			/* The items' main axis is parallel with the inline axis, so we
			   can do proper baseline alignment, if there's a
			   baseline-aligned item on the first line. */

			FlexItemBox* end_box = line->GetNextStartItem();

			for (FlexItemBox* item_box = start_box; item_box != end_box; item_box = item_box->Suc(FALSE))
			{
				if (!item_box)
				{
					// Band-aid fix for crash CORE-48636.

					OP_ASSERT(!"Inspecting lines while an item is not in the layout stack?");
					break;
				}

				if (item_box->GetFlexboxModes().GetAlignSelf() == FlexboxModes::ASELF_BASELINE)
				{
					LayoutCoord baseline = item_box->GetItemBaseline();

					if (baseline != LAYOUT_COORD_MIN)
						// Found one. That's our baseline then.

						return item_box->GetY() + baseline;
				}
			}
		}

		/* No baseline aligned items on the first flex line, or the main
		   axis isn't horizontal. Then examine the first item and see it
		   has a baseline at all. */

		if (start_box)
		{
			LayoutCoord y = start_box->GetY();
			LayoutCoord baseline = start_box->GetItemBaseline();

			if (baseline != LAYOUT_COORD_MIN)
				// Yes, the first item has a baseline. Use it.

				return y + baseline;
			else
				/* The first item has no baseline. Use its height then. We should
				   ideally use the content box bottom, but in order to do that, we
				   would need the cascade. */

				return y + start_box->GetHeight();
		}
	}

	// Failed to find a baseline. Leave the problem to the caller.

	return LAYOUT_COORD_MIN;
}

/** Get baseline of flexbox. */

/* virtual */ LayoutCoord
FlexContent::GetBaseline(BOOL last_line) const
{
	LayoutCoord baseline = CalculateBaseline();

	if (baseline == LAYOUT_COORD_MIN)
		/* No baseline found, presumably because there are no items in the
		   flexbox. Use flexbox height then. We should ideally use the content
		   box bottom, but in order to do that, we would need the cascade. */

		return height;

	return baseline;
}

/** Get baseline of inline-flexbox. */

/* virtual */ LayoutCoord
FlexContent::GetBaseline(const HTMLayoutProperties& props) const
{
	OP_ASSERT(placeholder->IsInlineBlockBox());

	LayoutCoord baseline = CalculateBaseline();

	return baseline != LAYOUT_COORD_MIN ? baseline : height - LayoutCoord(props.border.bottom.width) - props.padding_bottom;
}

/** Get the baseline if maximum width is satisfied. */

/* virtual */ LayoutCoord
FlexContent::GetMinBaseline(const HTMLayoutProperties& props) const
{
	// Room for improvement here, but good enough for now.

	return min_height;
}

/** Calculate need for scrollbars (if this is a scrollable flexbox). */

/* virtual */ void
ScrollableFlexContent::CalculateScrollbars(LayoutInfo& info, LayoutProperties* cascade)
{
	if (flex_packed.needs_reflow)
		/* There'll be another reflow pass, so wait before we enable auto scrollbars,
		   since content may shrink in the next pass, and thus remove the need for
		   them. */

		PreventAutoScrollbars();

	// Add or remove scrollbars as appropriate.

	DetermineScrollbars(info, *cascade->GetProps(), width, height);
}

/** Get extra width that should be added to min/max widths. */

/* virtual */ LayoutCoord
ScrollableFlexContent::GetExtraMinMaxWidth(LayoutProperties* cascade) const
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (props.overflow_y == CSS_VALUE_scroll || props.overflow_y == CSS_VALUE_auto)
		// This is very simplistic. May have to become smarter about 'auto' in the future.

		return LayoutCoord(vertical_scrollbar->GetInfo()->GetVerticalScrollbarWidth());

	return LayoutCoord(0);
}

/** Get extra height that should be added to minimum height. */

/* virtual */ LayoutCoord
ScrollableFlexContent::GetExtraMinHeight(LayoutProperties* cascade) const
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (props.overflow_x == CSS_VALUE_scroll || props.overflow_y == CSS_VALUE_auto)
		// This is very simplistic. May have to become smarter about 'auto' in the future.

		return GetHorizontalScrollbarWidth();

	return LayoutCoord(0);
}

/** Get min/max width. */

/* virtual */ BOOL
FlexContent::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	min_width = minimum_width;
	normal_min_width = minimum_width;
	max_width = maximum_width;

	return TRUE;
}

/** Clear min/max width. */

/* virtual */ void
FlexContent::ClearMinMaxWidth()
{
	flex_packed.content_uptodate = 0;
	minimum_width = LayoutCoord(0);
	maximum_width = LayoutCoord(0);
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
FlexContent::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	FlexboxModes modes = props.flexbox_modes;
	FlexboxModes::Direction dir = modes.GetDirection();

#ifdef WEBKIT_OLD_FLEXBOX
	flex_packed.oldspec_flexbox = props.GetIsOldspecFlexbox();
#endif // WEBKIT_OLD_FLEXBOX

	flex_packed.is_shrink_to_fit = props.IsShrinkToFit(cascade->html_element->Type()) || placeholder->IsInlineBox();

	flex_packed.relative_height = props.HeightIsRelative(placeholder->IsAbsolutePositionedBox()) ||
		cascade->flexbox && cascade->flexbox->IsVertical();

	flex_packed.is_vertical = dir == FlexboxModes::DIR_COLUMN || dir == FlexboxModes::DIR_COLUMN_REVERSE;
	flex_packed.items_reversed = 0;

	if (modes.GetDirection() == FlexboxModes::DIR_COLUMN_REVERSE)
		flex_packed.items_reversed = 1;
	else
#ifdef SUPPORT_TEXT_DIRECTION
		if (props.direction == CSS_VALUE_rtl)
			flex_packed.items_reversed = modes.GetDirection() == FlexboxModes::DIR_ROW;
		else
#endif // SUPPORT_TEXT_DIRECTION
			flex_packed.items_reversed = modes.GetDirection() == FlexboxModes::DIR_ROW_REVERSE;

#ifdef SUPPORT_TEXT_DIRECTION
	if (flex_packed.is_vertical && props.direction == CSS_VALUE_rtl)
		flex_packed.lines_reversed = modes.GetWrap() != FlexboxModes::WRAP_WRAP_REVERSE;
	else
#endif // SUPPORT_TEXT_DIRECTION
		flex_packed.lines_reversed = modes.GetWrap() == FlexboxModes::WRAP_WRAP_REVERSE;

	LayoutCoord old_width = width;

	if (cascade->flexbox)
		width = ((FlexItemBox*) placeholder)->GetFlexWidth(cascade);
	else
	{
		LayoutCoord content_width;
		LayoutCoord hor_border_padding =
			LayoutCoord(props.border.left.width) + LayoutCoord(props.border.right.width) +
			props.padding_left + props.padding_right;

		if (props.content_width >= 0)
		{
			content_width = props.content_width;

			if (props.box_sizing == CSS_VALUE_border_box)
				content_width -= hor_border_padding;
		}
		else
			if (props.WidthIsPercent())
			{
				content_width = props.ResolvePercentageWidth(placeholder->GetAvailableWidth(cascade));

				if (props.box_sizing == CSS_VALUE_border_box)
					content_width -= hor_border_padding;
			}
			else
			{
				LayoutCoord available = placeholder->GetAvailableWidth(cascade) -
					(props.margin_left + props.margin_right + hor_border_padding);

				if (flex_packed.is_shrink_to_fit)
				{
					LayoutCoord nonpercent_hor_border_padding = props.GetNonPercentHorizontalBorderPadding();
					LayoutCoord minimum_content_width = minimum_width - nonpercent_hor_border_padding;
					LayoutCoord maximum_content_width = maximum_width - nonpercent_hor_border_padding;

					if (minimum_content_width < 0) // may happen before min/max widths have been calculated.
						minimum_content_width = LayoutCoord(0);

					if (maximum_content_width < 0) // may happen before min/max widths have been calculated.
						maximum_content_width = LayoutCoord(0);

					content_width = MAX(minimum_content_width, MIN(available, maximum_content_width));
				}
				else
					content_width = available;
			}

		if (props.box_sizing == CSS_VALUE_border_box)
			width = props.CheckWidthBounds(MAX(LayoutCoord(0), content_width) + hor_border_padding);
		else
			width = props.CheckWidthBounds(content_width) + hor_border_padding;

		OP_ASSERT(width >= hor_border_padding);
	}

	if (width != old_width || flex_packed.relative_height)
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

/** Calculate flexbox border box height. */

LayoutCoord
FlexContent::CalculateHeight(LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord hor_scrollbar_height) const
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord ver_border_padding = props.padding_top + props.padding_bottom +
		LayoutCoord(props.border.top.width + props.border.bottom.width);
	LayoutCoord flex_height = CONTENT_HEIGHT_AUTO;

	if (cascade->flexbox)
		// Nested flexbox.

		flex_height = ((FlexItemBox*) placeholder)->GetFlexHeight(cascade);

	if (flex_height != CONTENT_HEIGHT_AUTO)
		content_height = flex_height - ver_border_padding;
	else
		if (reflow_state->css_height != CONTENT_HEIGHT_AUTO)
		{
			content_height = reflow_state->css_height;
			OP_ASSERT(content_height >= 0);

			if (props.box_sizing != CSS_VALUE_content_box)
				content_height -= ver_border_padding;
		}
		else
			content_height += hor_scrollbar_height;

	if (props.box_sizing == CSS_VALUE_border_box)
		return props.CheckHeightBounds(content_height + ver_border_padding);
	else
		return props.CheckHeightBounds(content_height) + ver_border_padding;
}

/** Lay out box. */

/* virtual */ LAYST
FlexContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;
	const HTMLayoutProperties& props = *cascade->GetProps();

	top_border_padding = LayoutCoord(props.border.top.width) + props.padding_top;

	LayoutCoord left_border_padding = LayoutCoord(props.border.left.width) + props.padding_left;
	LayoutCoord hor_border_padding = left_border_padding + LayoutCoord(props.border.right.width) + props.padding_right;
	LayoutCoord ver_border_padding = top_border_padding + LayoutCoord(props.border.bottom.width) + props.padding_bottom;
	BOOL vertical = IsVertical();
	BOOL allow_collapse = AllowVisibilityCollapse(info);

	OP_ASSERT(!first_child); // Meaningless situation.

	if (!reflow_state) // May have survived from a previous failed reflow pass.
	{
		reflow_state = OP_NEW(FlexContentReflowState, ());

		if (!reflow_state)
			return LAYOUT_OUT_OF_MEMORY;
	}

	reflow_state->css_height = CalculateCSSHeight(info, cascade);

	// Set up 'break-inside' policies.

	if (
#ifdef PAGED_MEDIA_SUPPORT
		info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
		cascade->multipane_container)
	{
		BOOL avoid_page_break_inside;
		BOOL avoid_column_break_inside;

		cascade->GetAvoidBreakInside(info, avoid_page_break_inside, avoid_column_break_inside);

		flex_packed.avoid_page_break_inside = !!avoid_page_break_inside;
		flex_packed.avoid_column_break_inside = !!avoid_column_break_inside;
	}

	// Find scrollbar widths.

	LayoutCoord hor_scrollbar_width = LayoutCoord(0);
	LayoutCoord ver_scrollbar_width = LayoutCoord(0);

	if (ScrollableArea* scrollable = GetScrollable())
	{
		hor_scrollbar_width = scrollable->GetHorizontalScrollbarWidth();
		ver_scrollbar_width = scrollable->GetVerticalScrollbarWidth();
	}

	// Calculate content box width.

	LayoutCoord content_width = width - hor_border_padding - ver_scrollbar_width;

	if (content_width < 0)
		content_width = LayoutCoord(0);

	LayoutCoord content_height = CONTENT_HEIGHT_AUTO;
	LayoutCoord max_content_height = LAYOUT_COORD_MAX;

	// Calculate maximum height.

	if (props.max_height >= 0)
	{
		max_content_height = props.max_height - hor_scrollbar_width;

		if (props.box_sizing == CSS_VALUE_border_box)
			max_content_height -= ver_border_padding;

		if (max_content_height < 0)
			max_content_height = LayoutCoord(0);
	}

	/* Calculate content box height. We may not be able to resolve it fully at
	   this point, in which case it will be left as auto until we have examined
	   the lines. */

	if (cascade->flexbox)
	{
		// Nested flexbox.

		content_height = ((FlexItemBox*) placeholder)->GetFlexHeight(cascade);

		if (content_height != CONTENT_HEIGHT_AUTO)
			content_height -= ver_border_padding;
	}

	if (content_height == CONTENT_HEIGHT_AUTO)
		if (reflow_state->css_height >= 0)
		{
			content_height = reflow_state->css_height;

			if (props.box_sizing == CSS_VALUE_border_box)
				content_height -= ver_border_padding;
		}

	if (content_height != CONTENT_HEIGHT_AUTO)
	{
		LayoutCoord min_content_height = props.min_height;

		if (props.box_sizing == CSS_VALUE_border_box)
		{
			min_content_height -= ver_border_padding;

			if (min_content_height < 0)
				min_content_height = LayoutCoord(0);
		}

		content_height -= hor_scrollbar_width;

		if (content_height > max_content_height)
			content_height = max_content_height;

		if (content_height < min_content_height)
			content_height = min_content_height;
	}

	// Set up main content size and restrictions.

	LayoutCoord main_content_size; // May be auto after this step, in which case it will be resolved later.
	LayoutCoord max_main_content_size = LAYOUT_COORD_MAX;

	if (vertical)
	{
		main_content_size = content_height;
		max_main_content_size = max_content_height;

		if (main_content_size != CONTENT_SIZE_AUTO && max_main_content_size > main_content_size)
			max_main_content_size = main_content_size;

		reflow_state->containing_block_size = content_height;

		/* If a vertical flexbox is multi-line, we can only tell where the
		   lines will wrap if the height is fixed. */

		if (props.flexbox_modes.GetWrap() == FlexboxModes::WRAP_NOWRAP ||
			!flex_packed.relative_height &&
			!props.GetMinHeightIsPercent() &&
			!props.GetMaxHeightIsPercent())
			reflow_state->max_line_main_size = max_main_content_size;
		else
			/* Since we have no clue about where lines might end, assume that
			   there won't be room for more than one item per line. This is the
			   only way to ensure that the flexbox becomes wide enough to
			   contain all the items. */

			reflow_state->max_line_main_size = LAYOUT_COORD_MIN;
	}
	else
	{
		main_content_size = content_width;
		max_main_content_size = content_width;
		reflow_state->containing_block_size = content_width;
		reflow_state->max_line_main_size = max_main_content_size;
	}

	reflow_state->items_bottom = LayoutCoord(0);
	reflow_state->cur_minimum_width = LayoutCoord(0);
	reflow_state->cur_maximum_width = LayoutCoord(0);
	reflow_state->line.Reset();
	reflow_state->hypothetical_item_height_changed = FALSE;
	reflow_state->has_collapsed_items = FALSE;

	if (!reflow_state->list_item_state.Init(info, html_element))
		return LAYOUT_OUT_OF_MEMORY;

	line_stack.Clear();

	if (!flex_packed.content_uptodate)
		min_height = LayoutCoord(0); // will be recalculated now.

	FlexLine* line = NULL;
	LayoutCoord lines_cross_size_used = LayoutCoord(0);
	LayoutCoord lines_max_cross_size_diff = LayoutCoord(0);
	int line_count = 0;
	FlexboxModes modes = props.flexbox_modes;
	BOOL wrappable = modes.GetWrap() != FlexboxModes::WRAP_NOWRAP;
	BOOL has_pending_stf_items = FALSE;
	FlexboxModes::AlignContent align_content = modes.GetAlignContent();
	FlexItemBox* first_skipped_abspos_item = NULL;

	/* If content isn't "up-to-date" (because of DOM changes or dirtiness), it
	   is dangerous to store pointers to flex items (in FlexLine objects),
	   since flex item objects may be deleted while reflowing this flex
	   container. So skip the following code block in that case. */

	if (flex_packed.content_uptodate)
		/* Prepare for flexing. Divide into lines. Reset previous flexing. Count
		   number of auto margins per line and find total hypothetical sizes. Set
		   tentative cross size of each item (may have to be modified again later
		   if stretched or if shrink-to-fit has to be delayed until all line sizes
		   are known). */

		for (FlexItemBox* item_box = layout_stack.First(FALSE); item_box; item_box = item_box->Suc(FALSE))
		{
			LayoutCoord hypothetical_size;
			LayoutCoord base_size;

			if (item_box->IsAbsolutePositionedSpecialItem())
			{
				/* Skip past absolutely positioned items.

				   Also try to avoid putting them at the end of a line. This has to
				   do with how their static position is calculated; the first thing
				   we try is to look at the next in-flow item on the line, so there
				   better be one, or things will look stupider than necessary. */

				if (!first_skipped_abspos_item)
					// This may be a start item candidate for the next line.

					first_skipped_abspos_item = item_box;

				continue;
			}

			if (allow_collapse && item_box->IsVisibilityCollapse())
				hypothetical_size = base_size = LayoutCoord(0);
			else
			{
				base_size = item_box->GetFlexBaseSize(reflow_state->containing_block_size);
				hypothetical_size = item_box->GetConstrainedMainSize(base_size);
			}

			if (wrappable && line && line->GetMainHypotheticalSize() + hypothetical_size > max_main_content_size)
			{
				// The current line is full. Need a new one. It will start with this item.

				line->Finish();
				lines_cross_size_used += line->GetCrossSize();
				lines_max_cross_size_diff += line->GetMaxCrossSizeDiff();
				line = NULL;
			}

			if (!line)
			{
				line = OP_NEW(FlexLine, (first_skipped_abspos_item ? first_skipped_abspos_item : item_box));

				if (!line)
					return LAYOUT_OUT_OF_MEMORY;

				line->Into(&line_stack);
				line_count ++;

				if (!line->Init())
					return LAYOUT_OUT_OF_MEMORY;
			}

			first_skipped_abspos_item = NULL;

			LayoutCoord cross_size;
			int auto_margin_count = 0;

			if (vertical)
			{
				LayoutCoord hor_margins = item_box->GetMarginLeft() + item_box->GetMarginRight();

				cross_size = item_box->GetComputedCrossSize();

				if (cross_size <= CONTENT_WIDTH_SPECIAL)
				{
					LayoutCoord min_width;
					LayoutCoord max_width;
					LayoutCoord dummy;

					item_box->GetMinMaxWidth(dummy, min_width, max_width);
					min_width += hor_margins;
					max_width += hor_margins;

					if (wrappable)
					{
						/* Line width is unknown here, so we cannot do shrink-to-fit
						   yet. Need to wait until all lines are finished and
						   stretched. Record maximum cross size. */

						cross_size = min_width;
						line->PropagateMaxCrossSize(max_width);
						has_pending_stf_items = TRUE;
					}
					else
						cross_size = MAX(min_width, MIN(content_width, max_width));
				}
				else
				{
					if (cross_size < 0)
						cross_size = PercentageToLayoutCoord(LayoutFixed(-cross_size), content_width);

					if (item_box->GetBoxSizing() == CSS_VALUE_content_box)
						cross_size += item_box->GetCrossBorderPadding();

					cross_size += hor_margins;
				}

				cross_size = item_box->GetConstrainedCrossSize(cross_size);
				item_box->SetNewCrossMarginSize(cross_size);

				if (item_box->IsMarginTopAuto())
					auto_margin_count ++;

				if (item_box->IsMarginBottomAuto())
					auto_margin_count ++;
			}
			else
			{
				cross_size = item_box->GetHypotheticalMarginBoxHeight();
				item_box->SetNewCrossMarginSize(cross_size);

				if (item_box->GetFlexboxModes().GetAlignSelf() == FlexboxModes::ASELF_BASELINE &&
					!item_box->IsMarginTopAuto() && !item_box->IsMarginBottomAuto())
				{
					LayoutCoord baseline = item_box->GetItemBaseline();

					if (baseline != LAYOUT_COORD_MIN)
					{
						LayoutCoord above_baseline = baseline + item_box->GetMarginTop();
						LayoutCoord below_baseline = cross_size - above_baseline;

						line->AddBaseline(above_baseline, below_baseline);
						cross_size = LayoutCoord(0); // baseline info is used to determine cross size
					}
				}

				if (item_box->IsMarginLeftAuto())
					auto_margin_count ++;

				if (item_box->IsMarginRightAuto())
					auto_margin_count ++;
			}

			line->AddItem(base_size, hypothetical_size, cross_size, auto_margin_count);
			item_box->SetViolation(0);
		}

	if (line)
	{
		// Finish the last line.

		line->Finish();

		if (!line->Pred())
		{
			/* Single line; either because we were not allowed to wrap, or
			   because it wasn't necessary to do so. Then this line should be
			   stretched to take all available cross space, no matter what
			   'align-content' says. */

			align_content = FlexboxModes::ACONTENT_STRETCH;

			/* Also, the cross-size of a line in a single-line flexbox must be the
			   same as the cross-size of the flex container. Perform the actual
			   line stretching or shrinking now, if cross size is definite. */

			if (vertical)
				line->SetCrossSize(content_width);
			else
				if (content_height != CONTENT_HEIGHT_AUTO)
					line->SetCrossSize(MIN(content_height, max_content_height));
				else
					if (line->GetCrossSize() > max_content_height)
						line->SetCrossSize(max_content_height);
		}

		lines_cross_size_used += line->GetCrossSize();
		lines_max_cross_size_diff += line->GetMaxCrossSizeDiff();
	}

	/* Resolve and constrain content box height (and thus main content size, if
	   previously unresolved). */

	if (content_height == CONTENT_HEIGHT_AUTO)
		if (vertical)
			if (line)
				if (line->Pred())
					// Wrapped. Max-height must have been reached.

					content_height = max_main_content_size;
				else
					content_height = line->GetMainHypotheticalSize();
			else
				content_height = LayoutCoord(0);
		else
			content_height = lines_cross_size_used;

	// Finally we can set the actual border box height.

	height = CalculateHeight(info, cascade, content_height, hor_scrollbar_width);
	content_height = height - ver_border_padding - hor_scrollbar_width;

	if (content_height < 0)
		content_height = LayoutCoord(0);

	reflow_state->content_width = content_width;
	reflow_state->content_height = content_height;

	LayoutCoord lines_extra_space; // Extra cross space for lines
	LayoutCoord unpacked_line_cross_pos(0); // Unpacked position of the current line being processed
	LayoutCoord extra_space_distributed(0); // Amount extra space distributed so far (only used for stretching)
	int line_no;
	BOOL lines_reversed = IsLineOrderReversed(); // Line progression direction is RTL or BTT.
	BOOL items_reversed = IsItemOrderReversed(); // Item progression direction is RTL or BTT.

	// Figure out how much extra cross space is left for the lines.

	if (vertical)
	{
		main_content_size = content_height;
		lines_extra_space = content_width - lines_cross_size_used;
	}
	else
		lines_extra_space = content_height - lines_cross_size_used;

	if (has_pending_stf_items)
	{
		/* This is a multi-line vertical flexbox and at least one item needed
		   the line width before it could calculate its own width. */

		OP_ASSERT(vertical && wrappable);

		if (lines_max_cross_size_diff > 0 && lines_extra_space > 0)
		{
			/* There is space available to grow the lines, to make the pending
			   shrink-to-fit items fit better. */

			LayoutCoord total_extra_space(lines_max_cross_size_diff);
			LayoutCoord diff_processed(0); // Avoid rounding errors.
			LayoutCoord extra_space_processed(0);

			if (total_extra_space > lines_extra_space)
				total_extra_space = lines_extra_space;

			for (line = line_stack.First(); line; line = line->Suc())
			{
				diff_processed += line->GetMaxCrossSizeDiff();
				LayoutCoord next_extra_space = total_extra_space * diff_processed / lines_max_cross_size_diff;

				line->SetCrossSize(line->GetCrossSize() + next_extra_space - extra_space_processed);
				extra_space_processed = next_extra_space;
			}

			// Adjust remaining extra cross space for 'align-content' to distribute.

			lines_extra_space -= total_extra_space;
			OP_ASSERT(lines_extra_space >= 0);
		}

		// Then apply shrink-to-fit to the items that still need it.

		for (line = line_stack.First(); line; line = line->Suc())
		{
			LayoutCoord line_width = line->GetCrossSize();
			FlexItemBox* end_item_box = line->GetNextStartItem();

			for (FlexItemBox* item_box = line->GetStartItem(); item_box != end_item_box; item_box = item_box->Suc(FALSE))
				if (item_box->GetComputedCrossSize() <= CONTENT_WIDTH_SPECIAL)
				{
					// This is one such item. Set correct width.

					LayoutCoord hor_margins = item_box->GetMarginLeft() + item_box->GetMarginRight();
					LayoutCoord min_width;
					LayoutCoord max_width;
					LayoutCoord dummy;

					item_box->GetMinMaxWidth(dummy, min_width, max_width);
					min_width += hor_margins;
					max_width += hor_margins;

					// Shrink-to-fit.

					item_box->SetNewCrossMarginSize(MAX(min_width, MIN(line_width, max_width)));
				}
		}
	}

	OP_ASSERT(main_content_size >= 0);

	for (line_no = 0, line = line_stack.First(); line; line_no ++, line = line->Suc())
	{
		// Arrange line and items along the cross axis. Stretch line and/or items as appropriate.

		LayoutCoord unstretched_size = line->GetCrossSize();

		switch (align_content)
		{
		case FlexboxModes::ACONTENT_STRETCH:
			if (lines_extra_space > 0)
			{
				// Also make sure that we distribute rounding errors.

				LayoutCoord new_extra_space_distributed = lines_extra_space * LayoutCoord(line_no + 1) / LayoutCoord(line_count);

				line->SetCrossPosition(unpacked_line_cross_pos + extra_space_distributed);
				line->SetCrossSize(unstretched_size + new_extra_space_distributed - extra_space_distributed);
				extra_space_distributed = new_extra_space_distributed;
				break;
			}

			// Fall-through; identical to 'flex-start' when there's no positive extra space.

		case FlexboxModes::ACONTENT_SPACE_BETWEEN:
			if (align_content == FlexboxModes::ACONTENT_SPACE_BETWEEN && line_no > 0 && lines_extra_space > 0)
			{
				line->SetCrossPosition(unpacked_line_cross_pos + lines_extra_space * LayoutCoord(line_no) / LayoutCoord(line_count - 1));
				break;
			}

			// Fall-through; identical to 'flex-start' when it's first on line or when there's no positive extra space.

		case FlexboxModes::ACONTENT_FLEX_START:
			line->SetCrossPosition(unpacked_line_cross_pos);
			break;

		case FlexboxModes::ACONTENT_FLEX_END:
			line->SetCrossPosition(unpacked_line_cross_pos + lines_extra_space);
			break;

		case FlexboxModes::ACONTENT_SPACE_AROUND:
			if (line_count > 1 && lines_extra_space > 0)
			{
				line->SetCrossPosition(unpacked_line_cross_pos + lines_extra_space * LayoutCoord((line_no * 2) + 1) / LayoutCoord(line_count * 2));
				break;
			}

			// identical to 'center' when there's only one line or when there's no positive extra space.

		case FlexboxModes::ACONTENT_CENTER:
			line->SetCrossPosition(unpacked_line_cross_pos + lines_extra_space / LayoutCoord(2));
			break;
		};

		unpacked_line_cross_pos += unstretched_size;

		FlexItemBox* start_item = line->GetStartItem();
		FlexItemBox* next_start_item = NULL;

		if (FlexLine* next_line = line->Suc())
			next_start_item = next_line->GetStartItem();

		// Set items' cross size and position.

		for (FlexItemBox* item_box = start_item; item_box != next_start_item; item_box = item_box->Suc(FALSE))
		{
			LayoutCoord extra_cross_space = line->GetCrossSize() - item_box->GetNewCrossMarginSize();
			FlexboxModes::AlignSelf align_self = item_box->GetFlexboxModes().GetAlignSelf();
			BOOL cross_start_margin_auto;
			BOOL cross_end_margin_auto;

			if (vertical)
				if (lines_reversed)
				{
					cross_start_margin_auto = item_box->IsMarginRightAuto();
					cross_end_margin_auto = item_box->IsMarginLeftAuto();
				}
				else
				{
					cross_start_margin_auto = item_box->IsMarginLeftAuto();
					cross_end_margin_auto = item_box->IsMarginRightAuto();
				}
			else
				if (lines_reversed)
				{
					cross_start_margin_auto = item_box->IsMarginBottomAuto();
					cross_end_margin_auto = item_box->IsMarginTopAuto();
				}
				else
				{
					cross_start_margin_auto = item_box->IsMarginTopAuto();
					cross_end_margin_auto = item_box->IsMarginBottomAuto();
				}

			if (cross_start_margin_auto || cross_end_margin_auto)
				if (extra_cross_space > 0)
					// Map to new alignment values based on auto margins.

					if (cross_start_margin_auto)
						if (cross_end_margin_auto)
							align_self = FlexboxModes::ASELF_CENTER;
						else
							align_self = FlexboxModes::ASELF_FLEX_END;
					else
						align_self = FlexboxModes::ASELF_FLEX_START;

			LayoutCoord cross_offset(0);

			switch (align_self)
			{
			default:
				OP_ASSERT(!"Unexpected value");

			case FlexboxModes::ASELF_STRETCH:
				if (item_box->AllowStretch() && extra_cross_space)
				{
					LayoutCoord old_size = item_box->GetNewCrossMarginSize();
					LayoutCoord stretched_size = item_box->GetConstrainedCrossSize(old_size + extra_cross_space);

					item_box->SetNewCrossMarginSize(stretched_size);
				}
				break;

			case FlexboxModes::ASELF_FLEX_START:
				break;

			case FlexboxModes::ASELF_BASELINE:
				if (!vertical && !item_box->IsMarginTopAuto() && !item_box->IsMarginBottomAuto())
				{
					LayoutCoord baseline = item_box->GetItemBaseline();

					if (baseline != LAYOUT_COORD_MIN)
					{
						cross_offset = line->GetBaseline() - baseline - item_box->GetMarginTop();

						if (lines_reversed)
							// The bottom margin edge of the item is the cross-start edge.

							cross_offset = line->GetCrossSize() - cross_offset - item_box->GetNewCrossMarginSize();
					}
				}

				break;

			case FlexboxModes::ASELF_FLEX_END:
				cross_offset = extra_cross_space;
				break;

			case FlexboxModes::ASELF_CENTER:
				cross_offset = extra_cross_space / LayoutCoord(2);
				break;
			};

			item_box->SetNewCrossMarginEdge(line->GetCrossPosition() + cross_offset);
		}

		// Arrange items along the main axis. Flex and justify.

		LayoutCoord extra_space = main_content_size - line->GetMainBaseSize();

		// The free space that we had initially is what determines whether to grow or shrink.

		LayoutCoord initial_extra_space(extra_space);

		// First: flex (change box sizes)

		int abspos_item_count = 0;
		LayoutCoord main_position(0);
		LayoutCoord violation(0);

		do
		{
			/* One or several passes. If flexing violates min/max-width/height
			   constraints on any of the items, perform another pass, to redistribute
			   the extra space on the items that didn't get their constraints
			   violated. Repeat until everyone's happy. If no violation occurs, on
			   the other hand, there will only be one pass. */

			float total_flex = 0.0;

			for (FlexItemBox* item_box = start_item; item_box != next_start_item; item_box = item_box->Suc(FALSE))
				if (!item_box->IsViolated(violation) &&
					(!allow_collapse || !item_box->IsVisibilityCollapse()) &&
					!item_box->IsAbsolutePositionedSpecialItem())
					if (initial_extra_space > 0)
						total_flex += item_box->GetFlexGrow();
					else
						/* Shrinking is done with respect to the items' base
						   size. Larger items will shrink more than smaller
						   items. Because so says the spec. */

						total_flex += item_box->GetFlexShrink() * float(item_box->GetFlexBaseSize(reflow_state->containing_block_size));

			LayoutCoord frozen_space_distributed(0);
			LayoutCoord prev_violation(violation);
			LayoutCoord space_flexed(0); // Total amount of space flexed so far.
			float flex_processed = 0.0; // Total amount of flexibility processed so far.

			main_position = LayoutCoord(0);
			violation = LayoutCoord(0);

			for (FlexItemBox* item_box = start_item; item_box != next_start_item; item_box = item_box->Suc(FALSE))
			{
				item_box->SetNewMainMarginEdge(main_position);

				if (allow_collapse && item_box->IsVisibilityCollapse())
					continue;

				if (item_box->IsAbsolutePositionedSpecialItem())
				{
					item_box->SetNewMainMarginSize(LayoutCoord(0));
					abspos_item_count ++;
					continue;
				}

				if (!item_box->IsViolated(prev_violation))
				{
					LayoutCoord base_size = item_box->GetFlexBaseSize(reflow_state->containing_block_size);
					LayoutCoord item_size = base_size;

					if (extra_space && total_flex > 0)
					{
						/* Stretch or shrink the item according to its flexibility and
						   extra space. Also be sure to distribute rounding errors, to
						   avoid undesired unused space or overflow at the end of the
						   line. */

						if (initial_extra_space > 0)
							flex_processed += item_box->GetFlexGrow();
						else
							/* Shrinking is done with respect to the items' base
							   size. Larger items will shrink more than smaller
							   items. Because so says the spec. */

							flex_processed += item_box->GetFlexShrink() * float(item_box->GetFlexBaseSize(reflow_state->containing_block_size));

						LayoutCoord new_space_flexed = LayoutCoord(int(extra_space * flex_processed / total_flex));

						item_size += new_space_flexed - space_flexed;
						space_flexed = new_space_flexed;
					}

					LayoutCoord constrained_size = item_box->GetConstrainedMainSize(item_size);

					item_box->SetNewMainMarginSize(constrained_size);

					// Check for violation

					if (constrained_size != item_size)
					{
						LayoutCoord diff = constrained_size - item_size;

						item_box->SetViolation(diff);
						violation += diff;
						frozen_space_distributed += constrained_size - base_size;
					}
				}

				main_position += item_box->GetNewMainMarginSize();
			}

			if (violation)
				extra_space -= frozen_space_distributed;
		}
		while (violation && extra_space);

		extra_space = main_content_size - main_position;

		if (extra_space)
		{
			// Still extra space after flexing.

			int auto_margin_count = line->GetAutoMarginCount();

			if (auto_margin_count && extra_space > 0)
			{
				// Give all extra space to auto margins.

				LayoutCoord extra_pos(0);
				int cur_margin = 0;

				for (FlexItemBox* item_box = start_item; item_box != next_start_item; item_box = item_box->Suc(FALSE))
				{
					if (allow_collapse && item_box->IsVisibilityCollapse())
						continue;

					BOOL main_start_margin_auto;
					BOOL main_end_margin_auto;

					if (vertical)
						if (items_reversed)
						{
							main_start_margin_auto = item_box->IsMarginBottomAuto();
							main_end_margin_auto = item_box->IsMarginTopAuto();
						}
						else
						{
							main_start_margin_auto = item_box->IsMarginTopAuto();
							main_end_margin_auto = item_box->IsMarginBottomAuto();
						}
					else
						if (items_reversed)
						{
							main_start_margin_auto = item_box->IsMarginRightAuto();
							main_end_margin_auto = item_box->IsMarginLeftAuto();
						}
						else
						{
							main_start_margin_auto = item_box->IsMarginLeftAuto();
							main_end_margin_auto = item_box->IsMarginRightAuto();
						}

					if (main_start_margin_auto)
					{
						cur_margin ++;
						extra_pos = extra_space * LayoutCoord(cur_margin) / LayoutCoord(auto_margin_count);
					}

					item_box->SetNewMainMarginEdge(item_box->GetNewMainMarginEdge() + extra_pos);

					if (main_end_margin_auto)
					{
						cur_margin ++;
						extra_pos = extra_space * LayoutCoord(cur_margin) / LayoutCoord(auto_margin_count);
					}
				}
			}
			else
			{
				// Otherwise, justify (change gaps between boxes)

				int item_no = 0;
				int item_count = line->GetItemCount();

				for (FlexItemBox* item_box = start_item; item_box != next_start_item; item_box = item_box->Suc(FALSE))
				{
					if (allow_collapse && item_box->IsVisibilityCollapse() ||
						abspos_item_count && item_box->IsAbsolutePositionedSpecialItem())
						continue;

					switch (modes.GetJustifyContent())
					{
					case FlexboxModes::JCONTENT_SPACE_BETWEEN:
						if (item_count > 1 && extra_space > 0)
						{
							item_box->SetNewMainMarginEdge(item_box->GetNewMainMarginEdge() + extra_space * LayoutCoord(item_no) / LayoutCoord(item_count - 1));
							break;
						}

						// Otherwise treat as 'flex-start' (fall-through)

					case FlexboxModes::JCONTENT_FLEX_START:
						break;

					case FlexboxModes::JCONTENT_FLEX_END:
						item_box->SetNewMainMarginEdge(item_box->GetNewMainMarginEdge() + extra_space);
						break;

					case FlexboxModes::JCONTENT_SPACE_AROUND:
						if (item_count > 1 && extra_space > 0)
						{
							item_box->SetNewMainMarginEdge(item_box->GetNewMainMarginEdge() + extra_space * LayoutCoord((item_no * 2 + 1)) / LayoutCoord(item_count * 2));
							break;
						}

						// Otherwise treat as 'center' (fall-through)

					case FlexboxModes::JCONTENT_CENTER:
						item_box->SetNewMainMarginEdge(item_box->GetNewMainMarginEdge() + extra_space / LayoutCoord(2));
						break;
					}

					item_no ++;
				}
			}
		}

		if (abspos_item_count)
		{
			/* Position special anonymous items for absolutely positioned
			   boxes. If an absolutely positioned "item" has auto position, it
			   will use this to find its "static" position. */

			FlexItemBox* first_pending_abspos_item = NULL;
			LayoutCoord last_in_flow_main_end(LAYOUT_COORD_MIN);
			LayoutCoord cross_position = line->GetCrossPosition();

			for (FlexItemBox* item_box = start_item; item_box != next_start_item; item_box = item_box->Suc(FALSE))
			{
				if (!item_box->IsAbsolutePositionedSpecialItem())
				{
					/* Found an in-flow item. Copy its main position to any
					   preceding absolutely positioned items. */

					for (FlexItemBox* abspos_item = first_pending_abspos_item;
						 abspos_item && abspos_item != item_box;
						 abspos_item = abspos_item->Suc(FALSE))
					{
						abspos_item->SetNewMainMarginEdge(item_box->GetNewMainMarginEdge());
						abspos_item->SetNewCrossMarginEdge(cross_position);
					}

					/* Remember the main-end edge of this one. It will be used
					   to position any absolutely positioned items that are
					   last on line. */

					last_in_flow_main_end = item_box->GetNewMainMarginEdge() + item_box->GetNewMainMarginSize();
					first_pending_abspos_item = NULL;
				}
				else
					if (!first_pending_abspos_item)
						first_pending_abspos_item = item_box;
			}

			if (first_pending_abspos_item)
			{
				// Trailing absolutely positioned items on line.

				LayoutCoord main_position = last_in_flow_main_end;

				if (main_position == LAYOUT_COORD_MIN)
				{
					/* No in-flow items on this line. Pretend that the abspos
					   items are 0x0 items and apply 'justify-content'. */

					switch (modes.GetJustifyContent())
					{
					default:
						main_position = LayoutCoord(0);
						break;

					case FlexboxModes::JCONTENT_FLEX_END:
						main_position = main_content_size;
						break;

					case FlexboxModes::JCONTENT_SPACE_AROUND:
					case FlexboxModes::JCONTENT_CENTER:
						main_position = main_content_size / LayoutCoord(2);
						break;
					}
				}

				for (FlexItemBox* abspos_item = first_pending_abspos_item;
					 abspos_item;
					 abspos_item = abspos_item->Suc(FALSE))
				{
					abspos_item->SetNewMainMarginEdge(main_position);
					abspos_item->SetNewCrossMarginEdge(cross_position);
				}
			}
		}
	}

	if (cascade->multipane_container
#ifdef PAGED_MEDIA_SUPPORT
		|| CSS_IsPagedMedium(info.doc->GetMediaType())
#endif // PAGED_MEDIA_SUPPORT
		)
	{
		LayoutCoord extra_space;

		if (OpStatus::IsMemoryError(FindBreaks(info, cascade, extra_space)))
			return LAYOUT_OUT_OF_MEMORY;

		/* Page breaking adds gaps between the end of content on one page and
		   the start of content on the next. This affects the final height of
		   the flexbox (and the position of the flex items (which has already
		   been dealt with), but it must never affect flexbox flexing (main
		   axis) or stretching (cross axis). */

		height += extra_space;
	}

	layout_stack.RemoveAll();

	return placeholder->LayoutChildren(cascade, info, first_child, start_position);
}

/** Update screen. */

/* virtual */ void
FlexContent::UpdateScreen(LayoutInfo& info)
{
}

/** Finish reflowing box. */

/* virtual */ LAYST
FlexContent::FinishLayout(LayoutInfo& info)
{
	if (!reflow_state)
	{
		// Reflow was skipped. Propagate previously calculated widths and margins and bounding box.

		placeholder->PropagateWidths(info, minimum_width, minimum_width, maximum_width);
		placeholder->PropagateBottomMargins(info);

		return LAYOUT_CONTINUE;
	}

	LayoutProperties* cascade = placeholder->GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();

	FinishLine();

	flex_packed.needs_reflow = 0;

	if (flex_packed.content_uptodate)
	{
		// Content was up to date in this reflow pass, but we might still need another pass.

		BOOL reflow;

		if (reflow_state->has_collapsed_items)
			/* If we were not allowed to collapse items in this pass, we need
			   another one. */

			reflow = !AllowVisibilityCollapse(info);
		else
			reflow = reflow_state->hypothetical_item_height_changed;

		if (reflow)
			if (info.external_layout_change)
				flex_packed.needs_reflow = 1;
			else
				if (flex_packed.additional_reflows_allowed)
				{
					flex_packed.needs_reflow = 1;
					flex_packed.additional_reflows_allowed --;
				}
				else
				{
					/* We are not allowed to add another reflow pass, but something with
					   the layout still hasn't settled. This is either caused by an
					   engine bug, or by an unanticipated situation so complex that the
					   number of reflows allowed for a flexbox needs to be increased. */

					// This DOES happen in some cases; see e.g. CORE-48907.

//					OP_ASSERT(!"Circular dependency failed");
				}
	}
	else
		flex_packed.needs_reflow = 1;

	if (info.external_layout_change)
		/* This reflow was triggered by a DOM/style/viewport size change. Allow
		   the next reflow pass to trigger up to two additional reflow passes,
		   then. This is required because flexboxes may sometimes need as many
		   as four (!) reflow passes, min/max width calculation, flexing, cross
		   size stretching, and presence of auto scrollbars. Auto scrollbars
		   may not be added as long as we have more reflow passes coming up, as
		   early passes may be done with unresolved min/max widths (which may
		   cause wider content than what's correct, so that we would
		   incorrectly add auto scrollbars (shrink-to-fit floats could
		   typically cause this)).

		   For horizontal flexboxes it typically goes like this:

		     Pass 1: Calculate items' hypothetical main sizes (width), to be
		     able to distribute flex.

		     Pass 2: With known hypothetical main sizes, we are ready to
		     correctly divide into lines. Use correctly flexed main sizes
		     (width) to calculate hypothetical cross sizes (height).

		     Pass 3: Stretch stretchable items (height) and lay out.

		   For vertical flexboxes it typically goes like this:

		     Pass 1: Calculate items' min/max cross sizes (width), to be able
		     to calculate hypothetical main sizes (height).

		     Pass 2: Flex using the base flex sizes found in the previous pass
			 (as one always does). Note that for regular containers these are
			 probably wrong at this point, since they were calculated using
			 tentative widths in the previous pass. Anyway, the hypothetical
			 cross sizes (widths) of the items can now be found, based on the
			 min/max widths and stretching policies. A layout pass with the
			 correct widths will give us the correct base main size (height),
			 for flexing, so that we can get it right in the next pass.

		     Pass 3: With known hypothetical main sizes, we are ready to
		     correctly divide into lines. Flex main sizes (height) and lay
		     out.

		   Pass 4: relayout with auto scrollbars applied. */

		flex_packed.additional_reflows_allowed = 2; // Allow up to 2 reflows after the next pass.

	if (flex_packed.needs_reflow)
		info.workplace->SetReflowElement(cascade->html_element);

	/* This is a good time to calculate the need for scrollbars, because now we
	   know if the flex algorithm requires another reflow pass, and auto
	   scrollbars should only be applied if it doesn't require that. */

	CalculateScrollbars(info, cascade);

	if (!flex_packed.content_uptodate)
	{
		// Calculate min/max width and minimum height.

		LayoutCoord hor_border_padding = props.GetNonPercentHorizontalBorderPadding();
		LayoutCoord ver_border_padding = props.GetNonPercentVerticalBorderPadding();
		LayoutCoord extra_minmax_width = GetExtraMinMaxWidth(cascade);

		// Convert from content-box to border-box values.

		minimum_width += hor_border_padding + extra_minmax_width;
		maximum_width += hor_border_padding + extra_minmax_width;

		if (props.content_height >= 0)
		{
			/* The calculated (auto) min_height isn't interesting, since we
			   have a fixed height. */

			min_height = props.content_height;

			if (props.box_sizing == CSS_VALUE_content_box)
				min_height += ver_border_padding;
		}
		else
			min_height += ver_border_padding;

		min_height += GetExtraMinHeight(cascade);

		// Constrain propagated widths and height to min-width, max-width, min-height and max-height.

		if (props.box_sizing == CSS_VALUE_content_box)
		{
			minimum_width = props.CheckWidthBounds(minimum_width - hor_border_padding, TRUE) + hor_border_padding;
			maximum_width = props.CheckWidthBounds(maximum_width - hor_border_padding, TRUE) + hor_border_padding;
			min_height = props.CheckHeightBounds(min_height - ver_border_padding, TRUE) + ver_border_padding;
		}
		else
		{
			minimum_width = props.CheckWidthBounds(minimum_width, TRUE);
			maximum_width = props.CheckWidthBounds(maximum_width, TRUE);
			min_height = props.CheckHeightBounds(min_height, TRUE);
		}
	}

	placeholder->PropagateWidths(info, minimum_width, minimum_width, maximum_width);

	if (cascade->flexbox)
	{
		// Nested flexbox. Propagate hypothetical border box height.

		LayoutCoord ver_border_padding = LayoutCoord(props.border.top.width + props.border.bottom.width) +
			props.padding_top + props.padding_bottom;
		LayoutCoord hor_scrollbar_width(0);

		if (ScrollableArea* scrollable = GetScrollable())
			hor_scrollbar_width = scrollable->GetHorizontalScrollbarWidth();

		LayoutCoord auto_height = reflow_state->items_bottom + hor_scrollbar_width;

		if (props.box_sizing == CSS_VALUE_border_box)
			auto_height += ver_border_padding;

		LayoutCoord hypothetical_height = reflow_state->css_height >= 0 ? reflow_state->css_height : auto_height;

		auto_height = props.CheckHeightBounds(auto_height);
		hypothetical_height = props.CheckHeightBounds(hypothetical_height);

		if (props.box_sizing != CSS_VALUE_border_box)
		{
			auto_height += ver_border_padding;
			hypothetical_height += ver_border_padding;
		}

		((FlexItemBox*) placeholder)->PropagateHeights(auto_height, hypothetical_height);
	}

#ifdef CSS_TRANSFORMS
	if (TransformContext *transform_context = placeholder->GetTransformContext())
		if (!placeholder->IsInlineBox())
			transform_context->Finish(info, cascade, width, height);
#endif // CSS_TRANSFORMS

	placeholder->PropagateBottomMargins(info);

	for (FlexLine* line = line_stack.First(); line; line = line->Suc())
		line->FinishLayout();

	OP_DELETE(reflow_state);
	reflow_state = NULL;

	flex_packed.content_uptodate = 1;

	return LAYOUT_CONTINUE;
}

/** Signal that content may have changed. */

/* virtual */ void
FlexContent::SignalChange(FramesDocument* doc)
{
	if (flex_packed.needs_reflow)
		placeholder->MarkDirty(doc, FALSE);
}

#ifdef PAGED_MEDIA_SUPPORT

/** Attempt to break page. */

/* virtual */ BREAKST
FlexContent::AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain)
{
	if (strength == 0 && flex_packed.avoid_page_break_inside)
		return BREAK_KEEP_LOOKING;

	LayoutCoord page_bottom(info.doc->GetRelativePageBottom());
	BREAK_POLICY prev_policy = BREAK_AVOID;

	if (flex_packed.is_vertical)
	{
		/* With the current design, a vertical flexbox can be fragmented only
		   if it is single-line. Furthermore, we cannot fragment reverse item
		   stacking, because of the mismatch between logical and visual
		   order. */

		if ((!line_stack.First() || !line_stack.First()->Suc()) && !flex_packed.items_reversed)
			for (FlexItemBox* item_box = layout_stack.Last(FALSE); item_box; item_box = item_box->Pred(FALSE))
			{
				if (item_box->HasTrailingPageBreak())
					/* Found a previously inserted break. Do not attempt to
					   insert anything before that one. */

					return BREAK_NOT_FOUND;

				LayoutCoord position = item_box->GetNewStaticMarginY(this);
				LayoutCoord height = item_box->GetNewMainMarginSize();
				BREAK_POLICY cur_policy = item_box->GetPageBreakPolicyAfter();

				if (position + height < page_bottom)
					// Found an item that ends on the current page.

					if (strength >= 2 || CombineBreakPolicies(prev_policy, cur_policy) != BREAK_AVOID)
					{
						// Found a break opportunity. Break after this item.

						item_box->AddTrailingImplicitPageBreak();
						info.workplace->SetPageBreakElement(info, GetHtmlElement());

						return BREAK_FOUND;
					}

				prev_policy = item_box->GetPageBreakPolicyBefore();
			}
	}
	else
		/* Cannot fragment reverse line stacking, because of the mismatch
		   between logical and visual order. */

		if (!flex_packed.lines_reversed)
			for (FlexLine* line = line_stack.Last(); line; line = line->Pred())
			{
				FlexItemBox* stop_box = line->GetNextStartItem();

				for (FlexItemBox* item_box = line->GetStartItem(); item_box != stop_box; item_box = item_box->Suc(FALSE))
					if (item_box->HasTrailingPageBreak())
						/* Found a previously inserted break. Do not attempt to
						   insert anything before that one. */

						return BREAK_NOT_FOUND;

				LayoutCoord position = line->GetCrossPosition() + top_border_padding;
				LayoutCoord height = line->GetCrossSize();
				BREAK_POLICY cur_policy;
				BREAK_POLICY break_before;
				BREAK_POLICY dummy;

				line->GetBreakPolicies(dummy, break_before, dummy, cur_policy);

				if (position + height < page_bottom)
					// Found an item that ends on the current page.

					if (strength >= 2 || CombineBreakPolicies(prev_policy, cur_policy) != BREAK_AVOID)
					{
						// Found a break opportunity. Break after this item.

						line->GetStartItem()->AddTrailingImplicitPageBreak();
						info.workplace->SetPageBreakElement(info, GetHtmlElement());

						return BREAK_FOUND;
					}

				prev_policy = break_before;
			}

	return BREAK_KEEP_LOOKING;
}

#endif // PAGED_MEDIA_SUPPORT

/** Combine the specified break policies with child break policies that should be propagated to their parent. */

/* virtual */ void
FlexContent::CombineChildBreakProperties(BREAK_POLICY& page_break_before,
										 BREAK_POLICY& column_break_before,
										 BREAK_POLICY& page_break_after,
										 BREAK_POLICY& column_break_after)
{
	if (flex_packed.is_vertical)
	{
		if (FlexItemBox* first_child = layout_stack.First(FALSE))
		{
			FlexItemBox* last_child = layout_stack.Last(FALSE);

			page_break_before = CombineBreakPolicies(page_break_before, first_child->StealPageBreakPolicyBefore());
			column_break_before = CombineBreakPolicies(column_break_before, first_child->StealColumnBreakPolicyBefore());
			page_break_after = CombineBreakPolicies(page_break_after, last_child->StealPageBreakPolicyAfter());
			column_break_after = CombineBreakPolicies(column_break_after, last_child->StealColumnBreakPolicyAfter());
		}
	}
	else
		if (FlexLine* first_line = line_stack.First())
		{
			FlexLine* last_line = line_stack.Last();
			FlexItemBox* second_line_start_box = first_line->GetNextStartItem();

			for (FlexItemBox* box = first_line->GetStartItem(); box != second_line_start_box; box = box->Suc(FALSE))
			{
				page_break_before = CombineBreakPolicies(page_break_before, box->StealPageBreakPolicyBefore());
				column_break_before = CombineBreakPolicies(column_break_before, box->StealColumnBreakPolicyBefore());
			}

			for (FlexItemBox* box = last_line->GetStartItem(); box; box = box->Suc(FALSE))
			{
				page_break_after = CombineBreakPolicies(page_break_after, box->StealPageBreakPolicyAfter());
				column_break_after = CombineBreakPolicies(column_break_after, box->StealColumnBreakPolicyAfter());
			}
		}
}

/** Traverse box. */

/* virtual */ void
FlexContent::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	Column* pane = traversal_object->GetCurrentPane();

	/* The 'order' property affects painting order, but if we're supposed to be
	   traversing in logical order, we still have to do that, obviously. */

	BOOL logical_order = traversal_object->TraverseInLogicalOrder();

	for (FlexItemBox* flex_item = layout_stack.First(logical_order);
		 flex_item && !traversal_object->IsOffTargetPath() && !traversal_object->IsPaneDone();
		 flex_item = flex_item->Suc(logical_order))
	{
		if (pane)
			if (!traversal_object->IsPaneStarted())
			{
				/* We're looking for a column's or page's start element. Until
				   we find it, we shouldn't traverse anything, except for the
				   start element's ancestors. */

				const ColumnBoundaryElement& start_element = pane->GetStartElement();

				if (flex_item == start_element.GetFlexItemBox())
					/* We found the start element. Traverse everything as
					   normally until we find the stop element. */

					traversal_object->SetPaneStarted(TRUE);
				else
					/* Skip the element unless it's an ancestor of the start
					   element. */

					if (!flex_item->GetHtmlElement()->IsAncestorOf(start_element.GetHtmlElement()))
						// Not an ancestor of the start element. Move along.

						continue;
			}
			else
				if (pane->ExcludeStopElement() && flex_item == pane->GetStopElement().GetFlexItemBox())
				{
					traversal_object->SetPaneDone(TRUE);
					break;
				}

		flex_item->Traverse(traversal_object, layout_props);

		if (pane)
		{
			const ColumnBoundaryElement& stop_element = pane->GetStopElement();

			/* If we get here, we have obviously traversed something, so mark
			   column or page traversal as started. We may have missed the
			   opportunity to do so above, if the traverse area doesn't
			   intersect with the start element. */

			traversal_object->SetPaneStarted(TRUE);

			if (stop_element.GetFlexItemBox() == flex_item)
				/* We found the stop element, which means that we're done with
				   a column or page. */

				traversal_object->SetPaneDone(TRUE);
			else
				if (flex_item->GetHtmlElement()->IsAncestorOf(stop_element.GetHtmlElement()))
					// We passed the stop element without visiting it.

					traversal_object->SetPaneDone(TRUE);
		}
	}
}

/** Does width propagation from this flexbox affect FlexRoot? */

BOOL
FlexContent::AffectsFlexRoot() const
{
	LayoutProperties* cascade = placeholder->GetCascade();

	if (cascade->container)
		return cascade->container->AffectsFlexRoot();
	else
		if (cascade->flexbox)
			return cascade->flexbox->AffectsFlexRoot();

	OP_ASSERT(!"Unexpected containing element for this flexbox");

	return FALSE;
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
FlexContent::IsColumnizable() const
{
	return TRUE;
}

/** Distribute content into columns. */

/* virtual */ BOOL
FlexContent::Columnize(Columnizer& columnizer, Container* container)
{
	BREAK_POLICY prev_page_break_policy = BREAK_AVOID;
	BREAK_POLICY prev_column_break_policy = BREAK_AVOID;
	BOOL columnized = FALSE;

	if (flex_packed.is_vertical)
	{
		/* With the current design, a vertical flexbox can be fragmented only
		   if it is single-line. Furthermore, we cannot fragment reverse item
		   stacking, because of the mismatch between logical and visual
		   order. */

		if ((!line_stack.First() || !line_stack.First()->Suc()) && !flex_packed.items_reversed)
		{
			for (FlexItemBox* item_box = layout_stack.First(FALSE); item_box; item_box = item_box->Suc(FALSE))
			{
				LayoutCoord virtual_position = item_box->GetStaticMarginY();
				BREAK_POLICY cur_column_break_policy = item_box->GetColumnBreakPolicyBefore();
				BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, cur_column_break_policy);
				BREAK_POLICY cur_page_break_policy = item_box->GetPageBreakPolicyBefore();
				BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, cur_page_break_policy);

				if (BreakForced(page_break_policy))
				{
					if (!columnizer.ExplicitlyBreakPage(item_box))
						return FALSE;
				}
				else
					if (BreakForced(column_break_policy))
						if (!columnizer.ExplicitlyBreakColumn(item_box))
							return FALSE;

				if (!flex_packed.avoid_column_break_inside && BreakAllowedBetween(prev_column_break_policy, cur_column_break_policy)
					&& (columnizer.GetColumnsLeft() > 0 ||
						!flex_packed.avoid_page_break_inside && BreakAllowedBetween(prev_page_break_policy, cur_page_break_policy)))
					// We are allowed to move on to the next column/page, if necessary.

					if (!columnizer.CommitContent())
						return FALSE;

				columnizer.AllocateContent(virtual_position, item_box);
				columnizer.AdvanceHead(virtual_position + item_box->GetHeight());

				prev_page_break_policy = item_box->GetPageBreakPolicyAfter();
				prev_column_break_policy = item_box->GetColumnBreakPolicyAfter();
			}

			columnized = TRUE;
		}
	}
	else
		/* Cannot fragment reverse line stacking, because of the mismatch
		   between logical and visual order. */

		if (!flex_packed.lines_reversed)
		{
			FlexLine* next_line;

			for (FlexLine* line = line_stack.First(); line; line = next_line)
			{
				next_line = line->Suc();

				LayoutCoord virtual_position = line->GetCrossPosition() + top_border_padding;
				FlexItemBox* start_box = line->GetStartItem();
				FlexItemBox* next_line_box = next_line ? next_line->GetStartItem() : NULL;

				FlexItemBox* last_box = NULL;
				BREAK_POLICY cur_column_break_policy = BREAK_ALLOW;
				BREAK_POLICY cur_page_break_policy = BREAK_ALLOW;
				BREAK_POLICY next_column_break_policy = BREAK_ALLOW;
				BREAK_POLICY next_page_break_policy = BREAK_ALLOW;

				for (FlexItemBox* item_box = start_box; item_box != next_line_box; item_box = item_box->Suc(FALSE))
				{
					last_box = item_box;
					cur_column_break_policy = CombineBreakPolicies(cur_column_break_policy, item_box->GetColumnBreakPolicyBefore());
					cur_page_break_policy = CombineBreakPolicies(cur_page_break_policy, item_box->GetPageBreakPolicyBefore());
					next_column_break_policy = CombineBreakPolicies(next_column_break_policy, item_box->GetColumnBreakPolicyAfter());
					next_page_break_policy = CombineBreakPolicies(next_page_break_policy, item_box->GetPageBreakPolicyAfter());
				}

				BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, cur_column_break_policy);
				BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, cur_page_break_policy);

				if (BreakForced(page_break_policy))
				{
					if (!columnizer.ExplicitlyBreakPage(start_box))
						return FALSE;
				}
				else
					if (BreakForced(column_break_policy))
						if (!columnizer.ExplicitlyBreakColumn(start_box))
							return FALSE;

				if (!flex_packed.avoid_column_break_inside && BreakAllowedBetween(prev_column_break_policy, cur_column_break_policy)
					&& (columnizer.GetColumnsLeft() > 0 ||
						!flex_packed.avoid_page_break_inside && BreakAllowedBetween(prev_page_break_policy, cur_page_break_policy)))
					// We are allowed to move on to the next column/page, if necessary.

					if (!columnizer.CommitContent())
						return FALSE;

				columnizer.AllocateContent(virtual_position, start_box);
				columnizer.AllocateContent(virtual_position, last_box);
				columnizer.AdvanceHead(virtual_position + line->GetCrossSize());

				prev_page_break_policy = next_page_break_policy;
				prev_column_break_policy = next_column_break_policy;
			}

			columnized = TRUE;
		}

	if (placeholder->IsBlockBox() && !((BlockBox*) placeholder)->IsColumnSpanned())
	{
		BlockBox* this_block = (BlockBox*) placeholder;

		if (columnized)
			if (!flex_packed.avoid_column_break_inside &&
				(columnizer.GetColumnsLeft() > 1 || !flex_packed.avoid_page_break_inside))
				return columnizer.AddEmptyBlockContent(this_block, container);

		columnizer.AllocateContent(height, this_block);
	}

	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
FlexContent::FindColumn(ColumnFinder& cf)
{
	for (FlexItemBox* item_box = layout_stack.First(FALSE);
		 item_box && !cf.IsBoxEndFound();
		 item_box = item_box->Suc(FALSE))
	{
		LayoutCoord stack_position = item_box->GetStaticMarginY();

		cf.SetPosition(stack_position);

		if (item_box->GetHtmlElement()->IsAncestorOf(cf.GetTarget()->GetHtmlElement()))
		{
			cf.EnterChild(stack_position);
			cf.SetBoxStartFound();
			cf.SetBoxEndFound();
			cf.LeaveChild(stack_position);
		}
	}
}

/** Finish current line. */

void
FlexContent::FinishLine()
{
	LayoutCoord line_cross_size = reflow_state->line.GetCrossSize();

	if (IsVertical())
	{
		if (!flex_packed.content_uptodate)
		{
			// We're calculating min/max widths. Let this line contribute.

			minimum_width += reflow_state->line.minimum_width;
			maximum_width += reflow_state->line.maximum_width;
		}
	}
	else
	{
		// Height propagation needs the unstretched height of the lines.

		reflow_state->items_bottom += line_cross_size;

		if (!flex_packed.content_uptodate)
			min_height += reflow_state->line.GetMinHeight();
	}

	// Prepare for a new line.

	reflow_state->line.Reset();
}

/** Add a new flexbox item to this flex content. */

LAYST
FlexContent::GetNewItem(LayoutInfo& info, LayoutProperties* box_cascade, FlexItemBox* item_box)
{
	item_box->Out();

	if (!reflow_state->has_collapsed_items)
		reflow_state->has_collapsed_items = item_box->IsVisibilityCollapse();

	return LAYOUT_CONTINUE;
}

/** Finish new flexbox item. */

void
FlexContent::FinishNewItem(LayoutInfo& info, LayoutProperties* box_cascade, FlexItemBox* item_box)
{
	item_box->Into(&layout_stack);

	const HTMLayoutProperties& box_props = *box_cascade->GetProps();
	LayoutCoord hyp_item_size = item_box->GetConstrainedMainSize(item_box->GetFlexBaseSize(reflow_state->containing_block_size));

	if (reflow_state->line.item_count && IsWrappable())
		if (reflow_state->line.hyp_main_space_used + hyp_item_size > reflow_state->max_line_main_size)
		{
			/* Line is assumed to be full, so assume that we move to the next.
			   One note here: Since we lay out in logical order, this won't
			   work perfectly if items are re-ordered by the 'order' property
			   and end up on a different line than they otherwise would do.
			   But we are only human... (this only affects min/max calculation,
			   not flexbox layout in general, though) */

			if (IsVertical())
				if (reflow_state->max_line_main_size >= 0)
				{
					reflow_state->items_bottom = reflow_state->max_line_main_size;

					if (!flex_packed.content_uptodate)
						// Update minimum (intrisic) height.

						min_height = reflow_state->max_line_main_size;
				}

			FinishLine();
		}

	reflow_state->line.hyp_main_space_used += hyp_item_size;

	if (IsVertical())
	{
		// Increase assumed bottom, unless we have already assumed that we have wrapped.

		if (reflow_state->items_bottom < reflow_state->max_line_main_size)
			reflow_state->items_bottom += hyp_item_size;

		if (!flex_packed.content_uptodate)
		{
			// Update minimum (intrisic) height.

			LayoutCoord item_min_height = item_box->GetContent()->GetMinHeight();

			if (reflow_state->max_line_main_size == LAYOUT_COORD_MIN)
			{
				// Assuming one line per item.

				if (min_height < item_min_height)
					min_height = item_min_height;
			}
			else
				if (min_height < reflow_state->max_line_main_size)
					// Not wrapped yet. Add to minimum height.

					min_height += item_min_height;
		}

		// Adjust min/max line width values if this item affects them in any way.

		if (reflow_state->line.minimum_width < reflow_state->cur_minimum_width)
			reflow_state->line.minimum_width = reflow_state->cur_minimum_width;

		if (reflow_state->line.maximum_width < reflow_state->cur_maximum_width)
			reflow_state->line.maximum_width = reflow_state->cur_maximum_width;

		// Done with this item. Prepare for another one.

		reflow_state->cur_minimum_width = LayoutCoord(0);
		reflow_state->cur_maximum_width = LayoutCoord(0);
	}
	else
	{
		// Horizontal item.

		LayoutCoord cross_size = item_box->GetHypotheticalMarginBoxHeight();
		LayoutCoord baseline = LAYOUT_COORD_MIN;
		LayoutCoord min_height = item_box->GetContent()->GetMinHeight();

		if (item_box->GetFlexboxModes().GetAlignSelf() == FlexboxModes::ASELF_BASELINE &&
			!item_box->IsMarginTopAuto() && !item_box->IsMarginBottomAuto())
			baseline = item_box->GetItemBaseline();

		if (baseline != LAYOUT_COORD_MIN)
		{
			/* Baseline aligned item. Record amount of space used below and
			   above baseline. */

			LayoutCoord above_baseline = baseline + item_box->GetMarginTop();
			LayoutCoord below_baseline = cross_size - above_baseline;

			if (reflow_state->line.max_above_baseline < above_baseline)
				reflow_state->line.max_above_baseline = above_baseline;

			if (reflow_state->line.max_below_baseline < below_baseline)
				reflow_state->line.max_below_baseline = below_baseline;

			if (!flex_packed.content_uptodate)
			{
				LayoutCoord nonpercent_baseline = item_box->GetItemMinBaseline(box_props);
				LayoutCoord nonpercent_above_baseline = nonpercent_baseline + (box_props.GetMarginTopIsPercent() ? box_props.margin_top : LayoutCoord(0));
				LayoutCoord nonpercent_below_baseline = min_height + (box_props.GetMarginBottomIsPercent() ? box_props.margin_bottom : LayoutCoord(0)) - above_baseline;

				if (reflow_state->line.nonpercent_above_baseline < nonpercent_above_baseline)
					reflow_state->line.nonpercent_above_baseline = nonpercent_above_baseline;

				if (reflow_state->line.nonpercent_below_baseline < nonpercent_below_baseline)
					reflow_state->line.nonpercent_below_baseline = nonpercent_below_baseline;
			}
		}
		else
		{
			// Non-baseline aligned item. Just record cross space used.

			if (reflow_state->line.cross_space_used < cross_size)
				reflow_state->line.cross_space_used = cross_size;

			if (!flex_packed.content_uptodate)
			{
				LayoutCoord min_margin_height = min_height;

				if (!box_props.GetMarginTopIsPercent())
					min_margin_height += box_props.margin_top;

				if (!box_props.GetMarginBottomIsPercent())
					min_margin_height += box_props.margin_bottom;

				if (reflow_state->line.min_height_used < min_margin_height)
					reflow_state->line.min_height_used = min_margin_height;
			}
		}
	}

	reflow_state->line.item_count ++;
}

/** Propagate min/max widths from flexbox item child. */

void
FlexContent::PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord max_width)
{
	if (flex_packed.content_uptodate)
		return;

	LayoutProperties* cascade = placeholder->GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (props.content_width >= 0 && !cascade->flexbox)
		// Honor specified width.

		minimum_width = maximum_width = props.content_width;
	else
		if (IsVertical())
		{
			if (reflow_state->cur_minimum_width < min_width)
				reflow_state->cur_minimum_width = min_width;

			if (reflow_state->cur_maximum_width < max_width)
				reflow_state->cur_maximum_width = max_width;
		}
		else
		{
			if (props.flexbox_modes.GetWrap() == FlexboxModes::WRAP_NOWRAP)
				minimum_width += min_width;
			else
				/* Line breaking is allowed. We don't need more width than that
				   of the widest item. */

				if (minimum_width < min_width)
					minimum_width = min_width;

			maximum_width += max_width;
		}
}

/** Propagate bounding box from absolutely positioned child of flexbox item. */

/* virtual */ void
FlexContent::PropagateBottom(const LayoutInfo& info, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	OP_ASSERT(opts.type != PROPAGATE_FLOAT);

	if (!placeholder->GetLocalStackingContext() || !placeholder->IsPositionedBox())
		// Propagate past flexbox

		placeholder->PropagateBottom(info, LayoutCoord(0), LayoutCoord(0), child_bounding_box, opts);
	else
		placeholder->UpdateBoundingBox(child_bounding_box, FALSE);
}

/** Update bounding box. */

/* virtual */ void
FlexContent::UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content)
{
	placeholder->UpdateBoundingBox(child_bounding_box, FALSE);
}

ScrollableFlexContent::~ScrollableFlexContent()
{
	/* placeholder may be NULL here if we managed to allocate content but
	   failed to allocate the box in LayoutProperties::CreateBox(). */

	if (placeholder)
		SaveScrollPos(GetHtmlElement(), GetViewX(), GetViewY());
}

/** Clear min/max width. */

/* virtual */ void
ScrollableFlexContent::ClearMinMaxWidth()
{
	FlexContent::ClearMinMaxWidth();
	sc_packed.lock_auto_scrollbars = 0;
}

/** Lay out box. */

/* virtual */ LAYST
ScrollableFlexContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (info.external_layout_change)
		/* It wasn't the layout engine itself that requested this reflow pass,
		   so it's safe to allow removal of scrollbars. */

		sc_packed.lock_auto_scrollbars = 0;

	RETURN_VALUE_IF_ERROR(SetUpScrollable(cascade, info), LAYOUT_OUT_OF_MEMORY);

	info.Translate(-GetViewX(), -GetViewY());

	if (placeholder->IsPositionedBox())
		info.TranslateRoot(-GetViewX(), -GetViewY());

	CoreView::SetFixedPositionSubtree(GetFixedPositionedAncestor(cascade));

	return FlexContent::Layout(cascade, info, first_child, start_position);
}

/** Finish reflowing box. */

/* virtual */ LAYST
ScrollableFlexContent::FinishLayout(LayoutInfo& info)
{
	if (!reflow_state)
		return FlexContent::FinishLayout(info);

	LayoutProperties* cascade = placeholder->GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();

	LAYST st = FlexContent::FinishLayout(info);

	info.Translate(GetViewX(), GetViewY());

	if (placeholder->IsPositionedBox())
		info.TranslateRoot(GetViewX(), GetViewY());

	SetPosition(info.visual_device->GetCTM());

	if (HasScrollbarVisibilityChanged())
	{
		// Scrollbars appeared or disappeared; need reflow.

		info.workplace->SetReflowElement(cascade->html_element);
		flex_packed.needs_reflow = 1;
		SaveScrollPos(cascade->html_element, scroll_x, scroll_y);
	}

	FinishScrollable(props, width, height);

	return st;
}

/** Traverse box. */

/* virtual */ void
ScrollableFlexContent::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();
	LayoutCoord inner_width = width - LayoutCoord(props.border.left.width + props.border.right.width + GetVerticalScrollbarWidth());
	LayoutCoord inner_height = LayoutCoord(height - props.border.top.width - props.border.bottom.width - GetHorizontalScrollbarWidth());
	TraverseInfo traverse_info;

	if (traversal_object->EnterScrollable(props, this, inner_width, inner_height, traverse_info))
	{
		LayoutCoord left_scrollbar_adjustment = LayoutCoord(LeftHandScrollbar() ? GetVerticalScrollbarWidth() : 0);
		LayoutCoord view_x = GetViewX();
		LayoutCoord view_y = GetViewY();

		if (traverse_info.pretend_noscroll)
		{
			view_x = LayoutCoord(0);
			view_y = LayoutCoord(0);
		}

		LayoutCoord root_scroll_translation_x(0);
		LayoutCoord root_scroll_translation_y(0);
		BOOL positioned = placeholder->IsPositionedBox();

		traversal_object->Translate(left_scrollbar_adjustment - view_x, -view_y);
		traversal_object->TranslateRoot(left_scrollbar_adjustment - view_x, -view_y);
		traversal_object->TranslateScroll(view_x, view_y);

		if (positioned)
			traversal_object->SyncRootScroll(root_scroll_translation_x, root_scroll_translation_y);

		FlexContent::Traverse(traversal_object, layout_props);

		if (positioned)
			traversal_object->TranslateRootScroll(-root_scroll_translation_x, -root_scroll_translation_y);

		traversal_object->TranslateScroll(-view_x, -view_y);
		traversal_object->TranslateRoot(view_x - left_scrollbar_adjustment, view_y);
		traversal_object->Translate(view_x - left_scrollbar_adjustment, view_y);

		traversal_object->LeaveScrollable(this, traverse_info);
	}
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
ScrollableFlexContent::IsColumnizable() const
{
	/* If there are no scrollbars, we _could_ have split the flexbox over
	   multiple columns without making it look bad. However, since a
	   ScrollableFlexContent is a CoreView, and a CoreView is a rectangle, we
	   cannot really do that anyway. This is especially true because we update
	   the CoreView's position every time we paint the
	   ScrollableFlexContent. Its position would then be updated for each
	   column painted (being moved back and forth), causing infinite paint
	   loops. */

	return FALSE;
}

/** Handle event that occurred on this element. */

/* virtual */ void
ScrollableFlexContent::HandleEvent(FramesDocument* doc, DOM_EventType event, int offset_x, int offset_y)
{
#ifndef MOUSELESS
	GenerateWidgetEvent(event, offset_x, offset_y);
#endif // !MOUSELESS
}

/** Get scroll offset, if applicable for this type of box / content. */

/* virtual */ OpPoint
ScrollableFlexContent::GetScrollOffset()
{
	return OpPoint(GetViewX(), GetViewY());
}

/** Set scroll offset, if applicable for this type of box / content. */

/* virtual */ void
ScrollableFlexContent::SetScrollOffset(int* x, int* y)
{
	if (x)
		SetViewX(LayoutCoord(*x), TRUE);

	if (y)
		SetViewY(LayoutCoord(*y), TRUE);
}

/** Create form, plugin and iframe objects */

/* virtual */ OP_STATUS
ScrollableFlexContent::Enable(FramesDocument* doc)
{
	InitCoreView(GetHtmlElement(), doc->GetVisualDevice());
	return OpStatus::OK;
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
ScrollableFlexContent::Disable(FramesDocument* doc)
{
	if (doc->IsUndisplaying() || doc->IsBeingDeleted())
		DisableCoreView();
}

/** Propagate bounding box from absolutely positioned child of flexbox item. */

/* virtual */ void
ScrollableFlexContent::PropagateBottom(const LayoutInfo& info, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	FlexContent::PropagateBottom(info, child_bounding_box, opts);

	if (placeholder->IsPositionedBox())
		UpdateContentSize(child_bounding_box);
}

/** Update bounding box. */

/* virtual */ void
ScrollableFlexContent::UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content)
{
	UpdateContentSize(child_bounding_box);
}
