/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * spoon / Arjan van Leeuwen (arjanl)
 */

#ifndef WIDGET_THUMBNAIL_GENERATOR_H
#define WIDGET_THUMBNAIL_GENERATOR_H

#include "modules/img/image.h"
#include "modules/skin/OpSkinManager.h"

class OpWidget;
class OpBrowserView;

class WidgetThumbnailGenerator : public TransparentBackgroundListener
{
public:
	/** Constructor
	  * @param widget Widget to generate thumbnails for
	  */
	WidgetThumbnailGenerator(OpWidget& widget);

	/** Generate a thumbnail of the current state of the widget
	  * @param thumbnail_width Desired width of the thumbnail
	  * @param thumbnail_height Desired height of the thumbnail
	  * @param high_quality TRUE for higher quality thumbnails - default is FALSE
	  */
	Image GenerateThumbnail(int thumbnail_width, int thumbnail_height, BOOL high_quality = FALSE);

	/** Generate a screen shot of the current state of the widget
	*/
	Image GenerateSnapshot();

	/** @return The scale (in percent) that this thumbnail is being painted at
	  */
	int GetScale() const;

	int GetThumbnailWidth() const { return m_thumbnail_width; }

	int GetThumbnailHeight() const { return m_thumbnail_height; }

	// TransparentBackgroundListener - needed to any persona image we might use, usually for speed dial only
	virtual void OnBackgroundCleared(OpPainter *painter, const OpRect& clear_rect);

private:
	Image GenerateThumbnail();
	OP_STATUS PrepareForPainting();
	void PaintWhiteBackground();
	OP_STATUS PaintThumbnail();
	OP_STATUS PaintSnapshot();
	void PaintBrowserViewThumbnails(OpWidget* widget);
	void PaintBrowserViewThumbnail(OpBrowserView* browser_view);
	OpRect ScaleRect(const OpRect& rect);
	void CleanupAfterPainting();

	OpWidget& m_widget;
	int m_thumbnail_width;
	int m_thumbnail_height;
	OpBitmap* m_bitmap;
	OpPainter* m_painter;
	Image m_thumbnail;
	BOOL	m_no_scale;			// TRUE if no scaling is to be performed
	BOOL	m_high_quality;		// TRUE for higher quality thumbnails
};

#endif // WIDGET_THUMBNAIL_GENERATOR_H
