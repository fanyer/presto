/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/layout/content/scrollable.h"

#include "modules/doc/frm_doc.h"
#include "modules/layout/box/box.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/content/content.h"
#include "modules/logdoc/htm_elm.h"

#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif

OpRect
CachedPosition::GetEdgesInDocumentCoords(const OpRect& edges) const
{
	OpRect r(edges);
	ctm.Apply(r);
	return r;
}

ScrollableArea::~ScrollableArea()
{
	if (vertical_scrollbar)
	{
		vertical_scrollbar->Delete();
		REPORT_MEMMAN_DEC(sizeof(OpScrollbar));
	}

	if (horizontal_scrollbar)
	{
		horizontal_scrollbar->Delete();
		REPORT_MEMMAN_DEC(sizeof(OpScrollbar));
	}
}

// BEGIN - implementation of OpWidgetListener

/* virtual */ void
ScrollableArea::OnScroll(OpWidget* widget, INT32 old_val, INT32 new_val, BOOL caused_by_input)
{
	OpScrollbar* scrollbar = (OpScrollbar*) widget;
	LayoutCoord dx(0);
	LayoutCoord dy(0);
	LayoutCoord diff(new_val - old_val);

	if (scrollbar->IsHorizontal())
	{
		dx = diff;

		if (caused_by_input)
			SetUnconstrainedScrollX(LayoutCoord(new_val));
	}
	else
	{
		dy = diff;

		if (caused_by_input)
			SetUnconstrainedScrollY(LayoutCoord(new_val));
	}

	ScrollContent(doc, dx, dy, caused_by_input);
}

// END - implementation of OpWidgetListener

// BEGIN - implementation of CoreView

/* virtual */ BOOL
ScrollableArea::OnInputAction(OpInputAction* action)
{
	if (sc_packed.horizontal_on && horizontal_scrollbar->CanScroll() ||
		sc_packed.vertical_on && vertical_scrollbar->CanScroll())
		return TriggerScrollInputAction(action);

	return FALSE;
}

/* virtual */ BOOL
ScrollableArea::GetBoundingRect(OpRect& rect)
{
	if (sc_packed.vertical_on || sc_packed.horizontal_on)
	{
		rect = GetEdgesInDocumentCoords(GetOwningBox()->GetBorderEdges());

		rect.OffsetBy(-doc->GetVisualDevice()->GetRenderingViewX(),
					  -doc->GetVisualDevice()->GetRenderingViewY());

		rect.width -= GetVerticalScrollbarWidth();
		rect.height -= GetHorizontalScrollbarWidth();

		return TRUE;
	}

	return FALSE;
}

// END - implementation of CoreView

/** Attach to CoreView hierarchy. */

void
ScrollableArea::InitCoreView(HTML_Element* html_element, VisualDevice* vis_dev)
{
	CoreView* parent_view = Content::FindParentCoreView(html_element);

	if (!CoreView::m_parent)
	{
		if (!parent_view && !vis_dev->GetView())
			/* No parent input context and no view. Not much we can do here but
			   return. We can't init the core-view without some kind of parent
			   core view.

			   Note: This is probably a symptom of something else being
			   wrong. Making this into a assert and fixing the root cause would
			   perhaps be worthwhile in the long run. There shouldn't be any
			   danger currently because this scrollable container will be
			   InitCoreView:ed again after the visual device has gotten a
			   view. */

			return;

		/* We are not using using the paint listeners or input listeners of the
		   CoreView. We only inherit CoreView so the hierarchy of CoreViews
		   will know about our visible area (so clipping can be done
		   correctly). We also get the InputContext from it so we can get
		   focused. */

		OpStatus::Ignore(CoreView::Construct(parent_view ? parent_view : vis_dev->GetView()));
		CoreView::SetWantPaintEvents(FALSE);
		CoreView::SetWantMouseEvents(FALSE);
		CoreView::SetVisibility(TRUE);
		CoreView::SetIsLayoutBox(TRUE);
	}

	CoreView::SetOwningHtmlElement(html_element);

	OpInputContext* parent_ctx = parent_view ? parent_view : static_cast<OpInputContext*>(vis_dev);

	if (GetParentInputContext() != parent_ctx)
		/* Set only if the parent context has changed or is set for the first
		   time to prevent losing focus. */

		SetParentInputContext(parent_ctx);
}

