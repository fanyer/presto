/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _DOM_HTMLDOMIMPLEMENTATION_
#define _DOM_HTMLDOMIMPLEMENTATION_

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domobj.h"

class DOM_HTMLDOMImplementation
  : public DOM_DOMImplementation
{
private:
	DOM_HTMLDOMImplementation();

	void InitializeL();

public:
	static OP_STATUS Make(DOM_HTMLDOMImplementation *&implementation, DOM_EnvironmentImpl *environment);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_IMPLEMENTATION || DOM_DOMImplementation::IsA(type); }

	DOM_DECLARE_FUNCTION(createHTMLDocument);
};

#endif
