/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/vis_dev.h"

#ifdef SKIN_OUTLINE_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif

// Define to get debugging of outline regions (Windows only)
//#define DEBUG_VD_OUTLINE_REGION

#ifdef DEBUG_VD_OUTLINE_REGION
#include <stdio.h>
void DEBUGOUTPRINT(char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	char buf[250]; // ARRAY OK 2009-06-01 wonko
	op_vsnprintf(buf, 250, format, arguments);
	va_end(arguments);
	OutputDebugStringA(buf);
}
# define DEBUGOUT(str) DEBUGOUTPRINT(str)
# define DEBUGOUT1(str,a) DEBUGOUTPRINT(str, a)
# define DEBUGOUT2(str,a,b) DEBUGOUTPRINT(str, a, b)
# define DEBUGOUT3(str,a,b,c) DEBUGOUTPRINT(str, a, b, c)
# define DEBUGOUT4(str,a,b,c,d) DEBUGOUTPRINT(str, a, b, c, d)
#else
# define DEBUGOUT(str) ((void)0)
# define DEBUGOUT1(str,a) ((void)0)
# define DEBUGOUT2(str,a,b) ((void)0)
# define DEBUGOUT3(str,a,b,c) ((void)0)
# define DEBUGOUT4(str,a,b,c,d) ((void)0)
#endif

/** Sorted (in X or Y) edge of a rect, used in outline algorithm. */
struct SORTED_RECT {
	int val;		///< x or y
	int index;		///< which rectangle index it belongs to
	bool first;		///< first or last edge.
};

/** Scanline for vertical or horizontal lines, used in outline algorithm. */
struct OUTLINE_SCANLINE {
	int pos1, pos2;	///< start and stop (x or y)
	union {
		struct {
			unsigned int corner_left:1;
			unsigned int corner_right:1;
			unsigned int corner_left_intersect:1;
			unsigned int corner_right_intersect:1;
		} packed;
		unsigned char packed_init;
	};
};

/** Sort outline edges */
int sorted_rect_cmp(const void *arg1,const void *arg2)
{
	SORTED_RECT *r1 = (SORTED_RECT *) arg1;
	SORTED_RECT *r2 = (SORTED_RECT *) arg2;
	return (r1->val < r2->val ? -1 : (r1->val > r2->val ? 1 : 0));
}

// == VisualDevice implementation ==========================================================

OP_STATUS VisualDevice::BeginOutline(int width, COLORREF color, int pen_style, HTML_Element *element, int offset)
{
	DEBUGOUT("BeginOutline\n");
	if (!m_outlines_enabled)
		return OpStatus::ERR;

#ifdef SKIN_OUTLINE_SUPPORT
/*	if (pen_style == CSS_VALUE__o_highlight_border)
	{
		OpSkinElement* skin_elm_inside = NULL;
		if (skin_elm_inside = g_skin_manager->GetSkinElement(UNI_L("Active Element Inside")))
		{
			// Uncomment if we want the skin elements padding affect the outline offset.
			//INT32 padding;
			//skin_elm_inside->GetPadding(&padding, &padding, &padding, &padding, 0);
			//offset = padding;

			// This will ignore outline width and take the skin border width. Unfortunately this doesn't work because
			// some code in layout needs to do it too. (Appearing outlines invalidates and make pending paint rect)
			//INT32 border = 0;
			//skin_elm_inside->GetBorderWidth(&border, &border, &border, &border, 0);
			//width = border;
		}
	}*/
#endif

	outlines_open_count++;

	// We might start the same outline again. Check if the element is matching the last open outline and continue it.
	if (element)
	{
		if (outlines_hash.Contains(element))
		{
			VisualDeviceOutline* old_o = NULL;

			outlines_hash.GetData(element, &old_o);

			old_o->Out();

			// Extra precaution if layout gives us unbalanced BeginOutline/EndOutline
			OP_ASSERT(current_outline != old_o);
			if (current_outline && current_outline != old_o)
				old_o->Follow(current_outline);
			else
				old_o->Into(&outlines);

			current_outline = old_o;

#ifdef DISPLAY_SPOTLIGHT
			VDSpotlight *sl = GetSpotlight(element);
			if (sl)
				current_outline->SetSpotlight(sl);
#endif

			return OpStatus::OK;
		}
	}

	VisualDeviceOutline *o = OP_NEW(VisualDeviceOutline, (width, color, pen_style, offset, element));
	if (!o)
		return OpStatus::ERR_NO_MEMORY;

	if (element)
	{
		RETURN_IF_ERROR(outlines_hash.Add(element, o));
	}

	if (current_outline)
		o->Follow(current_outline);
	else
		o->Into(&outlines);

	current_outline = o;

#ifdef DISPLAY_SPOTLIGHT
	VDSpotlight *sl = element ? GetSpotlight(element) : NULL;
	if (sl)
		current_outline->SetSpotlight(sl);
#endif
	return OpStatus::OK;
}

