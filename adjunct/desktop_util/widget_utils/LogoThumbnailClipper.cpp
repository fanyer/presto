/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
*
* Copyright (C) 2009 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.
* It may not be distributed under any circumstances.
*
* @author Authors: Haraldur Karlsson (haralkar), Arjan van Leeuwen (arjanl), Petter Nilsen (pettern)
*
*/

#include "core/pch.h"
#include "LogoThumbnailClipper.h"

void LogoThumbnailClipper::SetOriginalSize(int width, int height)
{
	m_originalRect.width=width;
	m_originalRect.height=height;
}

void LogoThumbnailClipper::SetClippedSize(int width, int height)
{
	m_clippedRect.width=width;
	m_clippedRect.height=height;
}

void LogoThumbnailClipper::SetLogoStart(const OpPoint &logoStart)
{
	m_logoStart = logoStart;
}

OpRect LogoThumbnailClipper::ClipRect()
{
	OpRect outRect = m_clippedRect;

	outRect.x = min(m_logoStart.x, m_originalRect.width - m_clippedRect.width);
	outRect.y = min(m_logoStart.y, m_originalRect.height - m_clippedRect.height);

	outRect.x = max(0, outRect.x);
	outRect.y = max(0, outRect.y);

	return outRect;
}

/*****************************************************************************************
*
* SmartCropScaling - smarter scaling/cropping combination
*
*****************************************************************************************/

void SmartCropScaling::SetOriginalSize(INT32 width, INT32 height)
{
	m_width = width;
	m_height = height;
}

void SmartCropScaling::GetCroppedRects(OpRect& src_rect, OpRect& dst_rect)
{
	// Set the target rectangle tall enough for the entire image
	float width_ratio = (float)m_width / (float)src_rect.width;
	float height_ratio = (float)m_height / (float)src_rect.height;
	if (width_ratio > height_ratio)
	{
		// we don't crop on top/bottom at all, but scale the whole image to fit
		INT32 width_of_src_to_use = src_rect.width * height_ratio;
		INT32 offset_x = (m_width - width_of_src_to_use) / 2;

		offset_x = op_abs(offset_x);

		dst_rect.width = width_of_src_to_use;
		dst_rect.x += offset_x;
	}
	else
	{
		INT32 img_height = src_rect.height;
		INT32 width_of_src_to_use = src_rect.width  - ((((float)src_rect.width * height_ratio) - (float)m_width) /  height_ratio);

		// find out how much to remove on the left or right :
		INT32 width_to_crop = (src_rect.width - width_of_src_to_use) / 2;

		width_to_crop = op_abs(width_to_crop);

		src_rect.Set(width_to_crop, 0 , width_of_src_to_use, img_height);
	}

	// using src_rect with 0 width or height will crash
	if (src_rect.height < 1)
		src_rect.height = 1;
	if (src_rect.width < 1 )
		src_rect.width = 1;

}
