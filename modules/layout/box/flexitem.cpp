/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/layout/box/flexitem.h"

#include "modules/layout/content/flexcontent.h"
#include "modules/layout/traverse/traverse.h"

FlexItemBox::FlexItemBox(HTML_Element* element, Content* content)
	: VerticalBox(element, content),
	  flexorder_prev(NULL),
	  flexorder_next(NULL),
	  z_element(element),
	  left(0),
	  top(0),
	  minimum_margin_height(0),
	  hypothetical_margin_height(0),
	  x(0),
	  y(0),
	  new_main_margin_edge(0),
	  new_cross_margin_edge(0),
	  new_main_margin_size(1000), // any initial value will do, but let's pick something common (that the layout engine is optimized for)
	  new_cross_margin_size(1000), // any initial value will do, but let's pick something common (that the layout engine is optimized for)
	  flexitem_packed_init(0)
{
}

/* virtual */
FlexItemBox::~FlexItemBox()
{
	Out();
}

/** Take this item out from the layout stack. */

void
FlexItemBox::Out()
{
	FlexItemList* layout_stack = (FlexItemList*) GetList();

	if (!layout_stack)
	{
		// Not in any stack; nothing to do

		OP_ASSERT(flexorder_next == NULL);
		OP_ASSERT(flexorder_prev == NULL);

		return;
	}

	// Remove logically

	Link::Out();

	// Remove from 'order' sorting.

	if (layout_stack->flexorder_first == this)
		layout_stack->flexorder_first = flexorder_next;

	if (layout_stack->flexorder_last == this)
		layout_stack->flexorder_last = flexorder_prev;

	if (flexorder_next)
		flexorder_next->flexorder_prev = flexorder_prev;

	if (flexorder_prev)
		flexorder_prev->flexorder_next = flexorder_next;

	flexorder_next = NULL;
	flexorder_prev = NULL;
}

/** Add this item to a layout stack. */

void
FlexItemBox::Into(FlexItemList* layout_stack)
{
	FlexItemBox* prev = layout_stack->Last(FALSE);

	OP_ASSERT(!InList());
	OP_ASSERT(flexorder_prev == NULL);
	OP_ASSERT(flexorder_next == NULL);

	// Insert logically as last item.

	Link::Into(layout_stack);

	// Insert at the appropriate position, sorted by 'order'.

	while (prev && prev->GetOrder() > order)
		prev = prev->flexorder_prev;

	if (prev)
	{
		flexorder_next = prev->flexorder_next;
		flexorder_prev = prev;

		prev->flexorder_next = this;

		if (flexorder_next)
			flexorder_next->flexorder_prev = this;
	}
	else
	{
		if (layout_stack->flexorder_first)
		{
			flexorder_next = layout_stack->flexorder_first;
			layout_stack->flexorder_first->flexorder_prev = this;
		}

		layout_stack->flexorder_first = this;
	}

	if (!flexorder_next)
		layout_stack->flexorder_last = this;
}

/** Is this box (and potentially also its children) columnizable? */

/* virtual */ BOOL
FlexItemBox::IsColumnizable(BOOL require_children_columnizable) const
{
	/* The spec says that flex items may be broken inside, but I say it's too
	   much of a hassle, with our current pagination and columnization
	   design. :) */

	return FALSE;
}

/** Return TRUE if this is a special anonymous flex item for an absolutely positioned child. */

BOOL
FlexItemBox::IsAbsolutePositionedSpecialItem() const
{
	HTML_Element* html_element = GetHtmlElement();

	if (html_element->Type() != Markup::LAYOUTE_ANON_FLEX_ITEM)
		return FALSE;

	for (HTML_Element* child = html_element->FirstChild(); child; child = child->Suc())
		if (Box* child_box = child->GetLayoutBox())
			return child_box->IsAbsolutePositionedBox();

	return FALSE;
}

/** Set page and column breaking policies on this flex item box. */

void
FlexItemBox::SetBreakPolicies(BREAK_POLICY page_break_before, BREAK_POLICY column_break_before,
							  BREAK_POLICY page_break_after, BREAK_POLICY column_break_after)
{
	flexitem_packed.page_break_before = page_break_before;
	flexitem_packed.column_break_before = column_break_before;
	flexitem_packed.page_break_after = page_break_after;
	flexitem_packed.column_break_after = column_break_after;

	/* The column break bitfields cannot hold 'left' and 'right' values (and
	   they are meaningless in that context anyway): */

	OP_ASSERT(column_break_before != BREAK_LEFT && column_break_before != BREAK_RIGHT);
	OP_ASSERT(column_break_after != BREAK_LEFT && column_break_after != BREAK_RIGHT);
}

