/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file traverse.cpp
 *
 * Implementation of the traversal objects.
 *
 * @author Geir Ivarsøy
 * @author Karl Anders Øygard
 *
 */

#include "core/pch.h"

#include "modules/logdoc/logdoc_util.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/layout/box/inline.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/cascade.h"
#include "modules/probetools/probepoints.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/cssprops.h"

#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/text_edit_element_of_interest.h"
# include "modules/widgets/finger_touch/form_element_of_interest.h"
# include "modules/widgets/finger_touch/anchor_element_of_interest.h"
#endif // NEARBY_ELEMENT_DETECTION

#include "modules/img/image.h"
#include "modules/logdoc/opelminfo.h"
#include "modules/logdoc/link.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/winman.h"
#include "modules/display/styl_man.h"
#include "modules/url/url_api.h"
#include "modules/layout/numbers.h"
#include "modules/logdoc/src/textdata.h"
#include "modules/dochand/win.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/util/opautoptr.h"

#include "modules/locale/oplanguagemanager.h"

#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif // DOCUMENT_EDIT_SUPPORT

#ifdef LAYOUT_CARET_SUPPORT
# include "modules/doc/caret_manager.h"
#endif // LAYOUT_CARET_SUPPORT

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_image.h"
# include "modules/svg/svg_tree_iterator.h"
#endif // SVG_SUPPORT

# include "modules/doc/searchinfo.h"

#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
#endif

#include "modules/windowcommander/OpViewportController.h"

#ifndef SEL_DEFAULT_BGCOLOR
# define SEL_DEFAULT_BGCOLOR OP_RGB(0, 0, 255)
#endif // SEL_DEFAULT_BGCOLOR

#define SEL_BR_WIDTH 5 ///< Width of selection drawn on selected linebreaks (editable mode)

#define SIGNIFICANT_BORDER_WIDTH 5

/* Intersects 'rect' with 'second'. Takes in account possible infinite
   extents of 'rect'. Returns whether the resulting rect is
   non-empty.*/

static BOOL IntersectRectWith(RECT& rect, const OpRect& second)
{
	rect.left = MAX(rect.left, second.x);
	rect.top = MAX(rect.top, second.y);

	rect.right = MIN(rect.right, second.Right());
	rect.bottom = MIN(rect.bottom, second.Bottom());

	return rect.left < rect.right && rect.top < rect.bottom;
}

/** Helper function to measure a text fragment substring.

	This function should be used where ever a tab character can occur.
	Tabs must be special cased because their width depends on their
	placement in the document.  Otherwise, this is simple a wrapper
	around VisualDevice::GetTxtExtentEx(). */

static inline LayoutCoord MeasureSubstring(VisualDevice *vis_dev, const uni_char *start,
										   int length, LayoutCoord word_width, int text_format, BOOL is_tab_character)
{
	if (length == 0)
		return LayoutCoord(0);
	else
		if (is_tab_character)
		{
			OP_ASSERT(*start == '\t' && length == 1);
			return word_width;
		}
		else
			return LayoutCoord(vis_dev->GetTxtExtentEx(start, length, text_format));
}

/** Helper function to return word's x position in line relative to container's border box.
	- segment is segment to which word belongs
	- word_offset is word's offset in segment */

static inline int GetWordX(const LineSegment& segment, int word_offset)
{
	return segment.x +
#ifdef SUPPORT_TEXT_DIRECTION
			segment.offset +
#endif //SUPPORT_TEXT_DIRECTION
			word_offset;
}

/** Calculate the clip rectangle of the specified column or page. */

static OpRect
CalculatePaneClipRect(LayoutProperties* multipane_lprops, Column* pane, HTML_Element* target_element, BOOL is_column)
{
	Box* mp_box = multipane_lprops->html_element->GetLayoutBox();
	const HTMLayoutProperties& mp_props = *multipane_lprops->GetProps();

#ifdef PAGED_MEDIA_SUPPORT
	if (!is_column)
	{
		PagedContainer* pc = (PagedContainer*) mp_box->GetContainer();

		OP_ASSERT(pc->GetPagedContainer());

		return OpRect(mp_props.border.left.width,
					  -mp_props.padding_top,
					  pc->GetWidth() - mp_props.border.right.width - mp_props.border.left.width,
					  pc->GetHeight() - pc->GetPageControlHeight() - mp_props.border.bottom.width);
	}
	else
#endif // PAGED_MEDIA_SUPPORT
	{
		MultiColumnContainer* mc = (MultiColumnContainer*) mp_box->GetContainer();

		OP_ASSERT(mc->GetMultiColumnContainer());

		LayoutCoord left_border_padding = LayoutCoord(mp_props.border.left.width) + mp_props.padding_left;
		LayoutCoord column_width = mc->GetColumnWidth();
		LayoutCoord column_gap = mc->GetColumnGap();
		LayoutCoord left_limit = left_border_padding - column_gap / LayoutCoord(2);
		LayoutCoord right_limit = left_border_padding + column_width + LayoutCoord((column_gap + 1) / 2);
		BOOL clip_left;
		BOOL clip_right;

#ifdef SUPPORT_TEXT_DIRECTION
		if (mp_props.direction == CSS_VALUE_rtl)
		{
			clip_left = pane->Suc() != NULL;
			clip_right = pane->Pred() != NULL;
		}
		else
#endif // SUPPORT_TEXT_DIRECTION
		{
			clip_left = pane->Pred() != NULL;
			clip_right = pane->Suc() != NULL;
		}

		/* Limit clip rectangle in the horizontal direction. Attempt to leave y
		   and height alone, by making them as far apart as possible. */

		int left = clip_left ? left_limit : LAYOUT_COORD_MIN / 200;
		int right = clip_right ? right_limit - left : LAYOUT_COORD_MAX / 100;

		if (target_element)
		{
			FloatedPaneBox* floated_pane_box = NULL;

			/* Pane-attached floats may span multiple columns. Look for the nearest float
			   descendant and expand the clip rectangle if necessary. We need to do this
			   for every column, since we don't know which column the float belongs to,
			   traversal-wise. */

			for (HTML_Element* elm = target_element; elm != multipane_lprops->html_element; elm = elm->Parent())
				if (elm->GetLayoutBox()->IsFloatedPaneBox())
					floated_pane_box = (FloatedPaneBox*) elm->GetLayoutBox();

			if (floated_pane_box)
			{
				int column_span = floated_pane_box->GetColumnSpan();

				if (column_span > 1)
				{
					Column* last_column = pane;

					for (int i = column_span; last_column && i > 0; i--)
						last_column = last_column->Suc();

					if (last_column)
					{
						/* Still columns following the last column that the float
						   touches. We actually need to clip then. Expand the current
						   clip rectangle to contain all the spanned columns. */

						int extra = (column_span - 1) * (column_gap + column_width);

#ifdef SUPPORT_TEXT_DIRECTION
						if (mp_props.direction == CSS_VALUE_rtl)
							left -= extra;
						else
#endif // SUPPORT_TEXT_DIRECTION
							right += extra;
					}
					else
						/* No columns following the last column that the float
						   touches. Don't clip. */

#ifdef SUPPORT_TEXT_DIRECTION
						if (mp_props.direction == CSS_VALUE_rtl)
							left = LAYOUT_COORD_MIN / 200;
						else
#endif // SUPPORT_TEXT_DIRECTION
							right = LAYOUT_COORD_MAX / 100;
				}
			}
		}

		return OpRect(left, LAYOUT_COORD_MIN / 200, right, LAYOUT_COORD_MAX / 100);
	}
}

/** Find next (below this) container element of given element. */

HTML_Element*
FindNextContainerElementOf(HTML_Element* from_element, HTML_Element* element)
{
	OP_ASSERT(element);

	if (!element || element == from_element)
		return NULL;

	HTML_Element* candidate = NULL;
	element = element->Parent();

	while (element)
	{
		if (element == from_element)
			return candidate;
		else
			if (element->GetLayoutBox() && element->GetLayoutBox()->IsContainingElement())
				candidate = element;

		element = element->Parent();
	}

	return NULL;
}

/** Set clip rectangle of the current column or page. */

void
TraversalObject::SetPaneClipRect(const OpRect& clip_rect)
{
	pane_clip_rect = clip_rect;
	pane_clip_rect.x += GetTranslationX();
	pane_clip_rect.y += GetTranslationY();
}

/** Return TRUE if this element starts before the start of the current column / page. */

BOOL
TraversalObject::TouchesCurrentPaneStart(LayoutProperties* layout_props) const
{
	if (!current_pane)
		return FALSE;

	if (!pane_started || pane_start_offset > 0)
		return TRUE;

	if (VerticalLayout* vertical_layout = current_pane->GetStartElement().GetVerticalLayout())
		if (vertical_layout->IsBlock() && vertical_layout->IsInStack())
		{
			Box* this_box = layout_props->html_element->GetLayoutBox();

			if (this_box->IsBlockBox() && (BlockBox*) this_box == (BlockBox*) vertical_layout)
				return TRUE;
		}

	return FALSE;
}

/** Return TRUE if this element stops after the stop of the current column / page. */

BOOL
TraversalObject::TouchesCurrentPaneStop(LayoutProperties* layout_props) const
{
	if (!current_pane)
		return FALSE;

	const ColumnBoundaryElement& stop_element = current_pane->GetStopElement();
	HTML_Element* this_element = layout_props->html_element;

	if (VerticalLayout* vertical_layout = stop_element.GetVerticalLayout())
	{
		if (vertical_layout->IsInStack())
		{
			if (vertical_layout->IsBlock())
			{
				Box* this_box = this_element->GetLayoutBox();

				if (this_box->IsBlockBox() && (BlockBox*) this_box == (BlockBox*) vertical_layout)
					return TRUE;
			}

			if (this_element->IsAncestorOf(stop_element.GetHtmlElement()))
				return TRUE;
		}
	}
	else
		if ((stop_element.GetColumnRow() || stop_element.GetFlexItemBox()) &&
			this_element->IsAncestorOf(stop_element.GetHtmlElement()))
			return TRUE;

	return FALSE;
}

/** Is the specified element the target or an ancestor of the target? */

BOOL
TraversalObject::IsTarget(HTML_Element* target, BOOL allow_logical) const
{
	return allow_logical && TraverseInLogicalOrder() ||
		target_element && target->IsAncestorOf(target_element);
}

/** Switch traversal target. */

/* virtual */ TraversalObject::SwitchTargetResult
TraversalObject::SwitchTarget(HTML_Element* containing_elm, HTML_Element* skipped_subtree /* = NULL */)
{
	OP_ASSERT(target_element && containing_elm);

	if (target_element->GetLayoutBox()->IsFloatedPaneBox())
	{
		if (FloatedPaneBox* next = ((FloatedPaneBox*) target_element->GetLayoutBox())->AdvanceToNextTraversalTarget())
		{
			OP_ASSERT(!target_element->IsAncestorOf(next->GetHtmlElement()));
			target_element = next->GetHtmlElement();
			return SWITCH_TARGET_OTHER_RESULT;
		}
	}
	else
		if (!IsInMultiPane())
		{
			/* Under certain circumstances it is possible to proceed to the next
			   target element without restarting traversal. However, we don't
			   attempt that if we're traversing a multicol container, simply
			   because that would complicate the code "too much". */

			Box* target_box = target_element->GetLayoutBox();
			ZElement* z_element = target_box->GetLocalZElement();

			if (z_element)
			{
				/* If the next Z element has the same z-index as the current target, it means
				   that it is an element defined later in the document, logically. We can
				   set it as a next target and start traversing to it at once, unless it
				   has a different clipping stack, so we need to return to zroot first.
				   Also if the next target is descendant of the previous one, which is an
				   inline box with inline content, we can't switch to next one,
				   because we have already passed the ancestor in the inline traverse.
				   Perhaps this can be improved. In case of inline block however there
				   are no line traverse state changes, so we can enter the box once
				   again with a new target, just as we do for other types of boxes. */

				ZElement* next_z = (ZElement*) z_element->Suc();
				HTML_Element* next_target = NULL;

				if (next_z && next_z->InSamePaintingStack(z_element) && next_z->HasSameClippingStack())
					next_target = next_z->GetHtmlElement();

				if (next_target)
				{
					BOOL next_is_descendant = target_element->IsAncestorOf(next_target);

					if (!next_is_descendant || !target_box->IsInlineBox() || !target_box->IsInlineContent())
					{
						target_element = next_target;
						return next_is_descendant ? SWITCH_TARGET_NEXT_IS_DESCENDANT : SWITCH_TARGET_NEXT_Z_ELEMENT;
					}
				}
			}
			else
			{
				FloatingBox* next_float = ((FloatingBox*) target_box)->GetNextFloat();

				if (next_float)
				{
					OP_ASSERT(!target_element->IsAncestorOf(next_float->GetHtmlElement()));
					target_element = next_float->GetHtmlElement();
					return SWITCH_TARGET_OTHER_RESULT;
				}
			}
		}

	target_done = TRUE;
	off_target_path = TRUE;

	return SWITCH_TARGET_OTHER_RESULT;
}

/** Notify that we are back at the target origin. */

/* virtual */ void
TraversalObject::TargetFinished(Box* box)
{
	target_done = FALSE;
	off_target_path = FALSE;
}

/** Enter vertical box */

/* virtual */ BOOL
TraversalObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	layout_props = parent_lprops->GetChildProperties(document->GetHLDocProfile(), box->GetHtmlElement());

	if (!layout_props)
	{
		SetOutOfMemory();
		return FALSE;
	}
	else
		return TRUE;
}

/** Add float to be traversed. */

void
TraversalObject::AddFloat(FloatingBox* floating_box)
{
	if (!last_float)
		first_float = last_float = floating_box;
	else
	{
		last_float->SetNextFloat(floating_box);
		last_float = floating_box;
	}

	floating_box->SetNextFloat(NULL);
}

/** Add pane float (GCPM float inside multicol or paged containers) to be traversed. */

void
TraversalObject::AddPaneFloats(PaneFloatList& float_list)
{
	if (!TraverseInLogicalOrder())
		for (PaneFloatEntry* entry = float_list.First(); entry; entry = entry->Suc())
			entry->GetBox()->QueueForTraversal(pane_floats);
}

/** Traverse floats, both regular CSS 2.1 and GCPM floats. */

void
TraversalObject::TraverseFloats(Box* box, LayoutProperties* layout_props)
{
	if (!TraverseInLogicalOrder())
	{
		/* Move lists of both CSS 2.1 floats and pane-attached floats to local
		   variables. We are now in the box where the floats should be
		   traversed, and we must make sure that descendant boxes don't attempt
		   to consume the lists and traverse them. */

		PaneFloatList local_pane_floats;
		FloatingBox* floating_box = first_float;

		first_float = NULL;
		last_float = NULL;
		local_pane_floats.Append(&pane_floats);
		pane_floats.RemoveAll();

		for (; floating_box; floating_box = floating_box->GetNextFloat())
		{
			HTML_Element* old_target = GetTarget();
			HTML_Element* html_element = floating_box->GetHtmlElement();

//			OP_ASSERT(!old_target);

			if (!old_target || old_target == html_element)
			{
				InitTargetTraverse(layout_props->html_element, html_element);

				box->TraverseContent(this, layout_props);

				html_element = GetTarget();
				floating_box = (FloatingBox*) html_element->GetLayoutBox();

				SetTarget(old_target);
				TargetFinished(box);
			}
		}

		if (!local_pane_floats.Empty())
		{
			HTML_Element* old_target = GetTarget();

			do
			{
				HTML_Element* html_element = local_pane_floats.First()->GetBox()->GetHtmlElement();

				InitTargetTraverse(layout_props->html_element, html_element);
				box->TraverseContent(this, layout_props);
				SetTarget(old_target);
				TargetFinished(box);

				if (PaneFloatEntry* missed_entry = local_pane_floats.First())
				{
					/* Missed at least one float. This happens either when the
					   target wasn't in a visible pane (in which case we now move
					   on to the next float, if any), or when there's mismatch
					   between visual and logical order (e.g. float 1 in the source
					   is on page 2, while float 2 in the source is on page 1) (in
					   which case we restart traversal from the logical and visual
					   start). */

					if (missed_entry->GetBox()->GetHtmlElement() == html_element)
						missed_entry->Out();
				}
			}
			while (!local_pane_floats.Empty());
		}
	}
}

/** Check the intersection of a positioned inline box that has its containing
    block calculated for offset purpose
	(PositionedInlineBox::IsContainingBlockCalculatedForOffset()). */

BOOL
AreaTraversalObject::Intersects(PositionedInlineBox* target_box, HTML_Element* container_element) const
{
	OP_ASSERT(target_box->IsContainingBlockCalculatedForOffset());

	LayoutCoord x(0);
	LayoutCoord y(0);

#ifdef DEBUG
	int offset_result = target_box->GetOffsetFromAncestor(x, y, container_element, Box::GETPOS_IGNORE_SCROLL);
	/* This method is supposed to be called only from the container of
	   the target_box that meets the previous assert.
	   In such case we can't have any result flags. */
	OP_ASSERT(offset_result == 0);
#else // !DEBUG
	target_box->GetOffsetFromAncestor(x, y, container_element, Box::GETPOS_IGNORE_SCROLL);
#endif // DEBUG

	x -= GetTranslationX() - container_translation_x;
	y -= GetTranslationY() - container_translation_y;

	return Intersects(target_box, x, y, TRUE);
}

/** Called when current_target_container is set and we have just reached it
	during the traverse (either just entered it or left an inline or a vertical
	box, whose containing element's container is the current_target_container). */

BOOL
AreaTraversalObject::CheckTargetContainer(HTML_Element* container_elm, BOOL entering)
{
	OP_ASSERT(current_target_container && target_element && target_intersection_checking);

	current_target_container = NULL;

	Box* target_box = target_element->GetLayoutBox();
	OP_ASSERT(target_box->IsInlineBox());

	if (target_box->IsPositionedBox() && static_cast<PositionedInlineBox*>(target_box)->IsContainingBlockCalculatedForOffset())
		if (!Intersects(static_cast<PositionedInlineBox*>(target_box), container_elm))
		{
			SwitchTarget(container_elm);

			if (entering && (IsTargetDone() || !HTMLayoutProperties::IsLayoutAncestorOf(container_elm, target_element)))
				return FALSE;
		}

	return TRUE;
}

/** Called when leaving an inline or a vertical box and in a need to handle
	some target traversal related logic. */

void
AreaTraversalObject::CalculateNextContainingElement(HTML_Element* element)
{
	HTML_Element* containing_element = Box::GetContainingElement(element);

	/* The only situation where this can be NULL is when the element is the root
	   element. But this method is not called when leaving the box of the root
	   element. */
	OP_ASSERT(containing_element);

	if (current_target_container && containing_element->GetLayoutBox()->GetContainer() == current_target_container)
	{
		CheckTargetContainer(containing_element, FALSE);

		if (!IsTargetDone())
			off_target_path = FALSE;

		// The above has handled the next_container_element update, if it was needed.
	}
	else if (HTMLayoutProperties::IsLayoutAncestorOf(containing_element, target_element))
	{
		next_container_element = FindNextContainerElementOf(containing_element, target_element);
		off_target_path = FALSE;
	}
}

/** Handle a situation when during traversing to a target, we do not enter
	some box that is an ancestor of it. */

BOOL
AreaTraversalObject::HandleSkippedTarget(HTML_Element* current_containing_elm, HTML_Element* tested_elm)
{
	OP_ASSERT(target_element);

	if (!target_intersection_checking)
	{
		Box* box = target_element->GetLayoutBox();

		/* The part box->IsFloatingBox() is somehow doubtful. We could almost assert
		   !box->IsFloatingBox(). That is because it is impossible to abandon float
		   target for the reason of failed intersection or clipping, because CSS 2.1
		   floats are only added to the float traverse list if they are entered
		   during background traverse pass. And in ONE_PASS_TRAVERSE non Z element
		   floats are traversed without the special float traversal pass. However
		   theoretically, a subclass can decide not to traverse to a float target
		   anymore for some other reason. Therefore this is needed.

		   Pane attached floats however doesn't have the intersection control
		   using the background traverse pass. */

		if (box->IsFloatedPaneBox() || box->IsFloatingBox())
		{
			// Take care of skipping all the descendant floats on our own.

			do
				SwitchTarget(current_containing_elm);
			while (!IsTargetDone() && HTMLayoutProperties::IsLayoutAncestorOf(tested_elm, target_element));

			return FALSE;
		}
	}

	SwitchTarget(current_containing_elm, tested_elm);

	if (!IsTargetDone())
		if (!target_intersection_checking)
			/* Fixed positioned box or a stacking context with some fixed positioned
			   element in it or in descendant stacking context. */

			return HTMLayoutProperties::IsLayoutAncestorOf(tested_elm, target_element);

	return FALSE;
}

/** Switch traversal target. */

/* virtual */ TraversalObject::SwitchTargetResult
AreaTraversalObject::SwitchTarget(HTML_Element* containing_elm, HTML_Element* skipped_subtree /* = NULL */)
{
	SwitchTargetResult result;

	while (TRUE)
	{
		result = TraversalObject::SwitchTarget(containing_elm, skipped_subtree);

		target_intersection_checking = result != SWITCH_TARGET_OTHER_RESULT;
		current_target_container = NULL;

		if (!target_intersection_checking)
			break;

		Box* target_box = target_element->GetLayoutBox();

		if (target_box->IsFixedPositionedBox())
			target_intersection_checking = FALSE;
		else
		{
			StackingContext *stacking_context = target_box->GetLocalStackingContext();
			target_intersection_checking = !stacking_context || !stacking_context->HasFixedElements();
		}

		if (!target_intersection_checking)
			break;

		if (skipped_subtree && HTMLayoutProperties::IsLayoutAncestorOf(skipped_subtree, target_element))
			continue;

		if (target_box->IsInlineBox())
		{
			HTML_Element* target_container_elm = Box::GetContainingElement(target_element);
			OP_ASSERT(target_container_elm);

			if (target_container_elm == containing_elm)
			{
				if (target_box->IsPositionedBox() && static_cast<PositionedInlineBox*>(target_box)->IsContainingBlockCalculatedForOffset())
					if (!Intersects(static_cast<PositionedInlineBox*>(target_box), target_container_elm))
						// Intersection check failed, try to switch to a next target.

						continue;
			}
			else
			{
				/* We are in some other container.
				   Store the new target's container and continue the traverse. */

				current_target_container = target_container_elm->GetLayoutBox()->GetContainer();
				OP_ASSERT(current_target_container);
			}
		}

		break;
	}

	if (!IsTargetDone())
	{
		OP_ASSERT(target_element);

		if (next_container_element && !HTMLayoutProperties::IsLayoutAncestorOf(next_container_element, target_element))
			next_container_element = NULL;

		if (!next_container_element)
			if (HTMLayoutProperties::IsLayoutAncestorOf(containing_elm, target_element))
				next_container_element = FindNextContainerElementOf(containing_elm, target_element);
			else
				off_target_path = TRUE;
	}

	return result;
}

/** Enter vertical box */

/* virtual */ BOOL
AreaTraversalObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	OP_PROBE6(OP_PROBE_DOC_AREATRAVERSALOBJECT_ENTERVERTICALBOX);

	if (target_element)
	{
		HTML_Element* html_element = box->GetHtmlElement();

		if (html_element == next_container_element &&
			(!target_intersection_checking ||
			Intersects(box) ||
			HandleSkippedTarget(box->GetContainingElement(), html_element)))
		{
			if (!traverse_info.dry_run)
			{
				if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
					return FALSE;

				if (next_container_element)
					next_container_element = FindNextContainerElementOf(next_container_element, target_element);
			}
		}
		else
			return FALSE;
	}
	else
	{
		BOOL enter = Intersects(box);

		if (!enter)
		{
			// Must traverse the root element (for TrueZoom)

			enter = parent_lprops->html_element == NULL;

			if (!enter)
			{
				// Stacking contexts with fixed positioned elements must be traversed no matter what.

				StackingContext *stacking_context = box->GetLocalStackingContext();

				enter = stacking_context && stacking_context->HasFixedElements();
			}
		}

		if (!enter && !enter_all)
			return FALSE;

		if (!traverse_info.dry_run && !layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
			return FALSE;
	}

	return TRUE;
}

/** Leave vertical box */

/* virtual */ void
AreaTraversalObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (target_element && !IsTargetDone() && !next_container_element)
		CalculateNextContainingElement(layout_props->html_element);
}

/** Enter inline box */

/* virtual */ BOOL
AreaTraversalObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (!target_element || layout_props->html_element->IsAncestorOf(target_element))
	{
		if (target_element && box->IsContainingElement())
		{
			OP_ASSERT(layout_props->html_element == next_container_element);

			if (next_container_element)
				next_container_element = FindNextContainerElementOf(next_container_element, target_element);
		}

		return TRUE;
	}

	return FALSE;
}

/** Leave inline box */

/* virtual */ void
AreaTraversalObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	if (target_element && !IsTargetDone() && !next_container_element && box->IsContainingElement())
		CalculateNextContainingElement(layout_props->html_element);
}

/* virtual */ BOOL
AreaTraversalObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	BOOL traverse = FALSE;

	if (target_element)
	{
		Box* target_box = next_container_element? next_container_element->GetLayoutBox() : target_element->GetLayoutBox();

		OP_ASSERT(!next_container_element || HTMLayoutProperties::IsLayoutAncestorOf(next_container_element, target_element));

		if (target_box->TraverseInLine())
			traverse = target_box->GetVirtualPosition() <= line->GetEndPosition(); // line may end with an abs_pos box, not in the flow.
	}
	else
		traverse = Intersects(line);

	if (traverse)
	{
		const HTMLayoutProperties* parent_props = &parent_lprops->GetCascadingProperties();

		if (parent_props->text_overflow == CSS_VALUE__o_ellipsis_lastline && parent_props->overflow_y != CSS_VALUE_visible)
		{
			OpRect content_rect;
			OpPoint scroll_offset;

			containing_element->GetLayoutBox()->GetContentEdges(*parent_props, content_rect, scroll_offset);
			content_rect.OffsetBy(scroll_offset);

			// Do not traverse if line is not entirely visible except very first line which should be
			// visible even if it overflows the cointainer verticaly.
			if (line->Pred() && line->GetY() + line->GetLayoutHeight() > content_rect.Bottom())
				traverse = FALSE;
		}
	}

	return traverse;
}

/** Enter container. */

/* virtual */ BOOL
AreaTraversalObject::EnterContainer(LayoutProperties* layout_props, Container* container, LayoutPoint& translation)
{
	translation.x = container_translation_x;
	translation.y = container_translation_y;
	container_translation_x = GetTranslationX();
	container_translation_y = GetTranslationY();

	if (container == current_target_container)
	{
		if (container->IsMultiPaneContainer())
			/* This is an exception, in which we simply proceed to an inline box target
			   without any intersection checking inside CheckTargetContainer().
			   That is because multi-pane containers involve their own translations when
			   it comes to the panes and those translations are already present now.
			   The stored positioned inline box'es offset is however relative to the
			   container's placeholder, so checking intersection now doesn't make sense.

			   This might be improved (like checking those intersection in appropriate
			   Enter[MultiPaneType]Container() methods). Currently, we can allow
			   such suboptimality, because we also don't allow quick target switches when
			   being inside multi-pane. So skipping at most one intersection check per
			   one multi-pane container doesn't make any noticeable impact. */

			current_target_container = NULL;
		else
			if (!CheckTargetContainer(layout_props->html_element, TRUE))
			{
				container_translation_x = translation.x;
				container_translation_y = translation.y;
				return FALSE;
			}
	}

	return TRUE;
}

/** Leave container. */

/* virtual */ void
AreaTraversalObject::LeaveContainer(const LayoutPoint& translation)
{
	container_translation_x = translation.x;
	container_translation_y = translation.y;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
AreaTraversalObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	if (target_element)
		return TRUE;

	const HTMLayoutProperties& props = *layout_props->GetProps();
	return height > 0 && Intersects(OpRect(props.border.left.width, props.border.top.width, width, height));
}

#endif // PAGED_MEDIA_SUPPORT

/** Enter column in multicol container or page in paged container. */

/* virtual */ BOOL
AreaTraversalObject::EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info)
{
	Box* mp_box = multipane_lprops->html_element->GetLayoutBox();
	OpRect clip_rect = CalculatePaneClipRect(multipane_lprops, pane, target_element, is_column);
	BOOL intersects = Intersects(clip_rect);
	BOOL clip_affects_target = !target_element || mp_box->GetClipAffectsTarget(target_element, TRUE);

	if (intersects || !clip_affects_target)
	{
		if (intersects && clip_affects_target)
			PushClipRect(clip_rect, traverse_info);

		return TRUE;
	}
	else
		return FALSE;
}

/** Leave column in multicol container or page in paged container. */

/* virtual */ void
AreaTraversalObject::LeavePane(TraverseInfo& traverse_info)
{
	if (traverse_info.has_clipped)
		PopClipRect(traverse_info);
}

/* virtual */ BOOL
AreaTraversalObject::EnterTableRow(TableRowBox* row)
{
	if (!target_element)
		return Intersects(row);
	else
		return row->GetHtmlElement()->IsAncestorOf(target_element) &&
			(!target_intersection_checking ||
			Intersects(row) ||
			HandleSkippedTarget(row->GetContainingElement(), row->GetHtmlElement()));
}

/** Leave table row. */

/* virtual */ void
AreaTraversalObject::LeaveTableRow(TableRowBox* row, TableContent* table)
{
	if (target_element && !IsTargetDone() && !next_container_element)
	{
		/* We need to bypass table row group as a containing element, because we don't
		   have a similar code executed when leaving a table row group box. */

		HTML_Element* containing_element = table->GetHtmlElement();

		if (HTMLayoutProperties::IsLayoutAncestorOf(containing_element, target_element))
		{
			next_container_element = FindNextContainerElementOf(containing_element, target_element);
			off_target_path = FALSE;
		}
	}
}

/** Check if this positioned element needs to be traversed. */

/* virtual */ BOOL
AreaTraversalObject::TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element)
{
	/* This method can have four outcomes:
	   1) The element is confirmed not to intersect the area and it is not
	   a fixed positioned element nor has any fixed positioned elemento on its
	   stacking context. In such case we don't need to traverse to it.
	   2) The element is confirmed to intersect the area. We traverse to it.
	   3) The element is not confirmed to intersect the area and it is possible
	   to check intersections on the path to it without the risk of missing it.
	   We traverse to it and set target_intersection_checking flag to TRUE.
	   4) The element is not confirmed to intersect the area and we can't check
	   intersections on the path to it. That is because of a presence of a
	   fixed positioned element:
	   - somewhere on the path between the element and containing_element
	   - in the stacking context of the element (or in some sub-stacking context)
	   - the element is a fixed positioned itself */

	Box* box = element->GetLayoutBox();

	if (box->IsFixedPositionedBox())
		return TRUE;

	StackingContext* stacking_context = box->GetLocalStackingContext();
	if (stacking_context && stacking_context->HasFixedElements())
		return TRUE;

	BOOL set_target_intersection_checking = FALSE;
	BOOL check_fixed_pos_on_path = FALSE;

	if (box->IsBlockBox() && (!box->IsAbsolutePositionedBox() || !static_cast<AbsolutePositionedBox*>(box)->IsHypotheticalHorizontalStaticInline()) ||
		box->IsTableCell() ||
		box->IsTableCaption() ||
		box->IsInlineBox() && box->IsPositionedBox() && !box->GetLocalStackingContext() && static_cast<PositionedInlineBox*>(box)->IsContainingBlockCalculated())
	{
		if (box->IsAbsolutePositionedBox() && !containing_element->GetLayoutBox()->IsPositionedBox())
		{
			HTML_Element* containing_element_abs_sense = Box::GetContainingElement(containing_element, TRUE);
			Box* containing_element_abs_box = containing_element_abs_sense->GetLayoutBox();
			if (containing_element_abs_box->IsInlineBox() && !containing_element_abs_box->IsInlineBlockBox())
				/* If we end up here, it means we can't compute the offset correctly.
				   See GetOffsetFromAncestor description. */
			{
				target_intersection_checking = TRUE;
				return TRUE;
			}
		}

		LayoutCoord x(0);
		LayoutCoord y(0);

		int offset_result = box->GetOffsetFromAncestor(x, y, containing_element, Box::GETPOS_ABORT_ON_INLINE);

		if (offset_result & (Box::GETPOS_INLINE_FOUND | Box::GETPOS_FIXED_FOUND
#ifdef CSS_TRANSFORMS
							 | Box::GETPOS_TRANSFORM_FOUND
#endif
				))
		{
			if (!(offset_result & Box::GETPOS_FIXED_FOUND))
			{
				set_target_intersection_checking = TRUE;

				/* We stopped too early to be fully sure that there are no fixed positioned
				   elements on the tree path between the element and containing_element. */
				check_fixed_pos_on_path = offset_result & Box::GETPOS_INLINE_FOUND;
			}
		}
		else
			return Intersects(static_cast<Content_Box*>(box), x, y, TRUE);
	}
	else
		check_fixed_pos_on_path = set_target_intersection_checking = TRUE;

	if (set_target_intersection_checking)
	{
		if (check_fixed_pos_on_path)
		{
			HTML_Element* containing_element = box->GetContainingElement();

			while (containing_element)
			{
				if (containing_element->GetLayoutBox()->IsFixedPositionedBox())
					return TRUE;

				containing_element = Box::GetContainingElement(containing_element);
			}
		}

		target_intersection_checking = TRUE;
		if (box->IsInlineBox())
		{
			current_target_container = box->GetContainingElement()->GetLayoutBox()->GetContainer();
			OP_ASSERT(current_target_container);
		}
	}

	return TRUE;
}

/** Callback for objects needing to set up code before starting traversal */

/* virtual */ void
HitTestingTraversalObject::StartingTraverse()
{
	if (!clipping_disabled)
	{
		int negative_overflow = document->NegativeOverflow();

		m_clip_rect.Set(-negative_overflow, 0, document->Width() + negative_overflow, document->Height());

		OP_ASSERT(!m_clip_rect.IsEmpty());
	}
}

/*virtual*/ void
HitTestingTraversalObject::Translate(LayoutCoord x, LayoutCoord y)
{
	visual_device_transform.Translate(x, y);
	translation_x += x;
	translation_y += y;

	if (!clipping_disabled)
		m_clip_rect.OffsetBy(-x, -y);
}

void
HitTestingTraversalObject::PushClipRect(const OpRect& cr, TraverseInfo& info)
{
	info.old_clip_rect = m_clip_rect;
	info.is_real_clip_rect = m_has_clip_rect;

	if (!clipping_disabled)
	{
		m_clip_rect.IntersectWith(cr);
		m_has_clip_rect = TRUE;
	}

	info.has_clipped = TRUE;
}

void
HitTestingTraversalObject::PopClipRect(TraverseInfo& info)
{
	if (info.has_clipped)
	{
		m_clip_rect = info.old_clip_rect;
		m_has_clip_rect = info.is_real_clip_rect;
		info.has_clipped = FALSE;
	}
}

/*virtual*/BOOL
HitTestingTraversalObject::IntersectsCustom(RECT& rect) const
{
	if (IsAreaSinglePoint())
	{
		if (IsLastClipRectOutsideHitArea())
			return FALSE;
		else
		{
#ifdef CSS_TRANSFORMS
			// Fail would make GetLocalAreaPoint() behave randomly. Ensured in PushTransform() to pass.
			OP_ASSERT(!(transform_root.IsTransform() && transform_root.GetTransform().Determinant() == 0.0f));
#endif // CSS_TRANSFORMS

			OpPoint point = GetLocalAreaPoint();

			return !(rect.left > point.x || rect.top > point.y ||
					rect.right <= point.x || rect.bottom <= point.y);
		}
	}
	else
	{
		if (m_has_clip_rect && !IntersectRectWith(rect, m_clip_rect))
			return FALSE;

		return visual_device_transform.TestIntersection(rect, area);
	}
}

BOOL
HitTestingTraversalObject::CheckIntersectionWithClipping(const OpRect& rect, const RECT* area /*= NULL*/, OpRect* clipped_rect /*= NULL*/) const
{
	OpRect rect_copy = rect;

	if (m_has_clip_rect)
		rect_copy.IntersectWith(m_clip_rect);

	if (!rect_copy.IsEmpty() && visual_device_transform.TestIntersection(rect_copy, area ? *area : this->area))
	{
		if (clipped_rect)
			*clipped_rect = rect_copy;
		return TRUE;
	}
	else
		return FALSE;
}

BOOL
HitTestingTraversalObject::CheckIntersectionWithClipping(const RECT& rect, const RECT* area /*= NULL*/, RECT* clipped_rect /*= NULL*/) const
{
	RECT rect_copy = rect;

	if (m_has_clip_rect)
		if (!IntersectRectWith(rect_copy, m_clip_rect))
			return FALSE;

	if (visual_device_transform.TestIntersection(rect_copy, area ? *area : this->area))
	{
		if (clipped_rect)
			*clipped_rect = rect_copy;
		return TRUE;
	}
	else
		return FALSE;
}

BOOL
HitTestingTraversalObject::DoubleCheckIntersectionWithClipping(const OpRect& rect, const RECT& inner, BOOL& intersects_inner, OpRect* clipped_rect /*= NULL*/) const
{
	OpRect rect_copy = rect;
	BOOL result;

	if (m_has_clip_rect)
		rect_copy.IntersectWith(m_clip_rect);

	if (rect_copy.IsEmpty())
		return FALSE;

	result = visual_device_transform.TestIntersection(rect_copy, inner);

	if (result)
		intersects_inner = TRUE;
	else
		result = Intersects(rect_copy);

	if (result && clipped_rect)
		*clipped_rect = rect_copy;

	return result;
}

BOOL
HitTestingTraversalObject::DoubleCheckIntersectionWithClipping(const RECT& rect, const RECT& inner, BOOL& intersects_inner, RECT* clipped_rect /*= NULL*/) const
{
	RECT rect_copy = rect;
	BOOL result;

	if (m_has_clip_rect)
		if (!IntersectRectWith(rect_copy, m_clip_rect))
			return FALSE;

	result = visual_device_transform.TestIntersection(rect_copy, inner);

	if (result)
		intersects_inner = TRUE;
	else
		result = Intersects(rect_copy);

	if (result && clipped_rect)
		*clipped_rect = rect_copy;

	return result;
}

BOOL
HitTestingTraversalObject::IsLastClipRectOutsideHitArea() const
{
	return m_has_clip_rect &&
		(m_clip_rect.IsEmpty()
		|| (IsAreaSinglePoint() && !m_clip_rect.Contains(GetLocalAreaPoint()))
		|| (!IsAreaSinglePoint() && !Intersects(m_clip_rect)));
}

/** Enter scrollable content. */

/* virtual */ BOOL
HitTestingTraversalObject::EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	Box* this_box = scrollable->GetOwningBox();

	int scrollbar_modifier = scrollable->LeftHandScrollbar() ? scrollable->GetVerticalScrollbarWidth() : 0;
	BOOL clip = this_box->GetClipAffectsTarget(target_element);
	BOOL result = HandleScrollable(props, scrollable, width, height, traverse_info, clip, scrollbar_modifier);

	if (result)
	{
		if (!clipping_disabled && clip)
		{
			int left = props.border.left.width + scrollbar_modifier;
			RECT new_clip_rect = {left, props.border.top.width, left+width, props.border.top.width+height};

			if (HitTestingTraversalObject::IntersectsCustom(new_clip_rect))
			{
				OpRect rect(left, props.border.top.width, width, height);
				PushClipRect(rect, traverse_info);
				return TRUE;
			}
		}
		else
			return TRUE;
	}

	if (target_element)
	{
		HTML_Element* element = this_box->GetHtmlElement();
		/* We shouldn't ignore the return value here.
		   See also the body of HitTestingTraversalObject::EnterVerticalBox() */
		HandleSkippedTarget(element, element);
	}

	return FALSE;
}

/* virtual */ BOOL
HitTestingTraversalObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	/* In almost all the cases we ensure that the intersection between the current
	   clip rect and the area is not empty before entering a given box. The exception
	   is a collapsed table cell. Before a call to EnterVerticalBox of a table cell,
	   BeginCollapsedTableCellClipping() might be called, where the new clip rect is
	   pushed unconditionally. Here, if the intersection between the current clip
	   rect and the area is empty, we do not enter that table cell. */

	if (box->IsTableCell() && IsLastClipRectOutsideHitArea())
	{
		if (target_element)
			HandleSkippedTarget(box->GetContainingElement(), box->GetHtmlElement());

		return FALSE;
	}

	if (AreaTraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
	{
		if (traverse_info.dry_run)
			return TRUE;

		const HTMLayoutProperties& props = *layout_props->GetProps();

		BOOL abs_clipping = props.clip_left != CLIP_NOT_SET && box->IsAbsolutePositionedBox();
		BOOL overflow_clipping = props.overflow_x != CSS_VALUE_visible && !box->GetTableContent();
		BOOL clip_affects_target = (abs_clipping || overflow_clipping) && box->GetClipAffectsTarget(target_element);

		BOOL result = HandleVerticalBox(parent_lprops, layout_props, box, traverse_info, clip_affects_target);

		if (result)
		{
			if (!clipping_disabled)
			{
				if (overflow_clipping && box->HasComplexOverflowClipping())
				{
					traverse_info.handle_overflow = TRUE;
					return TRUE;
				}
				else if (clip_affects_target)
				{
					OpRect clip_rect = box->GetClippedRect(props, overflow_clipping);
					RECT winrect_clip_rect = clip_rect;

					if (HitTestingTraversalObject::IntersectsCustom(winrect_clip_rect))
					{
						PushClipRect(clip_rect, traverse_info);
						return TRUE;
					}
				}
				else
					return TRUE;
			}
			else
				return TRUE;
		}

		if (target_element)
			/* We shouldn't ignore the return value here! The same for the other
			   places where HandleSkippedTarget is called inside HitTestingTraversalObject's
			   methods.

			   CORE-42313 is the associated bug. If HandleSkippedTarget returns TRUE, it
			   means that the next target is a fixed positioned box (or an element that
			   has a local stacking context that has a fixed positioned element in it)
			   and also that HasSameClippingStack() answered TRUE for the next Z Element
			   target. So what can go wrong here is that there could be a clip rect(s) that
			   comes from ancestor(s) of the current zroot. In such case we can't take it
			   off even if we would go back to the StackingContext first and then to the
			   current fixed positioned target. */

			HandleSkippedTarget(box->GetContainingElement(), box->GetHtmlElement());
	}

	return FALSE;
}

/* virtual */ void
HitTestingTraversalObject::LeaveScrollable(TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);

	AreaTraversalObject::LeaveScrollable(traverse_info);
}

/*virtual*/ void
HitTestingTraversalObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);

	AreaTraversalObject::LeaveVerticalBox(layout_props, box, traverse_info);
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
HitTestingTraversalObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	if (!AreaTraversalObject::EnterPagedContainer(layout_props, content, width, height, traverse_info))
		return FALSE;

	if (!clipping_disabled && content->GetPlaceholder()->GetClipAffectsTarget(target_element, TRUE))
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		PushClipRect(OpRect(props.border.left.width, props.border.top.width, width, height), traverse_info);
	}

	return TRUE;
}

/** Leave paged container. */

/* virtual */ void
HitTestingTraversalObject::LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info)
{
	if (traverse_info.has_clipped)
		PopClipRect(traverse_info);
}

#endif // PAGED_MEDIA_SUPPORT

/* virtual */ void
HitTestingTraversalObject::BeginCollapsedTableCellClipping(TableCellBox* box, const OpRect& clip_rect, TraverseInfo& traverse_info)
{
	if (!clipping_disabled && (!target_element ||
		box->GetHtmlElement()->IsAncestorOf(target_element) && box->GetClipAffectsTarget(target_element)))
		PushClipRect(clip_rect, traverse_info);
}

/* virtual */ void
HitTestingTraversalObject::EndCollapsedTableCellClipping(TableCellBox* box, TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);
}

/* virtual */ BOOL
HitTestingTraversalObject::EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();
	/* Keeping other browser engines compatibility (Gecko, WebKit)
	   - in case of tables only overflow_x matters and it controls also
	   the visiblity of y overflow. IE9 is slightly different - they
	   do not ignore overflow-y, but it also affects x overflow. */
	if (props.overflow_x == CSS_VALUE_hidden)
	{
		Content_Box* table_box = content->GetPlaceholder();
		if (table_box->GetClipAffectsTarget(target_element))
		{
			OpRect clip_rect = table_box->GetClippedRect(props, TRUE);
			RECT winrect_clip_rect = clip_rect;

			if (HitTestingTraversalObject::IntersectsCustom(winrect_clip_rect))
				PushClipRect(clip_rect, traverse_info);
			else
			{
				if (target_element)
				{
					HTML_Element* table_elm = layout_props->html_element;
					/* We shouldn't ignore the return value here.
					   See also the body of HitTestingTraversalObject::EnterVerticalBox() */
					HandleSkippedTarget(table_elm, table_elm);
				}

				return FALSE;
			}
		}
	}

	return TRUE;
}

/* virtual */ void
HitTestingTraversalObject::LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);
}

/** Enter column in multicol container or page in paged container. */

/* virtual */ BOOL
VisualTraversalObject::EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info)
{
	return TRUE;
}

/** Leave column in multicol container or page in paged container. */

/* virtual */ void
VisualTraversalObject::LeavePane(TraverseInfo& traverse_info)
{
}

#ifdef CSS_TRANSFORMS

/* virtual */ OP_BOOLEAN
VisualTraversalObject::PushTransform(const AffineTransform &t, TranslationState &state)
{
	/* Push transform */
	RETURN_IF_ERROR(visual_device->PushTransform(t));

	/* Save and reset the translations within the transformed
	   element */
	state.Read(this);
	SetTranslation(LayoutCoord(0), LayoutCoord(0));
	SetRootTranslation(LayoutCoord(0), LayoutCoord(0));
	SetFixedPos(LayoutCoord(0), LayoutCoord(0));

	/* Save the CTM to the transform root */
	transform_root = visual_device->GetCTM();

	return OpBoolean::IS_TRUE;
}

/* virtual */ void
VisualTraversalObject::PopTransform(const TranslationState &state)
{
	visual_device->PopTransform();

	/* Restore translations */

	state.Write(this);
}

/* virtual */ OP_BOOLEAN
HitTestingTraversalObject::PushTransform(const AffineTransform &t, TranslationState &state)
{
	OP_ASSERT(clipping_disabled || !m_clip_rect.IsEmpty());
	BOOL is_non_angle_transform = FALSE; // initializing to silence compiler warning (when used always initialized)

	// Such transform empties the non empty rects. No sense in entering such transform context.
	if (t.Determinant() == 0.0f)
		return OpBoolean::IS_FALSE;

	if (!clipping_disabled)
	{
		is_non_angle_transform = t.IsNonSkewing();

		state.old_clip_rect = m_clip_rect;
		state.is_real_clip_rect = m_has_clip_rect;
		state.old_area = area;
		if (m_clipping_precision == MERGED_WITH_AREA)
			state.precise_clipping = FALSE;

		if (is_non_angle_transform)
			// don't need to intersect the clip rect with area, we can just shift it
			AffinePos(t).ApplyInverse(m_clip_rect);
		else if (m_has_clip_rect)
		{
			OpRect doc_clip_rect = visual_device_transform.ToBBox(m_clip_rect);
#ifdef _DEBUG
			BOOL result = IntersectRectWith(area, doc_clip_rect);
			// if the intersection between area and current clip rect is empty, we shouldn't be here
			OP_ASSERT(result);
#else // !_DEBUG
			IntersectRectWith(area, doc_clip_rect);
#endif // _DEBUG

			m_clipping_precision = MERGED_WITH_AREA;
		}
	}

	OP_STATUS status = visual_device_transform.PushTransform(t);
	if (OpStatus::IsError(status))
	{
		if (!clipping_disabled)
		{
			area = state.old_area;
			m_clip_rect = state.old_clip_rect;
			m_clipping_precision = state.precise_clipping ? PRECISE : MERGED_WITH_AREA;
		}

		return status;
	}

	state.Read(this);

	transform_root = visual_device_transform.GetCTM();

	if (!clipping_disabled && !is_non_angle_transform)
	{
		/* Take the whole document rectangle and transform it to the current transform_root coordinate system.
		   Then mark that the current m_clip_rect is the whole doc's rectangle. */
		int negative_overflow = document->NegativeOverflow();

		m_clip_rect.Set(-negative_overflow, 0, document->Width() + negative_overflow, document->Height());
		transform_root.ApplyInverse(m_clip_rect);

		m_has_clip_rect = FALSE;
	}

	/* Reset translations */

	SetTranslation(LayoutCoord(0), LayoutCoord(0));
	SetRootTranslation(LayoutCoord(0), LayoutCoord(0));
	SetFixedPos(LayoutCoord(0), LayoutCoord(0));

	return OpBoolean::IS_TRUE;
}

/* virtual */ void
HitTestingTraversalObject::PopTransform(const TranslationState &state)
{
	visual_device_transform.PopTransform();

	/* Restore translations */

	if (!clipping_disabled)
	{
		m_has_clip_rect = state.is_real_clip_rect;
		area = state.old_area;
		m_clip_rect = state.old_clip_rect;
		m_clipping_precision = state.precise_clipping ? PRECISE : MERGED_WITH_AREA;
	}

	state.Write(this);
}

#endif

RECT BoxEdgesObject::MakeTranslated(const RECT &rect) const
{
	RECT r;

	r.left = (rect.left != LONG_MIN) ? rect.left + GetTranslationX(): LONG_MIN;
	r.top = (rect.top != LONG_MIN) ? rect.top + GetTranslationY() : LONG_MIN;
	r.right = (rect.right != LONG_MAX) ? rect.right + GetTranslationX(): LONG_MAX;
	r.bottom = (rect.bottom != LONG_MAX) ? rect.bottom + GetTranslationY() : LONG_MAX;

	return r;
}

void
BoxEdgesObject::UnionLocalRect(const RECT &rect, BOOL force_left, BOOL force_right, BOOL skip_left, BOOL skip_right, BOOL skip_for_containing_block_rect)
{
	RECT bbox = MakeTranslated(rect);

	if (box_edges.top > bbox.top)
		box_edges.top = bbox.top;

	if (box_edges.bottom < bbox.bottom)
		box_edges.bottom = bbox.bottom;

	if (box_edges.left > bbox.left)
		box_edges.left = bbox.left;

	if (box_edges.right < bbox.right)
		box_edges.right = bbox.right;

	if (area_type == CONTAINING_BLOCK_AREA && !skip_for_containing_block_rect)
	{
		if (containing_block_rect.top > bbox.top)
			containing_block_rect.top = bbox.top;

		if (containing_block_rect.bottom < bbox.bottom)
			containing_block_rect.bottom = bbox.bottom;

		if (!skip_left)
			if (containing_block_rect.left > bbox.left || force_left)
				containing_block_rect.left = bbox.left;

		if (!skip_right)
			if (containing_block_rect.right < bbox.right || force_right)
				containing_block_rect.right = bbox.right;
	}

	box_transform_root = GetTransformRoot();
}

void
BoxEdgesObject::AddLocalRect(LayoutCoord x, LayoutCoord y, LayoutCoord width, LayoutCoord height)
{
	OP_ASSERT(box_edges_list != NULL);
	RECT rect = { x, y, x + width, y + height };
	AddLocalRect(rect);
}

void
BoxEdgesObject::AddLocalRect(const RECT &rect)
{
	RECT bbox = MakeTranslated(rect);

	OP_ASSERT(box_edges_list);

	if (!box_edges_list->First())
		/* Set transform for first rect */
		box_transform_root = GetTransformRoot();

	RectListItem *rect_item = OP_NEW(RectListItem, (bbox));
	if (rect_item)
		rect_item->Into(box_edges_list);
	else
		SetOutOfMemory();
}

void
BoxEdgesObject::SetLocalRect(const RECT &rect)
{
	box_edges = MakeTranslated(rect);
	box_transform_root = GetTransformRoot();
}

/* virtual */ BOOL
BoxEdgesObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	HTML_Element* html_element = box->GetHtmlElement();

	if (IsInMultiPane() && html_element->IsAncestorOf(element))
		/* We may enter the same element several times if it lives in multiple
		   columns. */

		box_finished = FALSE;

	if (!box_finished)
	{
		if (box_edges_list && element->IsAncestorOf(html_element) && !box->GetTableContent())
		{
			/* Include (vertical) child boxes in list mode. See bug
			   317510.

			   Tables are excluded. They should be returned as their
			   respective caption box(es) and table box. Not as the
			   anonymous container box.  */

			AddLocalRect(LayoutCoord(0), LayoutCoord(0), box->GetWidth(), box->GetHeight());
		}

		if (html_element == element)
		{
			OP_ASSERT(area_type != CONTAINING_BLOCK_AREA); // Currently only properly supported for inline boxes

			if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
				return FALSE;

			box_found = TRUE;

			if (box_edges_list && box->GetTableContent())

				/* For table elements the rect_list should return the
				   table box and caption box (if present), not the
				   anonymous container box. So let's continue traverse
				   into these. */

				return TRUE;

			box_finished = TRUE;

			RECT edges = { 0, 0, 0, 0 };

			const HTMLayoutProperties& props = *layout_props->GetProps();

			switch (area_type)
			{
			case CONTENT_AREA:
			case PADDING_AREA:
			case BORDER_AREA:
			case OFFSET_AREA:
				edges.left = 0;
				edges.top = 0;
				edges.right = box->GetWidth();
				edges.bottom = box->GetHeight();

				if (area_type != BORDER_AREA && area_type != OFFSET_AREA)
				{
					edges.left += props.border.left.width;
					edges.top += props.border.top.width;
					edges.right -= props.border.right.width;
					edges.bottom -= props.border.bottom.width;

					if (area_type != PADDING_AREA)
					{
						edges.left += props.padding_left;
						edges.top += props.padding_top;
						edges.right -= props.padding_right;
						edges.bottom -= props.padding_bottom;
					}
				}
				break;
			case ENCLOSING_AREA:
			case BOUNDING_AREA:
				{
					AbsoluteBoundingBox bounding_box;
					box->GetBoundingBox(bounding_box);

					bounding_box.GetBoundingRect(edges);
				}
				break;
			default:
				OP_ASSERT(!"Unhandled area_type");
				break;
			}

			if (TouchesCurrentPaneBoundaries(layout_props))
			{
				// This block may live in multiple columns / pages.

				OpRect clipped_rect(pane_clip_rect);

				clipped_rect.x -= translation_x;
				clipped_rect.y -= translation_y;
				clipped_rect.IntersectWith(OpRect(&edges));

				RECT r = { clipped_rect.Left(), clipped_rect.Top(), clipped_rect.Right(), clipped_rect.Bottom() };

				UnionLocalRect(r);
			}
			else
				SetLocalRect(edges);
		}
		else
			if (html_element == next_container_element)
			{
				if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
					return FALSE;

				next_container_element = FindNextContainerElementOf(next_container_element, element);

				return TRUE;
			}
			else
				if (!next_container_element && box_found && (area_type == BOUNDING_AREA || area_type == ENCLOSING_AREA || area_type == OFFSET_AREA) && element->GetLayoutBox() && element->GetLayoutBox()->IsInlineContent())
				{
					OP_ASSERT(!box_finished);
					OP_ASSERT(element->IsAncestorOf(html_element));

					/* The element we're examining is inline content. We have
					   found its start but not its end. So the inline box is an
					   ancestor of this box. Then, in some cases, the area of
					   this block box must be included. Note that this takes
					   place during line traversal of this block box. */

					RECT tmp;
					AbsoluteBoundingBox bounding_box;

					box->GetBoundingBox(bounding_box, area_type != OFFSET_AREA);
					bounding_box.GetBoundingRect(tmp);
					UnionLocalRect(tmp);
				}
	}

	return FALSE;
}

/**
   any extra space to be included in bounding area. this
   is needed for check boxes and radio buttons that
   sometimes draw in the margins. the invalidation rect
   needs to include this extra space not to leave ugly
   residues when eg setting visibility to hidden.
*/
static BOOL
UseMargins(LayoutProperties* layout_props, InlineBox* box, BoxEdgesObject::AreaType area_type,
			   UINT8& extra_left, UINT8& extra_top, UINT8& extra_right, UINT8& extra_bottom)
{
	extra_left = extra_top = extra_right = extra_bottom = 0;

	if (area_type != BoxEdgesObject::BOUNDING_AREA ||
		layout_props->html_element->Type() != Markup::HTE_INPUT ||
		(layout_props->html_element->GetInputType() != INPUT_CHECKBOX &&
		 layout_props->html_element->GetInputType() != INPUT_CHECKBOX))
		return FALSE;

	Content* content = box->GetContent();
	if (!content || !content->IsReplaced() || !((ReplacedContent*)content)->IsForm())
		return FALSE;

	((FormContent*)content)->GetUsedMargins(extra_left, extra_top, extra_right, extra_bottom);
	return TRUE;
}

/** Enter inline box */

/* virtual */ BOOL
BoxEdgesObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (!box_finished)
	{
		BOOL first_line = FALSE;
		BOOL is_edges_element = layout_props->html_element == element;

		if (target_ancestor == layout_props->html_element)
		{
			target_ancestor_x = GetTranslationX() + LayoutCoord(box_area.left);
			target_ancestor_y = GetTranslationY() + LayoutCoord(box_area.top);
			target_ancestor = NULL;
		}

		if (is_edges_element)
		{
			if (!box_found)
			{
				first_line = TRUE;
				box_found = TRUE;
			}
		}

		// Only include children of the box-edges element for
		// area types that include children/overflow.
		if (box_found && (is_edges_element || area_type == CONTAINING_BLOCK_AREA || area_type == ENCLOSING_AREA || area_type == BOUNDING_AREA))
		{
			// Substring matching handled in BoxEdgesObject::HandleTextContent.
			if (element_text_start != 0 || element_text_end != -1)
				return TRUE;

			const HTMLayoutProperties& props = *layout_props->GetProps();
			RECT box_inline = { 0, 0, 0, 0 };

			BOOL skip_left = FALSE;
			BOOL skip_right = FALSE;
			BOOL force_left = FALSE;
			BOOL force_right = FALSE;
			if (area_type == CONTAINING_BLOCK_AREA)
			{
#ifdef SUPPORT_TEXT_DIRECTION
				if (props.direction == CSS_VALUE_rtl)
				{
					force_left = TRUE;
					skip_right = !first_line;
				}
				else
#endif // SUPPORT_TEXT_DIRECTION
				{
					skip_left = !first_line;
					force_right = TRUE;
				}
			}

			UINT8 extra_left, extra_top, extra_right, extra_bottom;
			BOOL use_margins = UseMargins(layout_props, box, area_type,
										  extra_left, extra_top, extra_right, extra_bottom);

			long edge_left = box_area.left;
			if ((area_type == CONTENT_AREA || area_type == PADDING_AREA) && start_of_box)
				edge_left += props.border.left.width + (area_type == CONTENT_AREA ? props.padding_left : 0);
			else if (use_margins)
				edge_left -= extra_left;

			if (!skip_left)
				box_inline.left = edge_left;

			long edge_right = box_area.right;
			if ((area_type == CONTENT_AREA || area_type == PADDING_AREA) && end_of_box)
				edge_right -= props.border.right.width + (area_type == CONTENT_AREA ? props.padding_right : 0);
			else if (use_margins)
				edge_right += extra_right;

			if (!skip_right)
				box_inline.right = edge_right;

			BOOL has_width = edge_right != edge_left;

			long edge = 0;
			if (has_width)
			{
				edge = box_area.top;
				if (area_type == CONTENT_AREA)
					edge += props.border.top.width + props.padding_top;
				else
					if (area_type == PADDING_AREA)
						edge += props.border.top.width;
			}

			if (use_margins)
				edge -= extra_top;

			box_inline.top = edge;

			edge = 0;
			if (has_width)
			{
				edge = box_area.bottom;
				if (area_type == CONTENT_AREA)
					edge -= props.border.bottom.width + props.padding_bottom;
				else
					if (area_type == PADDING_AREA)
						edge -= props.border.bottom.width;
			}

			if (use_margins)
				edge += extra_bottom;

			box_inline.bottom = edge;

			UnionLocalRect(box_inline, force_left, force_right, skip_left, skip_right, !is_edges_element);

			// The box edges list is not supported (or yet needed)
			// for CONTAINING_BLOCK_AREA.
			OP_ASSERT(!(box_edges_list && area_type == CONTAINING_BLOCK_AREA));

			if (box_edges_list && !box->GetTableContent())
			{
				if (area_type == ENCLOSING_AREA || layout_props->html_element == element)
					AddLocalRect(box_inline);
			}
		}
		else
			if (box->IsContainingElement())
				if (layout_props->html_element == next_container_element)
				{
					next_container_element = FindNextContainerElementOf(next_container_element, element);
					return TRUE;
				}
				else
					return FALSE;
	}

	return TRUE;
}

/** Leave vertical box */

/* virtual */ void
BoxEdgesObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (target_ancestor == layout_props->html_element)
	{
		/* At this moment, translation on y doesn't include the cell's content
		   beginning. Check whether this is ok. */
		OP_ASSERT(!box->IsTableCell());

		target_ancestor_x = GetTranslationX();
		target_ancestor_y = GetTranslationY();
		target_ancestor = NULL;
	}

	VisualTraversalObject::LeaveVerticalBox(layout_props, box, traverse_info);
}

/** Leave inline box */

/* virtual */ void
BoxEdgesObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	AreaTraversalObject::LeaveInlineBox(layout_props, box, box_area, start_of_box, end_of_box, traverse_info);

	if (!box_finished && end_of_box && layout_props->html_element == element)
		box_finished = TRUE;
}

/** Enter line */

/* virtual */ BOOL
BoxEdgesObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (!box_found)
	{
		LayoutCoord start(LAYOUT_COORD_MIN);
		LayoutCoord end(LAYOUT_COORD_MIN);

		if (next_container_element)
		{
			if (next_container_element->GetLayoutBox()->TraverseInLine())
				end = start = next_container_element->GetLayoutBox()->GetVirtualPosition();
		}
		else if (Box* box = element->GetLayoutBox())
		{
			start = box->GetVirtualPosition();
			end = start + box->GetWidth();
		}

		return start != LAYOUT_COORD_MIN && line->MayIntersectLogically(start, end);
	}
	else
	{
		if (!box_finished)
		{
			HTML_Element* start_elm = line->GetStartElement();
			if (start_elm != element && !element->IsAncestorOf(start_elm))
				box_finished = TRUE;
		}
		return !box_finished;
	}
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
BoxEdgesObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	return TRUE;
}

#endif // PAGED_MEDIA_SUPPORT

/** Enter table content. */

/* virtual */ BOOL
BoxEdgesObject::EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info)
{
	HTML_Element* html_element = content->GetHtmlElement();

	if (!box_finished)
	{
		/* When we are in list mode, TableContents should include both
		   the table box and the caption box, if any, but not the
		   anonymous container box. Here we add the table box to the
		   list */

		if (box_edges_list && element->IsAncestorOf(html_element))
		{
			AddLocalRect(LayoutCoord(0), table_top, content->GetWidth(), table_height);
		}
	}

	return TRUE;
}

/** Enter text box */

/* virtual */ void
BoxEdgesObject::EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment)
{
	if (!box_finished && layout_props->html_element == element)
		box_found = TRUE;
}

/** Leave text box */

/* virtual */ void
BoxEdgesObject::LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline)
{
	if (!box_finished && layout_props->html_element == element && segment.stop_element != element)
		box_finished = TRUE;
}

/** Enter table row. */

/* virtual */ BOOL
BoxEdgesObject::EnterTableRow(TableRowBox* row)
{
	if (!box_finished)
	{
		HTML_Element* html_element = row->GetHtmlElement();

		if (target_ancestor == html_element)
		{
			target_ancestor_x = GetTranslationX() + row->GetPositionedX();
			target_ancestor_y = GetTranslationY() + row->GetPositionedY();
			target_ancestor = NULL;
		}

		if (html_element == element)
		{
			TableContent* table = row->GetTable();

			box_found = TRUE;
			box_finished = TRUE;

			RECT edges;

			if (row->IsOverflowed() &&
				(area_type == ENCLOSING_AREA || area_type == BOUNDING_AREA) &&
				table->GetPlaceholder()->IsBlockBox())
			{
				/* The table-row has overflowing content, but it does not store
				   its bounding box. Use the table's bounding box. */

				BlockBox* table_box = (BlockBox*) table->GetPlaceholder();
				TableRowGroupBox* rowgroup = (TableRowGroupBox*) html_element->Parent()->GetLayoutBox();
				AbsoluteBoundingBox table_bounding_box;

				OP_ASSERT(rowgroup->IsTableRowGroup());

				table_box->GetBoundingBox(table_bounding_box);

				edges.left = table_bounding_box.GetX() - rowgroup->GetPositionedX() - row->GetPositionedX() - table->GetRowLeftOffset();
				edges.top = table_bounding_box.GetY() - rowgroup->GetPositionedY() - row->GetPositionedY();
				edges.right = edges.left + table_bounding_box.GetWidth();
				edges.bottom = edges.top + table_bounding_box.GetHeight();
			}
			else
			{
				edges.left = 0;
				edges.top = 0;
				edges.right = edges.left + table->GetWidth();
				edges.bottom = edges.top + row->GetHeight();
			}

			SetLocalRect(edges);
		}
		else
			if (html_element == next_container_element)
			{
				next_container_element = FindNextContainerElementOf(next_container_element, element);
				return TRUE;
			}
			else
				if (html_element->IsAncestorOf(element))
					return TRUE;
	}

	return FALSE;
}

/** Enter table row group. */

/* virtual */ BOOL
BoxEdgesObject::EnterTableRowGroup(TableRowGroupBox* rowgroup)
{
	HTML_Element* html_element = rowgroup->GetHtmlElement();

	if (html_element == element)
		/* We may enter the same element several times if it lives in multiple
		   columns. */

		box_finished = FALSE;

	if (!box_finished)
	{
		if (target_ancestor == html_element)
		{
			target_ancestor_x = GetTranslationX();
			target_ancestor_y = GetTranslationY();
			target_ancestor = NULL;
		}

		if (html_element == element)
		{
			TableContent* table = rowgroup->GetTable();
			RECT edges;

			if (rowgroup->IsOverflowed() &&
				(area_type == ENCLOSING_AREA || area_type == BOUNDING_AREA) &&
				table->GetPlaceholder()->IsBlockBox())
			{
				/* The table-row-group has overflowing content, but it does not
				   store its bounding box. Use the table's bounding box. */

				BlockBox* table_box = (BlockBox*) table->GetPlaceholder();
				AbsoluteBoundingBox table_bounding_box;

				table_box->GetBoundingBox(table_bounding_box);

				edges.left = table_bounding_box.GetX() - rowgroup->GetPositionedX() - table->GetRowLeftOffset();
				edges.top = table_bounding_box.GetY() - rowgroup->GetPositionedY();
				edges.right = edges.left + table_bounding_box.GetWidth();
				edges.bottom = edges.top + table_bounding_box.GetHeight();
			}
			else
			{
				edges.left = 0;
				edges.top = 0;
				edges.right = edges.left + table->GetWidth();
				edges.bottom = edges.top + rowgroup->GetHeight();
			}

			if (IsInMultiPane())
			{
				// This row group may live in multiple columns / pages.

				OpRect clipped_rect(pane_clip_rect);

				clipped_rect.x -= translation_x;
				clipped_rect.y -= translation_y;
				clipped_rect.IntersectWith(OpRect(&edges));

				RECT r = { clipped_rect.Left(), clipped_rect.Top(), clipped_rect.Right(), clipped_rect.Bottom() };

				UnionLocalRect(r);
			}
			else
				SetLocalRect(edges);

			box_found = TRUE;
			box_finished = TRUE;

			return FALSE;
		}

		if (html_element == next_container_element)
			next_container_element = FindNextContainerElementOf(next_container_element, element);

		return TRUE;
	}

	return FALSE;
}

/** Handle table column (group). */

/* virtual */ void
BoxEdgesObject::HandleTableColGroup(TableColGroupBox* col_group_box, LayoutProperties* table_lprops, TableContent* table, LayoutCoord table_top, LayoutCoord table_bottom)
{
	HTML_Element* html_element = col_group_box->GetHtmlElement();

	if (html_element == element)
		/* We may enter the same element several times if it lives in multiple
		   columns. */

		box_finished = FALSE;

	if (!box_finished && html_element == element)
	{
		LayoutCoord x = col_group_box->GetX();
		RECT edges = { x, table_top, x + col_group_box->GetWidth(), table_bottom };

		if (IsInMultiPane())
		{
			// This table column or column group may live in multiple columns / pages.

			OpRect clipped_rect(pane_clip_rect);

			clipped_rect.x -= translation_x;
			clipped_rect.y -= translation_y;
			clipped_rect.IntersectWith(OpRect(&edges));

			RECT r = { clipped_rect.Left(), clipped_rect.Top(), clipped_rect.Right(), clipped_rect.Bottom() };

			UnionLocalRect(r);
		}
		else
			SetLocalRect(edges);

		box_found = TRUE;
		box_finished = TRUE;
	}
}

/** Handle text content */

/* virtual */ void
BoxEdgesObject::HandleTextContent(LayoutProperties* layout_props,
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
	int word_start = (int)word_info.GetStart();
	int word_end = (int)(word_info.GetStart() + word_info.GetLength());

	// It's possible that element_text_end refers to a white space that is
	// not a neighbour to a word. In that case it will not be caught by the
	// overlapping word test below. If that's the case we should flag that
	// we're done and return.
	BOOL passed_end = element_text_end != -1 && element_text_end < word_start;

	if (box_found && passed_end)
	{
		box_finished = TRUE;
		return;
	}

	BOOL word_overlap = (word_start <= element_text_end || element_text_end == -1) && element_text_start <= word_end;

	// Only include children of the box-edges element for
	// area types that include children/overflow.
	if (box_found && !box_finished && (area_type == ENCLOSING_AREA ||
									   area_type == BOUNDING_AREA ||
									   area_type == CONTAINING_BLOCK_AREA ||
									   layout_props->html_element == element) && (word_overlap || passed_end))
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		BOOL found_start = element_text_start >= word_start && element_text_start <= word_end;
		BOOL found_end = element_text_end >= word_start && element_text_end <= word_end;

		// Calculate pixel offsets from word start and end to characters of interest.
		int word_pix_off_start = 0;
		int word_pix_off_end = 0;

		if (found_start)
		{
			int element_word_len = element_text_start - word_start;
			word_pix_off_start = visual_device->GetTxtExtentEx(word, element_word_len, GetTextFormat(props, word_info));
		}

		if (found_end)
		{
			int element_word_off = element_text_end - word_start;
			int element_word_len = word_end - element_text_end;
			word_pix_off_end = visual_device->GetTxtExtentEx(word + element_word_off,
															 element_word_len, GetTextFormat(props, word_info));
		}

		// If the last character of the requested range occurs after the
		// current word, include the trailing space width in the rectangle.
		if (element_text_end > word_end)
			word_pix_off_end -= space_width + justified_space_extra;

		RECT local_rect = { x, baseline - props.font_ascent,
							x + word_width,
							baseline + props.font_descent };

#ifdef SUPPORT_TEXT_DIRECTION
		if (props.direction == CSS_VALUE_rtl)
		{
			local_rect.left += word_pix_off_end;
			local_rect.right -= word_pix_off_start;
		}
		else
#endif
		{
			local_rect.left += word_pix_off_start;
			local_rect.right -= word_pix_off_end;
		}

		UnionLocalRect(local_rect, FALSE, FALSE, TRUE, TRUE, TRUE);

		// If we have found the last character we're done.
		if (found_end || passed_end)
			box_finished = TRUE;
	}
}

/** Check if this positioned element needs to be traversed. */

/* virtual */ BOOL
BoxEdgesObject::TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element)
{
	return !box_finished && element->IsAncestorOf(element);
}

/** Enter scrollable content. */

/* virtual */ BOOL
BoxEdgesObject::EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	/** offsetLeft and offsetTop should return values assuming that
		any scrollable ancestors are at scrolling offset 0,0.
		Also, containing block size/offsets calculated for
		CONTAINING_BLOCK_AREA, should also not be affected by
		scrolling. The correct amount of scrolling will be applied
		when used in traversal. */

	traverse_info.pretend_noscroll = (area_type == OFFSET_AREA || area_type == CONTAINING_BLOCK_AREA);

	return VisualTraversalObject::EnterScrollable(props, scrollable, width, height, traverse_info);
}


void
ClientRectObject::PushRect(LayoutCoord left, LayoutCoord top, LayoutCoord right, LayoutCoord bottom)
{
	OP_ASSERT(m_client_rect_list != NULL || m_bounding_rect != NULL);

	OpRect visual_viewport = GetDocument()->GetVisualViewport();

	LayoutCoord viewport_x(visual_viewport.Left());
	LayoutCoord viewport_y(visual_viewport.Top());

	if (m_bounding_rect)
	{
		if (m_bounding_rect->left > left - viewport_x)
			m_bounding_rect->left = left - viewport_x;

		if (m_bounding_rect->top > top - viewport_y)
			m_bounding_rect->top = top - viewport_y;

		if (m_bounding_rect->right < right - viewport_x)
			m_bounding_rect->right = right - viewport_x;

		if (m_bounding_rect->bottom < bottom - viewport_y)
			m_bounding_rect->bottom = bottom - viewport_y;
	}
	else
	{
		RECT *new_rect = OP_NEW(RECT, ());
		if (!new_rect)
			SetOutOfMemory();
		else
		{
			new_rect->left = left - viewport_x;
			new_rect->top = top - viewport_y;
			new_rect->right = right - viewport_x;
			new_rect->bottom = bottom - viewport_y;

			if (OpStatus::IsMemoryError(m_client_rect_list->Add(new_rect)))
				SetOutOfMemory();
		}
	}

	ResetRect();
}

/** Callback for objects needing to set up code before starting traversal */

/* virtual */ void
ClientRectObject::StartingTraverse()
{
	HTML_Element *candidate_elm = m_start_elm;
	HTML_Element *stop_elm = m_end_elm->Next();
	while (candidate_elm != stop_elm && !candidate_elm->GetLayoutBox())
		candidate_elm = candidate_elm->Next();

	if (candidate_elm != m_start_elm) // start is not visible, start at the beginning of next visible
		m_start_offset = 0;
	m_start_elm = candidate_elm;

	if (m_start_elm == stop_elm)
	{
		m_range_finished = TRUE;
	}
	else
	{
		candidate_elm = m_end_elm;
		while (candidate_elm != m_start_elm && !candidate_elm->GetLayoutBox())
			candidate_elm = candidate_elm->Prev();

		if (candidate_elm != m_end_elm)
			m_end_offset = candidate_elm->ContentLength();
		m_end_elm = candidate_elm;
	}

	ResetRect();
	if (m_bounding_rect)
	{
		m_bounding_rect->left = LONG_MAX;
		m_bounding_rect->top = LONG_MAX;
		m_bounding_rect->right = LONG_MIN;
		m_bounding_rect->bottom = LONG_MIN;
	}
}

/* virtual */ BOOL
ClientRectObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (!m_range_finished)
	{
		HTML_Element* html_element = box->GetHtmlElement();

		if (!m_start_found)
		{
			if (html_element->IsAncestorOf(m_start_elm))
			{
				if (html_element == m_start_elm)
					m_start_found = TRUE;

				if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
					return FALSE;

				return TRUE;
			}
			else
				return FALSE;
		}

		if (!box->GetTableContent() &&  // table content handled separately
			!html_element->IsAncestorOf(m_end_elm))
		{
			LayoutCoord x = GetTranslationX();
			LayoutCoord y = GetTranslationY();
			PushRect(x, y, x + box->GetWidth(), y + box->GetHeight());
		}
		else if (layout_props || TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
			return TRUE;
	}

	return FALSE;
}

/** Enter inline box */

/* virtual */ BOOL
ClientRectObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (!m_range_finished)
	{
		if (!m_skip_contents_of_this_inline)
		{
			HTML_Element *html_element = layout_props->html_element;
			if (!m_start_found)
			{
				if (html_element == m_start_elm)
					m_start_found = TRUE;
				else
					return TRUE;
			}

			if (!html_element->IsAncestorOf(m_end_elm) &&
				!box->GetTableContent())
			{
				LayoutCoord x = GetTranslationX();
				LayoutCoord y = GetTranslationY();

				// negative height or width indicates the strange situation where
				// the box is really not placed on this line but the next so we'll
				// push it there instead. Just because it is that way.
				if (box_area.right - box_area.left > 0 && box_area.bottom - box_area.top > 0)
				{
					PushRect(x + LayoutCoord(box_area.left), y + LayoutCoord(box_area.top), x + LayoutCoord(box_area.right), y + LayoutCoord(box_area.bottom));
					m_skip_contents_of_this_inline = box;
				}
			}
		}

		return TRUE;
	}

	// Only return FALSE if we are not caring about the rest of the
	// tree at all. We need to enter into inlines before that to be
	// sure to find the end-of-lines that might be within the inline.
	return FALSE;
}

/** Leave inline box */

/* virtual */ void
ClientRectObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	if (m_skip_contents_of_this_inline == box)
		m_skip_contents_of_this_inline = NULL;

	if (!m_range_finished && end_of_box && layout_props->html_element == m_end_elm)
		m_range_finished = TRUE;
}

/** Enter line */

/* virtual */ BOOL
ClientRectObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	// boxes creating a new block formatting context on a line will enter
	// a new line without leaving the last one so we need to push the rect
	// from the last line.
	if (m_box_edges.top != LONG_MAX)
		PushRect(LayoutCoord(m_box_edges.left),
				 LayoutCoord(m_box_edges.top),
				 LayoutCoord(m_box_edges.right),
				 LayoutCoord(m_box_edges.bottom));

	if (!m_range_finished && (m_start_found || containing_element->IsAncestorOf(m_start_elm)))
		return TRUE;

	return FALSE;
}

/** Leave line */

/* virtual */ void
ClientRectObject::LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (m_box_edges.top != LONG_MAX) // we have created a rect in the last line.
		PushRect(LayoutCoord(m_box_edges.left),
				 LayoutCoord(m_box_edges.top),
				 LayoutCoord(m_box_edges.right),
				 LayoutCoord(m_box_edges.bottom));
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
ClientRectObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	return TRUE;
}

#endif // PAGED_MEDIA_SUPPORT

/** Enter table content. */

/* virtual */ BOOL
ClientRectObject::EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info)
{
	if (!m_range_finished)
	{
		HTML_Element* html_element = layout_props->html_element;

		if ((m_start_found || m_start_elm == html_element) &&
			!html_element->IsAncestorOf(m_end_elm))
		{
			LayoutCoord x = GetTranslationX();
			LayoutCoord y = GetTranslationY();
			PushRect(x, y + table_top, x + content->GetWidth(), y + table_top + table_height);

			// don't traverse this table when we have already added it
			m_skip_contents_of_this_table = content;
		}
	}

	return TRUE;
}

/** Leave table content. */

/* virtual */ void
ClientRectObject::LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info)
{
	if (m_skip_contents_of_this_table == content)
		m_skip_contents_of_this_table = NULL;
}

/** Enter table row. */

/* virtual */ BOOL
ClientRectObject::EnterTableRow(TableRowBox* row)
{
	if (!m_range_finished && !m_skip_contents_of_this_table && !m_skip_contents_of_this_inline)
	{
		HTML_Element* html_element = row->GetHtmlElement();

		if (m_start_found ||
			html_element->IsAncestorOf(m_start_elm))
		{
			if (html_element == m_start_elm)
				m_start_found = TRUE;

			return TRUE;
		}
	}

	return FALSE;
}

/** Enter table row group. */

/* virtual */ BOOL
ClientRectObject::EnterTableRowGroup(TableRowGroupBox* rowgroup)
{
	if (!m_range_finished && !m_skip_contents_of_this_table && !m_skip_contents_of_this_inline)
	{
		HTML_Element* html_element = rowgroup->GetHtmlElement();

		if (m_start_found ||
			html_element->IsAncestorOf(m_start_elm))
		{
			if (html_element == m_start_elm)
				m_start_found = TRUE;

			return TRUE;
		}
	}

	return FALSE;
}

/** Enter text box */

/* virtual */ void
ClientRectObject::EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment)
{
	if (!m_range_finished && (m_start_found || layout_props->html_element == m_start_elm))
		m_text_block = layout_props->html_element->GetTextData();
}

/** Leave text box */

/* virtual */ void
ClientRectObject::LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline)
{
	if (m_box_edges.top != LONG_MAX) // we have created a rect in the last box
		PushRect(LayoutCoord(m_box_edges.left),
				 LayoutCoord(m_box_edges.top),
				 LayoutCoord(m_box_edges.right),
				 LayoutCoord(m_box_edges.bottom));
}

/** Handle text content */

/* virtual */ void
ClientRectObject::HandleTextContent(LayoutProperties* layout_props,
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
	if (!m_range_finished && !m_skip_contents_of_this_inline)
	{
		int offset_count = word - (m_text_block ? m_text_block->GetText() : 0);
		BOOL start_in_this_word = FALSE;
		BOOL end_in_this_word = layout_props->html_element == m_end_elm &&
			(m_end_offset == -1 ||
			offset_count + word_length >= m_end_offset);

		if (!m_start_found)
		{
			start_in_this_word = layout_props->html_element == m_start_elm &&
								(m_start_offset == 0 ||
								offset_count + word_length >= m_start_offset);

			// is this the right elm, and is the start offset in this word?
			if (!start_in_this_word)
			{
				m_prev_space = LayoutCoord(word_info.HasTrailingWhitespace() ? space_width : 0);
				return;
			}

			m_start_found = TRUE;
		}

		const HTMLayoutProperties& props = *layout_props->GetProps();
		LayoutCoord line_x = GetTranslationX();
		LayoutCoord line_y = GetTranslationY();

		// Calculate the pixel offset from the start of the word
		LayoutCoord word_offset_start(0);
		if (start_in_this_word)
		{
			int word_part_length = m_start_offset - offset_count;
			if (word_part_length < 0) // start was in preceding space
				word_offset_start = -m_prev_space;
			else if (word_part_length == word_length)
				word_offset_start = word_width;
			else if (word_part_length > 0)
				word_offset_start = LayoutCoord(visual_device->GetTxtExtentEx(word, word_part_length, GetTextFormat(props, word_info)));
		}

		// Calculate the pixel offset from the end of the word
		LayoutCoord word_offset_end(0);
		if (end_in_this_word)
		{
			int word_part_length = offset_count + word_length - m_end_offset;
			if (word_part_length == word_length) // ended in preceding space
				word_offset_end = word_width; // don't count any width of this word
			else if (word_part_length > 0)
				word_offset_end = LayoutCoord(visual_device->GetTxtExtentEx(word + (m_end_offset - offset_count), word_part_length, GetTextFormat(props, word_info)));
		}

		LayoutCoord text_y = LayoutCoord(baseline - props.font_ascent);
		if (m_box_edges.top > line_y + text_y)
			m_box_edges.top = line_y + text_y;

		if (m_box_edges.bottom < line_y + baseline + props.font_descent)
			m_box_edges.bottom = line_y + baseline + props.font_descent;

#ifdef SUPPORT_TEXT_DIRECTION
		if (!segment.left_to_right)
		{
			// BIDI turns things backside-forward
			LayoutCoord left_edge = line_x + x + word_offset_end;
			if (end_in_this_word)
			{
				if (m_box_edges.left > left_edge)
					m_box_edges.left = left_edge;
			}
			else
			{
				if (m_box_edges.left > left_edge - space_width - justified_space_extra)
					m_box_edges.left = left_edge - space_width - justified_space_extra;
			}

			if (m_box_edges.right < line_x + x + word_width - word_offset_start)
				m_box_edges.right = line_x + x + word_width - word_offset_start;
		}
		else
#endif // SUPPORT_TEXT_DIRECTION
		{
			if (m_box_edges.left > line_x + x + word_offset_start)
				m_box_edges.left = line_x + x + word_offset_start;

			LayoutCoord right_edge = line_x + x + word_width - word_offset_end;
			if (end_in_this_word)
			{
				if (m_box_edges.right < right_edge)
					m_box_edges.right = right_edge;
			}
			else
			{
				if (m_box_edges.right < right_edge + space_width + justified_space_extra)
					m_box_edges.right = right_edge + space_width + justified_space_extra;
			}
		}

		if (end_in_this_word)
			m_range_finished = TRUE;
		else
		{
			if (layout_props->html_element == m_end_elm && *word == ' ')
			{
				const uni_char *text = m_text_block->GetText();
				int text_len = m_text_block->GetTextLen();
				int i = offset_count;
				while (i != m_end_offset && i < text_len && uni_isspace(text[i]))
					i++;

				if (i == m_end_offset)
					m_range_finished = TRUE;
			}
		}

		m_prev_space = LayoutCoord(word_info.HasTrailingWhitespace() ? space_width : 0);
	}
}

/** Second-phase constructor. Returns FALSE on memory allocation error. */

BOOL
BorderCollapsedCells::Init(TableCollapsingBorderContent* table_content)
{
	table = table_content;

	int columns = table_content->GetColumnCount();

	if (columns > 0)
	{
		cells = OP_NEWAA(BorderCollapseCell, columns, 3);
		if (!cells)
			return FALSE;
	}

	return TRUE;
}

void
BorderCollapsedCells::CalculateCorners(int i)
{
#ifdef SUPPORT_TEXT_DIRECTION
	int columns = table->GetColumnCount();
	int cur_column = table->IsRTL() ? columns - 1 - i : i;
#else
	int cur_column = i;
#endif // SUPPORT_TEXT_DIRECTION

	if (table->IsColumnVisibilityCollapsed(cur_column))
		return;

	int prev_visible_i = i - 1;
	int next_visible_i = i + 1;

#ifdef SUPPORT_TEXT_DIRECTION
	if (table->IsRTL())
	{
		while (prev_visible_i >= 0 && table->IsColumnVisibilityCollapsed(columns - 1 - prev_visible_i))
			prev_visible_i --;

		while (next_visible_i < table->GetColumnCount() && table->IsColumnVisibilityCollapsed(columns - 1 - next_visible_i))
			next_visible_i ++;
	}
	else
#endif // SUPPORT_TEXT_DIRECTION
	{
		while (prev_visible_i >= 0 && table->IsColumnVisibilityCollapsed(prev_visible_i))
			prev_visible_i --;

		while (next_visible_i < table->GetColumnCount() && table->IsColumnVisibilityCollapsed(next_visible_i))
			next_visible_i ++;
	}

	int top_left_width = 0;
	int top_right_width = 0;
	int bottom_left_width = 0;
	int bottom_right_width = 0;
	TableCollapsingBorderCellBox* top_left_cell = NULL;
	TableCollapsingBorderCellBox* bottom_left_cell = NULL;
	TableCollapsingBorderCellBox* top_right_cell = cells[i][1].cell_box;
	TableCollapsingBorderCellBox* bottom_right_cell = cells[i][2].cell_box;

	if (prev_visible_i >= 0)
	{
		top_left_cell = cells[prev_visible_i][1].cell_box;
		bottom_left_cell = cells[prev_visible_i][2].cell_box;

		if (top_left_cell && top_left_cell != top_right_cell && top_left_cell != bottom_left_cell &&
			(!top_right_cell || top_right_cell->GetBorder().left.style != CSS_VALUE_hidden) &&
			(!bottom_left_cell || bottom_left_cell->GetBorder().top.style != CSS_VALUE_hidden))
			top_left_width = top_left_cell->GetBorder().bottom.width + top_left_cell->GetBorder().right.width;

		if (bottom_left_cell && bottom_left_cell != bottom_right_cell && bottom_left_cell != top_left_cell &&
			(!bottom_right_cell || bottom_right_cell->GetBorder().left.style != CSS_VALUE_hidden) &&
			(!top_left_cell || top_left_cell->GetBorder().bottom.style != CSS_VALUE_hidden))
			bottom_left_width = bottom_left_cell->GetBorder().top.width + bottom_left_cell->GetBorder().right.width;
	}

	if (top_right_cell && top_right_cell != bottom_right_cell && (prev_visible_i < 0 || top_right_cell != top_left_cell) &&
		(!top_left_cell || top_left_cell->GetBorder().right.style != CSS_VALUE_hidden) &&
		(!bottom_right_cell || bottom_right_cell->GetBorder().top.style != CSS_VALUE_hidden))
		top_right_width = top_right_cell->GetBorder().bottom.width + top_right_cell->GetBorder().left.width;

	if (bottom_right_cell && bottom_right_cell != top_right_cell && (prev_visible_i < 0 || bottom_right_cell != bottom_left_cell) &&
		(!top_right_cell || top_right_cell->GetBorder().bottom.style != CSS_VALUE_hidden) &&
		(!bottom_left_cell || bottom_left_cell->GetBorder().right.style != CSS_VALUE_hidden))
		bottom_right_width = bottom_right_cell->GetBorder().top.width + bottom_right_cell->GetBorder().left.width;

	TableCellBorderCornerStacking corner_stacking = bottom_right_width ? BOTTOM_RIGHT_ON_TOP : BORDER_CORNER_STACKING_NOT_SET;

	if (top_left_width > top_right_width)
	{
		if (top_left_width > bottom_left_width)
		{
			if (top_left_width > bottom_right_width)
				corner_stacking = TOP_LEFT_ON_TOP;
		}
		else
		{
			if (bottom_left_width > bottom_right_width)
				corner_stacking = BOTTOM_LEFT_ON_TOP;
		}
	}
	else
	{
		if (top_right_width > bottom_left_width)
		{
			if (top_right_width > bottom_right_width)
				corner_stacking = TOP_RIGHT_ON_TOP;
		}
		else
		{
			if (bottom_left_width > bottom_right_width)
				corner_stacking = BOTTOM_LEFT_ON_TOP;
		}
	}

	if (prev_visible_i >= 0 && top_left_cell != bottom_left_cell)
	{
		if (top_left_cell && top_left_cell != top_right_cell)
			top_left_cell->SetBottomRightCorner(corner_stacking);

		if (bottom_left_cell && bottom_left_cell != bottom_right_cell)
			bottom_left_cell->SetTopRightCorner(corner_stacking);
	}

	if (bottom_right_cell != top_right_cell)
	{
		if (top_right_cell && (prev_visible_i < 0 || top_right_cell != top_left_cell))
			top_right_cell->SetBottomLeftCorner(corner_stacking);

		if (bottom_right_cell && (prev_visible_i < 0 || bottom_right_cell != bottom_left_cell))
			bottom_right_cell->SetTopLeftCorner(corner_stacking);
	}

	if (next_visible_i == table->GetColumnCount() && top_right_cell != bottom_right_cell)
	{
		// last column

		if (top_right_cell && top_right_cell->GetBorder().bottom.style != CSS_VALUE_hidden &&
			(!bottom_right_cell || bottom_right_cell->GetBorder().top.style != CSS_VALUE_hidden))
			top_left_width = top_right_cell->GetBorder().bottom.width + top_right_cell->GetBorder().right.width;
		else
			top_left_width = 0;

		if (bottom_right_cell && bottom_right_cell->GetBorder().top.style != CSS_VALUE_hidden &&
			(!top_right_cell || top_right_cell->GetBorder().bottom.style != CSS_VALUE_hidden))
			bottom_left_width = bottom_right_cell->GetBorder().top.width + bottom_right_cell->GetBorder().right.width;
		else
			bottom_left_width = 0;

		if (top_left_width || bottom_left_width)
		{
			corner_stacking = top_left_width > bottom_left_width ? TOP_LEFT_ON_TOP : BOTTOM_LEFT_ON_TOP;

			if (top_right_cell)
				top_right_cell->SetBottomRightCorner(corner_stacking);

			if (bottom_right_cell)
				bottom_right_cell->SetTopRightCorner(corner_stacking);
		}
	}
}

void BorderCollapsedCells::FillRow(TableCollapsingBorderRowBox* next_row_box, BOOL restore)
{
	int columns = table->GetColumnCount();
	int cell_rowspan = 0;
	int cell_colspan = 0;

	TableCollapsingBorderCellBox* cell = 0;
	BOOL end_prev_rowspan = TRUE;

	/* Start at the leftmost cell, if there is a row to look in. (if there is
	   none, this method will just shift one row up) */

	if (next_row_box)
	{
#ifdef SUPPORT_TEXT_DIRECTION
		if (table->IsRTL())
			cell = (TableCollapsingBorderCellBox*) next_row_box->GetLastCell();
		else
#endif
			cell = (TableCollapsingBorderCellBox*) next_row_box->GetFirstCell();

		end_prev_rowspan = !next_row_box->Pred();
	}

	for (int i = 0; i < columns; i++)
	{
		int cur_column;

#ifdef SUPPORT_TEXT_DIRECTION
		if (table->IsRTL())
			cur_column = columns - 1 - i;
		else
#endif
			cur_column = i;

		if (restore)
		{
			cells[i][1].cell_box = NULL;
			cells[i][1].rowspan = 0;
			cells[i][1].colspan = 0;
		}
		else
		{
			// Advance row.

			cells[i][0] = cells[i][1];
			cells[i][1] = cells[i][2];
		}

		if (!end_prev_rowspan && cells[i][2].rowspan > 1)
			// The cell in this column still has more rows to span.

			cells[i][2].rowspan--;
		else
		{
			cells[i][2].cell_box = NULL;
			cells[i][2].rowspan = 0;
			cells[i][2].colspan = 0;
		}

		/* We normally have a cell, but not if the row is incomplete, and also
		   not if next_row_box is NULL (which happens when we're at the last
		   row in the table). */

		if (cell)
		{
			if (cell->GetColumn() == cur_column)
			{
				cell_rowspan = table->IsWrapped() ? 1 : cell->GetCellRowSpan();
				cell_colspan = cell->GetCellColSpan();
			}

			/* This cell's column number may be higher than cur_column if there
			   are cells spanned from previous rows in this row. */

			if (cell->GetColumn() <= cur_column)
			{
				// Fill out new row.

				cells[i][2].cell_box = cell;
				cells[i][2].rowspan = cell_rowspan;
				cells[i][2].colspan = cell_colspan;

				cell_colspan--;

				if (cell_colspan <= 0)
				{
					/* We have spanned all columns of this cell now. Move to
					   the next one (to the right of this one). */

#ifdef SUPPORT_TEXT_DIRECTION
					if (table->IsRTL())
						cell = (TableCollapsingBorderCellBox*) cell->Pred();
					else
#endif
						cell = (TableCollapsingBorderCellBox*) cell->Suc();
				}
			}
		}

		if (restore)
		{
			if (!cells[i][2].cell_box && next_row_box && next_row_box->Pred())
				/* We're restoring an incomplete row (a row that doesn't define cells for all the
				   columns in the table). This happens when a cell from a previous row spans into
				   this row, but also when the row simply doesn't define enough cells (with enough
				   colspan). This code tries to find some cell on a previous row. It is probably
				   somewhat broken, since it asks for a cell with rowspan 1 or more. If it has
				   rowspan 1, it has nothing to do with this row. */

				cells[i][2].cell_box = (TableCollapsingBorderCellBox*) TableRowBox::GetRowSpannedCell((TableRowBox*)next_row_box->Pred(), cur_column, 1, table->IsWrapped());
		}
		else
			CalculateCorners(i);
	}
}

void BorderCollapsedCells::PaintCollapsedCellBorders(PaintObject* paint_object, TableCollapsingBorderRowBox* next_row_box)
{
	if (rows_skipped)
	{
		/* Previous rows were skipped. Pull back, reset and recalculate borders/cells
		   now, so that we can paint them. */

		cur_row_box = next_row_box->GetPrevious();

		if (cur_row_box)
		{
			TableCollapsingBorderRowBox* prev_row_box = cur_row_box->GetPrevious();

			if (prev_row_box)
				FillRow(prev_row_box, TRUE);

			FillRow(cur_row_box, FALSE);
		}

		rows_skipped = FALSE;
	}

	FillRow(next_row_box, FALSE);

	int old_row_translation_x = row_translation_x;
	int old_row_translation_y = row_translation_y;

	row_translation_x = paint_object->GetTranslationX();
	row_translation_y = paint_object->GetTranslationY();

	if (cur_row_box)
	{
		BOOL clipped = FALSE;

		if (paint_object->IsInMultiPane())
		{
			/* Clip the background and borders of elements that might
			   start before or end after the current column or page. */

			OpRect clip_rect(paint_object->GetPaneClipRect());

			clip_rect.x -= paint_object->GetTranslationX();
			clip_rect.y -= paint_object->GetTranslationY();

			paint_object->GetVisualDevice()->BeginClipping(clip_rect);
			clipped = TRUE;
		}

		int columns = table->GetColumnCount();

		paint_object->Translate(LayoutCoord(old_row_translation_x - row_translation_x), LayoutCoord(old_row_translation_y - row_translation_y));

		LayoutCoord x(0);

		for (int i = 0; i < columns; i++)
		{
#ifdef SUPPORT_TEXT_DIRECTION
			int cur_column = table->IsRTL() ? columns - 1 - i : i;
#else
			int cur_column = i;
#endif // SUPPORT_TEXT_DIRECTION

			if (table->IsColumnVisibilityCollapsed(cur_column))
				continue;

			int prev_visible_i = i - 1;
			int next_visible_i = i + 1;

#ifdef SUPPORT_TEXT_DIRECTION
			if (table->IsRTL())
			{
				while (prev_visible_i >= 0 && table->IsColumnVisibilityCollapsed(columns - 1 - prev_visible_i))
					prev_visible_i --;

				while (next_visible_i < columns && table->IsColumnVisibilityCollapsed(columns - 1 - next_visible_i))
					next_visible_i ++;
			}
			else
#endif // SUPPORT_TEXT_DIRECTION
			{
				while (prev_visible_i >= 0 && table->IsColumnVisibilityCollapsed(prev_visible_i))
					prev_visible_i --;

				while (next_visible_i < columns && table->IsColumnVisibilityCollapsed(next_visible_i))
					next_visible_i ++;
			}

			LayoutCoord column_width = table->GetCellWidth(cur_column, 1, FALSE);

			TableCollapsingBorderCellBox* current_cell = cells[i][1].cell_box;
			TableCollapsingBorderCellBox* top_left_cell = NULL;
			TableCollapsingBorderCellBox* left_cell = NULL;
			TableCollapsingBorderCellBox* bottom_left_cell = NULL;
			TableCollapsingBorderCellBox* top_cell = cells[i][0].cell_box;
			TableCollapsingBorderCellBox* bottom_cell = cells[i][2].cell_box;
			TableCollapsingBorderCellBox* top_right_cell = NULL;
			TableCollapsingBorderCellBox* right_cell = NULL;
			TableCollapsingBorderCellBox* bottom_right_cell = NULL;

			if (prev_visible_i >= 0)
			{
				top_left_cell = cells[prev_visible_i][0].cell_box;
				left_cell = cells[prev_visible_i][1].cell_box;
				bottom_left_cell = cells[prev_visible_i][2].cell_box;
			}

			if (next_visible_i < columns)
			{
				top_right_cell = cells[next_visible_i][0].cell_box;
				right_cell = cells[next_visible_i][1].cell_box;
				bottom_right_cell = cells[next_visible_i][2].cell_box;
			}

			paint_object->Translate(x, LayoutCoord(0));

			if (current_cell)
				current_cell->PaintCollapsedBorders(paint_object, column_width, cur_row_box->GetHeight(), top_left_cell, left_cell, bottom_left_cell, top_cell, bottom_cell, top_right_cell, right_cell, bottom_right_cell);
			else
			{
				int start;
				int end;

				Border draw_border = cur_row_box->GetBorder();

				TableColGroupBox* column_box = table->GetColumnBox(i);
				if (column_box)
				{
					const Border* column_border = column_box->GetBorder();

					if (column_border)
					{
						if (first_row)
							draw_border.top.Collapse(column_border->top);

						if (i == 0)
							draw_border.left.Collapse(column_border->left);

						if (i == columns - 1)
							draw_border.right.Collapse(column_border->right);

						if (!next_row_box)
							draw_border.bottom.Collapse(column_border->bottom);
					}
				}

				if ((first_row && draw_border.top.width) || (!next_row_box && draw_border.bottom.width))
				{
					// top or bottom table border

					int left_border_width = 0;
					int right_border_width = 0;

					start = 0;
					end = column_width;

					if (prev_visible_i < 0)
					{
						// top or bottom left corner

						left_border_width = draw_border.left.width;
						start = -draw_border.left.width / 2;
					}
					else
						if (left_cell)
							start = left_cell->GetBorder().right.width / 2;

					if (next_visible_i >= columns)
					{
						// top right or bottom right corner

						right_border_width = draw_border.right.width;
						end += (draw_border.right.width + 1) / 2;
					}
					else
						if (right_cell)
							end -= right_cell->GetBorder().left.width / 2;

					if (first_row)	// top
						paint_object->GetVisualDevice()->LineOut(start, -draw_border.top.width / 2, end - start, draw_border.top.width, draw_border.top.style, draw_border.top.color, TRUE, TRUE, 0, right_border_width);
					else			// bottom
						paint_object->GetVisualDevice()->LineOut(start, cur_row_box->GetHeight() + (draw_border.bottom.width + 1) / 2 - 1, end - start, draw_border.bottom.width, draw_border.bottom.style, draw_border.bottom.color, TRUE, FALSE, left_border_width, right_border_width);
				}

				if ((prev_visible_i < 0 && draw_border.left.width) || (next_visible_i >= columns && draw_border.right.width))
				{
					// left or right table border

					int top_border_width = 0;
					int bottom_border_width = 0;

					start = 0;
					end = cur_row_box->GetHeight();

					if (top_cell)
						start = (top_cell->GetBorder().bottom.width + 1) / 2;
					else
						if (i && first_row)
						{
							// top right corner

							top_border_width = draw_border.top.width;
							start = -draw_border.top.width / 2;
						}

					if (bottom_cell)
						end -= bottom_cell->GetBorder().top.width / 2;
					else
						if (!next_row_box)
						{
							// bottom left or bottom right corner

							bottom_border_width = draw_border.bottom.width;
							end += (draw_border.bottom.width + 1) / 2;
						}

					if (i == 0)	// left
						paint_object->GetVisualDevice()->LineOut(-draw_border.left.width / 2, start, end - start, draw_border.left.width, draw_border.left.style, draw_border.left.color, FALSE, TRUE, 0, bottom_border_width);
					else		// right
						paint_object->GetVisualDevice()->LineOut(column_width + (draw_border.right.width + 1) / 2 - 1, start, end - start, draw_border.right.width, draw_border.right.style, draw_border.right.color, FALSE, FALSE, top_border_width, bottom_border_width);
				}
			}

			paint_object->Translate(-x, LayoutCoord(0));

			x += LayoutCoord(column_width);
		}
		paint_object->Translate(LayoutCoord(row_translation_x - old_row_translation_x),
								LayoutCoord(row_translation_y - old_row_translation_y));

		if (clipped)
			paint_object->GetVisualDevice()->EndClipping();

		first_row = FALSE;
	}

	if (!next_row_box)
		table->SetCornersCalculated(TRUE);

	cur_row_box = next_row_box;
}

void BorderCollapsedCells::FlushBorders(PaintObject* paint_object)
{
	if (rows_skipped)
		// We have not entered any rows we started to skip. Nothing to paint.

		return;

	PaintCollapsedCellBorders(paint_object, NULL);
}

RECT
TextSelectionObject::GetLocalSelectionFocusArea() const
{
	const VisualDeviceTransform *transform = GetVisualDeviceTransform();

	OpPoint conv1 = transform->ToLocal(OpPoint (orig_x, orig_y));
	OpPoint conv2 = transform->ToLocal(OpPoint (previous_x, previous_y));

	RECT l;

	if (conv1.x < conv2.x)
	{
		l.left = conv1.x;
		l.right = conv2.x + 1;
	}
	else
	{
		l.left = conv2.x;
		l.right = conv1.x + 1;
	}

	if (conv1.y < conv2.y)
	{
		l.top = conv1.y;
		l.bottom = conv2.y + 1;
	}
	else
	{
		l.top = conv2.y;
		l.bottom = conv1.y + 1;
	}

	return l;
}

BOOL
AreaTraversalObject::Intersects(const Content_Box *box, BOOL include_overflow) const
{
	AbsoluteBoundingBox bounding_box;
	box->GetBoundingBox(bounding_box, include_overflow);

	if (bounding_box.IsEmpty())
		return FALSE;

	RECT box_rect;
	bounding_box.GetBoundingRect(box_rect);

	if (box_rect.right == LAYOUT_COORD_MAX)
		box_rect.right = LONG_MAX;

	return IntersectsCustom(box_rect);
}

BOOL
AreaTraversalObject::Intersects(const Line *line) const
{
	BOOL treat_width_as_inf = line->GetBoundingBoxLeft() || line->GetBoundingBoxRight();

	RECT line_rect = {treat_width_as_inf ? LONG_MIN : 0,
					line->GetBoundingBoxTopInfinite() ? LONG_MIN : - line->GetBoundingBoxTop(),
					treat_width_as_inf ? LONG_MAX : line->GetWidth(),
					line->GetBoundingBoxBottomInfinite() ? LONG_MAX : line->GetLayoutHeight() + line->GetBoundingBoxBottom()};

	return IntersectsCustom(line_rect);
}

BOOL
AreaTraversalObject::Intersects(const TableRowBox *table_row) const
{
	if (table_row->RowspannedFrom() || table_row->IsOverflowed())
		return TRUE;

	RECT table_row_rect = {LONG_MIN,
							- table_row->GetBoundingBoxTop(),
							LONG_MAX,
							table_row->GetHeight() + table_row->GetBoundingBoxBottom()};

	return IntersectsCustom(table_row_rect);
}

BOOL
AreaTraversalObject::Intersects(const LineSegment& segment, LayoutCoord x, LayoutCoord word_width, LayoutCoord space_width) const
{
	LayoutCoord rtl_adjustment(0);

 #ifdef SUPPORT_TEXT_DIRECTION
 	if (!segment.left_to_right)
		rtl_adjustment -= space_width;
 #endif

	RECT segment_rect = {x + rtl_adjustment, LONG_MIN, x + rtl_adjustment + word_width, LONG_MAX};

	return IntersectsCustom(segment_rect);
}

BOOL
AreaTraversalObject::Intersects(const Content_Box *box, LayoutCoord x, LayoutCoord y, BOOL adjust_for_multicol) const
{
	AbsoluteBoundingBox bounding_box;
	box->GetBoundingBox(bounding_box, TRUE, adjust_for_multicol);

	RECT box_rect;
	bounding_box.GetBoundingRect(box_rect);

	box_rect.left += x;
	box_rect.top += y;

	if (bounding_box.GetHeight() == LAYOUT_COORD_MAX)
		box_rect.bottom = LONG_MAX;
	else
		box_rect.bottom += y;

	if (bounding_box.GetWidth() == LAYOUT_COORD_MAX)
		box_rect.right = LONG_MAX;
	else
		box_rect.right += x;

	return IntersectsCustom(box_rect);
}

BOOL
AreaTraversalObject::Intersects(const RECT &rect) const
{
	const VisualDeviceTransform *visual_device_transform = GetVisualDeviceTransform();
	return visual_device_transform->TestIntersection(rect, area);
}

BOOL
AreaTraversalObject::Intersects(const OpRect &rect) const
{
	const VisualDeviceTransform *visual_device_transform = GetVisualDeviceTransform();
	return visual_device_transform->TestIntersection(rect, area);
}

BOOL
AreaTraversalObject::IntersectsColumn(LayoutCoord left_limit, LayoutCoord right_limit) const
{
	RECT rect = {left_limit, INT32_MIN / 2, right_limit + 1, INT32_MAX / 2};

	return IntersectsCustom(rect);
}

BOOL
AreaTraversalObject::Intersects(const InlineBox *inline_box) const
{
	RECT r = {0, 0, inline_box->GetWidth(), inline_box->GetHeight()};

	return IntersectsCustom(r);
}

/** Paint all text decorations. */

/* virtual */ void
PaintObject::PaintTextDecoration(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord text_left, LayoutCoord text_width, COLORREF bg_color)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();
	int apply_text_decoration = props.text_decoration & (TEXT_DECORATION_OVERLINE | TEXT_DECORATION_LINETHROUGH | TEXT_DECORATION_UNDERLINE);
	int baseline;

	if (layout_props != segment.container_props)
	{
		baseline = props.textdecoration_baseline;

		if (props.GetHasDecorationAncestors())
			// Paint decorations defined by parents first

			PaintTextDecoration(layout_props->Pred(), segment, text_left, text_width, bg_color);
	}
	else
	{
		baseline = segment.line->GetBaseline();

		if (props.overline_color != USE_DEFAULT_COLOR)
			apply_text_decoration |= TEXT_DECORATION_OVERLINE;

		if (props.linethrough_color != USE_DEFAULT_COLOR)
			apply_text_decoration |= TEXT_DECORATION_LINETHROUGH;

		if (props.underline_color != USE_DEFAULT_COLOR)
			apply_text_decoration |= TEXT_DECORATION_UNDERLINE;
	}

	if (apply_text_decoration && props.visibility == CSS_VALUE_visible)
	{
		short text_height = props.font_ascent + props.font_descent - props.font_internal_leading;
		int line_width = 1 + text_height / 24;

		if (line_width == 1 && document->GetLayoutMode() != LAYOUT_NORMAL && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(document->GetPrefsRenderingMode(), PrefsCollectionDisplay::AvoidInterlaceFlicker)))
			line_width++;

#ifdef _PRINT_SUPPORT_
		BOOL printing = (visual_device->GetWindow()->GetPreviewMode() || visual_device->IsPrinter()) &&
						!g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrintBackground);
# define GET_TEXTDECORATION_COLOR(color) (printing ? visual_device->GetVisibleColorOnWhiteBg(color) : color)
#else
# define GET_TEXTDECORATION_COLOR(color) (color)
#endif

		if (apply_text_decoration & TEXT_DECORATION_OVERLINE)
		{
			int position = baseline - text_height * 9 / 10;

			if (position < 0)
				position = 0;

			COLORREF color = GET_TEXTDECORATION_COLOR(props.overline_color);

			if (minimum_color_contrast > 0 && bg_color != USE_DEFAULT_COLOR)
				color = CheckColorContrast(color, bg_color, minimum_color_contrast);

			visual_device->DecorationLineOut(text_left, position, text_width, line_width, color);
		}

		if (apply_text_decoration & TEXT_DECORATION_LINETHROUGH)
		{
			COLORREF color = GET_TEXTDECORATION_COLOR(props.linethrough_color);

			if (minimum_color_contrast > 0 && bg_color != USE_DEFAULT_COLOR)
				color = CheckColorContrast(color, bg_color, minimum_color_contrast);

			visual_device->DecorationLineOut(text_left, baseline - text_height / 3, text_width, line_width, color);
		}

		if (apply_text_decoration & TEXT_DECORATION_UNDERLINE)
		{
			int position = baseline + props.font_underline_position;
			COLORREF color = GET_TEXTDECORATION_COLOR(props.underline_color);
			int underline_width = props.font_underline_width;

			if (underline_width == 1 && document->GetLayoutMode() != LAYOUT_NORMAL && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(document->GetPrefsRenderingMode(), PrefsCollectionDisplay::AvoidInterlaceFlicker)))
				underline_width = 2;

			if (minimum_color_contrast > 0 && bg_color != USE_DEFAULT_COLOR)
				color = CheckColorContrast(color, bg_color, minimum_color_contrast);

/*#ifdef FONTSWITCHING
			if (document && document->GetHLDocProfile())
			{
				// Fix for Bug 157484. Since asian fonts have other position of baseline we have to increase underlineposition
				// by 1 so it doesn't overlap the text. Seems to be the same thing that IE and firefox do.
				OpFontInfo::CodePage preferred_codepage = document->GetHLDocProfile()->GetPreferredCodePage();
				if (preferred_codepage == OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE ||
					preferred_codepage == OpFontInfo::OP_TRADITIONAL_CHINESE_CODEPAGE ||
					preferred_codepage == OpFontInfo::OP_JAPANESE_CODEPAGE)
					position++;
			}
#endif // FONTSWITCHING*/

			visual_device->DecorationLineOut(text_left, position, text_width, underline_width, color);
		}
	}
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT

void PaintObject::AddMisspelling(const WordInfo& word_info, int text_left, int text_width, int space_width, LayoutProperties* layout_props, LineSegment& segment)
{
#ifdef _PRINT_SUPPORT_
	if (visual_device->GetWindow()->GetPreviewMode() || visual_device->IsPrinter())
		return;
#endif // _PRINT_SUPPORT

	if (document->GetDocumentEdit() && document->GetDocumentEdit()->GetDelayedMisspellWordInfo() == &word_info)
		return;

	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (props.white_space == CSS_VALUE_pre || props.white_space == CSS_VALUE_pre_wrap) // we can't trust space_width
	{
		HTML_Element* helm = layout_props->html_element;
		const uni_char* text = helm->TextContent();
		int sp_count = 0; // Number of spaces that we shouldn't draw as misspelled.
		unsigned int sp_start = MIN((int)(word_info.GetStart() + word_info.GetLength()), helm->GetTextContentLength());

		for(; sp_start > word_info.GetStart() && uni_isspace(text[sp_start-1]); sp_start--, sp_count++)
			;

		space_width = sp_count ? visual_device->GetTxtExtent(text + sp_start, sp_count) : 0;
	}

	text_width -= space_width;

#ifdef SUPPORT_TEXT_DIRECTION
	if (!segment.left_to_right)
		text_left += space_width;
#endif

	if ((text_left|text_width) & ~0xFFFF) // this values should be stored as UINT16...
		return;

	MisspellingPaintInfo* info = (MisspellingPaintInfo*)g_misspelling_paint_info_pool->New(sizeof(MisspellingPaintInfo));
	if (!info)
		return;

	info->next = misspelling_paint_info;
	info->word_left = text_left;
	info->word_width = text_width;
	misspelling_paint_info = info;
}

void PaintObject::PaintPendingMisspellings(LayoutProperties* layout_props, LayoutCoord base_line)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();
	short text_height = props.font_ascent + props.font_descent - props.font_internal_leading;
	int wave_height = text_height/9;
	int wave_width = wave_height;
	int line_base = base_line + wave_height;
	MisspellingPaintInfo* info = misspelling_paint_info;

	while (info)
	{
		MisspellingPaintInfo* next = info->next;
		visual_device->DrawMisspelling(OpPoint(info->word_left, line_base), (UINT32)info->word_width, (UINT32)wave_height, OP_RGB(0xFF, 0, 0), (UINT32)wave_width);
		g_misspelling_paint_info_pool->Delete(info);
		info = next;
	}

	misspelling_paint_info = NULL;
}

#endif // INTERNAL_SPELLCHECK_SUPPORT

PaintObject::PaintObject(FramesDocument* doc,
						 VisualDevice* visual_device,
						 const RECT& paint_area,
						 TextSelection* selection,
						 HTML_Element* highlighted_elm,
#ifdef _PRINT_SUPPORT_
						 BOOL check_color_on_white_bg,
#endif // _PRINT_SUPPORT_
						 int min_color_contrast,
						 long light_color,
						 long dark_color)
						 : VisualTraversalObject(doc, visual_device, paint_area),
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
						   m_current_hit(NULL),
						   m_inside_current_hit(FALSE),
						   m_has_more_hits(FALSE),
						   m_last_hit_textbox(NULL),
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
						   m_ellipsis_width(0),
						   m_draw_lastline_ellipsis(FALSE),
						   m_line_overflowed(FALSE),
						   start_word_number(0),
						   decoration_start(LAYOUT_COORD_MAX),
						   decoration_width(0),
						   text_selection(selection),
						   prev_trailing_whitespace_width(0),
						   paint_cell_background_only(FALSE),
#ifdef _PRINT_SUPPORT_
						   check_visible_color_on_white_bg(check_color_on_white_bg),
#endif // _PRINT_SUPPORT_
						   minimum_color_contrast(min_color_contrast),
						   light_font_color(light_color),
						   dark_font_color(dark_color),
						   highlighted_elm(highlighted_elm),
						   highlight_has_text(FALSE),
						   num_highlight_rects(0),
						   is_in_highlight(FALSE),
						   table_content_clip_rect(0, 0, INT32_MAX, 0),
						   is_in_outline(0),
						   is_in_inlinebox_outline(0),
						   m_incomplete_outline(FALSE)
#ifdef INTERNAL_SPELLCHECK_SUPPORT
						   , misspelling_paint_info(NULL)
#endif // INTERNAL_SPELLCHECK_SUPPORT
{
	SetEnterHidden(TRUE);
}

/* virtual */ BOOL
PaintObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	TraverseInfo dummy;

	if (AreaTraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, dummy))
	{
		if (traverse_info.dry_run)
			return TRUE;

		if (box->IsFixedPositionedBox())
		{
			AbsoluteBoundingBox bounding_box;
			box->GetBoundingBox(bounding_box);

			OpRect rect;
			bounding_box.GetBoundingRect(rect);

			OpRect bbox = visual_device->ToBBox(rect);
			GetLayoutWorkplace()->AddFixedContent(bbox);
		}

		const HTMLayoutProperties& props = *layout_props->GetProps();

#ifdef DEBUG_BOUNDING_BOX
		if (!box->GetScrollable())
		{
			AbsoluteBoundingBox bounding_box;

			box->GetBoundingBox(bounding_box);

			if (bounding_box.GetX() < 0 ||
				bounding_box.GetY() < 0 ||
				bounding_box.GetWidth() > box->GetWidth() ||
				bounding_box.GetHeight() > box->GetHeight())
				visual_device->RectangleOut(bounding_box.GetX(), bounding_box.GetY(), bounding_box.GetWidth(), bounding_box.GetHeight());
		}
#endif

		/* Begin using opacity if needed. If the element has no children and is not visible,
		   we know that we won't actually paint anything in it so it can be omitted (optimization). */

		if (props.opacity != 255 && (layout_props->html_element->FirstChild() || props.visibility == CSS_VALUE_visible))
		{
			OP_ASSERT(!traverse_info.has_buffered);

			AbsoluteBoundingBox bounding_box;

			box->GetBoundingBox(bounding_box);

			OpRect rect;
			bounding_box.GetBoundingRect(rect);

			if (!rect.IsEmpty() && OpStatus::IsSuccess(visual_device->BeginOpacity(rect, props.opacity)))
			{
				traverse_info.has_buffered = TRUE;
			}
		}

		if (props.clip_left != CLIP_NOT_SET && box->IsAbsolutePositionedBox())
		{
			OP_ASSERT(!traverse_info.has_clipped);
			PushClipRect(box->GetClippedRect(props, FALSE), traverse_info);
		}


		if (props.visibility == CSS_VALUE_visible && (props.IsOutlineVisible()
#ifdef DISPLAY_SPOTLIGHT
		                                              || visual_device->GetSpotlight(layout_props->html_element)
#endif
		                                              ))
		{
#ifdef DISPLAY_SPOTLIGHT
			if (VDSpotlight *sl = visual_device->GetSpotlight(layout_props->html_element))
				sl->SetProps(props);
#endif
			visual_device->BeginOutline(props.outline.width, props.outline.color, props.outline.style, layout_props->html_element, props.outline_offset);
			is_in_outline++;
		}


		if (!target_element)
		{
			if (GetTraverseType() == TRAVERSE_BACKGROUND && !box->GetTableContent() && props.visibility == CSS_VALUE_visible)
			{
				BOOL clipped = FALSE;

				if (TouchesCurrentPaneBoundaries(layout_props))
				{
					/* Clip the background and borders of elements that might
					   start before or end after the current column / page. */

					OpRect clip_rect(pane_clip_rect);

					clip_rect.x -= translation_x;
					clip_rect.y -= translation_y;

					visual_device->BeginClipping(clip_rect);
					clipped = TRUE;
				}

				box->PaintBgAndBorder(layout_props, GetDocument(), visual_device);

				if (clipped)
					visual_device->EndClipping();

				ButtonContainer* button_container = box->GetButtonContainer();

				if (button_container)
					button_container->PaintButton(visual_device, props);
			}

			if (!is_in_outline && props.visibility == CSS_VALUE_visible && props.GetIsInOutline())
			{
				// We have moved inside a skipped inline. Move up the cascade to find the outline
				LayoutProperties* iter = layout_props;
				while (iter && iter->GetProps()->outline.style == CSS_VALUE_none)
					iter = iter->Pred();

				OP_ASSERT(iter);
				if (iter)
				{
					const HTMLayoutProperties* outline_props = iter->GetProps();

					if (outline_props->visibility == CSS_VALUE_visible)
					{
						visual_device->BeginOutline(outline_props->outline.width, outline_props->outline.color, outline_props->outline.style, iter->html_element, props.outline_offset);
						is_in_outline++;
						traverse_info.rescued_outline_inside_inline = TRUE;
					}
				}
			}

			if (is_in_outline && (props.GetIsInOutline()
#ifdef DISPLAY_SPOTLIGHT
				                  || visual_device->GetSpotlight(layout_props->html_element)
#endif
								  ) && GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				OpRect r(0, 0, box->GetWidth(), box->GetHeight());

				if (TouchesCurrentPaneBoundaries(layout_props))
				{
					// This block may live in multiple columns / pages.

					OpRect clip_rect(pane_clip_rect);

					clip_rect.x -= translation_x;
					clip_rect.y -= translation_y;

					r.IntersectWith(clip_rect);
				}

				r = r.InsetBy(-props.outline_offset);

				OpRect this_bounding_rect(r);
				this_bounding_rect = this_bounding_rect.InsetBy(- props.outline.width);

				if (Intersects(this_bounding_rect))
				{
					visual_device->PushOutlineRect(r);
					outline_bounding_rect.UnionWith(visual_device->ToBBox(this_bounding_rect));
				}
			}

#ifdef DEBUG_BOUNDING_BOX
			if (!box->GetScrollable())
			{
				AbsoluteBoundingBox bounding_box;

				box->GetBoundingBox(bounding_box);

				if (bounding_box.GetX() < 0 ||
					bounding_box.GetY() < 0 ||
					bounding_box.GetWidth() > box->GetWidth() ||
					bounding_box.GetHeight() > box->GetHeight())
					visual_device->RectangleOut(bounding_box.GetX(), bounding_box.GetY(), bounding_box.GetWidth(), bounding_box.GetHeight());
			}
#endif // DEBUG_BOUNDING_BOX
		}

		if (props.overflow_x == CSS_VALUE_hidden && props.overflow_y == CSS_VALUE_hidden && !box->GetTableContent())
		{
			if (box->HasComplexOverflowClipping())
				traverse_info.handle_overflow = TRUE;
			else
			{
				BOOL clip_affects_target = box->GetClipAffectsTarget(target_element);

				if (clip_affects_target)
				{
					PopClipRect(traverse_info);
					PushClipRect(box->GetClippedRect(props, TRUE), traverse_info);
				}
			}
		}

		OP_ASSERT(is_in_inlinebox_outline == 0);

		return TRUE;
	}

	return FALSE;
}

/** Leave vertical box */

/* virtual */ void
PaintObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	AreaTraversalObject::LeaveVerticalBox(layout_props, box, traverse_info);

	if (layout_props->html_element == highlighted_elm)
		highlight_clip_rect = current_clip_rect;

	PopClipRect(traverse_info);

	if (traverse_info.has_buffered)
		visual_device->EndOpacity();

	while (is_in_inlinebox_outline > 0)
	{
		OP_ASSERT(is_in_outline);
		visual_device->EndOutline();
		is_in_outline--;
		is_in_inlinebox_outline--;
	}

	if (props.visibility == CSS_VALUE_visible && (props.IsOutlineVisible()
#ifdef DISPLAY_SPOTLIGHT
	                                              || visual_device->GetSpotlight(layout_props->html_element)
#endif
	                                              || traverse_info.rescued_outline_inside_inline))
	{
		OP_ASSERT(is_in_outline);
		visual_device->EndOutline();
		is_in_outline--;
	}

	if (m_incomplete_outline)
	{
		m_incomplete_outline = FALSE;

		// Create a dummy outline to invalidate this container;
		// Optimization could be to add extra outline on this element (for *:hover { outline } stylesheets)
		// Should probably include overflow also...

		OpRect rect(0, 0, box->GetWidth(), box->GetHeight());

		OpRect this_bounding_rect(rect);
		this_bounding_rect = this_bounding_rect.InsetBy(- props.outline.width);
		outline_bounding_rect.UnionWith(visual_device->ToBBox(this_bounding_rect));
	}

#ifdef DISPLAY_AVOID_OVERDRAW
	if (!target_element && GetTraverseType() == TRAVERSE_BACKGROUND && props.visibility == CSS_VALUE_visible)
		visual_device->FlushBackgrounds(layout_props->html_element);
#endif
}

void
PaintObject::LimitOpacityRect(OpRect& rect) const
{
	/* The layout coordinate system is bigger than we can handle at
	   the lower level. E.g. the transform system uses floats. Try to
	   keep the rectangle inside the "represented as integers" range
	   of floats. */

	const INT32 min_coord = - 1<<22, max_diff = 1<<23;
	OpRect limits(min_coord, min_coord, max_diff, max_diff);
	rect.SafeIntersectWith(limits);
}

void
PaintObject::BeginOpacityLayer(LayoutProperties* layout_props, const Line* line, InlineBox *box,
							   const RECT &box_area, TraverseInfo& traverse_info)
{
	OP_ASSERT(!traverse_info.has_buffered);
	const HTMLayoutProperties& props = *layout_props->GetProps();

	/* Retrieve the bounding box to make sure the backbuffer
	   will be large enough to contain the overflow. */

	AbsoluteBoundingBox bounding_box;

	if (box->IsPositionedBox() && static_cast<PositionedInlineBox*>(box)->IsContainingBlockCalculated())
	{
		static_cast<PositionedInlineBox*>(box)->GetBoundingBox(bounding_box);
		bounding_box.Translate(LayoutCoord(box_area.left), LayoutCoord(box_area.top));
	}
	else
	{
		StackingContext* stacking_context = box->GetLocalStackingContext();

		/** Checks for local stacking context in the case that the opacity has
			not caused a new stacking context. It really should, but we're currently
			not supporting it on run-ins for instance. */
		if (!stacking_context || !stacking_context->HasAbsolutePositionedElements())
			/* Use the Line bounding box, it's the best we got. */
			bounding_box = line_area;
		else
		{
			/* Use the bounding box of the container, it's the best we've got. */
			layout_props->container->GetPlaceholder()->GetBoundingBox(bounding_box);
			bounding_box.Translate(-line->GetX(), -line->GetY());
		}
	}

	OpRect rect;
	bounding_box.GetBoundingRect(rect);

	LimitOpacityRect(rect);

	if (!rect.IsEmpty() && OpStatus::IsSuccess(visual_device->BeginOpacity(rect, props.opacity)))
		traverse_info.has_buffered = TRUE;
}

/* virtual */ BOOL
PaintObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	prev_trailing_whitespace_width = 0;

	if (AreaTraversalObject::EnterInlineBox(layout_props, box, box_area, segment, start_of_box, end_of_box, baseline, traverse_info))
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();

		if (layout_props->html_element == highlighted_elm)
		{
			highlight_has_text = FALSE;
			is_in_highlight = TRUE;
		}
		else if (is_in_highlight)
		{
			/* We have another inlinebox inside the highlighted inlinebox.
			   Clear the list of highlightrects since we are not intrested in the rects if containing other things than text.
			   Highlight-drawing will fallback to GetBoxRect instead. (bug 240047)
			   Might want another solution later: To add highlight_rects for all inlineboxes, and making unions if intersecting rects.
			   */
			num_highlight_rects = 0;
			is_in_highlight = FALSE;
		}

		if (start_of_box && (props.IsOutlineVisible()
#ifdef DISPLAY_SPOTLIGHT
			                 || visual_device->GetSpotlight(layout_props->html_element)
#endif
							 ))
		{
#ifdef DISPLAY_SPOTLIGHT
			if (VDSpotlight* sl = visual_device->GetSpotlight(layout_props->html_element))
				sl->SetProps(props);
#endif // DISPLAY_SPOTLIGHT
			if (!visual_device->HasCurrentlyOpenOutline(layout_props->html_element))
			{
				visual_device->BeginOutline(props.outline.width, props.outline.color, props.outline.style, layout_props->html_element, props.outline_offset);
				is_in_outline++;
				is_in_inlinebox_outline++;
			}
		}

		/* Begin using opacity if needed. If the element has no children and is not visible,
		   we know that we won't actually paint anything in it so it can be omitted (optimization). */

		if (props.opacity != 255 && (layout_props->html_element->FirstChild() || props.visibility == CSS_VALUE_visible))
		{
			/* Calculate the line_area on-demand. It's only used for elements with
			   opacity < 1. The line_area will be empty if it has not been retrieved
			   after the reset in PaintObject::EnterLine. There is a possibility that
			   the line_area returned from GetBoundingBox actually will be empty and
			   additional calls will unnecessarily be made on the same Line, but that's
			   a corner case (zero width container with multiple zero-width children
			   with opacity < 1) with a cost we're willing to take. */
			if (line_area.IsEmpty())
				segment.line->GetBoundingBox(line_area);

			BeginOpacityLayer(layout_props, segment.line, box, box_area, traverse_info);
		}

		if (!target_element && layout_props->GetProps()->visibility == CSS_VALUE_visible)
		{
			if (!box->GetTableContent() && box_area.right - box_area.left > 0)
			{
				box->PaintBgAndBorder(layout_props, GetDocument(), visual_device, box_area, segment.start, start_of_box, end_of_box);
#ifdef DISPLAY_AVOID_OVERDRAW
				visual_device->FlushBackgrounds(layout_props->html_element);
#endif

				if (is_in_outline && (props.GetIsInOutline()
#ifdef DISPLAY_SPOTLIGHT
					                  || visual_device->GetSpotlight(layout_props->html_element)
#endif
									  ))
				{
					int outline_adjustment = props.outline_offset;

					OpRect r(box_area.left - outline_adjustment,
							 box_area.top - outline_adjustment,
							 box_area.right - box_area.left + 2 * outline_adjustment,
							 box_area.bottom- box_area.top + 2 * outline_adjustment);

					OpRect this_bounding_rect(r);
					this_bounding_rect = this_bounding_rect.InsetBy(- props.outline.width);
					if (Intersects(this_bounding_rect))
					{
						visual_device->PushOutlineRect(r);
						outline_bounding_rect.UnionWith(this_bounding_rect);
					}
				}
			}

			if (props.overflow_x != CSS_VALUE_visible && !box->IsInlineContent())
				/* Form objects have borders and padding 'inside' the actual
				   widget, so we can't clip here. Overflow for forms is handled
				   by scrollbars and stuff in widgets. */

				if (!box->GetFormObject() && !box->GetTableContent())
					if (box->HasComplexOverflowClipping())
						traverse_info.handle_overflow = TRUE;
					else
					{
						OpRect rect = box->GetClippedRect(props, TRUE);
						rect.OffsetBy(box_area.left, box_area.top);
						PushClipRect(rect, traverse_info);
					}

			ButtonContainer* button_container = box->GetButtonContainer();
			if (button_container)
			{
				visual_device->Translate(box_area.left, box_area.top);
				button_container->PaintButton(visual_device, props);
				visual_device->Translate(-box_area.left, -box_area.top);
			}
		}

		/* Check if this replaced content may need to trigger ellipsis.
		   Replaced content cannot be visually clipped like a text thus
		   in case of of overflow it is being skipped and replaced by ellipsis
		   The only exception from this rule is when replaced content is
		   first (and sometimes the only one) element in line - we paint such
		   element just not to having line consisting of ellipsis only.*/

		int text_overflow = segment.container_props->GetProps()->text_overflow;
		Content* content = box->GetContent();
		if (content && content->IsReplaced() &&																		// replaced content required
			(segment.start - segment.line->GetStartPosition() + box_area.left != 0 || m_draw_lastline_ellipsis) &&	// show ellipsis only if overflowing replaced content does not start a line or this is multiline box
			((text_overflow == CSS_VALUE__o_ellipsis_lastline || text_overflow == CSS_VALUE_ellipsis) &&			// text overflow must indicate need of ellipsis
				segment.container_props->GetProps()->overflow_x != CSS_VALUE_visible || m_draw_lastline_ellipsis))	// and either hotizontal overflow is clipped or this is multiline box
		{
			// Make sure we have initialized both
			OP_ASSERT(g_ellipsis_str_len && m_ellipsis_width);

			/* Text overflowed and possibly ellipsis is already shown
			   do not paint ellipsis but sice we already entered this inline give us a chance to leave it properly
			   by removing any clipping/opacity that has been applied when entering inline */
			if (m_line_overflowed)
			{
				traverse_info.skip_this_inline = TRUE;
				return TRUE;
			}

			const HTMLayoutProperties& container_props = *segment.container_props->GetProps();
			int line_right = segment.line->GetX() + segment.line->GetWidth();
			OpPoint scroll_offset = segment.container_props->html_element->GetLayoutBox()->GetContainer()->GetScrollOffset();

			BOOL ellipsis_overflows = FALSE;

			// We need an ellipsis either when subsequent replaced content overflows or this is the last visible line.
			BOOL needs_ellipsis = m_draw_lastline_ellipsis || segment.x + segment.line->GetUsedSpace() - scroll_offset.x > line_right;

#ifdef SUPPORT_TEXT_DIRECTION
				if (container_props.direction == CSS_VALUE_rtl && !segment.left_to_right)
					ellipsis_overflows = GetWordX(segment, box_area.left) - m_ellipsis_width < 0;
				else
#endif
					ellipsis_overflows = GetWordX(segment, box_area.right) - scroll_offset.x + m_ellipsis_width > line_right;

			if (ellipsis_overflows)
			{
				/* If an ellipsis overflows container and is needed at once, replaced element
				needs to be replaced with the ellipsis. If this replaced element starts a line
				we may need to set ellipsis coordinates first (if never set before) */
				if (needs_ellipsis)
				{
					if (m_ellipsis_point.x == INT_MIN)
					{
#ifdef SUPPORT_TEXT_DIRECTION
						if (container_props.direction == CSS_VALUE_rtl && !segment.left_to_right)
							m_ellipsis_point.x = box_area.right - m_ellipsis_width;
						else
#endif
							m_ellipsis_point.x = box_area.left;
					}

					m_line_overflowed = TRUE;
					PaintEllipsis(container_props, baseline);

					// skip painting this box
					traverse_info.skip_this_inline = TRUE;
					return TRUE;
				}
			}
			else
			{
#ifdef SUPPORT_TEXT_DIRECTION
				if (container_props.direction == CSS_VALUE_rtl && !segment.left_to_right)
					m_ellipsis_point.x = box_area.left - m_ellipsis_width;
				else
#endif
					m_ellipsis_point.x = box_area.right;

				/* If an ellipsis does not overflow container, just update it's coordinates.
				 In case this element ends a line that is supposed to show ellipsis, paint it now */

				BOOL replaced_ends_line =
#ifdef SUPPORT_TEXT_DIRECTION
				(container_props.direction == CSS_VALUE_rtl && !segment.left_to_right) ? segment.start - segment.line->GetStartPosition() + box_area.left == 0 :
#endif //SUPPORT_TEXT_DIRECTION
				segment.start - segment.line->GetStartPosition() + box_area.right == segment.line->GetUsedSpace();

				if (needs_ellipsis && replaced_ends_line)
				{
					m_line_overflowed = TRUE;
					PaintEllipsis(container_props, baseline);
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

void PaintObject::PaintEllipsis(const HTMLayoutProperties& container_props, short baseline)
{
	OP_ASSERT(g_ellipsis_str && g_ellipsis_str_len);

	if (m_ellipsis_point.x != INT_MIN && container_props.visibility == CSS_VALUE_visible)
	{
		container_props.SetDisplayProperties(visual_device);
		visual_device->SetColor(container_props.font_color);

#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)
		int old_font_number = visual_device->GetCurrentFontNumber();
		FontSupportInfo fsi(old_font_number);
		BOOL SwitchFont(const uni_char* str, int length, int& consumed, FontSupportInfo& font_info, WritingSystem::Script script);
		int consumed = 0;

		if (SwitchFont(g_ellipsis_str, g_ellipsis_str_len, consumed, fsi, container_props.current_script))
		{
			visual_device->SetFont(fsi.current_font->GetFontNumber());
			m_ellipsis_point.y = baseline - visual_device->GetFontAscent();
		}
#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING

		PaintTextShadow(container_props, m_ellipsis_point.x, m_ellipsis_point.y, g_ellipsis_str, g_ellipsis_str_len, 0, container_props.font_color, -1);
		visual_device->TxtOutEx(m_ellipsis_point.x, m_ellipsis_point.y, g_ellipsis_str, g_ellipsis_str_len, 0, -1);
#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)
		visual_device->SetFont(old_font_number);
#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING
	}
}

/** Flush backgrounds that may not have been painted yet, because of avoid-overdraw optimization. */

/*virtual */ void
PaintObject::FlushBackgrounds(LayoutProperties* layout_props, Box* box)
{
#ifdef DISPLAY_AVOID_OVERDRAW
	if (GetTraverseType() == TRAVERSE_BACKGROUND && layout_props->GetProps()->visibility == CSS_VALUE_visible)
		visual_device->FlushBackgrounds(layout_props->html_element);
#endif
}

/** Leave inline box */

/* virtual */ void
PaintObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	AreaTraversalObject::LeaveInlineBox(layout_props, box, box_area, start_of_box, end_of_box, traverse_info);

	if (is_in_highlight && highlight_has_text && num_highlight_rects < MAX_HIGHLIGHT_RECTS)
	{
		OpRect highlight(&box_area);
		highlight = GetVisualDevice()->ToBBox(highlight);

		if (!current_clip_rect.IsEmpty())
			highlight.IntersectWith(current_clip_rect);

		if (!highlight.IsEmpty())
			highlight_rects[num_highlight_rects++] = highlight;
	}

	if (layout_props->html_element == highlighted_elm)
	{
		highlight_clip_rect = current_clip_rect;
		is_in_highlight = FALSE;

		if (num_highlight_rects && !highlight_clip_rect.IsEmpty() && GetVisualDevice()->ToBBox(box_area).bottom > highlight_clip_rect.Bottom())
			num_highlight_rects = 0;
	}

	PopClipRect(traverse_info);

	// End opacity layer
	if (traverse_info.has_buffered)
		visual_device->EndOpacity();

	if (!target_element)
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();

#ifdef DISPLAY_AVOID_OVERDRAW
		if (GetTraverseType() == TRAVERSE_BACKGROUND && props.visibility == CSS_VALUE_visible)
			visual_device->FlushBackgrounds(layout_props->html_element);
#endif
	}

	if (end_of_box &&
		(layout_props->GetProps()->IsOutlineVisible()
#ifdef DISPLAY_SPOTLIGHT
	     || visual_device->GetSpotlight(layout_props->html_element)
#endif
	    ))
	{
		if (!is_in_outline)
		{
			// The entire outline was not painted, we need to reinvalidate
			m_incomplete_outline = TRUE;
		}
		else
		{
			is_in_outline--;
			is_in_inlinebox_outline--;
			visual_device->EndOutline();
		}
	}

	prev_trailing_whitespace_width = -1;
}

void
PaintObject::PaintText(const HTMLayoutProperties& props, const uni_char* word, int word_length, short word_width, int x, LayoutCoord baseline, LineSegment& segment, int text_format, const WordInfo& word_info, COLORREF text_color, short orig_word_width)
{
	int text_y = baseline - props.font_ascent;

	PaintTextShadow(props, x, text_y, word, word_length, text_format, text_color, orig_word_width);

	if (text_color != CSS_COLOR_transparent)
		visual_device->TxtOutEx(x, text_y, word, word_length, text_format, orig_word_width);
}

void PaintObject::PaintTextShadow(const HTMLayoutProperties& props, int x, int y, const uni_char* word, int word_length, int text_format, COLORREF text_color, short orig_word_width)
{
	if (props.text_shadows.GetCount())
	{
		Shadow shadow;
		CSSLengthResolver length_resolver(visual_device, FALSE, 0.0f, props.decimal_font_size_constrained, props.font_number, document->RootFontSize());
		Shadows::Iterator iter(props.text_shadows);
		iter.MoveToEnd();

		while (iter.GetPrev(length_resolver, shadow))
		{
			if (shadow.color == USE_DEFAULT_COLOR)
				shadow.color = props.font_color;
			if (shadow.color != USE_DEFAULT_COLOR)
			{
				visual_device->SetColor(shadow.color);

#ifdef LAYOUT_BEGIN_END_TEXT_SHADOW
				BeginTextShadow();
#endif // LAYOUT_BEGIN_END_TEXT_SHADOW

				if (shadow.blur && !visual_device->IsCurrentFontBlurCapable(shadow.blur))
				{
					DisplayEffect effect(DisplayEffect::EFFECT_TYPE_BLUR, shadow.blur);

					int word_width = visual_device->GetTxtExtentEx(word, word_length, text_format);
					OpRect rect(x + shadow.left, y + shadow.top, word_width, props.font_descent + props.font_ascent);

					BOOL begin_succeeded = OpStatus::IsSuccess(visual_device->BeginEffect(rect, effect));

					visual_device->TxtOutEx(x + shadow.left, y + shadow.top, word, word_length, text_format, orig_word_width);

					if (begin_succeeded)
						visual_device->EndEffect();
				}
				else
				{
					visual_device->SetFontBlurRadius(shadow.blur);
					visual_device->TxtOutEx(x + shadow.left, y + shadow.top, word, word_length, text_format, orig_word_width);
					visual_device->SetFontBlurRadius(0);
				}

#ifdef LAYOUT_BEGIN_END_TEXT_SHADOW
				EndTextShadow();
#endif // LAYOUT_BEGIN_END_TEXT_SHADOW
			}
		}

		visual_device->SetColor(text_color);
	}
}

/** Enter inline box */

/* virtual */ BOOL
PaintObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	const HTMLayoutProperties* parent_props = &parent_lprops->GetCascadingProperties();
	BOOL traverse = AreaTraversalObject::EnterLine(parent_lprops, line, containing_element, traverse_info);

	m_draw_lastline_ellipsis = FALSE;
	m_line_overflowed = FALSE;

	if (traverse)
	{
		if (!target_element)
		{
			if (line->IsFirstLine() && HasBackground(*parent_props))
			{
				LayoutCoord x_offset = line->GetTextAlignOffset(*parent_props);
				LayoutCoord width = line->GetUsedSpace();

				if (parent_props->text_align == CSS_VALUE_justify)
				{
					if (!line->HasForcedBreak() && line->GetNumberOfWords() > 1 && parent_props->white_space != CSS_VALUE_pre && parent_props->white_space != CSS_VALUE_pre_wrap)
					{
						width = line->GetWidth();
						x_offset = LayoutCoord(0);
					}
				}

				BackgroundsAndBorders bb(GetDocument(), visual_device, parent_lprops, &parent_props->border);
				OpRect border_box(x_offset, 0, width, line->GetLayoutHeight());
				bb.PaintBackgrounds(containing_element, parent_props->bg_color,
									parent_props->font_color,
									parent_props->bg_images,
									&parent_props->box_shadows,
									border_box);

	#ifdef DISPLAY_AVOID_OVERDRAW
				visual_device->FlushBackgrounds(containing_element);
	#endif
			}

	#ifdef DEBUG_BOUNDING_BOX
			if (line->GetBoundingBoxLeft() || line->GetBoundingBoxRight() || line->GetBoundingBoxTop() || line->GetBoundingBoxBottom())
			{
				COLORREF color = visual_device->GetColor();
				visual_device->SetColor(255, 0, 0);
				visual_device->RectangleOut(line->GetBoundingBoxLeft() ? visual_device->GetRenderingViewX() - visual_device->GetTranslationX() : 0,
											line->GetBoundingBoxTopInfinite() ? visual_device->GetRenderingViewY() - visual_device->GetTranslationY() : -line->GetBoundingBoxTop(),
											line->GetBoundingBoxRight() ? SHRT_MAX : line->GetWidth() + (line->GetBoundingBoxLeft() ? visual_device->GetTranslationX() - visual_device->GetRenderingViewX() : 0),
											line->GetBoundingBoxBottomInfinite() ? SHRT_MAX : line->GetLayoutHeight() + line->GetBoundingBoxBottom() + (line->GetBoundingBoxTopInfinite() ? visual_device->GetTranslationY() - visual_device->GetRenderingViewY() : line->GetBoundingBoxTop()));
				visual_device->SetColor(color);
			}
	#endif

		}

		if (parent_props->text_overflow == CSS_VALUE__o_ellipsis_lastline && parent_props->overflow_y != CSS_VALUE_visible)
		{
			OpRect content_rect;
			OpPoint scroll_offset;

			containing_element->GetLayoutBox()->GetContentEdges(*parent_props, content_rect, scroll_offset);
			content_rect.OffsetBy(scroll_offset);

			BOOL anon = FALSE;
			const Line* next_line = line->GetNextLine(anon);

			//draw ellipsis only if next line overflows container vertically.
			m_draw_lastline_ellipsis = next_line && next_line->GetY() + next_line->GetLayoutHeight() > content_rect.Bottom();
		}

		m_ellipsis_point.Set(INT_MIN, line->GetBaseline() - parent_props->font_ascent);
	}

	prev_trailing_whitespace_width = 0;

	traverse_info.old_line_area = line_area;
	line_area = AbsoluteBoundingBox();

	return traverse;
}

/** Leave line */

/* virtual */ void
PaintObject::LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	VisualTraversalObject::LeaveLine(parent_lprops, line, containing_element, traverse_info);
	m_line_overflowed = FALSE;

	line_area = traverse_info.old_line_area;
}

/** Enter text box */

/* virtual */ void
PaintObject::EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	OP_ASSERT(props.font_color != USE_DEFAULT_COLOR);

#ifdef _PRINT_SUPPORT_
	if (check_visible_color_on_white_bg)
		visual_device->SetColor(visual_device->GetVisibleColorOnWhiteBg(props.font_color));
	else
#endif // _PRINT_SUPPORT_
	{
		COLORREF font_color = props.font_color;
		if (minimum_color_contrast > 0 && props.current_bg_color != USE_DEFAULT_COLOR)
			font_color = CheckColorContrast(font_color, props.current_bg_color, minimum_color_contrast);

		if (font_color != visual_device->GetColor())
			visual_device->SetColor(font_color);
	}

	start_word_number = segment.word_number;
	decoration_start = LAYOUT_COORD_MAX;
	decoration_width = LayoutCoord(0);

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	/*
		Highlight painting may not be done in an order of highlight elements
		occurence on highlight list. This can lead to some search hits not
		being painted. Ensure that when switching text boxes m_has_more_hits
		will indicate wheather there are some highlights to be painted or not.
	*/
	if (box->GetSelected() && m_last_hit_textbox != box)
	{
		m_current_hit = NULL;

		if (document->GetHtmlDocument())
			m_current_hit = document->GetHtmlDocument()->GetSelectionElmFromHe(box->GetHtmlElement());

		m_has_more_hits = (m_current_hit != NULL);
		m_last_hit_textbox = box;
	}
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT
}

/** Leave text box */

/* virtual */ void
PaintObject::LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline)
{
	if (decoration_start != INT_MAX)
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		HTML_Element* parent = layout_props->html_element->Parent();

		// list_style_pos is inherited and a text element shouldn't meddle with that property.
		OP_ASSERT(layout_props->Pred()->GetProps()->list_style_pos == props.list_style_pos);
		// Do not paint decoration in case of an outside list item marker.
		if (!parent->GetIsListMarkerPseudoElement() || props.list_style_pos != CSS_VALUE_outside)
		{
			COLORREF bg_color = props.current_bg_color;

			if (props.text_bg_color != USE_DEFAULT_COLOR)
				bg_color = props.text_bg_color;

			PaintTextDecoration(layout_props, segment, decoration_start, decoration_width, bg_color);
		}
	}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if (misspelling_paint_info)
		PaintPendingMisspellings(layout_props, baseline);
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

/** Enter scrollable content. */

/* virtual */ BOOL
PaintObject::EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	OP_ASSERT(!traverse_info.has_clipped);

	Box* this_box = scrollable->GetOwningBox();

	traverse_info.has_clipped = this_box->GetClipAffectsTarget(target_element);

	if (traverse_info.has_clipped)
	{
		int scrollbar_modifier = scrollable->LeftHandScrollbar() ? scrollable->GetVerticalScrollbarWidth() : 0;

		if (GetTraverseType() == TRAVERSE_BACKGROUND)
			if (props.visibility == CSS_VALUE_visible)
			{
#ifdef DISPLAY_AVOID_OVERDRAW
				if (scrollable->HasVerticalScrollbar() || scrollable->HasHorizontalScrollbar())
					visual_device->FlushBackgrounds(OpRect(0, 0, this_box->GetWidth(), this_box->GetHeight()));
#endif

				scrollable->PaintScrollbars(props, visual_device, width, height);
			}
			else
				scrollable->UpdatePosition(props, visual_device, width, height);

		OpRect clip_rect;

		clip_rect.Set(props.border.left.width + scrollbar_modifier,
					  props.border.top.width,
					  width,
					  height);

		visual_device->BeginClipping(clip_rect);
		traverse_info.old_clip_rect = current_clip_rect;
		current_clip_rect = GetVisualDevice()->ToBBox(clip_rect);
	}

	if (props.visibility == CSS_VALUE_visible && !scrollable->GetVisibility())
		scrollable->SetVisibility(TRUE);

	if (props.text_overflow == CSS_VALUE_ellipsis || props.text_overflow == CSS_VALUE__o_ellipsis_lastline)
	{
		// store current ellipsis settings
		traverse_info.ellipsis_width = m_ellipsis_width;

		// calculate new ellipsis settings
		props.SetDisplayProperties(visual_device);
		m_ellipsis_width = visual_device->GetTxtExtentEx(g_ellipsis_str, g_ellipsis_str_len, 0);

		traverse_info.has_ellipsis_props = TRUE;
	}

	return TRUE;
}

/** Leave scrollable content. */

/* virtual */ void
PaintObject::LeaveScrollable(TraverseInfo& traverse_info)
{
	if (traverse_info.has_clipped)
	{
		visual_device->EndClipping();
		current_clip_rect = traverse_info.old_clip_rect;
	}

	if (traverse_info.has_ellipsis_props)
		// restore previously saved ellipsis settings
		m_ellipsis_width = traverse_info.ellipsis_width;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
PaintObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (GetTraverseType() == TRAVERSE_BACKGROUND && props.visibility == CSS_VALUE_visible)
		content->PaintPageControl(props, visual_device);

	if (!AreaTraversalObject::EnterPagedContainer(layout_props, content, width, height, traverse_info))
		return FALSE;

	if (content->GetPlaceholder()->GetClipAffectsTarget(target_element))
	{
		OpRect clip_rect(props.border.left.width, props.border.top.width, width, height);

		visual_device->BeginClipping(clip_rect);
		traverse_info.old_clip_rect = current_clip_rect;
		traverse_info.has_clipped = TRUE;
		current_clip_rect = GetVisualDevice()->ToBBox(clip_rect);
	}

	if (props.text_overflow == CSS_VALUE_ellipsis)
	{
		/* Support 'ellipsis', but not '-o-ellipsis-lastline'. The
		   text-overflow code doesn't handle -o-ellipsis-lastline for paged
		   containers, becasue there's virtual Y positions and all. */

		traverse_info.ellipsis_width = m_ellipsis_width;
		props.SetDisplayProperties(visual_device);
		m_ellipsis_width = visual_device->GetTxtExtentEx(g_ellipsis_str, g_ellipsis_str_len, 0);
		traverse_info.has_ellipsis_props = TRUE;
	}

	return TRUE;
}

/** Leave paged container. */

/* virtual */ void
PaintObject::LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info)
{
	if (traverse_info.has_clipped)
	{
		visual_device->EndClipping();
		current_clip_rect = traverse_info.old_clip_rect;
	}

	if (traverse_info.has_ellipsis_props)
		m_ellipsis_width = traverse_info.ellipsis_width;
}

#endif // PAGED_MEDIA_SUPPORT

/** Enter multi-column container. */

/* virtual */ void
PaintObject::EnterMultiColumnContainer(LayoutProperties* layout_props, MultiColumnContainer* content)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (GetTraverseType() == TRAVERSE_BACKGROUND && props.column_rule.width > 0 && props.visibility == CSS_VALUE_visible)
	{
		// Paint column rules.

		LayoutCoord column_gap = content->GetColumnGap();

		/* Calculate where to draw rules, specified in how many pixels from a
		   column's edge, plus container border and padding. */

		LayoutCoord rule_offset = LayoutCoord(props.border.left.width) + props.padding_left;

#ifdef SUPPORT_TEXT_DIRECTION
		if (props.direction == CSS_VALUE_rtl)
			rule_offset -= LayoutCoord((column_gap + props.column_rule.width) / 2);
		else
#endif // SUPPORT_TEXT_DIRECTION
			rule_offset += content->GetColumnWidth() + LayoutCoord((column_gap - props.column_rule.width) / 2);

		int rule_style = props.column_rule.style;

		if (rule_style == CSS_VALUE_inset)
			rule_style = CSS_VALUE_ridge;
		else
			if (rule_style == CSS_VALUE_outset)
				rule_style = CSS_VALUE_groove;

		BOOL start_found = IsPaneStarted();

		for (ColumnRow* row = content->GetFirstRow(); row; row = row->Suc())
		{
			if (current_pane)
				/* We're inside another multicol container. Skip rows that
				   precede the current column of the outer multicol
				   container. */

				if (!start_found)
					if (current_pane->GetStartElement().GetColumnRow() == row)
						start_found = TRUE;
					else
						continue;

			LayoutCoord y = row->GetPosition();
			LayoutCoord page_y = y;
			LayoutCoord height;

#ifdef PAGED_MEDIA_SUPPORT
			PagedContainer* paged_multicol = content->GetPagedContainer();

			if (paged_multicol)
				page_y += current_pane->GetTranslationY(); // relative to the current page
#endif // PAGED_MEDIA_SUPPORT

			if (row->Suc()
#ifdef PAGED_MEDIA_SUPPORT
				&& (!paged_multicol ||
					row->Suc()->GetPosition() - current_pane->GetTranslationY() < current_pane->GetHeight())
#endif // PAGED_MEDIA_SUPPORT
				)
				height = row->GetHeight();
			else
			{
				/* Last element in the multicol container (at least the last on the current
				   page). Draw the rules all the way to the bottom of the container. */

				LayoutCoord vertical_border_padding(props.border.top.width + props.padding_top +
													props.border.bottom.width + props.padding_bottom);

				height = content->GetHeight() - vertical_border_padding - page_y;

#ifdef PAGED_MEDIA_SUPPORT
				if (paged_multicol)
					height -= paged_multicol->GetPageControlHeight();
#endif // PAGED_MEDIA_SUPPORT
			}

			int i = 1;

			for (Column* column = row->GetFirstColumn(); column && column->Suc(); column = column->Suc(), i++)
			{
				LayoutCoord rule_x = column->GetX() + rule_offset;

#ifdef DISPLAY_AVOID_OVERDRAW
				visual_device->FlushBackgrounds(OpRect(rule_x, y, props.column_rule.width, height));
#endif // DISPLAY_AVOID_OVERDRAW

				LayoutCoord height_processed(0);
				int j = 0;

				for (Column* column = row->GetFirstColumn(); column && j < i; column = column->Suc(), j++)
					// Skip rules in areas covered by spanned pane floats.

					for (PaneFloatEntry* entry = column->GetFirstFloatEntry(); entry; entry = entry->Suc())
					{
						FloatedPaneBox* box = entry->GetBox();
						int column_span = box->GetColumnSpan();

						if (j + column_span > i)
						{
							LayoutCoord float_top = box->GetY() - box->GetMarginTop();
							LayoutCoord rule_height = float_top - height_processed;

							if (rule_height > 0)
								visual_device->LineOut(rule_x, y + height_processed, rule_height, props.column_rule.width,
													   rule_style, props.column_rule.color,
													   FALSE, TRUE, 0, 0);

							height_processed = float_top + box->GetHeight() + box->GetMarginTop() + box->GetMarginBottom();
						}
					}

				LayoutCoord rule_height = height - height_processed;

				if (rule_height > 0)
					visual_device->LineOut(rule_x, y + height_processed, rule_height, props.column_rule.width,
										   rule_style, props.column_rule.color,
										   FALSE, TRUE, 0, 0);
			}

			if (current_pane)
				/* We're inside another multicol container. Stop when we
				   find the stop element of the current column of the outer
				   multicol container. */

				if (current_pane->GetStopElement().GetColumnRow() == row)
					break;
		}
	}
}

/** Enter column in multicol container or page in paged container. */

/* virtual */ BOOL
PaintObject::EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info)
{
	// Bypass VisualTraversalObject, like a real traversal king!

	return AreaTraversalObject::EnterPane(multipane_lprops, pane, is_column, traverse_info);
}

/** Leave column in multicol container. */

/* virtual */ void
PaintObject::LeavePane(TraverseInfo& traverse_info)
{
	// Bypass VisualTraversalObject, like a real traversal king!

	AreaTraversalObject::LeavePane(traverse_info);
}

/** Handle border collapsing for table row. */

/* virtual */ void
PaintObject::HandleCollapsedBorders(TableCollapsingBorderRowBox* row, TableContent* table)
{
	if (GetTraverseType() == TRAVERSE_BACKGROUND && !row->IsVisibilityCollapsed())
	{
		BorderCollapsedCells* border_collapsed_cells = (BorderCollapsedCells*) border_collapsed_cells_list.Last();

		/* A border_collapsed_cells for the current table exists only in the
		   traversal pass for the table, not when traversing positioned
		   descendants of the table. */

		if (border_collapsed_cells && border_collapsed_cells->GetTableContent() == table)
		{
#ifdef DISPLAY_AVOID_OVERDRAW
			// Backgrounds for columns or rows might have been painted since last flush. Must be flushed now.
			visual_device->FlushBackgrounds(OpRect(0, 0, table->GetWidth(), row->GetHeight()));
#endif

			border_collapsed_cells->PaintCollapsedCellBorders(this, row);
		}
	}
}

/** Enter table row. */

/* virtual */ BOOL
PaintObject::EnterTableRow(TableRowBox* row)
{
	if (AreaTraversalObject::EnterTableRow(row))
	{
		if (table_content_clip_rect.width != INT32_MAX)
		{
			// Unusual clipping handling. See PaintObject::EnterTableContent for clarification. */
			current_clip_rect = table_content_clip_rect;
			if (!current_clip_rect.IsEmpty())
				transform_root.Apply(current_clip_rect);

#ifdef CSS_TRANSFORMS
			// The way how table_content_clip_rect is handled depends on that.
			OP_ASSERT(!row->GetTransformContext());
#endif // CSS_TRANSFORMS

			OpRect clip_rect_local = table_content_clip_rect;
			clip_rect_local.OffsetBy(-translation_x, -translation_y);
			visual_device->BeginClipping(clip_rect_local);
		}

		return TRUE;
	}
	else
		/* We're not entering this table row, but for the border-collapsed table model we
		   need to flush pending borders, and tell BorderCollapsedCells that next time we
		   decide to enter a table row, all border-collapsing state must be recalculated. */

		if (GetTraverseType() == TRAVERSE_BACKGROUND && !row->IsVisibilityCollapsed() && row->IsTableCollapsingBorderRowBox())
			if (BorderCollapsedCells* border_collapsed_cells = (BorderCollapsedCells*) border_collapsed_cells_list.Last())
				if (!border_collapsed_cells->AreRowsSkipped())
				{
					TableContent* table = row->GetTable();

					if (border_collapsed_cells->GetTableContent() == table)
					{
						// Flush pending borders.

						border_collapsed_cells->FlushBorders(this);

						// Invalidate all data, so that it's recalculated the next time we traverse (leave) a table row.

						border_collapsed_cells->SetRowsSkipped();
					}
				}

	//SetElementsSkipped(TRUE);

	return FALSE;
}

/** Leave table row. */

/* virtual */ void
PaintObject::LeaveTableRow(TableRowBox* row, TableContent* table)
{
	AreaTraversalObject::LeaveTableRow(row, table);

	if (table_content_clip_rect.width != INT32_MAX)
		/* Pop the table content's clip rect now, before potential handling
		   of collapsed borders. */
		visual_device->EndClipping();

	if (GetTraverseType() == TRAVERSE_BACKGROUND && table->GetCollapseBorders())
	{
		OP_ASSERT(row->IsTableCollapsingBorderRowBox());
		HandleCollapsedBorders((TableCollapsingBorderRowBox*) row, table);
	}
}

/** Handle table column (group). */

/* virtual */ void
PaintObject::HandleTableColGroup(TableColGroupBox* col_group_box, LayoutProperties* table_lprops, TableContent* table, LayoutCoord table_top, LayoutCoord table_bottom)
{
	LayoutProperties* layout_props = table_lprops->GetChildProperties(document->GetHLDocProfile(), col_group_box->GetHtmlElement());

	if (!layout_props)
	{
		SetOutOfMemory();
		return;
	}

	col_group_box->UpdateProps(*layout_props->GetProps());
}

/** Handle selectable box */

/*virtual */void
PaintObject::HandleSelectableBox(LayoutProperties* layout_props)
{
	Box* box = layout_props->html_element->GetLayoutBox();

	if (layout_props->GetProps()->visibility == CSS_VALUE_visible && box->GetSelected() && text_selection && !text_selection.IsEmpty())
		visual_device->DrawSelection(OpRect(0, 0, box->GetWidth(), box->GetHeight()));
}

/** Handle selectable box */

/* virtual */ void
PaintObject::HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break)
{
#ifdef LAYOUT_CARET_SUPPORT
	if (is_layout_break)
	{
		/* Need to paint something for standalone linebreaks. */
		Box* box = layout_props->html_element->GetLayoutBox();

		if (box->GetSelected() && document->GetCaretPainter() && text_selection && !text_selection.IsEmpty())
		{
			const HTMLayoutProperties& props = *layout_props->GetProps();
			UINT32 sel_bgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);

			int line_height = props.GetCalculatedLineHeight(GetVisualDevice());
			RECT selected_area = { 0, 0, SEL_BR_WIDTH, line_height};

#ifdef SUPPORT_TEXT_DIRECTION
			if (props.direction == CSS_VALUE_rtl)
			{
				selected_area.left -= SEL_BR_WIDTH;
				selected_area.right -= SEL_BR_WIDTH;
			}
#endif // SUPPORT_TEXT_DIRECTION

			visual_device->SetBgColor(sel_bgcol);
			visual_device->DrawBgColor(selected_area);
		}
	}
#endif // LAYOUT_CARET_SUPPORT
}

/** Handle text content */

/* virtual */ void
PaintObject::HandleTextContent(LayoutProperties* layout_props,
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
	const HTMLayoutProperties& props = *layout_props->GetProps();

	space_width += justified_space_extra;
	prev_trailing_whitespace_width = space_width;
	short orig_word_width = word_width;
	word_width += space_width;
	BOOL word_width_shortened = FALSE;

	if (word_info.IsTabCharacter())
	{
		/* Don't draw tab characters. */

		OP_ASSERT(word_length == 1);
		word_length = 0;
	}

	const HTMLayoutProperties& container_props = *segment.container_props->GetProps();

	BOOL draw_text = !props.GetAwaitingWebfont() && (Intersects(segment, x, word_width, space_width) || props.HasTextShadow());

	/* Usually we should handle text only if it intersects with clipping rectangle. When selecting text, if selection clipping rect
	intersects with ellipsis area, ellipsis will have to be repainted. From the other side, in order to paint ellipsis we need
	to traverse whole line to get correct ellipsis x position. Therefore we allow to calculate ellipsis coordinates
	even if handled word is outside selection clipping rect. Words outside clipping rect are only subject
	to ellipsis calculations - no painting is really done */

	if (draw_text || text_selection && (m_draw_lastline_ellipsis || container_props.text_overflow == CSS_VALUE_ellipsis))
	{
#if (defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT) || defined(LAYOUT_CARET_SUPPORT)
		HTML_Element* html_element = layout_props->html_element;
#endif

		int text_format = GetTextFormat(props, word_info);
		int text_y = baseline - props.font_ascent;
		int text_x = x;
		BOOL overflow = FALSE;
		BOOL skip_this_word = FALSE;

		if (m_ellipsis_width && (container_props.text_overflow == CSS_VALUE__o_ellipsis_lastline ||
			container_props.text_overflow == CSS_VALUE_ellipsis) &&	container_props.overflow_x != CSS_VALUE_visible ||
			m_draw_lastline_ellipsis)
		{
			/* This implementation will make things look a bit weird in writing
			systems that use glyph-joining (such as Arabic, where characters that
			are supposed to be displayed in their initial or middle presentation
			form may erroneously end up being displayed in their final or isolated
			form).

			The right solution to this problem would be to extend the VisualDevice
			APIs for text metrics and rendering, so that we can send the whole word
			to VisualDevice, and specify that we only want to render, or measure the
			width of, a specified number of characters. OpPainter would also need a
			similar change. This may not work on all platforms, though.

			An alternative solution that we can do internally, is to always tell
			VisualDevice to draw the entire word, but before doing so, we set a clip
			rectangle, to make sure that we only get the characters that we want
			This has two problems:
			1: Glyphs with negative bearings may get partially clipped when they
			shouldn't, or not be fully clipped when they should be.
			2: It doesn't really solve the problem for glyph-joining writing systems
			either; Arabic characters will be shown in their correct presentation
			forms, but the glyphs that are supposed to be fully visible may be
			partially or fully clipped (or glyphs that should be clipped may be
			partially or fully visible), because there is no way to measure the
			correct width of the visible characters.

			Final note about this implementation: It is experimental and very basic.
			It is optimized for code simplicity, not speed. :) It also has quite a
			few known errors (BIDI is not fully supported). I believe that a perfect
			solution cannnot be done entirely during traversal --
			we need to do some parts during layout. */

			int line_right = segment.line->GetX() + segment.line->GetWidth();
			OpPoint scroll_offset = segment.container_props->html_element->GetLayoutBox()->GetContainer()->GetScrollOffset();

			/*
			This is where we decide if we need ellipsis to be drawn or not.
			If content overflows container horizontaly or we're forced to draw ellipsis
			because of '-o-ellipsis-last-line' we need to check if ellipsis still fits
			into container's visible area. If not, current word is either shortened
			or completely skipped (depending on ellipsis mode used) allowing ellipsis
			to be fully visible.
			*/

			BOOL trim_this_word = FALSE;

			BOOL overflows_x = segment.x +
#ifdef SUPPORT_TEXT_DIRECTION
				segment.offset +
#endif //SUPPORT_TEXT_DIRECTION
				segment.length - scroll_offset.x > line_right;

#ifdef SUPPORT_TEXT_DIRECTION
			if (container_props.direction == CSS_VALUE_rtl)
				overflows_x = segment.x + segment.offset < 0;
#endif

			if (overflows_x || m_draw_lastline_ellipsis)
			{
#ifdef SUPPORT_TEXT_DIRECTION
				if (container_props.direction == CSS_VALUE_rtl)
					trim_this_word = GetWordX(segment, text_x) < m_ellipsis_width;
				else
#endif
					trim_this_word = GetWordX(segment, text_x) + (int)word_info.GetWidth() - scroll_offset.x > line_right - m_ellipsis_width;
			}

			/* The logical end coordinate of this word relative to the segment */
			int segment_logical_word_end_offset = text_x + word_width;

#ifdef SUPPORT_TEXT_DIRECTION
			/* We are comparing logical (bidi-unsorted) coordinates here, so any rtl
			   word offset needs to be reversed again.  */
			if (!segment.left_to_right)
				segment_logical_word_end_offset = segment.length - text_x;
#endif

			BOOL last_word_in_line = segment.start - segment.line->GetStartPosition() + segment_logical_word_end_offset == segment.line->GetUsedSpace();

			/* Word overflows container if it needs to be shortened to fit ellipsis or
			this is last word in line that is supposed to show ellipsis */
			overflow = trim_this_word || m_draw_lastline_ellipsis && last_word_in_line;

			// Check if current word overflows container horizontally.
			if (trim_this_word)
			{
				if (m_draw_lastline_ellipsis)
				{
					/* If -o-ellipsis-lastline is used we have to:
					- check if this is the very first word in line to be drawn (ellipsis.x == INT_MIN).
					If so we still need to draw it so we will trim it (works like 'text-overflow:ellipsis')

					- if this in not the first word in line (ellipsis.x != INT_MIN)
					we have to prevent this word from being painted as we're not supposed to break word
					in half when '-o-ellipsis-lastline' is used. We still can try to use it's coordinates
					to paint an ellipsis (if possible).
					*/

					if (m_ellipsis_point.x != INT_MIN)
						skip_this_word = TRUE;
				}

				int width_available = 0;
				if (!skip_this_word)
				{
					// How much space is left?
					width_available = scroll_offset.x + line_right - (GetWordX(segment, text_x) + m_ellipsis_width);

	#ifdef SUPPORT_TEXT_DIRECTION
					if (container_props.direction == CSS_VALUE_rtl)
						width_available =(int)word_info.GetWidth() - (m_ellipsis_width - (segment.x + segment.offset + text_x - segment.line->GetX())); // FIXME scroll offset
	#endif //SUPPORT_TEXT_DIRECTION

					/* If there is not enough space to fit at least one letter from a word and an ellipsis afterwards
					current word should not be painted */
					if (width_available <= 0)
						skip_this_word = TRUE;
				}

				if (!skip_this_word)
				{
#ifdef SUPPORT_TEXT_DIRECTION
					int old_word_width = visual_device->GetTxtExtentEx(word, word_length, text_format);
#endif //SUPPORT_TEXT_DIRECTION

					word_width_shortened = TRUE;

					// Calculate the number of characters to use
					// We could optimize this if necessary.
					for (; word_length > 0 && word_width > width_available;)
					{
						word_length--;
						word_width = LayoutCoord(visual_device->GetTxtExtentEx(word, word_length, text_format));
					}

					if (word_length)
					{
#ifdef SUPPORT_TEXT_DIRECTION
						if (container_props.direction == CSS_VALUE_rtl)
							text_x += (old_word_width - word_width);
#endif //SUPPORT_TEXT_DIRECTION

						m_ellipsis_point.x =
#ifdef SUPPORT_TEXT_DIRECTION
							(container_props.direction == CSS_VALUE_rtl) ? text_x - m_ellipsis_width :
#endif //SUPPORT_TEXT_DIRECTION
							text_x + word_width;
					}
					else
						skip_this_word = TRUE;
				}
			}
			else
			{
				/*
				No overflow detected but we're in a line that is supposed to show ellipsis
				update ellipsis coordinates to current word end - if next element in line (word or inline)
				makes ellipsis to overflow, these coordinates will be used to paint ellipsis
				*/
#ifdef SUPPORT_TEXT_DIRECTION
				if (container_props.direction == CSS_VALUE_rtl)
					m_ellipsis_point.x = text_x - m_ellipsis_width;
				else
#endif //SUPPORT_TEXT_DIRECTION
					m_ellipsis_point.x = text_x + (int)word_info.GetWidth();
			}
		}

		if (draw_text)
		{
			BOOL local_blink_on = g_opera->layout_module.m_blink_on;

			if (props.visibility == CSS_VALUE_visible)
			{
				highlight_has_text = TRUE;

				visual_device->SetContentFound(VisualDevice::DISPLAY_CONTENT_TEXT);

#ifdef LAYOUT_CARET_SUPPORT
				if (!local_blink_on && (props.text_decoration & TEXT_DECORATION_BLINK))
					if (document->GetCaretPainter() && document->GetCaret()->GetCaretContainer(html_element))
						local_blink_on = TRUE;
#endif // LAYOUT_CARET_SUPPORT

				if (minimum_color_contrast > 0 && props.text_bg_color != USE_DEFAULT_COLOR)
				{
					/* Paint background for text when in era mode (and minimum text contrast is used).
					   This is only needed for transparent floats and abs_pos boxes,
					   since they may be painted outside their origingal containing block. */

					RECT r;
					r.top = text_y;
					r.bottom = baseline + props.font_descent;
					r.left = x;
					r.right = x + word_width;
					long text_bg_col = HTM_Lex::GetColValByIndex(props.text_bg_color);
					visual_device->SetBgColor(text_bg_col);
					visual_device->DrawBgColor(r);
				}
			}

#ifdef SUPPORT_TEXT_DIRECTION
			if (!segment.left_to_right)
				text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif

			LayoutSelection highlight(NULL);

			if (props.visibility == CSS_VALUE_visible && box->GetSelected())
			{
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT

				if (m_has_more_hits)
				{
					OP_ASSERT(m_current_hit);
					LayoutSelection tmp_sel(m_current_hit->GetSelection());
					HTML_Element *selection_start_elem = tmp_sel.GetStartElement();
					const uni_char *selection_start = tmp_sel.GetStartWord();

					if (word > selection_start && selection_start_elem == html_element)
					{
						//search for selection element that is farther than (or starts at) currently painted word
						while (m_current_hit)
						{
							if (selection_start_elem != html_element)
							{
								/* We have left current text box and still not found anything interesting
								break now and continue later after entering proper text box */
								m_current_hit = NULL;
								m_has_more_hits = FALSE;
								break;
							}

							/* we have found selection element farther than currently painted word - it will be handled later
							when we'll paint proper word */
							if (tmp_sel.GetStartWord() > word)
								break;

							//selection fits word being painted - draw selection
							if (tmp_sel.GetStartWord() == word ||
								tmp_sel.GetStartWord() < word && (tmp_sel.GetEndWord() >= word || tmp_sel.GetEndElement() != html_element))
							{
								highlight = tmp_sel;
								break;
							}

							m_current_hit = (SelectionElm*) m_current_hit->Suc();

							if (!m_current_hit)
								break;

							tmp_sel = m_current_hit->GetSelection();
							selection_start_elem = tmp_sel.GetStartElement();
						}

						m_has_more_hits = (m_current_hit != NULL);
					}
					else if (word == selection_start || m_inside_current_hit)
						highlight = m_current_hit->GetSelection();
				}
				else
					highlight = text_selection;
#else // SEARCH_MATCHES_ALL
				highlight = text_selection;
#endif // SEARCH_MATCHES_ALL
			}

			if (highlight)
				HandleTextWithHighlight(layout_props, highlight, word, word_length, word_width, space_width, text_x, baseline, segment, text_format, word_info, props.font_color, word_width_shortened ? word_width : orig_word_width, local_blink_on, !skip_this_word);
			else
				// no highlight (which makes everything so simple)

				if (!skip_this_word && props.visibility == CSS_VALUE_visible && word_length && (!(props.text_decoration & TEXT_DECORATION_BLINK) || local_blink_on))
					PaintText(props, word, word_length, word_width, text_x, baseline, segment, text_format, word_info, props.font_color, word_width_shortened ? word_width : orig_word_width);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
			if (word_info.IsMisspelling())
				AddMisspelling(word_info, text_x, word_width, space_width, layout_props, segment);
#endif // INTERNAL_SPELLCHECK_SUPPORT
		}

#ifdef SUPPORT_TEXT_DIRECTION
		if (!segment.left_to_right)
			x -= space_width;
#endif

		if (decoration_start > x)
			decoration_start = x;

		if (!skip_this_word)
			decoration_width += word_width;

		if (overflow && !m_line_overflowed)
		{
			// trim/adjust decoration if needed
#ifdef SUPPORT_TEXT_DIRECTION
			if (!segment.left_to_right)
				decoration_start = LayoutCoord(m_ellipsis_point.x + m_ellipsis_width);
			else
#endif
				decoration_width -= decoration_start + decoration_width - LayoutCoord(m_ellipsis_point.x);

			m_line_overflowed = TRUE;
			PaintEllipsis(container_props, baseline);
		}
	}
}

/** Get start of selected text */

const uni_char*
LayoutSelection::GetStartText()
{
	const SelectionBoundaryPoint& point = m_selection->GetStartSelectionPoint();
	const uni_char* text = point.GetElement()->TextContent();
	if (!text)
		return NULL;
	return text + point.GetElementCharacterOffset();
}

/** Get end of selected text */

const uni_char*
LayoutSelection::GetEndText()
{
	const SelectionBoundaryPoint& point = m_selection->GetEndSelectionPoint();
	const uni_char* text = point.GetElement()->TextContent();
	if (!text)
		return NULL;
	return text + point.GetElementCharacterOffset();
}

/** Get start of word containing start of selected text */

const uni_char*
LayoutSelection::GetStartWord()
{
	if (!m_cached_start_word)
	{
		const SelectionBoundaryPoint& point = m_selection->GetStartSelectionPoint();
		SelectionPointWordInfo wi(point, FALSE);
		m_cached_start_word = wi.GetWord();
	}
	return m_cached_start_word;
}

/** Get start of word containing end of selected text */

const uni_char*
LayoutSelection::GetEndWord()
{
	if (!m_cached_end_word)
	{
		const SelectionBoundaryPoint& point = m_selection->GetEndSelectionPoint();
		SelectionPointWordInfo wi(point, FALSE);
		m_cached_end_word = wi.GetWord();
	}
	return m_cached_end_word;
}

void
PaintObject::HandleTextWithHighlight(LayoutProperties *layout_props, LayoutSelection highlight, const uni_char* word, int word_length, short word_width, short space_width, int x, LayoutCoord baseline, LineSegment& segment, int text_format, const WordInfo& word_info, COLORREF text_color, short orig_word_width, BOOL local_blink_on, BOOL draw_text)
{

	HTML_Element* html_element = layout_props->html_element;
	const HTMLayoutProperties& props = *layout_props->GetProps();

	BOOL has_word_after_sel = FALSE; // If there is a part of the word left after the selection
	BOOL selected_trailing_space;
	const uni_char* word_after_selection;
	const uni_char* word_end = word + word_length;
	const uni_char* word_selected = word;
	const uni_char* use_word = word;

	short next_unpainted_word_offset = 0; // start from the logical start of the word

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	BOOL hit_finished;

	do
	{
		hit_finished = FALSE;

#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

		/* Calculate offset coordinates and word pointers for the different
		   chunks (part of word before selection, selected part, part of
		   word after selection) */

		/* Offset in pixels from the start of the word (works the same regardless of
		   word direction, ltr or rtl). */

		short start_selection_word_offset = 0;
		short end_selection_word_offset = 0;

		selected_trailing_space = FALSE;
		word_after_selection = word_end;

		const SelectionBoundaryPoint* highlight_end = &highlight.GetEndSelectionPoint();

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
		/* In case we have overlapping multiple selection matches, we may have to
		   end this selection earlier, unless this is the active selection in
		   which case it should overlap later selections. */

		if (highlight.GetHighlightType() == TextSelection::HT_SEARCH_HIT &&
			m_current_hit)
		{
			// normal search hit (not active)
			if (SelectionElm* next = (SelectionElm*) m_current_hit->Suc())
			{
				const SelectionBoundaryPoint* next_start = &next->GetSelection()->GetStartSelectionPoint();

				/* Cheap and fairly reliable test.  Can't use Precedes() in general since
				   search hits aren't ordered exactly as the document tree. */

				if (next_start->GetElement() == highlight_end->GetElement() &&
					next_start->Precedes(*highlight_end))
				{
					highlight_end = next_start;
				}
			}
		}
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

		if (highlight.GetStartElement() == html_element)
		{
			const uni_char* start_text = highlight.GetStartText();

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
			m_inside_current_hit = TRUE;
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

			if (word > highlight.GetStartWord())
			{
				// Started in a previous word
				start_selection_word_offset = 0;
			}
			else
				if (word < highlight.GetStartWord())
				{
					// Has not started yet
					start_selection_word_offset = word_width;
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
					m_inside_current_hit = FALSE;
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
				}
				else
					if (start_text >= word)
						// We have "word [...] start_text"

						if (start_text <= word_end)
						{
							// We have "word [...] start_text [...] word_end"

							// Convert the logical position to a layout position (including virtual pos)
							start_selection_word_offset = visual_device->GetTxtExtentEx(word, start_text - word, text_format);
						}
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
						else
							m_inside_current_hit = FALSE;
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

			if (start_text > word)
				// We have "word ... start_text"

				if (start_text >= word_end)
				{
					// We have "word ... word_end [...] start_text"

					word_selected = word_end;

					if (word_info.HasTrailingWhitespace() &&
						highlight.GetStartWord() == word &&
						start_text >= word_end &&
						props.white_space != CSS_VALUE_pre_wrap)
					{
						selected_trailing_space = TRUE;

						/* Selection starts in the trailing whitespace of this word.
						   However not necessarily a the first whitespace (logically)
						   after this word. So we need to make sure we have the
						   correct start coordinate of the selected space. */
						start_selection_word_offset = visual_device->GetTxtExtentEx(word, word_length, text_format);
					}
				}
				else
					// we have "word ... start_text ... word_end"

					word_selected = start_text;
		}

		if (highlight_end->GetElement() == html_element)
		{
			SelectionPointWordInfo highlight_end_word_info(*highlight_end, FALSE);
			const uni_char* end_text = highlight_end_word_info.GetWord() + highlight_end_word_info.GetOffsetIntoWord();

			if (end_text >= word)
			{
				// We have "word [...] end_text"

				if (end_text <= word_end)
					// We have "word [...] end_text [...] word_end"

					end_selection_word_offset = visual_device->GetTxtExtentEx(word, end_text - word, text_format);
				else
					if (highlight_end_word_info.GetWord() >= word)
						// selected to the end of the word

						end_selection_word_offset = word_width;
					else
					{
						OP_ASSERT(!"Hmm, where does the selection really end? Must set end_selection_word_offset or we'll get paint errors");
					}
			}
			else
				// Selection ended in a previous word

				end_selection_word_offset = next_unpainted_word_offset;

			if (end_text <= word)
				word_after_selection = word;
			else
				if (end_text < word_end)
					word_after_selection = end_text;

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
			if (end_text <= word_end || (!word_length && highlight.GetEndWord() == word_end))
				hit_finished = TRUE;
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
		}
		else
			// Selection doesn't end in this element so just select the whole word

			end_selection_word_offset = word_width;


		/* Decide which of the three blocks
		   [before, selection, after] we have to paint. */

		if (!word_length && !highlight.IsEmpty())
		{
			SelectionPointWordInfo highlight_end_word_info(*highlight_end, FALSE);
			if ((highlight.GetStartElement() != html_element || highlight.GetStartWord() <= word) &&
				(highlight_end->GetElement() != html_element || highlight_end_word_info.GetWord() >= word))
				selected_trailing_space = TRUE;
		}

		BOOL has_word_before_sel = use_word < word_selected;
		BOOL has_word_with_sel = word_selected < word_after_selection;

		has_word_after_sel = word_after_selection < word_end;

		if (has_word_before_sel && start_selection_word_offset == end_selection_word_offset)
			// Empty selection, collapse into only has_word_before_sel

			has_word_after_sel = FALSE;


		// Calculate x coordinates for the left and right edge of the selection

		int left_selection_x;
		int right_selection_x;

		if (has_word_with_sel || selected_trailing_space)
		{
			SelectionPointWordInfo highlight_end_word_info(*highlight_end, FALSE);
#ifdef SUPPORT_TEXT_DIRECTION
			if (!segment.left_to_right)
			{
				int word_x = x - space_width;

				if (highlight_end_word_info.GetWord() == word)
					left_selection_x = word_x + word_width - end_selection_word_offset;
				else
					// the whole word

					left_selection_x = word_x;

				right_selection_x = word_x + word_width;

				if (has_word_before_sel)
				{
					OP_ASSERT(start_selection_word_offset >= next_unpainted_word_offset);
					right_selection_x -= start_selection_word_offset;
				}
				else
					right_selection_x -= next_unpainted_word_offset;
			}
			else
#endif // SUPPORT_TEXT_DIRECTION
			{
				if (has_word_before_sel)
					left_selection_x = x + start_selection_word_offset;
				else
					// From the start of the word (or rather from the place we stopped painting)

					left_selection_x = x + next_unpainted_word_offset;

				if (highlight_end_word_info.GetWord() == word)
					right_selection_x = x + end_selection_word_offset;
				else
					// To the end of the word

					right_selection_x = x + word_width;
			}

			OP_ASSERT(left_selection_x <= right_selection_x);
		}
		else
		{
			// Nothing selected
#ifdef SUPPORT_TEXT_DIRECTION
			if (!segment.left_to_right)
				left_selection_x = right_selection_x = x;
			else
#endif // SUPPORT_TEXT_DIRECTION
				left_selection_x = right_selection_x = x + word_width;
		}

		/* Paint the current selection and the word before it
		   using the calculcated coordinates, left_selection_x
		   and right_selection_x. */

		if (has_word_before_sel && props.font_color != CSS_COLOR_transparent && (!(props.text_decoration & TEXT_DECORATION_BLINK) || local_blink_on))
		{
			int text_left_edge = 0, text_width = 0;

#ifdef SUPPORT_TEXT_DIRECTION
			if (!segment.left_to_right)
			{
				/* RTL text needs to be offset since we in that case is going to paint
				   to the right in the "full text" box. */

				int right_text_edge_x = x - space_width + (word_width - next_unpainted_word_offset);

				text_left_edge = right_selection_x;
				text_width = right_text_edge_x - right_selection_x;
			}
			else
#endif // SUPPORT_TEXT_DIRECTION
			{
				text_left_edge = x + next_unpainted_word_offset;
				text_width = left_selection_x - x;
			}

			/* Paint the whole word, but the clipping will ensure that only
			   the part before the selection is visible */
			if (draw_text)
			{
				visual_device->BeginHorizontalClipping(text_left_edge, text_width);
				PaintText(props, word, word_length, word_width, x, baseline, segment, text_format, word_info, props.font_color, orig_word_width);
				visual_device->EndHorizontalClipping();
			}

			next_unpainted_word_offset = start_selection_word_offset;
		}

		if (selected_trailing_space || has_word_with_sel)
		{
			COLORREF sel_bgcol;
			COLORREF sel_fgcol;

			const ServerName *sn =
				reinterpret_cast<const ServerName *>(document->GetURL().GetAttribute(URL::KServerName, NULL));

			RECT selected_area;

			selected_area.left = left_selection_x;
			selected_area.right = right_selection_x;
			selected_area.top = baseline - props.font_ascent;
			selected_area.bottom = baseline + props.font_descent;

#if defined  SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
			int h_type = highlight.GetHighlightType();

			if (h_type == TextSelection::HT_ACTIVE_SEARCH_HIT)
			{
				// active search hit

				sel_bgcol = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED, sn);
				sel_fgcol = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED, sn);
			}
			else
				if (h_type == TextSelection::HT_SEARCH_HIT)
				{
					// search hit

					sel_bgcol = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS, sn);
					sel_fgcol = visual_device->GetColor();	// use original text color

					/* if the original text is not readable on the highlight
					   background, use a predefined color with more contrast */

					if (CheckColorContrast(sel_fgcol, sel_bgcol, 70) != sel_fgcol)
						sel_fgcol = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS, sn);
				}
				else
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
				{
					sel_bgcol = props.selection_bgcolor;
					sel_fgcol = props.selection_color;

					if (sel_fgcol == CSS_COLOR_transparent)
						sel_fgcol = props.font_color;

					// Don't use the system color for selection color if background color is set and vice versa.
					if (sel_fgcol == USE_DEFAULT_COLOR && sel_bgcol != USE_DEFAULT_COLOR)
						sel_fgcol = props.font_color;
					else if (sel_bgcol == USE_DEFAULT_COLOR && sel_fgcol != USE_DEFAULT_COLOR)
					{
						sel_bgcol = BackgroundColor(props);
						if (sel_bgcol == USE_DEFAULT_COLOR)
							sel_bgcol = COLORREF(CSS_COLOR_transparent);
					}

					if (sel_bgcol == USE_DEFAULT_COLOR)
						sel_bgcol = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED, sn);
					if (sel_fgcol == USE_DEFAULT_COLOR)
						sel_fgcol = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_TEXT_SELECTED, sn);
				}

			if (draw_text && sel_bgcol != CSS_COLOR_transparent)
			{
				visual_device->SetBgColor(sel_bgcol);
#if defined(SEARCH_MATCHES_ALL) && !defined(HAS_NO_SEARCHTEXT)
				visual_device->DrawTextBgHighlight(OpRect(&selected_area), sel_bgcol, (VD_TEXT_HIGHLIGHT_TYPE)h_type);
#else
				visual_device->DrawBgColor(selected_area);
#endif
			}

			if (draw_text && has_word_with_sel && (!(props.text_decoration & TEXT_DECORATION_BLINK) || local_blink_on))
			{
				/* Paint part of word that is selected. We paint the whole word but the clipping
				   will ensure that only the relevant parts are painted */

				long old_color = visual_device->GetColor();

				int text_left_edge = left_selection_x;
				int text_width = right_selection_x - left_selection_x;

				visual_device->SetColor(sel_fgcol);

				visual_device->BeginHorizontalClipping(text_left_edge, text_width);
				PaintText(props, word, word_length, word_width, x, baseline, segment, text_format, word_info, sel_fgcol, orig_word_width);
				visual_device->EndHorizontalClipping();

				visual_device->SetColor(old_color);
			}

			/* Move the "paint cursor" forward to after the selection. In the case
			   of painting a trailing space that offset is messy with respect to
			   ltr/rtl but it doesn't matter so just move it to SHRT_MAX */

			next_unpainted_word_offset = has_word_with_sel ? end_selection_word_offset : SHRT_MAX;
		}

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT

		SelectionPointWordInfo highlight_end_word_info(*highlight_end, FALSE);
		if (m_current_hit &&
			highlight_end_word_info.GetWord() == word &&
			(highlight_end_word_info.GetWord() + highlight_end_word_info.GetOffsetIntoWord() > word + word_length || selected_trailing_space))
		{

			/* This hit ended in the trailing whitespace for this word. All subsequent hits in
			   this word are just space hits (and in fact, if there are more hits in this word,
			   the search term contained only spaces).
			   Consume all following hits in this word, because they are collapsed spaces. */

			SelectionElm* next_hit = (SelectionElm*)m_current_hit->Suc();

			while (next_hit)
			{
				TextSelection* next_highlight_selection = next_hit->GetSelection();
				LayoutSelection next_highlight(next_highlight_selection);

				if (next_highlight_selection->GetStartElement() != html_element || next_highlight.GetStartWord() != word)
					break; // next hit is not in this word;

				/* Stop one position before a hit outside this word. Following statement will do the final advance. */
				m_current_hit = next_hit;
				highlight = next_highlight;

				next_hit = (SelectionElm*)next_hit->Suc();
			}
		}

		/* if has_word_after_sel is TRUE we haven't finished the whole word
		   and we should loop to look for more hits in the same word */

		if (hit_finished && m_current_hit)
		{
			m_inside_current_hit = FALSE;

			m_current_hit = (SelectionElm*) m_current_hit->Suc();

			m_has_more_hits = (m_current_hit != NULL);

			if (m_current_hit)
			{
				highlight = m_current_hit->GetSelection();

				if (highlight.GetStartElement() == html_element)
				{
					use_word = word_after_selection;
					word_selected = word_after_selection;

					// loop to mark more hits in this element

					continue;
				}
			}
		}

		// break out of the loop if no more hits in this element
		break;

		// the only way to loop is the continue in the block above.
	} while (highlight);
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

	if (has_word_after_sel && props.font_color != CSS_COLOR_transparent && (!(props.text_decoration & TEXT_DECORATION_BLINK) || local_blink_on))
	{
		// Paint part of word after selection
		int text_left_edge = 0, text_width = 0;

#ifdef SUPPORT_TEXT_DIRECTION
		if (!segment.left_to_right)
		{
			/* RTL text needs to be offset since we in that case is
			   going to paint to the right in the "full text" box. */

			text_left_edge = x - space_width;
			text_width = word_width - next_unpainted_word_offset;
		}
		else
#endif // SUPPORT_TEXT_DIRECTION
		{
			text_left_edge = x + next_unpainted_word_offset;
			text_width = word_width - next_unpainted_word_offset;
		}

		/* Paint the whole word, but the clipping will ensure that only
		   the part after the selection is visible. */
		if (draw_text)
		{
			visual_device->BeginHorizontalClipping(text_left_edge, text_width);
			PaintText(props, word, word_length, word_width, x, baseline, segment, text_format, word_info, props.font_color, orig_word_width);
			visual_device->EndHorizontalClipping();
		}
	}
}

/** Handle replaced content */

/* virtual */ void
PaintObject::HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content)
{
	OP_ASSERT(!target_element);

	if (layout_props->GetProps()->visibility == CSS_VALUE_visible && GetTraverseType() == TRAVERSE_CONTENT)
	{
		if (content->IsImage())
			GetVisualDevice()->SetContentFound(VisualDevice::DISPLAY_CONTENT_IMAGE);

		// If there is border-radius, use rounded clipping when painting the content.

		BOOL has_radius_clipping = FALSE;
		const HTMLayoutProperties& props = *layout_props->GetProps();

		if (props.HasBorderRadius() && !content->IsForm()) // Form content background follow the radius already (optimization)
		{
			int left = props.padding_left + props.border.left.width;
			int top = props.padding_top + props.border.top.width;

			int inner_height = content->GetHeight() - top - (props.padding_bottom + props.border.bottom.width);
			int inner_width = content->GetWidth() - left - (props.padding_right + props.border.right.width);

			OpRect border_box(0, 0, content->GetWidth(), content->GetHeight());
			OpRect clip_rect(left, top, inner_width, inner_height);
			Border used_border = props.border.GetUsedBorder(border_box);

			OP_STATUS status = visual_device->BeginRadiusClipping(border_box, clip_rect, &used_border);
			if (status == OpStatus::ERR_NO_MEMORY)
				SetOutOfMemory();

			has_radius_clipping = OpStatus::IsSuccess(status);
		}

		if (content->Paint(layout_props, GetDocument(), visual_device, area) == OpStatus::ERR_NO_MEMORY)
			SetOutOfMemory();

		if (has_radius_clipping)
			visual_device->EndRadiusClipping();
	}
}

/* virtual */ void
PaintObject::HandleBulletContent(LayoutProperties* layout_props, BulletContent* content)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (props.visibility == CSS_VALUE_visible)
	{
		// Paint the list marker bullet.

		// So far there is no situation where list marker can be entered during the target traversal.
		OP_ASSERT(!target_element);
		OP_ASSERT(layout_props->html_element->GetIsListMarkerPseudoElement());

		COLORREF font_color = props.font_color;

		if (minimum_color_contrast > 0 && props.current_bg_color != USE_DEFAULT_COLOR)
			font_color = CheckColorContrast(font_color, props.current_bg_color, minimum_color_contrast);

		visual_device->SetColor(font_color);

		if (OpStatus::IsMemoryError(content->PaintBullet(layout_props, document, visual_device)))
			SetOutOfMemory();
	}
}

/** Enter table content */

/* virtual */ BOOL
PaintObject::EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (GetTraverseType() == TRAVERSE_BACKGROUND)
	{
		BOOL clipped = FALSE;

		if (current_pane)
		{
			clipped = !IsPaneStarted();

			if (!clipped)
				clipped = layout_props->html_element->IsAncestorOf(current_pane->GetStopElement().GetHtmlElement());

			if (clipped)
			{
				/* Clip the background and borders of elements that might
				   start before or end after the current column / page. */

				OpRect clip_rect(pane_clip_rect);

				clip_rect.x -= translation_x;
				clip_rect.y -= translation_y;

				visual_device->BeginClipping(clip_rect);
			}
		}

		Border border = props.border;
		LayoutCoord table_width = content->GetWidth();

		visual_device->Translate(0, table_top);

		if (content->GetCollapseBorders())
		{
			BorderCollapsedCells* border_collapsed_cells;
			short top_border_width;
			short left_border_width;
			short right_border_width;
			short bottom_border_width;
			LayoutCoord collapsed_top_offset;
			LayoutCoord collapsed_left_offset;

			content->GetBorderWidths(props, top_border_width, left_border_width, right_border_width, bottom_border_width, FALSE);

			if (border.top.style == CSS_VALUE_ridge)
				border.top.style = CSS_VALUE_outset;
			else
				if (border.top.style == CSS_VALUE_groove)
					border.top.style = CSS_VALUE_inset;

			if (border.left.style == CSS_VALUE_ridge)
				border.left.style = CSS_VALUE_outset;
			else
				if (border.left.style == CSS_VALUE_groove)
					border.left.style = CSS_VALUE_inset;

			if (border.right.style == CSS_VALUE_ridge)
				border.right.style = CSS_VALUE_outset;
			else
				if (border.right.style == CSS_VALUE_groove)
					border.right.style = CSS_VALUE_inset;

			if (border.bottom.style == CSS_VALUE_ridge)
				border.bottom.style = CSS_VALUE_outset;
			else
				if (border.bottom.style == CSS_VALUE_groove)
					border.bottom.style = CSS_VALUE_inset;

			collapsed_top_offset = LayoutCoord(top_border_width - border.top.width / 2);
			collapsed_left_offset = LayoutCoord(left_border_width - border.left.width / 2);

			table_width -= collapsed_left_offset + LayoutCoord(right_border_width - LayoutCoord((border.right.width + 1) / 2));

			border_collapsed_cells = OP_NEW(BorderCollapsedCells, ());

			if (!border_collapsed_cells || !border_collapsed_cells->Init((TableCollapsingBorderContent*)content))
			{
				OP_DELETE(border_collapsed_cells);
				SetOutOfMemory();
				return FALSE;
			}

			border_collapsed_cells->Into(&border_collapsed_cells_list);

			if (props.visibility == CSS_VALUE_visible)
			{
				visual_device->Translate(collapsed_left_offset, collapsed_top_offset);

				BackgroundsAndBorders bb(GetDocument(), visual_device, layout_props, &props.border);
				const OpRect border_box(0, 0, table_width,
										table_height - collapsed_top_offset - bottom_border_width + (border.bottom.width + 1) / 2);

				bb.PaintBackgrounds(layout_props->html_element, props.bg_color, props.font_color,
									props.bg_images, &props.box_shadows, border_box);

				// Collapsed borders are painted soon, so backgrounds must be flushed.
				visual_device->FlushBackgrounds(OpRect(-left_border_width,
														-top_border_width,
														table_width + left_border_width + right_border_width,
														table_height + top_border_width + bottom_border_width));

				visual_device->Translate(-collapsed_left_offset, -collapsed_top_offset);
			}
		}
		else
			if (props.visibility == CSS_VALUE_visible)
			{
				BackgroundsAndBorders bb(GetDocument(), visual_device, layout_props, &border);
				const OpRect border_box(0, 0, table_width, table_height);
				bb.PaintBackgrounds(layout_props->html_element, props.bg_color, props.font_color,
									props.bg_images, &props.box_shadows, border_box);
				bb.PaintBorders(layout_props->html_element, border_box, props.font_color);
			}

		visual_device->Translate(0, -table_top);

		if (clipped)
			visual_device->EndClipping();
	}

	/* Keeping other browser engines compatibility (Gecko, WebKit)
	   - in case of tables only overflow_x matters and it controls also
	   the visiblity of y overflow. IE9 is slightly different - they
	   do not ignore overflow-y, but it also affects x overflow. */
	if (props.overflow_x == CSS_VALUE_hidden)
	{
		Content_Box* table_box = content->GetPlaceholder();
		if (table_box->GetClipAffectsTarget(target_element))
		{
			OpRect clip_rect = table_box->GetClippedRect(props, TRUE);

			traverse_info.old_table_content_clip_rect = table_content_clip_rect;
			traverse_info.has_clipped = TRUE;

			if (props.border_collapse == CSS_VALUE_collapse)
			{
				/* We need to postpone pushing the clip rect to the moment
				   when it doesn't affect painting nor flushing the collapsed
				   borders. So the pushing/popping will take place inside
				   EnterTableRow/LeaveTableRow. */
				table_content_clip_rect = clip_rect;
				table_content_clip_rect.OffsetBy(translation_x, translation_y);

				/* Store the old clip rect now. It will be restored in LeaveTableContent.
				   During the operations inside EnterTableRow/LeaveTableRow the
				   current_clip_rect may not match the VisualDevice state then.
				   So operations there must not depend on current_clip_rect. */
				traverse_info.old_clip_rect = current_clip_rect;
			}
			else
			{
				// Push clip rect and make sure it won't be pushed again in EnterTableRow.
				PushClipRect(clip_rect, traverse_info);
				table_content_clip_rect = OpRect(0, 0, INT32_MAX, 0);
			}
		}
	}

	return TRUE;
}

/** Leave table content. */

/* virtual */ void
PaintObject::LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info)
{
	if (content->GetCollapseBorders())
	{
		BorderCollapsedCells* border_collapsed_cells = (BorderCollapsedCells*) border_collapsed_cells_list.Last();

		if (border_collapsed_cells && border_collapsed_cells->GetTableContent() == content)
		{
			// Paint the border on the last row that we entered.

			border_collapsed_cells->FlushBorders(this);

			border_collapsed_cells->Out();
			OP_DELETE(border_collapsed_cells);
		}
	}

	if (traverse_info.has_clipped)
	{
		if (layout_props->GetProps()->border_collapse == CSS_VALUE_collapse)
			// No need for popping (as it was done during leaving rows) - just restore the old clip rect.
			current_clip_rect = traverse_info.old_clip_rect;
		else
			PopClipRect(traverse_info);
		table_content_clip_rect = traverse_info.old_table_content_clip_rect;
	}
}

/** Handle marquee. */

/* virtual */ OP_STATUS
PaintObject::HandleMarquee(MarqueeContainer* marquee, LayoutProperties* layout_props)
{
	return marquee->Animate(this, layout_props);
}

/** Handle empty table cells. */

/* virtual */ void
PaintObject::HandleEmptyCells(const TableRowBox* row, TableContent* table, LayoutProperties* table_lprops)
{
	// Drop border on nonexistant cells in collapsing border model, 'frames' and 'rules'

	// FIXME: We should probably paint column-group/column/row-group/row background as well.

	if (!target_element &&
		!table->GetCollapseBorders() &&
		!table->NeedSpecialBorders() &&
		table_lprops->GetProps()->empty_cells == CSS_VALUE_show && !table_lprops->GetProps()->GetSkipEmptyCellsBorder())
	{
		int column = 0;
		TableCellBox* cell = row->GetLastCell();

		if (cell)
			column = cell->GetColumn() + cell->GetCellColSpan();

		if (column < table->GetLastColumn())
		{
			// Has nonexistant cells

			HTML_Element* table_element = table_lprops->html_element;

			if (table_element->HasNumAttr(Markup::HA_BORDER))
				if ((short)(INTPTR) table_element->GetAttr(Markup::HA_BORDER, ITEM_TYPE_NUM, (void*) 0))
				{
					// Table has border, so draw border on nonexistant cells also

					LayoutCoord cell_spacing = LayoutCoord(table_lprops->GetProps()->border_spacing_horizontal);
					Border border;
					LayoutCoord cell_x = cell_spacing;

					if (cell)
						cell_x = cell->GetStaticPositionedX() + cell->GetWidth();

					border.Reset();

					border.top.width = 1;
					border.left.width = 1;
					border.right.width = 1;
					border.bottom.width = 1;
					border.top.style = CSS_VALUE_inset;
					border.left.style = CSS_VALUE_inset;
					border.right.style = CSS_VALUE_inset;
					border.bottom.style = CSS_VALUE_inset;
					border.top.color = DEFAULT_BORDER_COLOR;
					border.left.color = DEFAULT_BORDER_COLOR;
					border.right.color = DEFAULT_BORDER_COLOR;
					border.bottom.color = DEFAULT_BORDER_COLOR;

					long cell_height = row->GetHeight();
					TableRowBox* prev_row = (TableRowBox*) row->Pred();

					HTML_Element* html_element = row->GetHtmlElement();

					LayoutProperties* layout_props = table_lprops->GetChildProperties(document->GetHLDocProfile(), html_element);

					if (layout_props)
						for (; column < table->GetLastColumn(); column++)
						{
							TableCellBox* spanning_cell = prev_row ? TableRowBox::GetRowSpannedCell(prev_row, column, 2, table->IsWrapped()) : NULL;
							LayoutCoord cell_width;

							cell_x += cell_spacing;

							if (spanning_cell)
							{
								cell_width = spanning_cell->GetWidth();
								column += spanning_cell->GetCellColSpan() - 1;
							}
							else
							{
								cell_width = table->GetCellWidth(column, 1, FALSE);

								Translate(cell_x, LayoutCoord(0));

								BackgroundsAndBorders bb(GetDocument(), visual_device, layout_props, &border);
								OpRect border_box(0, 0, cell_width, cell_height);
								bb.PaintBorders(html_element, border_box, layout_props->GetProps()->font_color);

								Translate(-cell_x, LayoutCoord(0));
							}

							cell_x += cell_width;
						}
					else
						SetOutOfMemory();
				}
		}
	}
}

/** Begin clipping of a partially collapsed table cell. */

/* virtual */ void
PaintObject::BeginCollapsedTableCellClipping(TableCellBox* box, const OpRect& clip_rect, TraverseInfo& traverse_info)
{
	visual_device->BeginClipping(clip_rect);
}

/** End clipping of a partially collapsed table cell. */

/* virtual */ void
PaintObject::EndCollapsedTableCellClipping(TableCellBox* box, TraverseInfo& traverse_info)
{
	visual_device->EndClipping();
}

/** Applies clipping */

/* virtual */ void
PaintObject::PushClipRect(const OpRect& clip_rect, TraverseInfo& info)
{
	info.old_clip_rect = current_clip_rect;
	current_clip_rect = visual_device->ToBBox(clip_rect);

	visual_device->BeginClipping(clip_rect);

	info.has_clipped = TRUE;
}

/** Removes clipping */

/* virtual */ void
PaintObject::PopClipRect(TraverseInfo& info)
{
	if (info.has_clipped)
	{
		current_clip_rect = info.old_clip_rect;
		visual_device->EndClipping();
		info.has_clipped = FALSE;
	}
}

BOOL
IntersectionObject::GetRelativeToBox(Box* box, LayoutCoord& out_relative_x, LayoutCoord& out_relative_y)
{
	OpPoint* point;
	if (OpStatus::IsSuccess(box_relative_hash.GetData(box, &point)))
	{
		out_relative_x = LayoutCoord(point->x);
		out_relative_y = LayoutCoord(point->y);
		return TRUE;
	}
	return FALSE;
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** @return TRUE if traversal of dirty trees is allowed. */

/* virtual */ BOOL
IntersectionObject::AllowDirtyTraverse()
{
	// FIXME: Should check with LayoutWorkplace::needs_another_reflow_pass before returning TRUE.

	return !document->IsParsed() || !document->InlineImagesLoaded();
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

/** Translate x and y coordinates */

/* virtual */ void
IntersectionObject::Translate(LayoutCoord x, LayoutCoord y)
{
	HitTestingTraversalObject::Translate(x, y);
	translated_x -= x;
	translated_y -= y;
}

BOOL
IntersectionObject::CheckIntersection(LayoutCoord x, LayoutCoord y,
									  LayoutProperties*& layout_props,
									  VerticalBox* box
#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
									  , BOOL real_intersection
#endif // LAYOUT_INTERSECT_CONTAINER_INFO
									  )
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	OP_ASSERT(inner_box);

	if (!target_element && GetTraverseType() == TRAVERSE_BACKGROUND)
	{
		BOOL is_inside;

		if (TouchesCurrentPaneBoundaries(layout_props))
		{
			// This element may live in multiple columns / pages.

			OpRect clip_rect(pane_clip_rect);

			clip_rect.x -= translation_x;
			clip_rect.y -= translation_y;

			is_inside = clip_rect.Contains(OpPoint(x, y));
		}
		else
			is_inside = x >= 0 && x < box->GetWidth() && y >= 0 && y < box->GetHeight();

        if (is_inside)
		{
			// Enclosing, visible links should hit even though this very box is hidden (bugs DSK-133725 & DSK-169152)
			if (props.visibility != CSS_VALUE_visible)
			{
				HTML_Element* enclosing_link = layout_props->html_element->GetA_Tag();

				if (enclosing_link && enclosing_link != layout_props->html_element)
				{
					LayoutProperties* iter = layout_props;

					while (iter && iter->html_element != enclosing_link)
					{
						Box* iter_box = iter->html_element->GetLayoutBox();
						// ... but only if they're in normal flow of the enclosing link (fix for bug CORE-3473).
						if (iter_box && iter_box->IsBlockBox() && !((BlockBox*)iter_box)->IsInStack())
							return FALSE;
						iter = iter->Pred();
					}

					if (iter && iter->GetProps()->visibility != CSS_VALUE_visible)
						return FALSE;
				}
				else
					// 'visibility: hidden' and no enclosing links
					return FALSE;
			}

			BOOL hit = TRUE;
			if (box->IsContentReplaced())
				content_found = TRUE;

			// Don't hit if we are outside this elements clip box
			if (box->IsAbsolutePositionedBox() &&
				(props.clip_left != CLIP_NOT_SET ||
				 props.clip_right != CLIP_NOT_SET ||
				 props.clip_top != CLIP_NOT_SET ||
				 props.clip_bottom != CLIP_NOT_SET))
			{
				AbsoluteBoundingBox bounding_box;

				box->GetClippedBox(bounding_box, props, FALSE);

				if (!bounding_box.Intersects(LayoutCoord(x), LayoutCoord(y)))
					hit = FALSE;
			}

#ifdef SVG_SUPPORT
			if (hit && content_found)
				if (SVGContent *svg_content = box->GetSVGContent())
					if (svg_content->IsTransparentAt(document, layout_props, orig_x, orig_y))
					{
						hit = FALSE;
						content_found = FALSE;
					}
#endif

#ifdef LAYOUT_CARET_SUPPORT
			if (document->GetCaretPainter())
				hit = TRUE;
#endif // LAYOUT_CARET_SUPPORT

			if (hit)
			{
#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
				if (real_intersection)
				{
					OpRect local_rect(0, 0, box->GetWidth(), box->GetHeight());
					container_rect = visual_device_transform.ToBBox(local_rect);

					line_rect.width = 0;
					line_rect.height = 0;
				}
#endif // LAYOUT_INTERSECT_CONTAINER_INFO

				return TRUE;
			}
		}
	}

	return FALSE;
}

/* virtual */ BOOL
IntersectionObject::HandleScrollable(const HTMLayoutProperties& props,
									 ScrollableArea* scrollable,
									 LayoutCoord width,
									 LayoutCoord height,
									 TraverseInfo& traverse_info,
									 BOOL clip_affects_target,
									 int scrollbar_modifier)
{
#ifdef GADGET_SUPPORT
	if (clip_affects_target && IsCentralPointInClipRect() &&
		GetDocument()->GetWindow()->GetType() == WIN_TYPE_GADGET)
	{
		Box* box = scrollable->GetOwningBox();
		int scrollbar_left = props.border.left.width + width;
		int scrollbar_right = box->GetWidth() - props.border.right.width;

		if (scrollable->LeftHandScrollbar())
		{
			scrollbar_left -= width;
			scrollbar_right -= width;
		}

		if (translated_y > props.border.top.width + height &&
			translated_y < box->GetHeight() - props.border.bottom.width &&
			translated_x > scrollbar_left &&
			translated_x < scrollbar_right)
			control_region_found = TRUE;
	}
#endif // GADGET_SUPPORT

	return TRUE;
}

void
IntersectionObject::RecordHit(Box* box, int box_relative_x, long box_relative_y)
{
	OP_ASSERT(box);

	inner_box = box;
	relative_x = LayoutCoord(box_relative_x);
	relative_y = LayoutCoord(box_relative_y);

	OpPoint* relative_point = OP_NEW(OpPoint, (box_relative_x, box_relative_y));
	if (relative_point)
	{
		if (!OpStatus::IsSuccess(box_relative_hash.Add(inner_box, relative_point)))
		{
			OP_DELETE(relative_point);

			/* Intentionally ignoring the memory problem since this is a non-critical part of the operation. */
		}
	}
}

/*virtual*/ BOOL
IntersectionObject::HandleVerticalBox(LayoutProperties* parent_lprops,
									  LayoutProperties*& layout_props,
									  VerticalBox* box,
									  TraverseInfo& traverse_info,
									  BOOL clip_affects_traversed_descendants)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (!inner_box ||
		layout_props->html_element->Type() != Markup::HTE_HTML &&
		IsCentralPointInClipRect() &&
		CheckIntersection(translated_x, translated_y, layout_props, box))
	{
		RecordHit(box,
		          translated_x - props.border.left.width - props.padding_left,
		          translated_y - props.border.top.width - props.padding_top);
	}

	if (box->GetButtonContainer())
		return FALSE;

#ifdef NEARBY_ELEMENT_DETECTION
	cur_box_form_eoi = NULL;

	if (eoi_list)
	{
		if (!target_element)
		{
			RECT box_area;

			box_area.left = 0;
			box_area.top = 0;
			box_area.right = box->GetWidth();
			box_area.bottom = box->GetHeight();

			if (!CheckAnchorCandidate(layout_props, box_area))
			{
				SetOutOfMemory();
				return FALSE;
			}
		}
	}
#endif // NEARBY_ELEMENT_DETECTION

	return TRUE;
}

/** Callback for objects needing to run code after traverse has finished */

/* virtual */ void
IntersectionObject::TraverseFinished()
{
#ifdef NEARBY_ELEMENT_DETECTION
	if (eoi_list)
	{
		// Remove all anchors that didn't have any boxes within the specified radius.

		unsigned int max_distance = eoi_radius * eoi_radius;

		while (ElementOfInterest* cand = (ElementOfInterest*) eoi_list->Last())
		{
			if (cand->GetDistance() < max_distance)
				break;

			cand->Out();
			OP_DELETE(cand);
		}
	}
#endif // NEARBY_ELEMENT_DETECTION

	HTML_Element* inner_element = inner_box ? inner_box->GetHtmlElement() : NULL;

	if (inner_element && inner_element->Type() == Markup::HTE_DOC_ROOT)
	{
		/* Unless this is over the page control widget, report that it was the
		   logical document root (for HTML documents, the HTML element) that
		   was hit. */

#ifdef PAGED_MEDIA_SUPPORT
		BOOL over_page_control = FALSE;

		if (PagedContainer* root_paged_container = inner_box->GetContainer()->GetPagedContainer())
		{
			/* Need to convert to content-box (@page margins are stored as
			   padding on the root). We could have done this back when we
			   actually had the cascade (a few nanoseconds ago), but this is
			   cheap enough anyway, and causes less code complexity. */

			AutoDeleteHead prop_list;
			LayoutProperties* lprops = LayoutProperties::CreateCascade(inner_element, prop_list, document->GetHLDocProfile());
			const HTMLayoutProperties& props = *lprops->GetProps();
			LayoutCoord x = orig_x - props.padding_left;
			LayoutCoord y = orig_y - props.padding_top;

			OP_ASSERT(!props.border.left.width && !props.border.top.width);
			over_page_control = root_paged_container->IsOverPageControl(x, y);
		}

		if (!over_page_control)
#endif // PAGED_MEDIA_SUPPORT
			if (HTML_Element* logical_root = document->GetLogicalDocument()->GetDocRoot())
				inner_box = logical_root->GetLayoutBox();
	}
}


#ifdef GADGET_SUPPORT
/*virtual*/BOOL
IntersectionObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (HitTestingTraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
	{
		if (!traverse_info.dry_run)
		{
			OP_ASSERT(layout_props);
			if (IsCentralPointInClipRect())
				CheckControlRegion(layout_props, box, NULL);
		}

		return TRUE;
	}

	return FALSE;
}
#endif // GADGET_SUPPORT

#if defined(LAYOUT_INTERSECT_CONTAINER_INFO) || defined NEARBY_ELEMENT_DETECTION

/** Leave vertical box */

/* virtual */ void
IntersectionObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
	if (grab_container_info)
	{
		OP_ASSERT(box->GetContainer());

		grab_container_info = FALSE;
		container_rect = visual_device_transform.ToBBox(OpRect(0, 0, box->GetWidth(), box->GetHeight()));

		switch (layout_props->GetProps()->text_align)
		{
		case CSS_VALUE_left:
			text_align = OpViewportRequestListener::VIEWPORT_ALIGN_LEFT;
			break;
		case CSS_VALUE_center:
			text_align = OpViewportRequestListener::VIEWPORT_ALIGN_HCENTER;
			break;
		case CSS_VALUE_right:
			text_align = OpViewportRequestListener::VIEWPORT_ALIGN_RIGHT;
			break;
		case CSS_VALUE_justify:
			text_align = OpViewportRequestListener::VIEWPORT_ALIGN_JUSTIFY;
			break;
		default:
			OP_ASSERT(!"Unhandled text align value");
		}
	}
#endif // LAYOUT_INTERSECT_CONTAINER_INFO

#ifdef NEARBY_ELEMENT_DETECTION
	if (cur_anchor_eoi && cur_anchor_eoi->GetHtmlElement() == layout_props->html_element)
	{
		cur_anchor_eoi = NULL;
		inside_cur_anchor_eoi = FALSE;
	}

	cur_box_form_eoi = NULL;
#endif // NEARBY_ELEMENT_DETECTION

	HitTestingTraversalObject::LeaveVerticalBox(layout_props, box, traverse_info);
}

/** Leave line. */

/* virtual */ void
IntersectionObject::LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
	if (grab_container_info)
	{
		OpRect local_line_rect;

		/* Is this correct? There is no handling of line overflow or rtl text. */
		switch (parent_lprops->GetProps()->text_align)
		{
		case CSS_VALUE_center:
			local_line_rect.x -= (line->GetWidth() - line->GetUsedSpace()) / 2;
			break;
		case CSS_VALUE_right:
			local_line_rect.x -= line->GetWidth() - line->GetUsedSpace();
			break;
		}

		local_line_rect.width = line->GetWidth();
		local_line_rect.height = line->GetLayoutHeight();

		line_rect = visual_device_transform.ToBBox(local_line_rect);
	}
#endif // LAYOUT_INTERSECT_CONTAINER_INFO

#ifdef NEARBY_ELEMENT_DETECTION
	cur_box_form_eoi = NULL;
#endif // NEARBY_ELEMENT_DETECTION

	HitTestingTraversalObject::LeaveLine(parent_lprops, line, containing_element, traverse_info);
}

#endif // LAYOUT_INTERSECT_CONTAINER_INFO || NEARBY_ELEMENT_DETECTION

/** Enter inline box */

/* virtual */ BOOL
IntersectionObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (HitTestingTraversalObject::EnterInlineBox(layout_props, box, box_area, segment, start_of_box, end_of_box, baseline, traverse_info))
	{
		BOOL ret = TRUE;
		const HTMLayoutProperties& props = *layout_props->GetProps();

		if (!target_element &&
			props.visibility == CSS_VALUE_visible)
		{
			if (IsCentralPointInClipRect())
			{
				if (layout_props->html_element->GetIsListMarkerPseudoElement() && props.list_style_pos == CSS_VALUE_outside)
				{
					/* There is a special treatment in case of a list marker element
					   and the marker with position outside. We want to hit it also
					   in case of clicking in the gap between the marker
					   and the begin of the actual content. Hitting the marker will ensure
					   hitting the list item element as it is the first non layout inserted ancestor. */
					OpRect rect(&box_area);
					int inner_offset = document->GetHandheldEnabled() ? MARKER_INNER_OFFSET_HANDHELD : MARKER_INNER_OFFSET;
#ifdef SUPPORT_TEXT_DIRECTION
					if (!segment.left_to_right)
						rect.x -= inner_offset;
#endif // SUPPORT_TEXT_DIRECTION
					rect.width += inner_offset;

					if (rect.Contains(OpPoint(translated_x, translated_y)))
						RecordHit(box, translated_x - rect.x, translated_y - rect.y);

					return TRUE;
				}
				else if (box_area.left <= translated_x &&
					box_area.right >= translated_x &&
					box_area.top <= translated_y &&
					box_area.bottom >= translated_y)
				{
					if (box->IsContentReplaced())
						content_found = TRUE;

					RecordHit(box,
							  translated_x - box_area.left - props.border.left.width - props.padding_left,
							  translated_y - box_area.top - props.border.top.width - props.padding_top);

#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
					grab_container_info = TRUE;
#endif // LAYOUT_INTERSECT_CONTAINER_INFO

					if (box->GetButtonContainer())
						ret = FALSE;
				}
			}

#ifdef NEARBY_ELEMENT_DETECTION
			if (eoi_list)
				if (!CheckAnchorCandidate(layout_props, box_area))
				{
					SetOutOfMemory();
					return FALSE;
				}
#endif // NEARBY_ELEMENT_DETECTION
		}

#ifdef GADGET_SUPPORT
		if (IsCentralPointInClipRect())
			CheckControlRegion(layout_props, box, &box_area);
#endif // GADGET_SUPPORT

		return ret;
	}
	else
		return FALSE;
}

#ifdef NEARBY_ELEMENT_DETECTION

/** Leave inline box */

/* virtual */ void
IntersectionObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	if (cur_anchor_eoi && cur_anchor_eoi->GetHtmlElement() == layout_props->html_element)
		inside_cur_anchor_eoi = FALSE;

	HitTestingTraversalObject::LeaveInlineBox(layout_props, box, box_area, start_of_box, end_of_box, traverse_info);
}

#endif // NEARBY_ELEMENT_DETECTION

#ifdef GADGET_SUPPORT

/** See if this element has a control region, and if mouse is inside this controle region.
	If we are inside, this->control_region_found is set to true. if control_region_found is
	already set, nothing more is done.

	Method handles:
		Control elements like buttons, text-areas, inputs
		CSS control_regions
		Scrollbars in overflowed elements*/

void
IntersectionObject::CheckControlRegion(LayoutProperties* layout_props, Box* box, const RECT* box_area)
{
	/* If we already found a control_region there is no need to check for more. */
	if (control_region_found)
		return;

	/* If this isn't a gadget window, we exit */
	if (GetDocument()->GetWindow()->GetType() != WIN_TYPE_GADGET)
		return;

	// Translated x/y equals mouse position relative to the element we are inside.
	INT32 mouse_x = translated_x;
	INT32 mouse_y = translated_y;

	// Does the element have scrollbars?

	if (ScrollableArea* scrollable = box->GetScrollable())
		if (scrollable->IsOverScrollbar(OpPoint(orig_x, orig_y)))
		{
			control_region_found = TRUE;
			return;
		}

	/* Is the whole element a control region (button or similar)? */
	BOOL whole_is_a_control = FALSE;

	if (layout_props->GetProps()->control_region_cp)
	{
		/* We have a control region css element */
		whole_is_a_control = TRUE;
	}
	else
		switch (layout_props->html_element->Type())
		{
			case Markup::HTE_BUTTON:
			case Markup::HTE_SELECT:
#ifdef _SSL_SUPPORT_
			case Markup::HTE_KEYGEN:
#endif
			case Markup::HTE_TEXTAREA:
			case Markup::HTE_INPUT:
				/* The element is of control type */
				whole_is_a_control = TRUE;
				break;
			default:
				break;
		}

	if (whole_is_a_control)
	{
		/* Find area from box */
		OpRect area;
		if (box_area != 0){
			area.x = box_area->left;
			area.y = box_area->top;
			area.width = (box_area->right - box_area->left);
			area.height = (box_area->bottom - box_area->top);
		} else {
			area.x = 0;
			area.y = 0;
			area.width = box->GetWidth();
			area.height = box->GetHeight();
		}

		/* We default to a full-sized rectangle. */
		CSSValue shape      = CSS_VALUE_rectangle;

		/* Did we have a css controle area */
		if (layout_props->GetProps()->control_region_cp)
		{
			/* Adjust area and shape */
			const CSS_generic_value* gen_arr = layout_props->GetProps()->control_region_cp->GenArrayValue();

			shape = (CSSValue)gen_arr[0].value.type;

			INT32 offset_top    = (INT32)gen_arr[1].value.real;
			INT32 offset_right  = (INT32)gen_arr[2].value.real;
			INT32 offset_bottom = (INT32)gen_arr[3].value.real;
			INT32 offset_left   = (INT32)gen_arr[4].value.real;

			area.x += offset_left;
			area.y += offset_top;
			area.width -= (offset_left + offset_right);
			area.height -= (offset_top + offset_bottom);
		}

		/* Finally, lets check if we are inside! */
		if (shape == CSS_VALUE_rectangle)
		{
			if (area.Intersecting(OpRect(mouse_x, mouse_y, 1, 1)))
			{
				control_region_found = true;
				return;
			}
		}
		else /* shape == CSS_VALUE_circle */
		{
			INT32 interior_w = area.width;
			INT32 interior_h = area.height;
			INT32 diameter   = MIN(interior_w, interior_h);

			/*	We need to find out if (mouse_x, mouse_y) is within
				the largest circle contained within the offset region
				(and centered therein).
				First, let's translate the mouse point to the offset
				region: */
			mouse_x -= area.x;
			mouse_y -= area.y;

			/*	We want to translate to the origin of the circle,
				double the units (so we can deal with values
				in the center of pixels with ints), and compute
				the distance from the center of the circle to
				the center of the pixel the mouse is on.

				First, we double the units and move the mouse
				to the center of the pixel: */
			mouse_x *= 2;  mouse_x++;
			mouse_y *= 2;  mouse_y++;

			/*	With the units doubled, the old diameter in the new
				radius.  Similarly, interior_w and interior_h are now
				offsets to the center of the circle.
				So let's rename the diameter to reflect this, and
				translate the mouse point to be relative to the center
				of the circle. */
			INT32 radius = diameter;

			mouse_x -= interior_w;
			mouse_y -= interior_h;

			/*  Now we just check that the distance to the origin is
				less than the radius.  What could be simpler?  :) */
			if (mouse_x*mouse_x + mouse_y*mouse_y <= radius*radius)
			{
				control_region_found = TRUE;
				/*	Now we return since we found what we were looking for.
					Currently no more work is done so this isnt needed, but
					in case we later add more checks it is logically right
					not to do anything more after we found the control region */
				return;
			}
		}
	}
}

#endif // GADGET_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION

/** Enable element of interest (nearby anchor and form elements) detection.

	Set the radius within which to search for elements of interest, and a list in which to put them.

	@param eoi_list Where to put elements of interest found.
	@param radius Radius in pixels. */

void
IntersectionObject::EnableEoiDetection(Head* eoi_list, unsigned int radius)
{
	this->eoi_list = eoi_list;
	eoi_radius = radius;

	/* Add some extra space on top and bottom, which will ensure that we don't
	   miss parts of the text content in anchors (at least in most cases, at
	   least). */

	area.top = translated_y - ((int)radius + 100);
	area.right = translated_x + (int)radius + 1;
	area.bottom = translated_y + ((int)radius + 100) + 1;
	area.left = translated_x - (int)radius;
}

/** Check if a link should be added to the list of candidates, and, if it should, add it. */

BOOL
IntersectionObject::CheckAnchorCandidate(LayoutProperties* layout_props, const RECT& box_area)
{
	HTML_Element* html_element = layout_props->html_element;

	if (html_element->GetAnchor_HRef(document))
	{
		VisualDevice* vis_dev = document->GetVisualDevice();

		OP_ASSERT(!inside_cur_anchor_eoi);

		cur_box_form_eoi = NULL;

		if (cur_anchor_eoi && cur_anchor_eoi->GetHtmlElement() == html_element)
		{
			// Entered same anchor on next line.

			inside_cur_anchor_eoi = TRUE;

			return TRUE;
		}

		cur_text_anchor_fragment = NULL;
		eoi_first_text_anchor_fragment_x = 0;
		cur_anchor_eoi = NULL;

		for (ElementOfInterest* eoi = (ElementOfInterest*) eoi_list->First(); eoi; eoi = (ElementOfInterest*) eoi->Suc())
		{
			HTML_Element *element = html_element;

#ifdef _WML_SUPPORT_
			if (element->GetNsType() == NS_WML && (WML_ElementType)element->Type() == WE_GO)
			{
				/* In the case of wml's <go> we want the element that has the actual
				   text we want to show (if available), e.g. <anchor>, <do>, etc. */
				while (element && element != eoi->GetHtmlElement())
					element = element->ParentActual();
			}
#endif // _WML_SUPPORT_

			if (eoi->GetHtmlElement() == element)
			{
				// Has entered anchor before (in background traversal pass)

				OP_ASSERT(!element->GetLayoutBox()->IsInlineContent());
				OP_ASSERT(GetTraverseType() == TRAVERSE_CONTENT);
				OP_ASSERT(eoi->GetOriginalRect().IsEmpty());

				cur_anchor_eoi = static_cast<AnchorElementOfInterest*>(eoi);
				inside_cur_anchor_eoi = TRUE;

				return TRUE;
			}
		}

		// Entered new anchor.

		const HTMLayoutProperties& props = *layout_props->GetProps();
		COLORREF background_color;

		if (props.current_bg_color != USE_DEFAULT_COLOR)
			background_color = props.current_bg_color;
		else
		{
			/* Search the cascade for a suitable background color, or background image whose
			   average color to use as background color. Couldn't use props.current_bg_color
			   because that value is meant for color contrasting, and no color contrasting is done
			   if the active background is an image. Walking the cascade like this may be
			   expensive. Consider storing this information in props instead. */

			background_color = USE_DEFAULT_COLOR;

			LayoutProperties *bgcascade;
			CSSLengthResolver dummy(vis_dev);

			for (bgcascade = layout_props;
				 bgcascade && bgcascade->html_element;
				 bgcascade = bgcascade->Pred())
			{
				const HTMLayoutProperties& bgprops = *bgcascade->GetProps();

				if (HasBackgroundImage(bgprops))
				{
					 BackgroundProperties background;
					 BOOL repeated;

					 /* Dummy CSSLengthResolver reference, because we don't
						use background's sizes nor positions afterwards. */
					 bgprops.bg_images.GetLayer(dummy, 0, background);
					 repeated = background.bg_repeat_x == CSS_VALUE_repeat && background.bg_repeat_y == CSS_VALUE_repeat;

					 CssURL bg_cssurl = background.bg_img;

					 /* Skip unrepeated background images. This may be problematic,
						but is believed to cause less problems than the other way
						around. */

					 if (bg_cssurl && repeated)
					 {
						 URL bg_url = g_url_api->GetURL(*document->GetHLDocProfile()->BaseURL(), bg_cssurl);

						 if (!bg_url.IsEmpty())
						 {
							 // Get average color of background image.

							 Image img = UrlImageContentProvider::GetImageFromUrl(bg_url);
							 HEListElm* helm = bgcascade->html_element->GetHEListElm(TRUE);
							 UINT32 width = img.Width();
							 UINT32 height = img.Height();

							 if (height > img.GetLastDecodedLine())
								 height = img.GetLastDecodedLine();

							 if (helm && width && height)
							 {
								 background_color = img.GetAverageColor(helm);
								 break;
							 }
						 }
					 }
				}

				background_color = BackgroundColor(bgprops);

				if (background_color != USE_DEFAULT_COLOR)
					break;
			}

			if (background_color == USE_DEFAULT_COLOR)
				background_color = colorManager->GetBackgroundColor();
		}

		AnchorElementOfInterest* anchor_eoi = OP_NEW(AnchorElementOfInterest, (html_element));

		if (!anchor_eoi)
			return FALSE;

		FontAtt font;
		font = vis_dev->GetFontAtt();
		font.SetSize(vis_dev->ScaleToScreen(font.GetSize()));
		anchor_eoi->SetFont(font);
		anchor_eoi->SetTextColor(props.font_color);
		anchor_eoi->SetBackgroundColor(background_color);

#ifdef SUPPORT_TEXT_DIRECTION
		anchor_eoi->SetRTL(props.direction == CSS_VALUE_rtl);
#endif // SUPPORT_TEXT_DIRECTION

		/* Initially insert candidate at end of list; it will be moved to
		   earlier positions as fragments (text and images) closer to the
		   origin point are discovered. */

		anchor_eoi->Into(eoi_list);

		cur_anchor_eoi = anchor_eoi;
		inside_cur_anchor_eoi = TRUE;
	}
	else if (html_element->Type() == Markup::HTE_BUTTON)
	{
		cur_box_form_eoi = NULL;
		cur_text_anchor_fragment = NULL;

		OpRect local_rect(box_area.left, box_area.top,
						  box_area.right - box_area.left,
						  box_area.bottom - box_area.top);

		OpRect real_rect = visual_device_transform.ToBBox(local_rect);

		FormElementOfInterest* form_eoi = FormElementOfInterest::Create(html_element);

		if (!form_eoi)
		{
			SetOutOfMemory();
			return FALSE;
		}

		const HTMLayoutProperties& props = *layout_props->GetProps();

		form_eoi->Into(eoi_list);

#ifdef SUPPORT_TEXT_DIRECTION
		form_eoi->SetRTL(props.direction == CSS_VALUE_rtl);
#endif // SUPPORT_TEXT_DIRECTION

		OpRect fragment_rect = form_eoi->GetOriginalRect();
		ExpandEoiRegion(form_eoi, fragment_rect, real_rect);
		form_eoi->UnionOriginalRectWith(fragment_rect);

		VisualDevice* vis_dev = document->GetVisualDevice();
		FontAtt font;
		font = vis_dev->GetFontAtt();
		font.SetSize(vis_dev->ScaleToScreen(font.GetSize()));
		form_eoi->SetFont(font);
		form_eoi->SetTextColor(OP_RGB(0, 0, 0)); // FIXME: use the actual color?
		form_eoi->SetBackgroundColor(OP_RGB(0xff, 0xff, 0xff)); // FIXME: use the actual color?
	}

	return TRUE;
}

/** Expand the region of the specified element of interest. */

void
IntersectionObject::ExpandEoiRegion(ElementOfInterest* eoi, OpRect& eoi_rect, const OpRect& box_rect)
{
	// Calculate the shortest distance from the origin point to the rectangle.

	VisualDevice* vis_dev = document->GetVisualDevice();

	LayoutCoord root_doc_x(0);
	LayoutCoord root_doc_y(0);

	// Find the distance from a frame to its window.
	// FIXME: would be really neat with a utility function that did this for us.

	for (FramesDocument* doc = document; doc; doc = doc->GetDocManager()->GetParentDoc())
	{
		AffinePos pos;
		VisualDevice* vd = doc->GetVisualDevice();

		vd->GetPosition(pos);

		OpPoint translation = pos.GetTranslation();
		root_doc_x += LayoutCoord(translation.x - vd->ScaleToScreen(vd->GetRenderingViewX()));
		root_doc_y += LayoutCoord(translation.y - vd->ScaleToScreen(vd->GetRenderingViewY()));
	}

	// new_rect is relative to the top left corner of the window, in screen coordinates.

	OpRect new_rect(vis_dev->ScaleToScreen(box_rect));

	new_rect.x += root_doc_x;
	new_rect.y += root_doc_y;

	eoi_rect.UnionWith(new_rect);

	// x_ofs and y_ofs are the click origin coordinates relative to the top left corner of the window, in screen coordinates.

	INT32 x_ofs = vis_dev->ScaleToScreen(orig_x) + root_doc_x;
	INT32 y_ofs = vis_dev->ScaleToScreen(orig_y) + root_doc_y;

	int x = x_ofs, y = y_ofs;

	if (x < eoi_rect.Left())
		x = eoi_rect.Left();
	else
		if (x > eoi_rect.Right())
			x = eoi_rect.Right();

	if (y < eoi_rect.Top())
		y = eoi_rect.Top();
	else
		if (y > eoi_rect.Bottom())
			y = eoi_rect.Bottom();

	int distance_x = x - x_ofs;
	int distance_y = y - y_ofs;
	unsigned int distance = distance_x * distance_x + distance_y * distance_y;

	if (distance < eoi->GetDistance())
	{
		// Found a rectangle closer to the origin; may need to reposition in candidate list
		// This can be optimized, but it's probably not important.

		eoi->Out();
		eoi->SetDistance(distance);

		for (ElementOfInterest* before = (ElementOfInterest*) eoi_list->First(); before; before = before->Suc())
			if (before->GetDistance() > distance)
			{
				eoi->Precede(before);
				break;
			}

		if (!eoi->InList())
			eoi->Into(eoi_list);
	}

	if (inside_cur_anchor_eoi)
	{
		if (eoi_first_text_anchor_fragment_x == 0)
			eoi_first_text_anchor_fragment_x = eoi_rect.x;
		else if (op_abs(eoi_first_text_anchor_fragment_x - x_ofs) < op_abs(eoi_rect.x - x_ofs))
		{
			// in multiline links, align the second and subsequent lines with the x of the first
			// fragment, only if the origin click is closer to the start (first line) of the link,
			// otherwise leave the x of the non-first line but adjust the y.
			eoi_rect.x = eoi_first_text_anchor_fragment_x;
		}
		else
		{
			// the current line (non-first line) of the link is closer to the origin click,
			// so adjust the y so the expanded element "starts" from the position of the
			// non-first line.
			// TODO: this looks rather unclean, consider looking for a way that meddles less directly with the rects.

			eoi->OffsetOriginalRectBy(0, new_rect.y - eoi->GetOriginalRect().y);
			eoi_rect.y = new_rect.y;

			if (cur_text_anchor_fragment)
			{
				OpRect new_text_anchor_rect = cur_text_anchor_fragment->GetRect();
				new_text_anchor_rect.y = new_rect.y;
				cur_text_anchor_fragment->SetRect(new_text_anchor_rect, 0);
			}
		}
	}

	eoi->UnionOriginalRectWith(eoi_rect);
}

#endif // NEARBY_ELEMENT_DETECTION

/** Handle text content */

/* virtual */ void
IntersectionObject::HandleTextContent(LayoutProperties* layout_props,
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
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (include_text_boxes && IsCentralPointInClipRect() &&
		x <= translated_x &&
		x + word_width + space_width + justified_space_extra > translated_x &&
		baseline - props.font_ascent <= translated_y &&
		baseline + props.font_descent > translated_y)
	{
		RecordHit(box,
		          translated_x - x,
		          translated_y - baseline + props.font_ascent);

#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
		grab_container_info = TRUE;

		container_rect.x = segment.line->GetX() + GetTranslationX();
		container_rect.y = segment.line->GetY() + GetTranslationY();
		container_rect.width = segment.line->GetWidth();
		container_rect.height = segment.line->GetLayoutHeight();
#endif // LAYOUT_INTERSECT_CONTAINER_INFO

		this->word = word;
		this->word_length = word_length;

		content_found = TRUE;
	}

#ifdef NEARBY_ELEMENT_DETECTION
	ElementOfInterest* eoi = inside_cur_anchor_eoi ? (ElementOfInterest*) cur_anchor_eoi : (ElementOfInterest*) cur_box_form_eoi;

	if (eoi && word_info.GetWidth() > 0 && !word_info.IsCollapsed())
	{
		OpRect local_rect(x,
						  baseline - props.font_ascent,
						  word_width + space_width,
						  props.font_ascent + props.font_descent - 1);

		OpRect box_rect = visual_device_transform.ToBBox(local_rect);

		if (!cur_text_anchor_fragment)
		{
			cur_text_anchor_fragment = OP_NEW(TextAnchorFragment, ());

			if (!cur_text_anchor_fragment)
			{
				SetOutOfMemory();
				return;
			}

			eoi->AppendFragment(cur_text_anchor_fragment);
		}

		OpRect fragment_rect = cur_text_anchor_fragment->GetRect();

		ExpandEoiRegion(eoi, fragment_rect, box_rect);
		cur_text_anchor_fragment->ExpandRect(fragment_rect);

		if (word_info.IsStartOfWord())
		{
			const uni_char* text = cur_text_anchor_fragment->GetText();

			if (text && *text)
				if (OpStatus::IsMemoryError(cur_text_anchor_fragment->AppendText(UNI_L(" "), 1)))
					SetOutOfMemory();
		}

		if (OpStatus::IsMemoryError(cur_text_anchor_fragment->AppendText(word, word_length)))
			SetOutOfMemory();
	}
#endif // NEARBY_ELEMENT_DETECTION
}

#ifdef NEARBY_ELEMENT_DETECTION

/** Handle replaced content */

/* virtual */ void
IntersectionObject::HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content)
{
	HitTestingTraversalObject::HandleReplacedContent(layout_props, content);

	if (eoi_list && GetTraverseType() == TRAVERSE_CONTENT)
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		HTML_Element* html_element = layout_props->html_element;
		OpRect box_rect = visual_device_transform.ToBBox(OpRect(0, 0, content->GetWidth(), content->GetHeight()));

		if ((inside_cur_anchor_eoi ||
			 (html_element->Type() == Markup::HTE_INPUT &&
			  html_element->GetInputType() == INPUT_IMAGE &&
			  !layout_props->html_element->GetDisabled())) &&
			content->IsImage())
		{
			/* FIXME: cleanup input_image stuff
			 * <input type="image"> and links work pretty much the same way, but they shouldn't
			 * both be lumped here.
			 * Image/alt text extraction should be moved to a function somewhere else, that way
			 * a FormImageInputElementOfInterest or ImageInputElementOfInterest
			 * (image input types have a widget associated??) can be created and separate
			 * link text/image fragments creation from image input content creation.
			 */
			if (html_element->Type() == Markup::HTE_INPUT && html_element->GetInputType() == INPUT_IMAGE)
			{
				cur_anchor_eoi = OP_NEW(AnchorElementOfInterest, (html_element));
				if (!cur_anchor_eoi)
				{
					SetOutOfMemory();
					return;
				}
				cur_anchor_eoi->Into(eoi_list);
			}

			// Crappy code for getting the Image.

			Image img;
			HEListElm* hle = html_element->GetHEListElm(FALSE);
			if (hle)
				img = hle->GetImage();

			if (img.ImageDecoded())
			{
				OP_ASSERT(hle); // img.ImageDecoded() can't be TRUE if hle is NULL.

				if (hle->GetImageWidth() <= 2 || hle->GetImageHeight() <= 2)
					// Try to ignore spacers
					return;

				AnchorFragment* anchor_fragment = OP_NEW(ImageAnchorFragment, (img));

				if (!anchor_fragment)
				{
					SetOutOfMemory();
					return;
				}

				cur_text_anchor_fragment = NULL;
				cur_anchor_eoi->AppendFragment(anchor_fragment);

				OpRect fragment_rect = anchor_fragment->GetRect();
				short padding_border_left = props.border.left.width + props.padding_left;
				short padding_border_top = props.border.top.width + props.padding_top;
				short padding_border_right = props.border.right.width + props.padding_right;
				short padding_border_bottom = props.border.bottom.width + props.padding_bottom;
				OpRect content_rect(padding_border_left,
									padding_border_top,
									content->GetWidth() - padding_border_left - padding_border_right,
									content->GetHeight() - padding_border_top - padding_border_bottom);
				content_rect = visual_device_transform.ToBBox(content_rect);
				ExpandEoiRegion(cur_anchor_eoi, fragment_rect, content_rect);
				anchor_fragment->ExpandRect(fragment_rect);
			}
			else
			{
				// NOTE : code to get the alternative text was copy/paste from layout/content/content.cpp
				// May be this part can be refactorized and reuse across several files in "layout" modules

				OpString translated_alt_text;

#define ImageStr UNI_L("Image")

				// If the image is not decoded (not yet loaded, image link broken...)
				// display its alternative text instead.
				const uni_char* alt_text = html_element->GetIMG_Alt();
				if (!alt_text)
				{
					TRAPD(rc, g_languageManager->GetStringL(Str::SI_DEFAULT_IMG_ALT_TEXT, translated_alt_text));
					alt_text = translated_alt_text.CStr();

					if (!alt_text)
						alt_text = ImageStr;
				}

				cur_text_anchor_fragment = OP_NEW(TextAnchorFragment, ());

				if (!cur_text_anchor_fragment)
					SetOutOfMemory();

				cur_anchor_eoi->AppendFragment(cur_text_anchor_fragment);

				OpRect fragment_rect = cur_text_anchor_fragment->GetRect();

				ExpandEoiRegion(cur_anchor_eoi, fragment_rect, box_rect);
				cur_text_anchor_fragment->ExpandRect(fragment_rect);

				if (!alt_text || uni_strlen(alt_text)==0)
				{
					// If text is empty, ensure at least one space character
					if (OpStatus::IsMemoryError(cur_text_anchor_fragment->AppendText(UNI_L(" "), 1)))
						SetOutOfMemory();
				}
				else
				{
					if (OpStatus::IsMemoryError(cur_text_anchor_fragment->AppendText(alt_text, uni_strlen(alt_text))))
						SetOutOfMemory();
				}
			}
		}
		else
			if (content->GetFormObject() && !layout_props->html_element->GetDisabled())
			{
				cur_box_form_eoi = NULL;
				cur_text_anchor_fragment = NULL;

				FormElementOfInterest* form_eoi = FormElementOfInterest::Create(html_element);

				if (!form_eoi)
				{
					SetOutOfMemory();
					return;
				}

#ifdef SUPPORT_TEXT_DIRECTION
				form_eoi->SetRTL(props.direction == CSS_VALUE_rtl);
#endif // SUPPORT_TEXT_DIRECTION

				form_eoi->Into(eoi_list);

				OpRect fragment_rect = form_eoi->GetOriginalRect();
				ExpandEoiRegion(form_eoi, fragment_rect, box_rect);
				form_eoi->UnionOriginalRectWith(fragment_rect);

				VisualDevice* vis_dev = document->GetVisualDevice();
				FontAtt font;
				font = vis_dev->GetFontAtt();
				font.SetSize(vis_dev->ScaleToScreen(font.GetSize()));
				form_eoi->SetFont(font);
				form_eoi->SetTextColor(OP_RGB(0, 0, 0)); // FIXME: use the actual color?
				form_eoi->SetBackgroundColor(OP_RGB(0xff, 0xff, 0xff)); // FIXME: use the actual color?

				if (html_element->Type() == Markup::HTE_INPUT)
					switch (html_element->GetInputType())
					{
					case INPUT_RADIO:
					case INPUT_CHECKBOX:
						cur_box_form_eoi = form_eoi;
						break;
					}
			}
	}
}

#endif // NEARBY_ELEMENT_DETECTION

#ifdef CSS_TRANSFORMS

/* virtual */ OP_BOOLEAN
IntersectionObject::PushTransform(const AffineTransform &t, TranslationState &state)
{
	OP_BOOLEAN res = HitTestingTraversalObject::PushTransform(t, state);

	if (res != OpBoolean::IS_TRUE)
		return res;

	OpPoint local = visual_device_transform.ToLocal(OpPoint(orig_x, orig_y));
	translated_x = LayoutCoord(local.x);
	translated_y = LayoutCoord(local.y);
	return OpBoolean::IS_TRUE;
}

/* virtual */ void
IntersectionObject::PopTransform(const TranslationState &state)
{
	HitTestingTraversalObject::PopTransform(state);

	OpPoint local = visual_device_transform.ToLocal(OpPoint(orig_x, orig_y));
	translated_x = LayoutCoord(local.x);
	translated_y = LayoutCoord(local.y);
}

#endif // CSS_TRANSFORMS

ElementSearchObject::ElementSearchObject(FramesDocument *doc,
										 const RECT& area,
										 ElementSearchCustomizer& customizer,
										 const RECT* extended_area /*= NULL*/,
										 BOOL one_pass_traverse /*=FALSE*/)
	: HitTestingTraversalObject(doc)
	, customizer(customizer)
	, suppress_elements(FALSE)
	, terminate_traverse(FALSE)
	, intersects_main_area(TRUE)
	, opacity_in_vertical_box_started(FALSE)
	, ancestor_containers_locked(TRUE)
	, use_extended_area(!!extended_area)
{
	if (!customizer.notify_overlapping_rects || one_pass_traverse)
		SetTraverseType(TRAVERSE_ONE_PASS);

	if (use_extended_area)
	{
		this->main_area = area;
		main_area_empty = FALSE;
		this->area = *extended_area;
	}
	else
		this->area = area;
}

/*virtual*/ BOOL
ElementSearchObject::HandleVerticalBox(LayoutProperties* parent_lprops,
									   LayoutProperties*& layout_props,
									   VerticalBox* box,
									   TraverseInfo& traverse_info,
									   BOOL clip_affects_traversed_descendants)
{
	if (!target_element)
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		BOOL3 is_opaque = MAYBE;

		if (customizer.care_for_opacity && props.opacity < UINT8_MAX)
		{
			if (!BeginOpacity())
			{
				terminate_traverse = TRUE;
				return FALSE;
			}
			opacity_in_vertical_box_started = TRUE;
		}

		if (props.visibility == CSS_VALUE_visible)
		{
			// A check in ElementSearchObject::EntereVerticalBox ensures that
			OP_ASSERT(layout_props->html_element->Parent());

			OP_ASSERT(layout_props->html_element->Parent()->GetLayoutBox());

			OpRect rect(0, 0, box->GetWidth(), box->GetHeight()), clipped_rect;
			HTML_Element* element = layout_props->html_element;
			Box* parent_box = element->Parent()->GetLayoutBox();

			BOOL check_for_opaqueness = customizer.notify_overlapping_rects &&
				(!(element->IsMatchingType(Markup::HTE_BODY, NS_HTML) || element->IsMatchingType(Markup::HTE_HTML, NS_HTML)) ||
				element->GetInserted() != HE_NOT_INSERTED);

			// Do we have to check whether this box intersects the extended area ?
			BOOL check_extended;
			/** This is the flag that is later passed as an out param to the IsRectIntersectingAreas.
				TRUE means this box intersects only the extended area.
				FALSE means this box intersects the main area. Stays FALSE if check_extended will be FALSE. */
			BOOL extended_only = FALSE;

			/** Is there a possibility that the rect from this box should be added to its ancestors,
				such that there is no box with a container (or table content), on a path
				this box (exclusive), ... , the given ancestor (inclusive).
				By path I mean taking the parent from the logical tree in each step. */
			BOOL check_for_direct_ancestors = FALSE;

			/** Is there a possibility that the rect from this box should be added to any of its
				ancestors up to the root. */
			BOOL check_for_ancestors = FALSE;
			// initializing the above two flags to silence compiler warning (when used always initialized)

			if (use_extended_area)
			{
				if (intersects_main_area) // bounding box intersects the main area
					check_extended = TRUE; // don't need check_for_ancestors flag in such case
				else
				{
					check_for_direct_ancestors = !parent_box->IsContainingElement();
					check_for_ancestors = check_for_direct_ancestors || !ancestor_containers_locked;
					check_extended = check_for_opaqueness || check_for_ancestors;
				}
			}
			else
				check_extended = FALSE; // don't need check_for_ancestors flag in such case

			if (IsRectIntersectingAreas(rect, check_extended, &extended_only, &clipped_rect))
			{
				// If the box'es rect intersects the main area, it's bounding box also must
				OP_ASSERT(extended_only || intersects_main_area);

				if (check_for_opaqueness)
				{
					OpRect out_rect;
					is_opaque = IsBoxOpaque(box, props, rect, out_rect) ? YES : NO;
					if (is_opaque == YES && !out_rect.IsEmpty())
					{
						out_rect.OffsetBy(translation_x, translation_y);
						if (!HandleOverlappingRect(element, out_rect, extended_only))
						{
							terminate_traverse = TRUE;
							return FALSE;
						}
					}
				}

				if (!suppress_elements && (intersects_main_area || check_for_ancestors))
				{
					// Empty rect can't intersect any area
					OP_ASSERT(!clipped_rect.IsEmpty());

					if (!extended_only && GetTraverseType() != TRAVERSE_BACKGROUND &&
						element->HasAttr(Markup::HA_USEMAP))
					{
						INT32 offset_x = props.border.left.width + props.padding_left,
							offset_y = props.border.top.width + props.padding_top;

						if (!HandleUsemapElement(element, clipped_rect, offset_x, offset_y))
						{
							terminate_traverse = TRUE;
							return FALSE;
						}
					}

					clipped_rect.OffsetBy(translation_x, translation_y);

					if ((intersects_main_area || check_for_direct_ancestors) &&
						!HandleElementsUpToContainer(intersects_main_area ? element : element->Parent(), clipped_rect, extended_only))
					{
						terminate_traverse = TRUE;
						return FALSE;
					}

					if (!ancestor_containers_locked && !AddRectToOpenElements(clipped_rect, extended_only))
					{
						terminate_traverse = TRUE;
						return FALSE;
					}
				}
			}
		}

		if (intersects_main_area)
		{
			/** suppress elements is expected to be TRUE only in case of being inside a box, which bounding box is totally
				outside the main area */
			OP_ASSERT(!suppress_elements);

			// Entering a box with a container. We don't report overflow as part of ancestor elements regions.
			traverse_info.old_ancestor_containers_locked = ancestor_containers_locked;
			ancestor_containers_locked = TRUE;
			return TRUE;
		}
		// We enter the box only to seek for opaque areas, if the below condition is TRUE it makes sense to traverse to it's children
		else if (customizer.notify_overlapping_rects &&
			(box->HasContentOverflow() || is_opaque == NO) )
		{
			traverse_info.old_suppress_elements = suppress_elements;
			suppress_elements = TRUE;

			// Entering a box with a container. We don't report overflow as part of ancestor elements regions.
			traverse_info.old_ancestor_containers_locked = ancestor_containers_locked;
			ancestor_containers_locked = TRUE;
			return TRUE;
		}
		else
			return FALSE;
	}
	else if (!ancestor_containers_locked && box->HasContentOverflow())
		/** Traversing to a positioned element. From the interval: [layout_props->html_element, first element with a container or a table content)
			store the first ancestor that passes a filter for possible fragments reporting (if found one).
			When reporting a rectangle for ancestors during the target traversal, we iterate through the open_elements vector. For each element there
			we call HandleElementsUpToContainer. Therefore, if there is any element in
			layout_props->html_element, ... , (first element with a container or a table content) path that passes the filter, we need to store the one
			that is the farthest from the root. The path here means taking parents from layout_props->html_element up to the first element
			with a container or a table content exclusive. Then in HandleElementsUpToContainer we won't skip anything. */
		return AddFirstInterestingElement(layout_props->html_element);
	else
		return TRUE;
}

/** Checks and possibly reports a scrollbar in terms of an object and an overlapping rect. */

BOOL
ElementSearchObject::CheckScrollbar(ScrollableArea* scrollable, OpRect& scrollbar_rect, BOOL check_extended, BOOL vertical)
{
	// suppress elements may be true only if we search for overlapping areas being in extended area only

	OP_ASSERT(!suppress_elements || check_extended);

	OpRect clipped_rect;

	if ((suppress_elements && CheckIntersectionWithClipping(scrollbar_rect)) ||
		(!suppress_elements && IsRectIntersectingAreas(scrollbar_rect, check_extended, &check_extended, &clipped_rect)))
	{
		HTML_Element* html_element = scrollable->GetOwningBox()->GetHtmlElement();

		/** The scrollbar_rect is intersecting only the extended area (which
			implies we report overlapping rects) or it's intersecting the main
			area and we report overlapping rects. */

		if (check_extended || customizer.notify_overlapping_rects)
		{
			OpRect rough_clip_rect = GetRoughClipRect();

			scrollbar_rect.IntersectWith(rough_clip_rect);

			if (!scrollbar_rect.IsEmpty())
			{
				scrollbar_rect.OffsetBy(translation_x, translation_y);
				if (!HandleOverlappingRect(html_element, scrollbar_rect, check_extended))
					return FALSE;
			}
		}

		// In order to report a scrollbar it must intersect the main area and we must accept scrollbars.

		if (!check_extended && customizer.accept_scrollbar)
		{
			OP_ASSERT(!clipped_rect.IsEmpty());

			clipped_rect.OffsetBy(translation_x, translation_y);

			if (!HandleScrollbarFound(html_element, vertical, clipped_rect))
				return FALSE;
		}
	}

	return TRUE;
}

/* virtual */ BOOL
ElementSearchObject::HandleScrollable(const HTMLayoutProperties& props,
									  ScrollableArea* scrollable,
									  LayoutCoord width,
									  LayoutCoord height,
									  TraverseInfo& traverse_info,
									  BOOL clip_affects_target,
									  int scrollbar_modifier)
{
	if (!target_element && GetTraverseType() != TRAVERSE_BACKGROUND &&
		(customizer.accept_scrollbar || customizer.notify_overlapping_rects) &&
		(scrollable->HasVerticalScrollbar() || scrollable->HasHorizontalScrollbar()))
	{
		OpRect scrollbar_rect, clipped_rect;
		BOOL check_extended = use_extended_area && customizer.notify_overlapping_rects;

		if (scrollable->HasVerticalScrollbar())
		{
			scrollbar_rect.x = props.border.left.width + (scrollbar_modifier ? 0 : width);
			scrollbar_rect.y = props.border.top.width;
			scrollbar_rect.width = scrollable->GetVerticalScrollbarWidth();
			scrollbar_rect.height = height;

			if (!CheckScrollbar(scrollable, scrollbar_rect, check_extended, TRUE))
			{
				terminate_traverse = TRUE;
				return FALSE;
			}
		}

		if (scrollable->HasHorizontalScrollbar())
		{
			scrollbar_rect.x = props.border.left.width;
			scrollbar_rect.y = props.border.top.width + height;
			scrollbar_rect.width = width;
			scrollbar_rect.height = scrollable->GetHorizontalScrollbarWidth();

			if (!CheckScrollbar(scrollable, scrollbar_rect, check_extended, FALSE))
			{
				terminate_traverse = TRUE;
				return FALSE;
			}
		}
	}

	return TRUE;
}

#ifdef CSS_TRANSFORMS

/*virtual*/ OP_BOOLEAN
ElementSearchObject::PushTransform(const AffineTransform &t, TranslationState &state)
{
	if (!GetClipRect())
		return HitTestingTraversalObject::PushTransform(t, state);

	OpRect old_clip_rect = *GetClipRect();
	AffinePos old_transform = visual_device_transform.GetCTM();

	OP_BOOLEAN res = HitTestingTraversalObject::PushTransform(t, state);
	if (res != OpBoolean::IS_TRUE)
		return res;

	if (GetClippingPrecision() == MERGED_WITH_AREA && use_extended_area)
	{
		/** If we suppress_elements and the clipping precision is MERGED_WITH_AREA, we will not report
			any non empty overlapping rectangle, so no point in entering. */
		if (suppress_elements)
		{
			HitTestingTraversalObject::PopTransform(state);
			return OpBoolean::IS_FALSE;
		}

		state.old_main_area = main_area;

		if (!main_area_empty)
		{
			/** If the clipping precision is MERGED_WITH_AREA, we must also do similar operation for main_area,
				because in such case the clip rect was set to be the rectangle of the whole document, so the
				main_area isn't efectively clipped. To keep the consistence we must intersect it with the
				clip rect that was present before entering this transform context. For other states of
				clipping precision, the clip rect is kept and can be used to clip the main area as well
				as the extended area. */

			old_transform.Apply(old_clip_rect);

			BOOL intersection_res = IntersectRectWith(main_area, old_clip_rect);

			if (!intersection_res)
				main_area_empty = TRUE;
		}
	}

	return OpBoolean::IS_TRUE;
}

/*virtual*/ void
ElementSearchObject::PopTransform(const TranslationState &state)
{
	BOOL restore = use_extended_area && GetClippingPrecision() == MERGED_WITH_AREA;

	HitTestingTraversalObject::PopTransform(state);

	// At that moment, if restore is TRUE it means that there must had been a finite clip rect before the transform
	OP_ASSERT(!restore || GetClipRect());

	if (restore)
	{
		main_area = state.old_main_area;
		main_area_empty = main_area.left >= main_area.right || main_area.top >= main_area.bottom;
	}
}

#endif //CSS_TRANSFORMS

/*virtual*/BOOL
ElementSearchObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (terminate_traverse)
		return FALSE;

	OP_ASSERT(parent_lprops);

	// To get rid of possible anomalies in ElementSearchObject::HandleVerticalBox code
	if (!parent_lprops->html_element)
		return TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info);

	BOOL result = HitTestingTraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info);

	if (opacity_in_vertical_box_started)
	{
		// This is the only moment where we can be sure about not entering the box and thus the need to call EndOpacity
		if (!result)
			EndOpacity();
		opacity_in_vertical_box_started = FALSE;
	}

	return result;
}

/*virtual*/void
ElementSearchObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	HitTestingTraversalObject::LeaveVerticalBox(layout_props, box, traverse_info);

	if (!target_element)
	{
		// Complex condition that determines whether this box has begun an opacity context
		if (!traverse_info.dry_run && customizer.care_for_opacity && layout_props->GetProps()->opacity < UINT8_MAX)
			EndOpacity();

		suppress_elements = traverse_info.old_suppress_elements;
		ancestor_containers_locked = traverse_info.old_ancestor_containers_locked;
	}
	else if (!ancestor_containers_locked && open_elements.GetCount())
		// Positioned element traversal. Pop a possibly previously added element that is this box'es element or some ancestor.
		RemoveCorrespondingElement(layout_props->html_element);
}

/*virtual*/ BOOL
ElementSearchObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (terminate_traverse)
		return FALSE;

	return HitTestingTraversalObject::EnterLine(parent_lprops, line, containing_element, traverse_info);
}

/*virtual*/ BOOL
ElementSearchObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (!terminate_traverse && HitTestingTraversalObject::EnterInlineBox(layout_props, box, box_area, segment, start_of_box, end_of_box, baseline, traverse_info))
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();

		if (!target_element)
		{
			if (customizer.care_for_opacity && props.opacity < UINT8_MAX)
				if (!BeginOpacity())
				{
					terminate_traverse = TRUE;
					return FALSE;
				}

			if (props.visibility == CSS_VALUE_visible)
			{
				BOOL in_extended_only = use_extended_area;
				RECT clipped_rect;

				if (IsRectIntersectingAreas(box_area, in_extended_only, &in_extended_only, &clipped_rect))
				{
					if (customizer.notify_overlapping_rects)
					{
						OpRect box_rect(&box_area), out_rect;

						if (IsBoxOpaque(box, props, box_rect, out_rect) && !out_rect.IsEmpty())
						{
							out_rect.OffsetBy(translation_x, translation_y);
							if (!HandleOverlappingRect(layout_props->html_element, out_rect, in_extended_only))
							{
								terminate_traverse = TRUE;
								return FALSE;
							}
						}
					}

					if (suppress_elements)
						return TRUE;

					OpRect clipped_op_rect(&clipped_rect);

					if (!clipped_op_rect.IsEmpty())
					{
						if (!in_extended_only && layout_props->html_element->HasAttr(Markup::HA_USEMAP))
						{
							INT32 offset_x = box_area.left + props.border.left.width + props.padding_left,
								offset_y = box_area.top + props.border.top.width + props.padding_top;

							if (!HandleUsemapElement(layout_props->html_element, clipped_op_rect, offset_x, offset_y))
							{
								terminate_traverse = TRUE;
								return FALSE;
							}
						}

						clipped_op_rect.OffsetBy(translation_x, translation_y);

						if (!HandleElementsUpToContainer(layout_props->html_element, clipped_op_rect, in_extended_only))
						{
							terminate_traverse = TRUE;
							return FALSE;
						}

						if (!ancestor_containers_locked && !AddRectToOpenElements(clipped_op_rect, in_extended_only))
						{
							terminate_traverse = TRUE;
							return FALSE;
						}
					}
				}
			}

			if (box->IsInlineBlockBox())
			{
				// Entering a box with a container. We don't report overflow as part of ancestor elements regions.
				traverse_info.old_ancestor_containers_locked = ancestor_containers_locked;
				ancestor_containers_locked = TRUE;
			}
		}
		else if (!ancestor_containers_locked && box->IsInlineBlockBox())
			/** Traversing to a positioned element. From the interval: [layout_props->html_element, first element with a container or a table content)
				store the first ancestor that passes a filter for possible fragments reporting (if found one).
				When reporting a rectangle for ancestors during the target traversal, we iterate through the open_elements vector. For each element there
				we call HandleElementsUpToContainer. Therefore, if there is any element in
				layout_props->html_element, ... , (first element with a container or a table content) path that passes the filter, we need to store the one
				that is the farthest from the root. The path here means taking parents from layout_props->html_element up to the first element
				with a container or a table content exclusive. Then in HandleElementsUpToContainer we won't skip anything. */
			return AddFirstInterestingElement(layout_props->html_element);

		return TRUE;
	}

	return FALSE;
}

/*virtual*/void
ElementSearchObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	HitTestingTraversalObject::LeaveInlineBox(layout_props, box, box_area, start_of_box, end_of_box, traverse_info);

	// Complex condition that determines whether this box has begun an opacity context
	if (customizer.care_for_opacity && !target_element && layout_props->GetProps()->opacity < UINT8_MAX)
		EndOpacity();

	if (box->IsInlineBlockBox())
	{
		if (!target_element)
			ancestor_containers_locked = traverse_info.old_ancestor_containers_locked;
		else if (!ancestor_containers_locked && open_elements.GetCount())
			// Positioned element traversal. Pop a possibly previously added element that is this box'es element or some ancestor.
			RemoveCorrespondingElement(layout_props->html_element);
	}
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
ElementSearchObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	return TRUE;
}

#endif // PAGED_MEDIA_SUPPORT

/*virtual*/BOOL
ElementSearchObject::EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info)
{
	if (terminate_traverse)
		return FALSE;

	return HitTestingTraversalObject::EnterPane(multipane_lprops, pane, is_column, traverse_info);
}

/*virtual*/BOOL
ElementSearchObject::EnterTableRow(TableRowBox* row)
{
	if (terminate_traverse)
		return FALSE;

	return HitTestingTraversalObject::EnterTableRow(row);
}

/** Notify that we are back at the target origin. */

/* virtual */ void
ElementSearchObject::TargetFinished(Box* box)
{
	HitTestingTraversalObject::TargetFinished(box);

	/* In case of returning to the stacking_context that we started target
	   traversal from and the box owning the stacking_context having a container,
	   we reach the point, where we unlocked elements with containers storing.
	   Lock them now again then. */

	if (box->GetLocalZElement() && box->IsContainingElement())
	{
		ancestor_containers_locked = TRUE;

		if (open_elements.GetCount())
			RemoveCorrespondingElement(box->GetHtmlElement());
	}
}

/*virtual*/BOOL
ElementSearchObject::TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element)
{
	if (terminate_traverse)
		return FALSE;

	BOOL result = HitTestingTraversalObject::TraversePositionedElement(element, containing_element);

	if (result && !suppress_elements)
	{
		ancestor_containers_locked = FALSE;
		if (containing_element->Type() != Markup::HTE_DOC_ROOT)
		{
			Box* containing_box = containing_element->GetLayoutBox();

			if ((!containing_box->IsVerticalBox() || static_cast<VerticalBox*>(containing_box)->HasContentOverflow()))
			/** Traversing to a positioned element and it is possible that the containing_element's or some of its ancestors
				will need to have their regions extended by the rectangles of the element (or some of its descendants).
				From the interval: [element, first element with a container or a table content)
				store the first ancestor that passes a filter for possible fragments reporting (if found one).
				When reporting a rectangle for ancestors during the target traversal, we iterate through the open_elements vector. For each element there
				we call HandleElementsUpToContainer. Therefore, if there is any element in
				layout_props->html_element, ... , (first element with a container or a table content) path that passes the filter, we need to store the one
				that is the farthest from the root in the tree. The path here means taking parents from element up to the first element
				with a container or a table content exclusive. Then in HandleElementsUpToContainer we won't skip anything. */
				result = AddFirstInterestingElement(containing_element);
		}
	}

	return result;
}

BOOL
ElementSearchObject::IsBoxOpaque(Box* box, const HTMLayoutProperties& props, const OpRect& box_rect, OpRect& covering_rect) const
{
	// Inline boxes are not background traversed
	OP_ASSERT(!box->IsInlineBox() || GetTraverseType() != TRAVERSE_BACKGROUND);
	// visibility checked before
	OP_ASSERT(props.visibility == CSS_VALUE_visible);

	OpRect clip_rect = GetRoughClipRect();

	if (clip_rect.IsEmpty()) // No point in further checks, since we will result with empty covering_rect always
		return FALSE;

	BOOL has_background = FALSE;
	BOOL has_opaque_content = FALSE;

	switch(GetTraverseType())
	{
	case TRAVERSE_ONE_PASS:
	case TRAVERSE_CONTENT:
		has_opaque_content = box->IsContentReplaced();
		if (!box->IsInlineBox() && GetTraverseType() == TRAVERSE_CONTENT)
			break;
	case TRAVERSE_BACKGROUND:
		has_background = HasBackground(props);
	}

	if (has_background || has_opaque_content)
	{
		if (has_background)
			covering_rect = box_rect;

		if (has_opaque_content && !has_background)
		{
			/** Should we take the inner content rect or the rect surrounding the border ?
				Let's say that if at least one border has some significant width and the padding on that side
				is not greater than the border, we take the border rectangle. */
			if ((props.border.left.width >= SIGNIFICANT_BORDER_WIDTH && props.padding_left <= props.border.left.width) ||
				(props.border.top.width >= SIGNIFICANT_BORDER_WIDTH && props.padding_top <= props.border.top.width) ||
				(props.border.right.width >= SIGNIFICANT_BORDER_WIDTH && props.padding_right <= props.border.right.width) ||
				(props.border.bottom.width >= SIGNIFICANT_BORDER_WIDTH && props.padding_bottom <= props.border.bottom.width))
				covering_rect = box_rect;
			else
			{
				covering_rect = static_cast<ReplacedContent*>(box->GetContent())->GetInnerRect(props);
				covering_rect.OffsetBy(box_rect.x, box_rect.y);
			}
		}

		covering_rect.IntersectWith(clip_rect);

		if (props.clip_left != CLIP_NOT_SET && box->IsAbsolutePositionedBox())
			covering_rect.IntersectWith(static_cast<Content_Box*>(box)->GetClippedRect(props, FALSE));

		return TRUE;
	}

	return FALSE;
}

BOOL
ElementSearchObject::IsRectIntersectingAreas(const RECT& rect, BOOL check_extended, BOOL* intersects_extended_only /*= NULL*/, RECT* clipped_rect /*= NULL*/) const
{
	if (check_extended)
	{
		OP_ASSERT(intersects_extended_only);

		if (!main_area_empty)
		{
			*intersects_extended_only = FALSE;
			BOOL result = DoubleCheckIntersectionWithClipping(rect, main_area, *intersects_extended_only, clipped_rect);
			*intersects_extended_only = !(*intersects_extended_only);

			return result;
		}
		else
			*intersects_extended_only = TRUE;
	}
	else if (use_extended_area)
		return !main_area_empty && CheckIntersectionWithClipping(rect, &main_area, clipped_rect);

	return CheckIntersectionWithClipping(rect, NULL, clipped_rect);
}

BOOL
ElementSearchObject::IsRectIntersectingAreas(const OpRect& rect, BOOL check_extended, BOOL* intersects_extended_only /*= NULL*/, OpRect* clipped_rect /*= NULL*/) const
{
	if (check_extended)
	{
		OP_ASSERT(intersects_extended_only);

		if (!main_area_empty)
		{
			*intersects_extended_only = FALSE;
			BOOL result = DoubleCheckIntersectionWithClipping(rect, main_area, *intersects_extended_only, clipped_rect);
			*intersects_extended_only = !(*intersects_extended_only);

			return result;
		}
		else
			*intersects_extended_only = TRUE;
	}
	else if (use_extended_area)
		return !main_area_empty && CheckIntersectionWithClipping(rect, &main_area, clipped_rect);

	return CheckIntersectionWithClipping(rect, NULL, clipped_rect);
}

BOOL
ElementSearchObject::HandleElementsUpToContainer(HTML_Element *elm, const OpRect& elm_rect, BOOL in_extended_only, BOOL check_first)
{
	/** Handle this element, then iterate through all ancestor inline placed elements
		and handle adding this elm_rect as fragment of each element that passes the filter. */

	if ((!check_first || customizer.AcceptElement(elm, document)) &&
		!HandleElementFound(elm, elm_rect, in_extended_only))
		return FALSE;

	HTML_Element* iter = elm->Parent();

	while (iter && !iter->GetLayoutBox()->IsContainingElement())
	{
		if (customizer.AcceptElement(iter, document) &&
			!HandleElementFound(iter, elm_rect, in_extended_only))
			return FALSE;

		iter = iter->Parent();
	}

	return TRUE;
}

BOOL
ElementSearchObject::AddRectToOpenElements(const OpRect& elm_rect, BOOL in_extended_only)
{
	int count = open_elements.GetCount();
	for (int i=0; i < count; i++)
	{
		HTML_Element* elm = open_elements.Get(i);
		if (!HandleElementsUpToContainer(elm, elm_rect, in_extended_only, FALSE))
			return FALSE;
	}

	return TRUE;
}

BOOL
ElementSearchObject::AddFirstInterestingElement(HTML_Element* container_elm)
{
	do
	{
		if (customizer.AcceptElement(container_elm, document))
		{
			if (OpStatus::IsMemoryError(open_elements.Add(container_elm)))
			{
				SetOutOfMemory();
				terminate_traverse = TRUE;
				return FALSE;
			}
			else
				return TRUE;
		}

		container_elm = container_elm->Parent();
	}
	while (container_elm && !container_elm->GetLayoutBox()->IsContainingElement());

	return TRUE;
}

void
ElementSearchObject::RemoveCorrespondingElement(HTML_Element* container_elm)
{
	OP_ASSERT(open_elements.GetCount());

	UINT32 last_idx = open_elements.GetCount() - 1;
	HTML_Element* last_elm = open_elements.Get(last_idx);

	do
	{
		if (last_elm == container_elm)
		{
			open_elements.Remove(last_idx);
			break;
		}

		container_elm = container_elm->Parent();
	}
	while (container_elm && !container_elm->GetLayoutBox()->IsContainingElement());
}

BOOL
ElementSearchObject::HandleUsemapElement(HTML_Element* usemap_owner, const OpRect& clip_rect, INT32 offset_x, INT32 offset_y)
{
	LogicalDocument* logDoc = document->GetLogicalDocument();
	URL* map_url = usemap_owner->GetUrlAttr(Markup::HA_USEMAP, NS_IDX_HTML, logDoc);
	HTML_Element* mapElm = logDoc->GetNamedHE(map_url->UniRelName());

	if (!mapElm)
		return TRUE;

	int width_scale = (int)(INTPTR)usemap_owner->GetSpecialAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, ITEM_TYPE_NUM, (void*)1000, SpecialNs::NS_LAYOUT);
	int height_scale = (int)(INTPTR)usemap_owner->GetSpecialAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, ITEM_TYPE_NUM, (void*)1000, SpecialNs::NS_LAYOUT);

	HTML_Element* areaElm = mapElm->GetNextLinkElementInMap(TRUE, mapElm);
	while (areaElm)
	{
		if (!customizer.AcceptElement(areaElm, document))
		{
			areaElm = areaElm->GetNextLinkElementInMap(TRUE, mapElm);
			continue;
		}

		if (areaElm->GetAREA_Shape() == AREA_SHAPE_DEFAULT)
		{
			OpRect whole_map_rect = clip_rect;
			whole_map_rect.OffsetBy(translation_x, translation_y);
			if (!HandleElementFound(areaElm, whole_map_rect))
				return FALSE;
		}
		else
		{
			CoordsAttr* ca = (CoordsAttr*)areaElm->GetAttr(Markup::HA_COORDS, ITEM_TYPE_COORDS_ATTR, (void*)0);

			if (!ca)
			{
				areaElm = areaElm->GetNextLinkElementInMap(TRUE, mapElm);
				continue;
			}

			int coords_len = ca->GetLength();
			int* coords = ca->GetCoords();
			RECT areaRect;

			switch (areaElm->GetAREA_Shape())
			{
			case AREA_SHAPE_CIRCLE:
				{
					if (coords_len < 3)
					{
						areaElm = areaElm->GetNextLinkElementInMap(TRUE, mapElm);
						continue; // while(areaElm)
					}

					int radius_x = coords[2]*width_scale / 1000,
						radius_y = coords[2]*height_scale / 1000;

					INT32 centre_x = offset_x + coords[0]*width_scale/1000;
					INT32 centre_y = offset_y + coords[1]*height_scale/1000;

					areaRect.left = centre_x - radius_x + 1;
					areaRect.right = areaRect.left + 2*radius_x - 1;
					areaRect.top = centre_y - radius_y + 1;
					areaRect.bottom = areaRect.top + 2*radius_y -1;
				}
				break;
			case AREA_SHAPE_POLYGON:
				{
					// Approximate that polygon with a bounding rectangle
					int counter = 0;
					areaRect.left = LONG_MAX;
					areaRect.top = LONG_MAX;
					areaRect.right = LONG_MIN;
					areaRect.bottom = LONG_MIN;

					if (coords_len < 2)
					{
						areaElm = areaElm->GetNextLinkElementInMap(TRUE, mapElm);
						continue; // while(areaElm)
					}

					while (counter < coords_len)
					{
						if (coords[counter] < areaRect.left)
							areaRect.left = coords[counter];
						if (coords[counter] > areaRect.right)
							areaRect.right = coords[counter];
						if (coords[counter+1] < areaRect.top)
							areaRect.top = coords[counter+1];
						if (coords[counter+1] > areaRect.bottom)
							areaRect.bottom = coords[counter+1];

						counter += 2;
					}

					areaRect.left *= width_scale;
					areaRect.left /= 1000;
					areaRect.right *= width_scale;
					areaRect.right /= 1000;
					areaRect.top *= height_scale;
					areaRect.top /= 1000;
					areaRect.bottom *= height_scale;
					areaRect.bottom /= 1000;

					areaRect.left += offset_x;
					areaRect.right += offset_x;
					areaRect.top += offset_y;
					areaRect.bottom += offset_y;
				}
				break;
			case AREA_SHAPE_RECT:
				{
					if (coords_len < 4)
					{
						areaElm = areaElm->GetNextLinkElementInMap(TRUE, mapElm);
						continue; // while(areaElm)
					}

					if (coords[2] >= coords[0])
					{
						// Normal case
						areaRect.left = coords[0]*width_scale/1000;
						areaRect.right = coords[2]*width_scale/1000;
					}
					else
					{
						// Coords are swapped
						areaRect.left = coords[2]*width_scale/1000;
						areaRect.right = coords[0]*width_scale/1000;
					}
					if (coords[3] >= coords[1])
					{
						// Normal case
						areaRect.top = coords[1]*height_scale/1000;
						areaRect.bottom = coords[3]*height_scale/1000;
					}
					else
					{
						// Coords are swapped
						areaRect.top = coords[3]*height_scale/1000;
						areaRect.bottom = coords[1]*height_scale/1000;
					}

					areaRect.left += offset_x;
					areaRect.right += offset_x;
					areaRect.top += offset_y;
					areaRect.bottom += offset_y;
				}
				break;
			default:
				OP_ASSERT(!"Unknown usemap shape type!");
			}

			if (IsRectIntersectingAreas(areaRect, FALSE))
			{
				OpRect area_op_rect(&areaRect);
				area_op_rect.IntersectWith(clip_rect);
				if (!area_op_rect.IsEmpty())
				{
					area_op_rect.OffsetBy(translation_x, translation_y);
					if (!HandleElementFound(areaElm, area_op_rect))
						return FALSE;
				}
			}
		}

		areaElm = areaElm->GetNextLinkElementInMap(TRUE, mapElm);
	}

	return TRUE;
}

/*virtual*/ BOOL
ElementSearchObject::IntersectsCustom(RECT& rect) const
{
	if (use_extended_area)
	{
		intersects_main_area = FALSE;
		if (!main_area_empty)
			return DoubleCheckIntersectionWithClipping(rect, main_area, intersects_main_area);
		else
			return CheckIntersectionWithClipping(rect);
	}
	else
	{
		intersects_main_area = TRUE;
		return CheckIntersectionWithClipping(rect);
	}
}

ElementCollectingObject::RectLink::~RectLink()
{
	Out();
	transform_tree->ref_counter--;
}

OP_STATUS
ElementCollectingObject::ElementValue::AddRect(const OpRect& rect, TransformTree *tree)
{
	RectLink* new_rect = OP_NEW(RectLink, (rect, tree));

	OP_ASSERT(tree);

	if (new_rect)
	{
		new_rect->Into(&rects);
		tree->ref_counter++;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

void
ElementCollectingObject::ElementValue::MergeRects(const RectList& second)
{
	RectLink* iter = second.First();

	while (iter)
	{
		RectLink* to_add = iter;
		iter = iter->Suc();

		to_add->Out();
		to_add->Into(&rects);
	}
}

OP_STATUS
ElementCollectingObject::ElementValue::DivideRect(const RectLink* to_divide, const OpRect& dividing_rect)
{
	// No sense of the below operation otherwise
	OP_ASSERT(dividing_rect.Intersecting(to_divide->rect) && !dividing_rect.Contains(to_divide->rect));

	OpRect new_rects[4]; //left_rect, top_rect, right_rect, bottom_rect;
	const OpRect& initial_rect = to_divide->rect;

	// Four checks whether in each direction there is uncovered part/rect
	if (dividing_rect.x > initial_rect.x)
		new_rects[0].Set(initial_rect.x, initial_rect.y, dividing_rect.x - initial_rect.x, initial_rect.height);

	if (dividing_rect.y > initial_rect.y)
		new_rects[1].Set(initial_rect.x, initial_rect.y, initial_rect.width, dividing_rect.y - initial_rect.y);

	if (dividing_rect.Right() < initial_rect.Right())
		new_rects[2].Set(dividing_rect.Right(), initial_rect.y, initial_rect.Right() - dividing_rect.Right(), initial_rect.height);

	if (dividing_rect.Bottom() < initial_rect.Bottom())
		new_rects[3].Set(initial_rect.x, dividing_rect.Bottom(), initial_rect.width, initial_rect.Bottom() - dividing_rect.Bottom());

	// If there is left or right rect and top or bottom rect, reduce the later ones, so that the same pixels are not covered twice
	for (int i=1; i<4; i+=2)
		if (!new_rects[i].IsEmpty())
		{
			if (!new_rects[0].IsEmpty())
			{
				new_rects[i].x += new_rects[0].width;
				new_rects[i].width -= new_rects[0].width;
			}

			if (!new_rects[2].IsEmpty())
				new_rects[i].width -= new_rects[2].width;
		}

	// Finally add the new rects
	for (int i=0; i<4; i++)
		if (!(new_rects[i]).IsEmpty())
		{
			RectLink* new_link = OP_NEW(RectLink, (new_rects[i], to_divide->transform_tree));
			if (!new_link)
				return OpStatus::ERR_NO_MEMORY;

			new_link->transform_tree->ref_counter++;
			new_link->Into(&rects);
		}

	return OpStatus::OK;
}

/*virtual*/UINT32
ElementCollectingObject::ElementHashFunctions::Hash(const void* key)
{
	const ElementKey* element = reinterpret_cast<const ElementKey*>(key);
	return static_cast<UINT32>(reinterpret_cast<UINTPTR>(element->elm)) + static_cast<UINT32>(element->type);
}

/*virtual*/BOOL
ElementCollectingObject::ElementHashFunctions::KeysAreEqual(const void* key1, const void* key2)
{
	return *(reinterpret_cast<const ElementKey*>(key1)) == *(reinterpret_cast<const ElementKey*>(key2)) ;
}

void
ElementCollectingObject::RemoveElement(ElementKey* elm, ElementHashMap* hash)
{
	ElementValue* data;

	hash->Remove(elm, &data);
	elm->Out();
	OP_DELETE(elm);
	OP_DELETE(data);
}

/*virtual*/BOOL
ElementCollectingObject::HandleScrollbarFound(HTML_Element* element, BOOL vertical, const OpRect& rect)
{
	ElementKey potential_new(element, vertical ? DOCUMENT_ELEMENT_TYPE_VERTICAL_SCROLLBAR :	DOCUMENT_ELEMENT_TYPE_HORIZONTAL_SCROLLBAR);

	// have to check, because during traverse we may enter the same scrollable multiple times
	if (!m_elm_hash->Contains(&potential_new))
	{
		ElementKey *key;
		ElementValue* data;

		key = OP_NEW(ElementKey, (potential_new));
		if (!key)
		{
			SetOutOfMemory();
			return FALSE;
		}

		data = OP_NEW(ElementValue, ());
		if (!data)
		{
			OP_DELETE(key);
			SetOutOfMemory();
			return FALSE;
		}

		if (OpStatus::IsMemoryError(m_elm_hash->Add(key, data)))
		{
			OP_DELETE(key);
			OP_DELETE(data);
			SetOutOfMemory();
			return FALSE;
		}

		key->Into(&m_elm_list);

		if (OpStatus::IsMemoryError(data->AddRect(rect, m_current_transform)))
		{
			SetOutOfMemory();
			return FALSE;
		}
	}

	return TRUE;
}

/*virtual*/BOOL
ElementCollectingObject::HandleElementFound(HTML_Element* html_element, const OpRect& rect, BOOL extended_area)
{
	ElementKey potential_new(html_element, DOCUMENT_ELEMENT_TYPE_HTML_ELEMENT);
	ElementHashMap* hash_to_check = extended_area ? m_extended_hash : m_elm_hash;
	ElementHashMap*	second_hash = IsUsingExtendedArea() ? (!extended_area ? m_extended_hash : m_elm_hash) : NULL;
	ElementValue* data;
	ElementKey* key = NULL;

	if (hash_to_check->GetData(&potential_new, &data) != OpStatus::OK) // Not found, create a new key, value pair and insert them into collections
	{
		key = OP_NEW(ElementKey, (potential_new));
		if (!key)
		{
			SetOutOfMemory();
			return FALSE;
		}

		data = OP_NEW(ElementValue, ());
		if (!data)
		{
			OP_DELETE(key);
			SetOutOfMemory();
			return FALSE;
		}

		if (OpStatus::IsMemoryError(hash_to_check->Add(key, data)))
		{
			OP_DELETE(key);
			OP_DELETE(data);
			SetOutOfMemory();
			return FALSE;
		}

		key->Into(extended_area ? m_extended_list : &m_elm_list);
	}

	BOOL add_new_rect = TRUE, covered_other = FALSE;
	RectLink* rect_link = data->rects.First();
	OpRect rect_translated = rect;
	rect_translated.OffsetBy(-translation_x, -translation_y); // Offseting (translating), because using visual_device_transform for inclusion checks

	/** Check the hash, where the rect was inserted to, for rectangles that are fully inside the new one
		or the rectangles that contain the new one. Remove the useless ones or mark the new one that it is not needed. */
	while (rect_link)
	{
		if (visual_device_transform.TestInclusionOfLocal(rect_translated, rect_link->transform_tree->m_transform, rect_link->rect))
		{
			add_new_rect = FALSE;
			break;
		}
		else if (visual_device_transform.TestInclusion(rect_translated, rect_link->transform_tree->m_transform, rect_link->rect))
		{
			RectLink* to_remove = rect_link;
			rect_link = rect_link->Suc();

			OP_DELETE(to_remove); // also unchains
			covered_other = TRUE;
		}
		else
			rect_link = rect_link->Suc();
	}

	/** Check the other hash when needed. Rectangles from the main area hash may contain the rectangles from the extended area hash.
		The exclusions are - if the new rect has been already marked as being fully inside some other, it can't contain any other.
		If the new rect is in extended area and it contained some other, it can't be inside any rect from the main area, because
		in such case a pair of rects before adding the new one, would be in inclusion relation (integrity is always preserved). */
	if (add_new_rect && !(extended_area && covered_other) && second_hash)
	{
		ElementValue* data_in_second;
		if (second_hash->GetData(&potential_new, &data_in_second) == OpStatus::OK)
		{
			rect_link = data_in_second->rects.First();

			if (extended_area)
			{
				while (rect_link)
				{
					if (visual_device_transform.TestInclusionOfLocal(rect_translated, rect_link->transform_tree->m_transform, rect_link->rect))
					{
						add_new_rect = FALSE;
						break;
					}
					rect_link = rect_link->Suc();
				}
			}
			else
			{
				while (rect_link)
				{
					if (visual_device_transform.TestInclusion(rect_translated, rect_link->transform_tree->m_transform, rect_link->rect))
					{
						RectLink* to_remove = rect_link;
						rect_link = rect_link->Suc();

						OP_DELETE(to_remove); // also unchains
					}
					else
						rect_link = rect_link->Suc();
				}

				// Can't store an element that has no rectangles at the moment
				if (data_in_second->rects.Empty())
				{
					// Find the exact key pointer to remove and do it
					ElementKey* iter = static_cast<ElementKey*>(m_extended_list->First());
					while (iter)
					{
						if (*iter == potential_new)
						{
							RemoveElement(iter, second_hash);
							break;
						}
						iter = iter->Suc();
					}
				}
			}
		}
	}

	if (add_new_rect)
	{
		if (OpStatus::IsMemoryError(data->AddRect(rect, m_current_transform)))
		{
			SetOutOfMemory();
			return FALSE;
		}
	}
	else if (extended_area && key) // the first rect of that element was reported in the extended area and it was discarded
		RemoveElement(key, hash_to_check);

	return TRUE;
}

/*virtual*/BOOL
ElementCollectingObject::HandleOverlappingRect(HTML_Element* element, const OpRect& overlapping_rect, BOOL extended_area)
{
	ElementHashMap* hash_to_check = m_elm_hash;
	AutoDeleteHead* list_to_check = &m_elm_list;

	/** Ugly. Have to offset the rect again to the current translation, because using
		visual_device_transform later for inclusion testing. */
	OpRect overlapping_rect_translated = overlapping_rect;
	overlapping_rect_translated.OffsetBy(-translation_x, -translation_y);

	OpacityMarker* opacity_marker = m_opacity_markers ? static_cast<OpacityMarker*>(m_opacity_markers->Last()) : NULL;

	do
	{
		ElementKey* iter;

		/** If we have a marker, set the starting element to be the first after the marked one.
			The elements before needn't be checked, because they are not visually covered by elements with some transparency against them. */
		if (opacity_marker)
		{
			if (list_to_check == m_extended_list)
				iter = opacity_marker->extended_key ? opacity_marker->extended_key->Suc() : static_cast<ElementKey*>(list_to_check->First());
			else
				iter = opacity_marker->main_key ? opacity_marker->main_key->Suc() : static_cast<ElementKey*>(list_to_check->First());
		}
		else // No marker, simply check the whole list
			iter = static_cast<ElementKey*>(list_to_check->First());

		// Ugly in terms of complexity. Checking every element and every rect of it, whether it is covered, possibly computing the remaining part.
		while (iter)
		{
			BOOL something_left = TRUE;
			ElementValue* data;

			if (!iter->elm->IsAncestorOf(element)) // An element cannot be covered by it's descendant
			{
				// There must be ElementValue for each element in the list
				OP_ASSERT(hash_to_check->Contains(iter));
				hash_to_check->GetData(iter, &data);

				RectLink *rect_link = data->rects.First(), *last_link = data->rects.Last();
				BOOL finish_loop;

				do // check every rect (not counting the possible new ones that come from a division of partially covered rect)
				{
					finish_loop = (rect_link == last_link);

					OpRect translated_rect;
					BOOL remove;

					if (visual_device_transform.TestInclusion(overlapping_rect_translated, rect_link->transform_tree->m_transform, rect_link->rect, &translated_rect))
						remove = TRUE; // whole rect is covered
					// not the whole rect is covered and we have the covering rect in rect_link->transform_tree->m_transform coordinates
					else if (!translated_rect.IsEmpty())
					{
						if (translated_rect.Intersecting(rect_link->rect))
						{
							if (OpStatus::IsMemoryError(data->DivideRect(rect_link, translated_rect)))
							{
								SetOutOfMemory();
								return FALSE;
							}
							remove = TRUE;
						}
						else
							remove = FALSE;
					}
					else
						remove = FALSE; // rect_link->rect not fully covered and the translated_rect is empty, so nothing will be covered

					if (remove)
					{
						RectLink* to_remove = rect_link;
						rect_link = rect_link->Suc();

						OP_DELETE(to_remove); // also unchains
					}
					else
						rect_link = rect_link->Suc();

				} while (!finish_loop);

				something_left = !data->rects.Empty();
			}

			// Remove the element that has no rects
			if (!something_left)
			{
				ElementKey* to_remove = iter;
				iter = iter->Suc();

				RemoveElement(to_remove, hash_to_check);
			}
			else
				iter = iter->Suc();
		}

		BOOL check_extended = FALSE;

		// Check whether we need to check against the rectangles that intersect the extended area only
		if (IsUsingExtendedArea() && hash_to_check != m_extended_hash)
		{
			if (extended_area)
				check_extended = TRUE;
			else
			{
				// Main area must be non empty when the reported opaque rect is intersecting it
				OP_ASSERT(GetMainArea()->left < GetMainArea()->right && GetMainArea()->top < GetMainArea()->bottom);

				OpRect main_area_op_rect(GetMainArea());

				/**	The only case when we can limit the iteration only to the elements that are intersecting
					the main area is that the overlapping_rect is fully inside the main area. */
				check_extended = !visual_device_transform.TestInclusionOfLocal(overlapping_rect, NULL, main_area_op_rect);
			}
		}

		if (check_extended)
		{
			hash_to_check = m_extended_hash;
			list_to_check = m_extended_list;
		}
		else
			break;

	} while (TRUE);

	return TRUE;
}

/*virtual*/BOOL
ElementCollectingObject::BeginOpacity()
{
	if (!m_opacity_markers)
	{
		m_opacity_markers = OP_NEW(AutoDeleteHead, ());
		if (!m_opacity_markers)
		{
			SetOutOfMemory();
			return FALSE;
		}
	}

	OpacityMarker* marker = OP_NEW(OpacityMarker, ());
	if (!marker)
	{
		SetOutOfMemory();
		return FALSE;
	}

	/** Mark the end of the currently found elements when entering an opacity context
		All the elements that currently exist can't be visually covered until leaving that context. */
	marker->main_key = static_cast<ElementKey*>(m_elm_list.Last());
	marker->extended_key = IsUsingExtendedArea() ? static_cast<ElementKey*>(m_extended_list->Last()) : NULL;

	marker->Into(m_opacity_markers);

	return TRUE;
}

/*virtual*/void
ElementCollectingObject::EndOpacity()
{
	// There must have been a successful BeginOpacity before
	OP_ASSERT(m_opacity_markers);

	OpacityMarker* marker = static_cast<OpacityMarker*>(m_opacity_markers->Last());

	// The number of EndOpacity calls should not exceed the number of BeginOpacity and should be after them
	OP_ASSERT(marker);

	marker->Out();
	OP_DELETE(marker);
}

#ifdef CSS_TRANSFORMS

/*virtual*/OP_BOOLEAN
ElementCollectingObject::PushTransform(const AffineTransform &t, TranslationState &state)
{
	OP_BOOLEAN status = ElementSearchObject::PushTransform(t, state);

	switch (status)
	{
	case OpBoolean::ERR_NO_MEMORY:
		TerminateTraverse();
	case OpBoolean::IS_FALSE:
		return status;
	}

	// Create a new node and put it under the current one
	AffinePos* new_affine_pos = OP_NEW(AffinePos, (transform_root));
	if (!new_affine_pos)
	{
		TerminateTraverse();
		ElementSearchObject::PopTransform(state);
		return OpBoolean::ERR_NO_MEMORY;
	}

	TransformTree* new_transform = OP_NEW(TransformTree, (new_affine_pos));
	if (!new_transform)
	{
		OP_DELETE(new_affine_pos);
		TerminateTraverse();
		ElementSearchObject::PopTransform(state);
		return OpBoolean::ERR_NO_MEMORY;
	}

	new_transform->Under(m_current_transform);
	m_current_transform = new_transform;

	return OpBoolean::IS_TRUE;
}

/*virtual*/void
ElementCollectingObject::PopTransform(const TranslationState &state)
{
	ElementSearchObject::PopTransform(state);

	m_current_transform = m_current_transform->Parent();
}

#endif //CSS_TRANSFORMS


OP_STATUS
ElementCollectingObject::Init()
{
	m_elm_hash = OP_NEW(ElementHashMap, (&m_hash_functions));

	if (!m_elm_hash)
		return OpStatus::ERR_NO_MEMORY;

	if (IsUsingExtendedArea())
	{
		m_extended_hash = OP_NEW(ElementHashMap, (&m_hash_functions));

		if (!m_extended_hash)
			return OpStatus::ERR_NO_MEMORY;

		m_extended_list = OP_NEW(AutoDeleteHead, ());

		if (!m_extended_list)
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

ElementCollectingObject::~ElementCollectingObject()
{
	if (m_elm_hash)
	{
		m_elm_hash->DeleteAll();
		OP_DELETE(m_elm_hash);
	}

	if (m_extended_hash)
	{
		m_extended_hash->DeleteAll();
		OP_DELETE(m_extended_hash);
	}

	OP_DELETE(m_extended_list);
	OP_DELETE(m_opacity_markers);
}

OP_STATUS
ElementCollectingObject::CollectElements(DocumentElementCollection& collection)
{
	ElementKey* iter;

	/** Check all elements in extended area, whether some other part of them was found in the main area
		and add the rectangles from extended area if so. */
	if (IsUsingExtendedArea())
	{
		iter = static_cast<ElementKey*>(m_extended_list->First());

		while (iter)
		{
			ElementValue *data_in_extended, *data_in_proper;

			OP_ASSERT(m_extended_hash->Contains(iter));
			m_extended_hash->GetData(iter, &data_in_extended);

			if (m_elm_hash->GetData(iter, &data_in_proper) == OpStatus::OK)
				data_in_proper->MergeRects(data_in_extended->rects);

			iter = iter->Suc();
		}

		m_extended_hash->DeleteAll();
		OP_DELETE(m_extended_hash);
		m_extended_hash = NULL;
		OP_DELETE(m_extended_list);
		m_extended_list = NULL;
	}

	// Put all the elements into the element vector of the ElementCollection
	iter = static_cast<ElementKey*>(m_elm_list.First());

	if (iter)
	{
		collection.SetAllocationStepSize(m_elm_list.Cardinal());

		do
		{
			ElementValue* elm_data;
			OpDocumentElement* new_element = OP_NEW(OpDocumentElement, (iter->elm, iter->type));

			if (!new_element)
				return OpStatus::ERR_NO_MEMORY;

			OP_ASSERT(m_elm_hash->Contains(iter));
			m_elm_hash->GetData(iter, &elm_data);

			OP_ASSERT(elm_data->rects.Cardinal()); // we do not allow elements that have no rects
			new_element->region.SetAllocationStepSize(elm_data->rects.Cardinal());

			RectLink* rect_link = elm_data->rects.First();
			while (rect_link)
			{
				OpTransformedRect *rect = OP_NEW(OpTransformedRect, ());

				if (!rect)
				{
					OP_DELETE(new_element);
					return OpStatus::ERR_NO_MEMORY;
				}

				rect->rect = rect_link->rect;
				rect->affine_pos = rect_link->transform_tree->m_transform;

				if (OpStatus::IsMemoryError(new_element->region.Add(rect)))
				{
					OP_DELETE(rect);
					OP_DELETE(new_element);
					return OpStatus::ERR_NO_MEMORY;
				}

				rect_link = rect_link->Suc();
			}

			if (OpStatus::IsMemoryError(collection.Add(new_element)))
			{
				OP_DELETE(new_element);
				return OpStatus::ERR_NO_MEMORY;
			}

			iter = iter->Suc();
		} while (iter);
	}

	// Move all the referenced transforms pointers to the AffinePos vector of ElementCollection
	TransformTree* tree_iter = m_transform_tree.FirstChild();
	UINT referenced_transforms_count = 0;

	while (tree_iter)
	{
		if (tree_iter->ref_counter)
			referenced_transforms_count++;

		tree_iter = tree_iter->Next();
	}

	if (referenced_transforms_count)
	{
		collection.transforms.SetAllocationStepSize(referenced_transforms_count);
		tree_iter = m_transform_tree.FirstChild();

		while (tree_iter)
		{
			if (tree_iter->ref_counter)
			{
				OP_ASSERT(tree_iter->m_transform); // Every node, except the top one, must have a valid AffinePos pointer

				if (OpStatus::IsMemoryError(collection.transforms.Add(tree_iter->m_transform)))
					return OpStatus::ERR_NO_MEMORY;
				tree_iter->m_transform = NULL;
			}

			tree_iter = tree_iter->Next();
		}
	}

	m_elm_list.Clear();
	m_elm_hash->DeleteAll();
	OP_DELETE(m_elm_hash);
	m_elm_hash = NULL;

	m_transform_tree.Clear();

	return OpStatus::OK;
}

/** Takes element_character_offset and calculates the word based offset. */

void
SelectionPointWordInfo::CalculateWordBasedOffset(const HTML_Element* element, unsigned int offset, BOOL prefer_word_before_boundary)
{
	if (!element ||
	    element->Type() != Markup::HTE_TEXT ||
	    !element->GetLayoutBox() ||
	    !element->GetLayoutBox()->IsTextBox())
	{
		/* Calculating a word-based offset doesn't make much sense for the
		   element. */
		m_word = NULL;
		m_word_character_offset = offset;

		return;
	}

	Text_Box* box = (Text_Box*) element->GetLayoutBox();
	WordInfo* words = box->GetWords();
	INT32 wordcount = box->GetWordCount();

	if (wordcount == 0)
	{
		m_word = NULL;
		m_word_character_offset = 0;
		return;
	}

	// Locate the word relative to which to calculate the offset.
	int i;
	for (i = 0; i < wordcount; i++)
	{
		if (offset < words[i].GetStart() + words[i].GetLength() ||
		    i == wordcount - 1) // last word, if this check triggers, we're probably in collapsed whitespace after the last word
		{
			/* Check if we should calculate the offset relative to the
			   preceding word instead of this one. */
			if (i > 0)
			{
				// Between words?
				if (offset < words[i].GetStart())
				{
					--i;

					OP_ASSERT(words[i].HasTrailingWhitespace() ||
					          words[i].GetStart() + words[i].GetLength() > offset);
				}

				// See SetPreferWordBeforeBoundary().
				else if (prefer_word_before_boundary &&
				         offset == words[i].GetStart())
				{
					--i;
				}
			}

			break;
		}
	}

	/* 'rel_word_info' is the word relative to which to calculate the
	   offset. */
	const WordInfo& rel_word_info = words[i];

	OP_ASSERT(offset >= rel_word_info.GetStart());

	const uni_char* new_word = element->TextContent() + rel_word_info.GetStart();
	int new_word_character_offset = offset - rel_word_info.GetStart();

	m_word = new_word;
	m_word_character_offset = new_word_character_offset;
}

/** Converts a "standard" selection point to one suitable for layout. */

/* static */ LayoutSelectionPoint
LayoutSelectionPointExtended::LayoutAdjustSelectionPoint(const SelectionBoundaryPoint& selection_point, BOOL adjust_direction_forward, BOOL require_text)
{
	HTML_Element* elm;
	int offset;
	TextSelection::ConvertPointToOldStyle(selection_point, elm, offset);

	if (elm)
	{
		OP_ASSERT(!elm->Root()->IsDirty());

		BOOL forward = (offset == 0);
		HTML_Element* base_elm = elm;
		int base_offset = offset;

		while (elm)
		{
			if (elm->GetLayoutBox())
			{
				/* Our selection code can handle text and replaced content as end points, nothing else at the moment. */

				if (!require_text && elm->GetLayoutBox()->IsContentReplaced())
					break;
				if (elm->Type() == Markup::HTE_TEXT || elm->IsMatchingType(Markup::HTE_BR, NS_HTML))
					break;
			}
			elm = forward ? elm->NextActualStyle() : elm->PrevActualStyle();
			offset = 0;
			if (elm && !forward)
				offset = elm->Type() == Markup::HTE_TEXT ? elm->ContentLength() : 1;
		}

		if (!elm)
		{
			/* If we couldn't find a good layout adapted point, fallback to the next best thing. */

			elm = base_elm;
			offset = base_offset;
		}
	}

	return LayoutSelectionPoint(elm, offset);
}

/* Do the calculations necessary for adjusting the selection points. This must be called after a Reflow() but before the
   object is actually used. TraversalObject::StartTraversing() is a good place. */

void
LayoutSelectionPointExtended::AdjustForTraverse()
{
#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(!m_adjusted || !"LayoutSelectionPointExtended::AdjustForTraverse() is called twice. Once is enough");
	m_adjusted = TRUE;
#endif // DEBUG_ENABLE_OPASSERT
	m_adjusted_selection_point = LayoutAdjustSelectionPoint(m_original_selection_point, m_adjust_direction_forward, m_require_text);
	m_word_info = SelectionPointWordInfo(m_adjusted_selection_point, m_prefer_word_before_boundary);
}

/* virtual */ void
TextSelectionObject::HandleTextContent(LayoutProperties* layout_props,
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
	HTML_Element* html_element = layout_props->html_element;

	// Elements which are meant to be invisible to the external world can't be selection boundary points.
	if (html_element->Parent()->GetIsListMarkerPseudoElement() ||
		!html_element->IsIncludedActualStyle())
		return;

	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (selection_container_element &&
		html_element != selection_container_element &&
		!selection_container_element->IsAncestorOf(html_element))
		return;

	if (*word == 173) // soft hyphen (&shy;)
		return;

	if (only_alphanum && !uni_isalnum(*word) && *word != '\'')
		return;

	RECT local_focus_area = {0, 0, 0, 0};
	if (!find_nearest)
		local_focus_area = GetLocalSelectionFocusArea();

	if (find_nearest || (local_focus_area.bottom > MIN(baseline - props.font_ascent, 0) &&
		local_focus_area.top < MAX(baseline + props.font_descent, line_height)))
	{
		LayoutCoord distance_x(0);

#ifdef LAYOUT_CARET_SUPPORT
		if (document->GetCaretPainter() && html_element->Parent()->GetIsPseudoElement())
			// Don't handle generated text in editable mode. It shouldn't be edited.
			return;
#endif // LAYOUT_CARET_SUPPORT

		OpPoint local_previous_position = visual_device_transform.ToLocal(OpPoint(previous_x, previous_y));

		if (local_previous_position.x < x)
			distance_x = LayoutCoord(x - local_previous_position.x);
		else
			if (local_previous_position.x > x + word_width)
				distance_x = LayoutCoord(local_previous_position.x - (x + word_width));

		LayoutCoord distance(0);
		if (find_nearest)
		{
			LayoutCoord distance_y(0);
			if (local_previous_position.y < baseline)
				distance_y = LayoutCoord(baseline - local_previous_position.y);
			else
				if (local_previous_position.y > baseline)
					distance_y = LayoutCoord(local_previous_position.y - baseline);

			distance = LayoutSqrt(distance_x * distance_x + distance_y * distance_y);
		}
		else
			distance = distance_x;

		if (distance <= nearest_box_distance)
		{
			nearest_element = html_element;
			nearest_word_element = html_element;
			nearest_line = line;
			nearest_word = word;
			nearest_box_distance = distance;
			nearest_box_x = virtual_pos;

			if (local_previous_position.x >= x)
				if (local_previous_position.x > x + word_width)
					nearest_box_x += word_width;
				else
					nearest_box_x += LayoutCoord(local_previous_position.x - x);

			// Find word character offset
			int word_pixel_offset = local_previous_position.x - x;

#ifdef SUPPORT_TEXT_DIRECTION
				if (!segment.left_to_right)
					word_pixel_offset = word_width - word_pixel_offset;
#endif

			if (word_pixel_offset <= 0)
			{
				is_at_end_of_line = FALSE;

				nearest_word_character_offset = 0; // before first character (of word)
				nearest_offset = word - html_element->Content();
			}
			if (word_pixel_offset >= word_width)
			{
				/* We are to the right of the word, so tentatively set
				   is_at_end_of_line to TRUE. If this is the last word, it will
				   remain set. */
				is_at_end_of_line = TRUE;

				nearest_word_character_offset = word_length; // after last character (of word)
				nearest_offset = word - html_element->Content() + word_length;
			}
			else
			{
				is_at_end_of_line = FALSE;

				int text_format = GetTextFormat(props, word_info);

#ifdef SUPPORT_TEXT_DIRECTION
				const uni_char* word_end = word + word_length;

				if (!segment.left_to_right)
					text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif

				VisualDevice* visual_device = document->GetVisualDevice();
				LayoutCoord word_part_width = word_width;
				int word_part_length = word_length;
				int consumed = 0;
				while (word_part_length >= 0)
				{
					LayoutCoord prev_word_part_width = word_part_width;
					int last_consumed = consumed;
					consumed = 1;

#ifdef SUPPORT_TEXT_DIRECTION
					if (!segment.left_to_right)
					{
						if (word_part_length > 0 && Unicode::IsHighSurrogate(*(word_end - word_part_length)))
							++consumed;

						// Calculcate from behind and include the trailing space
						word_part_width = MeasureSubstring(visual_device,
														   word_end - word_part_length,
														   word_part_length,
														   word_width,
														   text_format,
														   word_info.IsTabCharacter());
					}
					else
#endif
					{
						if (word_part_length > 0 && Unicode::IsLowSurrogate(*(word + word_part_length - 1)))
							++consumed;

						word_part_width = MeasureSubstring(visual_device,
														   word,
														   word_part_length,
														   word_width,
														   text_format,
														   word_info.IsTabCharacter());
					}

					if (word_pixel_offset >= word_part_width)
					{
						// Stop searching

						if (prev_word_part_width - word_pixel_offset < word_pixel_offset - word_part_width)
						{
							// Previous point was closer

							word_part_length += last_consumed;
							word_part_width = prev_word_part_width;
						}

						break;
					}
					else if (word_part_length == 0)
						break;

					word_part_length -= consumed;
				}

				nearest_word_character_offset = word_part_length;
				nearest_offset = word - html_element->Content() + word_part_length;

				if (word_part_length == word_length)
				{
					/* We are precisely after the last character in the word,
					   so tentatively set is_at_end_of_line to TRUE. If this is
					   the last word, it will remain set. */
					is_at_end_of_line = TRUE;
				}
			}
		}
	}
}

/* virtual */ void
TextSelectionObject::HandleSelectableBox(LayoutProperties* layout_props)
{
#ifdef LAYOUT_CARET_SUPPORT
	if (!document->GetCaretPainter())
		return;

	const HTMLayoutProperties& props = *layout_props->GetProps();
	HTML_Element* html_element = layout_props->html_element;
	Box* box = html_element->GetLayoutBox();

	// Elements which are meant to be invisible to the external world can't be selection boundary points.
	if (!html_element->IsIncludedActualStyle())
		return;

	if (selection_container_element &&
		html_element != selection_container_element &&
		!selection_container_element->IsAncestorOf(html_element))
		return;

	if (html_element->Type() == Markup::HTE_BR && !only_alphanum)
	{
		RECT local_focus_area = GetLocalSelectionFocusArea();
		int line_height = props.GetCalculatedLineHeight(document->GetVisualDevice());
		LayoutCoord distance(0);
		int height = (line_height + 1) / 2;
		int y = height - 1;

		if (local_focus_area.top < y)
			distance = LayoutCoord(y - local_focus_area.top);
		else
			if (local_focus_area.top > y)
				distance = LayoutCoord(local_focus_area.top - y);

		if (distance < nearest_box_distance && distance < height)
		{
			nearest_element = html_element;
			nearest_line = line;
			nearest_box_distance = distance;
			nearest_box_x = LayoutCoord(0);
			nearest_offset = LayoutCoord(0);
		}
	}
	else
	{
		LayoutCoord x(0);
		LayoutCoord y(GetTranslationY());
		LayoutCoord distance_x(0);
		LayoutCoord width = box->GetWidth();
		LayoutCoord height = box->GetHeight();

		if (orig_y < GetTranslationY() || orig_y >= GetTranslationY() + height)
			return;

		OpPoint local_previous_position(visual_device_transform.ToLocal(OpPoint(previous_x, previous_y)));

		if (local_previous_position.x < x)
			distance_x = x - LayoutCoord(local_previous_position.x);
		else
			if (local_previous_position.x > x + width)
				distance_x = LayoutCoord(local_previous_position.x) - (x + width);

		LayoutCoord distance(0);
		if (find_nearest)
		{
			LayoutCoord distance_y(0);
			if (local_previous_position.y < y)
				distance_y = y - LayoutCoord(local_previous_position.y);
			else
				if (local_previous_position.y > y + height)
					distance_y = LayoutCoord(local_previous_position.y) - (y + height);

			distance = LayoutSqrt(distance_x * distance_x + distance_y * distance_y);
		}
		else
			distance = distance_x;

		if (distance <= nearest_box_distance)
		{
			nearest_offset = local_previous_position.x > width/2 ? 1 : 0;
			BOOL first_on_line = (!nearest_element || (nearest_offset == 0 && nearest_element && line != nearest_line));
			if (first_on_line || nearest_offset == 1)
			{
				nearest_element = html_element;
				nearest_line = line;
				nearest_box_distance = distance;
				nearest_box_x = box->GetVirtualPosition();

				if (local_previous_position.x >= x)
					if (local_previous_position.x > x + width)
						nearest_box_x += width;
					else
						nearest_box_x += LayoutCoord(local_previous_position.x) - x;
			}
		}
	}
#endif // LAYOUT_CARET_SUPPORT
}

/** Enter line */

BOOL
TextSelectionObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	this->line = line;

	return HitTestingTraversalObject::EnterLine(parent_lprops, line, containing_element, traverse_info);
}

/** Leave line */

void
TextSelectionObject::LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	this->line = NULL;

	HitTestingTraversalObject::LeaveLine(parent_lprops, line, containing_element, traverse_info);
}

/* virtual */ void
SelectionUpdateObject::HandleSelectableBox(LayoutProperties* layout_props)
{
	BOOL update = FALSE;
	VisualDevice* visual_device = GetVisualDevice();
	HTML_Element* html_element = layout_props->html_element;
	Box* box = html_element->GetLayoutBox();
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (!start_box_done)
	{
		if (html_element == start_update_range.GetElement())
		{
			start_box_done = TRUE;
			update = TRUE;
		}
	}
	if (start_box_done)
	{
		if (!end_box_done)
		{
			if (start_update_range != end_update_range)
			{
				update = TRUE;
				if (html_element == end_update_range.GetElement())
					end_box_done = TRUE;
			}
			else
				end_box_done = TRUE;
		}
	}
	if (update)
	{
		int line_height, width, height;
		if (html_element->Type() == Markup::HTE_BR)
		{
			line_height = props.GetCalculatedLineHeight(visual_device);
			width = SEL_BR_WIDTH;
			height = line_height;
		}
		else
		{
			line_height = box->GetHeight();
			width = box->GetWidth();
			height = box->GetHeight();
		}

		visual_device->UpdateRelative(0, 0, width, height, FALSE);
	}
}

SelectionUpdateObject::SelectionUpdateObject(FramesDocument* doc, const SelectionBoundaryPoint* new_start, const SelectionBoundaryPoint* new_end, BOOL calc_bounding_rect)
  : VisualTraversalObject(doc),
	start_update_range(*new_start, FALSE, FALSE, FALSE),
	end_update_range(*new_end, FALSE, TRUE, FALSE),
	start_box_done(new_start->GetElement() == NULL),
	end_box_done(new_end->GetElement() == NULL),
	calculate_bounding_rect(calc_bounding_rect)
{
	SetTraverseType(TRAVERSE_ONE_PASS);
	SetEnterHidden(TRUE);

	if (calc_bounding_rect)
	{
		bounding_rect.left = LONG_MAX;
		bounding_rect.top = LONG_MAX;
		bounding_rect.right = LONG_MIN;
		bounding_rect.bottom = LONG_MIN;
	}

	if (new_start->GetElement())
		next_container_element = doc->GetLogicalDocument()->GetRoot();
}

/* virtual */ BOOL
SelectionUpdateObject::EnterLayoutBreak(HTML_Element* break_element)
{
#ifdef LAYOUT_CARET_SUPPORT
	if (!target_element && document->GetCaretPainter())
		return TRUE;
#endif // LAYOUT_CARET_SUPPORT

	if (end_box_done)
		return FALSE;

	BOOL enter = start_box_done;

	if (!enter)
		enter = break_element == start_update_range.GetElement();

	return enter;
}

/* virtual */ void
SelectionUpdateObject::HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break)
{
	if (!start_box_done && layout_props->html_element == start_update_range.GetElement())

		/* Not much to do except record that the start box has been found */

		start_box_done = TRUE;
}

/** Handle text content */

/* virtual */ void
SelectionUpdateObject::HandleTextContent(LayoutProperties* layout_props,
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
	OP_NEW_DBG("SelectionUpdateObject::HandleTextContent\n", "selection");
	const HTMLayoutProperties& props = *layout_props->GetProps();
	BOOL update = FALSE;
	VisualDevice* visual_device = GetVisualDevice();
	LayoutCoord justified_space_width = space_width + justified_space_extra;
	LayoutCoord start_selection_x(0);
	LayoutCoord end_selection_x = word_width + justified_space_width;
	HTML_Element* html_element = layout_props->html_element;

#ifdef SUPPORT_TEXT_DIRECTION
	if (!segment.left_to_right)
		x -= justified_space_width;
#endif

	OP_ASSERT(start_box_done || !start_update_range.GetElement()->Precedes(html_element) || !"We've passed the start selection point without noticing");

	if (!start_box_done)
		if (html_element == start_update_range.GetElement())
			if (start_update_range.GetWord() == word)
			{
				// This is start of changed selection area
				start_box_done = TRUE;

				LayoutCoord non_selection_width(0);

				int text_format = GetTextFormat(props, word_info);
#ifdef SUPPORT_TEXT_DIRECTION
				if (!segment.left_to_right)
					text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif
				// Number of characters into the word
				int length = start_update_range.GetElementCharacterOffset() - word_info.GetStart();

				if (length <= 0)
					/* If less than zero, we are probably in collapsed whitespace before the
					   word. If equal to zero, we know that the selection starts at the
					   beginning. */
					non_selection_width = LayoutCoord(0);
				else if (length == word_length)
					non_selection_width = word_width;
				else if (length > word_length)
					non_selection_width = end_selection_x; // 'end_selection_x' can include white-space, 'word_length' does not
				else
					non_selection_width = LayoutCoord(visual_device->GetTxtExtentEx(word, length, text_format));

				// Start of update area

				OP_DBG(("non_selection_width: %d", int(non_selection_width)));

#ifdef SUPPORT_TEXT_DIRECTION
				if (!segment.left_to_right)
					end_selection_x = word_width - non_selection_width + justified_space_width;
				else
#endif
					start_selection_x = non_selection_width;
			}

	if (start_box_done)
		// Now we are inside the selected area

		if (!end_box_done)
			// The first time 'start_selection' equals 'end_selection',
			// this is a special case so drop the following actions

			if (start_update_range != end_update_range)
			{
				update = TRUE;

				if (html_element == end_update_range.GetElement())
				{
					if (end_update_range.GetWord() == word)
					{
						// Now we have found the end of the changed area

						LayoutCoord selected_width = word_width + justified_space_width;

						end_box_done = TRUE;

						int text_format = GetTextFormat(props, word_info);
#ifdef SUPPORT_TEXT_DIRECTION
						if (!segment.left_to_right)
							text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif
						// Number of characters into the word
						const int length = end_update_range.GetElementCharacterOffset() - word_info.GetStart();

						if (length <= 0)
							/* If less than zero, we are probably in collapsed whitespace before the
							   word. Might be a problem with GetStartOfWord(). If equal to zero, we
							   know that the selected width is zero. */
							selected_width = LayoutCoord(0);
						else if (length == word_length)
							selected_width = word_width;
						else if (length >= word_length)
							selected_width = word_width + justified_space_width; // Include trailing whitespace
						else
							selected_width = LayoutCoord(visual_device->GetTxtExtentEx(word, length, text_format));

						// End of update area

#ifdef SUPPORT_TEXT_DIRECTION
						if (!segment.left_to_right)
							start_selection_x = word_width - selected_width;
						else
#endif
							end_selection_x = selected_width;
					}
				}
			}
			else
				end_box_done = TRUE;

	if (update)
	{
		// Update changed area

		LayoutCoord update_width;

		OP_ASSERT(start_selection_x <= end_selection_x);
		update_width = end_selection_x - start_selection_x;

		OP_DBG(("Update text selection x: %d start_selection_x: %d update_width: %d", int(x), int(start_selection_x), int(update_width)));

		visual_device->UpdateRelative(x + start_selection_x,
									  baseline - props.font_ascent,
									  update_width,
									  props.font_ascent + props.font_descent + 1, FALSE);

		if (calculate_bounding_rect)
		{
			// Make a union between the current bounding box and the box we've calculated for this word
			OpRect local_word_rect;
			local_word_rect.x = x + start_selection_x;
			local_word_rect.y = baseline - props.font_ascent;
			local_word_rect.width = end_selection_x - start_selection_x;
			local_word_rect.height = props.font_descent + props.font_ascent + 1;

			OpRect this_word_rect = visual_device->ToBBox(local_word_rect);

			if (bounding_rect.left > this_word_rect.Left())
				bounding_rect.left = this_word_rect.Left();

			if (bounding_rect.right < this_word_rect.Right())
				bounding_rect.right = this_word_rect.Right();

			if (bounding_rect.top > this_word_rect.Top())
				bounding_rect.top = this_word_rect.Top();

			if (bounding_rect.bottom < this_word_rect.Bottom())
				bounding_rect.bottom = this_word_rect.Bottom();
		}
	}
}

/** Callback for objects needing to set up code before starting traversal */

/* virtual */ void
SelectionUpdateObject::StartingTraverse()
{
	start_update_range.AdjustForTraverse();
	end_update_range.AdjustForTraverse();
}

/** Enter vertical box */

/* virtual */ BOOL
SelectionUpdateObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (end_box_done)
		return FALSE;

	BOOL enter = start_box_done;

	if (!enter)
	{
		HTML_Element* html_element = box->GetHtmlElement();

		enter = html_element == next_container_element || html_element == start_update_range.GetElement();
	}

	if (enter)
	{
		if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
			return FALSE;

		if (!start_box_done)
			next_container_element = FindNextContainerElementOf(next_container_element, start_update_range.GetElement());

		return TRUE;
	}
	else
		return FALSE;
}

/** Enter inline box */

/* virtual */ BOOL
SelectionUpdateObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	HTML_Element* html_element = layout_props->html_element;

	if (html_element->GetIsListMarkerPseudoElement())
		return FALSE;

	if (!start_box_done && box->IsContainingElement())
		if (html_element == next_container_element)
		{
			next_container_element = FindNextContainerElementOf(next_container_element, start_update_range.GetElement());

			return TRUE;
		}
		else
			return FALSE;

	return !start_box_done || !end_box_done;
}

/** Enter line */

/* virtual */ BOOL
SelectionUpdateObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (end_box_done)
		return FALSE;

	if (!start_box_done)
	{
		LayoutCoord start(LAYOUT_COORD_MIN);
		LayoutCoord end(LAYOUT_COORD_MIN);

		if (!next_container_element)
		{
			Box* box = start_update_range.GetElement()->GetLayoutBox();

			if (box)
			{
				start = end = box->GetVirtualPosition();
				end += box->GetWidth();
			}

			if (start != LAYOUT_COORD_MIN)
				return (line->HasContentAfterEnd() || start <= line->GetEndPosition()) && (line->HasContentBeforeStart() || end >= line->GetStartPosition());
		}
		else
			if (next_container_element->GetLayoutBox()->TraverseInLine())
				end = start = next_container_element->GetLayoutBox()->GetVirtualPosition();

		if (start != LAYOUT_COORD_MIN)
			return line->MayIntersectLogically(start, end);
	}

	return TRUE;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
SelectionUpdateObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	return TRUE;
}

#endif // PAGED_MEDIA_SUPPORT

/** Enter table row. */

/* virtual */ BOOL
SelectionUpdateObject::EnterTableRow(TableRowBox* row)
{
	if (end_box_done || (!start_box_done && !row->GetHtmlElement()->IsAncestorOf(start_update_range.GetElement())))
		return FALSE;

	return TRUE;
}

/** Check if this positioned element needs to be traversed. */

/* virtual */ BOOL
SelectionUpdateObject::TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element)
{
	return !end_box_done && (start_box_done || element->IsAncestorOf(start_update_range.GetElement()));
}

CalcSelPointPositionObject::CalcSelPointPositionObject(FramesDocument* doc, const SelectionBoundaryPoint* in_sel_point, BOOL calc_bounding_rect)
  : VisualTraversalObject(doc),
    sel_point(*in_sel_point),
    sel_point_done(sel_point.GetElement() == NULL),
    calculate_bounding_rect(calc_bounding_rect)
{
	SetTraverseType(TRAVERSE_ONE_PASS);
	SetEnterHidden(TRUE);

	if (calc_bounding_rect)
	{
		bounding_rect.left = LONG_MAX;
		bounding_rect.top = LONG_MAX;
		bounding_rect.right = LONG_MIN;
		bounding_rect.bottom = LONG_MIN;
	}

	if (!sel_point_done)
		next_container_element = doc->GetLogicalDocument()->GetRoot();
}

/** Handle text content */

/* virtual */ void
CalcSelPointPositionObject::HandleTextContent(LayoutProperties* layout_props,
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
	OP_NEW_DBG("CalcSelPointPositionObject::HandleTextContent\n", "selection");
	const HTMLayoutProperties& props = *layout_props->GetProps();
	VisualDevice* visual_device = GetVisualDevice();
	LayoutCoord justified_space_width = space_width + justified_space_extra;
	LayoutCoord start_selection_x(0);
	LayoutCoord end_selection_x = word_width + justified_space_width;
	HTML_Element* html_element = layout_props->html_element;
#ifdef LAYOUT_CARET_SUPPORT
	int charw = 0;
#endif // LAYOUT_CARET_SUPPORT

#ifdef SUPPORT_TEXT_DIRECTION
	if (!segment.left_to_right)
		x -= justified_space_width;
#endif

	OP_ASSERT(sel_point_done || !sel_point.GetElement()->Precedes(html_element) || !"We've passed the sel_point point without noticing");

	if (sel_point_done || html_element != sel_point.GetElement())
		return;

	SelectionPointWordInfo sel_point_word_info(sel_point, sel_point.GetBindDirection() == SelectionBoundaryPoint::BIND_BACKWARD);
	if (sel_point_word_info.GetWord() == word)
	{
		/* We are at the right word. */

		sel_point_done = TRUE;

		LayoutCoord non_selection_width(0);

		int text_format = GetTextFormat(props, word_info);
#ifdef SUPPORT_TEXT_DIRECTION
		if (!segment.left_to_right)
			text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif
		/* Number of characters into the word. */

		int length = sel_point.GetOffset() - word_info.GetStart();

		if (length <= 0)
			/* If less than zero, we are probably in collapsed whitespace before the
				word. If equal to zero, we know that the selection starts at the
				beginning. */
			non_selection_width = LayoutCoord(0);
		else if (length == word_length)
			non_selection_width = word_width;
		else if (length > word_length)
			non_selection_width = end_selection_x; // 'end_selection_x' can include white-space, 'word_length' does not
		else
			non_selection_width = LayoutCoord(visual_device->GetTxtExtentEx(word, length, text_format));

#ifdef LAYOUT_CARET_SUPPORT
		if (length < word_length)
			charw = visual_device->GetTxtExtentEx(word + sel_point_word_info.GetOffsetIntoWord(), 1, GetTextFormat(props, word_info));
#endif // LAYOUT_CARET_SUPPORT

		sel_point_layout.SetVirtualPosition(non_selection_width + virtual_pos);

		OP_DBG(("non_selection_width: %d", int(non_selection_width)));

#ifdef SUPPORT_TEXT_DIRECTION
		if (!segment.left_to_right)
			end_selection_x = word_width - non_selection_width + justified_space_width;
		else
#endif
			start_selection_x = non_selection_width;
	}

#ifdef DEBUG
	LayoutCoord update_width;

	OP_ASSERT(start_selection_x <= end_selection_x);
	update_width = end_selection_x - start_selection_x;
#endif // DEBUG

#ifdef LAYOUT_CARET_SUPPORT
	int caret_x = 0;

	LayoutCoord end_virtual_pos = sel_point_layout.GetVirtualPosition();

	if (end_virtual_pos != LAYOUT_COORD_MIN)
		caret_x = end_virtual_pos - virtual_pos;

# ifdef SUPPORT_TEXT_DIRECTION
	if (!segment.left_to_right)
	{
		caret_x = word_width + justified_space_width - caret_x;

#  ifdef DOCUMENTEDIT_CHARACTER_WIDE_CARET
		caret_x -= charw;
#  endif // DOCUMENTEDIT_CHARACTER_WIDE_CARET
	}
# endif // SUPPORT_TEXT_DIRECTION

	sel_point_layout.SetCaretPixelOffset(caret_x);
	sel_point_layout.SetHeight(props.font_ascent + props.font_descent);
	sel_point_layout.SetLineTranslationY(GetTranslationY());
	sel_point_layout.SetLineHeight(line_height);
	sel_point_layout.SetPixelTranslationX(GetTranslationX() + x);
	sel_point_layout.SetPixelTranslationY(GetTranslationY() + baseline - props.font_ascent);
	sel_point_layout.SetTransformRoot(GetTransformRoot());
	sel_point_layout.SetCharacterWidth(charw);
#endif // LAYOUT_CARET_SUPPORT

#ifdef DEBUG
	OP_DBG(("Update text selection x: %d start_selection_x: %d update_width: %d", int(x), int(start_selection_x), int(update_width)));
#endif // DEBUG

	if (calculate_bounding_rect)
	{
		// Make a union between the current bounding box and the box we've calculated for this word

		OpRect local_word_rect;
		local_word_rect.x = x + start_selection_x;
		local_word_rect.y = baseline - props.font_ascent;
		local_word_rect.width = end_selection_x - start_selection_x;
		local_word_rect.height = props.font_descent + props.font_ascent + 1;

		OpRect this_word_rect = visual_device->ToBBox(local_word_rect);

		if (bounding_rect.left > this_word_rect.Left())
			bounding_rect.left = this_word_rect.Left();

		if (bounding_rect.right < this_word_rect.Right())
			bounding_rect.right = this_word_rect.Right();

		if (bounding_rect.top > this_word_rect.Top())
			bounding_rect.top = this_word_rect.Top();

		if (bounding_rect.bottom < this_word_rect.Bottom())
			bounding_rect.bottom = this_word_rect.Bottom();
	}
}

/** Enter vertical box */

/* virtual */ BOOL
CalcSelPointPositionObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (sel_point_done)
		return FALSE;

	if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
		return FALSE;

	if (!sel_point_done && box->GetHtmlElement() == next_container_element)
		next_container_element = FindNextContainerElementOf(next_container_element, sel_point.GetElement());

	return TRUE;
}

/** Enter inline box */

/* virtual */ BOOL
CalcSelPointPositionObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	HTML_Element* html_element = layout_props->html_element;

	if (html_element->GetIsListMarkerPseudoElement())
		return FALSE;

	if (!sel_point_done && box->IsContainingElement())
		if (html_element == next_container_element)
		{
			next_container_element = FindNextContainerElementOf(next_container_element, sel_point.GetElement());

			return TRUE;
		}
		else
			return FALSE;

	return !sel_point_done;
}

/** Enter line */

/* virtual */ BOOL
CalcSelPointPositionObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (sel_point_done)
		return FALSE;

	LayoutCoord start(LAYOUT_COORD_MIN);
	LayoutCoord end(LAYOUT_COORD_MIN);

	if (!next_container_element)
	{
		Box* box = sel_point.GetElement()->GetLayoutBox();

		if (box)
		{
			start = end = box->GetVirtualPosition();
			end += box->GetWidth();
		}
		else
			return TRUE;
	}
	else
		if (next_container_element->GetLayoutBox()->TraverseInLine())
			end = start = next_container_element->GetLayoutBox()->GetVirtualPosition();

	if (start != LAYOUT_COORD_MIN)
		return line->MayIntersectLogically(start, end);

	return TRUE;
}

/** Enter table row. */

/* virtual */ BOOL
CalcSelPointPositionObject::EnterTableRow(TableRowBox* row)
{
	return !sel_point_done && row->GetHtmlElement()->IsAncestorOf(sel_point.GetElement());
}

/** Check if this positioned element needs to be traversed. */

/* virtual */ BOOL
CalcSelPointPositionObject::TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element)
{
	return !sel_point_done && element->IsAncestorOf(sel_point.GetElement());
}

/** Handle selectable box */

/* virtual */ void
CalcSelPointPositionObject::HandleSelectableBox(LayoutProperties* layout_props)
{
	HTML_Element* html_element = layout_props->html_element;

	if (!sel_point_done && html_element == sel_point.GetElement())
	{
#ifdef LAYOUT_CARET_SUPPORT
		int line_height;

		if (html_element->Type() == Markup::HTE_BR)
			line_height = layout_props->GetProps()->GetCalculatedLineHeight(GetVisualDevice());
		else
			line_height = html_element->GetLayoutBox()->GetHeight();

		sel_point_layout.SetHeight(line_height);
		sel_point_layout.SetLineTranslationY(GetTranslationY());
		sel_point_layout.SetLineHeight(line_height);
		sel_point_layout.SetPixelTranslationX(GetTranslationX());
		sel_point_layout.SetPixelTranslationY(GetTranslationY());
		sel_point_layout.SetTransformRoot(GetTransformRoot());
#endif // LAYOUT_CARET_SUPPORT

		sel_point_done = TRUE;
	}
}

/** "Enter" layout break. */

/* virtual */ BOOL
CalcSelPointPositionObject::EnterLayoutBreak(HTML_Element* break_element)
{
#ifdef LAYOUT_CARET_SUPPORT
	if (!target_element && document->GetCaretPainter())
		return TRUE;
#endif // LAYOUT_CARET_SUPPORT

	return !sel_point_done && break_element == sel_point.GetElement();
}

CalculateSelectionRectsObject::CalculateSelectionRectsObject(FramesDocument* doc, const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end)
  : VisualTraversalObject(doc),
	start_range(*start, FALSE, FALSE, FALSE),
	end_range(*end, FALSE, FALSE, FALSE),
	start_box_done(start->GetElement() == NULL),
	end_box_done(end->GetElement() == NULL),
	check_point_only(FALSE),
	point_is_contained(FALSE)
{
	SetTraverseType(TRAVERSE_ONE_PASS);
	SetEnterHidden(TRUE);

	if (start->GetElement())
		next_container_element = doc->GetLogicalDocument()->GetRoot();
}

CalculateSelectionRectsObject::CalculateSelectionRectsObject(FramesDocument* doc, const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, const OpPoint& point)
  : VisualTraversalObject(doc),
	start_range(*start, FALSE, FALSE, FALSE),
	end_range(*end, FALSE, FALSE, FALSE),
	start_box_done(start->GetElement() == NULL),
	end_box_done(end->GetElement() == NULL),
	check_point_only(TRUE),
	point_to_check(point),
	point_is_contained(FALSE)
{
	SetTraverseType(TRAVERSE_ONE_PASS);
	SetEnterHidden(TRUE);

	if (start->GetElement())
		next_container_element = doc->GetLogicalDocument()->GetRoot();
}

/** Callback for objects needing to set up code before starting traversal */

/* virtual */ void
CalculateSelectionRectsObject::StartingTraverse()
{
	int negative_overflow = document->NegativeOverflow();

	clip_rect.Set(-negative_overflow, 0, document->Width() + negative_overflow, document->Height());
	OP_ASSERT(!clip_rect.IsEmpty());

	start_range.AdjustForTraverse();
	end_range.AdjustForTraverse();
}

void
CalculateSelectionRectsObject::HandleSelectionRectangle(const OpRect& rect)
{
	OP_ASSERT(!check_point_only || !point_is_contained);

	if (check_point_only)
		point_is_contained = rect.Contains(point_to_check);
	else
	{
		OpRect* sel_rect_elm = OP_NEW(OpRect, (rect));

		if (!sel_rect_elm)
		{
			SetOutOfMemory();
			return;
		}

		sel_rect_elm->IntersectWith(clip_rect);
		union_rect.UnionWith(*sel_rect_elm);

		if (OpStatus::IsMemoryError(selection_rects.Add(sel_rect_elm)))
		{
			OP_DELETE(sel_rect_elm);
			SetOutOfMemory();
			return;
		}
	}
}

/** "Enter" layout break. */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterLayoutBreak(HTML_Element* break_element)
{
	if (point_is_contained)
		return FALSE;

#ifdef LAYOUT_CARET_SUPPORT
	if (!target_element && document->GetCaretPainter())
		return TRUE;
#endif

	if (end_box_done)
		return FALSE;

	return start_box_done || break_element == start_range.GetElement();
}

/** Handle break box */

/* virtual */ void
CalculateSelectionRectsObject::HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break)
{
	if (!start_box_done && layout_props->html_element == start_range.GetElement())
		// Not much to do except record that the start box has been found.

		start_box_done = TRUE;
}

/** Handle text content */

/* virtual */ void
CalculateSelectionRectsObject::HandleTextContent(LayoutProperties* layout_props,
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
	OP_NEW_DBG("CalculateSelectionRectsObject::HandleTextContent\n", "selection");
	const HTMLayoutProperties& props = *layout_props->GetProps();
	VisualDevice* visual_device = GetVisualDevice();
	LayoutCoord justified_space_width = space_width + justified_space_extra;
	LayoutCoord start_selection_x(0);
	LayoutCoord end_selection_x = word_width + justified_space_width;
	HTML_Element* html_element = layout_props->html_element;
	BOOL include = FALSE;

	if (point_is_contained)
		return;

#ifdef SUPPORT_TEXT_DIRECTION
	if (!segment.left_to_right)
		x -= justified_space_width;
#endif

	if (!start_box_done)
		if (html_element == start_range.GetElement())
			if (start_range.GetWord() == word)
			{
				start_box_done = TRUE;

				LayoutCoord non_selection_width(0);

				int text_format = GetTextFormat(props, word_info);
#ifdef SUPPORT_TEXT_DIRECTION
				if (!segment.left_to_right)
					text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif
				// Number of characters into the word
				int length = start_range.GetElementCharacterOffset() - word_info.GetStart();

				if (length <= 0)
					/* If less than zero, we are probably in collapsed whitespace before the
					   word. If equal to zero, we know that the selection starts at the
					   beginning. */
					non_selection_width = LayoutCoord(0);
				else if (length == word_length)
					non_selection_width = word_width;
				else if (length > word_length)
					non_selection_width = end_selection_x; // 'end_selection_x' can include white-space, 'word_length' does not
				else
					non_selection_width = LayoutCoord(visual_device->GetTxtExtentEx(word, length, text_format));

				OP_DBG(("non_selection_width: %d", int(non_selection_width)));

#ifdef SUPPORT_TEXT_DIRECTION
				if (!segment.left_to_right)
					end_selection_x = word_width - non_selection_width + justified_space_width;
				else
#endif
					start_selection_x = non_selection_width;
			}

	if (start_box_done)
		// Now we are inside the selected area

		if (!end_box_done)
			// The first time 'start_selection' equals 'end_selection',
			// this is a special case so drop the following actions

			if (start_range != end_range)
			{
				include = TRUE;
				if (html_element == end_range.GetElement())
				{
					if (end_range.GetWord() == word)
					{
						// Now we have found the end of the area

						LayoutCoord selected_width = word_width + justified_space_width;

						end_box_done = TRUE;

						int text_format = GetTextFormat(props, word_info);
#ifdef SUPPORT_TEXT_DIRECTION
						if (!segment.left_to_right)
							text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif
						// Number of characters into the word
						const int length = end_range.GetElementCharacterOffset() - word_info.GetStart();

						if (length <= 0)
							/* If less than zero, we are probably in collapsed whitespace before the
							   word. Might be a problem with GetStartOfWord(). If equal to zero, we
							   know that the selected width is zero. */
							selected_width = LayoutCoord(0);
						else if (length == word_length)
							selected_width = word_width;
						else if (length >= word_length)
							selected_width = word_width + justified_space_width; // Include trailing whitespace
						else
							selected_width = LayoutCoord(visual_device->GetTxtExtentEx(word, length, text_format));

#ifdef SUPPORT_TEXT_DIRECTION
						if (!segment.left_to_right)
							start_selection_x = word_width - selected_width;
						else
#endif
							end_selection_x = selected_width;
					}
				}
			}
			else
				end_box_done = TRUE;

	if (include)
	{
		OpRect local_word_rect;
		local_word_rect.x = x + start_selection_x;
		local_word_rect.y = baseline - props.font_ascent;
		local_word_rect.width = end_selection_x - start_selection_x;
		local_word_rect.height = props.font_descent + props.font_ascent + 1;

		OpRect this_word_rect = visual_device->ToBBox(local_word_rect);

		HandleSelectionRectangle(this_word_rect);
	}
}

/** Handle replaced content */

/* virtual */ void
CalculateSelectionRectsObject::HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content)
{
	if (point_is_contained)
		return;

	if (layout_props->html_element->IsInSelection())
	{
		OpRect box_rect = visual_device->ToBBox(OpRect(0, 0, content->GetWidth(), content->GetHeight()));
		HandleSelectionRectangle(box_rect);
	}
}

/** Enter vertical box */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (point_is_contained || end_box_done)
		return FALSE;

	BOOL enter = start_box_done;

	if (!enter)
	{
		HTML_Element* html_element = box->GetHtmlElement();

		enter = html_element == next_container_element || html_element == start_range.GetElement();
	}

	if (enter)
	{
		if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
			return FALSE;

		if (!start_box_done)
			next_container_element = FindNextContainerElementOf(next_container_element, start_range.GetElement());

		const HTMLayoutProperties& props = *layout_props->GetProps();

		BOOL abs_clipping = props.clip_left != CLIP_NOT_SET && box->IsAbsolutePositionedBox();
		BOOL overflow_clipping = props.overflow_x != CSS_VALUE_visible && !box->GetTableContent();
		BOOL clip_affects_target = (abs_clipping || overflow_clipping) && box->GetClipAffectsTarget(target_element);

		if (clip_affects_target)
		{
			OpRect clip_rect = box->GetClippedRect(props, overflow_clipping);
			PushClipRect(clip_rect, traverse_info);

			return TRUE;
		}

		return TRUE;
	}
	else
		return FALSE;
}

/** Leave vertical box */

/* virtual */ void
CalculateSelectionRectsObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);
}

/** Enter inline box */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (point_is_contained)
		return FALSE;

	HTML_Element* html_element = layout_props->html_element;

	if (html_element->GetIsListMarkerPseudoElement())
		return FALSE;

	if (!start_box_done && box->IsContainingElement())
		if (html_element == next_container_element)
		{
			next_container_element = FindNextContainerElementOf(next_container_element, start_range.GetElement());

			return TRUE;
		}
		else
			return FALSE;

	return !start_box_done || !end_box_done;
}

/** Enter line */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (point_is_contained || end_box_done)
		return FALSE;

	if (!start_box_done)
	{
		if (!next_container_element)
		{
			LayoutCoord start;
			LayoutCoord end;

			Box* box = start_range.GetElement()->GetLayoutBox();

			if (box)
			{
				start = end = box->GetVirtualPosition();
				end += box->GetWidth();
			}
			else
				return TRUE;

			if (start != LAYOUT_COORD_MIN)
				return (line->HasContentAfterEnd() || start <= line->GetEndPosition()) && (line->HasContentBeforeStart() || end >= line->GetStartPosition());
		}
		else
			if (next_container_element->GetLayoutBox()->TraverseInLine())
				return next_container_element->GetLayoutBox()->GetVirtualPosition() < line->GetEndPosition();
			else
				return FALSE;
	}

	return TRUE;
}

/** Begin clipping of a partially collapsed table cell. */

/* virtual */ void
CalculateSelectionRectsObject::BeginCollapsedTableCellClipping(TableCellBox* box, const OpRect& clip_rect, TraverseInfo& traverse_info)
{
	if ((!target_element ||
		box->GetHtmlElement()->IsAncestorOf(target_element) && box->GetClipAffectsTarget(target_element)))
		PushClipRect(clip_rect, traverse_info);
}

/** End clipping of a partially collapsed table cell. */

/* virtual */ void
CalculateSelectionRectsObject::EndCollapsedTableCellClipping(TableCellBox* box, TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);
}

/** Enter table content. */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info)
{
	if (point_is_contained)
		return FALSE;

	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (props.overflow_x == CSS_VALUE_hidden)
	{
		Content_Box* table_box = content->GetPlaceholder();

		if (table_box->GetClipAffectsTarget(target_element))
		{
			OpRect clip_rect = table_box->GetClippedRect(props, TRUE);

			PushClipRect(clip_rect, traverse_info);
		}
	}

	return TRUE;
}

/** Leave table content. */

/* virtual */ void
CalculateSelectionRectsObject::LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);
}

/** Enter table row. */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterTableRow(TableRowBox* row)
{
	if (point_is_contained)
		return FALSE;

	if (end_box_done || (!start_box_done && !row->GetHtmlElement()->IsAncestorOf(start_range.GetElement())))
		return FALSE;

	return TRUE;
}

/** Check if this positioned element needs to be traversed. */

/* virtual */ BOOL
CalculateSelectionRectsObject::TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element)
{
	return !end_box_done && (start_box_done || element->IsAncestorOf(start_range.GetElement()));
}

/** Handle selectable box */

/* virtual */ void
CalculateSelectionRectsObject::HandleSelectableBox(LayoutProperties* layout_props)
{
	if (point_is_contained)
		return;

	BOOL include = FALSE;
	VisualDevice* visual_device = GetVisualDevice();
	HTML_Element* html_element = layout_props->html_element;
	Box* box = html_element->GetLayoutBox();
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (!start_box_done)
	{
		if (html_element == start_range.GetElement())
		{
			start_box_done = TRUE;
			include = TRUE;
		}
	}
	if (start_box_done)
	{
		if (!end_box_done)
		{
			if (start_range != end_range)
			{
				include = TRUE;
				if (html_element == end_range.GetElement())
				{
					end_box_done = TRUE;
				}
			}
			else
				end_box_done = TRUE;
		}
	}
	if (include)
	{
		int line_height, width, height;
		if (html_element->Type() == HE_BR)
		{
			line_height = props.GetCalculatedLineHeight(visual_device);
			width = SEL_BR_WIDTH;
			height = line_height;
		}
		else
		{
			line_height = box->GetHeight();
			width = box->GetWidth();
			height = box->GetHeight();
		}

		OpRect rect = visual_device->ToBBox(OpRect(0, 0, width, height));
		HandleSelectionRectangle(rect);
	}
}

/** Applies clipping */

/* virtual */ void
CalculateSelectionRectsObject::PushClipRect(const OpRect& cr, TraverseInfo& info)
{
	info.old_clip_rect = clip_rect;
	clip_rect = visual_device->ToBBox(cr);
	info.has_clipped = TRUE;
}

/** Removes clipping */

/* virtual */ void
CalculateSelectionRectsObject::PopClipRect(TraverseInfo& info)
{
	if (info.has_clipped)
	{
		clip_rect = info.old_clip_rect;
		info.has_clipped = FALSE;
	}
}

/** Enter scrollable content. */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	if (point_is_contained)
		return FALSE;

	Box* this_box = scrollable->GetOwningBox();

	if (this_box->GetClipAffectsTarget(target_element))
	{
		int left = props.border.left.width + (scrollable->LeftHandScrollbar() ? scrollable->GetVerticalScrollbarWidth() : 0);
		OpRect rect(left, props.border.top.width, width, height);

		PushClipRect(rect, traverse_info);
	}

	return TRUE;
}

/** Leave scrollable content. */

/* virtual */ void
CalculateSelectionRectsObject::LeaveScrollable(TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);
}

/** Enter column in multicol container or page in paged container. */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info)
{
	if (point_is_contained)
		return FALSE;

	if (!target_element || multipane_lprops->html_element->GetLayoutBox()->GetClipAffectsTarget(target_element, TRUE))
	{
		OpRect clip_rect = CalculatePaneClipRect(multipane_lprops, pane, target_element, is_column);
		PushClipRect(clip_rect, traverse_info);
	}

	return TRUE;
}

/** Leave column in multicol container or page in paged container. */

/* virtual */ void
CalculateSelectionRectsObject::LeavePane(TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
CalculateSelectionRectsObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	if (point_is_contained)
		return FALSE;

	Box* this_box = content->GetPlaceholder();

	if (this_box->GetClipAffectsTarget(target_element))
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		PushClipRect(OpRect(props.border.left.width, props.border.top.width, width, height), traverse_info);
	}

	return TRUE;
}

/** Leave paged container. */

/* virtual */ void
CalculateSelectionRectsObject::LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info)
{
	PopClipRect(traverse_info);
}

#endif // PAGED_MEDIA_SUPPORT

ExpandSelectionObject::ExpandSelectionObject(FramesDocument* doc, const SelectionBoundaryPoint& old_start, const SelectionBoundaryPoint& old_end, TextSelectionType type)
  : VisualTraversalObject(doc),
	start_old_selection(old_start),
	end_old_selection(old_end),
	selection_type(type),
	start_new_found(FALSE),
	finished(FALSE),
	start_old_found(FALSE),
	new_selection(TRUE)
{
	next_container_element = doc->GetLogicalDocument()->GetRoot();
	SetTraverseType(TRAVERSE_ONE_PASS);

	start_new_selection.SetLogicalPosition(start_old_selection.GetElement(), 0);

	if (selection_type == TEXT_SELECTION_SENTENCE ||
		selection_type == TEXT_SELECTION_PARAGRAPH)
	{
		HTML_Element* element = start_old_selection.GetElement();
		HTML_Element* candidate = element;

		while (element)
		{
			Box* box = element->GetLayoutBox();

			if (box)
			{
				if (box->IsTextBox())
				{
					if (selection_type == TEXT_SELECTION_SENTENCE)
					{
						TextData* text_data = element->GetTextData();

						if (text_data)
						{
							const uni_char* text = text_data->GetText();

							if (text)
							{
								int i;

								if (element == start_old_selection.GetElement())
								{
									SelectionPointWordInfo start_old_selection_word_info(start_old_selection, FALSE);
									i = start_old_selection_word_info.GetWord() - text;
								}
								else
									i = text_data->GetTextLen() - 1;

								while (i >= 0 && text[i] != '.')
									i--;

								if (i >= 0)
								{
									start_new_selection.SetLogicalPosition(element, 0);
									break;
								}
							}
						}
					}

					candidate = element;
				}
				else
					if (box->IsContainingElement() || element->Type() == Markup::HTE_BR)
					{
						start_new_selection.SetLogicalPosition(candidate, 0);
						break;
					}
			}

			if (element->LastChild())
				element = element->LastChild();
			else
				for (; element; element = element->Parent())
				{
					Box* box = element->GetLayoutBox();

					if (box)
						if (box->IsContainingElement() || element->Type() == Markup::HTE_BR)
							break;

					if (element->Pred())
					{
						element = element->Pred();
						break;
					}
				}
		}
	}
}

/** Callback for objects needing to set up code before starting traversal */

/* virtual */ void
ExpandSelectionObject::StartingTraverse()
{
	/* Guarantee that unless we're finished, the new start points to
	   an element with non-NULL layout box. Otherwise abort traversal
	   as soon as possible and keep what we currently had. */

	if (!start_new_selection.GetElement() ||
		!start_new_selection.GetElement()->GetLayoutBox())
	{
		finished = TRUE;
		start_new_selection = start_old_selection;
		end_new_selection = end_old_selection;
	}
}

/** Handle text content */

/* virtual */ void
ExpandSelectionObject::HandleTextContent(LayoutProperties* layout_props,
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
	if (!finished && !next_container_element)
	{
		if (!start_new_found)
			if (box == start_new_selection.GetElement()->GetLayoutBox())
				start_new_found = TRUE;
			else
				return;

		const HTMLayoutProperties& props = *layout_props->GetProps();
		HTML_Element* html_element = layout_props->html_element;

		if (selection_type == TEXT_SELECTION_WORD && (word_info.IsStartOfWord() ||
			                                          (props.white_space == CSS_VALUE_pre && word_length && uni_isspace(*word))))
		{
			if (start_old_found)
				finished = TRUE;
			else
				new_selection = TRUE;
		}

		if (!start_old_found)
		{
			int char_offset = 0;

			SelectionPointWordInfo start_old_selection_word_info(start_old_selection, FALSE);
			if (start_old_selection_word_info.GetWord() == word)
			{
				start_old_found = TRUE;

				if (selection_type == TEXT_SELECTION_WORD)
				{
					OP_ASSERT(start_old_selection_word_info.GetOffsetIntoWord() <= word_length);

					for (int i = 0; i < start_old_selection_word_info.GetOffsetIntoWord(); i++)
						if (!uni_isalnum(word[i]) && word[i] != '\'')
						{
							new_selection = TRUE;
							char_offset = i + 1;
						}
				}
			}
			else
				if (!space_width && selection_type == TEXT_SELECTION_WORD)
					for (int i = 0; i < word_length; i++)
						if (!uni_isalnum(word[i]) && word[i] != '\'')
						{
							new_selection = TRUE;
							char_offset = i + 1;
						}

			if (new_selection)
			{
				start_new_selection.SetLogicalPosition(html_element, word - html_element->TextContent() + char_offset);

				new_selection = FALSE;
			}
		}

		if (!finished && start_old_found)
		{
			int char_offset = word_length;

			if (selection_type == TEXT_SELECTION_WORD)
			{
				SelectionPointWordInfo start_old_selection_word_info(start_old_selection, FALSE);
				OP_ASSERT(start_old_selection_word_info.GetWord() != word || start_old_selection_word_info.GetOffsetIntoWord() <= word_length);
				int start_i = start_old_selection_word_info.GetWord() == word ? start_old_selection_word_info.GetOffsetIntoWord() : 0;

				for (int i = start_i; i < word_length; i++)
					if (!uni_isalnum(word[i]) && word[i] != '\'')
					{
						finished = TRUE;
						char_offset = i;
						break;
					}
			}

			if (char_offset < word_length)
				word_width = LayoutCoord(GetVisualDevice()->GetTxtExtentEx(word, char_offset, GetTextFormat(props, word_info)));

			end_new_selection.SetLogicalPosition(html_element, word - html_element->TextContent() + char_offset);
		}

		if (selection_type == TEXT_SELECTION_SENTENCE && word_length > 0 && word[word_length - 1] == '.')
			if (start_old_found)
				finished = TRUE;
			else
				new_selection = TRUE;
	}
}

/** Enter vertical box */

/* virtual */ BOOL
ExpandSelectionObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (!finished && box->GetHtmlElement() == next_container_element)
	{
		if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
			return FALSE;

		next_container_element = FindNextContainerElementOf(next_container_element, start_new_selection.GetElement());

		return TRUE;
	}
	else
		if (start_old_found)
			finished = TRUE;

	new_selection = TRUE;
	return FALSE;
}

/** Leave vertical box */

/* virtual */ void
ExpandSelectionObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (!finished && start_old_found)
		finished = TRUE;
	else
		new_selection = TRUE;
}

/** Enter inline box */

/* virtual */ BOOL
ExpandSelectionObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (!finished && box->IsContainingElement())
	{
		if (layout_props->html_element == next_container_element)
		{
			next_container_element = FindNextContainerElementOf(next_container_element, start_new_selection.GetElement());

			return TRUE;
		}
		else
		{
			new_selection = TRUE;
			return FALSE;
		}
	}

	return !finished;
}

/** Leave inline box */

/* virtual */ void
ExpandSelectionObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	VisualTraversalObject::LeaveInlineBox(layout_props, box, box_area, start_of_box, end_of_box, traverse_info);

	if (start_old_found && box->IsInlineBlockBox())
		finished = TRUE;
}

/** Enter line */

/* virtual */ BOOL
ExpandSelectionObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (!finished)
	{
		LayoutCoord start;
		LayoutCoord end;

		if (next_container_element)
			if (next_container_element->GetLayoutBox()->TraverseInLine())
				end = start = next_container_element->GetLayoutBox()->GetVirtualPosition();
			else
				return FALSE;
		else
			if (start_new_found)
				return !finished;
			else
			{
				Box* box = start_new_selection.GetElement()->GetLayoutBox();

				start = box->GetVirtualPosition();
				end = start + box->GetWidth();
			}

		return line->MayIntersectLogically(start, end);
	}

	return FALSE;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
ExpandSelectionObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	return TRUE;
}

#endif // PAGED_MEDIA_SUPPORT

/** Enter table row. */

/* virtual */ BOOL
ExpandSelectionObject::EnterTableRow(TableRowBox* row)
{
	return !finished;
}

/** Handle break box */

/* virtual */ void
ExpandSelectionObject::HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break)
{
	if (start_old_found)
		finished = TRUE;
}

/** Callback for objects needing to set up code before starting traversal */

/* virtual */ void
SelectionTextCopyObject::StartingTraverse()
{
	start_selection.AdjustForTraverse();
	end_selection.AdjustForTraverse();
}

/** Enter vertical box */

//FIXME:OOM Check callers
/* virtual */ BOOL
SelectionTextCopyObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (end_box_done)
		return FALSE;
	else
	{
		if (start_box_done || box->GetHtmlElement() == next_container_element)
		{
			if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
				return FALSE;

			if (!start_box_done && layout_props->html_element == start_selection.GetElement())
				start_box_done = TRUE;

			if (!start_box_done)
				next_container_element = FindNextContainerElementOf(next_container_element, start_selection.GetElement());
			else
				if (!(GetDocument()->GetParentDoc() &&
					  GetDocument()->GetParentDoc()->GetURL().Type() == URL_EMAIL))
					if (box->IsTableCell())
					{
						if (previous_was_cell)
						{
							if (buffer && buffer_length > copy_length + 1)
								uni_strncpy(buffer + copy_length, UNI_L("\t"), 1);

							copy_length++;
						}

						add_newline = FALSE;
					}
					else
						if (layout_props->html_element->Type() == Markup::HTE_P)
						{
							AddNewline();

							if (blockquotes_as_text)
								AddQuoteCharacters();
						}

			if (layout_props->html_element->Type() == Markup::HTE_BLOCKQUOTE)
				blockquote_count++;

			return TRUE;
		}
		else
			return FALSE;
	}
}

/** Leave vertical box */

/* virtual */ void
SelectionTextCopyObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (start_box_done && !end_box_done)
	{
		if (!(GetDocument()->GetParentDoc() &&
			GetDocument()->GetParentDoc()->GetURL().Type() == URL_EMAIL))
		{
			previous_was_cell = box->IsTableCell();
		}

		if (layout_props->html_element == end_selection.GetElement())
			end_box_done = TRUE;

		if (layout_props->html_element->Type() == Markup::HTE_BLOCKQUOTE)
		{
			blockquote_count--;
			OP_ASSERT(blockquote_count >= 0);
		}
	}
}

/** Enter inline box */

/* virtual */ BOOL
SelectionTextCopyObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	HTML_Element* html_element = layout_props->html_element;

	if (html_element->GetIsListMarkerPseudoElement())
		return FALSE;

	if (!start_box_done && html_element == start_selection.GetElement())
		start_box_done = TRUE;

	if (!start_box_done && box->IsContainingElement())
		if (html_element == next_container_element)
		{
			next_container_element = FindNextContainerElementOf(next_container_element, start_selection.GetElement());

			return TRUE;
		}
		else
			return FALSE;

	return !start_box_done || !end_box_done;
}


/** Leave inline box */

/* virtual */ void
SelectionTextCopyObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	if (start_box_done && !end_box_done && layout_props->html_element == end_selection.GetElement())
		end_box_done = TRUE;
}

/** Enter line */

/* virtual */ BOOL
SelectionTextCopyObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (end_box_done)
		return FALSE;
	else
	{
		if (add_newline)
		{
			AddNewline();
			add_newline = FALSE;
		}

		if (start_box_done)
		{
			if (blockquotes_as_text)
				AddQuoteCharacters();

			return TRUE;
		}
		else
		{
			LayoutCoord start(LAYOUT_COORD_MIN);
			LayoutCoord end(LAYOUT_COORD_MIN);

			if (next_container_element)
			{
				if (next_container_element->GetLayoutBox()->TraverseInLine())
					end = start = next_container_element->GetLayoutBox()->GetVirtualPosition();
			}
			else
				if (Box* box = start_selection.GetElement()->GetLayoutBox())
				{
					start = box->GetVirtualPosition();
					end = start + box->GetWidth();
				}

			return start == LAYOUT_COORD_MIN || line->MayIntersectLogically(start, end);
		}
	}
}

/** Leave line */

/* virtual */ void
SelectionTextCopyObject::LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (start_box_done && !end_box_done && line->HasForcedBreak())
		add_newline = TRUE;
}


/** Handle text content */

/* virtual */ void
SelectionTextCopyObject::HandleTextContent(LayoutProperties* layout_props,
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
#ifdef LAYOUT_CARET_SUPPORT
	// Collapsed text is visited when in document edit mode (see Text_Box::LineTraverseBox), skip it
	if (document->GetCaretPainter() && word_info.IsCollapsed())
		return;
#endif

	const HTMLayoutProperties& props = *layout_props->GetProps();
	const uni_char* copy_word = NULL;
	HTML_Element* html_element = layout_props->html_element;

	// Trailing whitespaces are included in the word length for pre & pre-wrap (I think it is a kludge that they still have the trailing whitespace flag set).
	BOOL has_trailing_whitespace = word_info.HasTrailingWhitespace() && props.white_space != CSS_VALUE_pre_wrap && props.white_space != CSS_VALUE_pre;

	int copy_word_length = 0; // The length of the part of the actual word we plan to copy
	int use_trailing_whitespace_length = 0; // can be 0 or 1. 1 if we plan to copy this word's trailing whitespace.

	if (props.visibility == CSS_VALUE_hidden)
	{
		if (!start_box_done)
			if (html_element == start_selection.GetElement())
				// The selection starts in this invisible text box.
				start_box_done = TRUE;

		if (start_box_done && !end_box_done)
			if (html_element == end_selection.GetElement())
				// The selection ends in this invisible text box.
				end_box_done = TRUE;

		return;
	}

	if (!start_box_done)
		if (html_element == start_selection.GetElement())
			if (start_selection.GetWord() == word)
			{
				// This is start of selection area
				start_box_done = TRUE;

				if (blockquotes_as_text)
					AddQuoteCharacters();

				int offset_into_word = start_selection.GetOffsetIntoWord();
				if (offset_into_word <= word_length)
				{
					copy_word_length = word_length - offset_into_word;
					copy_word = word + offset_into_word;
					if (has_trailing_whitespace)
						use_trailing_whitespace_length = 1;
				}
				else
				{
					copy_word_length = 0;
					copy_word = word + word_length;
					// = implicit use_trailing_whitespace_length = 0. This is a simplification - ignore trailing whitespaces after the first.
				}
			}

	if (start_box_done && !end_box_done)
	{
		// Now we are inside the selected area

		if (html_element == end_selection.GetElement())
		{
			if (end_selection.GetWord() == word)
			{
				// This is the end of the selected area

				end_box_done = TRUE;

				if (!copy_word)
				{
					// Not the first word

					copy_word = word;

					int offset_into_word = end_selection.GetOffsetIntoWord();
					if (offset_into_word <= word_length)
					{
						copy_word = word;
						copy_word_length = offset_into_word;
					}
					else
					{
						copy_word_length = word_length;
						OP_ASSERT(has_trailing_whitespace);
						use_trailing_whitespace_length = 1;
					}
				}
				else
				{
					// This is also the first word

					OP_ASSERT(html_element == start_selection.GetElement());
					OP_ASSERT(start_selection.GetWord() == word);

					BOOL starts_inside_word = start_selection.GetOffsetIntoWord() <= word_length;
					BOOL ends_inside_word = end_selection.GetOffsetIntoWord() <= word_length;

					if (starts_inside_word && ends_inside_word)
					{
						copy_word_length = end_selection.GetOffsetIntoWord() - start_selection.GetOffsetIntoWord();
						copy_word = word + start_selection.GetOffsetIntoWord();
						use_trailing_whitespace_length = 0;
					}
					else if (starts_inside_word && !ends_inside_word)
					{
						copy_word_length = word_length - start_selection.GetOffsetIntoWord();
						copy_word = word + start_selection.GetOffsetIntoWord();
						use_trailing_whitespace_length = 1;
						OP_ASSERT(has_trailing_whitespace);
					}
					else if (!starts_inside_word && !ends_inside_word)
					{
						OP_ASSERT(has_trailing_whitespace);
						copy_word_length = 0;
						copy_word = word + word_length;
						use_trailing_whitespace_length = 0; // simplification - ignore any trailing whitespace after the first.
					}
					else
					{
						OP_ASSERT(0); // The end point is before the start point.
					}

				}
			}
		}

		if (!copy_word)
		{
			// This is a word between the start and end point. Copy all.

			copy_word = word;
			copy_word_length = word_length;

			if (has_trailing_whitespace)
				use_trailing_whitespace_length = 1;
		}

		if (copy_word_length + use_trailing_whitespace_length > 0)
		{
			if (buffer && buffer_length > copy_length + copy_word_length + use_trailing_whitespace_length)
			{
				if (copy_word_length > 0)
					uni_strncpy(buffer + copy_length, copy_word, copy_word_length);

				if (use_trailing_whitespace_length)
					buffer[copy_length + copy_word_length] = ' ';
			}

			copy_length += copy_word_length + use_trailing_whitespace_length;
		}
	}
}

/** Leave table row. */

/* virtual */ void
SelectionTextCopyObject::LeaveTableRow(TableRowBox* row, TableContent* table)
{
	if (start_box_done && !end_box_done)
	{
		AddNewline();
		previous_was_cell = FALSE;
		add_newline = TRUE;
	}
}

/** Handle break box */

/* virtual */ void
SelectionTextCopyObject::HandleLineBreak(LayoutProperties* layout_props, BOOL is_layout_break)
{
	HTML_Element* html_element = layout_props->html_element;

	if (html_element == start_selection.GetElement())
		// This is start of selection area

		start_box_done = TRUE;

	if (start_box_done && !end_box_done)
	{
		const BOOL exclude_line_break =
			html_element == end_selection.GetElement() &&
			end_selection.GetElementCharacterOffset() == 0;

		/* We need to add line breaks here for actual LayoutBreaks
		   but only for those, not for line-ending <br>s. Those
		   are handled in ::LeaveLine along with other forced breaks. */

		if (!exclude_line_break && is_layout_break)
		{
			AddNewline();

			if (blockquotes_as_text)
				AddQuoteCharacters();
		}

		if (html_element == end_selection.GetElement())
			// This is the end of the selected area

			end_box_done = TRUE;
	}
}


void SelectionTextCopyObject::AddNewline()
{
	if (buffer && buffer_length > copy_length + 1)
		uni_strncpy(buffer + copy_length, UNI_L("\n"), 1);

	copy_length++;
}

void SelectionTextCopyObject::AddQuoteCharacters()
{
	int num_blockquotes_to_add = blockquote_count;
	while (num_blockquotes_to_add--)
	{
		if (buffer && buffer_length > copy_length + 1)
			uni_strncpy(buffer + copy_length, UNI_L(">"), 1);

		copy_length++;
	}
}

/* virtual */ void
CliprectObject::Intersect(const RECT &box_area)
{
	OP_ASSERT(target_found);

	RECT bbox = visual_device_transform.ToBBox(box_area);

	if (bbox.left < area.right && bbox.top < area.bottom &&
		bbox.right > area.left && bbox.bottom > area.top)
	{
		/* Always treat a covered box as invalid. */

		invalid_cliprect = TRUE;
	}
}

/*virtual*/ BOOL
CliprectObject::HandleVerticalBox(LayoutProperties* parent_lprops,
								  LayoutProperties*& layout_props,
								  VerticalBox* box,
								  TraverseInfo& traverse_info,
								  BOOL clip_affects_traversed_descendants)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();
	RECT box_area;

	if (clip_affects_traversed_descendants)
		box_area = box->GetClippedRect(props, FALSE);
	else
	{
		box_area.left = 0;
		box_area.top = 0;
		box_area.right = box->GetWidth();
		box_area.bottom = box->GetHeight();
	}

	if (!target_found && target_box == box)
	{
		/* Set area */

		target_found = TRUE;
		area = visual_device_transform.ToBBox(box_area);

		return FALSE;
	}
	else if (target_found)
	{
		if (GetTraverseType() == TRAVERSE_BACKGROUND)
		{
			Intersect(box_area);
			return !invalid_cliprect;
		}
		else
			if (box == target_box)
			{
				// 2'nd traversepass and box == target_box, so we know that we are done.
				content_done = TRUE;
				return FALSE;
			}
	}

	return TRUE;
}

/* virtual */ BOOL
CliprectObject::HandleScrollable(const HTMLayoutProperties& props,
								 ScrollableArea* scrollable,
								 LayoutCoord width,
								 LayoutCoord height,
								 TraverseInfo& traverse_info,
								 BOOL clip_affects_target,
								 int scrollbar_modifier)
{
	if (target_found)
	{
		RECT box_area = { props.border.left.width + scrollbar_modifier,
						  props.border.top.width,
						  props.border.left.width + scrollbar_modifier + width,
						  props.border.top.width + height };

		Intersect(box_area);
	}

	return TRUE;
}

/* virtual */ BOOL
CliprectObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (invalid_cliprect)
		return FALSE;

	return HitTestingTraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info);
}

/** Enter inline box */

/* virtual */ BOOL
CliprectObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	OP_ASSERT(GetTraverseType() == TRAVERSE_CONTENT);

	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (layout_props->html_element->IsAncestorOf(target_box->GetHtmlElement()) && props.visibility != CSS_VALUE_visible)
		invalid_cliprect = TRUE;

	if (invalid_cliprect)
		return FALSE;
	else
		if (!target_found && target_box == box)
		{
			target_found = TRUE;

			// set area

			if (props.overflow_x != CSS_VALUE_visible && !box->IsInlineContent())
			{
				AbsoluteBoundingBox bounding_box;

				box->GetClippedBox(bounding_box, props, FALSE);

				RECT r = { MAX(box_area.left, bounding_box.GetX() + box_area.left),
						   MAX(box_area.top, bounding_box.GetY() + box_area.top),
						   MIN(box_area.right, bounding_box.GetX() + box_area.left + bounding_box.GetWidth()),
						   MIN(box_area.bottom, bounding_box.GetY() + box_area.top + bounding_box.GetHeight()) };
				area = visual_device_transform.ToBBox(r);
			}
			else
			{
				area = visual_device_transform.ToBBox(box_area);
			}

			return FALSE;
		}
		else
			if (!target_found)
				return layout_props->html_element->IsAncestorOf(target_box->GetHtmlElement());
			else
				if (HitTestingTraversalObject::EnterInlineBox(layout_props, box, box_area, segment, start_of_box, end_of_box, baseline, traverse_info))
				{
					Intersect(box_area);

					return !invalid_cliprect;
				}

	return FALSE;
}

/* virtual */ BOOL
CliprectObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	OP_ASSERT(GetTraverseType() == TRAVERSE_CONTENT);

	if (invalid_cliprect || content_done)
		return FALSE;
	else
		return !target_found || HitTestingTraversalObject::EnterLine(parent_lprops, line, containing_element, traverse_info);
}

/** Handle text content */

/* virtual */ void
CliprectObject::HandleTextContent(LayoutProperties* layout_props,
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
	const HTMLayoutProperties& props = *layout_props->GetProps();

	OP_ASSERT(GetTraverseType() == TRAVERSE_CONTENT);
	OP_ASSERT(box != target_box);

	if (!invalid_cliprect && target_found)
	{
		RECT box_area = { x,
						  baseline - props.font_ascent,
						  x + word_width + space_width + justified_space_extra,
						  baseline + props.font_descent };
		Intersect(box_area);
	}
}

/** Reset the search state to not found. */

void
SearchTextObject::ResetSearch(BOOL reset_to_previous_match, int *next_pos/*=NULL*/)
{
	if (reset_to_previous_match)
	{
		if (next_pos)
		{
			if (matches[1].start_match.GetElement())
			{
				SelectionPointWordInfo start_match_word_info(matches[1].start_match, FALSE);
				*next_pos = start_match_word_info.GetOffsetIntoWord() + 1;
			}
			else
			{
				SelectionPointWordInfo end_match_word_info(end_match, FALSE);
				*next_pos = end_match_word_info.GetOffsetIntoWord() + 1;
			}
		}

		RemovePartialMatch(0);
	}
	else
	{
		if (next_pos)
			*next_pos = 0;

		for (int i = uni_strlen(text) - 1; i >= 0; i--)
		{
			matches[i].characters_done = 0;
			matches[i].start_match.Reset();
		}
	}

	start_found = TRUE;
	finished = FALSE;
	start_match.Reset();
	end_match.Reset();
	match_current = TRUE;
	characters_done = 0;
	pending_finished = FALSE;
}

/** Remove a partial match and compact array. */

void
SearchTextObject::RemovePartialMatch(int i)
{
	unsigned int j = i;
	for (; matches[j+1].characters_done; j++)
		matches[j] = matches[j+1];

	OP_ASSERT(j <= uni_strlen(text));

	matches[j].characters_done = 0;
	matches[j].start_match.Reset();
}

/** Initialize search. */

BOOL
SearchTextObject::Init()
{
	int len = uni_strlen(text) + 1;
	matches = OP_NEWA(SearchTextMatch, len);
	if (matches)
	{
		for (int i = 0; i < len; i++)
			matches[i].characters_done = 0;

		return TRUE;
	}
	else
		return FALSE;
}

/** Handle text content */

/* virtual */ void
SearchTextObject::HandleTextContent(LayoutProperties* layout_props,
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
	if (forward && start_form_object) // Don't search until after start_form_object
		return;

	const HTMLayoutProperties& props = *layout_props->GetProps();
	HTML_Element* html_element = layout_props->html_element;

	// is searching only for links, text object must be within a link

	if (match_only_links)
	{
		HTML_Element* anchor_element = html_element->GetAnchorTags(document);

		if (!anchor_element)
			return;
	}

	if (!finished)
	{
		int start_word_idx = 0;

		SelectionPointWordInfo start_point_word_info(start_point, FALSE);
		if (!start_found &&
			start_point.GetElement() == html_element &&
			start_point_word_info.GetWord() == word)
		{
			OP_ASSERT(forward);

			start_found = TRUE;
			start_word_idx = start_point_word_info.GetOffsetIntoWord();

			if (!match_current)
				// Start matching at the first character after start point.
				start_word_idx++;

			if (start_word_idx >= word_length)
				return;
		}

		if (start_found)
		{
			int last_match_start = pending_finished ? 0 : word_length;

			if (!forward &&
				start_point.GetElement() == html_element &&
				start_point_word_info.GetWord() == word)
			{
				// When searching backwards we have to stop matching new
				// occurences of search string when start point is reached.
				// Need to finished already started matches though...

				last_match_start = start_point_word_info.GetOffsetIntoWord();
				pending_finished = TRUE;
			}

			int match_i = 0;

			// Loop through all started matches plus one new ...

			while (!match_i || matches[match_i-1].characters_done)
			{
				SelectionBoundaryPoint& partial_start_match = matches[match_i].start_match;
				characters_done = matches[match_i].characters_done;

				BOOL force_match_continue = FALSE;
				int start_char_offset = 0;
				int i = start_word_idx;

				while (i < word_length && (forward || characters_done || i < last_match_start))
				{
					if (!text[characters_done])
						break;

					else if ((!match_words
							  || characters_done
							  || uni_isspace(word[i])
							  || (i == 0 && word_info.IsStartOfWord())
							  || (i > 0 && word[i-1] == NBSP_CHAR)
							 )
							 &&
							 ((word[i] == NBSP_CHAR && text[characters_done] == ' ')
							   || (match_case && text[characters_done] == word[i])
							   || (!match_case && uni_tolower(text[characters_done]) == uni_tolower(word[i]))
							 ))
					{
						if (!characters_done)
							start_char_offset = i;

						characters_done++;
					}

					else
					{
						if (characters_done)
						{
							if (characters_done <= i)
							{
								// This match started in this word.
								// Restart with character number two in the match.

								i -= characters_done - 1;
								characters_done = 0;
								continue;
							}
							else
							{
								// This match started in another word.
								// Remove it and proceed with next partial match.

								RemovePartialMatch(match_i);

								force_match_continue = TRUE;
								break;
							}
						}
					}

					i++;
				}

				if (force_match_continue)
					continue;

				if (!text[characters_done])
				{
					if (match_words)
					{
						if (i == 0)
						{
							if (word_info.IsStartOfWord()
								|| (!word_length && word_info.HasTrailingWhitespace())
								|| (word_length && (uni_isspace(word[0]) || Unicode::IsCSSPunctuation(word[0]))))
							{
								finished = TRUE;
							}
							else
							{
								OP_ASSERT(text[0]);
								OP_ASSERT(partial_start_match.GetElement());
								OP_ASSERT(partial_start_match.GetElement() != html_element);

								// This match started in another word.
								// Remove it and proceed with next partial match.

								RemovePartialMatch(match_i);
								if (end_pending_word_finished.GetElement())
									end_pending_word_finished.Reset();

								continue;
							}
						}
						else if (i == word_length && !word_info.HasTrailingWhitespace())
						{
							end_pending_word_finished.SetLogicalPosition(html_element, word - html_element->TextContent() + i);
							//finished = TRUE;
							//matches[match_i].pending_word_finished = TRUE;
						}
						else if (i < word_length && word[i] != NBSP_CHAR)
						{
							if (characters_done <= i)
							{
								// This match started in this word.
								// Restart with character number two in the match.

								start_word_idx = i - characters_done + 1;
								OP_ASSERT(matches[match_i].characters_done == 0);
							}
							else
							{
								// This match started in another word.
								// Remove it and proceed with next partial match.

								RemovePartialMatch(match_i);
							}

							if (end_pending_word_finished.GetElement())
								end_pending_word_finished.Reset();
							continue;
						}
						else
							finished = TRUE;
					}
					else
						finished = TRUE;
				}

				int end_char_offset = i;

				if (!finished && (word_info.IsTabCharacter() || (word_info.HasTrailingWhitespace() && props.white_space != CSS_VALUE_pre_wrap)))
				{
					if (text[characters_done] == ' ')
					{
						if (start_word_idx <= word_length)
						{
							if (!characters_done)
								start_char_offset = word_length;

							end_char_offset++;
							characters_done++;

							if (!text[characters_done])
								finished = TRUE;
						}
					}
					else
						if (i == word_length)
						{
							if (characters_done <= i)
							{
								// This match started in this word.
								// Restart with character number two in the match.

								start_word_idx = i - characters_done + 1;
								OP_ASSERT(matches[match_i].characters_done == 0);
							}
							else
							{
								// This match started in another word.
								// Remove it and proceed with next partial match.

								RemovePartialMatch(match_i);
							}

							continue;
						}
				}

				if (characters_done && !partial_start_match.GetElement())
				{
					// Set start of partial match

					partial_start_match.SetLogicalPosition(html_element, word - html_element->TextContent() + start_char_offset);
					SelectionPointWordInfo partial_start_match_word_info(partial_start_match, FALSE);
					if (partial_start_match_word_info.GetWord() == word)
						// Next check need to start at the character following start of this match.

						start_word_idx = partial_start_match_word_info.GetOffsetIntoWord() + 1;
				}

				if (finished)
				{
					OP_ASSERT(match_i == 0);

					// Set start of match

					start_match = matches[0].start_match;

					// Set end of match

					if (end_pending_word_finished.GetElement())
					{
						end_match = end_pending_word_finished;
						end_pending_word_finished.Reset();
					}
					else
						end_match.SetLogicalPosition(html_element, word - html_element->TextContent() + end_char_offset);

					if (forward)
						// This is it.

						return;
					else
					{
						// Search must go on.

						finished = FALSE;

						// Remove this match and proceed with the next.

						RemovePartialMatch(match_i);

						continue;
					}
				}

				matches[match_i].characters_done = characters_done;

				match_i++;
			}

			if (pending_finished && !matches[0].characters_done)
			{
				OP_ASSERT(!forward);

				// Stop now because we have no more ongoing matches.

				finished = TRUE;
				return;
			}
		}
	}
}

/** Handle text content */

/* virtual */ void
SearchTextObject::HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj)
{
	// Note: This code is not run, if search-matches-all is used.
	// FIX: Remove or ifdef if never used.

	SEARCH_TYPE type = forward ? SEARCH_FROM_BEGINNING : SEARCH_FROM_END;
	if (start_form_object == form_obj)
	{
		start_form_object = NULL;
		type = SEARCH_FROM_CARET;
	}
	if (!finished && !match_only_links && (!forward || start_form_object == NULL))
	{
		HTML_Element* elm = form_obj->GetHTML_Element();
		// Don't search in password fields. They are "secret".
		if (elm->Type() != Markup::HTE_INPUT || elm->GetInputType() != INPUT_PASSWORD)
		{
			BOOL found = form_obj->GetWidget()->SearchText(text, uni_strlen(text), forward, match_case, match_words, type, FALSE);
			if (found)
			{
				match_form_object = form_obj;
				if (forward)
					finished = TRUE;
			}
		}
	}
	if (!forward && type == SEARCH_FROM_CARET)
		finished = TRUE;
}

/** Enter vertical box */

/* virtual */ BOOL
SearchTextObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (!finished)
	{
		if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
			return FALSE;

		// Reset all partial matches

		for (int i = 0; matches[i].characters_done; i++)
		{
			matches[i].characters_done = 0;
			matches[i].start_match.Reset();
		}

		return TRUE;
	}
	else
		return FALSE;
}

void
SearchTextObject::FinishPendingMatches(const Line* line)
{
	// Do we have a pending match
	if (end_pending_word_finished.GetElement())
	{
		start_match = matches[0].start_match;

		end_match = end_pending_word_finished;
		end_pending_word_finished.Reset();

		finished = TRUE;
	}

	// Reset all partial matches
	for (int i = 0; matches[i].characters_done; i++)
	{
		// still valid after linebreak if next character is space
		if (line)
		{
			// BR, can match space
			if (line->HasForcedBreak()
				&& uni_isspace(text[matches[i].characters_done]))
			{
				matches[i].characters_done++;
				continue;
			}
			// trailing whitespace, already incremented char count
			else if (uni_isspace(text[matches[i].characters_done - 1]))
				continue;
		}

		matches[i].characters_done = 0;
		matches[i].start_match.Reset();
	}
}

/* virtual */ void
SearchTextObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (!finished)
	{
		TraversalObject::LeaveVerticalBox(layout_props, box, traverse_info);

		FinishPendingMatches(NULL);
	}
}

/* virtual */ void
SearchTextObject::LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (!finished)
		FinishPendingMatches(line);
}

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL

void
SearchTextObject::MatchAll(LayoutProperties* layout_props,
						   Text_Box* box,
						   const uni_char* word,
						   int word_length,
						   LayoutCoord word_width,
						   LayoutCoord space_width,
						   LayoutCoord justified_space_extra,
						   const WordInfo& word_info,
						   LayoutCoord virtual_pos,
						   int start_word_idx/*=0*/)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();
	HTML_Element* html_element = layout_props->html_element;

	// is searching only for links, text object must be within a link
	if (match_only_links)
	{
		HTML_Element* anchor_element = html_element->GetAnchorTags(document);

		if (!anchor_element)
			return;
	}

	if (!finished)
	{
		// check if we have reached the starting point
		if (!start_found)
		{
			SelectionPointWordInfo start_point_word_info(start_point, FALSE);
			if (start_point.GetElement() != html_element
				|| start_point_word_info.GetWord() != word)
				return;

			start_found = TRUE;

			start_word_idx = start_point_word_info.GetOffsetIntoWord();
		}

		BOOL match_found = FALSE;
		int match_i = 0;

		// Loop through all started matches plus one new ...

		while (!match_i || matches[match_i-1].characters_done)
		{
			SelectionBoundaryPoint& partial_start_match = matches[match_i].start_match;
			characters_done = matches[match_i].characters_done;

			BOOL force_match_continue = FALSE;
			int start_char_offset = 0;
			int i = start_word_idx;

			while (i < word_length)
			{
				if (!text[characters_done])
					break;

				else if ((!match_words
						  || characters_done
						  || uni_isspace(word[i])
						  || (i == 0 && word_info.IsStartOfWord())
						  || (i > 0 && word[i-1] == NBSP_CHAR)
						 )
						 &&
						 ((word[i] == NBSP_CHAR && text[characters_done] == ' ')
						   || (match_case && text[characters_done] == word[i])
						   || (!match_case && uni_tolower(text[characters_done]) == uni_tolower(word[i]))
						 ))
				{
					if (!characters_done)
						start_char_offset = i;

					characters_done++;
				}

				else
				{
					if (characters_done)
					{
						if (characters_done <= i)
						{
							// This match started in this word.
							// Restart with character number two in the match.

							i -= characters_done - 1;
							characters_done = 0;
							continue;
						}
						else
						{
							// This match started in another word.
							// Remove it and proceed with next partial match.

							RemovePartialMatch(match_i);

							force_match_continue = TRUE;
							break;
						}
					}
				}

				i++;
			}

			if (force_match_continue)
				continue;

			if (!text[characters_done])
			{
				if (match_words)
				{
					if (i == 0)
					{
						if (word_info.IsStartOfWord()
							|| (!word_length && word_info.HasTrailingWhitespace())
							|| (word_length && (uni_isspace(word[0]) || Unicode::IsCSSPunctuation(word[0]))))
						{
							finished = TRUE;
						}
						else
						{
							OP_ASSERT(text[0]);
							OP_ASSERT(partial_start_match.GetElement());
							OP_ASSERT(partial_start_match.GetElement() != html_element);

							// This match started in another word.
							// Remove it and proceed with next partial match.

							RemovePartialMatch(match_i);
							if (end_pending_word_finished.GetElement())
								end_pending_word_finished.Reset();

							continue;
						}
					}
					else if (i == word_length && !word_info.HasTrailingWhitespace())
					{
						end_pending_word_finished.SetLogicalPosition(html_element, word - html_element->TextContent() + i);
						//finished = TRUE;
						//matches[match_i].pending_word_finished = TRUE;
					}
					else if (i < word_length && word[i] != NBSP_CHAR)
					{
						if (characters_done <= i)
						{
							// This match started in this word.
							// Restart with character number two in the match.

							start_word_idx = i - characters_done + 1;
							OP_ASSERT(matches[match_i].characters_done == 0);
						}
						else
						{
							// This match started in another word.
							// Remove it and proceed with next partial match.

							RemovePartialMatch(match_i);
						}

						if (end_pending_word_finished.GetElement())
							end_pending_word_finished.Reset();
						continue;
					}
					else
						finished = TRUE;
				}
				else
					finished = TRUE;
			}

			int end_char_offset = i;

			if (!finished && (word_info.IsTabCharacter() || (word_info.HasTrailingWhitespace() && props.white_space != CSS_VALUE_pre_wrap)))
			{
				if (text[characters_done] == ' ')
				{
					if (start_word_idx <= word_length)
					{
						if (!characters_done)
							start_char_offset = word_length;

						end_char_offset++;
						characters_done++;

						if (!text[characters_done])
							finished = TRUE;
					}
				}
				else if (i == word_length)
				{
					if (characters_done <= i)
					{
						// This match started in this word.
						// Restart with character number two in the match.

						start_word_idx = i - characters_done + 1;
						OP_ASSERT(matches[match_i].characters_done == 0);
					}
					else
					{
						// This match started in another word.
						// Remove it and proceed with next partial match.

						RemovePartialMatch(match_i);
					}

					continue;
				}
			}

			if (characters_done && !partial_start_match.GetElement())
			{
				// Set start of partial match

				partial_start_match.SetLogicalPosition(html_element, word - html_element->TextContent() + start_char_offset);

				SelectionPointWordInfo partial_start_match_word_info(partial_start_match, FALSE);
				if (partial_start_match_word_info.GetWord() == word)
					// Next check need to start at the character following start of this match.

					start_word_idx = partial_start_match_word_info.GetOffsetIntoWord() + 1;
			}

			if (finished)
			{
				OP_ASSERT(match_i == 0);

				// Set end of match
				if (end_pending_word_finished.GetElement())
				{
					end_match = end_pending_word_finished;
					end_pending_word_finished.Reset();
				}
				else
					end_match.SetLogicalPosition(html_element, word - html_element->TextContent() + end_char_offset);

				document->GetHtmlDocument()->AddSearchHit(&matches[0].start_match, &end_match);

				RemovePartialMatch(0);

				finished = FALSE;
				end_match.Reset();
				characters_done = 0;
				pending_finished = FALSE;
				continue;
			}

			matches[match_i].characters_done = characters_done;

			match_i++;
		}

		finished = match_found;
	}
}

/* virtual */ void
SearchTextAllObject::HandleTextContent(LayoutProperties* layout_props,
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
	MatchAll(layout_props, box, word, word_length, word_width, space_width,
		justified_space_extra, word_info, virtual_pos);
}

/* virtual */ void
SearchTextAllObject::HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj)
{
	// Don't search in password fields. They are "secret".
	HTML_Element* elm = form_obj->GetHTML_Element();
	if (elm->Type() == Markup::HTE_INPUT && elm->GetInputType() == INPUT_PASSWORD)
		return;

	SEARCH_TYPE type = SEARCH_FROM_BEGINNING;
	form_obj->SetSelection(0, 0);

	while(TRUE)
	{
		INT32 old_selstart, old_selstop;
		form_obj->GetSelection(old_selstart, old_selstop);

		BOOL found = form_obj->GetWidget()->SearchText(text, uni_strlen(text), TRUE, match_case, match_words, type, TRUE, FALSE);

		if (found)
		{
			INT32 selstart, selstop;
			form_obj->GetSelection(selstart, selstop);
			if (old_selstart == selstart && old_selstop == selstop)
				break;

			// Only the character_offset is used for form selection
			start_match.SetLogicalPosition(elm, 0);
			end_match.SetLogicalPosition(elm, 0);

			document->GetHtmlDocument()->AddSearchHit(&start_match, &end_match);
			ResetSearch(TRUE);
		}
		else
			break;

		type = SEARCH_FROM_CARET;
	}

	form_obj->SetSelection(0, 0);
}

void
SearchTextAllObject::LeaveContext(const Line* line)
{
	if (!IsFinished())
	{
		FinishPendingMatches(line);
		if (IsFinished())
		{
			document->GetHtmlDocument()->AddSearchHit(GetStart(), GetEnd());
			ResetSearch(TRUE);
		}
	}
}

/* virtual */ void
SearchTextAllObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	LeaveContext(NULL);
}

/* virtual */ void
SearchTextAllObject::LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	LeaveContext(line);
}

HighlightUpdateObject::HighlightUpdateObject(FramesDocument* doc,
											 SelectionElm* first_hit,
											 BOOL select)
                                             : VisualTraversalObject(doc)
                                             , m_next_hit(first_hit)
											 , m_start_done(FALSE)
                                             , m_select(select)
{
	SetTraverseType(TRAVERSE_ONE_PASS);

	m_bounding_rect.left = LONG_MIN;
	m_bounding_rect.top = LONG_MIN;
	m_bounding_rect.right = LONG_MIN;
	m_bounding_rect.bottom = LONG_MIN;
}

/* virtual */ void
HighlightUpdateObject::EnterTextBox(LayoutProperties* layout_props,
									Text_Box* box,
									LineSegment& segment)
{
	if (!m_next_hit)
		return;

	if (TextSelection *selection = m_next_hit->GetSelection())
	{
		HTML_Element* html_element = layout_props->html_element;

		if (!m_start_done)
		{
			if (html_element != selection->GetStartElement())
				return;

			m_start_done = TRUE;
		}
	}
}

/* virtual */ void
HighlightUpdateObject::HandleTextContent(LayoutProperties* layout_props,
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
	if (!m_start_done || !m_next_hit)
		return;

	if (TextSelection* selection = m_next_hit->GetSelection())
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		HTML_Element* html_element = layout_props->html_element;

		OpRect local_rect(x,
						  baseline - props.font_ascent,
						  word_width,
						  props.font_ascent + props.font_descent + 1);

		OpRect box_rect = visual_device->ToBBox(local_rect);

		// start of selected text not found yet
		if (m_bounding_rect.left == LONG_MIN)
		{
			LayoutSelection selection_info(selection);
			if (selection_info.GetStartWord() != word)
				return;

			m_bounding_rect.left = box_rect.Left();
			m_bounding_rect.right = box_rect.Right();
		}

		if (box_rect.Right() > m_bounding_rect.right)
			m_bounding_rect.right = box_rect.Right();

		if (box_rect.Bottom() > m_bounding_rect.bottom)
			m_bounding_rect.bottom = box_rect.Bottom();

		while (html_element == selection->GetEndElement())
		{
			LayoutSelection selection_info(selection);
			if (word != selection_info.GetEndWord())
				break;

			if (box_rect.Right() > m_bounding_rect.right)
				m_bounding_rect.right = box_rect.Right();

			if (box_rect.Bottom() > m_bounding_rect.bottom)
				m_bounding_rect.bottom = box_rect.Bottom();

			visual_device->Update(m_bounding_rect.left,
								  m_bounding_rect.top,
								  m_bounding_rect.right - m_bounding_rect.left,
								  m_bounding_rect.bottom - m_bounding_rect.top, FALSE);

			selection->SetBoundingRect(m_bounding_rect);

			// reset the start
			m_bounding_rect.left = LONG_MIN;
			m_bounding_rect.top = LONG_MIN;
			m_bounding_rect.right = LONG_MIN;
			m_bounding_rect.bottom = LONG_MIN;

			m_start_done = FALSE;

			m_next_hit = (SelectionElm*) m_next_hit->Suc();
			if (m_next_hit)
			{
				selection = m_next_hit->GetSelection();

				if (html_element == selection->GetStartElement())
				{
					m_start_done = TRUE;

					LayoutSelection selection_new_info(selection);
					if (selection_new_info.GetStartWord() == word)
					{
						m_bounding_rect.left = box_rect.Left();
						m_bounding_rect.top = box_rect.Top();
						m_bounding_rect.right = box_rect.Right();
						m_bounding_rect.bottom = box_rect.Bottom();

						continue;
					}
				}
			}
			break;
		}
	}
}

/*virtual*/ void
HighlightUpdateObject::HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj)
{
	if (!m_next_hit)
		return;

	if (TextSelection* selection = m_next_hit->GetSelection())
	{
		HTML_Element* html_element = form_obj->GetHTML_Element();

		if (html_element == selection->GetStartElement())
		{
			AffinePos doc_pos;
			form_obj->GetPosInDocument(&doc_pos);

			OpRect bounds = form_obj->GetWidget()->GetBounds();
			doc_pos.Apply(bounds);

			RECT bounding_rect = { bounds.x, bounds.y, bounds.x + bounds.width - 1, bounds.y + bounds.height - 1 };

			do {
				m_next_hit->GetSelection()->SetBoundingRect(bounding_rect);
				m_next_hit = (SelectionElm*) m_next_hit->Suc();
			} while (m_next_hit && m_next_hit->GetSelection()->GetStartElement() == html_element);
		}
	}
}

/* virtual */ void
HighlightUpdateObject::HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content)
{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
	if (content->GetSVGContent())
	{
		SVGTreeIterator* iter;
		if (OpStatus::IsSuccess(g_svg_manager->GetHighlightUpdateIterator(layout_props->html_element, layout_props, m_next_hit, &iter)))
		{
			// we don't need to process the elements individually here, but we do need to loop through all of them
			while(iter->Next()) {
				;
			}

			g_svg_manager->ReleaseIterator(iter);
		}
	}
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

	VisualTraversalObject::HandleReplacedContent(layout_props, content);
}

/*virtual*/ BOOL
HighlightUpdateObject::EnterVerticalBox(LayoutProperties* parent_lprops,
										LayoutProperties*& layout_props,
										VerticalBox* box,
										TraverseInfo& traverse_info)
{
	if (!m_next_hit)
		return FALSE;

	if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
		return FALSE;

	return TRUE;
}

/*virtual*/ BOOL
HighlightUpdateObject::EnterInlineBox(LayoutProperties* layout_props,
									  InlineBox* box,
									  const RECT& box_area,
									  LineSegment& segment,
									  BOOL start_of_box,
									  BOOL end_of_box,
									  LayoutCoord baseline,
									  TraverseInfo& traverse_info)
{
	if (!m_next_hit)
		return FALSE;

	return TRUE;
}

/*virtual*/ BOOL
HighlightUpdateObject::EnterLine(LayoutProperties* parent_lprops,
								 const Line* line,
								HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (!m_next_hit)
		return FALSE;
	return TRUE;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Enter paged container. */

/* virtual */ BOOL
HighlightUpdateObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	return TRUE;
}

#endif // PAGED_MEDIA_SUPPORT

/*virtual*/ BOOL
HighlightUpdateObject::EnterTableRow(TableRowBox* row)
{
	if (!m_next_hit)
		return FALSE;
	return TRUE;
}

/*virtual*/ BOOL
HighlightUpdateObject::TraversePositionedElement(HTML_Element* element,
                                                 HTML_Element* containing_element)
{
	if (!m_next_hit)
		return FALSE;
	return TRUE;
}
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

/* virtual */ void
BlinkObject::HandleTextContent(LayoutProperties* layout_props,
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
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (props.text_decoration & TEXT_DECORATION_BLINK)
	{
		document->GetVisualDevice()->UpdateRelative(x,
			                                        baseline - props.font_ascent,
													word_width,
													props.font_ascent + props.font_descent + 1, FALSE);
	}
}

#ifdef LINK_TRAVERSAL_SUPPORT
//
// Implementation of the LinkTraversalObject
//

void
LinkTraversalObject::InsertCommonInfo(HTML_Element *elm, LayoutProperties* layout_props, URL *url, OpElementInfo::EIType type, LinkType kind)
{
	if (!m_op_elm_info && url && !url->IsEmpty())
	{
		m_op_elm_info = OP_NEW(OpElementInfo, (type, kind));

		if (m_op_elm_info)
		{
			m_op_elm_info->SetURL(url);
			m_op_elm_info->SetTitle(elm->GetStringAttr(Markup::HA_TITLE));
			m_op_elm_info->SetRel(elm->GetStringAttr(Markup::HA_REL));
			m_op_elm_info->SetRev(elm->GetStringAttr(Markup::HA_REV));

			if (layout_props)
			{
				m_op_elm_info->SetFontSize(layout_props->GetProps()->font_size);
				m_op_elm_info->SetFontWeight(layout_props->GetProps()->font_weight);
				m_op_elm_info->SetTextColor(layout_props->GetProps()->font_color);
			}

			m_vector->Add(m_op_elm_info);
			m_last_elm = elm;
		}
	}
}

void
LinkTraversalObject::CheckForLinkElements()
{
	const LinkElement *link_elm = document ? document->GetHLDocProfile()->GetLinkList() : NULL;
	while (link_elm)
	{
		unsigned int kinds = link_elm->GetKinds();
		unsigned int interesting_types =
			LINK_TYPE_ALTERNATE |
			LINK_TYPE_START |
			LINK_TYPE_NEXT |
			LINK_TYPE_PREV |
			LINK_TYPE_CONTENTS |
			LINK_TYPE_INDEX |
			LINK_TYPE_GLOSSARY |
			LINK_TYPE_COPYRIGHT |
			LINK_TYPE_CHAPTER |
			LINK_TYPE_SECTION |
			LINK_TYPE_SUBSECTION |
			LINK_TYPE_APPENDIX |
			LINK_TYPE_HELP |
			LINK_TYPE_BOOKMARK |
			LINK_TYPE_END |
			LINK_TYPE_FIRST |
			LINK_TYPE_LAST |
			LINK_TYPE_UP |
			LINK_TYPE_FIND |
			LINK_TYPE_AUTHOR |
			LINK_TYPE_MADE;

		// Filter out the types we're interested of.
		// Only interested in "alternate" if it's not a stylesheet.
		if ((kinds & LINK_GROUP_ALT_STYLESHEET) == LINK_GROUP_ALT_STYLESHEET)
			interesting_types &= ~LINK_TYPE_ALTERNATE;
		kinds = kinds & interesting_types;
		for (int bit = 0; bit < 32; bit++)
		{
			LinkType kind = static_cast<LinkType>(1 << bit);
			if (kinds & kind)
			{
				InsertCommonInfo(link_elm->GetElement(), NULL,
					link_elm->GetHRef(document->GetLogicalDocument()),
					OpElementInfo::LINK, kind);
			}
		}

		link_elm = link_elm->Suc();

		m_last_elm = NULL;
		m_op_elm_info = NULL;
	}
}

void
LinkTraversalObject::CheckForCLink(HTML_Element *elm, LayoutProperties* layout_props)
{
	URL* css_url = (URL *)elm->GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, (void*)NULL, SpecialNs::NS_LOGDOC);

	InsertCommonInfo(elm, layout_props, css_url, OpElementInfo::CLINK);
}

/* virtual */ BOOL
LinkTraversalObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	HTML_Element* this_elm = box->GetHtmlElement();

	if (m_last_elm != this_elm)
	{
		if (this_elm->Type() == Markup::HTE_DOC_ROOT)
			CheckForLinkElements();
		else
			CheckForCLink(this_elm, layout_props);
	}

	if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
		return FALSE;

	return TRUE;
}

/* virtual */ void
LinkTraversalObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (m_last_elm == layout_props->html_element)
	{
		m_last_elm = NULL;
		m_op_elm_info = NULL;
	}
}

/* virtual */ void
LinkTraversalObject::HandleTextContent(LayoutProperties* layout_props,
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
	if (m_op_elm_info && word_info.GetStart() == 0)
		m_op_elm_info->AppendText(word);
}

/* virtual */ BOOL
LinkTraversalObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	HTML_Element* this_elm = layout_props->html_element;

	if (m_last_elm != this_elm)
	{
		switch (this_elm->GetNsType())
		{
		case NS_HTML:
			switch (this_elm->Type())
			{
			case Markup::HTE_A:
				InsertCommonInfo(this_elm, layout_props, this_elm->GetA_URL(document->GetLogicalDocument()), OpElementInfo::A);
				return TRUE;
			}
			break;

#ifdef _WML_SUPPORT_
		case NS_WML:
			switch (this_elm->Type())
			{
			case WE_GO:
				InsertCommonInfo(this_elm, layout_props, this_elm->GetUrlAttr(Markup::HA_HREF, NS_IDX_HTML, document->GetLogicalDocument()), OpElementInfo::GO);
				return TRUE;
			}
			break;
#endif // _WML_SUPPORT_
		}

		CheckForCLink(this_elm, layout_props);
	}

	return TRUE;
}

/* virtual */ void
LinkTraversalObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
	if (m_last_elm == layout_props->html_element)
	{
		m_op_elm_info = NULL;
		m_last_elm = NULL;
	}
}

#endif // LINK_TRAVERSAL_SUPPORT

/** Update root translation and root scroll to regular translation and scroll. */

void TraversalObject::SyncRootScrollAndTranslation(RootTranslationState& translation_state)
{
	GetRootTranslation(translation_state.translate_root_x, translation_state.translate_root_y);
	translation_state.translate_root_x = GetTranslationX() - translation_state.translate_root_x;
	translation_state.translate_root_y = GetTranslationY() - translation_state.translate_root_y;
	TranslateRoot(translation_state.translate_root_x, translation_state.translate_root_y);
	SyncRootScroll(translation_state.root_scroll_x, translation_state.root_scroll_y);
}

/** Restore previous root translation and root scroll. */

void TraversalObject::RestoreRootScrollAndTranslation(const RootTranslationState& translation_state)
{
	TranslateRoot(-translation_state.translate_root_x, -translation_state.translate_root_y);
	TranslateRootScroll(-translation_state.root_scroll_x, -translation_state.root_scroll_y);
}

/** Traverse document. */

void TraversalObject::StartingTraverse()
{
}

void TraversalObject::TraverseFinished()
{
}

OP_STATUS
TraversalObject::Traverse(HTML_Element* root, Head* props_list, BOOL allow_reflow)
{
	OP_PROBE6(OP_PROBE_TRAVERSALOBJECT_TRAVERSE);

	// Multi-pass traversal in logical order makes no sense:

	OP_ASSERT(GetTraverseType() == TRAVERSE_ONE_PASS || !TraverseInLogicalOrder());

	if (!document->IsWaitingForStyles())
	{
		LayoutWorkplace *layout_workplace = document->GetLogicalDocument()->GetLayoutWorkplace();

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
		if (AllowDirtyTraverse())
			allow_reflow = FALSE;
#endif // LAYOUT_TRAVERSE_DIRTY_TREE

		if (allow_reflow)
		{
			BOOL has_dirty_content_sized_iframe_children = layout_workplace->HasDirtyContentSizedIFrameChildren();

			if (!document->IsPrintDocument() && (root->IsDirty() || has_dirty_content_sized_iframe_children
												 || root->HasDirtyChildProps() || root->IsPropsDirty()
												 || layout_workplace->GetYieldElement()
					))
			{
				// Reflow
				OP_STATUS status = document->Reflow(FALSE, TRUE);

				if (status == OpStatus::ERR_NO_MEMORY)
				{
					SetOutOfMemory();
					return OpStatus::ERR_NO_MEMORY;
				}
				else if (status == OpStatus::ERR_YIELD)
				{
					return OpStatus::ERR_YIELD;
				}
			}

			if (HTML_Document* html_doc = document->GetHtmlDocument())
				/* This is not a very good place to scroll, but luckily, in most cases, the
				   scrolling has already been done, so that nothing will happen now. However, in
				   some situations for smart-frames, scrolling to the saved element may be delayed,
				   because the frameset size wasn't known at the time of reflowing this
				   document. */

				html_doc->ScrollToSavedElement();
		}

		fixed_pos_x = layout_workplace->GetLayoutViewX();
		fixed_pos_y = layout_workplace->GetLayoutViewY();

#ifdef PAGED_MEDIA_SUPPORT
		if (PageDescription* current_page = document->GetCurrentPage())
			fixed_pos_print_y = LayoutCoord(current_page->GetPageTop());
#endif

		if (AbsolutePositionedBox* box = (AbsolutePositionedBox*) root->GetLayoutBox())
		{
			OP_ASSERT(box->IsAbsolutePositionedBox());

			AutoDeleteHead my_props_list;
			LayoutProperties* layout_props = NULL;

			if (!props_list)
				props_list = &my_props_list;
			else
			{
				// If there are existing cascade entries, attach to them.

				layout_props = (LayoutProperties*) props_list->Last();

				if (layout_props)
					/* Look for the nearest ancestor of "root". If no such
					   thing exists, we expect that there are no elements at
					   all in the cascade. */

					while (layout_props->Pred() && (!layout_props->html_element || root->IsAncestorOf(layout_props->html_element)))
						layout_props = layout_props->Pred();

				// Verify that the ancestry is sane, or we're in for a bumpy ride.

				OP_ASSERT(!layout_props || !layout_props->html_element || layout_props->html_element->IsAncestorOf(root));
			}

			if (!layout_props)
			{
				/* Create a dummy cascade entry, so that we have something to
				   pass to the box traversal below. */

				layout_props = new LayoutProperties();

				if (!layout_props)
				{
					SetOutOfMemory();
					return OpStatus::ERR_NO_MEMORY;
				}

				layout_props->Into(props_list);
			}

			document->GetVisualDevice()->Reset();

			GetViewportScroll(scroll_x, scroll_y);
			root_scroll_x = scroll_x;
			root_scroll_y = scroll_y;

			// Set UA default values.

			HTMLayoutProperties& props = *layout_props->GetProps();

			props.Reset(NULL, document->GetHLDocProfile());

			props.current_bg_color = layout_workplace->GetDocRootProperties().bg_color;
			if (props.current_bg_color == USE_DEFAULT_COLOR)
				props.current_bg_color = colorManager->GetBackgroundColor();

			// Traverse the tree.

			OP_ASSERT(!layout_workplace->IsTraversing()); // Nested traversing is not allowed
			layout_workplace->SetIsTraversing(TRUE);

			StartingTraverse();
			box->Traverse(this, layout_props, NULL);
			TraverseFinished();

			layout_workplace->SetIsTraversing(FALSE);
		}
	}

	return IsOutOfMemory() ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}

/** Get amount of root viewport scroll that is to be canceled by fixed-positioned elements. */

void
TraversalObject::GetViewportScroll(LayoutCoord &x, LayoutCoord &y) const
{
	y = LayoutCoord(0);
	x = LayoutCoord(0);

#ifdef _PRINT_SUPPORT_
	if (document->IsPrintDocument())
		y = fixed_pos_print_y;
	else
#endif // _PRINT_SUPPORT_
		y = fixed_pos_y;

	x = fixed_pos_x;
}


#define TCTO_LINE_DIFF_THRESHOLD 160
#define TCTO_LINE_DIFF_VERTICAL_LIMIT 160
#define TCTO_IMPORTANT_CONTENT_HORIZONTAL_THRESHOLD 60
#define TCTO_IMPORTANT_CONTENT_VERTICAL_THRESHOLD 60
#define TCTO_TITLE_HORIZONTAL_THRESHOLD 40
#define TCTO_TITLE_VERTICAL_THRESHOLD 20

OP_STATUS TextContainerTraversalObject::CommitContainerRectangle(BOOL force, Markup::Type t)
{
	/* content with 4 words or more, replaced content of a certain size or content with
	   one word or more (headings) of a certain size. */
	if (text_content_count > 3 || force ||
		(text_content_count > 0 &&
		 pending_rectangle.width > TCTO_TITLE_HORIZONTAL_THRESHOLD &&
		 pending_rectangle.height > TCTO_TITLE_VERTICAL_THRESHOLD))
	{
		OpRectListItem* item = OP_NEW(OpRectListItem, (pending_rectangle));

		if (!item)
			return OpStatus::ERR_NO_MEMORY;

		item->Into(&list);
	}

	pending_rectangle.Empty();
	text_content_count = 0;

	return OpStatus::OK;
}

/* ReplacedContent larger than a certain threshold or form content deserve
 * their own rect */

BOOL TextContainerTraversalObject::ImportantContent(ReplacedContent* c)
{
	if (!c)
		return FALSE;

	if (c->IsForm() || (c->GetWidth()> TCTO_IMPORTANT_CONTENT_HORIZONTAL_THRESHOLD && c->GetHeight() > TCTO_IMPORTANT_CONTENT_VERTICAL_THRESHOLD))
		return TRUE;

	return FALSE;
}

void TextContainerTraversalObject::HandleLineContent(const OpRect& box_rect, LayoutCoord line_height /* = LayoutCoord(0) */)
{
	if (starting_line)
	{
		int diff = op_abs(box_rect.Left() - pending_rectangle.x);

		if (pending_rectangle.height > TCTO_LINE_DIFF_VERTICAL_LIMIT && diff > TCTO_LINE_DIFF_THRESHOLD)
			if (OpStatus::IsMemoryError(CommitContainerRectangle(FALSE, Markup::HTE_TEXT)))
				SetOutOfMemory();
	}

	starting_line = FALSE;

	if (line_height)
	{
		if (text_content_count == 0)
		{
			pending_rectangle.x = box_rect.Left();
			pending_rectangle.y = box_rect.Top();
		}

		if (pending_rectangle.Right() < box_rect.Right())
			pending_rectangle.width = box_rect.Right() - pending_rectangle.x;

		if (box_rect.Left() < pending_rectangle.Left())
		{
			pending_rectangle.width += pending_rectangle.Left() - box_rect.Left();
			pending_rectangle.x = box_rect.Left();
		}

		pending_rectangle.height = static_cast<INT32>(box_rect.Top() - pending_rectangle.y + line_height);
	}
	else // line_height isn't of much use in case of transforms, fallback to ordinary union
		pending_rectangle.UnionWith(box_rect);

	text_content_count++;
}

/*virtual*/ BOOL
TextContainerTraversalObject::HandleVerticalBox(LayoutProperties* parent_lprops,
												LayoutProperties*& layout_props,
												VerticalBox* box,
												TraverseInfo& traverse_info,
												BOOL clip_affects_traversed_descendants)
{
	if (CommitContainerRectangle(FALSE, Markup::HTE_TEXT) == OpStatus::ERR_NO_MEMORY)
	{
		SetOutOfMemory();
		return FALSE;
	}

	if (box->IsContentReplaced())
	{
		ReplacedContent* c = (ReplacedContent*)box->GetContent();

		if (ImportantContent(c))
		{
			OpRect local_rect(0, 0, c->GetWidth(), c->GetHeight());
			pending_rectangle = visual_device_transform.ToBBox(local_rect);

			if (CommitContainerRectangle(TRUE, box->GetHtmlElement()->Type()) == OpStatus::ERR_NO_MEMORY)
			{
				SetOutOfMemory();
				return FALSE;
			}

		}
	}
	return TRUE;
}

void TextContainerTraversalObject::HandleTextContent(LayoutProperties* layout_props,
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
	if (!word_width || !word_length)
		return;

	OpRect local_rect(x, 0,
					  word_width + space_width,
					  line_height);

	LayoutCoord line_height_final;
#ifdef CSS_TRANSFORMS
	line_height_final = visual_device_transform.GetCTM().IsTransform() ? LayoutCoord(0) : line_height;
#else // !CSS_TRANSFORMS
	line_height_final = line_height;
#endif // CSS_TRANSFORMS

	HandleLineContent(visual_device_transform.ToBBox(local_rect), line_height_final);
}

/** Leave vertical box */
void TextContainerTraversalObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	HitTestingTraversalObject::LeaveVerticalBox(layout_props, box, traverse_info);

	BOOL force = FALSE;

	if (CommitContainerRectangle(force, Markup::HTE_TEXT) == OpStatus::ERR_NO_MEMORY)
		SetOutOfMemory();
}

/** Enter inline box */
BOOL TextContainerTraversalObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (box->IsContentReplaced())
	{
		ReplacedContent* c = (ReplacedContent*)box->GetContent();

		BOOL restart = FALSE;

		if (starting_line)
		{
			int diff = op_abs(GetTranslationX() - pending_rectangle.x);

			if (pending_rectangle.height > TCTO_LINE_DIFF_VERTICAL_LIMIT && diff > TCTO_LINE_DIFF_THRESHOLD)
				restart = TRUE;
		}

		starting_line = FALSE;

		if (ImportantContent(c))
			restart = TRUE;

		if (restart && CommitContainerRectangle(FALSE, Markup::HTE_TEXT) == OpStatus::ERR_NO_MEMORY)
		{
			SetOutOfMemory();
			return FALSE;
		}

		if (ImportantContent(c))
		{
			OpRect local_rect(box_area.left, box_area.top, c->GetWidth(), c->GetHeight());
			pending_rectangle = visual_device_transform.ToBBox(local_rect);

			if (CommitContainerRectangle(TRUE, layout_props->html_element->Type()) == OpStatus::ERR_NO_MEMORY)
			{
				SetOutOfMemory();
				return FALSE;
			}
		}
	}
	else if (box->GetBulletContent())
	{
		OpRect bullet_rect(box_area.left, box_area.right, box->GetWidth(), box->GetHeight());
		LayoutCoord line_height;
#ifdef CSS_TRANSFORMS
		line_height = visual_device_transform.GetCTM().IsTransform() ? LayoutCoord(0) : segment.line->GetLayoutHeight();
#else // !CSS_TRANSFORMS
		line_height = segment.line->GetLayoutHeight();
#endif // CSS_TRANSFORMS

		HandleLineContent(visual_device_transform.ToBBox(bullet_rect), line_height);
	}

	return HitTestingTraversalObject::EnterInlineBox(layout_props, box, box_area, segment, start_of_box, end_of_box, baseline, traverse_info);
}

/* virtual */
BOOL TextContainerTraversalObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
	if (HitTestingTraversalObject::EnterLine(parent_lprops, line, containing_element, traverse_info))
	{
		starting_line = TRUE;
		return TRUE;
	}
	return FALSE;
}

#ifdef RESERVED_REGIONS
ReservedRegionTraversalObject::ReservedRegionTraversalObject(FramesDocument* doc, const RECT& area, OpRegion& region)
	: HitTestingTraversalObject(doc, area), reserved_region(region), table_row_ancestor_count(0)
{
	SetTraverseType(TRAVERSE_ONE_PASS);
}

/*virtual*/ BOOL
ReservedRegionTraversalObject::HandleVerticalBox(LayoutProperties* parent_lprops,
												 LayoutProperties*& layout_props,
												 VerticalBox* box,
												 TraverseInfo& traverse_info,
												 BOOL clip_affects_traversed_descendants)
{
	BOOL test_ancestors = table_row_ancestor_count > 0 || box->IsFloatingBox() || box->IsAbsolutePositionedBox();
	const HTMLayoutProperties& props = *layout_props->GetProps();

	if (props.visibility == CSS_VALUE_visible)
	{
		BOOL is_reserved_element = HasReservedEventHandler(layout_props->html_element, test_ancestors);
		BOOL is_svg_element = FALSE;
#ifdef SVG_SUPPORT
		is_svg_element = layout_props->html_element->IsMatchingType(Markup::SVGE_SVG, NS_SVG);
#endif // SVG_SUPPORT

		if (is_reserved_element || is_svg_element)
		{
			OpRect rect(0, 0, box->GetWidth(), box->GetHeight());

			if (clip_affects_traversed_descendants)
				rect.IntersectWith(box->GetClippedRect(props, FALSE));

			if (is_reserved_element)
				AddRectangle(OpRect(rect.x, rect.y, rect.width, rect.height));
#ifdef SVG_SUPPORT
			else
				ProcessSVGElement(layout_props->html_element, rect);
#endif // SVG_SUPPORT
		}
	}

	return !IsOutOfMemory();
}

/* virtual */ BOOL
ReservedRegionTraversalObject::HandleScrollable(const HTMLayoutProperties& props,
												ScrollableArea* scrollable,
												LayoutCoord width,
												LayoutCoord height,
												TraverseInfo& traverse_info,
												BOOL clip_affects_target,
												int scrollbar_modifier)
{
	if (scrollable->HasHorizontalScrollbar())
	{
		int x = props.border.left.width;
		if (scrollable->LeftHandScrollbar())
			x += scrollable->GetVerticalScrollbarWidth();

		AddRectangle(OpRect(x, props.border.top.width + height, width, scrollable->GetHorizontalScrollbarWidth()));
	}

	if (scrollable->HasVerticalScrollbar())
	{
		int x = props.border.left.width;
		if (!scrollable->LeftHandScrollbar())
			x += width;

		AddRectangle(OpRect(x, props.border.top.width, scrollable->GetVerticalScrollbarWidth(), height));
	}

	return TRUE;
}

/* virtual */ BOOL
ReservedRegionTraversalObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& position, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (HitTestingTraversalObject::EnterInlineBox(layout_props, box, box_area, position, start_of_box, end_of_box, baseline, traverse_info))
	{
		BOOL test_ancestors = table_row_ancestor_count > 0 || box->IsFloatingBox() || box->IsAbsolutePositionedBox();
		const HTMLayoutProperties& props = *layout_props->GetProps();

		if (props.visibility == CSS_VALUE_visible)
		{
			BOOL is_reserved_element = HasReservedEventHandler(layout_props->html_element, test_ancestors);
			BOOL is_svg_element = FALSE;
#ifdef SVG_SUPPORT
			is_svg_element = layout_props->html_element->IsMatchingType(Markup::SVGE_SVG, NS_SVG);
#endif // SVG_SUPPORT

			if (is_reserved_element || is_svg_element)
			{
				OpRect rect(box_area.left, box_area.top, box_area.right - box_area.left, box_area.bottom - box_area.top);

				if (props.overflow_x != CSS_VALUE_visible && !box->IsInlineContent())
					rect.IntersectWith(box->GetClippedRect(props, FALSE));

				if (is_reserved_element)
					AddRectangle(OpRect(rect.x, rect.y, rect.width, rect.height));
#ifdef SVG_SUPPORT
				else
					ProcessSVGElement(layout_props->html_element, rect);
#endif // SVG_SUPPORT

			}
		}

		return !IsOutOfMemory();
	}

	return FALSE;
}

/* virtual */ BOOL
ReservedRegionTraversalObject::EnterTableRow(TableRowBox* row)
{
	if (HitTestingTraversalObject::EnterTableRow(row))
	{
		table_row_ancestor_count++;
		return TRUE;
	}

	return FALSE;
}

/** Enter scrollable content. */

/* virtual */ BOOL
ReservedRegionTraversalObject::EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	if (HitTestingTraversalObject::EnterScrollable(props, scrollable, width, height, traverse_info))
	{
		int scrollbar_modifier = scrollable->LeftHandScrollbar() ? scrollable->GetVerticalScrollbarWidth() : 0;
		OpRect clip_rect = visual_device_transform.ToBBox(OpRect(props.border.left.width + scrollbar_modifier, props.border.top.width, width, height));

		if (!clip_rects.Empty())
			clip_rect.IntersectWith(clip_rects.Last()->rect);

		/* HitTestingTraversalObject does not enter transforms with zero determinants and does not enter stacking clipping
		   when they do not intersect. */
		OP_ASSERT(!clip_rect.IsEmpty());

		if (ReservedRegionClipRect* clip_rect_ptr = OP_NEW(ReservedRegionClipRect, (clip_rect)))
		{
			clip_rect_ptr->Into(&clip_rects);
			return TRUE;
		}

		SetOutOfMemory();
	}

	return FALSE;
}

/*virtual*/void
ReservedRegionTraversalObject::LeaveScrollable(TraverseInfo& traverse_info)
{
	HitTestingTraversalObject::LeaveScrollable(traverse_info);

	ReservedRegionClipRect* inner = clip_rects.Last();
	inner->Out();
	OP_DELETE(inner);
}

/* virtual */ void
ReservedRegionTraversalObject::LeaveTableRow(TableRowBox* row, TableContent* table)
{
	HitTestingTraversalObject::LeaveTableRow(row, table);

	OP_ASSERT(table_row_ancestor_count > 0);
	table_row_ancestor_count--;
}

#ifdef PAGED_MEDIA_SUPPORT

/* virtual */ BOOL
ReservedRegionTraversalObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();

	int padding_box_x = props.border.left.width;
	int padding_box_y = props.border.top.width;
	int padding_box_width = content->GetWidth() - props.border.left.width - props.border.right.width;
	int padding_box_height = content->GetHeight() - props.border.top.width - props.border.bottom.width;

	AddRectangle(OpRect(padding_box_x, padding_box_y, padding_box_width, padding_box_height));

	return HitTestingTraversalObject::EnterPagedContainer(layout_props, content, width, height, traverse_info);
}

#endif // PAGED_MEDIA_SUPPORT

BOOL
ReservedRegionTraversalObject::HasReservedEventHandler(HTML_Element* element, BOOL test_ancestors)
{
	for (int i = 0; g_reserved_region_types[i] != DOM_EVENT_NONE; i++)
		if (element->HasEventHandler(document, g_reserved_region_types[i], !test_ancestors))
			return TRUE;

	return FALSE;
}

void
ReservedRegionTraversalObject::AddRectangle(const OpRect& rect)
{
	OpRect rect_copy = rect;
	const OpRect* clip_rect_ptr = GetClipRect();

	if (clip_rect_ptr)
		rect_copy.IntersectWith(*clip_rect_ptr);

	if (!rect_copy.IsEmpty())
	{
		OpRect bbox = visual_device_transform.ToBBox(rect_copy);

		// HitTestingTraversalObject is not entering transforms contexts with determinants equal to zero.
		OP_ASSERT(!bbox.IsEmpty());

		/* FIXME. Intersect with additional own clip rect. That is redundant to htto clipping functionality and should be fixed
		   by moving the additional clip stack functionality to htto. */
		if (!clip_rects.Empty())
			bbox.IntersectWith(clip_rects.Last()->rect);

		if (!bbox.IsEmpty() && !reserved_region.IncludeRect(bbox))
			SetOutOfMemory();
	}
}

#ifdef SVG_SUPPORT
void
ReservedRegionTraversalObject::ProcessSVGElement(HTML_Element* element, OpRect& clip_rect)
{
	SVGTreeIterator* it;

	if (OpStatus::IsSuccess(g_svg_manager->GetReservedRegionIterator(element, &clip_rect, &it)))
	{
		OpRect svg_rect;

		while (HTML_Element* elm = it->Next())
			if (OpStatus::IsSuccess(g_svg_manager->GetNavigationData(elm, svg_rect)))
			{
				svg_rect.IntersectWith(clip_rect);
				AddRectangle(OpRect(svg_rect.x, svg_rect.y, svg_rect.width, svg_rect.height));
			}

		g_svg_manager->ReleaseIterator(it);
	}
}
#endif // SVG_SUPPORT
#endif // RESERVED_REGIONS

/*static*/ OP_STATUS
CoreViewFinder::TraverseWithFixedPositioned(FramesDocument* doc,
											const OpRect& area,
											OpPointerHashTable<HTML_Element, CoreView>& core_views,
											OpPointerSet<HTML_Element>& fixed_position_subtrees)
{
	HTML_Element* doc_root = doc->GetDocRoot();
	OP_ASSERT(doc_root);
	CoreViewFinder cvf(doc, area, core_views, fixed_position_subtrees);
	RETURN_IF_MEMORY_ERROR(cvf.Traverse(doc_root));
	AutoDeleteHead props_list;
	OpAutoPtr<OpHashIterator> iter(fixed_position_subtrees.GetIterator());
	RETURN_OOM_IF_NULL(iter.get());

	/* Iterate while we still have fixed positioned subtrees that were
	   not reached. We need to do some special settings for cvf object
	   for each traverse starting from a fixed positioned element instead of
	   the root. */
	while (OpStatus::IsSuccess(iter->First()))
	{
		HTML_Element* fixed_root = static_cast<HTML_Element*>(iter->GetData());
		AbsolutePositionedBox* box = static_cast<AbsolutePositionedBox*>(fixed_root->GetLayoutBox());
		OP_ASSERT(box->IsFixedPositionedBox());
		LayoutCoord initial_translation_x = LayoutCoord(0);
		LayoutCoord initial_translation_y = LayoutCoord(0);

		if (box->GetHorizontalOffset() == NO_HOFFSET || box->GetVerticalOffset() == NO_VOFFSET)
		{
			LayoutCoord offset_x = LayoutCoord(0);
			LayoutCoord offset_y = LayoutCoord(0);
			int res = fixed_root->Parent()->GetLayoutBox()->GetOffsetFromAncestor(offset_x, offset_y, NULL, Box::GETPOS_ABORT_ON_INLINE | Box::GETPOS_IGNORE_SCROLL);
			if (res & (Box::GETPOS_INLINE_FOUND
#ifdef CSS_TRANSFORMS
					| Box::GETPOS_TRANSFORM_FOUND
#endif
					))
			{
				// Too expensive to determine the correct offsets.
				OpStatus::Ignore(fixed_position_subtrees.Remove(fixed_root));
				continue;
			}

			if (box->GetHorizontalOffset() == NO_HOFFSET)
				initial_translation_x = offset_x;

			if (box->GetVerticalOffset() == NO_VOFFSET)
				initial_translation_y = offset_y;
		}

		LayoutProperties* cascade = LayoutProperties::CreateCascade(fixed_root->Parent(), props_list, doc->GetHLDocProfile());
		RETURN_OOM_IF_NULL(cascade);
		cvf.Translate(initial_translation_x, initial_translation_y);
		box->Traverse(&cvf, cascade, box->GetContainingElement());
		cvf.Translate(-initial_translation_x, -initial_translation_y);
		props_list.Clear();
		if (cvf.IsOutOfMemory())
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/** Enter vertical box */

BOOL
CoreViewFinder::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	BOOL enter = Intersects(box);

	if (box->IsFixedPositionedBox())
		OpStatus::Ignore(fixed_position_subtrees.Remove(box->GetHtmlElement()));

	if (enter)
	{
		if (box->HasCoreView())
		{
			HTML_Element* elm = box->GetHtmlElement();

			if (core_views.Contains(elm))
				if (Intersects(box, FALSE))
				{
					CoreView* dummy;
					core_views.Remove(elm, &dummy);
				}
		}

		if (!layout_props && !TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

/** Enter inline box */

BOOL
CoreViewFinder::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	if (box->HasCoreView())
	{
		HTML_Element* elm = box->GetHtmlElement();

		if (core_views.Contains(elm))
			if (Intersects(box_area))
			{
				CoreView* dummy;
				core_views.Remove(elm, &dummy);
			}
	}

	return TRUE;
}

#ifdef LAYOUT_WIDTHTRAVERSALOBJECT

//
// Implementation of the WidthTraversalObject
//

/* virtual */ BOOL
WidthTraversalObject::HandleScrollable(const HTMLayoutProperties& props,
									   ScrollableArea* scrollable,
									   LayoutCoord width,
									   LayoutCoord height,
									   TraverseInfo& traverse_info,
									   BOOL clip_affects_target,
									   int scrollbar_modifier)
{
	if (scrollable->HasHorizontalScrollbar())
	{
		HandleWidth(translation_x, translation_x + scrollable->GetOwningBox()->GetWidth());

		return FALSE;
	}
	else
		return TRUE;
}

void WidthTraversalObject::HandleReplacedContent(LayoutProperties* layout_props, ReplacedContent* content)
{
	HandleWidth(translation_x, translation_x + content->GetWidth());
}

BOOL WidthTraversalObject::EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info)
{
	if (HitTestingTraversalObject::EnterTableContent(layout_props, content, table_top, table_height, traverse_info))
	{
		HandleWidth(translation_x, translation_x + content->GetWidth());
		return TRUE;
	}

	return FALSE;
}

void WidthTraversalObject::HandleSelectableBox(LayoutProperties* layout_props)
{
	HandleWidth(translation_x, translation_x + layout_props->html_element->GetLayoutBox()->GetWidth());
}

BOOL WidthTraversalObject::GetHorizontalRange(long &l, long &r)
{
	if (left == right) // no content encountered
		return FALSE;

	l = left;
	r = right;

	return TRUE;
}

BOOL WidthTraversalObject::GetRecommendedHorizontalRange(long &l, long &r)
{
	static const int typical_size[] = { 240, 320, 480, 640, 800, 1024, 1280 };

	if (left == right) // no content encountered
		return FALSE;

	long width = right - left;

	int size_found = 0;
	for (size_t i = 0; i < sizeof(typical_size)/sizeof(*typical_size); ++i)
	{
		if (width >= typical_size[i] * 0.85 && width <= typical_size[i] * 1.05)
		{
			size_found = typical_size[i];
			break;
		}
	}

	if (size_found)
	{
		long root_width = GetDocument()->GetLogicalDocument()->GetRoot()->GetLayoutBox()->GetWidth();
		if (left <= size_found * 0.1) // left-aligned
		{
			l = 0;
			r = MAX(right, size_found);
			return TRUE;
		}
		else if (op_abs(right + left - root_width) <= size_found * 0.1) // centered
		{
			l = (root_width - size_found) / 2;
			r = (root_width + size_found) / 2;
			if (l > left)
			{
				r += l - left;
				l = left;
			}
			else if (r < right)
			{
				l += r - right;
				r = right;
			}
			return TRUE;
		}
	}
	return FALSE;
}

#endif // LAYOUT_WIDTHTRAVERSALOBJECT
