/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_PATH_IMPL_H
#define SVG_DOM_PATH_IMPL_H

# ifdef SVG_TINY_12
#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/OpBpath.h"
class OpBpath;

class SVGDOMPathImpl : public SVGDOMPath
{
public:
							SVGDOMPathImpl(OpBpath* path);
	virtual 				~SVGDOMPathImpl();

	virtual const char*		GetDOMName();
	virtual OP_STATUS		GetSegment(unsigned long cmdIndex, short& outsegment);
	virtual OP_STATUS		GetSegmentParam(unsigned long cmdIndex, unsigned long paramIndex, double& outparam); 

	virtual UINT32			GetCount() { return m_path->GetCount(TRUE); }

	virtual SVGObject*		GetSVGObject() { return m_path; }
	virtual SVGPath*		GetPath() { return m_path; }

private:
	OpBpath* m_path;
};

# endif // SVG_TINY_12
#endif // SVG_DOM_PATH_IMPL_H
