/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/layout/content/multicol.h"
#include "modules/layout/box/blockbox.h"
#include "modules/layout/box/flexitem.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/content/content.h"
#include "modules/layout/traverse/traverse.h"

/** Set to be vertical layout. */

void
ColumnBoundaryElement::Set(VerticalLayout* vertical_layout, Container* c)
{
	OP_ASSERT(c);
	type = VERTICAL_LAYOUT;
	this->vertical_layout = vertical_layout;
	container = c;
}

/** Set to be vertical layout. */

void
ColumnBoundaryElement::Set(BlockBox* block_box)
{
	type = VERTICAL_LAYOUT;
	vertical_layout = block_box;
	container = NULL;
}

/** Set to be table list element. */

void
ColumnBoundaryElement::Set(TableListElement* table_list_element)
{
	type = TABLE_LIST_ELEMENT;
	this->table_list_element = table_list_element;
	container = NULL;
}

/** Set to be table row. */

void
ColumnBoundaryElement::Set(TableRowBox* table_row_box)
{
	type = TABLE_ROW_BOX;
	this->table_row_box = table_row_box;
	container = NULL;
}

/** Set to be column row. */

void
ColumnBoundaryElement::Set(ColumnRow* column_row, MultiColumnContainer* mc)
{
	type = COLUMN_ROW;
	this->column_row = column_row;
	container = mc;
}

/** Set to be flex item. */

void
ColumnBoundaryElement::Set(FlexItemBox* flex_item_box)
{
	type = FLEX_ITEM_BOX;
	this->flex_item_box = flex_item_box;
	container = NULL;
}

/** Return the HTML_Element associated with this start/end element, if there is one. */

HTML_Element*
ColumnBoundaryElement::GetHtmlElement() const
{
	switch (type)
	{
	case VERTICAL_LAYOUT:
		if (vertical_layout->IsBlock())
			return ((BlockBox*) vertical_layout)->GetHtmlElement();
		else
			if (vertical_layout->IsLayoutBreak())
				return ((LayoutBreak*) vertical_layout)->GetHtmlElement();
			else
				return container->GetHtmlElement();

	case TABLE_LIST_ELEMENT:
		return table_list_element->GetHtmlElement();

	case TABLE_ROW_BOX:
		return table_row_box->GetHtmlElement();

	case COLUMN_ROW:
		return container->GetHtmlElement();

	case FLEX_ITEM_BOX:
		return flex_item_box->GetHtmlElement();

	default:
		OP_ASSERT(!"Unhandled type");
	case NOT_SET:
		return NULL;
	}
}

/** Attach a float to this page or column. */

void
Column::AddFloat(FloatedPaneBox* box)
{
	PaneFloatEntry* first_bottom;

	for (first_bottom = floats.First(); first_bottom; first_bottom = first_bottom->Suc())
		if (!first_bottom->GetBox()->IsTopFloat())
			break;

	box->IntoPaneFloatList(floats, first_bottom);
}

/** Position top-aligned floats. */

void
Column::PositionTopFloats()
{
	LayoutCoord float_y = LayoutCoord(0);

	for (PaneFloatEntry* entry = floats.First(); entry; entry = entry->Suc())
	{
		FloatedPaneBox* box = entry->GetBox();

		if (!box->IsTopFloat())
			break;

		float_y += box->GetMarginTop();
		box->SetY(float_y);
		float_y += box->GetMarginBottom() + box->GetHeight();
	}
}

/** Position bottom-aligned floats. */

void
Column::PositionBottomFloats(LayoutCoord row_height)
{
	LayoutCoord float_y = row_height;

	for (PaneFloatEntry* entry = floats.Last(); entry; entry = entry->Pred())
	{
		FloatedPaneBox* box = entry->GetBox();

		if (box->IsTopFloat())
			break;

		float_y -= box->GetMarginBottom() + box->GetHeight();
		box->SetY(float_y);
		float_y -= box->GetMarginTop();
	}
}

/** Return TRUE if the specified float lives in this column. */

BOOL
Column::HasFloat(FloatedPaneBox* box)
{
	for (PaneFloatEntry* entry = floats.First(); entry; entry = entry->Suc())
		if (entry->GetBox() == box)
			return TRUE;

	return FALSE;
}

/** Move the row down by the specified amount. */

void
ColumnRow::MoveDown(LayoutCoord amount)
{
	y += amount;
}

/** Get the height of this row. */

LayoutCoord
ColumnRow::GetHeight() const
{
	if (!columns.First())
		return LayoutCoord(0);

	LayoutCoord row_height = LayoutCoord(0);

	for (Column* column = (Column*) columns.First(); column; column = column->Suc())
	{
		LayoutCoord column_height = column->GetHeight() + column->GetTopFloatsHeight() + column->GetBottomFloatsHeight();

		if (row_height < column_height)
			row_height = column_height;
	}

	return row_height;
}

/** Traverse the row. */

void
ColumnRow::Traverse(MultiColumnContainer* multicol_container, TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	LayoutCoord old_pane_x_tr;
	LayoutCoord old_pane_y_tr;

	traversal_object->GetPaneTranslation(old_pane_x_tr, old_pane_y_tr);

	TargetTraverseState target_traversal;
	TraverseInfo traverse_info;
	BOOL target_done = FALSE;

	traversal_object->StoreTargetTraverseState(target_traversal);
	traversal_object->Translate(LayoutCoord(0), y);

	for (Column* column = GetFirstColumn(); column; column = column->Suc())
	{
		traversal_object->SetPaneClipRect(OpRect(SHRT_MIN / 2, LayoutCoord(column->GetTopFloatsHeight()), SHRT_MAX, column->GetHeight()));

		// Set up the traversal object for column traversal.

		traversal_object->SetCurrentPane(column);
		traversal_object->SetPaneDone(FALSE);
		traversal_object->SetPaneStartOffset(column->GetStartOffset());
		traversal_object->SetPaneStarted(FALSE);

		/* Restore target traversal state before traversing this column. The
		   target element may live in multiple columns. */

		traversal_object->RestoreTargetTraverseState(target_traversal);

		traversal_object->Translate(column->GetX(), column->GetY());
		traversal_object->SetPaneTranslation(traversal_object->GetTranslationX(), traversal_object->GetTranslationY());
		traversal_object->Translate(LayoutCoord(0), column->GetTranslationY());

		if (traversal_object->EnterPane(layout_props, column, TRUE, traverse_info))
		{
			multicol_container->Container::Traverse(traversal_object, layout_props);
			traversal_object->LeavePane(traverse_info);
		}

		traversal_object->Translate(-column->GetX(), -column->GetY() - column->GetTranslationY());

		if (!target_done)
			target_done = traversal_object->IsTargetDone();
	}

	traversal_object->Translate(LayoutCoord(0), -y);

	if (target_done)
		// We found the target while traversing one or more of these columns

		traversal_object->SwitchTarget(layout_props->html_element);

	traversal_object->SetPaneTranslation(old_pane_x_tr, old_pane_y_tr);
}

Columnizer::Columnizer(Columnizer* ancestor,
					   BOOL ancestor_columnizer_in_same_element,
					   const LayoutInfo& layout_info,
					   ColumnRowStack& rows,
					   MultiColBreakpoint* first_break,
					   SpannedElm* first_spanned_elm,
					   LayoutCoord max_height,
					   int column_count,
					   LayoutCoord column_width,
					   LayoutCoord content_width,
					   LayoutCoord column_gap,
					   LayoutCoord column_y_stride,
					   LayoutCoord top_offset,
					   LayoutCoord virtual_height,
					   LayoutCoord floats_virtual_height,
					   BOOL height_restricted,
#ifdef SUPPORT_TEXT_DIRECTION
					   BOOL is_rtl,
#endif // SUPPORT_TEXT_DIRECTION
					   BOOL balance)
	: layout_info(layout_info),
	  column_count(column_count),
	  column_width(column_width),
	  content_width(content_width),
	  column_gap(column_gap),
	  column_y_stride(column_y_stride),
	  virtual_height(virtual_height),
	  remaining_floats_virtual_height(floats_virtual_height),
	  max_height(max_height),
	  always_balance(balance),
	  ancestor_columnizer(ancestor),
	  ancestor_columnizer_in_same_element(ancestor_columnizer_in_same_element),
	  descendant_multicol(NULL),
	  rows(rows),
	  next_break(first_break),
	  next_spanned_elm(first_spanned_elm),
	  pending_start_offset(0),
	  virtual_y(0),
	  committed_virtual_y(0),
	  pending_virtual_y_start(0),
	  col_virtual_y_start(0),
	  earliest_break_virtual_y(0),
	  stack_offset(-top_offset),
	  initial_row_height(0),
	  current_row_height(0),
	  max_row_height(0),
	  top_floats_height(0),
	  bottom_floats_height(0),
	  pending_margin(0),
	  current_column_num(0),
	  column_y(0),
	  y_translation(-top_offset),
	  span_all(FALSE),
