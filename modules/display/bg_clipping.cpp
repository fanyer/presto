/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/vis_dev.h"
#include "modules/logdoc/htm_elm.h"

#ifdef DEBUG_GFX
static int max_region_rect_count = 0;
int display_flushrects = 0;
int avoid_overdraw = 1;
#endif

class BgClipRect : public OpRect, public Link
{
public:
	BgClipRect(const OpRect& rect) : OpRect(rect) {}
};

BOOL IsWorthDelaying(const OpRect &rect)
{
	return rect.width > 20 && rect.height > 20;
}

/*BOOL IsWorthExcluding(const OpRect &rect)
{
	// FIX: If the rect is small enough to flush a big enough background from stack or exclude it and keep hole.
	return TRUE;
}*/

BOOL IsRegionSmall(const BgRegion &region)
{
	// Downshifts with 3 to get 8px resolution and decrease risk of overflow
	unsigned int area = 0;
	for(int j = 0; j < region.num_rects; j++)
		area += (region.rects[j].width >> 3) * (region.rects[j].height >> 3);
	if (area < 3 * 3)
		return TRUE;
	return FALSE;
}

OpRect OpRectClip(const OpRect &this_rect, const OpRect &clip_rect)
{
	OP_ASSERT(clip_rect.width >= 0 && clip_rect.height >= 0);

	OpRect tmp;
	if(!this_rect.Intersecting(clip_rect))
		return tmp;
	int x1 = this_rect.x;
	int x2 = this_rect.x + this_rect.width;
	int y1 = this_rect.y;
	int y2 = this_rect.y + this_rect.height;
	tmp.x = MAX(x1, clip_rect.x);
	tmp.y = MAX(y1, clip_rect.y);
	tmp.width = MIN(x2, clip_rect.x + clip_rect.width) - tmp.x;
	tmp.height = MIN(y2, clip_rect.y + clip_rect.height) - tmp.y;
	return tmp;
}

// == BgRegion =================================================

BgRegion::BgRegion()
	: rects(NULL)
	, num_rects(0)
	, max_rects(0)
{
}

BgRegion::~BgRegion()
{
	Reset();
}

void BgRegion::Reset(BOOL free_mem)
{
	if (free_mem)
	{
		OP_DELETEA(rects);
		rects = NULL;
		max_rects = 0;
	}
	num_rects = 0;
	bounding_rect.Empty();
}

OP_STATUS BgRegion::Set(OpRect rect)
{
	Reset();
	bounding_rect = rect;
	return AddRect(rect);
}

OP_STATUS BgRegion::Set(const BgRegion& rgn)
{
	Reset(TRUE);

	int new_max_rects = rgn.num_rects;
	OpRect *new_rects = OP_NEWA(OpRect, new_max_rects);
	if (!new_rects)
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(new_rects, rgn.rects, sizeof(OpRect) * new_max_rects);

	rects = new_rects;
	num_rects = rgn.num_rects;
	bounding_rect = rgn.bounding_rect;
	max_rects = new_max_rects;
	return OpStatus::OK;
}

OP_STATUS BgRegion::GrowIfNeeded()
{
	if(num_rects == max_rects)
	{
		// FIX: realloc would be better, and *2 or something like that.
		int new_max_rects = (max_rects == 0 ? 1 : max_rects + 8);
		OpRect *new_rects = OP_NEWA(OpRect, new_max_rects);
		if (!new_rects)
			return OpStatus::ERR_NO_MEMORY;

		if(rects)
			op_memcpy(new_rects, rects, sizeof(OpRect) * max_rects);

		OP_DELETEA(rects);
		rects = new_rects;
		max_rects = new_max_rects;
	}
	return OpStatus::OK;
}

