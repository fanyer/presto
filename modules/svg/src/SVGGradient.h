/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVGGRADIENT_H
#define SVGGRADIENT_H

#ifdef SVG_SUPPORT
#ifdef SVG_SUPPORT_GRADIENTS

#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGLength.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/svgpaintserver.h"

class SVGDocumentContext;
class SVGElementResolver;

class SVGStop
{
public:
	SVGStop() : m_stopcolor(OP_RGB(0,0,0)), m_stopopacity(0xFF), m_offset(0) {}
	SVGStop(const SVGStop& s) : m_stopcolor(s.m_stopcolor), m_stopopacity(s.m_stopopacity), m_offset(s.m_offset) {}

	UINT32 GetColorRGB() const;
	UINT8 GetOpacity() const { return m_stopopacity; }
	SVGNumber GetOffset() const { return m_offset; }

	void SetOffset(SVGNumber offset) { m_offset = offset; }
	void SetColorRGB(UINT32 color) { m_stopcolor = color; }
	void SetOpacity(UINT8 opacity) { m_stopopacity = opacity; }

private:
	UINT32		m_stopcolor;
	UINT8		m_stopopacity;
	SVGNumber	m_offset;
};

struct SVGGradientParameters;

class SVGGradient : public SVGPaintServer
{
public:
	enum GradientType
	{
		LINEAR,
		RADIAL
	};

	SVGGradient(GradientType type)
		: m_a(0), m_b(0), m_c(0), m_d(0), m_e(0), m_type(type),
		  m_spread(SVGSPREAD_UNKNOWN), m_units(SVGUNITS_UNKNOWN) {}
	
	virtual OP_STATUS GetFill(VEGAFill** vfill, VEGATransform& vfilltrans,
							  SVGPainter* painter, SVGPaintNode* context_node);
	virtual void PutFill(VEGAFill* vfill);

	static OP_STATUS Create(HTML_Element *gradient_element,
							SVGElementResolver* resolver, SVGDocumentContext* doc_ctx,
							const SVGValueContext& vcxt,
							SVGGradient **outgrad);
	
	GradientType Type() const { return m_type; }
	void SetType(GradientType t) { m_type = t; }
	
	SVGSpreadMethodType GetSpreadMethod() const { return m_spread; }
	void SetSpreadMethod(SVGSpreadMethodType type) { m_spread = type; }

	SVGUnitsType GetUnits() const { return m_units; }
	void SetUnits(SVGUnitsType type) { m_units = type; }

	UINT32 GetNumStops() const { return m_stops.GetCount(); }
	const SVGStop* GetStop(UINT32 index) const { return m_stops.Get(index); }
	OP_STATUS AddStop(SVGStop *stop) { return stop ? m_stops.Add(stop) : OpStatus::ERR; }

	const SVGMatrix& GetTransform() const { return m_transform; }
	void SetTransform(const SVGMatrix& t) { m_transform.Copy(t); }

	SVGNumber GetCx() const { return m_a; }
	SVGNumber GetCy() const { return m_b; }
	SVGNumber GetFx() const { return m_c; }
	SVGNumber GetFy() const { return m_d; }
	SVGNumber GetR() const { return m_e; }

	void SetCx(SVGNumber cx) { m_a = cx; }
	void SetCy(SVGNumber cy) { m_b = cy; }
	void SetFx(SVGNumber fx) { m_c = fx; }
	void SetFy(SVGNumber fy) { m_d = fy; }
	void SetR(SVGNumber r) { m_e = r; }
	
	SVGNumber GetX1() const { return m_a; }
	SVGNumber GetY1() const { return m_b; }
	SVGNumber GetX2() const { return m_c; }
	SVGNumber GetY2() const { return m_d; }

	void SetX1(SVGNumber x1) { m_a = x1; }
	void SetY1(SVGNumber y1) { m_b = y1; }
	void SetX2(SVGNumber x2) { m_c = x2; }
	void SetY2(SVGNumber y2) { m_d = y2; }
	
	const SVGRect& GetTargetBoundingBox() const { return m_bbox; }
	void SetTargetBoundingBox(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h);

	OP_STATUS CreateCopy(SVGGradient **outcopy) const;

#ifdef SELFTEST
	BOOL operator==(const SVGGradient& other) const;
#endif // SELFTEST
#ifdef _DEBUG
	void Print() const;
#endif

private:
	OP_STATUS FetchValues(HTML_Element *gradient_element,
						  SVGElementResolver* resolver, SVGDocumentContext* doc_ctx,
						  SVGGradientParameters* params, HTML_Element** stop_root);
	OP_STATUS FetchGradientStops(HTML_Element* stop_root);
	void ResolveGradientParameters(const SVGGradientParameters& params, const SVGValueContext& vcxt);

	static OP_STATUS CreateStop(HTML_Element *stopelm, LayoutProperties* props, LayoutInfo& info,
								SVGStop **outstop);

	virtual ~SVGGradient() {}

	SVGNumber m_a;
	SVGNumber m_b;
	SVGNumber m_c;
	SVGNumber m_d;
	SVGNumber m_e;
	GradientType m_type;
	SVGSpreadMethodType m_spread;
	SVGUnitsType m_units;
	OpAutoVector<SVGStop> m_stops;
	SVGMatrix m_transform;
	SVGRect m_bbox;
};

#endif // SVG_SUPPORT_GRADIENTS
#endif // SVG_SUPPORT
#endif // SVGGRADIENT_H