#ifdef SUPPORT_TEXT_DIRECTION
	  is_rtl(is_rtl),
#endif // SUPPORT_TEXT_DIRECTION
	  balance_current_row(balance),
	  allow_column_stretch(FALSE),
	  column_open(FALSE),
	  row_open(FALSE),
	  height_restricted(height_restricted),
	  reached_max_height(FALSE),
	  row_has_spanned_floats(FALSE),
	  trial(FALSE)
#ifdef PAGED_MEDIA_SUPPORT
	, last_allowed_page_break_after(NULL),
	  page_break_policy_after_previous(BREAK_AVOID),
	  page_break_policy_before_current(BREAK_AVOID),
	  page_open(TRUE)
#endif // PAGED_MEDIA_SUPPORT
{
	OP_ASSERT(ancestor || !ancestor_columnizer_in_same_element);
}

/** Cancel the break caused by this spanned element. */

void
SpannedElm::CancelBreak()
{
	if (breakpoint)
	{
		breakpoint->Out();
		OP_DELETE(breakpoint);

		breakpoint = NULL;
	}
}

/** Get the block box established by the spanned element. */

BlockBox*
SpannedElm::GetSpannedBox() const
{
	if (Box* box = html_element->GetLayoutBox())
		if (box->IsBlockBox())
			return (BlockBox*) box;

	return NULL;
}

/** Update row and/or container height. */

/* virtual */ void
Columnizer::UpdateHeight(LayoutCoord height)
{
	if (balance_current_row && current_row_height < height)
	{
		current_row_height = height;

		if (current_row_height > max_row_height)
		{
			current_row_height = max_row_height;
			reached_max_height = TRUE;
		}
	}
}

/** Return the position of the current row, if any. */

LayoutCoord
Columnizer::GetRowPosition() const
{
	LayoutCoord cur_row_y;

	if (ColumnRow* last_row = rows.Last())
	{
		cur_row_y = last_row->GetPosition();

		if (!row_open)
			cur_row_y += last_row->GetHeight() + pending_margin;
	}
	else
		cur_row_y = LayoutCoord(0);

	return cur_row_y;
}

/** Get the current (non-virtual) bottom, relative to the first row. */

LayoutCoord
Columnizer::GetBottom(LayoutCoord some_virtual_y) const
{
	LayoutCoord row_pos(0);

	if (ColumnRow* row = GetCurrentRow())
		row_pos = row->GetPosition();

	return row_pos + some_virtual_y - col_virtual_y_start + top_floats_height + bottom_floats_height;
}

/** Set virtual position of the earliest break, if later than already set. */

void
Columnizer::SetEarliestBreakPosition(LayoutCoord local_virtual_y)
{
	LayoutCoord new_virtual_y = local_virtual_y + stack_offset;

	if (earliest_break_virtual_y < new_virtual_y)
		earliest_break_virtual_y = new_virtual_y;
}

/** Get accumulated height occupied by floats in the current column. */

LayoutCoord
Columnizer::GetFloatsHeight()
{
	LayoutCoord top_height = LAYOUT_COORD_MIN;
	LayoutCoord bottom_height = LAYOUT_COORD_MIN;
	int min_span = 1;

	for (Column* column = GetCurrentColumn(); column; column = column->Pred(), min_span++)
		for (PaneFloatEntry* entry = column->GetFirstFloatEntry(); entry; entry = entry->Suc())
		{
			FloatedPaneBox* box = entry->GetBox();

			if (box->GetColumnSpan() >= min_span)
				if (box->IsTopFloat())
				{
					LayoutCoord bottom = box->GetHeight() + box->GetMarginTop() + box->GetMarginBottom();

					if (top_height < bottom)
						top_height = bottom;
				}
				else
				{
					/* Bottom floats have not yet got their final position;
					   they are currently positioned as if row height were 0. */

					// FIXME: this is wrong, but will work fine as long as there's only one bottom float.

					LayoutCoord height = box->GetHeight() + box->GetMarginTop() + box->GetMarginBottom();

					if (bottom_height < height)
						bottom_height = height;
				}
		}

	// No top/bottom floats means 0 height.

	if (top_height == LAYOUT_COORD_MIN)
		top_height = LayoutCoord(0);

	if (bottom_height == LAYOUT_COORD_MIN)
		bottom_height = LayoutCoord(0);

	return top_height + bottom_height;
}

/** Prepare content for addition. */

void
Columnizer::AllocateContent(LayoutCoord local_virtual_y, const ColumnBoundaryElement& elm, LayoutCoord start_offset)
{
	LayoutCoord new_virtual_y = local_virtual_y + stack_offset;

	if (!first_uncommitted_element.IsSet())
	{
		if (allow_column_stretch && GetCurrentColumn())
		{
			LayoutCoord bottom = col_virtual_y_start + current_row_height;

			if (pending_virtual_y_start < bottom && new_virtual_y >= bottom)
				/* Walked past the assumed bottom of the column, thanks to a
				   margin. We can ignore margins that cross column boundaries,
				   so don't let it stretch the current column. */

				allow_column_stretch = FALSE;
		}

		first_uncommitted_element = elm;
		pending_start_offset = start_offset;

		if (GetCurrentColumn())
			pending_virtual_y_start = new_virtual_y;
	}

	last_allocated_element = elm;

	if (ancestor_columnizer)
		ancestor_columnizer->AllocateContent(GetBottom(new_virtual_y), elm, start_offset);
}

/** Return TRUE if we can create taller columns by advancing to a next row or a next page. */

BOOL
Columnizer::MoreSpaceAhead() const
{
	if (column_open && GetSpaceUsed() > 0)
		return TRUE;

	if (ancestor_columnizer)
		return ancestor_columnizer->MoreSpaceAhead();
#ifdef PAGED_MEDIA_SUPPORT
	else
		if (!trial && layout_info.paged_media != PAGEBREAK_OFF)
			/* If the current row doesn't start at the top of the page, we can
			   get taller columns by advancing to the next page.

			   Note that this doesn't accurately answer the question we meant
			   to ask. If height or max-height properties have constrained
			   max_row_height, we should really return FALSE here (then there
			   isn't really any more "space ahead"). But moving to the next
			   page instead of continuing to the left or right of the page's
			   boundaries is probably better anyway. */

			return max_row_height < LayoutCoord(layout_info.doc->GetRelativePageBottom() - layout_info.doc->GetRelativePageTop());
#endif // PAGED_MEDIA_SUPPORT

	return FALSE;
}

/** Calculate X position of new column to be added. */