/** Get flexed (horizontal flexbox) or stretched (vertical flexbox) border box width. */

LayoutCoord
FlexItemBox::GetFlexWidth(LayoutProperties* cascade) const
{
	LayoutCoord flex_width;

	if (flexitem_packed.vertical)
		flex_width = new_cross_margin_size - margin_left - margin_right;
	else
		flex_width = new_main_margin_size - margin_left - margin_right;

	if (flex_width < 0)
		flex_width = LayoutCoord(0);

	return flex_width;
}

/** Get flexed (vertical flexbox) or stretched (horizontal flexbox) border box height. */

LayoutCoord
FlexItemBox::GetFlexHeight(LayoutProperties* cascade) const
{
	LayoutCoord stretch_height;

	if (flexitem_packed.vertical)
		stretch_height = new_main_margin_size;
	else
		if (flexitem_packed.stretch)
			stretch_height = new_cross_margin_size;
		else
			return CONTENT_HEIGHT_AUTO;

	const HTMLayoutProperties& props = *cascade->GetProps();

	stretch_height -= props.margin_top + props.margin_bottom;

	if (stretch_height < 0)
		stretch_height = LayoutCoord(0);

	return stretch_height;
}

/** Lay out box. */

/* virtual */ LAYST
FlexItemBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	VerticalBoxReflowState* state = InitialiseReflowState();

	if (!state)
		return LAYOUT_OUT_OF_MEMORY;

	HTML_Element* html_element = cascade->html_element;
	FlexContent* flexbox = cascade->flexbox;
	BOOL vertical = flexbox->IsVertical();

	OP_ASSERT(!first_child); // Meaningless situation.

	int reflow_content = html_element->MarkClean();
	const HTMLayoutProperties& props = *cascade->GetProps();

	flexitem_packed.is_positioned = props.position != CSS_VALUE_static;

	/* To simplify things (number of classes and amount of code duplication),
	   we create the same box type for flexbox items, regardless positioning
	   scheme, opacity, transforms and z-index. Instead, calculate flags that
	   determine whether or not we should return NULL from methods like
	   GetLocalZElement() and GetLocalStackingContext(), and whether
	   IsPositionedBox() should return FALSE or TRUE. Note that flex items let
	   'z-index' apply even if 'position' is 'static'. */

	flexitem_packed.has_stacking_context =
#ifdef CSS_TRANSFORMS
		props.transforms_cp != NULL ||
#endif // CSS_TRANSFORMS;
		props.opacity != 255 || props.z_index != CSS_ZINDEX_auto;

	flexitem_packed.has_z_element = flexitem_packed.has_stacking_context || flexitem_packed.is_positioned;

	if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
		return LAYOUT_OUT_OF_MEMORY;

	state->cascade = cascade;
	state->old_x = x;
	state->old_y = y;
	state->old_width = content->GetWidth();
	state->old_height = content->GetHeight();
	state->old_bounding_box = bounding_box;

#ifdef CSS_TRANSFORMS
	if (state->transform)
		state->transform->old_transform = state->transform->transform_context->GetCurrentTransform();
#endif // CSS_TRANSFORMS

#ifdef LAYOUT_YIELD_REFLOW
	if (info.force_layout_changed)
	{
		space_manager.EnableFullBfcReflow();
		reflow_content = ELM_DIRTY;
	}
