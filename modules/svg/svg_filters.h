/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009- Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SVG_FILTER_INTERFACES_H
#define SVG_FILTER_INTERFACES_H

#if defined(SVG_SUPPORT) && defined(SVG_SUPPORT_FILTERS)

class OpPainter;

/**
 * This is an interface for generating filter input image sources: SourceGraphic | SourceAlpha | BackgroundImage | BackgroundAlpha | FillPaint | StrokePaint.
 */
class SVGFilterInputImageGenerator
{
public:
	virtual ~SVGFilterInputImageGenerator() {}

	/**
	 * Render the BackgroundImage, using the given painter. The area is the area corresponding to the
	 * part that is needed. The painter will have an origo setup to be at the x,y-position given in area.
	 * Nothing outside of the given area will be needed.
	 */
	virtual OP_STATUS CreateBackgroundImage(OpPainter* painter, const OpRect& area) = 0;

	/**
	 * Render the SourceGraphic, using the given painter. The area is the area corresponding to the
	 * part that is needed. The painter will have an origo setup to be at the x,y-position given in area.
	 * Nothing outside of the given area will be needed.
	 */
	virtual OP_STATUS CreateSourceGraphic(OpPainter* painter, const OpRect& area) = 0;

	/**
	 * Render the FillPaint/StrokePaint, using the given painter. The area is the area corresponding to the
	 * part that is needed. The painter will have an origo setup to be at the x,y-position given in area.
	 * Nothing outside of the given area will be needed.
	 */
	virtual OP_STATUS CreatePaintSource(OpPainter* painter, const OpRect& area, BOOL fill) = 0;
};

#endif // SVG_SUPPORT && SVG_SUPPORT_FILTERS
#endif // SVG_FILTER_INTERFACES_H