/** Disable widgets and CoreView. */

void
ScrollableArea::DisableCoreView()
{
	/* We must destruct CoreView here so we get it out from the hierarchy, and
	   also reset VisualDevice pointers. Our layout boxes may outlive the
	   CoreView hierarchy and VisualDevice. */

	if (CoreView::m_parent)
		CoreView::Destruct();

	if (vertical_scrollbar)
		vertical_scrollbar->SetVisualDevice(NULL);

	if (horizontal_scrollbar)
		horizontal_scrollbar->SetVisualDevice(NULL);
}

/** Set up scrollable area and scrollbars, and store current state. */

OP_STATUS
ScrollableArea::SetUpScrollable(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	doc = info.doc;

	if (!doc->IsPrintDocument())
		InitCoreView(cascade->html_element, info.visual_device);

	sc_packed.left_hand_scrollbar = !!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LeftHandedUI);

#ifdef SUPPORT_TEXT_DIRECTION
	if (props.direction == CSS_VALUE_rtl && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::RTLFlipsUI))
		sc_packed.left_hand_scrollbar = !sc_packed.left_hand_scrollbar;
#endif // SUPPORT_TEXT_DIRECTION

	if (props.overflow_y == CSS_VALUE_scroll)
		sc_packed.vertical_on = 1;
	else
		if (props.overflow_y != CSS_VALUE_auto)
			sc_packed.vertical_on = 0;

	if (props.overflow_x == CSS_VALUE_scroll)
		sc_packed.horizontal_on = 1;
	else
		if (props.overflow_x != CSS_VALUE_auto)
			sc_packed.horizontal_on = 0;

	sc_packed.vertical_was_on = sc_packed.vertical_on;
	sc_packed.horizontal_was_on = sc_packed.horizontal_on;
	sc_packed.prevent_auto_scrollbars = 0;

	if (!horizontal_scrollbar && props.overflow_x != CSS_VALUE_hidden)
	{
		RETURN_IF_ERROR(OpScrollbar::Construct(&horizontal_scrollbar, TRUE));
		REPORT_MEMMAN_INC(sizeof(OpScrollbar));
	}

	if (!vertical_scrollbar && props.overflow_y != CSS_VALUE_hidden)
	{
		RETURN_IF_ERROR(OpScrollbar::Construct(&vertical_scrollbar, FALSE));
		REPORT_MEMMAN_INC(sizeof(OpScrollbar));
	}

	scroll_padding_box_height = LayoutCoord(0);
	scroll_padding_box_width = LayoutCoord(0);

	switch (GetSavedScrollPos(cascade->html_element, scroll_x, scroll_y))
	{
	case OpBoolean::IS_TRUE:
		unconstrained_scroll_x = scroll_x;
		unconstrained_scroll_y = scroll_y;
		break;

	case OpBoolean::IS_FALSE:
		scroll_x = GetViewX(FALSE);
		scroll_y = GetViewY(FALSE);
		break;

	case OpStatus::ERR_NO_MEMORY:
		return OpStatus::ERR_NO_MEMORY;

	default:
		break;
	}

#ifdef CSS_SCROLLBARS_SUPPORT
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableScrollbarColors, doc->GetHostName()))
	{
		ScrollbarColors colors;

		if (info.hld_profile)
			colors = *info.hld_profile->GetScrollbarColors();

		cascade->html_element->GetScrollbarColor(&colors);

		if (horizontal_scrollbar)
			horizontal_scrollbar->SetScrollbarColors(&colors);

		if (vertical_scrollbar)
			vertical_scrollbar->SetScrollbarColors(&colors);
	}
#endif // CSS_SCROLLBARS_SUPPORT

	if (horizontal_scrollbar)
		horizontal_scrollbar->SetListener(this);

	if (vertical_scrollbar)
		vertical_scrollbar->SetListener(this);

	sc_packed.border_top = props.border.top.width;
	sc_packed.border_left = props.border.left.width;

	return OpStatus::OK;
}

/** Set scrollbars' position, dimension and ranges. */