#endif // LAYOUT_YIELD_REFLOW

	margin_left = props.margin_left;
	margin_right = props.margin_right;
	margin_top = props.margin_top;
	margin_bottom = props.margin_bottom;
	modes = props.flexbox_modes;
	flexitem_packed.border_box_sizing = props.box_sizing == CSS_VALUE_border_box;
	flexitem_packed.margin_left_auto = props.GetMarginLeftAuto();
	flexitem_packed.margin_right_auto = props.GetMarginRightAuto();
	flexitem_packed.margin_top_auto = props.GetMarginTopAuto();
	flexitem_packed.margin_bottom_auto = props.GetMarginBottomAuto();

	if (flexitem_packed.is_positioned)
		// Store relative offsets.

		props.GetLeftAndTopOffset(left, top);

	preferred_main_size = props.flex_basis;

	if (preferred_main_size <= CONTENT_SIZE_SPECIAL)
		if (vertical)
			preferred_main_size = props.content_height;
		else
			preferred_main_size = props.content_width;

	flex_grow = props.flex_grow;
	flex_shrink = props.flex_shrink;
	order = props.order;

	LAYST st = flexbox->GetNewItem(info, cascade, this);

	if (st != LAYOUT_CONTINUE)
		return st;

	if (
#ifdef PAGED_MEDIA_SUPPORT
		info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
		cascade->multipane_container)
	{
		BREAK_POLICY page_break_before;
		BREAK_POLICY column_break_before;
		BREAK_POLICY page_break_after;
		BREAK_POLICY column_break_after;

		CssValuesToBreakPolicies(info, cascade, page_break_before, page_break_after, column_break_before, column_break_after);
		SetBreakPolicies(page_break_before, column_break_before, page_break_after, column_break_after);

		if (cascade->multipane_container)
			/* There's an ancestor multi-pane container, but that doesn't
			   have to mean that this item is to be columnized. */

			flexitem_packed.in_multipane = flexbox->GetPlaceholder()->IsColumnizable(TRUE);
	}

#ifdef PAGED_MEDIA_SUPPORT
	/* Under no circumstances break inside flex items, even if that is allowed
	   according to the spec. If we add a page break inside a flex item, that
	   will affect its hypothetical height, because of how our paged media
	   implementation works. That may lead to circular dependencies. */

	state->old_paged_media = info.paged_media;
	info.paged_media = PAGEBREAK_OFF;
#endif // PAGED_MEDIA_SUPPORT

	switch (content->ComputeSize(cascade, info))
	{
	case OpBoolean::IS_TRUE:
		reflow_content |= ELM_DIRTY;

	case OpBoolean::IS_FALSE:
		break;

	default:
		OP_ASSERT(!"Unexpected result");

	case OpStatus::ERR_NO_MEMORY:
		return LAYOUT_OUT_OF_MEMORY;
	}

	flexitem_packed.visibility_collapse = props.visibility == CSS_VALUE_collapse;
	flexitem_packed.vertical = vertical;

	if (vertical)
	{
		min_main_size = props.min_height;
		max_main_size = props.max_height;
		min_cross_size = props.min_width;
		max_cross_size = props.max_width;

		computed_cross_size = props.content_width;

		main_border_padding =
			LayoutCoord(props.border.top.width + props.border.bottom.width) +
			props.padding_top + props.padding_bottom;

		cross_border_padding =
			LayoutCoord(props.border.left.width + props.border.right.width) +
			props.padding_left + props.padding_right;
	}
	else
	{
		min_main_size = props.min_width;
		max_main_size = props.max_width;
		min_cross_size = props.min_height;
		max_cross_size = props.max_height;

		computed_cross_size = props.content_height;

		main_border_padding =
			LayoutCoord(props.border.left.width + props.border.right.width) +
			props.padding_left + props.padding_right;

		cross_border_padding =
			LayoutCoord(props.border.top.width + props.border.bottom.width) +
			props.padding_top + props.padding_bottom;
	}

	flexitem_packed.stretch =
		props.flexbox_modes.GetAlignSelf() == FlexboxModes::ASELF_STRETCH &&
		computed_cross_size == CONTENT_SIZE_AUTO;

	CalculateVisualMarginPos(flexbox, x, y);
	x += margin_left + left;
	y += margin_top + top;

	info.Translate(x, y);

#ifdef CSS_TRANSFORMS
	if (state->transform)
		if (!state->transform->transform_context->PushTransforms(info, state->transform->translation_state))
			return LAYOUT_OUT_OF_MEMORY;
