/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_PAINT_IMPL_H
#define SVG_DOM_PAINT_IMPL_H

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGPaint.h"

class SVGDOMCSSPrimitiveValuePaintImpl : public SVGDOMCSSPrimitiveValue
{
public:
	enum PaintChannel
	{
		PAINTCHANNEL_RED,
		PAINTCHANNEL_GREEN,
		PAINTCHANNEL_BLUE
	};

	SVGDOMCSSPrimitiveValuePaintImpl(SVGPaintObject* paint, PaintChannel channel);
	virtual ~SVGDOMCSSPrimitiveValuePaintImpl();

	virtual const char* GetDOMName();

	virtual OP_BOOLEAN	SetFloatValue(UnitType unit_type, double float_value);
	virtual OP_BOOLEAN	GetFloatValue(UnitType unit_type, double& float_value);
	virtual UnitType	GetPrimitiveType() { return SVG_DOM_CSS_NUMBER; }
	virtual SVGObject*	GetSVGObject() { return m_paint; }

	// dummy implementations of CSSValue
	virtual OP_STATUS GetCSSText(TempBuffer *buffer) { OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS SetCSSText(uni_char* buffer)  { OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual SVGDOMCSSValue::UnitType GetCSSValueType() { return SVG_DOM_CSS_PRIMITIVE_VALUE; }

private:
	SVGPaintObject*			m_paint;
	PaintChannel			m_channel;
};

class SVGDOMCSSRGBColorPaintImpl : public SVGDOMCSSRGBColor
{
public:
	SVGDOMCSSRGBColorPaintImpl(SVGPaintObject* paint);
	virtual ~SVGDOMCSSRGBColorPaintImpl();

	virtual const char* GetDOMName();

	virtual SVGDOMCSSPrimitiveValue* GetRed();
	virtual SVGDOMCSSPrimitiveValue* GetGreen();
	virtual SVGDOMCSSPrimitiveValue* GetBlue();
	virtual SVGObject*				GetSVGObject() { return m_paint; }

private:
	SVGPaintObject*			m_paint;
};

class SVGDOMPaintImpl : public SVGDOMPaint
{
public:
	SVGDOMPaintImpl(SVGPaintObject* paint);
	virtual ~SVGDOMPaintImpl();

	// Implementations of SVGDOMItem
	virtual const char*		GetDOMName();
	virtual SVGObject*		GetSVGObject() { return m_paint; }
	
	// Implementations of SVGDOMColor
	virtual ColorType		GetColorType();
	virtual OP_BOOLEAN		GetRGBColor(SVGDOMCSSRGBColor*& rgb_color);
	virtual OP_BOOLEAN		SetRGBColor(const uni_char* rgb_color);
	virtual OP_BOOLEAN		SetColor(ColorType color_type, const uni_char* rgb_color, uni_char* icc_color);

	// Implementations of SVGDOMCSSValue
	virtual OP_STATUS		GetCSSText(TempBuffer *buffer);
	virtual OP_STATUS		SetCSSText(uni_char* buffer);
	virtual UnitType		GetCSSValueType();
	
	// Implemenation of SVGDOMPaint
	virtual PaintType		GetPaintType();
	virtual const uni_char*	GetURI();
	virtual OP_STATUS		SetURI(const uni_char* uri);
	virtual OP_STATUS		SetPaint(PaintType type, const uni_char* uri,
									 const uni_char* rgb_color, const uni_char* icc_color);
private:
	SVGPaintObject*			m_paint;
};

#endif // !SVG_DOM_PAINT_IMPL_H

