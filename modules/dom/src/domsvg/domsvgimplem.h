/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _DOM_SVGDOMIMPLEMENTATION_
#define _DOM_SVGDOMIMPLEMENTATION_

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domobj.h"

class DOM_SVGDOMImplementation
  : public DOM_DOMImplementation
{
private:
	DOM_SVGDOMImplementation();

public:
	static OP_STATUS Make(DOM_SVGDOMImplementation *&implementation, DOM_EnvironmentImpl *environment);

	virtual void InitializeL(ES_Runtime* runtime);
	virtual BOOL IsA(DOM_ObjectType type) { return type == DOM_TYPE_SVG_IMPLEMENTATION || DOM_DOMImplementation::IsA(type); }

//	DOM_DECLARE_FUNCTION(createHTMLDocument);
};

#endif