#endif

	if (flexitem_packed.is_positioned)
	{
		info.GetRootTranslation(state->previous_root_x, state->previous_root_y);
		info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());
	}

	/* First remove from stacking context. This shouldn't really be necessary,
	   unless flexitem_packed.has_z_element suddenly goes from 1 to 0 (see
	   CORE-49171). We should recreate the layout box object in such cases, but
	   if we don't do that, this class is capable of coping - as long as we
	   make sure we aren't an entry in a stacking context when we shouldn't
	   be. Better cope than crash. */

	z_element.Out();

	if (flexitem_packed.has_z_element)
		if (cascade->stacking_context)
		{
			// Set up local stacking context, if any.

			if (flexitem_packed.has_stacking_context)
				z_element.SetZIndex(props.z_index);

			// Add this element to parent stacking context.

			z_element.SetOrder(props.order);
			cascade->stacking_context->AddZElement(&z_element);
		}

	if (!reflow_content)
		if (
#ifdef PAGED_MEDIA_SUPPORT
			info.paged_media == PAGEBREAK_ON ||
#endif
			props.display_type == CSS_VALUE_list_item && flexbox->IsReversedList())
			reflow_content |= ELM_DIRTY;
		else
		{
			/* If there are width changes, they have already been detected by
			   ComputeSize() as usual, but for height changes, we need to check
			   on our own. The height may be stretched in horizontal flexboxes,
			   while it may be flexed in vertical flexboxes. */

			LayoutCoord flex_height = GetFlexHeight(cascade);

			if (flex_height != CONTENT_HEIGHT_AUTO && flex_height != state->old_height)
				reflow_content |= ELM_DIRTY;
		}

	if (reflow_content)
	{
		if (flexitem_packed.has_stacking_context)
			stacking_context.Restart();

		if (!space_manager.Restart())
			return LAYOUT_OUT_OF_MEMORY;

		bounding_box.Reset(props.DecorationExtent(info.doc), LayoutCoord(0));

		return content->Layout(cascade, info);
	}
	else
	{
		if (!cascade->SkipBranch(info, FALSE, TRUE))
			return LAYOUT_OUT_OF_MEMORY;

		if (props.display_type == CSS_VALUE_list_item && HasListMarkerBox())
			/* Layout of this box is skipped, so list item marker layout will
			   also be skipped. Process numbering, so that later list items
			   still get the correct value. */

			flexbox->GetNewListItemValue(cascade->html_element);
	}

	return LAYOUT_CONTINUE;
}

/** Finish reflowing box. */

/* virtual */ LAYST
FlexItemBox::FinishLayout(LayoutInfo& info)
{
	VerticalBoxReflowState* state = GetReflowState();
	LayoutProperties* cascade = state->cascade;
	LAYST st = content->FinishLayout(info);

	if (st != LAYOUT_CONTINUE)
		return st;

	space_manager.FinishLayout();

	if (flexitem_packed.has_stacking_context)
		stacking_context.FinishLayout(cascade);

	cascade->flexbox->FinishNewItem(info, cascade, this);
	UpdateScreen(info);

	if (flexitem_packed.is_positioned)
		info.SetRootTranslation(state->previous_root_x, state->previous_root_y);

	if (cascade->GetProps()->display_type == CSS_VALUE_list_item && !HasListMarkerBox())
		/* Need to process the list number, since if the list marker was not
		   displayed, the number was not processed during marker box layout,
		   but it should still affect numbering of later list items. */

		cascade->flexbox->GetNewListItemValue(cascade->html_element);

	PropagateBottomMargins(info);

	DeleteReflowState();

	return LAYOUT_CONTINUE;
}

/** Update screen. */

/* virtual */ void
FlexItemBox::UpdateScreen(LayoutInfo& info)
{
	VerticalBoxReflowState* state = GetReflowState();

	content->UpdateScreen(info);

	CheckAbsPosDescendants(info);

#ifdef CSS_TRANSFORMS
	TransformContext* transform_context = state->transform ? state->transform->transform_context : NULL;

	if (transform_context)
		transform_context->PopTransforms(info, state->transform->translation_state);
#endif // CSS_TRANSFORMS

#ifdef PAGED_MEDIA_SUPPORT
	info.paged_media = state->old_paged_media;
#endif // PAGED_MEDIA_SUPPORT

	info.Translate(-x, -y);

	if (state->old_x != x ||
		state->old_y != y ||
		state->old_width != content->GetWidth() ||
		state->old_height != content->GetHeight() ||
		state->old_bounding_box != bounding_box
#ifdef CSS_TRANSFORMS
		|| transform_context && state->transform->old_transform != transform_context->GetCurrentTransform()
#endif // CSS_TRANSFORMS
		)
	{
#ifdef CSS_TRANSFORMS
		if (transform_context)
		{
			VisualDevice* visual_device = info.visual_device;

			visual_device->Translate(state->old_x, state->old_y);
			visual_device->PushTransform(state->transform->old_transform);
			state->old_bounding_box.Invalidate(visual_device, LayoutCoord(0), LayoutCoord(0), state->old_width, state->old_height);
			visual_device->PopTransform();

			visual_device->Translate(x - state->old_x, y - state->old_y);

			visual_device->PushTransform(transform_context->GetCurrentTransform());
			bounding_box.Invalidate(visual_device, LayoutCoord(0), LayoutCoord(0), content->GetWidth(), content->GetHeight());
			visual_device->PopTransform();
			visual_device->Translate(-x, -y);
		}
		else
#endif // CSS_TRANSFORMS
		{
			AbsoluteBoundingBox before;
			AbsoluteBoundingBox bbox;

			bbox.Set(bounding_box, content->GetWidth(), content->GetHeight());
			bbox.Translate(x, y);
			before.Set(state->old_bounding_box, state->old_width, state->old_height);
			before.Translate(state->old_x, state->old_y);

			bbox.UnionWith(before);

			info.visual_device->UpdateRelative(bbox.GetX(), bbox.GetY(), bbox.GetWidth(), bbox.GetHeight());
		}
	}
}

