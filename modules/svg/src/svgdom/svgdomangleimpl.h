/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_ANGLE_IMPL_H
#define SVG_DOM_ANGLE_IMPL_H

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGValue.h"

#if defined(SVG_DOM) && defined(SVG_FULL_11)

class SVGDOMAngleImpl : public SVGDOMAngle
{
public:
									SVGDOMAngleImpl(SVGAngle* angle);
	virtual							~SVGDOMAngleImpl();

	virtual const char*				GetDOMName();
	virtual SVGDOMAngle::UnitType	GetUnitType();
	virtual OP_BOOLEAN				SetValue(double v);
	virtual double					GetValue();
	virtual OP_BOOLEAN				SetValueInSpecifiedUnits(double v);
	virtual double					GetValueInSpecifiedUnits();
	virtual OP_STATUS				SetValueAsString(const uni_char* v);
	virtual const uni_char* 		GetValueAsString();
	virtual OP_BOOLEAN				NewValueSpecifiedUnits(SVGDOMAngle::UnitType unitType, double valueInSpecifiedUnits);
	virtual OP_BOOLEAN				ConvertToSpecifiedUnits(SVGDOMAngle::UnitType unitType);
	virtual SVGObject*				GetSVGObject() { return m_angle; }

	SVGAngle*						GetAngle() { return m_angle; }

private:

	SVGAngle*						m_angle;

	static SVGDOMAngle::UnitType	SVGAngleTypeToDOMAngleType(SVGAngleType svg_angle_type);
	static SVGAngleType				DOMAngleTypeToSVGAngleType(SVGDOMAngle::UnitType dom_angle_type);
};

#endif // SVG_DOM && SVG_FULL_11
#endif // !SVG_DOM_ANGLE_IMPL_H