void
ScrollableArea::FinishScrollable(const HTMLayoutProperties& props, LayoutCoord border_width, LayoutCoord border_height)
{
	LayoutCoord hor_scroll_size = GetHorizontalScrollbarWidth();
	LayoutCoord ver_scroll_size = GetVerticalScrollbarWidth();
	LayoutCoord visible_width = border_width - LayoutCoord(props.border.left.width + props.border.right.width) - ver_scroll_size;
	LayoutCoord visible_height = border_height - LayoutCoord(props.border.top.width + props.border.bottom.width) - hor_scroll_size;

	if (visible_width < 0)
		visible_width = LayoutCoord(0);

	if (visible_height < 0)
		visible_height = LayoutCoord(0);

	total_horizontal_scroll_amount = scroll_padding_box_width - visible_width;
	total_vertical_scroll_amount = scroll_padding_box_height - visible_height;

	if (horizontal_scrollbar)
	{
		if (sc_packed.horizontal_on)
		{
			LayoutCoord left(0);

			if (LeftHandScrollbar())
				left += ver_scroll_size;

			/* Set the scrollbar rectangle to be relative the content edge to
			   correspond to the coordinates we receive in Box::HandleEvent. */

			OpRect rect(left - props.padding_left,
						border_height - hor_scroll_size - props.border.bottom.width - props.border.top.width - props.padding_top,
						border_width - props.border.left.width - props.border.right.width - ver_scroll_size,
						hor_scroll_size);

			horizontal_scrollbar->SetRect(rect);
		}

		// Set scrollbar range and step size.

		horizontal_scrollbar->SetLimit(0, total_horizontal_scroll_amount, visible_width);
		horizontal_scrollbar->SetSteps(DISPLAY_SCROLL_STEPSIZE, visible_width);
	}

	if (vertical_scrollbar)
	{
		if (sc_packed.vertical_on)
		{
			LayoutCoord left;

			if (LeftHandScrollbar())
				left = LayoutCoord(props.border.left.width);
			else
				left = border_width - LayoutCoord(props.border.right.width) - GetVerticalScrollbarWidth();

			/* Set the scrollbar rectangle to be relative the content edge to
			   correspond to the coordinates we receive in Box::HandleEvent. */

			OpRect rect(left - props.border.left.width - props.padding_left,
						-props.padding_top,
						ver_scroll_size,
						border_height - props.border.top.width - props.border.bottom.width - hor_scroll_size);

			vertical_scrollbar->SetRect(rect);
		}

		// Set scrollbar range and step size.

		vertical_scrollbar->SetLimit(0, total_vertical_scroll_amount, visible_height);
		vertical_scrollbar->SetSteps(DISPLAY_SCROLL_STEPSIZE, visible_height);
	}

	// Restore scroll position.

	SetViewX(scroll_x, FALSE, TRUE);
	SetViewY(scroll_y, FALSE, TRUE);
}

/** Save scroll position in the HTML element. */

void
ScrollableArea::SaveScrollPos(HTML_Element* html_element, LayoutCoord sx, LayoutCoord sy)
{
	SavedScrollPos* scrollpos = static_cast<SavedScrollPos *>(html_element->GetSpecialAttr(Markup::LAYOUTA_SCROLL_POS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LAYOUT));

	if (scrollpos) // if we went OOM during layout, we may have failed to allocate and store the SavedScrollPos object
	{
		scrollpos->x = sx;
		scrollpos->y = sy;
		scrollpos->valid = TRUE;
	}
}

/** Restore previously saved (if any) scroll position. */

OP_BOOLEAN
ScrollableArea::GetSavedScrollPos(HTML_Element* html_element, LayoutCoord& x, LayoutCoord& y)
{
	SavedScrollPos* scrollpos = static_cast<SavedScrollPos *>(html_element->GetSpecialAttr(Markup::LAYOUTA_SCROLL_POS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LAYOUT));

	if (scrollpos)
		if (scrollpos->valid)
		{
			x = scrollpos->x;
			y = scrollpos->y;

			scrollpos->valid = FALSE;

			return OpBoolean::IS_TRUE;
		}
		else
			return OpBoolean::IS_FALSE;
	else
	{
		scrollpos = OP_NEW(SavedScrollPos, ());

		if (!scrollpos ||
		    html_element->SetSpecialAttr(Markup::LAYOUTA_SCROLL_POS, ITEM_TYPE_COMPLEX, scrollpos, TRUE, SpecialNs::NS_LAYOUT) < 0)
		{
			OP_DELETE(scrollpos);
			return OpStatus::ERR_NO_MEMORY;
		}

		return OpBoolean::IS_FALSE;
	}
}

