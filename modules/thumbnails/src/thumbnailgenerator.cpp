/** -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Haraldur Karlsson (haralkar)
 * @author Co-owner: Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef SUPPORT_GENERATE_THUMBNAILS

#include "modules/thumbnails/thumbnailgenerator.h"
#include "modules/thumbnails/src/thumbnail.h"


ThumbnailGenerator::ThumbnailGenerator(Window* window)
  : m_window(window)
  , m_target_height(THUMBNAIL_HEIGHT)
  , m_target_width(THUMBNAIL_WIDTH)
  , m_high_quality(TRUE)
{
}

#ifdef THUMBNAILS_LOGO_FINDER

OpRect ThumbnailGenerator::FindLogoInOriginalWindow()
{
	return OpThumbnail::FindArea(m_window, m_target_width, m_target_height, TRUE, OpThumbnail::FindLogo);
}

OpPoint ThumbnailGenerator::FindLogo()
{
	if (m_area.width == 0 || m_area.height ==0)
	{
		return OpPoint();
	}
	OpRect logoRect = FindLogoInOriginalWindow();
	logoRect.OffsetBy(-m_area.x, -m_area.y);

	OpPoint logoStart;
	logoStart.x = (logoRect.x * m_target_width) / m_area.width;
	logoStart.y = (logoRect.y * m_target_height) / m_area.height;

	return logoStart;
}
#endif

Image ThumbnailGenerator::GenerateThumbnail()
{
	OP_NEW_DBG("ThumbnailGenerator::GenerateThumbnail()", "thumbnail");
	m_area = OpThumbnail::FindDefaultArea(m_window, m_target_width, m_target_height, TRUE);
	OpBitmap* bitmap = OpThumbnail::CreateThumbnail(m_window, m_target_width, m_target_height, m_area, m_high_quality);

	if (bitmap)
	{
		OP_DBG(("returning image of created bitmap"));
		return imgManager->GetImage(bitmap);
	}
	else
	{
		OP_DBG(("return an empty image"));
		return Image();
	}
}

Image ThumbnailGenerator::GenerateSnapshot()
{
	OP_NEW_DBG("ThumbnailGenerator::GenerateThumbnail()", "thumbnail");
	OpBitmap* bitmap = OpThumbnail::CreateSnapshot(m_window);

	if (bitmap)
	{
		OP_DBG(("returning image of created bitmap"));
		return imgManager->GetImage(bitmap);
	}
	else
	{
		OP_DBG(("return an empty image"));
		return Image();
	}
}

#endif // SUPPORT_GENERATE_THUMBNAILS
