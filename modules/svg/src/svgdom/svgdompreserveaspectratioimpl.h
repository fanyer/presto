/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_PRESERVEASPECTRATIO_IMPL_H
#define SVG_DOM_PRESERVEASPECTRATIO_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGValue.h"

class SVGDOMPreserveAspectRatioImpl : public SVGDOMPreserveAspectRatio
{
public:
							SVGDOMPreserveAspectRatioImpl(SVGAspectRatio* aspect_val);
	virtual					~SVGDOMPreserveAspectRatioImpl();

	virtual const char*		GetDOMName();
	virtual SVGObject*		GetSVGObject() { return m_aspect_val; }

	virtual OP_BOOLEAN		SetAlign(unsigned short value);
	virtual unsigned short	GetAlign();

	virtual OP_BOOLEAN		SetMeetOrSlice(unsigned short value);
	virtual unsigned short	GetMeetOrSlice();

#if 0
	SVGAspectRatio*			GetAspectRatio() { return m_aspect_val; }
#endif

private:
	SVGDOMPreserveAspectRatioImpl(const SVGDOMPreserveAspectRatioImpl& X);
	void operator=(const SVGDOMPreserveAspectRatioImpl& X);

	SVGAspectRatio*			m_aspect_val;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_PRESERVEASPECTRATIO_IMPL_H
