/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpImageBackground.h"

#include "modules/display/vis_dev.h"


void OpImageBackground::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (m_image.IsEmpty())
		return OpWidget::OnPaint(widget_painter, paint_rect);

	const OpRect bounds_rect = GetBounds();
	const OpRect content_rect(bounds_rect);
	OpRect src_rect(0, 0, m_image.Width(), m_image.Height());

	switch (m_layout)
	{
		case BEST_FIT:
		{
			// Best fit aka Crop aka proportional stretch
			float width_ratio = (float)content_rect.width / (float)src_rect.width;
			float height_ratio = (float)content_rect.height / (float)src_rect.height;
			if (width_ratio > height_ratio)
			{
				INT32 height_of_src_to_use = src_rect.height - ((((float)src_rect.height * width_ratio) - (float)content_rect.height) /  width_ratio);
				// find out how much to remove on the top or bottom:
				INT32 height_to_crop = (src_rect.height - height_of_src_to_use) / 2;
				if (height_to_crop<0)
					height_to_crop = - height_to_crop;
				src_rect.Set(0, height_to_crop, m_image.Width(), height_of_src_to_use);
			}
			else
			{
				INT32 width_of_src_to_use = src_rect.width  - ((((float)src_rect.width * height_ratio) - (float)content_rect.width) /  height_ratio);
				// find out how much to remove on the left or right :
				INT32 width_to_crop = (src_rect.width - width_of_src_to_use) / 2;
				if (width_to_crop<0)
					width_to_crop = - width_to_crop;
				src_rect.Set(width_to_crop, 0 , width_of_src_to_use, m_image.Height());
			}

			// using src_rect with 0 width or height will crash
			if (src_rect.height < 1)
				src_rect.height = 1;
			if (src_rect.width < 1 )
				src_rect.width = 1;

			GetVisualDevice()->ImageOut(m_image, src_rect, content_rect);

			break;
		}

		case STRETCH:
			GetVisualDevice()->ImageOut(m_image, src_rect, content_rect);
			break;

		case TILE:
			GetVisualDevice()->ImageOutTiled(m_image, content_rect, OpPoint(0, 0), null_image_listener, 100, 100, 0, 0, m_image.Width(), m_image.Height());
			break;

		case CENTER:
		{
			OpRect center_rect;
			center_rect.x = (bounds_rect.width - (INT32)m_image.Width()) / 2;
			center_rect.y = (bounds_rect.height - (INT32)m_image.Height()) / 2;
			center_rect.width = m_image.Width();
			center_rect.height = m_image.Height();
			if(center_rect.x < 0)
			{
				src_rect.x -= center_rect.x;
				center_rect.x = 0;
			}
			if(center_rect.y < 0)
			{
				src_rect.y -= center_rect.y;
				center_rect.y = 0;
			}
			GetVisualDevice()->ImageOut(m_image, src_rect, center_rect);

			break;
		}

	default:
		OP_ASSERT(!"Unknown layout type");
	}
}
