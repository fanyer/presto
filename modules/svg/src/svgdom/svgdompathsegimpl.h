/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_PATH_SEG_IMPL_H
#define SVG_DOM_PATH_SEG_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/OpBpath.h"

class SVGDOMPathSegImpl : public SVGDOMPathSeg
{
public:
						SVGDOMPathSegImpl(SVGPathSegObject* seg);
	virtual				~SVGDOMPathSegImpl();

	virtual const char* GetDOMName();
	virtual SVGObject*	GetSVGObject() { return m_pathseg; }

	virtual int			GetSegType() const;
	virtual uni_char	GetSegTypeAsLetter() const;

	virtual double		GetX();
	virtual double		GetY();
	virtual double		GetX1();
	virtual double		GetY1();
	virtual double		GetX2();
	virtual double		GetY2();
	virtual double		GetR1() { return GetX1(); }
	virtual double		GetR2() { return GetY1(); }
	virtual double		GetAngle() { return GetX2(); }
	virtual BOOL		GetLargeArcFlag();
	virtual BOOL		GetSweepFlag();

	virtual OP_BOOLEAN	SetX(double val);
	virtual OP_BOOLEAN	SetY(double val);
	virtual OP_BOOLEAN	SetX1(double val);
	virtual OP_BOOLEAN	SetY1(double val);
	virtual OP_BOOLEAN	SetX2(double val);
	virtual OP_BOOLEAN	SetY2(double val);
	virtual OP_BOOLEAN	SetR1(double val) { return SetX1(val); }
	virtual OP_BOOLEAN	SetR2(double val) { return SetY1(val); }
	virtual OP_BOOLEAN	SetAngle(double val) { return SetX2(val); }
	virtual OP_BOOLEAN	SetLargeArcFlag(BOOL large);
	virtual OP_BOOLEAN	SetSweepFlag(BOOL sweep);

	SVGPathSegObject*	GetSVGPathSegObj() { return m_pathseg; }

	virtual BOOL IsValid(UINT32 idx);

private:
	SVGDOMPathSegImpl(const SVGDOMPathSegImpl& X);
	void operator=(const SVGDOMPathSegImpl& X);

	SVGPathSegObject* m_pathseg;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_PATH_SEG_IMPL_H
