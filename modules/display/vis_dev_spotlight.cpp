/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/vis_dev.h"
#include "modules/layout/layoutprops.h"

#ifdef DISPLAY_SPOTLIGHT

// == VDSpotlightInfo implementation ==========================================================

OP_STATUS VDSpotlightInfo::Paint(VisualDevice *vd, VDSpotlightInfo *exclude_from_fill)
{
	if (rect.IsEmpty())
		return OpStatus::OK;
	if (HasFill())
	{
		vd->SetColor(color_fill);

		if (exclude_from_fill && !exclude_from_fill->rect.IsEmpty())
		{
			BgRegion region_outside, region_inside;
			RETURN_IF_ERROR(region_outside.Set(rect));
			RETURN_IF_ERROR(region_outside.ExcludeRect(exclude_from_fill->rect, FALSE));
			RETURN_IF_ERROR(region_inside.Set(exclude_from_fill->rect));
			RETURN_IF_ERROR(region_inside.ExcludeRect(rect, FALSE));
			int i;
			for(i = 0; i < region_inside.num_rects; i++)
				vd->FillRect(region_inside.rects[i]);
			for(i = 0; i < region_outside.num_rects; i++)
				vd->FillRect(region_outside.rects[i]);
		}
		else
			vd->FillRect(rect);
	}
	if (HasInnerFrame())
	{
		vd->SetColor(color_inner_frame);
		vd->DrawRect(rect);
	}
	if (HasGrid())
	{
		vd->SetColor(color_grid);
		OpRect screen_rect = vd->OffsetToContainerAndScroll(vd->ScaleToScreen(rect.x + vd->GetTranslationX(),
																		rect.y + vd->GetTranslationY(),
																		rect.width, rect.height));
		vd->painter->DrawLine(OpPoint(screen_rect.x, vd->GetOffsetY()), OpPoint(screen_rect.x, vd->GetOffsetY() + vd->WinHeight()), 1);
		vd->painter->DrawLine(OpPoint(screen_rect.x + screen_rect.width - 1, vd->GetOffsetY()), OpPoint(screen_rect.x + screen_rect.width - 1, vd->GetOffsetY() + vd->WinHeight()), 1);
		vd->painter->DrawLine(OpPoint(vd->GetOffsetX(), screen_rect.y), OpPoint(vd->GetOffsetX() + vd->WinWidth(), screen_rect.y), 1);
		vd->painter->DrawLine(OpPoint(vd->GetOffsetX(), screen_rect.y + screen_rect.height - 1), OpPoint(vd->GetOffsetX() + vd->WinWidth(), screen_rect.y + screen_rect.height - 1), 1);
	}
	return OpStatus::OK;
}

// == VDSpotlight implementation ==========================================================

VDSpotlight::VDSpotlight()
	: id(0)
{
	for(int i = 0; i < 4; i++)
	{
		props_margin[i] = 0;
		props_border[i] = 0;
		props_padding[i] = 0;
	}
}

void VDSpotlight::SetProps(const HTMLayoutProperties& props)
{
	props_margin[0] = props.margin_left;
	props_margin[1] = props.margin_top;
	props_margin[2] = props.margin_right;
	props_margin[3] = props.margin_bottom;
	props_border[0] = IS_BORDER_ON(props.border.left) ? props.border.left.width : 0;
	props_border[1] = IS_BORDER_ON(props.border.top) ? props.border.top.width : 0;
	props_border[2] = IS_BORDER_ON(props.border.right) ? props.border.right.width : 0;
	props_border[3] = IS_BORDER_ON(props.border.bottom) ? props.border.bottom.width : 0;
	props_padding[0] = props.padding_left;
	props_padding[1] = props.padding_top;
	props_padding[2] = props.padding_right;
	props_padding[3] = props.padding_bottom;
}

OP_STATUS VDSpotlight::Paint(VisualDevice *vd)
{
	RETURN_IF_ERROR(margin.Paint(vd, &border));
	RETURN_IF_ERROR(border.Paint(vd, &padding));
	RETURN_IF_ERROR(padding.Paint(vd, &content));
	RETURN_IF_ERROR(content.Paint(vd, NULL));
	return OpStatus::OK;
}