OP_STATUS BgRegion::AddRect(const OpRect& rect)
{
	OP_ASSERT(!rect.IsEmpty());
	RETURN_IF_ERROR(GrowIfNeeded());
	rects[num_rects++] = rect;
#ifdef DEBUG_GFX
	if (num_rects > max_region_rect_count)
		max_region_rect_count = num_rects;
#endif
	if (num_rects == 1)
		bounding_rect = rects[0];
	else
		bounding_rect.UnionWith(rect);

#ifdef DEBUG_GFX
#ifdef _DEBUG
	uni_char tmp[300]; // ARRAY OK 2009-08-11 marcusc
	uni_sprintf(tmp, L"BgRegion::AddRect num_rects: %d \n", num_rects);
	OutputDebugString(tmp);
#endif
#endif

	return OpStatus::OK;
}

OP_STATUS BgRegion::AddRectIfNotContained(const OpRect& rect)
{
	for (int i = 0; i < num_rects; i++)
		if (rects[i].Contains(rect))
			return OpStatus::OK;
	return AddRect(rect);
}

OP_STATUS BgRegion::Constrain(unsigned int tile_side)
{
	OP_ASSERT(tile_side);
	OP_ASSERT(num_rects);

	// first pass, count number of rects needed
	int count = 0;
	for (int i = 0; i < num_rects; ++ i)
	{
		const OpRect& r = rects[i];
		const unsigned tiles_x = (r.width  + tile_side-1) / tile_side;
		const unsigned tiles_y = (r.height + tile_side-1) / tile_side;
		count += tiles_x * tiles_y;
	}
	OP_ASSERT(count >= num_rects);

	if (count == num_rects)
		return OpStatus::OK;

	// allocate new rect array
	OpRect* split_rects = OP_NEWA(OpRect, count);
	RETURN_OOM_IF_NULL(split_rects);

	// second pass, split rects
	int j = 0;
	for (int i = 0; i < num_rects; ++i)
	{
		const OpRect& r = rects[i];
		const unsigned tiles_x = (r.width  + tile_side-1) / tile_side;
		const unsigned tiles_y = (r.height + tile_side-1) / tile_side;

		// easier to start from 0 and add r.y to each rect
		int y_pos = 0;
		for (unsigned y = 0; y < tiles_y; ++y)
		{
			const unsigned remaining_h = r.height - y_pos;
			const unsigned tile_h = MIN(tile_side, remaining_h);
			// easier to start from 0 and add r.x to each rect
			int x_pos = 0;

			for (unsigned x = 0; x < tiles_x; ++x)
			{
				const unsigned remaining_w = r.width - x_pos;
				const unsigned tile_w = MIN(tile_side, remaining_w);
				split_rects[j].Set(r.x + x_pos, r.y + y_pos, tile_w, tile_h);
				++ j;
				x_pos += tile_w;
			}
			OP_ASSERT(x_pos == r.width);
			y_pos += tile_h;
		}
		OP_ASSERT(y_pos == r.height);
	}
	OP_ASSERT(j == count);

	OP_DELETEA(rects);
	rects = split_rects;
	num_rects = max_rects = count;

	return OpStatus::OK;
}

void BgRegion::RemoveRect(int index)
{
	OP_ASSERT(index >= 0 && index < num_rects);

	if(index < num_rects - 1)
		for(int i = index; i < num_rects - 1; i++)
			rects[i] = rects[i + 1];
	num_rects--;

	if (num_rects == 0)
		bounding_rect.Empty();
	else if (num_rects == 1)
		bounding_rect = rects[0];
}

void BgRegion::ExtendBoundingRect(const OpRect& rect)
{
	bounding_rect.UnionWith(rect);
}

OP_STATUS BgRegion::IncludeRect(OpRect rect)
{
	OP_ASSERT(!rect.IsEmpty());

	for(int i = 0; i < num_rects; i++)
	{
		if (rect.Contains(rects[i]))
		{
			RemoveRect(i);
			i--; // Redo this index
		}
		else if (rect.Intersecting(rects[i]))
		{
			BgRegion r_split;
			RETURN_IF_ERROR(r_split.ExcludeRectInternal(rect, rects[i]));
			for(int j = 0; j < r_split.num_rects; j++)
			{
				RETURN_IF_ERROR(IncludeRect(r_split.rects[j]));
			}
			return OpStatus::OK;
		}
	}
	return AddRect(rect);
}

