/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/vis_dev.h"

#ifdef _PLUGIN_SUPPORT_

OP_STATUS CoreViewContainer::AddPluginArea(const OpRect& rect, ClipViewEntry* entry)
{
	if (rect.IsEmpty())
		return OpStatus::OK;

	PluginAreaIntersection *info = OP_NEW(PluginAreaIntersection, ());
	if (!info)
		return OpStatus::ERR_NO_MEMORY;

	info->entry = entry;
	info->rect = rect;
	OP_STATUS status = info->visible_region.Set(info->rect);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(info);
		return status;
	}
	info->Into(&plugin_intersections);
	return OpStatus::OK;
}

void CoreViewContainer::UpdateAndDeleteAllPluginAreas(const OpRect &rect)
{
	while (plugin_intersections.First())
	{
		PluginAreaIntersection *info = (PluginAreaIntersection*) plugin_intersections.First();
		UpdatePluginArea(rect, info);
		info->Out();
		OP_DELETE(info);
	}
}

void CoreViewContainer::UpdatePluginArea(const OpRect &rect, PluginAreaIntersection* info)
{
	// Make info relative to the plugin
	int plugin_x = info->rect.x;
	int plugin_y = info->rect.y;
	for(int i = 0; i < info->visible_region.num_rects; i++)
	{
		info->visible_region.rects[i].x -= plugin_x;
		info->visible_region.rects[i].y -= plugin_y;
	}
	OpRect clip_rect = rect;
	clip_rect.x -= plugin_x;
	clip_rect.y -= plugin_y;
	info->rect.x = 0;
	info->rect.y = 0;
	OpRect plugin_rect = info->rect;

	info->visible_region.CoalesceRects();

	// Calculate the visible area of the plugin and set it on its view.
	// 
	// We may have painted only a part of the area which intersects with the plugin.
	// Theirfore we can only update the plugins visibility region with changes done to that part.
	// This is done in the following way:
	// 1. Exclude the clip_rect from the "work region"* to assume full visibility on that painted area.
	// 2. Include rectangles we know are visible, also only for the painted area
	//
	// This will cause the work_region to be fragmented and change a lot when we get multiple OnPaint on fractions of the area (that may
	// even happen during the same screen validation). To get rid of fragmentation so we can see of the region differ from the "final region"*,
	// we will do this:
	// 1. Create a inverted region (Represent the covered area instead of the non-covered)
	// 2. Recreate "work_region" from the inverted region to again represent the non-covered area. Now it will be as optimal as it can be.
	// Note: There's no guarantee that the actual representation of the region will be the same as the final region, even if the area it represents
	// is the exact same (due to order of rectangles). This will sometimes cause updates that is not needed.
	//
	// * "work region"  Region used to calculate the new visible region and compared to final region.
	// * "final region" Region that is currently on the plugin. Compared to the work region to detect changes.

	BgRegion &vis_region = info->visible_region;
	BgRegion &final_region = info->entry->GetClipView()->m_visible_region;

	// Create the work region from the current final_region
	BgRegion work_region;
	RETURN_VOID_IF_ERROR(work_region.Set(final_region));

	// Update the work_region with the changes we did in this paint pass.
	// Feed it with scaled coordinates since final_region is also scaled.
	RETURN_VOID_IF_ERROR(work_region.ExcludeRect(clip_rect, FALSE));
	for(int i = 0; i < vis_region.num_rects; i++)
	{
		OpRect r = vis_region.rects[i];
		r.IntersectWith(clip_rect);
		if (!r.IsEmpty())
		{
			RETURN_VOID_IF_ERROR(work_region.IncludeRect(r));
		}
	}
	work_region.CoalesceRects();

	// Invert so we get a "overlap region"
	BgRegion inverted_region;
	RETURN_VOID_IF_ERROR(inverted_region.Set(plugin_rect));
	for(int i = 0; i < work_region.num_rects; i++)
	{
		RETURN_VOID_IF_ERROR(inverted_region.ExcludeRect(work_region.rects[i], FALSE));
	}

	// Invert back from the overlap region so work_region is a optimal visibility region.
	RETURN_VOID_IF_ERROR(work_region.Set(plugin_rect));
	for(int i = 0; i < inverted_region.num_rects; i++)
	{
		RETURN_VOID_IF_ERROR(work_region.ExcludeRect(inverted_region.rects[i], FALSE));
	}

	// The plugin may be partly outside of its owning CoreView (partly scrolled out of view)
	// So get the cliprect from the ClipView and exclude it from the work region.
	OpRect view_clip_rect = info->entry->GetClipView()->GetClipRect();
	if (view_clip_rect.IsEmpty())
		work_region.Reset();
	else
	{
		OpRect top(0, 0, plugin_rect.width, view_clip_rect.y);
		OpRect bottom(0, view_clip_rect.y + view_clip_rect.height, plugin_rect.width, plugin_rect.height - (view_clip_rect.y + view_clip_rect.height));
		OpRect left(0, 0, view_clip_rect.x, plugin_rect.height);
		OpRect right(view_clip_rect.x + view_clip_rect.width, 0, plugin_rect.width - (view_clip_rect.x + view_clip_rect.width), plugin_rect.height);
		if (!top.IsEmpty())
			RETURN_VOID_IF_ERROR(work_region.ExcludeRect(top, FALSE));
		if (!bottom.IsEmpty())
			RETURN_VOID_IF_ERROR(work_region.ExcludeRect(bottom, FALSE));
		if (!left.IsEmpty())
			RETURN_VOID_IF_ERROR(work_region.ExcludeRect(left, FALSE));
		if (!right.IsEmpty())
			RETURN_VOID_IF_ERROR(work_region.ExcludeRect(right, FALSE));
	}

	// If the work_region differs, update the clipviews OpView and final_region to cut the plugin according to the new visibility.
	if (!work_region.Equals(final_region) ||
		// If visibility regions are empty, the overlap region might still differ (f.ex if the zoom has changed, the viewsize will have changed)
		// so always enter then. SetCustomOverlapRegion will bail out if it is equal anyway.
		(work_region.num_rects == 0 && final_region.num_rects == 0))
	{
		// Invert to overlap-region and set it
		BgRegion rgn;
		RETURN_VOID_IF_ERROR(rgn.Set(plugin_rect));
		for(int i = 0; i < work_region.num_rects; i++)
		{
			RETURN_VOID_IF_ERROR(rgn.ExcludeRect(work_region.rects[i], FALSE));
		}
		RETURN_VOID_IF_ERROR(info->entry->GetOpView()->SetCustomOverlapRegion(rgn.rects, rgn.num_rects));

		// The work region is now in use, so update final_region to it
		RETURN_VOID_IF_ERROR(final_region.Set(work_region));
	}
}