/** Update the content size. Fed by child content. Used to calculate the scrollbars. */

void
ScrollableArea::UpdateContentSize(const AbsoluteBoundingBox& child_bounding_box)
{
	if (child_bounding_box.GetContentWidth() != LAYOUT_COORD_MAX)
	{
		LayoutCoord bounding_box_width = child_bounding_box.GetX() + child_bounding_box.GetContentWidth() - LayoutCoord(sc_packed.border_left);

		if (scroll_padding_box_width < bounding_box_width)
			scroll_padding_box_width = bounding_box_width;
	}

	if (child_bounding_box.GetContentHeight() != LAYOUT_COORD_MAX)
	{
		LayoutCoord bounding_box_height = child_bounding_box.GetY() + child_bounding_box.GetContentHeight() - LayoutCoord(sc_packed.border_top);

		if (scroll_padding_box_height < bounding_box_height)
			scroll_padding_box_height = bounding_box_height;
	}
}

/** Based on new layout, re-evaluate the need for scrollbars and enable and disable them as appropriate. */

void
ScrollableArea::DetermineScrollbars(const LayoutInfo& info, const HTMLayoutProperties& props, LayoutCoord border_box_width, LayoutCoord border_box_height)
{
	LayoutCoord hor_scrollbar_width = GetHorizontalScrollbarWidth();
	LayoutCoord ver_scrollbar_width(0);
	LayoutCoord visible_width;
	LayoutCoord visible_height;
	BOOL scrollbar_removed = FALSE;
	BOOL second_evaluation_pass = FALSE;

	for (;;)
	{
		hor_scrollbar_width = GetHorizontalScrollbarWidth();
		ver_scrollbar_width = GetVerticalScrollbarWidth();
		visible_width = border_box_width - LayoutCoord(props.border.left.width + props.border.right.width) - ver_scrollbar_width;
		visible_height = border_box_height - LayoutCoord(props.border.top.width + props.border.bottom.width) - hor_scrollbar_width;

		if (visible_width < 0)
			visible_width = LayoutCoord(0);

		if (visible_height < 0)
			visible_height = LayoutCoord(0);

		if (props.overflow_x == CSS_VALUE_auto && !sc_packed.prevent_auto_scrollbars)
			if (sc_packed.horizontal_on)
			{
				if (!sc_packed.lock_auto_scrollbars && scroll_padding_box_width <= visible_width)
				{
					sc_packed.horizontal_on = 0;
					scrollbar_removed = TRUE;

					// Need to adjust visible height, so that it's evaluated correctly below.

					visible_height += hor_scrollbar_width;
					hor_scrollbar_width = LayoutCoord(0);
				}
			}
			else
				if (scroll_padding_box_width > visible_width)
					sc_packed.horizontal_on = 1;

		if (props.overflow_y == CSS_VALUE_auto && !sc_packed.prevent_auto_scrollbars)
			if (sc_packed.vertical_on)
			{
				if (!sc_packed.lock_auto_scrollbars && scroll_padding_box_height <= visible_height)
				{
					sc_packed.vertical_on = 0;
					scrollbar_removed = TRUE;
				}
			}
			else
				if (scroll_padding_box_height > visible_height)
					sc_packed.vertical_on = 1;

		if (scrollbar_removed && !second_evaluation_pass)
			if (sc_packed.horizontal_on || sc_packed.vertical_on)
			{
				/* If a scrollbar was removed, and overflow is auto in both directions,
				   and there's still one scrollbar left, we need to re-evaluate the need
				   for it now. With the other scrollbar removed, it may be that the other
				   one can go away as well. Even if removing one scrollbar will trigger
				   an additional reflow pass, removing one scrollbar will lock existing
				   auto scrollbars (i.e. the other one), so the next reflow pass will do
				   us no good when it comes to removing the other scrollbar. */

				second_evaluation_pass = TRUE;
				continue;
			}

		if (scrollbar_removed && !info.external_layout_change)
			/* A scrollbar was removed during reflow and this reflow pass was triggered
			   by the layout engine itself. Only allow this to happen once before we lock
			   auto scrollbars (or we may get infinite reflow loops). From now on, we may
			   only add scrollbars, not remove any. This limitation is lifted if min/max
			   widths are cleared. */

			sc_packed.lock_auto_scrollbars = 1;

		break;
	}

	// The scrollable area cannot be smaller than the visible area.

	if (scroll_padding_box_height < visible_height)
		scroll_padding_box_height = visible_height;

	if (scroll_padding_box_width < visible_width)
		scroll_padding_box_width = visible_width;
}