OpRect VDSpotlight::GetBoundingRect(VisualDevice *vd)
{
	OpRect bounding_rect;
	if (margin.HasGrid() || border.HasGrid() || padding.HasGrid() || content.HasGrid())
		bounding_rect = OpRect(0, 0, vd->GetDocumentWidth(), vd->GetDocumentHeight());
	else
	{
		bounding_rect = margin.rect;
		bounding_rect.UnionWith(border.rect);
		bounding_rect.UnionWith(padding.rect);
		bounding_rect.UnionWith(content.rect);
	}
	return bounding_rect;
}

void VDSpotlight::UpdateInfo(VisualDevice *vd, VisualDeviceOutline *outline)
{
	OpRect outline_rect = outline->GetBoundingRect().InsetBy(outline->GetPenWidth());
	if (outline->GetPenStyle() != CSS_VALUE_none)
		outline_rect = outline_rect.InsetBy(outline->GetOffset());
	margin.SetRect(OpRect(outline_rect.x - props_margin[0], outline_rect.y - props_margin[1], outline_rect.width + props_margin[0] + props_margin[2], outline_rect.height + props_margin[1] + props_margin[3]));
	border.SetRect(OpRect(outline_rect.x, outline_rect.y, outline_rect.width, outline_rect.height));
	padding.SetRect(OpRect(outline_rect.x + props_border[0], outline_rect.y + props_border[1], outline_rect.width - props_border[0] - props_border[2], outline_rect.height - props_border[1] - props_border[3]));
	content.SetRect(OpRect(outline_rect.x + props_border[0] + props_padding[0], outline_rect.y + props_border[1] + props_padding[1],
					outline_rect.width - props_border[0] - props_border[2] - props_padding[0] - props_padding[2],
					outline_rect.height - props_border[1] - props_border[3] - props_padding[1] - props_padding[3]));

	outline_rect.UnionWith(GetBoundingRect(vd));
	outline->ExtendBoundingRect(outline_rect);
	// FIX: Must make sure the outline is updated even when it's not paint-traversed by layout.
}

// == VisualDevice implementation ==========================================================

OP_STATUS VisualDevice::AddSpotlight(VDSpotlight *spotlight, HTML_Element *element, InsertionMethod insertion, VDSpotlight *target)
{
	OP_STATUS status = OpStatus::OK;

	UpdateAll();

	switch (insertion)
	{
	case IN_FRONT:
		status = spotlights.Add(spotlight);
		break;

	case IN_BACK:
		status = spotlights.Insert(0, spotlight);
		break;

	case ABOVE_TARGET:
	case BELOW_TARGET:
	{
		INT32 target_index = spotlights.Find(target);
		if (target_index == -1)
		{
			status = OpStatus::ERR_NO_SUCH_RESOURCE;
			break;
		}

		if (insertion == ABOVE_TARGET)
			target_index++;

		status = spotlights.Insert(target_index, spotlight);
		break;
	}

	default:
		OP_ASSERT(!"Unhandled insertion method!");
		break;
	}

	if (OpStatus::IsSuccess(status))
		spotlight->SetElement(element);

	return status;
}

void VisualDevice::RemoveSpotlight(HTML_Element *element)
{
	for (UINT32 i = 0; i < spotlights.GetCount(); i++)
		if (spotlights.Get(i)->element == element)
		{
			spotlights.Delete(i);
			break;
		}
}

void VisualDevice::RemoveSpotlight(unsigned int id)
{
	OP_ASSERT(id > 0);
	for (UINT32 i = 0; i < spotlights.GetCount(); i++)
		if (spotlights.Get(i)->id == id)
		{
			spotlights.Delete(i);
			break;
		}
}

VDSpotlight *VisualDevice::GetSpotlight(HTML_Element *element)
{
	for (UINT32 i = 0; i < spotlights.GetCount(); i++)
		if (spotlights.Get(i)->element == element)
			return spotlights.Get(i);

	return NULL;
}

VDSpotlight *VisualDevice::GetSpotlight(unsigned int id)
{
	OP_ASSERT(id > 0);
	for (UINT32 i = 0; i < spotlights.GetCount(); i++)
		if (spotlights.Get(i)->id == id)
			return spotlights.Get(i);

	return NULL;
}

void VisualDevice::UpdateSpotlight(HTML_Element *element)
{
	for (UINT32 i = 0; i < spotlights.GetCount(); i++)
		if (spotlights.Get(i)->element == element)
		{
			Update(spotlights.Get(i)->GetBoundingRect(this));
			break;
		}
}

#endif // DISPLAY_SPOTLIGHT