OP_STATUS BgRegion::ExcludeRect(OpRect rect, BOOL keep_hole)
{
	if (num_rects > 1 && !bounding_rect.Intersecting(rect))
		return OpStatus::OK; // Optimization

	if (!rect.IsEmpty())
	{
		// Split all intersecting rectangles
		int num_to_split = num_rects;
		for(int i = 0; i < num_to_split; i++)
		{
			if (rects[i].Intersecting(rect))
			{
				RETURN_IF_ERROR(ExcludeRectInternal(rects[i], rect));
				if (keep_hole)
				{
					rects[i] = OpRectClip(rects[i], rect);
				}
				else
				{
					RemoveRect(i);
					num_to_split--;
					i--;
				}
			}
		}
	}

#ifdef _DEBUG
	int k, m, area = 0;
	for(k = 0; k < num_rects; k++)
		for(m = 0; m < num_rects; m++)
			if (k != m && rects[k].Intersecting(rects[m]))
			{
				OP_ASSERT(0);
			}
	for(k = 0; k < num_rects; k++)
	{
		int rect_area = rects[k].width * rects[k].height;
		if (rect_area != 0 && rect_area / rects[k].width == rects[k].height)
			area += rect_area;
	}
	int bounding_area = bounding_rect.width * bounding_rect.height;
	if (bounding_area != 0 && bounding_area / bounding_rect.width == bounding_rect.height)
		OP_ASSERT(area <= bounding_area);
#endif
	return OpStatus::OK;
}

OP_STATUS BgRegion::ExcludeRectInternal(OpRect rect, OpRect remove)
{
	OP_ASSERT(rect.Intersecting(remove));

	remove = OpRectClip(remove, rect);

	// Top
	if (remove.y > rect.y)
		RETURN_IF_ERROR(AddRect(OpRect(rect.x, rect.y, rect.width, remove.y - rect.y)));

	// Left
	if (remove.x > rect.x)
		RETURN_IF_ERROR(AddRect(OpRect(rect.x, remove.y, remove.x - rect.x, remove.height)));

	// Right
	if (remove.x + remove.width < rect.x + rect.width)
		RETURN_IF_ERROR(AddRect(OpRect(remove.x + remove.width, remove.y, rect.x + rect.width - (remove.x + remove.width), remove.height)));

	// Bottom
	if (remove.y + remove.height < rect.y + rect.height)
		RETURN_IF_ERROR(AddRect(OpRect(rect.x, remove.y + remove.height, rect.width, rect.y + rect.height - (remove.y + remove.height))));

	return OpStatus::OK;
}

void BgRegion::CoalesceRects()
{
	for(int i = 0; i < num_rects; i++)
	{
		for(int j = 0; j < num_rects; j++)
		{
			if (i == j)
				continue;
			if (i > num_rects - 1)
				break;
			if (// Vertical
				(rects[i].x == rects[j].x && rects[i].width == rects[j].width &&
				((rects[i].y + rects[i].height == rects[j].y) || (rects[j].y + rects[j].height == rects[i].y)))
				|| // Horizontal
				(rects[i].y == rects[j].y && rects[i].height == rects[j].height &&
				((rects[i].x + rects[i].width == rects[j].x) || (rects[j].x + rects[j].width == rects[i].x)))
				)
			{
				rects[i].UnionWith(rects[j]);
				RemoveRect(j);
				j--;
			}
		}
	}
}

BOOL BgRegion::Equals(BgRegion& other)
{
	if (num_rects != other.num_rects)
		return FALSE;
	for(int i = 0; i < num_rects; i++)
	{
		if (!rects[i].Equals(other.rects[i]))
			return FALSE;
	}
	return TRUE;
}

// == BgClipStack =================================================