/** Move the area within this content with dx and dy pixels. */

void
ScrollableArea::ScrollContent(FramesDocument* doc, LayoutCoord dx, LayoutCoord dy, BOOL caused_by_input)
{
	/* We'll have to invalidate it (slower than ScrollRect) until we can find
	   out if the content has a fixed positioned background, or has overlapping
	   content. */

	Box* box = GetOwningBox();
	OpRect r = GetEdgesInDocumentCoords(box->GetBorderEdges());
	doc->GetVisualDevice()->Update(r.x, r.y, r.width, r.height);

#ifdef DOCUMENT_EDIT_SUPPORT
	OpDocumentEdit* doc_edit = doc->GetDocumentEdit();
	if (doc_edit && !doc->IsReflowing())
		doc_edit->OnLayoutMoved();
#endif

	doc->GetVisualDevice()->ScrollClipViews(-dx, -dy, this);

	if (caused_by_input)
		OpStatus::Ignore(doc->HandleEvent(ONSCROLL, NULL, box->GetHtmlElement(), SHIFTKEY_NONE));
}


#ifndef MOUSELESS

/** Generate a widget (scrollbar) event based on the DOM event. */

void
ScrollableArea::GenerateWidgetEvent(DOM_EventType event, int offset_x, int offset_y)
{
	OpScrollbar* hit_scrollbar = NULL;
	OpPoint local_point(offset_x, offset_y);
	OpPoint scrollbar_relative_point(local_point);
	OpRect local_horizontal_rect;
	OpRect local_vertical_rect;

	if (horizontal_scrollbar)
		local_horizontal_rect = horizontal_scrollbar->GetRect();

	if (vertical_scrollbar)
		local_vertical_rect = vertical_scrollbar->GetRect();

	if (local_horizontal_rect.Contains(local_point))
	{
		hit_scrollbar = horizontal_scrollbar;
		scrollbar_relative_point -= local_horizontal_rect.TopLeft();
	}
	else
		if (local_vertical_rect.Contains(local_point))
		{
			hit_scrollbar = vertical_scrollbar;
			scrollbar_relative_point -= local_vertical_rect.TopLeft();
		}

	if (hit_scrollbar)
		switch (event)
		{
		case ONMOUSEDOWN:
			hit_scrollbar->GenerateOnMouseDown(scrollbar_relative_point, current_mouse_button, 1);
			break;
		case ONMOUSEMOVE:
			OP_ASSERT(OpWidget::hooked_widget == NULL);
			hit_scrollbar->GenerateOnMouseMove(scrollbar_relative_point);
			break;
		}
}

#endif // !MOUSELESS

/** Get the width of the currently displayed horizontal scrollbar, if any, otherwise 0. */

LayoutCoord
ScrollableArea::GetHorizontalScrollbarWidth() const
{
	if (HasHorizontalScrollbar() && horizontal_scrollbar)
		return LayoutCoord(horizontal_scrollbar->GetInfo()->GetHorizontalScrollbarHeight());

	return LayoutCoord(0);
}

/** Get the width of the currently displayed vertical scrollbar, if any, otherwise 0. */

LayoutCoord
ScrollableArea::GetVerticalScrollbarWidth() const
{
	if (HasVerticalScrollbar() && vertical_scrollbar)
		return LayoutCoord(vertical_scrollbar->GetInfo()->GetVerticalScrollbarWidth());

	return LayoutCoord(0);
}

/** Return TRUE if the specified point (in document coordinates) is over a scrollbar. */

BOOL
ScrollableArea::IsOverScrollbar(const OpPoint& pos)
{
	return HasVerticalScrollbar() && vertical_scrollbar->GetDocumentRect(TRUE).Contains(pos) ||
		HasHorizontalScrollbar() && horizontal_scrollbar->GetDocumentRect(TRUE).Contains(pos);
}

/** Return the current horizontal scroll value. */