void VisualDevice::EndOutline()
{
	OP_ASSERT(current_outline);
	if (current_outline)
	{
		VisualDeviceOutline* pred = (VisualDeviceOutline *) current_outline->Pred();
		if (current_outline->IsEmpty())
		{
			DEBUGOUT("outline was deleted because it had no rects\n");

			RemoveOutline(current_outline);
			OP_DELETE(current_outline);
		}
#ifdef SKIN_OUTLINE_SUPPORT
		// If we have a skinned outline nestled (and contained) in another skinned outline, we should skip drawing the outer outline.
		else if (current_outline->GetPenStyle() == CSS_VALUE__o_highlight_border &&
				pred && pred->GetPenStyle() == CSS_VALUE__o_highlight_border)
		{
			if (pred->GetBoundingRect().Contains(current_outline->GetBoundingRect()))
				pred->SetIgnore();
		}
#endif

		current_outline = pred;

		// We must guarantee that current_outline will not be a old closed outline when it should be NULL, since it might be used as a insertpoint in BeginOutline.
		// Could also be implemented by having 2 lists. One for currently active outlines, and one for closed ones.
		outlines_open_count--;
		if (!outlines_open_count)
			current_outline = NULL;
	}
	DEBUGOUT1("EndOutline. Count: %d\n", outlines.Cardinal());
}

