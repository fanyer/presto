/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * spoon / Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/widget_utils/WidgetThumbnailGenerator.h"

#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpPainter.h"
#include "modules/widgets/OpWidget.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinElement.h"
#include "adjunct/desktop_pi/DesktopOpWindow.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

WidgetThumbnailGenerator::WidgetThumbnailGenerator(OpWidget& widget)
	: m_widget(widget)
	, m_thumbnail_width(0)
	, m_thumbnail_height(0)
	, m_bitmap(0)
	, m_painter(0)
	, m_no_scale(FALSE)
	, m_high_quality(FALSE)
{
}

Image WidgetThumbnailGenerator::GenerateThumbnail(int thumbnail_width, int thumbnail_height, BOOL high_quality)
{
	m_thumbnail_width = thumbnail_width;
	m_thumbnail_height = thumbnail_height;
	m_high_quality = high_quality;

	return GenerateThumbnail();
}

Image WidgetThumbnailGenerator::GenerateThumbnail()
{
	// Fix for bug DSK-329715: we temporarily expand the
	// widget's rect to be the aspect ratio of the thumbnails.
	OpRect old_rect = m_widget.GetRect();
	OpRect new_rect = old_rect;
	bool need_to_reset_rect = false;

	if (new_rect.width * m_thumbnail_height > new_rect.height * m_thumbnail_width)
	{
		need_to_reset_rect = true;
		new_rect.height = new_rect.width * m_thumbnail_height / m_thumbnail_width;
		m_widget.SetRect(new_rect);
	}

	if (OpStatus::IsError(PrepareForPainting()))
	{
		if (need_to_reset_rect)
			m_widget.SetRect(old_rect);
		return Image();
	}

	m_no_scale = FALSE;

	PaintWhiteBackground();

	PaintThumbnail();

	PaintBrowserViewThumbnails(&m_widget);

	CleanupAfterPainting();

	if (need_to_reset_rect)
		m_widget.SetRect(old_rect);

	return m_thumbnail;
}

Image WidgetThumbnailGenerator::GenerateSnapshot()
{
	OpRect rect = m_widget.GetRect(TRUE);

	m_no_scale = TRUE;
	m_thumbnail_width = rect.width;
	m_thumbnail_height = rect.height;

	if (OpStatus::IsError(PrepareForPainting()))
		return Image();

	PaintWhiteBackground();

	PaintSnapshot();

	PaintBrowserViewThumbnails(&m_widget);

	CleanupAfterPainting();

	return m_thumbnail;
}

OP_STATUS WidgetThumbnailGenerator::PrepareForPainting()
{
	RETURN_IF_ERROR(OpBitmap::Create(&m_bitmap, m_thumbnail_width, m_thumbnail_height, FALSE, FALSE, 0, 0, TRUE));
	m_painter = m_bitmap->GetPainter();
	m_thumbnail = imgManager->GetImage(m_bitmap);
	if (m_thumbnail.IsEmpty())     // OOM
	{
		OP_DELETE(m_bitmap);
		m_bitmap = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	if (!m_painter)
		return OpStatus::ERR;

	g_skin_manager->AddTransparentBackgroundListener(this);

	return OpStatus::OK;
}

void WidgetThumbnailGenerator::PaintWhiteBackground()
{
	m_painter->SetColor(0xffffffff); // OpPainter colours are in ABGR_8888 format
	m_painter->FillRect(OpRect(0, 0, m_thumbnail_width, m_thumbnail_height));
}

OP_STATUS WidgetThumbnailGenerator::PaintSnapshot()
{
	return PaintThumbnail();
}

OP_STATUS WidgetThumbnailGenerator::PaintThumbnail()
{
	VisualDevice* vis_dev = m_widget.GetVisualDevice();
	if (!vis_dev)
		return OpStatus::ERR;

	OpRect rect = m_widget.GetRect(TRUE);

	VDStateNoScale old_scale;
		
	if(!m_no_scale)
	{
		old_scale = vis_dev->BeginScaledPainting(OpRect(), GetScale());
	}
	vis_dev->TranslateView(rect.x, rect.y);

	CoreView* view = vis_dev->GetContainerView() ? vis_dev->GetContainerView() : vis_dev->GetView();

	// Doubled width and height to avoid rounding errors
	// when scaling down to thumbnail.
	if(m_no_scale)
	{
		view->Paint(OpRect(0, 0, m_thumbnail_width, m_thumbnail_height), m_painter, 0, 0, TRUE);
	}
	else
	{
		view->Paint(OpRect(0, 0, m_thumbnail_width * 2, m_thumbnail_height * 2), m_painter, 0, 0, TRUE);
	}
	vis_dev->TranslateView(-rect.x, -rect.y);

	if(!m_no_scale)
	{
		vis_dev->EndScaledPainting(old_scale);
	}
	return OpStatus::OK;
}

int WidgetThumbnailGenerator::GetScale() const
{
	OpRect rect = m_widget.GetRect();

	OP_ASSERT(rect.width > 0 && rect.height > 0);
	if (rect.width == 0 || rect.height == 0)
		return 100;

	int scale;
	int scale_width = (100 * m_thumbnail_width) / rect.width;
	int scale_height = (100 * m_thumbnail_height) / rect.height;

	if (scale_width > scale_height)
	{
		scale = scale_width;
		if ((100 * m_thumbnail_width) % rect.width > 0)
			scale++;
	}
	else
	{
		scale = scale_height;
		if ((100 * m_thumbnail_height) % rect.height > 0)
			scale++;
	}

	return scale;
}

void WidgetThumbnailGenerator::PaintBrowserViewThumbnails(OpWidget* widget)
{
	// we need to handle OpBrowserView specially, if present - they're in a different OpWindow altogether
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_BROWSERVIEW && widget->IsVisible())
	{
		PaintBrowserViewThumbnail(static_cast<OpBrowserView *>(widget));
	}

	for (OpWidget* child = widget->GetFirstChild(); child; child = child->GetNextSibling())
	{
		PaintBrowserViewThumbnails(child);
	}
}