LayoutCoord
ScrollableArea::GetViewX(BOOL constrained) const
{
	if (constrained)
		return LayoutCoord(horizontal_scrollbar ? horizontal_scrollbar->GetValue() : scroll_x);
	else
		return unconstrained_scroll_x;
}

/** Return the current vertical scroll value. */

LayoutCoord
ScrollableArea::GetViewY(BOOL constrained) const
{
	if (constrained)
		return LayoutCoord(vertical_scrollbar ? vertical_scrollbar->GetValue() : scroll_y);
	else
		return unconstrained_scroll_y;
}

/** Set horizontal scroll value. */

void
ScrollableArea::SetViewX(LayoutCoord new_x, BOOL caused_by_input, BOOL by_layout)
{
	if (horizontal_scrollbar)
	{
		horizontal_scrollbar->SetValue(new_x, caused_by_input);
		if (!by_layout)
			unconstrained_scroll_x = LayoutCoord(horizontal_scrollbar->GetValue());
	}
	else
	{
		LayoutCoord old_x = scroll_x;

		scroll_x = MAX(LayoutCoord(0), MIN(new_x, total_horizontal_scroll_amount));

		unconstrained_scroll_x = by_layout ? new_x : scroll_x;

		if (old_x != scroll_x)
			ScrollContent(doc, scroll_x - old_x, LayoutCoord(0), caused_by_input);
	}
}

/** Set vertical scroll value. */

void
ScrollableArea::SetViewY(LayoutCoord new_y, BOOL caused_by_input, BOOL by_layout)
{
	if (vertical_scrollbar)
	{
		vertical_scrollbar->SetValue(new_y, caused_by_input);
		if (!by_layout)
			unconstrained_scroll_y = LayoutCoord(vertical_scrollbar->GetValue());
	}
	else
	{
		LayoutCoord old_y = scroll_y;

		scroll_y = MAX(LayoutCoord(0), MIN(new_y, total_vertical_scroll_amount));

		unconstrained_scroll_y = by_layout ? new_y : scroll_y;

		if (old_y != scroll_y)
			ScrollContent(doc, LayoutCoord(0), scroll_y - old_y, caused_by_input);
	}
}

/** Update and report current position of CoreView and scrollbars, relative to the document. */

void
ScrollableArea::UpdatePosition(const HTMLayoutProperties& props, VisualDevice* visual_device, LayoutCoord visible_width, LayoutCoord visible_height)
{
	OpRect visible_area(props.border.left.width, props.border.top.width, visible_width, visible_height);

	SetPosition(visual_device->GetCTM());

	// Update scrollbars' position

	if (sc_packed.vertical_on)
	{
		LayoutCoord left(props.border.left.width);
		LayoutCoord top(props.border.top.width);
		AffinePos ctm = GetPosInDocument();

		if (LeftHandScrollbar())
			visible_area.x += GetVerticalScrollbarWidth();
		else
			left += visible_width;

		ctm.AppendTranslation(left, top);
		vertical_scrollbar->SetPosInDocument(ctm);
	}

	if (sc_packed.horizontal_on)
	{
		LayoutCoord left(props.border.left.width);
		LayoutCoord top(props.border.top.width + visible_height);
		AffinePos ctm = GetPosInDocument();

		if (LeftHandScrollbar())
			left += GetVerticalScrollbarWidth();

		ctm.AppendTranslation(left, top);
		horizontal_scrollbar->SetPosInDocument(ctm);
	}

	OpRect document_visible_area = GetEdgesInDocumentCoords(visible_area);

	// Update CoreView to the visible area.

	document_visible_area.x -= visual_device->GetRenderingViewX();
	document_visible_area.y -= visual_device->GetRenderingViewY();
	document_visible_area = visual_device->ScaleToScreen(document_visible_area);

	// FIXME: I believe this is supposed to mimic what goes on in
	// ClipView(?), but in the context of a transform I guess it'll
	// all end up on it's head.

	/* Must translate document_padding_edges so it is relative to ancestor
	   scrollables' CoreViews (if any). */

	if (CoreView::m_parent)
	{
		for (CoreView* parent = CoreView::m_parent;
			 parent && parent->GetIsLayoutBox();
			 parent = parent->GetParent())
		{
			OpRect parent_extents = parent->GetExtents();

			document_visible_area.x -= parent_extents.x;
			document_visible_area.y -= parent_extents.y;
		}

		// FIXME: Above FIXME most likely applies here as well.

		CoreView::SetPos(AffinePos(document_visible_area.x, document_visible_area.y));
		CoreView::SetSize(document_visible_area.width, document_visible_area.height);
	}
}

