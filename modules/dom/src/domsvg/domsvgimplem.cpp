/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/domcore/implem.h"
#include "modules/dom/src/domsvg/domsvgimplem.h"

DOM_SVGDOMImplementation::DOM_SVGDOMImplementation()
{
}

/* static */ OP_STATUS
DOM_SVGDOMImplementation::Make(DOM_SVGDOMImplementation *&implementation, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	return DOM_Object::DOMSetObjectRuntime(implementation = OP_NEW(DOM_SVGDOMImplementation, ()), runtime, runtime->GetObjectPrototype(), "DOMImplementation");
}

/* virtual */ void
DOM_SVGDOMImplementation::InitializeL(ES_Runtime* runtime)
{
    DOM_Implementation::InitializeL(runtime);
}

#endif // SVG_DOM