void WidgetThumbnailGenerator::PaintBrowserViewThumbnail(OpBrowserView* browser_view)
{
	OpRect root_rect = m_widget.GetRect(TRUE);

	OpRect browser_rect = browser_view->GetRect(TRUE);
	browser_rect.OffsetBy(-root_rect.x, -root_rect.y);
	browser_rect = ScaleRect(browser_rect);

	if (browser_rect.width == 0 || browser_rect.height == 0)
		return;

	Image thumbnail;

	if(m_no_scale)
	{
		thumbnail = browser_view->GetSnapshotImage();
	}
	else
	{
		thumbnail = browser_view->GetThumbnailImage(browser_rect.width, browser_rect.height, m_high_quality);
	}
	OpBitmap *bitmap = thumbnail.GetBitmap(0);
	if(bitmap)
	{
		if (!thumbnail.IsEmpty())
			m_painter->DrawBitmapClipped(bitmap, OpRect(0, 0, thumbnail.Width(), thumbnail.Height()), OpPoint(browser_rect.x, browser_rect.y));

		thumbnail.ReleaseBitmap();
	}
}

OpRect WidgetThumbnailGenerator::ScaleRect(const OpRect& rect)
{
	int scale = GetScale();

	OpRect scaled_rect(rect.x * scale / 100,
					   rect.y * scale / 100,
					   rect.width * scale / 100,
					   rect.height * scale / 100);

	return scaled_rect;
}

void WidgetThumbnailGenerator::CleanupAfterPainting()
{
	g_skin_manager->RemoveTransparentBackgroundListener(this);

	m_bitmap->ReleasePainter();
	m_painter = 0;
}

void WidgetThumbnailGenerator::OnBackgroundCleared(OpPainter *p, const OpRect& clear_rect)
{
#ifdef PERSONA_SKIN_SUPPORT
	OP_ASSERT(p == m_painter);

	if(!g_skin_manager->HasPersonaSkin())
		return;

	OpSkinElement *elm = g_skin_manager->GetPersona()->GetBackgroundImageSkinElement();
	if(!elm)
		return;

	// we need to paint the persona image relative to the top level browser window so we need to
	// get where our widget is in relation to that
	OpRect root_rect;
	OpRect win_rect;
	OpWindow::State state;
	DesktopOpWindow *this_window = static_cast<DesktopOpWindow *>(m_widget.GetParentOpWindow());
	OpWindow *root_window = this_window->GetRootWindow();

	root_window->GetDesktopPlacement(root_rect, state);

	win_rect = m_widget.GetScreenRect();

	OpRect r(0, 0, root_rect.width, root_rect.height);
	
	VisualDevice* vis_dev = m_widget.GetVisualDevice();

	INT32 offset_x = root_rect.x - win_rect.x - vis_dev->GetTranslationX();
	INT32 offset_y = root_rect.y - win_rect.y - vis_dev->GetTranslationY();

	// copy over the translation to the visual device
	vis_dev->Translate(offset_x, offset_y);

	OpSkinElement::DrawArguments args;

	if(!clear_rect.IsEmpty())
	{
		args.clip_rect = &clear_rect;
	}
	elm->Draw(vis_dev, r, 0, args);

	vis_dev->Translate(-offset_x, -offset_y);

#endif // PERSONA_SKIN_SUPPORT
}