LayoutCoord
Columnizer::CalculateColumnX() const
{
	LayoutCoord accumulated_gap;

	if (column_gap == 0 || column_count < 2)
		accumulated_gap = LayoutCoord(current_column_num) * column_gap;
	else
	{
		/* If column-gap is non-zero, we allow distribution of any column width
		   rounding error there, so that the far column will be flush with the
		   multicol container's content edge. */

		LayoutCoord total_column_gap = column_gap * LayoutCoord(column_count - 1);
		LayoutCoord error = content_width - (column_width * LayoutCoord(column_count) + total_column_gap);

		total_column_gap += error;
		accumulated_gap = total_column_gap * LayoutCoord(current_column_num) / LayoutCoord(column_count - 1);
	}

#ifdef SUPPORT_TEXT_DIRECTION
	if (is_rtl)
		return content_width - LayoutCoord(current_column_num + 1) * column_width - accumulated_gap;
#endif // SUPPORT_TEXT_DIRECTION

	return LayoutCoord(current_column_num) * column_width + accumulated_gap;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Create a new page. */

BOOL
Columnizer::GetNewPage(LayoutCoord& y_offset)
{
	OP_ASSERT(!ancestor_columnizer);

	LayoutCoord y_pos(0);

	if (last_allowed_page_break_after)
		y_pos = last_allowed_page_break_after->GetPosition() + last_allowed_page_break_after->GetHeight();

	y_offset = LayoutCoord(0);

	if (page_open)
		ClosePage();

	BREAK_POLICY break_policy = CombineBreakPolicies(page_break_policy_after_previous, page_break_policy_before_current);

	do
	{
		PageDescription* page_description = layout_info.doc->AdvancePage(y_pos);

		if (!page_description)
			return FALSE;

		switch (break_policy)
		{
		case BREAK_LEFT:
			if (page_description->GetNumber() % 2 == 1)
				break_policy = BREAK_ALLOW;
			else
				break_policy = BREAK_ALWAYS; // need one blank page, so that the next page is a left-page.

			break;

		case BREAK_RIGHT:
			if (page_description->GetNumber() % 2 == 0)
				break_policy = BREAK_ALLOW;
			else
				break_policy = BREAK_ALWAYS; // need one blank page, so that the next page is a right-page.

			break;

		default:
			break_policy = BREAK_ALLOW;
		}
	}
	while (break_policy != BREAK_ALLOW);

	LayoutCoord this_page_top = LayoutCoord(layout_info.doc->GetRelativePageTop());

	y_offset = this_page_top - y_pos;
	max_row_height = LayoutCoord(layout_info.doc->GetRelativePageBottom()) - this_page_top;
	page_open = TRUE;

	// Move all elements that follow the page break down to the new page.

	for (ColumnRow* row = last_allowed_page_break_after ? last_allowed_page_break_after->Suc() : rows.First();
		 row;
		 row = row->Suc())
		row->MoveDown(y_offset);

	return TRUE;
}

#endif // PAGED_MEDIA_SUPPORT

/** Create a new column. */

BOOL
Columnizer::GetNewColumn(const ColumnBoundaryElement& start_element, LayoutCoord minimum_height, BOOL add_queued_floats)
{
	if (column_open)
		// Implicit column break.

		CloseColumn();

	BOOL need_new_ancestor_column = FALSE;

	/* Should the potential new pane started on the ancestor columnizer begin
	   with the first uncommitted element in the ancestor? This is the case when
	   the ancestor starts its first pane, however not if the ancestor distributes
	   the same content as this (e.g. paged overflow and multiple columns in
	   the same element). In such case the first_uncommitted_element might not
	   be yet set in the ancestor or using it as a start element could lead
	   to skipping floats when rendering. */

	BOOL use_ancestors_first_uncommitted_element = FALSE;

	if (ancestor_columnizer &&
		(!row_open || GetCurrentRow()->GetPosition() + current_row_height < max_height))
	{
		/* Nested columnization, and there's still height availble in the inner
		   multicol. Need to consult with the ancestor multicol. */

		BOOL ancestor_has_no_column = !ancestor_columnizer->GetCurrentColumn();
		need_new_ancestor_column = ancestor_has_no_column || GetColumnsLeft() == 0;
		use_ancestors_first_uncommitted_element = ancestor_has_no_column && !ancestor_columnizer_in_same_element;

		if (!need_new_ancestor_column && (!row_open || minimum_height > current_row_height))
		{
			/* There is an open column in the ancestor, and we're not out of
			   columns down here yet, either.

			   However, the minimum height of this column is larger than the
			   current row height, so we have to check if there is enough space
			   for that. Does the ancestor column have enough space for us? And
			   if we have created a row previously, does it have enough space
			   as well? */

			LayoutCoord space_used = ancestor_columnizer->stack_offset + GetRowPosition() - ancestor_columnizer->col_virtual_y_start;
			LayoutCoord available_space;

			if (ancestor_columnizer->balance_current_row)
				available_space = ancestor_columnizer->current_row_height - space_used;
			else
				available_space = ancestor_columnizer->max_row_height - space_used;

			if (row_open && available_space > max_row_height)
				available_space = max_row_height;

			if (available_space < minimum_height)
				/* No, either the ancestor doesn't have enough space for us, or
				   the current row cannot be stretched to make the new content
				   fit. Create a new column, but only if it is likely that
				   doing so will mitigate the space shortage. */

				need_new_ancestor_column = MoreSpaceAhead();
		}

		if (need_new_ancestor_column)
		{
			/* The outer multicol container either still has no column, or its
			   current column is full, or all columns in this inner multicol
			   container have been filled, or the current row height of the
			   inner multicol container is too short (and we can improve
			   it). Advance to the next ancestor column, and start a new row
			   down here. */

			if (row_open)
				// Unleash the floats that were waiting for a new row.

				for (PaneFloatEntry* entry = float_queue.First(); entry; entry = entry->Suc())
					entry->GetBox()->ResetIsForNextRow();

			CloseRow(TRUE);

			if (ColumnRow* row = rows.Last())
			{
				/* The next outer column's virtual Y start position will be
				   just below the bottom of the current one's. During
				   columnization virtual Y start candidates have been
				   propagated to the ancestor columnizer, but they cannot
				   really be trusted. */

				ancestor_columnizer->pending_virtual_y_start = ancestor_columnizer->stack_offset + row->GetPosition() + row->GetHeight();
				ancestor_columnizer->committed_virtual_y = ancestor_columnizer->pending_virtual_y_start;
			}
		}
	}
#ifdef PAGED_MEDIA_SUPPORT
	else
		if (!trial && layout_info.paged_media != PAGEBREAK_OFF)
		{
			BOOL create_new_page = FALSE;

			if (GetColumnsLeft() == 0)
			{
				if (ColumnRow* current_row = rows.Last())
				{
					int height = current_row->GetHeight();

					if (height < current_row_height)
						height = current_row_height;

					create_new_page = current_row->GetPosition() + height < max_height;
				}

				if (create_new_page)
					last_allowed_page_break_after = rows.Last();
			}

			if (!create_new_page)
				if (row_open)
					create_new_page = minimum_height > current_row_height && MoreSpaceAhead();
				else
					if (ColumnRow* row = rows.Last())
					{
						int start_position = row->GetPosition() + row->GetHeight() + pending_margin;

						if (start_position + minimum_height > (int) layout_info.doc->GetRelativePageBottom() &&
							start_position > (int) layout_info.doc->GetRelativePageTop())
							create_new_page = MoreSpaceAhead();
					}

			if (create_new_page)
				/* All columns on the current page have been filled (and we
				   have more vertical space available), or the row height is
				   too short (and we can improve it). Advance to the next page
				   (just close the current page for now; actual page creation
				   takes place further down). */

				ClosePage();
		}
#endif // PAGED_MEDIA_SUPPORT

	if (!row_open)
	{
		// No open row. Prepare a new one.

		ColumnRow* last_row = rows.Last();
		LayoutCoord new_row_y;

		if (last_row)
			new_row_y = last_row->GetPosition() + last_row->GetHeight();
		else
			new_row_y = LayoutCoord(0);

		new_row_y += pending_margin;
		y_translation -= pending_margin;
		pending_margin = LayoutCoord(0);

#ifdef PAGED_MEDIA_SUPPORT
		if (!page_open)
		{
			// No open page. Prepare for a new one.

			LayoutCoord offset;

			OP_ASSERT(layout_info.paged_media != PAGEBREAK_OFF);

			if (!GetNewPage(offset))
				return FALSE;

			new_row_y += offset;
			last_allowed_page_break_after = last_row;
		}
		else
#endif // PAGED_MEDIA_SUPPORT
			max_row_height = max_height - (MAX(new_row_y, LayoutCoord(0)));

		ColumnRow* new_row = OP_NEW(ColumnRow, (new_row_y));

		if (!new_row)
			return FALSE;

		new_row->Into(&rows);

		if (need_new_ancestor_column)
		{
			if (ColumnRow* prev_row = new_row->Pred())
				if (ColumnRow* outer_row = ancestor_columnizer->GetCurrentRow())
					if (Column* outer_column = outer_row->GetLastColumn())
						outer_column->SetStopAfterElement(ColumnBoundaryElement(prev_row, ancestor_columnizer->descendant_multicol));

			/* If we are about to make the new column begin with
			   ancestor_columnizer->first_uncommitted_element, we must assert it is set.
			   And we can do that, because at least the element that this Columnizer
			   distributes content of should have been allocated already (providing that
			   the ancestor columnizer distributes the content of some ancestor element). */

			OP_ASSERT(!use_ancestors_first_uncommitted_element || ancestor_columnizer->first_uncommitted_element.IsSet());

			ColumnBoundaryElement start_element = use_ancestors_first_uncommitted_element ?
				ancestor_columnizer->first_uncommitted_element :
				ColumnBoundaryElement(new_row, ancestor_columnizer->descendant_multicol);

			if (!ancestor_columnizer->GetNewColumn(start_element, minimum_height))
				return FALSE;
		}

#ifdef PAGED_MEDIA_SUPPORT
		if (!trial && !ancestor_columnizer && layout_info.paged_media != PAGEBREAK_OFF)
		{
			LayoutCoord remaining_page_height = LayoutCoord(layout_info.doc->GetRelativePageBottom()) - new_row_y;

			if (max_row_height > remaining_page_height)
				max_row_height = remaining_page_height;

			if (BreakAllowedBetween(page_break_policy_after_previous, page_break_policy_before_current))
				last_allowed_page_break_after = last_row;
		}
#endif // PAGED_MEDIA_SUPPORT

		balance_current_row = always_balance;

		MultiColBreakpoint* bp;
		LayoutCoord row_virtual_y_stop = virtual_height;
		int relevant_explicit_breaks = 0;

		for (bp = next_break; bp; bp = bp->Suc())
		{
			// Reset any previous balancing calculation attempts.

			bp->ResetAssumedImplicitBreaks();

			if (pending_virtual_y_start > next_break->GetVirtualY())
			{
				/* Discrepancy between explicit break list and columnizer break
				   handling. We have missed a break. */

				OP_ASSERT(!"We're already past this break.");
				remaining_floats_virtual_height += next_break->GetPaneFloatsVirtualHeight();
				next_break = bp->Suc();
				continue;
			}

			if (bp->IsRowBreak())
			{
				// The row stops here (because a spanned box or a page break).

				row_virtual_y_stop = bp->GetVirtualY();

				if (bp->IsSpannedElement())
					// A spanned element requires us to balance earlier content.

					balance_current_row = TRUE;

				break;
			}
			else
				relevant_explicit_breaks++;
		}

		/* To get column balancing right, add the total virtual height that would have
		   been occupied by the remaining floats (the pane-attached floats that still
		   haven't been added to any column) if they were regular content. */

		row_virtual_y_stop += remaining_floats_virtual_height;

		MultiColBreakpoint* new_next_break = bp ? bp->Suc() : NULL; // First break in next row

		int free_breaks = 0;

		if (balance_current_row)
		{
			/* We may want to balance the columns - either because the CSS
			   properties say so, or because there's a spanned box ahead.

			   Find the number of available implicit breaks for this row. If
			   there are still too many explicit breaks ahead, there will be no
			   implicit breaks available (and there's going to need another row
			   after this one), in which case column balancing will be
			   futile. */

			if (!span_all)
				free_breaks = (column_count - 1) - relevant_explicit_breaks;

			if (free_breaks <= 0)
				balance_current_row = FALSE;
		}

		if (free_breaks > 0)
		{
			/* We have at least one free column break to use for implicit
			   column breaking, and we're supposed to balance the columns as
			   best we can. Calculate a suitable row height. May be increased
			   later if necessary and allowed. */

			OP_ASSERT(balance_current_row);

			if (relevant_explicit_breaks > 0)
			{
				/* There are explicit column breaks to consider. Figure out
				   where it's best to put the implicit breaks (as many as
				   specified by free_breaks), to calculate an initial row
				   height. */

				int trailing_implicit_breaks = 0; // implicit breaks after the last explicit break
				int implicit_breaks;

				for (implicit_breaks = 0; implicit_breaks < free_breaks; implicit_breaks ++)
				{
					MultiColBreakpoint* break_following_tallest_col = NULL;
					LayoutCoord prev_break_virtual_y = pending_virtual_y_start;
					LayoutCoord max_column_height = LAYOUT_COORD_MIN;
					MultiColBreakpoint* b;

					for (b = next_break; b != new_next_break; b = b->Suc())
					{
						LayoutCoord next_break_virtual_y = b->GetVirtualY();
						int implicit_breaks = b->GetAssumedImplicitBreaksBefore();
						LayoutCoord column_height = (next_break_virtual_y - prev_break_virtual_y + LayoutCoord(implicit_breaks)) / LayoutCoord(implicit_breaks + 1);

						if (max_column_height < column_height)
						{
							max_column_height = column_height;
							break_following_tallest_col = b;
						}

						prev_break_virtual_y = next_break_virtual_y;
					}

					if (!b && max_column_height < (row_virtual_y_stop - prev_break_virtual_y) / (trailing_implicit_breaks + 1))
						// The longest distance is after the last explicit break.

						trailing_implicit_breaks ++;
					else
					{
						OP_ASSERT(break_following_tallest_col);

						if (break_following_tallest_col)
							/* Assume an(other) implicit column break before
							   this explicit break. */

							break_following_tallest_col->AssumeImplicitBreakBefore();
					}
				}

				/* All implicit column breaks have been distributed. Now find
				   the tallest column. */

				for (implicit_breaks = 0; implicit_breaks < free_breaks; implicit_breaks ++)
				{
					LayoutCoord prev_break_virtual_y = pending_virtual_y_start;
					LayoutCoord max_column_height = LAYOUT_COORD_MIN;
					MultiColBreakpoint* b;

					for (b = next_break; b != new_next_break; b = b->Suc())
					{
						LayoutCoord next_break_virtual_y = b->GetVirtualY();
						int implicit_breaks = b->GetAssumedImplicitBreaksBefore();
						LayoutCoord column_height = (next_break_virtual_y - prev_break_virtual_y + LayoutCoord(implicit_breaks)) / LayoutCoord(implicit_breaks + 1);

						if (max_column_height < column_height)
							max_column_height = column_height;

						prev_break_virtual_y = next_break_virtual_y;
					}

					LayoutCoord trailing_height = (row_virtual_y_stop - prev_break_virtual_y) / LayoutCoord(trailing_implicit_breaks + 1);

					if (!b && max_column_height < trailing_height)
						// The longest distance is after the last explicit break.

						max_column_height = trailing_height;

					initial_row_height = max_column_height;
				}
			}
			else
				// No explicit column breaks.

				initial_row_height = (row_virtual_y_stop - pending_virtual_y_start + LayoutCoord(free_breaks)) / LayoutCoord(free_breaks + 1);
		}
		else
			/* We are either not supposed to balance the columns, or there are
			   page breaks or just too many explicit column breaks ahead to do
			   column balancing. Use as much space as possible and allowed. */

			initial_row_height = row_virtual_y_stop - pending_virtual_y_start;

		if (ancestor_columnizer)
		{
			/* Clamp row height and max row height to what's available in the
			   ancestor multicol. If balancing is disabled, stretch row height
			   to what's available in the ancestor. */

			LayoutCoord space_used = ancestor_columnizer->stack_offset + new_row_y - ancestor_columnizer->col_virtual_y_start + ancestor_columnizer->GetFloatsHeight();
			LayoutCoord space = ancestor_columnizer->current_row_height - space_used;
			LayoutCoord max_space = ancestor_columnizer->max_row_height - space_used;

			if (balance_current_row)
			{
				if (initial_row_height > space)
					initial_row_height = space;
			}
			else
				if (initial_row_height < space)
					initial_row_height = space;

			if (max_row_height > max_space)
				max_row_height = max_space;
		}

		if (initial_row_height > max_row_height)
			initial_row_height = max_row_height;

		current_row_height = initial_row_height;
		row_open = TRUE;
		row_has_spanned_floats = FALSE;
	}

	// Cut margins crossing column boundaries.

	LayoutCoord margin_gap = pending_virtual_y_start - committed_virtual_y;

	if (margin_gap > 0)
	{
		if (balance_current_row && current_column_num < column_count)
			/* Reduce the row height. A part of some margin was skipped. This
			   wasn't accounted for in the initial row height calculation, so
			   reduce the height now. This means that the columns that we have
			   already created probably are too tall already, but let's at
			   least balance the content for the rest, instead of ending up
			   with unused or mostly empty column at the end. */

			IncreaseRowHeight(-(margin_gap / LayoutCoord(column_count - current_column_num)));

		/* Eat margins between last commit and the top of the column we're
		   about to create. */

		y_translation -= margin_gap;
	}

	LayoutCoord column_x = CalculateColumnX();
	ColumnRow* current_row = GetCurrentRow();
	Column* new_column = OP_NEW(Column, (start_element, pending_virtual_y_start, pending_start_offset, column_x, column_y, y_translation));

	if (!new_column)
		return FALSE;

	current_row->AddColumn(new_column);

	col_virtual_y_start = pending_virtual_y_start;
	column_open = TRUE;
	top_floats_height = LAYOUT_COORD_MIN;
	bottom_floats_height = LAYOUT_COORD_MIN;

	if (row_has_spanned_floats)
	{
		int i = 0;

		for (Column* column = current_row->GetFirstColumn(); column != new_column; column = column->Suc(), i++)
		{
			LayoutCoord this_col_top_floats_height(0);

			for (PaneFloatEntry* entry = column->GetFirstFloatEntry(); entry; entry = entry->Suc())
			{
				FloatedPaneBox* box = entry->GetBox();
				int column_span = box->GetColumnSpan();

				this_col_top_floats_height += box->GetHeight() + box->GetMarginTop() + box->GetMarginBottom();

				if (current_column_num - i < column_span)
					// This previous float affects this column.

					if (box->IsTopFloat())
					{
						if (top_floats_height < this_col_top_floats_height)
							top_floats_height = this_col_top_floats_height;
					}
					else
					{
						LayoutCoord this_col_bottom_floats_height(0);

						for (; entry; entry = entry->Suc())
						{
							box = entry->GetBox();
							this_col_bottom_floats_height += box->GetHeight() + box->GetMarginTop() + box->GetMarginBottom();
						}

						if (bottom_floats_height < this_col_bottom_floats_height)
							bottom_floats_height = this_col_bottom_floats_height;

						break;
					}
			}
		}
	}

	// No top/bottom floats means 0 height.

	if (top_floats_height == LAYOUT_COORD_MIN)
		top_floats_height = LayoutCoord(0);

	if (bottom_floats_height == LAYOUT_COORD_MIN)
		bottom_floats_height = LayoutCoord(0);

	for (Columnizer* columnizer = this; columnizer; columnizer = columnizer->ancestor_columnizer)
	{
		columnizer->pending_start_offset = LayoutCoord(0);
		columnizer->allow_column_stretch = TRUE;
	}

	if (add_queued_floats)
	{
		if (!AddQueuedFloats(COLUMNS_NONE))
			return FALSE;

		if (!GetCurrentColumn() ||
			top_floats_height + bottom_floats_height > 0 && max_row_height - (top_floats_height + bottom_floats_height) < minimum_height)
		{
			/* Floats in the way. No room for any content in this column. Give up
			   fitting any content here and get a new one straight away. */

			new_column->SetContentLess();

			return GetNewColumn(start_element, minimum_height, add_queued_floats);
		}
	}

	return TRUE;
}

/** Enter a spanned box before columnizing it. */

BOOL
Columnizer::EnterSpannedBox(BlockBox* box, Container* container)
{
	/* We already have a list of pending spanned boxes. They were created as
	   spanned boxes were encountered during layout. It's done like this in
	   order to store each spanned box's vertical margins. A different solution
	   could have been to create designated box classes for spanned boxes, but
	   that would require at least 3 new classes (regular block, list item,
	   opacity!=1), so I decided against it. */

	SpannedElm* spanned_elm = next_spanned_elm;

	OP_ASSERT(spanned_elm);

	if (spanned_elm)
	{
		OP_ASSERT(spanned_elm->GetSpannedBox() == box);

		LayoutCoord stack_position = box->GetStackPosition();

		/* Advance past the bottom margin of the last element in column, so it
		   gets committed. */

		AdvanceHead(stack_position - spanned_elm->GetMarginTop());

		if (!FinalizeColumn())
			return FALSE;

		SetStopBefore(box);

		if (!AddQueuedFloats(COLUMNS_ANY))
			return FALSE;

		CloseRow();

		/* Collapse margins between the top of this spanned element and the
		   bottom of the previous adjacent spanned element (if any). */

		LayoutCoord margin_top = spanned_elm->GetMarginTop();
		LayoutCoord total_margin = pending_margin + margin_top;

		if (pending_margin >= 0)
		{
			if (pending_margin < margin_top)
				pending_margin = margin_top;
			else
				if (margin_top < 0)
					pending_margin += margin_top;
		}
		else
			if (pending_margin > margin_top)
				pending_margin = margin_top;
			else
				if (margin_top > 0)
					pending_margin += margin_top;

		/* The adjacent spanned elements were laid out as if no margin
		   collapsing occurred. Compensate for that, now that we know the final
		   result. */

		y_translation -= total_margin - pending_margin;

#ifdef PAGED_MEDIA_SUPPORT
		if (!trial && !ancestor_columnizer && layout_info.paged_media != PAGEBREAK_OFF)
		{
			page_break_policy_before_current = box->GetPageBreakPolicyBefore();

			if (BreakAllowedBetween(page_break_policy_after_previous, page_break_policy_before_current))
				last_allowed_page_break_after = rows.Last();
		}
#endif // PAGED_MEDIA_SUPPORT

		AllocateContent(stack_position, box);
		AdvanceHead(stack_position);
		committed_virtual_y = virtual_y;
		pending_virtual_y_start = virtual_y;

		OP_ASSERT(next_break);

		if (next_break)
		{
			// Skip past the break caused by this spanned element.

			OP_ASSERT(next_break->IsSpannedElement());
			remaining_floats_virtual_height += next_break->GetPaneFloatsVirtualHeight();
			next_break = next_break->Suc();
		}

		span_all = TRUE;
	}

	return TRUE;
}

/** Leave a spanned box after having columnized it. */

BOOL
Columnizer::LeaveSpannedBox(BlockBox* box, Container* container)
{
	SpannedElm* spanned_elm = next_spanned_elm;

	OP_ASSERT(spanned_elm);

	if (spanned_elm)
	{
		OP_ASSERT(spanned_elm->GetSpannedBox() == box);

		/* This was a spanned element. If we somehow end up with more than one
		   column, things are going to look stupid. */

		OP_ASSERT(!GetCurrentRow() || GetCurrentRow()->GetColumnCount() == 1);

		last_allocated_element = box;

		if (!FinalizeColumn())
			return FALSE;

		CloseRow();

		pending_margin = spanned_elm->GetMarginBottom();
		AdvanceHead(box->GetStackPosition() + box->GetLayoutHeight() + pending_margin);
		committed_virtual_y = virtual_y;
		pending_virtual_y_start = virtual_y;

		next_spanned_elm = (SpannedElm*) next_spanned_elm->Suc();
		span_all = FALSE;
	}

	return TRUE;
}

/** Skip a spanned box. */

void
Columnizer::SkipSpannedBox(BlockBox* box)
{
	SpannedElm* spanned_elm = next_spanned_elm;

	OP_ASSERT(spanned_elm);

	if (spanned_elm)
	{
		OP_ASSERT(spanned_elm->GetSpannedBox() == box);
		next_spanned_elm = (SpannedElm*) spanned_elm->Suc();
	}
}

/** Add pane-attached (GCPM) floating box. */

void
Columnizer::AddPaneFloat(FloatedPaneBox* box)
{
	int start_pane = -1;

	if (box->IsFarCornerFloat())
		start_pane = column_count - box->GetColumnSpan();

	BOOL in_next_row = FALSE;

#ifdef PAGED_MEDIA_SUPPORT
	if (box->IsForNextPage())
		if (ancestor_columnizer)
			/* Note: we're not checking if the ancestor is a paginator, so if
			   it isn't, the "next page" thing will really mean "next
			   column". Nice or bad side-effect? */

			in_next_row = TRUE;
		else
			start_pane = current_column_num + 1;
#endif // PAGED_MEDIA_SUPPORT

	box->IntoPaneFloatList(float_queue, start_pane, in_next_row);
}

/** Find space for pending content. */

BOOL
Columnizer::FindSpace(LayoutCoord new_virtual_y)
{
	if (GetCurrentColumn())
	{
		LayoutCoord uncommitted_height = new_virtual_y - committed_virtual_y;

		if (GetSpaceLeft() < uncommitted_height)
		{
			/* Content doesn't fit in the current column given the current row
			   height. But we may be able to stretch it. */

			if (ancestor_columnizer)
				// Does the ancestor allow us to stretch as much as we would need?

				if (!ancestor_columnizer->FindSpace(GetBottom(new_virtual_y) + ancestor_columnizer->stack_offset))
					// No. We're out of space, then.

					return FALSE;

			if (!StretchColumn(uncommitted_height))
				/* Couldn't stretch the column to make the content fit. We're
				   out of space. */

				return FALSE;
		}
	}

	return TRUE;
}

/** Commit pending content to one or more columns. */

BOOL
Columnizer::CommitContent(BOOL check_available_space)
{
	if (first_uncommitted_element.IsSet() && virtual_y >= earliest_break_virtual_y)
	{
		// We have something to commit, and we are even allowed to do it.

		if (!GetCurrentColumn() || check_available_space && !FindSpace(virtual_y))
			// Need new column

			if (!GetNewColumn(first_uncommitted_element, virtual_y - pending_virtual_y_start))
				return FALSE;

		if (ancestor_columnizer)
		{
			LayoutCoord new_ancestor_virtual_y = GetBottom(virtual_y) + ancestor_columnizer->stack_offset;

			if (ancestor_columnizer->virtual_y < new_ancestor_virtual_y)
				ancestor_columnizer->virtual_y = new_ancestor_virtual_y;

			if (!ancestor_columnizer->CommitContent(FALSE))
				return FALSE;
		}

		GetCurrentColumn()->SetStopAfterElement(last_allocated_element);

		committed_virtual_y = virtual_y;
		pending_virtual_y_start = virtual_y;
		pending_start_offset = LayoutCoord(0);
		first_uncommitted_element.Reset();

		if (!AddQueuedFloats(COLUMNS_NONE))
			return FALSE;
	}

	return TRUE;
}

/** Flush pane floats in ancestors. */

void
Columnizer::FlushFloats(LayoutCoord virtual_bottom)
{
	virtual_bottom += stack_offset;

	if (virtual_y < virtual_bottom)
	{
		virtual_y = virtual_bottom;

		committed_virtual_y = virtual_y;
		pending_virtual_y_start = virtual_y;

		if (ancestor_columnizer)
			ancestor_columnizer->FlushFloats(GetBottom(virtual_y));
	}
}

/** Fill up with any queued floats that might fit in the current column. */

BOOL
Columnizer::AddQueuedFloats(ColumnCreationPolicy policy)
{
	const ColumnBoundaryElement null_elm;
	BOOL allow_new_columns = policy != COLUMNS_NONE;

	if (span_all)
		return TRUE;

	while (PaneFloatEntry* entry = float_queue.First())
	{
		FloatedPaneBox* box = entry->GetBox();

		if (ancestor_columnizer && !box->IsForNextRow() &&
			current_column_num > 0 && box->GetStartPane() != -1 &&
			box->GetStartPane() < current_column_num)
		{
			/* Not enough columns left for this float. Move to next row (and
			   next outer pane) and continue processing the floats queue from
			   the head. Moving one float to the next row may move it backwards
			   in the queue and reveal others that are already ready for
			   addition. */

			box->MoveToNextRow();
			continue;
		}

		if (policy == COLUMNS_ANY || policy == COLUMNS_ONE_ROW)
		{
			/* In this mode we are allowed (and supposed to, if necessary) add
			   new column / pages if a float cannot live on the current
			   one. This typically happens when we finish columnization and
			   there are still floats in the queue that haven't got a suitable
			   column or page to live in yet. */

			if (policy == COLUMNS_ANY && ancestor_columnizer && box->IsForNextRow())
			{
				/* Float is for the next row. Create columns until we end up on
				   a new row. */

				ColumnRow* current_row = GetCurrentRow();

				if (!current_row)
				{
					/* No row created yet, which also means no column
					   yet. Creating a new column should solve that. */

					if (!GetNewColumn(null_elm, LayoutCoord(0), FALSE))
						return FALSE;

					current_row = GetCurrentRow();
					OP_ASSERT(current_row);
				}

				do
					// Create columns until we get a new row.

					if (!GetNewColumn(null_elm, LayoutCoord(0), FALSE))
						return FALSE;
				while (current_row == GetCurrentRow());
			}

			if (box->GetStartPane() != -1 && !box->IsForNextRow())
				/* Float has a designated start pane number in this row. Create
				   new columns until we get there. */

				while (current_column_num < box->GetStartPane())
					if (!GetNewColumn(null_elm, LayoutCoord(0), FALSE))
						return FALSE;
		}
		else
			if (box->IsForNextRow() || box->GetStartPane() > current_column_num)
				/* We have reached a float that we are not yet ready to add
				   (it's either for the next row, or we haven't reached its
				   start pane yet). Stop for now. */

				break;

		LayoutCoord height = box->GetHeight() + box->GetMarginTop() + box->GetMarginBottom();
		BOOL new_column = FALSE;

		if (!GetCurrentColumn())
			if (allow_new_columns)
			{
				new_column = TRUE;

				if (!GetNewColumn(box, height, FALSE))
					return FALSE;

				if (policy == COLUMNS_ONE)
					// That was it. Now we cannot add any more.

					allow_new_columns = FALSE;
			}
			else
				break;

		if (new_column || GetSpaceUsed() == 0 || FindSpace(virtual_y + height))
		{
			int column_span = box->GetColumnSpan();

			if (column_span > 1)
				row_has_spanned_floats = TRUE;

			remaining_floats_virtual_height -= height * LayoutCoord(column_span);

			entry->Out();
			GetCurrentColumn()->AddFloat(box);

			if (box->IsTopFloat())
				top_floats_height += height;
			else
				bottom_floats_height += height;

#ifdef PAGED_MEDIA_SUPPORT
			if (box->IsPageBreakAfterForced())
			{
				if (!ExplicitlyBreakPage(null_elm, TRUE))
					return FALSE;
			}
			else
#endif // PAGED_MEDIA_SUPPORT
				if (box->IsColumnBreakAfterForced())
				{
					int remaining_columns = column_count - (current_column_num + column_span);
					int span_left = column_span;

					do
					{
						if (balance_current_row && remaining_columns > 0)
							// Compensate for the space wasted because of the forced break.

							IncreaseRowHeight(GetSpaceLeft() / LayoutCoord(remaining_columns));

						if (!ExplicitlyBreakColumn(null_elm, TRUE))
							return FALSE;

						if (--span_left > 0)
						{
							/* Skip empty columns that the float spans. We don't check if
							   we're allowed to create new columns here, but there's no
							   need to care in this case. */

							if (!GetNewColumn(null_elm, LayoutCoord(0), FALSE))
								return FALSE;

							continue;
						}

						break;
					}
					while (1);
				}
		}
		else
			if (policy == COLUMNS_ANY || policy == COLUMNS_ONE_ROW)
			{
				CloseColumn();

				if (current_column_num == column_count)
				{
					if (policy == COLUMNS_ONE_ROW)
						break;

					CloseRow();

					for (PaneFloatEntry* entry = float_queue.First(); entry; entry = entry->Suc())
						entry->GetBox()->ResetIsForNextRow();
				}

				continue;
			}

		break;
	}

	return TRUE;
}

/** Add and the empty parts of a block, and split it into multiple columns, if necessary. */

BOOL
Columnizer::AddEmptyBlockContent(BlockBox* block, Container* container)
{
	LayoutCoord layout_height = block->GetLayoutHeight();

	if (virtual_y < earliest_break_virtual_y)
	{
		// Adjacent to unbreakable content.

		LayoutCoord unbreakable_amount = earliest_break_virtual_y - stack_offset;

		if (unbreakable_amount >= layout_height)
		{
			AdvanceHead(layout_height);

			/* Since something unbreakable already sticks at least as deep
			   as this block, there's no point in continuing; we are not
			   going to be able to split this block into multiple
			   columns. */

			return TRUE;
		}
		else
			// Move past adjacent unbreakable content.

			AdvanceHead(unbreakable_amount);
	}

	if (virtual_y < stack_offset)
		/* Empty block and positive top margin. Let's walk to the top border
		   edge. Margins shouldn't be columnized. */

		AdvanceHead(LayoutCoord(0));

	LayoutCoord height_processed = virtual_y - stack_offset;

	if (height_processed < layout_height)
	{
		/* If we didn't manage to go through the whole block (due to lack of
		   natural potential break points (e.g. when there is little or no
		   child content at all)), split the remaining part of the block into
		   columns (if necessary) now. */

		last_allocated_element = block;

		if (!CommitContent())
			return FALSE;

		if (!GetCurrentRow())
			// This happens when a spanned element is the last child of this block.

			if (!GetNewColumn(block, LayoutCoord(0)))
				return FALSE;

		if (current_row_height <= 0)
		{
			/* In constrained height situations, row height may become
			   zero. Distributing the block over an infinite number of columns
			   is obviously not the solution then. */

			AdvanceHead(layout_height);
			return TRUE;
		}

		do
		{
			LayoutCoord advancement = GetSpaceLeft();

			if (advancement <= 0)
			{
				/* We are out of space. Close the current column. We don't want
				   to stretch it or anything like that. */

				CloseColumn();
				advancement = current_row_height;
			}

			AllocateContent(height_processed, block, height_processed);

			height_processed += advancement;
			if (height_processed > layout_height)
				height_processed = layout_height;

			AdvanceHead(height_processed);

			if (height_processed < layout_height)
				// The current column cannot hold all remaining content.

				if (!CommitContent())
					return FALSE;
		}
		while (height_processed < layout_height);
	}

	return TRUE;
}

/** Attempt to stretch the column to make some element fit. */

BOOL
Columnizer::StretchColumn(LayoutCoord element_height)
{
	/* If we're in the last column (and there's no ancestor to create
	   another row in), be a bit more lenient. Allow as much
	   stretching as requested, as long as it doesn't exceed the row
	   maximum height. This comes in handy when our initial height
	   guess was too low. As long as the height limit (if any) isn't
	   reached, we should not use more columns than specified. If
	   we're not in the last column, on the other hand, be more strict
	   when it comes to stretching: */

	if (ancestor_columnizer || current_column_num + 1 < column_count)
	{
		if (!allow_column_stretch)
			return FALSE;

		allow_column_stretch = FALSE;

		if (GetSpaceUsed() >= initial_row_height)
			/* We're already past the initial row height, so don't stretch any
			   further. */

			return FALSE;
	}

	// Stretch the row, but not beyond its max height.

	current_row_height = GetSpaceUsed() + element_height;

	if (current_row_height > max_row_height)
	{
		/* We may may have stretched it to maximum, but that was not enough for
		   the content to fit. */

		current_row_height = max_row_height;
		reached_max_height = TRUE;

		return FALSE;
	}

	return TRUE;
}

/** Increase current row height. */

void
Columnizer::IncreaseRowHeight(LayoutCoord increase)
{
	current_row_height += increase;

	if (current_row_height > max_row_height)
	{
		current_row_height = max_row_height;
		reached_max_height = TRUE;
	}

	if (current_row_height < 0)
		current_row_height = LayoutCoord(0);
}

/** Explicitly break (end) the current page (and therefore column and row as well). */

/* virtual */ BOOL
Columnizer::ExplicitlyBreakPage(const ColumnBoundaryElement& stop_before, BOOL closed_by_pane_float, BOOL has_child_columnizer)
{
	if (!closed_by_pane_float)
	{
		if (!FinalizeColumn())
			return FALSE;

		if (!has_child_columnizer)
		{
			// Must do this before closing the row.

			SetStopBefore(stop_before);

			/* Row is soon closing. If there are any floats still queued for this
			   row, add them now. */

			if (!AddQueuedFloats(COLUMNS_ONE_ROW))
				return FALSE;
		}
	}

	CloseRow();

	if (ancestor_columnizer)
		// Break columns in all ancestor columnizers and get the page closed.

		if (!ancestor_columnizer->ExplicitlyBreakPage(stop_before, FALSE, TRUE))
			return FALSE;

#ifdef PAGED_MEDIA_SUPPORT
	if (!trial)
		ClosePage();
#endif // PAGED_MEDIA_SUPPORT

	if (!closed_by_pane_float && !has_child_columnizer)
	{
		OP_ASSERT(next_break);

		if (next_break)
		{
			// Skip past the page break.

			OP_ASSERT(next_break->IsRowBreak() && !next_break->IsSpannedElement());
			remaining_floats_virtual_height += next_break->GetPaneFloatsVirtualHeight();
			next_break = next_break->Suc();
		}
	}

	return TRUE;
}

/** Explicitly break (end) the current column. */

/* virtual */ BOOL
Columnizer::ExplicitlyBreakColumn(const ColumnBoundaryElement& stop_before, BOOL closed_by_pane_float)
{
	if (closed_by_pane_float)
		CloseColumn();
	else
	{
		if (!FinalizeColumn())
			return FALSE;

		OP_ASSERT(next_break);

		if (next_break)
		{
			// Skip past the column break.

			OP_ASSERT(!next_break->IsRowBreak());
			remaining_floats_virtual_height += next_break->GetPaneFloatsVirtualHeight();
			next_break = next_break->Suc();
		}

		SetStopBefore(stop_before);
	}

	return TRUE;
}

/** Get the row we're currently at, or NULL if none. */

ColumnRow*
Columnizer::GetCurrentRow() const
{
	if (!row_open)
		return NULL;

	return rows.Last();
}

/** Get the column we're currently at, or NULL if none. */

Column*
Columnizer::GetCurrentColumn() const
{
	if (!column_open)
		return NULL;

	if (ColumnRow* row = GetCurrentRow())
		return row->GetLastColumn();

	return NULL;
}

/** Finish and close current column. */

void
Columnizer::CloseColumn()
{
	if (Column* current_column = GetCurrentColumn())
	{
		LayoutCoord column_height = GetSpaceUsed();
		LayoutCoord floats_height = top_floats_height + bottom_floats_height;

		current_column->SetTopFloatsHeight(top_floats_height);
		current_column->SetBottomFloatsHeight(bottom_floats_height);
		current_column->TranslateY(top_floats_height);
		current_column->SetHeight(column_height - floats_height);
		current_column->PositionTopFloats();

		UpdateHeight(column_height);
		column_y += column_y_stride;
		y_translation -= column_height - floats_height;
		current_column_num ++;

		if (ancestor_columnizer && top_floats_height + bottom_floats_height)
			/* Pane floats are never committed, so we need to make sure that
			   they are accounted for. */

			ancestor_columnizer->FlushFloats(GetBottom(committed_virtual_y));
	}

	column_open = FALSE;
}

/** Finish and close current row. */

void
Columnizer::CloseRow(BOOL stretch_to_ancestor)
{
	if (ColumnRow* current_row = GetCurrentRow())
	{
		CloseColumn();

		column_y = LayoutCoord(0);

		current_column_num = 0;

		LayoutCoord row_height = current_row->GetHeight();

		if (stretch_to_ancestor && ancestor_columnizer && ancestor_columnizer->height_restricted)
			if (row_height < max_row_height)
				row_height = max_row_height;

		for (Column* column = current_row->GetFirstColumn(); column; column = column->Suc())
			column->PositionBottomFloats(row_height);
	}

	row_open = FALSE;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Finish and close current page. */

/* virtual */ void
Columnizer::ClosePage()
{
	CloseRow();

	if (ancestor_columnizer)
		ancestor_columnizer->ClosePage();
	else
		if (!trial)
			page_open = FALSE;
}

/** Set page breaking policy before the element that is about to be added. */

void
Columnizer::SetPageBreakPolicyBeforeCurrent(BREAK_POLICY page_break_policy)
{
	if (!first_uncommitted_element.IsSet() && !ancestor_columnizer)
		page_break_policy_before_current = page_break_policy;
}

/** Set page breaking policy after the element that was just added. */

void
Columnizer::SetPageBreakPolicyAfterPrevious(BREAK_POLICY page_break_policy)
{
	if ((row_open || !first_uncommitted_element.IsSet()) && !ancestor_columnizer)
		page_break_policy_after_previous = page_break_policy;
}

#endif // PAGED_MEDIA_SUPPORT

/** Finalize column. Commit pending content. */

BOOL
Columnizer::FinalizeColumn()
{
	if (virtual_y < earliest_break_virtual_y)
		// Pull ourselves past all content.

		virtual_y = earliest_break_virtual_y;

	if (!CommitContent())
		return FALSE;

	if (!AddQueuedFloats(COLUMNS_ONE))
		return FALSE;

	CloseColumn();

	return TRUE;
}

/** Finalize columnization. Commit pending content and floats. */

BOOL
Columnizer::Finalize(BOOL stretch_to_ancestor)
{
	if (!FinalizeColumn())
		return FALSE;

	if (!AddQueuedFloats(COLUMNS_ANY))
		return FALSE;

	CloseRow(stretch_to_ancestor);

	return TRUE;
}

/** Set the element before which the last column added shall stop. */

void
Columnizer::SetStopBefore(const ColumnBoundaryElement& elm)
{
	if (ColumnRow* row = GetCurrentRow())
	{
		if (Column* last_column = row->GetLastColumn())
			/* Need to stop right before the spanned box. If the spanned
			   box is the first child of some block, there's no element
			   after which to stop. So we stop before this spanned box
			   instead. */

			last_column->SetStopBeforeElement(elm);

		if (ancestor_columnizer)
			ancestor_columnizer->SetStopBefore(elm);
	}
}

#ifdef PAGED_MEDIA_SUPPORT

Paginator::Paginator(const LayoutInfo& layout_info,
					 LayoutCoord max_height,
					 LayoutCoord page_width,
					 LayoutCoord column_y_stride,
					 LayoutCoord top_offset,
					 LayoutCoord vertical_padding,
					 BOOL height_restricted)
	: Columnizer(NULL,
				 FALSE,
				 layout_info,
				 page_list_holder,
				 NULL,
				 NULL,
				 max_height,
				 INT_MAX,
				 page_width,
				 page_width,
				 LayoutCoord(0),
				 column_y_stride,
				 top_offset,
				 LAYOUT_COORD_MAX,
				 LayoutCoord(0),
				 height_restricted,
#ifdef SUPPORT_TEXT_DIRECTION
				 FALSE,
#endif // SUPPORT_TEXT_DIRECTION
				 FALSE),
	  vertical_padding(vertical_padding),
	  page_height_auto(column_y_stride == CONTENT_HEIGHT_AUTO)
{
}

/** Update row and/or container height. */

/* virtual */ void
Paginator::UpdateHeight(LayoutCoord height)
{
	Columnizer::UpdateHeight(height);

	if (!page_height_auto)
		return;

	LayoutCoord new_column_y_stride = height - (top_floats_height + bottom_floats_height) + vertical_padding;

	if (column_y_stride < new_column_y_stride)
	{
		if (Column* col = GetCurrentRow()->GetFirstColumn())
		{
			// Increased page height in vertical pagination; update page positions.

			col = col->Suc();

			if (col)
			{
				LayoutCoord increase = new_column_y_stride - column_y_stride;
				LayoutCoord acc_increase = increase;

				OP_ASSERT(column_y_stride != CONTENT_HEIGHT_AUTO);

				for (; col; col = col->Suc(), acc_increase += increase)
				{
					col->MoveDown(acc_increase);
					column_y += increase;
				}
			}
		}

		column_y_stride = new_column_y_stride;
	}
}

/** Explicitly break (end) the current page (and therefore column and row as well). */

/* virtual */ BOOL
Paginator::ExplicitlyBreakPage(const ColumnBoundaryElement& stop_before, BOOL closed_by_pane_float, BOOL has_child_columnizer)
{
	if (closed_by_pane_float)
		CloseColumn();
	else
	{
		if (!FinalizeColumn())
			return FALSE;

		SetStopBefore(stop_before);
	}

	return TRUE;
}

/** Explicitly break (end) the current column. */

/* virtual */ BOOL
Paginator::ExplicitlyBreakColumn(const ColumnBoundaryElement& stop_before, BOOL closed_by_pane_float)
{
	OP_ASSERT(!"Attempting to insert a column break while not inside a multicol container");

	return TRUE;
}

/** Finish and close current page. */

/* virtual */ void
Paginator::ClosePage()
{
	/* "WTF - nothing?", you say? Yeah, I know... This is for the paged mode
	   done in printing, which is a completely different machinery from the one
	   that assists a paged container (this class). Some wonderful day we
	   should consolidate the two of them. */
}

/** Get page height. */

LayoutCoord
Paginator::GetPageHeight() const
{
	if (ColumnRow* all_pages = page_list_holder.Last())
	{
		OP_ASSERT(!all_pages->Pred());

		return all_pages->GetHeight();
	}

	return LayoutCoord(0);
}

/** Update bottom floats when final paged container height is known. */

void
Paginator::UpdateBottomFloats(LayoutCoord content_height)
{
	OP_ASSERT(page_list_holder.Empty() || page_list_holder.Cardinal() == 1);

	if (ColumnRow* last_row = page_list_holder.Last())
	{
		/* The bottom edge of the last row should be the same as the bottom
		   content edge of the multi-pane container. */

		LayoutCoord row_y = last_row->GetPosition();
		LayoutCoord stretched_row_height = content_height - row_y;

		for (Column* column = last_row->GetFirstColumn(); column; column = column->Suc())
			column->PositionBottomFloats(stretched_row_height);
	}
}

/** Move all pages created during pagination into 'pages'. */

void
Paginator::GetAllPages(PageStack& pages)
{
	OP_ASSERT(!pages.GetFirstColumn());

	if (ColumnRow* all_pages = page_list_holder.Last())
	{
		OP_ASSERT(!all_pages->Pred());

		while (Column* page = all_pages->GetFirstColumn())
		{
			page->Out();
			pages.AddColumn(page);
		}
	}
}

#endif // PAGED_MEDIA_SUPPORT

ColumnFinder::ColumnFinder(const Box* box, ColumnRow* first_row, LayoutCoord top_border_padding, ColumnFinder* ancestor_cf)
	: box(box),
	  ancestor_cf(ancestor_cf),
	  cur_row(first_row),
	  cur_column(NULL),
	  start_row(NULL),
	  start_column(NULL),
	  box_translation_x(0),
	  box_translation_y(0),
	  start_offset_from_top(0),
	  end_row(NULL),
	  end_column(NULL),
	  virtual_y(0),
	  stack_offset(-top_border_padding),
	  descendant_translation_y(0),
	  descendant_translation_x(0)
{
	OP_ASSERT(cur_row);
	OP_ASSERT(box);

	if (cur_row)
		cur_column = cur_row->GetFirstColumn();

	border_rect.left = 0;
	border_rect.right = 0;
	border_rect.top = 0;
	border_rect.bottom = 0;

	if (box->IsFloatedPaneBox())
		/* Look for the first column that is occupied the pane-attached
		   float. It may not live in this multi-pane container at all, but
		   rather in some descendant multi-pane container, but if we do find it
		   here, we're actually done, and wont't have to traverse through the
		   layout tree. */

		for (ColumnRow* row = first_row; row; row = row->Suc())
			for (Column* column = row->GetFirstColumn(); column; column = column->Suc())
				if (column->HasFloat((FloatedPaneBox*) box))
				{
					cur_column = column;
					cur_row = row;
					SetBoxStartFound();
					SetBoxEndFound();
					border_rect.bottom = box->GetHeight();
				}
}

/** Update the union of the border area in each column in which the target box lives. */

void
ColumnFinder::UpdateBorderRect(LayoutCoord virtual_y)
{
	if (start_row && !box->IsFloatedPaneBox())
	{
		LayoutCoord virtual_start_y(cur_column->GetVirtualY());
		LayoutCoord x(cur_column->GetX() - start_column->GetX());
		LayoutCoord y = (cur_row->GetPosition() + cur_column->GetY()) -
						 (start_row->GetPosition() + start_column->GetY()) +
						 virtual_y - virtual_start_y - start_offset_from_top;

		if (border_rect.left > x)
			border_rect.left = x;

		if (border_rect.right < x)
			border_rect.right = x;

		if (border_rect.top > y)
			border_rect.top = y;

		if (border_rect.bottom < y)
			border_rect.bottom = y;
	}
}

/** Set position of the current element we're examining. */

void
ColumnFinder::SetPosition(LayoutCoord local_virtual_y)
{
	virtual_y = stack_offset + local_virtual_y;

	/* Advance to next column/spanned element as long as the virtual Y position
	   is beyond the bounds of the current one. */

	do
	{
		if (cur_column && cur_column->Suc())
		{
			Column* next_column = cur_column->Suc();

			if (virtual_y >= next_column->GetVirtualY())
			{
				// Record the bottom of the column we're about to leave:

				UpdateBorderRect(next_column->GetVirtualY());

				cur_column = next_column;

				// Record the top of the column we just entered:

				UpdateBorderRect(next_column->GetVirtualY());

				continue;
			}
		}
		else
		{
			if (ColumnRow* next_row = cur_row->Suc())
			{
				if (Column* next_column = next_row->GetFirstColumn())
				{
					if (virtual_y >= next_column->GetVirtualY())
					{
						// Record the bottom of the column we're about to leave:

						UpdateBorderRect(next_column->GetVirtualY());

						cur_column = next_column;
						cur_row = next_row;

						// Record the top of the column we just entered:

						UpdateBorderRect(next_column->GetVirtualY());
					}
				}
				else
					OP_ASSERT(!"Row with no columns");

				if (cur_row == next_row)
				{
					if (ancestor_cf)
						/* We advanced to the next row (or spanned element). Notify the
						   ancestor, since this may mean that the ancestor needs to move
						   to the next column. */

						ancestor_cf->SetPosition(cur_row->GetPosition());

					continue;
				}
			}
		}

		break; // Didn't advance. We're done.
	}
	while (1);
}

/** Found the start of the box. */

void
ColumnFinder::SetBoxStartFound()
{
	OP_ASSERT(cur_row);
	OP_ASSERT(!start_row);

	start_column = cur_column;
	start_row = cur_row;

	box_translation_x = cur_column->GetX();
	box_translation_y = cur_row->GetPosition() + cur_column->GetY();

	if (!box->IsFloatedPaneBox())
		box_translation_y += cur_column->GetTranslationY();

	if (cur_column)
		start_offset_from_top = virtual_y - cur_column->GetVirtualY();

	if (ancestor_cf)
		ancestor_cf->SetBoxStartFound();
}

/** Found the end of the box. We're done. */

void
ColumnFinder::SetBoxEndFound()
{
	OP_ASSERT(cur_row);
	OP_ASSERT(!end_row);

	end_column = cur_column;
	end_row = cur_row;

	// Record the end position of the target box.

	UpdateBorderRect(virtual_y);

	if (ancestor_cf)
		ancestor_cf->SetBoxEndFound();
}