/** Propagate bottom margins and bounding box to parents, and walk the dog. */

/* virtual */ void
FlexItemBox::PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin, BOOL has_inflow_content)
{
	if (flexitem_packed.visibility_collapse)
		return;

	AbsoluteBoundingBox abs_bounding_box;
	LayoutProperties* cascade = GetCascade();
	FlexContent* flexbox = cascade->flexbox;

	GetBoundingBox(abs_bounding_box, IsOverflowVisible());

#ifdef CSS_TRANSFORMS
	if (TransformContext* transform_context = GetTransformContext())
		transform_context->ApplyTransform(abs_bounding_box);
#endif // CSS_TRANSFORMS

	abs_bounding_box.Translate(x, y);

	flexbox->UpdateBoundingBox(abs_bounding_box, FALSE);
}

/** Expand relevant parent containers and their bounding-boxes to contain floating and
	absolutely positioned boxes. A call to this method is only to be initiated by floating or
	absolutely positioned boxes. */

/* virtual */ void
FlexItemBox::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	// A flexbox item has its own space manager, so you never propagate a float past it:

	OP_ASSERT(opts.type != PROPAGATE_FLOAT);

	if (flexitem_packed.visibility_collapse)
		return;

	ReflowState* state = GetReflowState();

	if (opts.type == PROPAGATE_ABSPOS_BOTTOM_ALIGNED)
	{
		StackingContext* context = GetLocalStackingContext();

		if (context)
			context->SetNeedsBottomAlignedUpdate();

		if (flexitem_packed.is_positioned)
		{
			if (!context)
				state->cascade->stacking_context->SetNeedsBottomAlignedUpdate();

			return;
		}
	}
	else
		if (opts.type != PROPAGATE_ABSPOS_SKIPBRANCH || IsOverflowVisible())
			UpdateBoundingBox(child_bounding_box, !flexitem_packed.is_positioned);

	if (flexitem_packed.is_positioned || opts.type == PROPAGATE_ABSPOS_SKIPBRANCH)
	{
		// We either reached a containing element or we already passed it.

		if (state)
			/* If we're not in SkipBranch or we reached an element that is
			   still being reflowed, we can safely stop bounding box
			   propagation here. The bounding box will be propagated when
			   layout of this box is finished */

			return;

		/* We've already found a containing element but since it had no reflow
		   state we must be skipping a branch. Switch propagation type (this
		   will enable us to clip such bbox) and continue propagation until we
		   reach first element that is being reflowed. */

		opts.type = PROPAGATE_ABSPOS_SKIPBRANCH;
	}

	HTML_Element* containing_element = GetContainingElement();
	OP_ASSERT(containing_element->GetLayoutBox()->GetFlexContent());

	if (FlexContent* flexbox = containing_element->GetLayoutBox()->GetFlexContent())
	{
		AbsoluteBoundingBox abs_bounding_box;

		if (opts.type == PROPAGATE_ABSPOS_SKIPBRANCH)
			GetBoundingBox(abs_bounding_box, IsOverflowVisible());
		else
			abs_bounding_box = child_bounding_box;

		abs_bounding_box.Translate(x, y);
		flexbox->PropagateBottom(info, abs_bounding_box, opts);
	}
}

/** Invalidate the screen area that the box uses. */

/* virtual */ void
FlexItemBox::Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const
{
	info.Translate(x, y);

#ifdef CSS_TRANSFORMS
	TranslationState translation_state;
	const TransformContext* transform_context = GetTransformContext();

	if (transform_context)
		if (!transform_context->PushTransforms(info, translation_state))
		{
			info.Translate(-x, -y);
			return;
		}

#endif // CSS_TRANSFORMS

	bounding_box.Invalidate(info.visual_device, LayoutCoord(0), LayoutCoord(0), content->GetWidth(), content->GetHeight());

#ifdef CSS_TRANSFORMS
	if (transform_context)
		transform_context->PopTransforms(info, translation_state);
#endif // CSS_TRANSFORMS

	info.Translate(-x, -y);
}