/** Paint the scrollbars. */

void
ScrollableArea::PaintScrollbars(const HTMLayoutProperties& props, VisualDevice* visual_device, LayoutCoord visible_width, LayoutCoord visible_height)
{
	VisualDevice* old_vis_dev = vertical_scrollbar ? vertical_scrollbar->GetVisualDevice() : horizontal_scrollbar ? horizontal_scrollbar->GetVisualDevice() : NULL;

	if (vertical_scrollbar)
		vertical_scrollbar->SetVisualDevice(visual_device);

	if (horizontal_scrollbar)
		horizontal_scrollbar->SetVisualDevice(visual_device);

	// Inform CoreView and widgets machineries about our current position.

	UpdatePosition(props, visual_device, visible_width, visible_height);

	COLORREF old_color = visual_device->GetColor();
	LayoutCoord horizontal_top = LayoutCoord(props.border.top.width) + visible_height;
	LayoutCoord horizontal_left = LayoutCoord(props.border.left.width);
	LayoutCoord vertical_top = LayoutCoord(props.border.top.width);
	LayoutCoord vertical_left = LayoutCoord(props.border.left.width);

	if (!LeftHandScrollbar())
		vertical_left += visible_width;

	if (sc_packed.vertical_on)
	{
		// Paint vertical scrollbar.

		visual_device->Translate(vertical_left, vertical_top);
		vertical_scrollbar->GenerateOnPaint(vertical_scrollbar->GetBounds());
		visual_device->Translate(-vertical_left, -vertical_top);

		if (LeftHandScrollbar())
			horizontal_left += GetVerticalScrollbarWidth();
	}

	if (sc_packed.horizontal_on)
	{
		// Paint horizontal scrollbar.

		visual_device->Translate(horizontal_left, horizontal_top);
		horizontal_scrollbar->GenerateOnPaint(horizontal_scrollbar->GetBounds());
		visual_device->Translate(-horizontal_left, -horizontal_top);
	}

	if (sc_packed.vertical_on && sc_packed.horizontal_on)
	{
		// Paint corner between vertical and horizontal scrollbar.

		OpRect vr = vertical_scrollbar->GetRect();
		OpRect hr = horizontal_scrollbar->GetRect();
		int vertical_bottom = vertical_top + vr.height;

		visual_device->SetColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON));
		visual_device->Translate(vertical_left, vertical_bottom);
		visual_device->FillRect(OpRect(0, 0, vr.width, hr.height));
		visual_device->Translate(-vertical_left, -vertical_bottom);
	}

	// Restore state.

	visual_device->SetColor(old_color);

	if (old_vis_dev && visual_device != old_vis_dev)
	{
		if (vertical_scrollbar)
			vertical_scrollbar->SetVisualDevice(old_vis_dev);

		if (horizontal_scrollbar)
			horizontal_scrollbar->SetVisualDevice(old_vis_dev);
	}
}

/** Trigger scroll operation based on the given input action. */