BgClipStack::BgClipStack()
	: num(0)
{
	op_memset(info, 0, sizeof(BgInfo *) * MAX_BG_INFO);
#ifdef DEBUG_GFX
	flushed_color_pixels = 0;
	total_color_pixels = 0;
	flushed_img_pixels = 0;
	total_img_pixels = 0;
#endif
}

BgClipStack::~BgClipStack()
{
	while (BgClipRect *cr = (BgClipRect *) cliplist.Last())
	{
		cr->Out();
		OP_DELETE(cr);
	}
	for(int i = 0; i < MAX_BG_INFO; i++)
	{
		if (info[i])
		{
			OP_DELETE(info[i]);
			info[i] = NULL;
		}
	}
}

OP_STATUS BgClipStack::Begin(VisualDevice *vd)
{
	for(int i = 0; i < MAX_BG_INFO; i++)
	{
		if (!info[i])
			info[i] = OP_NEW(BgInfo, ());
		if (!info[i])
			return OpStatus::ERR_NO_MEMORY;
	}

#ifdef DEBUG_GFX
	flushed_color_pixels = 0;
	total_color_pixels = 0;
	flushed_img_pixels = 0;
	total_img_pixels = 0;
#endif

	return OpStatus::OK;
}

void BgClipStack::End(VisualDevice *vd)
{
	FlushAll(vd);

#ifdef DEBUG_GFX
#ifdef _DEBUG
	double color_ratio = total_color_pixels == 0 ? 1 : (double)flushed_color_pixels / (double)total_color_pixels;
	double img_ratio = total_img_pixels == 0 ? 1 : (double)flushed_img_pixels / (double)total_img_pixels;
	uni_char buf[200]; // ARRAY OK 2009-01-27 emil
	uni_sprintf(buf, L"Avoid overdraw ratio: color %d%%  img %d%%\n", (int)(color_ratio * 100.0), (int)(img_ratio * 100.0));
	OutputDebugString(buf);
	OP_ASSERT(color_ratio <= 1 && img_ratio <= 1);
#endif
#endif
}

OP_STATUS BgClipStack::PushClipping(OpRect rect)
{
	if (cliplist.Last())
	{
		BgClipRect *cr = (BgClipRect *) cliplist.Last();
		rect = OpRectClip(rect, *cr);
	}
	BgClipRect *cr = OP_NEW(BgClipRect, (rect));
	if (!cr)
		return OpStatus::ERR_NO_MEMORY;
	cr->Into(&cliplist);
	return OpStatus::OK;
}

void BgClipStack::PopClipping()
{
	BgClipRect *cr = (BgClipRect *) cliplist.Last();
	if (cr)
	{
		cr->Out();
		OP_DELETE(cr);
	}
}

BOOL BgClipStack::GetClipping(OpRect &clip_rect)
{
	BgClipRect *cr = (BgClipRect *) cliplist.Last();
	if (cr)
	{
		clip_rect = *cr;
		return TRUE;
	}
	return FALSE;
}

void BgClipStack::RemoveBg(int index)
{
	// Unreference the Image object.
	if (info[index]->type == BgInfo::IMAGE)
		info[index]->img = Image();

	for(int j = index; j < num - 1; j++)
	{
		BgInfo *tmp = info[j + 1];
		info[j + 1] = info[j];
		info[j] = tmp;
	}
	num--;
}

void BgClipStack::CoverBg(VisualDevice *vd, const OpRect& crect, BOOL keep_hole)
{
	OpRect rect = crect;

	// Clip to curent cliprect
	BgClipRect *cr = (BgClipRect *) cliplist.Last();
	if (cr)
		rect = OpRectClip(rect, *cr);

	if (rect.IsEmpty())
		return;

	for(int i = 0; i < num; i++)
	{
		OP_STATUS status = info[i]->region.ExcludeRect(rect, keep_hole);
		if (OpStatus::IsError(status))
		{
			FlushAll(vd);
			return;
		}
		// If Bg has become small or has too many rectangles, we'll flush it.
		// Too many rectangles results in expensive region calculations.
		if (IsRegionSmall(info[i]->region) || info[i]->region.num_rects > 10)
		{
			// We should not flush anything outside the current clip rectangle. Then those areas are lost.
			if (!cr || cr->Contains(info[i]->rect))
			{
				FlushBg(vd, i);
				i--;
			}
		}
	}
}