/** Do we need to calculate min/max widths of this box's content? */

/* virtual */ BOOL
FlexItemBox::NeedMinMaxWidthCalculation(LayoutProperties* cascade) const
{
	return TRUE;
}

/** Propagate widths to flexbox. */

/* virtual */ void
FlexItemBox::PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	LayoutProperties* cascade = GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord border_box_width = props.content_width;
	LayoutCoord extra_width(0);

	if (!flexitem_packed.vertical)
		if (props.flex_basis != CONTENT_SIZE_AUTO)
			border_box_width = props.flex_basis;

	if (props.box_sizing == CSS_VALUE_content_box)
		extra_width = props.GetNonPercentHorizontalBorderPadding();

	if (border_box_width >= 0)
		border_box_width += extra_width;

	OP_ASSERT(max_width >= normal_min_width);
	OP_ASSERT(normal_min_width >= min_width);

	min_width = normal_min_width;

	if (border_box_width >= 0)
		max_width = border_box_width;

	if (!flexitem_packed.vertical && flex_shrink == 0.0)
		// Not shrinkable. Max width is then used to size the item.

		min_width = max_width;

	if (props.max_width >= 0 && !props.GetMaxWidthIsPercent())
	{
		if (max_width > props.max_width + extra_width)
			max_width = props.max_width + extra_width;

		if (min_width > props.max_width + extra_width)
			min_width = props.max_width + extra_width;
	}

	if (props.min_width != CONTENT_WIDTH_AUTO && !props.GetMinWidthIsPercent())
	{
		if (max_width < props.min_width + extra_width)
			max_width = props.min_width + extra_width;

		if (min_width < props.min_width + extra_width)
			min_width = props.min_width + extra_width;
	}

	if (max_width < min_width)
		max_width = min_width;

	LayoutCoord hor_margin(0);

	if (!props.GetMarginLeftIsPercent())
		hor_margin += props.margin_left;

	if (!props.GetMarginRightIsPercent())
		hor_margin += props.margin_right;

	cascade->flexbox->PropagateMinMaxWidths(info, min_width + hor_margin, max_width + hor_margin);
}

/** Propagate hypothetical border box height. */

void
FlexItemBox::PropagateHeights(LayoutCoord min_border_height, LayoutCoord hyp_border_height)
{
	LayoutCoord new_hypothetical_margin_height = hyp_border_height + margin_top + margin_bottom;
	LayoutCoord new_minimum_margin_height = min_border_height + margin_top + margin_bottom;

	if (new_hypothetical_margin_height != hypothetical_margin_height ||
		new_minimum_margin_height != minimum_margin_height)
	{
		// This change may trigger another reflow.

		GetCascade()->flexbox->SetItemHypotheticalHeightChanged();

		hypothetical_margin_height = new_hypothetical_margin_height;
		minimum_margin_height = new_minimum_margin_height;
	}
}

/** Propagate the 'order' property from an absolutely positioned child. */

void
FlexItemBox::PropagateOrder(int child_order)
{
	if (order != child_order && IsAbsolutePositionedSpecialItem())
		order = child_order;
}

/** Traverse box with children. */

