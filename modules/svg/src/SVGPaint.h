/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_PAINT_H
#define SVG_PAINT_H

#ifdef SVG_SUPPORT

#include "modules/util/str.h"

#include "modules/svg/svg_external_types.h"
#ifdef SVG_DOM
# include "modules/svg/svg_dominterfaces.h"
#endif // SVG_DOM
#include "modules/svg/src/SVGObject.h"

class CSS_decl;

/**
 * The paint values we animate are only RGBCOLOR. Other values needs
 * to be resolved (so that they are converted to RGBCOLORs).
 */
class SVGPaintObject : public SVGObject
{
public:
	SVGPaintObject() : SVGObject(SVGOBJECT_PAINT), m_paint(SVGPaint::UNKNOWN) { }
	SVGPaintObject(SVGPaint::PaintType t) : SVGObject(SVGOBJECT_PAINT), m_paint(t) { }

	virtual SVGObject *Clone() const ;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual BOOL IsEqual(const SVGObject& other) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	SVGPaint& GetPaint() { return m_paint; }

private:
	SVGPaintObject(const SVGPaintObject& X);			// Don't use this
	void operator=(const SVGPaintObject& X);			// Don't use this

	SVGPaint		m_paint;
};

#endif // SVG_SUPPORT
#endif // SVG_PAINT_H