OP_STATUS VisualDevice::PushOutlineRect(const OpRect& rect)
{
	OP_ASSERT(current_outline);
	if (current_outline)
	{
		//DEBUGOUT4("PushOutlineRect: %d %d %d %d\n", rect.x + translation_x, rect.y + translation_y, rect.width, rect.height);

		// Add translation/transformation here since it will be lost at any time after this call.
		OpRect o_rect = ToBBox(rect);
		OpRect o_rect_clipped(o_rect);

		// Clip to current layout cliprect (if there is any). We must NOT use the painters clipping since that might have non-layout related
		// clipping which should not affect the shape of the outline.
		// The outline itself won't be clipped inside, only the region (so the outline will surround it).
		OpRect clip_rect;
		BOOL clipped = bg_cs.GetClipping(clip_rect);
		if (clipped)
			o_rect_clipped.IntersectWith(clip_rect);

		RETURN_IF_ERROR(current_outline->AddRect(o_rect_clipped));

		if (clipped)
		{
			// We must still extend the bounding box with the nonclipped rect to make sure we don't confuse
			// layout (which does clipping itself to not get into paintloops)
			current_outline->ExtendBoundingRect(o_rect);
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

BOOL VisualDevice::HasCurrentlyOpenOutline(HTML_Element* element) const
{
	VisualDeviceOutline* outline;
	if (OpStatus::IsSuccess(outlines_hash.GetData(element, &outline)))
		return outline == current_outline;

	return FALSE;
}

void VisualDevice::SetContentFound(DISPLAY_CONTENT content)
{
	if (!current_outline)
		return;

	if (content == DISPLAY_CONTENT_IMAGE)
		current_outline->SetContainsImage();
	else if (content == DISPLAY_CONTENT_TEXT)
		current_outline->SetContainsText();
}

OP_STATUS VisualDevice::DrawOutlines()
{
	// Extra precaution if layout gives us unbalanced BeginOutline/EndOutline
	OP_ASSERT(outlines_open_count == 0);
	outlines_open_count = 0;

	int old_translation_x = translation_x;
	int old_translation_y = translation_y;

	// Make sure we clear translation before we draw outline.
	// If this assert trig, the layout engine has a bug. But we will still draw the outlines correctly.
	OP_ASSERT(translation_x == 0 && translation_y == 0);
	translation_x = 0;
	translation_y = 0;

	int count = 0;
	VisualDeviceOutline *o = (VisualDeviceOutline *) outlines.First();
	while (o)
	{
		if (o->GetBoundingRect().Intersecting(doc_display_rect))
			RETURN_IF_ERROR(o->Paint(this));
		o = (VisualDeviceOutline *) o->Suc();
		count++;
	}
#ifdef DISPLAY_SPOTLIGHT
	// Paint spotlight on top of regular outlines.
	for (UINT32 i = 0; i < spotlights.GetCount(); i++)
	{
		VDSpotlight *s = spotlights.Get(i);

		o = (VisualDeviceOutline *) outlines.First();
		while (o && o->GetSpotlight() != s)
			o = (VisualDeviceOutline *) o->Suc();

		if (o)
			s->UpdateInfo(this, o);

		if (o && o->GetBoundingRect().Intersecting(doc_display_rect))
			RETURN_IF_ERROR(o->GetSpotlight()->Paint(this));
		else
			s->Paint(this);
	}
#endif // DISPLAY_SPOTLIGHT

	DEBUGOUT1("number of outlines %d\n", count);

	translation_x = old_translation_x;
	translation_y = old_translation_y;

	return OpStatus::OK;
}

BOOL VisualDevice::EnlargeWithIntersectingOutlines(OpRect& rect)
{
	OpRect old_rect = rect;
	VisualDeviceOutline *o = (VisualDeviceOutline *) outlines.First();
	while (o)
	{
		OpRect o_bounding_rect = o->GetBoundingRect();

		if (rect.Intersecting(o_bounding_rect))
		{
			BOOL safe_to_skip = FALSE;
			if (o->GetRegion()->num_rects == 1 && !o->GetSpotlight() && !o->GetTextHighlight())
			{
				// If the outline fully contains the rect inside the outline, we can ignore it since it
				// doesn't interfer with the drawing.
#ifdef SKIN_OUTLINE_SUPPORT
				// Not for skinned outline though
				if (o->GetPenStyle() != CSS_VALUE__o_highlight_border)
#endif
				{
					// Inset by thickness and offset of the border since we need the innermost painted rectangle.
					// Also add 1, since we are intrested in repaints of the layoutbox that owns this outline.
					OpRect tmp_bounding_rect = o_bounding_rect.InsetBy(o->GetPenWidth() + o->GetOffset() + 1);
					if (tmp_bounding_rect.Contains(rect))
					{
						safe_to_skip = TRUE;
						DEBUGOUT4("Skip enlarging cliprect because of outline (fully contains rect %d %d %d %d)\n", rect.x, rect.y, rect.width, rect.height);
					}
				}
			}

			if (!safe_to_skip)
			{
				rect.UnionWith(o_bounding_rect);

				DEBUGOUT4("Enlarging cliprect because of outline  %d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
			}
		}
		o = (VisualDeviceOutline *) o->Suc();
	}
	return !old_rect.Equals(rect);
}

void VisualDevice::RemoveIntersectingOutlines(const OpRect& rect, BOOL remove_intersecting)
{
	VisualDeviceOutline *o = (VisualDeviceOutline *) outlines.First();

	while (o)
	{
		VisualDeviceOutline *tmp = o;
		o = (VisualDeviceOutline *) o->Suc();

		BOOL intersecting = rect.Intersecting(tmp->GetBoundingRect());
		if (intersecting == remove_intersecting)
		{
			DEBUGOUT(remove_intersecting ? "Removed outline because it intersect with paintarea\n" : "Removed outline because it doesn't intersect with paintarea\n");

			if (remove_intersecting)
			{
				/* Add the outline to be removed to the temporary outline
				   list for later comparison. */
				RemoveOutline(tmp);

				tmp->Into(&temporary_outlines);
				if (tmp->GetHtmlElement())
				{
					OpStatus::Ignore(temporary_outlines_hash.Add(tmp->GetHtmlElement(), tmp));
				}
			}
			else
			{
				RemoveOutline(tmp);
				OP_DELETE(tmp);
			}
			//OP_ASSERT(current_outline != tmp);
			if (current_outline == tmp)
				current_outline = NULL;
		}
	}
}

void VisualDevice::UpdateAddedOrChangedOutlines(const OpRect& paint_rect)
{
	VisualDeviceOutline* iter = (VisualDeviceOutline*)outlines.First();

	while (iter)
	{
		HTML_Element* iter_elm = iter->GetHtmlElement();
		OpRect iter_bounding_rect = iter->GetBoundingRect();

		if (iter_elm && iter_bounding_rect.Intersecting(paint_rect))
		{
			BOOL exists = FALSE;

			if (temporary_outlines_hash.Contains(iter_elm))
			{
				VisualDeviceOutline* previous_outline = NULL;

				temporary_outlines_hash.GetData(iter_elm, &previous_outline);

				if (previous_outline->GetBoundingRect().Contains(iter_bounding_rect))
					exists = TRUE;
			}

			if (!exists && !paint_rect.Contains(iter_bounding_rect))
			{
				OpRect br = iter->GetBoundingRect();
				Update(br.x, br.y, br.width, br.height);
			}
		}
		iter = (VisualDeviceOutline*)iter->Suc();
	}
}

void VisualDevice::RemoveOutline(VisualDeviceOutline* o)
{
	o->Out();
	if (o->GetHtmlElement())
	{
		VisualDeviceOutline* tmp;
		OP_STATUS stat = outlines_hash.Remove(o->GetHtmlElement(), &tmp);
		OpStatus::Ignore(stat);
		OP_ASSERT(stat == OpStatus::OK);
	}
}

void VisualDevice::RemoveAllOutlines()
{
	// Delete all outline regions
	outlines_hash.RemoveAll();
	outlines.Clear();
	current_outline = NULL;

	RemoveAllTemporaryOutlines();
}

void VisualDevice::RemoveAllTemporaryOutlines()
{
	temporary_outlines_hash.RemoveAll();
	temporary_outlines.Clear();
}

// == VisualDeviceOutline implementation ==========================================================

BOOL VisualDeviceOutline::IsOverlappingEdge(VisualDevice *vis_dev)
{
	OpRect viewport = vis_dev->GetRenderingViewport();
	OpRect rect = GetBoundingRect();
	return viewport.Intersecting(rect) && !viewport.Contains(rect);
}

OP_STATUS VisualDeviceOutline::AddRect(const OpRect& rect)
{
	// Enlarge with outline width (border is painted *inside* the rects)
	return m_region.AddRectIfNotContained(rect.InsetBy(-m_width));
}

void VisualDeviceOutline::ExtendBoundingRect(const OpRect& rect)
{
	m_region.ExtendBoundingRect(rect.InsetBy(-m_width));
}

OP_STATUS VisualDeviceOutline::Paint(VisualDevice *vd)
{
	if (!m_region.num_rects || m_ignore)
		return OpStatus::OK;
	int i;

#ifdef DEBUG_VD_OUTLINE_REGION
	// debug bounding box
	vd->SetColor(32, 32, 32, 128);
	vd->DrawRect(m_region.bounding_rect);
	// debug region rects
	for(i = 0; i < m_region.num_rects; i++)
	{
		vd->SetColor(0, 192, 0, 128);
		vd->DrawRect(m_region.rects[i]);
	}
#endif

#ifdef SKIN_OUTLINE_SUPPORT
	if (m_pen_style == CSS_VALUE__o_highlight_border)
	{
		// Fill region with tiled skin center
		// Would ideally be done by the linedrawing algorithm.

		OpSkinElement* skin_elm_inside = NULL;
		OpSkinElement* skin_elm_outside = NULL;

		if (ShouldDrawImageStyle())
		{
			skin_elm_inside = g_skin_manager->GetSkinElement("Active Element Inside Image");
			skin_elm_outside = g_skin_manager->GetSkinElement("Active Element Outside Image");
		}

		if (!skin_elm_inside)
		{
			skin_elm_inside = g_skin_manager->GetSkinElement("Active Element Inside");
		}
		if (!skin_elm_outside)
		{
			skin_elm_outside = g_skin_manager->GetSkinElement("Active Element Outside");
		}

		BOOL has_edge_skinning = FALSE;
		if (skin_elm_inside)
		{
			BgRegion inner_region;
			int i;
			for(i = 0; i < m_region.num_rects; i++)
			{
				// Inset with border which (we have already painted).
				//OpRect r = m_region.rects[i].InsetBy(m_width); // will make holes
				OpRect r = m_region.rects[i];
				if (!r.IsEmpty() && OpStatus::IsError(inner_region.IncludeRect(r)))
					return OpStatus::ERR_NO_MEMORY;
			}

			// Clip & draw the outside-skin where (and if) the highlight intersects with the edge of the visualdevice.
			OpRect viewport(0, 0, vd->VisibleWidth(), vd->VisibleHeight());
			if (skin_elm_outside)
			{
				has_edge_skinning = IsOverlappingEdge(vd);

				if (has_edge_skinning)
				{
					INT32 left, top, right, bottom;
					skin_elm_inside->GetPadding(&left, &top, &right, &bottom, 0);
					left = vd->ScaleToScreen(left);
					top = vd->ScaleToScreen(top);
					right = vd->ScaleToScreen(right);
					bottom = vd->ScaleToScreen(bottom);
					viewport.x += left;
					viewport.y += top;
					viewport.width -= left + right;
					viewport.height -= top + bottom;

					// Draw the parts of the skin that is at the edge or the visible area of the screen.
					// (Do some magic with the clipping to achieve the correct result)
					OpRect r[4];
					r[0].Set(0, 0, left, vd->VisibleHeight());
					r[1].Set(vd->VisibleWidth() - right, 0, right, vd->VisibleHeight());
					r[2].Set(left, 0, vd->VisibleWidth() - left - right, top);
					r[3].Set(left, vd->VisibleHeight() - bottom, vd->VisibleWidth() - left - right, bottom);

					for(int j = 0; j < 4; j++)
						if (OpStatus::IsSuccess(vd->painter->SetClipRect(vd->OffsetToContainer(r[j]))))
						{
							for(i = 0; i < inner_region.num_rects; i++)
							{
								skin_elm_outside->DrawSkinPart(vd, inner_region.rects[i], SKINPART_TILE_CENTER);
							}
							vd->painter->RemoveClipRect();
						}
				}
			}

			BOOL clipped = has_edge_skinning && OpStatus::IsSuccess(vd->painter->SetClipRect(vd->OffsetToContainer(viewport)));
			for(i = 0; i < inner_region.num_rects; i++)
			{
				skin_elm_inside->DrawSkinPart(vd, inner_region.rects[i], SKINPART_TILE_CENTER);
			}
			if (clipped)
				vd->painter->RemoveClipRect();
		}
		SetNeedUpdateOnScroll(has_edge_skinning);
	}
#endif

	// Allocate temporary data
	int num_sorted = m_region.num_rects * 2;
	SORTED_RECT *order_x = OP_NEWA(SORTED_RECT, num_sorted);
	SORTED_RECT *order_y = OP_NEWA(SORTED_RECT, num_sorted);
	OUTLINE_SCANLINE *scanline = OP_NEWA(OUTLINE_SCANLINE, num_sorted);
	if (!order_x || !order_y || !scanline)
	{
		OP_DELETEA(order_x);
		OP_DELETEA(order_y);
		OP_DELETEA(scanline);
		return OpStatus::ERR_NO_MEMORY;
	}

	// Sort horizontal and vertical edges in order_x and order_y.
	for(i = 0; i < m_region.num_rects; i++)
	{
		order_x[i * 2].val = m_region.rects[i].x;
		order_x[i * 2].index = i;
		order_x[i * 2].first = TRUE;
		order_x[i * 2 + 1].val = m_region.rects[i].x + m_region.rects[i].width;
		order_x[i * 2 + 1].index = i;
		order_x[i * 2 + 1].first = FALSE;
		order_y[i * 2].val = m_region.rects[i].y;
		order_y[i * 2].index = i;
		order_y[i * 2].first = TRUE;
		order_y[i * 2 + 1].val = m_region.rects[i].y + m_region.rects[i].height;
		order_y[i * 2 + 1].index = i;
		order_y[i * 2 + 1].first = FALSE;
	}
	op_qsort(order_x, num_sorted, sizeof(SORTED_RECT), sorted_rect_cmp);
	op_qsort(order_y, num_sorted, sizeof(SORTED_RECT), sorted_rect_cmp);

	// We first draw all horizontal lines and corners, and then all vertical lines and corners.
	// Instead of duplicating the code with a vertical version, we switch X and Y axis and use
	// the same code.
	PaintOutlineScanlines(vd, order_x, order_y, scanline, num_sorted, FALSE);
	PaintOutlineScanlines(vd, order_y, order_x, scanline, num_sorted, TRUE);

	OP_DELETEA(order_x);
	OP_DELETEA(order_y);
	OP_DELETEA(scanline);
	return OpStatus::OK;
}

void VisualDeviceOutline::PaintOutlineScanlines(VisualDevice *vd, SORTED_RECT *order_x, SORTED_RECT *order_y, OUTLINE_SCANLINE *scanline, int num_sorted, BOOL x_is_y)
{
	int thickness = m_width; // For readability of the code below

#ifdef SKIN_OUTLINE_SUPPORT
	OpSkinElement* skin_elm_inside = NULL;
	OpSkinElement* skin_elm_border_inverse = NULL;
	if (m_pen_style == CSS_VALUE__o_highlight_border)
	{
		if (ShouldDrawImageStyle())
		{
			skin_elm_inside = g_skin_manager->GetSkinElement("Active Element Inside Image");
		}

		if (!skin_elm_inside)
		{
			skin_elm_inside = g_skin_manager->GetSkinElement("Active Element Inside");
		}

		if (ShouldDrawImageStyle())
		{
			skin_elm_border_inverse = g_skin_manager->GetSkinElement("Active Element Inside Image Inverse");
		}

		if (!skin_elm_border_inverse)
		{
			skin_elm_border_inverse = g_skin_manager->GetSkinElement("Active Element Inside Inverse");
		}
	}
#endif

	// Iterate from top to bottom and build vertical scanline info.
	int oy = 0;
	while (oy < num_sorted)
	{
		// Find next y
		int y = order_y[oy].val;
		int next_oy = oy + 1;
		while (next_oy < num_sorted && order_y[next_oy].val == y)
			next_oy++;
		//int y_stop = (next_oy == num_sorted) ? y : order_y[next_oy].val;

		int inside_count = 0;
		int inside_count_stop = 0;
		int num_ipoints = 0;

		for(int ox = 0; ox < num_sorted; ox++)
		{
			int	next_ox = (ox == num_sorted - 1) ? ox : ox + 1;
			int x = order_x[ox].val;
			int x_stop = order_x[next_ox].val;

			OpRect rect_b = m_region.rects[order_x[ox].index];

			if (x_is_y)
			{
				// Rotate rectangle
				int tmp = rect_b.x;
				rect_b.x = rect_b.y;
				rect_b.y = tmp;
				tmp = rect_b.width;
				rect_b.width = rect_b.height;
				rect_b.height = tmp;
			}

			BOOL corner_left = FALSE;
			BOOL corner_right = FALSE;
			BOOL corner_left_intersect = FALSE;
			BOOL corner_right_intersect = FALSE;

			if (rect_b.y + rect_b.height >= y && rect_b.y <= y)
			{
				if (inside_count && (rect_b.y == y || rect_b.y + rect_b.height == y))
				{
					// Continue. Edges go inside each other.
				}
				else if (rect_b.y < y && rect_b.y + rect_b.height > y)
				{
					// Intersect. Intersection corner should be set, and scanline stopped.
					if (order_x[ox].first)
					{
						inside_count_stop++;

						if (num_ipoints > 0 && scanline[num_ipoints - 1].pos2 == x)
							scanline[num_ipoints - 1].packed.corner_right_intersect = TRUE;
					}
					else
					{
						inside_count_stop--;

						corner_left_intersect = TRUE;
					}
				}

				if (order_x[ox].first)
				{
					// Entering a rectangle
					inside_count++;

					if (inside_count == 1)
						corner_left = TRUE;
				}
				else
				{
					// Leaving a rectangle
					inside_count--;

					if (inside_count == 0)
					{
						if (num_ipoints > 0)
							scanline[num_ipoints - 1].packed.corner_right = TRUE;
					}
				}
			}

			BOOL on = (inside_count > 0 && !inside_count_stop);
			if (on)
			{
				if (num_ipoints > 0 && scanline[num_ipoints - 1].pos2 == x)
				{
					// Continue the last line
					scanline[num_ipoints - 1].packed.corner_right = corner_right;
					scanline[num_ipoints - 1].packed.corner_right_intersect = corner_right_intersect;
					scanline[num_ipoints - 1].pos2 = x_stop;
				}
				else
				{
					scanline[num_ipoints].packed.corner_left = corner_left;
					scanline[num_ipoints].packed.corner_left_intersect = corner_left_intersect;
					scanline[num_ipoints].packed.corner_right = corner_right;
					scanline[num_ipoints].packed.corner_right_intersect = corner_right_intersect;
					scanline[num_ipoints].pos1 = x;
					scanline[num_ipoints].pos2 = x_stop;
					num_ipoints++;
				}
			}
		}

		// Draw scanlines
		for (int i = 0; i < num_ipoints; i++)
		{
			int pos = scanline[i].pos1;
			int length = scanline[i].pos2 - scanline[i].pos1;

#ifdef SKIN_OUTLINE_SUPPORT
			if (length > 0 && skin_elm_inside && skin_elm_border_inverse)
			{
				pos += thickness;
				length -= thickness * 2;

				if (x_is_y)
				{
					if (order_y[oy].first) // Left edge
					{
						if (scanline[i].packed.corner_left_intersect)
						{
							pos -= thickness;
							length += thickness;
							skin_elm_border_inverse->DrawSkinPart(vd, OpRect(y, pos - thickness, thickness, thickness), SKINPART_CORNER_BOTTOMLEFT);
						}
						else if (scanline[i].packed.corner_left)
						{
							skin_elm_inside->DrawSkinPart(vd, OpRect(y, pos - thickness, thickness, thickness), SKINPART_CORNER_TOPLEFT);
						}

						if (scanline[i].packed.corner_right_intersect)
						{
							length += thickness;
							skin_elm_border_inverse->DrawSkinPart(vd, OpRect(y, pos + length, thickness, thickness), SKINPART_CORNER_TOPLEFT);
						}
						else if (scanline[i].packed.corner_right)
						{
							skin_elm_inside->DrawSkinPart(vd, OpRect(y, pos + length, thickness, thickness), SKINPART_CORNER_BOTTOMLEFT);
						}

						skin_elm_inside->DrawSkinPart(vd, OpRect(y, pos, thickness, length), SKINPART_TILE_LEFT);
					}
					else // Right edge
					{
						if (scanline[i].packed.corner_left_intersect)
						{
							pos -= thickness;
							length += thickness;
							skin_elm_border_inverse->DrawSkinPart(vd, OpRect(y - thickness, pos - thickness, thickness, thickness), SKINPART_CORNER_BOTTOMRIGHT);
						}
						else if (scanline[i].packed.corner_left)
						{
							skin_elm_inside->DrawSkinPart(vd, OpRect(y - thickness, pos - thickness, thickness, thickness), SKINPART_CORNER_TOPRIGHT);
						}

						if (scanline[i].packed.corner_right_intersect)
						{
							length += thickness;
							skin_elm_border_inverse->DrawSkinPart(vd, OpRect(y - thickness, pos + length, thickness, thickness), SKINPART_CORNER_TOPRIGHT);
						}
						else if (scanline[i].packed.corner_right)
						{
							skin_elm_inside->DrawSkinPart(vd, OpRect(y - thickness, pos + length, thickness, thickness), SKINPART_CORNER_BOTTOMRIGHT);
						}

						skin_elm_inside->DrawSkinPart(vd, OpRect(y - thickness, pos, thickness, length), SKINPART_TILE_RIGHT);
					}
				}
				else
				{
					if (scanline[i].packed.corner_left_intersect)
					{
						pos -= thickness;
						length += thickness;
					}
					if (scanline[i].packed.corner_right_intersect)
					{
						length += thickness;
					}
					if (order_y[oy].first) // Top edge
					{
						skin_elm_inside->DrawSkinPart(vd, OpRect(pos, y, length, thickness), SKINPART_TILE_TOP);
					}
					else // Bottom edge
					{
						skin_elm_inside->DrawSkinPart(vd, OpRect(pos, y - thickness, length, thickness), SKINPART_TILE_BOTTOM);
					}
				}
			}
			else
#endif // SKIN_OUTLINE_SUPPORT
			if (length > 0)
			{
				int c1 = scanline[i].packed.corner_left ? thickness : 0;
				int c2 = scanline[i].packed.corner_right ? thickness : 0;

				// Intersect corners is faced inwards, so use negative thickness
				// Also add 1 px in corner direction because we are one pixel off.
				if (scanline[i].packed.corner_left_intersect)
				{
					c1 = -thickness;
					pos--;
					length++;
				}
				if (scanline[i].packed.corner_right_intersect)
				{
					c2 = -thickness;
					length++;
				}

				if (x_is_y)
				{
					if (order_y[oy].first) // Left edge
						vd->LineOut(y, pos, length, thickness, m_pen_style, m_color, FALSE, TRUE, c1, c2);
					else // Right edge
						vd->LineOut(y - 1, pos, length, thickness, m_pen_style, m_color, FALSE, FALSE, c1, c2);
				}
				else
				{
					if (order_y[oy].first) // Top edge
						vd->LineOut(pos, y, length, thickness, m_pen_style, m_color, TRUE, TRUE, c1, c2);
					else // Bottom edge
						vd->LineOut(pos, y - 1, length, thickness, m_pen_style, m_color, TRUE, FALSE, c1, c2);
				}
			}
		}
		oy = next_oy;
	}
}