void
FlexItemBox::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops)
{
	HTML_Element* html_element = GetHtmlElement();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();

	OP_ASSERT(flexitem_packed.has_z_element || !flexitem_packed.has_stacking_context);
	OP_ASSERT(flexitem_packed.has_z_element || !flexitem_packed.is_positioned);

	if (!flexitem_packed.visibility_collapse &&
		(!flexitem_packed.has_z_element || traversal_object->IsTarget(html_element) && old_traverse_type != TRAVERSE_BACKGROUND))
	{
		TraverseInfo traverse_info;
		RootTranslationState root_translation_state;
		LayoutProperties* layout_props = NULL;
		HTML_Element* containing_element = parent_lprops->html_element;
		BOOL traverse_descendant_target = FALSE;

		traversal_object->Translate(x, y);

		if (flexitem_packed.is_positioned)
			traversal_object->SyncRootScrollAndTranslation(root_translation_state);

#ifdef CSS_TRANSFORMS
		TransformContext* transform_context = GetTransformContext();
		TranslationState transforms_translation_state;

		if (transform_context)
		{
			OP_BOOLEAN status = transform_context->PushTransforms(traversal_object, transforms_translation_state);
			switch (status)
			{
			case OpBoolean::ERR_NO_MEMORY:
				traversal_object->SetOutOfMemory();
			case OpBoolean::IS_FALSE:
				if (traversal_object->GetTarget())
					traversal_object->SwitchTarget(containing_element);
				traversal_object->RestoreRootScrollAndTranslation(root_translation_state);
				traversal_object->Translate(-x, -y);
				return;
			}
		}
#endif // CSS_TRANSFORMS

		HTML_Element* old_target = traversal_object->GetTarget();

		if (flexitem_packed.has_z_element)
		{
			if (old_target == html_element)
				traversal_object->SetTarget(NULL);

			if (!traversal_object->GetTarget() && old_traverse_type != TRAVERSE_ONE_PASS)
				traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);
		}

		if (traversal_object->EnterVerticalBox(parent_lprops, layout_props, this, traverse_info))
		{
			if (flexitem_packed.has_stacking_context)
			{
				if (stacking_context.HasNegativeZChildren() && traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
					traversal_object->FlushBackgrounds(layout_props, this);

				// Traverse stacking context children with negative z-index.

				stacking_context.Traverse(traversal_object, layout_props, this, FALSE, FALSE);
			}

			// Traverse children in normal flow

			if (flexitem_packed.has_z_element)
				if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
				{
					content->Traverse(traversal_object, layout_props);

					traversal_object->FlushBackgrounds(layout_props, this);
					traversal_object->SetTraverseType(TRAVERSE_CONTENT);

					traversal_object->TraverseFloats(this, layout_props);
				}

			content->Traverse(traversal_object, layout_props);

			if (flexitem_packed.has_stacking_context)
				// Traverse stacking context children with zero or positive z-index.

				stacking_context.Traverse(traversal_object, layout_props, this, TRUE, FALSE);

			traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
		}
		else
			traversal_object->SetTraverseType(old_traverse_type);

#ifdef CSS_TRANSFORMS
		if (transform_context)
			transform_context->PopTransforms(traversal_object, transforms_translation_state);
#endif // CSS_TRANSFORMS

		if (flexitem_packed.has_z_element)
			if (old_target == html_element)
			{
				traversal_object->SetTarget(old_target);

				if (traversal_object->SwitchTarget(containing_element) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
				{
					OP_ASSERT(!GetLocalStackingContext());
					traverse_descendant_target = TRUE;
				}
			}

		if (flexitem_packed.has_z_element)
			traversal_object->RestoreRootScrollAndTranslation(root_translation_state);

		traversal_object->Translate(-x, -y);

		if (traverse_descendant_target)
			/* This box was the target (whether it was entered or not is not interesting,
			   though). The next target is a descendant. We can proceed (no need to
			   return to the stacking context loop), but first we have to retraverse
			   this box with the new target set - most importantly because we have to
			   make sure that we enter it and that we do so in the right way. */

			Traverse(traversal_object, parent_lprops);
	}
}

/** Get flex base size (main axis margin box size). */

LayoutCoord
FlexItemBox::GetFlexBaseSize(LayoutCoord containing_block_size)
{
	LayoutCoord item_size = preferred_main_size;

	if (item_size <= CONTENT_SIZE_SPECIAL ||
		item_size < 0 && containing_block_size == CONTENT_SIZE_AUTO)
	{
		// Treat as auto.

		if (flexitem_packed.vertical)
			item_size = hypothetical_margin_height;
		else
		{
			LayoutCoord dummy;
			content->GetMinMaxWidth(dummy, dummy, item_size);
			item_size += margin_left + margin_right;
		}
	}
	else
	{
		if (item_size < 0)
		{
			// Resolve percentage.

			OP_ASSERT(item_size > CONTENT_SIZE_SPECIAL);
			OP_ASSERT(containing_block_size >= 0);
			item_size = PercentageToLayoutCoord(LayoutFixed(-item_size), containing_block_size);
		}

		// Convert to margin box.

		if (!flexitem_packed.border_box_sizing)
			item_size += main_border_padding;

		if (flexitem_packed.vertical)
			item_size += margin_top + margin_bottom;
		else
			item_size += margin_left + margin_right;
	}

	return item_size;
}

/** Calculate real coordinates (values for upcoming reflow). */

void
FlexItemBox::CalculateVisualMarginPos(FlexContent* flexbox, LayoutCoord& x, LayoutCoord& y) const
{
	if (!flexbox->GetPlaceholder()->GetReflowState())
	{
		/* Not reflowing the flexbox (anymore). Then the stored X and Y
		   coordinates are valid. */

		x = this->x - margin_left - left;
		y = this->y - margin_top - top;

		return;
	}

	const HTMLayoutProperties& flex_props = *flexbox->GetPlaceholder()->GetCascade()->GetProps();
	LayoutCoord margin_x; // item's margin left edge relative to content box of flexbox
	LayoutCoord margin_y; // item's margin top edge relative to content box of flexbox
	BOOL lines_reversed = flexbox->IsLineOrderReversed();
	BOOL items_reversed = flexbox->IsItemOrderReversed();

	if (flexbox->IsVertical())
	{
		if (lines_reversed)
			margin_x = flexbox->GetContentWidth() - new_cross_margin_edge - new_cross_margin_size;
		else
			margin_x = new_cross_margin_edge;

		if (items_reversed)
			margin_y = flexbox->GetContentHeight() - new_main_margin_edge - new_main_margin_size;
		else
			margin_y = new_main_margin_edge;
	}
	else
	{
		if (items_reversed)
			margin_x = flexbox->GetContentWidth() - new_main_margin_edge - new_main_margin_size;
		else
			margin_x = new_main_margin_edge;

		if (lines_reversed)
			margin_y = flexbox->GetContentHeight() - new_cross_margin_edge - new_cross_margin_size;
		else
			margin_y = new_cross_margin_edge;
	}

	x = LayoutCoord(flex_props.border.left.width) + flex_props.padding_left + margin_x;
	y = LayoutCoord(flex_props.border.top.width) + flex_props.padding_top + margin_y;
}

/** Push new static Y position of margin box (for upcoming reflow) downwards. */

void
FlexItemBox::MoveNewStaticMarginY(FlexContent* flexbox, LayoutCoord amount)
{
	if (flexbox->IsVertical())
	{
		if (flexbox->IsItemOrderReversed())
			new_main_margin_edge -= amount;
		else
			new_main_margin_edge += amount;
	}
	else
	{
		if (flexbox->IsLineOrderReversed())
			new_cross_margin_edge -= amount;
		else
			new_cross_margin_edge += amount;
	}
}

/** Constrain the specified main size to min/max-width/height and return that value. */

LayoutCoord
FlexItemBox::GetConstrainedMainSize(LayoutCoord margin_box_size) const
{
	LayoutCoord main_margin = flexitem_packed.vertical ? margin_top + margin_bottom : margin_left + margin_right;
	LayoutCoord new_size = margin_box_size - main_margin;

	if (!flexitem_packed.border_box_sizing)
		new_size -= main_border_padding;

	if (max_main_size >= 0)
		if (new_size > max_main_size)
			new_size = max_main_size;

	LayoutCoord min_size;

	if (min_main_size == CONTENT_SIZE_AUTO)
	{
		if (flexitem_packed.vertical)
			min_size = minimum_margin_height - main_margin;
		else
		{
			LayoutCoord dummy;

			content->GetMinMaxWidth(dummy, min_size, dummy);
		}

		if (!flexitem_packed.border_box_sizing)
		{
			min_size -= main_border_padding;

			if (min_size < 0)
				min_size = LayoutCoord(0);
		}
	}
	else
		min_size = min_main_size;

	if (new_size < min_size)
		new_size = min_size;

	if (!flexitem_packed.border_box_sizing)
		new_size += main_border_padding;

	new_size += main_margin;

	return new_size;
}

/** Constrain the specified cross size to min/max-height/width and return that value. */

LayoutCoord
FlexItemBox::GetConstrainedCrossSize(LayoutCoord margin_box_size) const
{
	LayoutCoord cross_margin = flexitem_packed.vertical ? margin_left + margin_right : margin_top + margin_bottom;
	LayoutCoord new_size = margin_box_size - cross_margin;

	if (!flexitem_packed.border_box_sizing)
		new_size -= cross_border_padding;

	if (max_cross_size >= 0)
		if (new_size > max_cross_size)
			new_size = max_cross_size;

	if (new_size < min_cross_size)
		new_size = min_cross_size;

	if (!flexitem_packed.border_box_sizing)
		new_size += cross_border_padding;

	new_size += cross_margin;

	return new_size;
}