BOOL
ScrollableArea::TriggerScrollInputAction(OpInputAction* action, BOOL use_scrollbars)
{
	int times = action->GetActionData();

	if (times < 1)
		times = 1;

	int scroll_by = (action->IsMouseInvoked() ? DISPLAY_SCROLLWHEEL_STEPSIZE : DISPLAY_SCROLL_STEPSIZE) * times;
	int new_x = 0;
	int new_y = 0;
	int old_x = 0;
	int old_y = 0;
	BOOL smooth = TRUE;

	if (use_scrollbars)
	{
		if (sc_packed.horizontal_on)
		{
			old_x = GetViewX();
			new_x = horizontal_scrollbar->IsSmoothScrollRunning() ? horizontal_scrollbar->GetSmoothScrollTargetValue() : horizontal_scrollbar->GetValue();
		}

		if (sc_packed.vertical_on)
		{
			old_y = GetViewY();
			new_y = vertical_scrollbar->IsSmoothScrollRunning() ? vertical_scrollbar->GetSmoothScrollTargetValue() : vertical_scrollbar->GetValue();
		}
	}
	else
	{
		old_x = new_x = GetViewX();
		old_y = new_y = GetViewY();
	}

	switch (action->GetAction())
	{
	case OpInputAction::ACTION_SCROLL:			new_y += times; break;
	case OpInputAction::ACTION_PAGE_UP:			new_y -= VisualDevice::GetPageScrollAmount(GetOwningBox()->GetHeight()); break;
	case OpInputAction::ACTION_PAGE_LEFT:		new_x -= VisualDevice::GetPageScrollAmount(GetOwningBox()->GetWidth()); break;
	case OpInputAction::ACTION_PAGE_DOWN:		new_y += VisualDevice::GetPageScrollAmount(GetOwningBox()->GetHeight()); break;
	case OpInputAction::ACTION_PAGE_RIGHT:		new_x += VisualDevice::GetPageScrollAmount(GetOwningBox()->GetWidth()); break;
	case OpInputAction::ACTION_SCROLL_UP:		new_y -= scroll_by; break;
	case OpInputAction::ACTION_SCROLL_DOWN:		new_y += scroll_by; break;
	case OpInputAction::ACTION_SCROLL_LEFT:		new_x -= scroll_by; break;
	case OpInputAction::ACTION_SCROLL_RIGHT:	new_x += scroll_by; break;
	case OpInputAction::ACTION_GO_TO_START:		new_y = 0; smooth = FALSE; break;
	case OpInputAction::ACTION_GO_TO_END:		new_y = scroll_padding_box_height; smooth = FALSE; break;
#ifdef PAN_SUPPORT
# ifdef ACTION_PAN_X_ENABLED
	case OpInputAction::ACTION_PAN_X:			new_x += action->GetActionData(); break;
# endif // ACTION_PAN_X_ENABLED
# ifdef ACTION_PAN_Y_ENABLED
	case OpInputAction::ACTION_PAN_Y:			new_y += action->GetActionData(); break;
# endif // ACTION_PAN_Y_ENABLED
# ifdef ACTION_COMPOSITE_PAN_ENABLED
	case OpInputAction::ACTION_COMPOSITE_PAN:
	{
		OP_ASSERT(use_scrollbars); // View-based panning is not supported.

		INT16* delta = (INT16*)action->GetActionData();

		if (sc_packed.horizontal_on)
		{
			horizontal_scrollbar->SetValue(old_x + delta[0], TRUE, TRUE, FALSE);
			delta[0] -= (INT16) (horizontal_scrollbar->GetValue() - old_x);
		}

		if (sc_packed.vertical_on)
		{
			vertical_scrollbar->SetValue(old_y + delta[1], TRUE, TRUE, FALSE);
			delta[1] -= (INT16) (vertical_scrollbar->GetValue() - old_y);
		}

		return !delta[0] && !delta[1];
	}
# endif // ACTION_COMPOSITE_PAN_ENABLED
#endif // PAN_SUPPORT
	default:
		return FALSE;
	}

	if (use_scrollbars)
	{
		BOOL moved_horizontally = FALSE;
		BOOL moved_vertically = FALSE;

		if (sc_packed.horizontal_on)
		{
			horizontal_scrollbar->SetValue(new_x, TRUE, TRUE, smooth);

			long new_target_x = horizontal_scrollbar->IsSmoothScrollRunning() ? horizontal_scrollbar->GetSmoothScrollTargetValue() : horizontal_scrollbar->GetValue();

			if (old_x != new_target_x)
				moved_horizontally = TRUE;
		}

		if (sc_packed.vertical_on)
		{
			vertical_scrollbar->SetValue(new_y, TRUE, TRUE, smooth);

			long new_target_y = vertical_scrollbar->IsSmoothScrollRunning() ? vertical_scrollbar->GetSmoothScrollTargetValue() : vertical_scrollbar->GetValue();

			if (old_y != new_target_y)
				moved_vertically = TRUE;
		}

		return moved_horizontally || moved_vertically;
	}
	else
	{
		// Keep new view position within bounds.

		new_x = MAX(0, MIN(new_x, scroll_padding_box_width));
		new_y = MAX(0, MIN(new_y, scroll_padding_box_height));

		if (new_x != old_x)
			SetViewX(LayoutCoord(new_x), FALSE);

		if (new_y != old_y)
			SetViewY(LayoutCoord(new_y), FALSE);

		return old_x != new_x || old_y != new_y;
	}
}
