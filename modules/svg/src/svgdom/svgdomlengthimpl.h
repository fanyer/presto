/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_LENGTH_IMPL_H
#define SVG_DOM_LENGTH_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGLength.h"

class SVGDOMLengthImpl : public SVGDOMLength
{
public:
							SVGDOMLengthImpl(SVGLengthObject* length);
	virtual					~SVGDOMLengthImpl();
	virtual const char*		GetDOMName();
	virtual SVGDOMLength::UnitType GetUnitType();
	virtual OP_BOOLEAN		SetValue(double v);
	virtual double			GetValue(HTML_Element* elm, Markup::AttrType attr_name, NS_Type ns);
	virtual OP_BOOLEAN		SetValueInSpecifiedUnits(double v);
	virtual double			GetValueInSpecifiedUnits();
	virtual OP_BOOLEAN		SetValueAsString(const uni_char* v);
	virtual const uni_char* GetValueAsString();
	virtual OP_BOOLEAN		NewValueSpecifiedUnits(SVGDOMLength::UnitType unitType, double valueInSpecifiedUnits);
	virtual OP_BOOLEAN		ConvertToSpecifiedUnits(SVGDOMLength::UnitType unitType, HTML_Element* elm, Markup::AttrType attr_name, NS_Type ns);
	virtual SVGObject*		GetSVGObject() { return m_length; }

	SVGLengthObject*		GetLengthObject() { return m_length; }

protected:
	static SVGDOMLength::UnitType	CSSUnitToUnitType(int css_unit);
	static int 						UnitTypeToCSSUnit(SVGDOMLength::UnitType unit_type);

private:
	SVGLengthObject*		m_length;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_LENGTH_IMPL_H

