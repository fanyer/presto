/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_COLOR_IMPL_H
#define SVG_DOM_COLOR_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGValue.h"

class SVGDOMCSSPrimitiveValueColorImpl : public SVGDOMCSSPrimitiveValue
{
public:
	enum ColorChannel
	{
		COLORCHANNEL_RED,
		COLORCHANNEL_GREEN,
		COLORCHANNEL_BLUE
	};

	SVGDOMCSSPrimitiveValueColorImpl(SVGColorObject* color_val, ColorChannel channel);
	~SVGDOMCSSPrimitiveValueColorImpl();

	virtual const char*		GetDOMName();

	virtual UnitType		GetPrimitiveType();
	virtual OP_BOOLEAN		SetFloatValue(UnitType unit_type, double float_value);
	virtual OP_BOOLEAN		GetFloatValue(UnitType unit_type, double& float_value);
	virtual SVGObject*		GetSVGObject() { return m_color_val; }

	// dummy implementations of CSSValue
	virtual OP_STATUS GetCSSText(TempBuffer *buffer) { OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS SetCSSText(uni_char* buffer)  { OP_ASSERT(!"NOT IMPLEMENTED"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual SVGDOMCSSValue::UnitType GetCSSValueType() { return SVG_DOM_CSS_PRIMITIVE_VALUE; }

private:
	SVGColorObject* m_color_val;
	ColorChannel m_channel;
};

class SVGDOMCSSRGBColorImpl : public SVGDOMCSSRGBColor
{
public:
	SVGDOMCSSRGBColorImpl(SVGColorObject* color_val);
	~SVGDOMCSSRGBColorImpl();

	virtual const char*		GetDOMName();

	virtual SVGDOMCSSPrimitiveValue* GetRed();
	virtual SVGDOMCSSPrimitiveValue* GetGreen();
	virtual SVGDOMCSSPrimitiveValue* GetBlue();
	virtual SVGObject*		GetSVGObject() { return m_color_val; }
private:
	SVGColorObject* m_color_val;
};

class SVGDOMColorImpl : public SVGDOMColor
{
public:

	SVGDOMColorImpl(SVGColorObject* color_val);
	virtual ~SVGDOMColorImpl();

	virtual const char*		GetDOMName();

	virtual ColorType		GetColorType();

	virtual OP_BOOLEAN		GetRGBColor(SVGDOMCSSRGBColor*& rgb_color);
	virtual OP_BOOLEAN		SetRGBColor(const uni_char* rgb_color);
	virtual OP_BOOLEAN		SetColor(ColorType color_type, const uni_char* rgb_color, uni_char* icc_color);

	virtual OP_STATUS		GetCSSText(TempBuffer *buffer);
	virtual OP_STATUS		SetCSSText(uni_char *buffer);
	virtual UnitType		GetCSSValueType();
	virtual SVGObject*		GetSVGObject() { return m_color_val; }

private:
	SVGColorObject*			m_color_val;
};

# endif // SVG_FULL_11
#endif // SVG_DOM_COLOR_IMPL_H