void CoreViewContainer::CoverPluginArea(const OpRect& rect)
{
	if (rect.IsEmpty())
		return;

	// Exclude the rect from any intersecting plugins visible region.
	PluginAreaIntersection* info = (PluginAreaIntersection*) plugin_intersections.First();
	while (info)
	{
		if (info->visible_region.bounding_rect.Intersecting(rect))
		{
			if (OpStatus::IsError(info->visible_region.ExcludeRect(rect, FALSE)))
				return;
		}
		info = (PluginAreaIntersection*) info->Suc();
	}
}

void CoreViewContainer::RemoveAllPluginAreas()
{
	while (plugin_intersections.First())
	{
		PluginAreaIntersection* info = (PluginAreaIntersection*) plugin_intersections.First();
		info->Out();
		OP_DELETE(info);
	}
}

void CoreViewContainer::RemovePluginArea(ClipViewEntry* entry)
{
	PluginAreaIntersection* info = (PluginAreaIntersection*) plugin_intersections.First();
	while (info)
	{
		PluginAreaIntersection* next = static_cast<PluginAreaIntersection*>(info->Suc());

		if (info->entry == entry)
		{
			info->Out();
			OP_DELETE(info);
		}

		info = next;
	}
}

BOOL CoreViewContainer::HasPluginArea()
{
	return plugin_intersections.First() ? TRUE : FALSE;
}

#endif // _PLUGIN_SUPPORT_