void BgClipStack::FlushBg(VisualDevice *vd, OpRect rect)
{
	// Clip to curent cliprect
	BgClipRect *cr = (BgClipRect *) cliplist.Last();
	if (cr)
		rect = OpRectClip(rect, *cr);

	int i;
	int count = 0;
	for(i = 0; i < num; i++)
	{
		if (info[i]->region.bounding_rect.Intersecting(rect)) // Might intersect
		{
			for(int j = 0; j < info[i]->region.num_rects; j++)
			{
				if (rect.Intersecting(info[i]->region.rects[j]))
				{
					BgInfo *bg = info[i];

// FIX:				if (bg->region.rects[j].width * bg->region.rects[j].height > (bg->region.bounding_rect.width * bg->region.bounding_rect.height) * 0.7)
//						int stop = 0;

					vd->PaintBg(bg->region.rects[j], bg, 0);
					bg->region.RemoveRect(j);
					j--;
					count++;
				}
			}
		}
	}

	// Remove empty regions
	for(i = 0; i < num; i++)
	{
		if (info[i]->region.num_rects == 0)
		{
			RemoveBg(i);
			i--;
		}
	}
}

void BgClipStack::FlushAll(VisualDevice *vd)
{
	while (num)
		FlushLast(vd);
}

void BgClipStack::FlushLast(VisualDevice *vd)
{
	FlushBg(vd, num - 1);
}

void BgClipStack::FlushBg(VisualDevice *vd, int index)
{
	OP_ASSERT(index >= 0 && index < num);
	BgInfo *bg = info[index];

	for(int i = 0; i < bg->region.num_rects; i++)
	{
		const OpRect &r = bg->region.rects[i];
		// If this trig, we flush background that will not be visible
		// due to the current clipping! We must avoid that!
#ifdef CSS_TRANSFORMS
		// ...unless we have a transform, and cliprects are
		// in different coordinate spaces
		OP_ASSERT(!cliplist.Last() || ((BgClipRect *) cliplist.Last())->Contains(r) || vd->HasTransform());
#else
		OP_ASSERT(!cliplist.Last() || ((BgClipRect *) cliplist.Last())->Contains(r));
#endif // CSS_TRANSFORMS
		vd->PaintBg(r, bg, 1);
	}

	RemoveBg(index);
}

void BgClipStack::FlushBg(HTML_Element* element, VisualDevice *vd)
{
	int i;
	for (i = 0; i < num; i++)
	{
		if (info[i]->element == element)
			break;
		if (info[i]->element->Type() == HE_BODY && element->Type() == HE_DOC_ROOT)
			// Sometimes the HE_DOC_ROOT is added as a bodybackground (special case)
			break;
		if (element->IsAncestorOf(info[i]->element))
			break;
	}
	while (i < num)
	{
		FlushLast(vd);
	}
}

BgInfo *BgClipStack::Add(VisualDevice *vd)
{
	if (num >= MAX_BG_INFO - 1)
// FIX: Is the last one the best to flush? Maby the first one is better!?!
//      Maby even check which one to flush? Based on area or intersecting with the current coverarea.
		FlushLast(vd);
	num++;
	return info[num - 1];
}

void BgClipStack::AddBg(HTML_Element* element, VisualDevice *vd, const COLORREF &bg_color, OpRect rect)
{
	// Clip to current cliprect
#ifdef CSS_TRANSFORMS
	// ... but not if a transform is set, because then the clipping
	// won't be correct
	if (!vd->HasTransform())
#endif // CSS_TRANSFORMS
	{
		BgClipRect *cr = (BgClipRect *) cliplist.Last();
		if (cr)
			rect = OpRectClip(rect, *cr);
	}

	if (rect.IsEmpty())
		return;

	BgInfo *last = Add(vd);
	last->element = element;
	last->type = BgInfo::COLOR;
	last->bg_color = bg_color;
	last->rect = rect;
	OP_STATUS status = last->region.Set(rect);

#ifdef DEBUG_GFX
	total_color_pixels += rect.width * rect.height;
	if (!avoid_overdraw)
	{
		FlushAll(vd);
		return;
	}
#endif

	if (!IsWorthDelaying(rect) || OpStatus::IsError(status))
		FlushLast(vd);
}

void BgClipStack::AddBg(HTML_Element* element, VisualDevice *vd, Image& img, OpRect rect, OpPoint offset, ImageListener* image_listener, int imgscale_x, int imgscale_y, CSSValue image_rendering)
{
	// Clip to current cliprect
#ifdef CSS_TRANSFORMS
	// ... but not if a transform is set, because then the clipping
	// won't be correct
	if (!vd->HasTransform())
#endif // CSS_TRANSFORMS
	{
		BgClipRect *cr = (BgClipRect *) cliplist.Last();
		if (cr)
		{
			OpRect oldrect = rect;
			rect = OpRectClip(rect, *cr);
			offset.x += rect.x - oldrect.x;
			offset.y += rect.y - oldrect.y;
		}
	}

	if (rect.IsEmpty())
		return;

	BgInfo *last = Add(vd);
	last->element = element;
	last->type = BgInfo::IMAGE;
	last->img = img;
	last->rect = rect;
	OP_STATUS status = last->region.Set(rect);

	last->offset = offset;
	last->image_listener = image_listener;
	last->imgscale_x = imgscale_x;
	last->imgscale_y = imgscale_y;

	last->image_rendering = image_rendering;

#ifdef DEBUG_GFX
	total_img_pixels += rect.width * rect.height;
	if (!avoid_overdraw)
	{
		FlushAll(vd);
		return;
	}
#endif

	if (!IsWorthDelaying(rect) || OpStatus::IsError(status))
		FlushLast(vd);
}

#ifdef CSS_GRADIENT_SUPPORT
void BgClipStack::AddBg(HTML_Element* element, VisualDevice *vd, COLORREF current_color, const CSS_Gradient &gradient, OpRect rect, const OpRect &gradient_rect, const OpRect &repeat_space, CSSValue repeat_x, CSSValue repeat_y)
{
	// Clip to current cliprect
#ifdef CSS_TRANSFORMS
	// ... but not if a transform is set, because then the clipping
	// won't be correct
	if (!vd->HasTransform())
#endif // CSS_TRANSFORMS
	{
		BgClipRect *cr = (BgClipRect *) cliplist.Last();
		if (cr)
			rect = OpRectClip(rect, *cr);
	}

	if (rect.IsEmpty())
		return;

	BgInfo *last = Add(vd);
	last->element = element;
	last->type = BgInfo::GRADIENT;
	// This pointer is guaranteed to live past painting, provided that all functions in the call stack
	// from VerticalBox::PaintBgAndBorder and up pass BgImages or CSS_Gradient arguments by reference.
	last->gradient = &gradient;

	last->current_color = current_color;
	last->rect = rect;
	last->img_rect = gradient_rect;
	last->repeat_x = repeat_x;
	last->repeat_y = repeat_y;
	last->repeat_space = repeat_space;
	OP_STATUS region_status = last->region.Set(rect);

#ifdef DEBUG_GFX
	total_color_pixels += rect.width * rect.height;
	if (!avoid_overdraw)
	{
		FlushAll(vd);
		return;
	}
#endif

	if (!IsWorthDelaying(rect)
		|| OpStatus::IsError(region_status))
		FlushLast(vd);
}
#endif // CSS_GRADIENT_SUPPORT
